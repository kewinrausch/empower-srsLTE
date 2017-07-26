/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2017 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of srsLTE.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/thread/mutex.hpp>
#include <emage/emage.h>
#include <emage/emproto.h>
#include <emage/pb/main.pb-c.h>

#include "agent/empower_agent.h"

#define EMAGE_BUF_SIZE          4096
#define EMAGE_REPORT_MAX_UES    32

#define Error(fmt, ...)                     \
  do {                                      \
    m_logger->error_line(		    \
    __FILE__, __LINE__, fmt, ##__VA_ARGS__);\
    printf(fmt, ##__VA_ARGS__);             \
  } while(0)

#define Warning(fmt, ...)                   \
  do {                                      \
    m_logger->warning_line(		    \
    __FILE__, __LINE__, fmt, ##__VA_ARGS__);\
    printf(fmt, ##__VA_ARGS__);             \
  } while(0)

#define Info(fmt, ...)                      \
  do {                                      \
    m_logger->info_line(		    \
    __FILE__, __LINE__, fmt, ##__VA_ARGS__);\
    printf(fmt, ##__VA_ARGS__);             \
  } while(0)

#define Debug(fmt, ...)                     \
  do {                                      \
    m_logger->debug_line(		    \
    __FILE__, __LINE__, fmt, ##__VA_ARGS__);\
    printf(fmt, ##__VA_ARGS__);             \
  } while(0)

namespace srsenb {

/******************************************************************************
 * Agent callback system.                                                     *
 ******************************************************************************/

/* NOTE: This can hold only one reference of agent. If you plan to go with more
 * consider using a map enb_id --> agent instance.
 */
static empower_agent * em_agent = 0;

static int empower_agent_ue_report (
  EmageMsg * request, EmageMsg ** reply, unsigned int trigger_id)
{
  if(!em_agent) {
    printf("Agent instance not found!\n");
    return 0;
  }

  em_agent->enable_feature(
    AGENT_FEAT_UE_REPORT, request->head->t_id, trigger_id);

  return 0;
}

static struct em_agent_ops empower_agent_ops = {
  .init = 0,
  .release = 0,
  .handover_request = 0,
  .UEs_ID_report = empower_agent_ue_report,
  .RRC_measurements = 0,
  .RRC_meas_conf = 0,
  .cell_statistics_report = 0,
  .eNB_cells_report = 0,
  .ran_sharing_control = 0
};

/******************************************************************************
 * Constructor/destructors.                                                   *
 ******************************************************************************/

empower_agent::empower_agent()
{
  m_id       = -1;
  m_state    = AGENT_STATE_STOPPED;

  m_rf       = 0;
  m_phy      = 0;
  m_mac      = 0;
  m_rlc      = 0;
  m_pdcp     = 0;
  m_rrc      = 0;
  m_logger   = 0;

  m_mod_ue   = 0;
  m_feat_ue  = 0;
  m_ues_dirty= 0;
  m_nof_ues  = 0;

  m_thread   = 0;
}

empower_agent::~empower_agent()
{
  release();
}

/******************************************************************************
 * Generic purposes procedures.                                               *
 ******************************************************************************/


int empower_agent::enable_feature(int feature, int module, int trigger)
{
  switch(feature){
  case AGENT_FEAT_UE_REPORT:
    Debug("Enabling UE report agent feature, trigger=%d\n", trigger);
    m_mod_ue  = module;
    m_feat_ue = trigger;
    return 0;
  default:
    return -1;
  }
}

int empower_agent::init(int            enb_id,
		       srslte::radio * rf,
		       srsenb::phy *   phy,
		       srsenb::mac *   mac,
		       srsenb::rlc *   rlc,
		       srsenb::pdcp *  pdcp,
		       srsenb::rrc *   rrc,
		       srslte::log *   logger)
{
  if(!rf || !phy || !mac || !rlc || !pdcp || !rrc || !logger) {
    return -EINVAL;
  }

  m_id    = enb_id;

  m_rf    = rf;
  m_phy   = phy;
  m_mac   = mac;
  m_rlc   = rlc;
  m_pdcp  = pdcp;
  m_rrc   = rrc;
  m_logger= logger;

  pthread_spin_init(&m_lock, 0);

  /* NOTE:
   * This is no more valid if you initialize more than one empower_agent!!!
   *
   * The callback systems will be redirected always on the least initialized
   * agent, this way. The problem does not arise for the moment since the eNB
   * just need one agent, and no more than one (for now).
   *
   * A proper connection between c callback system and C++ class must be put
   * in place, like for example:
   *
   * 	b_id --> class pointer to use
   */
  em_agent = this;

  /* Create a new thread; we don't use the given thread library since we don't
   * want RT capabilities for this thread, which will run with low priority.
   */
  pthread_create(&m_thread, 0, empower_agent::agent_loop, this);

  return 0;
}


void empower_agent::release()
{
  Info("Agent released\n");
}

/******************************************************************************
 * agent_interface_rrc.                                                       *
 ******************************************************************************/

void empower_agent::add_user(uint16_t rnti)
{
  std::map<uint16_t, em_ue *>::iterator it;

  pthread_spin_lock(&m_lock);

  it = m_ues.find(rnti);

  if(it == m_ues.end()) {
    m_ues.insert(std::make_pair(rnti, new em_ue()));
    m_nof_ues++;

    m_ues[rnti]->imsi = 0;
    m_ues[rnti]->plmn = 0x222f93; /* This need to be retrieved somehow... */

    m_ues_dirty = 1;
  }

  pthread_spin_unlock(&m_lock);
}

void empower_agent::rem_user(uint16_t rnti)
{
  em_ue * ue;
  std::map<uint16_t, em_ue *>::iterator it;

  pthread_spin_lock(&m_lock);

  it = m_ues.find(rnti);

  if(it != m_ues.end()) {
    ue = it->second;

    m_nof_ues--;
    m_ues.erase(it);

    m_ues_dirty = 1;

    delete it->second;
  }

  pthread_spin_unlock(&m_lock);
}

/******************************************************************************
 * Interaction with controller.                                               *
 ******************************************************************************/

void empower_agent::send_UE_report(void)
{
  int i = 0;
  std::map<uint16_t, em_ue *>::iterator it;

  EmageMsg * msg = 0;

  unsigned int nof_a = 0;
  emp_UE act[EMAGE_REPORT_MAX_UES] = {0};

  pthread_spin_lock(&m_lock);

  nof_a = m_nof_ues;

  for(it = m_ues.begin(); it != m_ues.end(); ++it, i++)
  {
    act[i].rnti = it->first;
    act[i].plmn = it->second->plmn;
    act[i].imsi = it->second->imsi;
  }

  pthread_spin_unlock(&m_lock);

  if(emp_format_UE_report(m_id, m_mod_ue, act, nof_a, 0, 0, &msg)) {
    Error("Error formatting UE report message!\n");
    return;
  }

  em_send(m_id, msg);
}

/******************************************************************************
 * Agent threading context.                                                   *
 ******************************************************************************/

void * empower_agent::agent_loop(void * args)
{
  empower_agent * a = (empower_agent *)args;

  if(em_start(&empower_agent_ops, a->m_id)) {
    return 0;
  }

  a->m_state = AGENT_STATE_STARTED;

  while(a->m_state != AGENT_STATE_STOPPED) {
    if(a->m_ues_dirty) {
      a->send_UE_report();
      a->m_ues_dirty = 0;
    }

    sleep(1);
  }

  em_terminate_agent(a->m_id);

  return 0;
}

void empower_agent::stop()
{
  if(m_state != AGENT_STATE_STOPPED) {
    m_state = AGENT_STATE_STOPPED;

    pthread_join(m_thread, 0);

    Debug("Agent stopped\n");
  }

  release();
}

} /* namespace srsenb */
