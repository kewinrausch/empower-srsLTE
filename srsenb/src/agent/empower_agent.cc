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

#include "srslte/asn1/liblte_rrc.h"

#include "enb.h"
#include "agent/empower_agent.h"

#define EMAGE_BUF_SMALL_SIZE    2048
#define EMAGE_REPORT_MAX_UES    32

#define Error(fmt, ...)                       \
  do {                                        \
    m_logger->error_line(                     \
      __FILE__, __LINE__, fmt, ##__VA_ARGS__);\
    printf(fmt, ##__VA_ARGS__);               \
  } while(0)

#define Warning(fmt, ...)                     \
  do {                                        \
    m_logger->warning_line(                   \
      __FILE__, __LINE__, fmt, ##__VA_ARGS__);\
    printf(fmt, ##__VA_ARGS__);               \
  } while(0)

#define Info(fmt, ...)                        \
  do {                                        \
    m_logger->info_line(                      \
      __FILE__, __LINE__, fmt, ##__VA_ARGS__);\
    printf(fmt, ##__VA_ARGS__);               \
  } while(0)

#define Debug(fmt, ...)                       \
  do {                                        \
    m_logger->debug_line(                     \
      __FILE__, __LINE__, fmt, ##__VA_ARGS__);\
    printf(fmt, ##__VA_ARGS__);               \
  } while(0)

#define RSRP_RANGE_TO_VALUE(x)	((float)x - 140.0f)
#define RSRQ_RANGE_TO_VALUE(x)	(((float)x / 2) - 20.0f)

namespace srsenb {

/******************************************************************************
 * Agent callback system.                                                     *
 ******************************************************************************/

/* NOTE: This can hold only one reference of agent. If you plan to go with more
 * consider using a map enb_id --> agent instance.
 */
static empower_agent * em_agent = 0;

static int ea_enb_setup(void)
{
  char        buf[EMAGE_BUF_SMALL_SIZE] = {0};
  ep_cell_det cells[1];
  int         blen                      = 0;

  all_args_t * args = enb::get_instance()->get_args();

  cells[0].cap       = EP_CCAP_NOTHING;
  cells[0].pci       = (uint16_t)args->enb.pci;
  cells[0].DL_earfcn = (uint16_t)args->rf.dl_earfcn;
  cells[0].UL_earfcn = (uint16_t)args->rf.ul_earfcn;
  cells[0].DL_prbs   = (uint8_t) args->enb.n_prb;
  cells[0].UL_prbs   = (uint8_t) args->enb.n_prb;

  blen = epf_single_ecap_rep(
    buf, EMAGE_BUF_SMALL_SIZE,
    em_agent->get_id(),
    0,
    0,
    EP_ECAP_UE_REPORT,
    cells,
    1);

  if(blen < 0) {
    return -1;
  }

  return em_send(em_agent->get_id(), buf, blen);
}

static int ea_ue_report(unsigned int mod, int trig_id, int trig_type)
{
  return em_agent->enable_feature(AGENT_FEAT_UE_REPORT, mod, trig_id);
}

static struct em_agent_ops empower_agent_ops = {
  .init                   = 0,
  .release                = 0,
  .enb_setup_request      = ea_enb_setup,
  .ue_report              = ea_ue_report,
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

  m_uer_mod  = 0;
  m_uer_feat = 0;
  m_uer_tr   = 0;

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
    m_uer_mod  = module;
    m_uer_tr   = trigger;
    m_uer_feat = 1;
    return 0;
  default:
    return -1;
  }
}

unsigned int empower_agent::get_id()
{
  return m_id;
}

int empower_agent::init(
  int             enb_id,
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

void empower_agent::setup_UE_period_meas(
  uint16_t rnti,
  uint32_t mod_id,
  uint16_t freq,
  uint8_t max_cells,
  int interval,
  uint8_t meas_id, uint8_t obj_id, uint8_t rep_id)
{
  LIBLTE_RRC_MEAS_CONFIG_STRUCT meas;
  LIBLTE_RRC_REPORT_INTERVAL_ENUM rep_int;

  /*
   * Setup agent mechanism:
   */
  if(m_ues.count(rnti) == 0) {
    Error("No %x RNTI known\n", rnti);
    return;
  }

  m_ues[rnti]->meas[meas_id].mod_id = mod_id;

  /*
   * Prepare RRC request to send to the UE.
   */

  bzero(&meas, sizeof(LIBLTE_RRC_MEAS_CONFIG_STRUCT));

  if(interval <= 120) {
    rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS120;
  } else if(interval > 120 && interval <= 240) {
    rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS240;
  } else if(interval > 240 && interval <= 480) {
    rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS480;
  } else if(interval > 480 && interval <= 640) {
    rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS640;
  } else if(interval > 640 && interval <= 1024) {
    rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS1024;
  } else if(interval > 1024 && interval <= 2048) {
    rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS2048;
  } else if(interval > 2048 && interval <= 5120) {
    rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS5120;
  } else {
    rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS10240;
  }

  meas.meas_obj_to_add_mod_list_present = true;
  meas.rep_cnfg_to_add_mod_list_present = true;
  meas.meas_id_to_add_mod_list_present  = true;
  meas.quantity_cnfg_present            = false;
  meas.meas_gap_cnfg_present            = false;
  meas.s_meas_present                   = false;
  meas.pre_reg_info_hrpd_present        = false;
  meas.speed_state_params_present       = false;
  meas.N_meas_id_to_remove              = 0;
  meas.N_meas_obj_to_remove             = 0;
  meas.N_rep_cnfg_to_remove             = 0;

  // Measurement object:

  meas.meas_obj_to_add_mod_list.N_meas_obj                                                         = 1;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_id                                       = obj_id;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_type                                     = LIBLTE_RRC_MEAS_OBJECT_TYPE_EUTRA;

  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.offset_freq_not_default            = false;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.presence_ant_port_1                = true;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.cells_to_remove_list_present       = false;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.black_cells_to_remove_list_present = false;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.cell_for_which_to_rep_cgi_present  = false;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.N_black_cells_to_add_mod           = 0;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.N_cells_to_add_mod                 = 0;

  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.allowed_meas_bw                    = LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_MBW25;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.offset_freq                        = LIBLTE_RRC_Q_OFFSET_RANGE_DB_0;
  meas.meas_obj_to_add_mod_list.meas_obj_list[0].meas_obj_eutra.carrier_freq                       = freq;

  // Measurement report:

  meas.rep_cnfg_to_add_mod_list.N_rep_cnfg                                                         = 1;
  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_id                                       = rep_id;
  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_type                                     = LIBLTE_RRC_REPORT_CONFIG_TYPE_EUTRA;

  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_eutra.trigger_type                       = LIBLTE_RRC_TRIGGER_TYPE_EUTRA_PERIODICAL;
  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_eutra.trigger_quantity                   = LIBLTE_RRC_TRIGGER_QUANTITY_RSRQ;
  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_eutra.periodical.purpose                 = LIBLTE_RRC_PURPOSE_EUTRA_REPORT_STRONGEST_CELL;
  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_eutra.report_amount                      = LIBLTE_RRC_REPORT_AMOUNT_INFINITY;
  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_eutra.report_quantity                    = LIBLTE_RRC_REPORT_QUANTITY_BOTH;
  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_eutra.report_interval                    = rep_int;
  meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[0].rep_cnfg_eutra.max_report_cells                   = max_cells;

  // Measurement IDs:

  meas.meas_id_to_add_mod_list.N_meas_id                                                           = 1;
  meas.meas_id_to_add_mod_list.meas_id_list[0].meas_id                                             = meas_id;
  meas.meas_id_to_add_mod_list.meas_id_list[0].meas_obj_id                                         = obj_id;
  meas.meas_id_to_add_mod_list.meas_id_list[0].rep_cnfg_id                                         = rep_id;

  m_rrc->setup_ue_measurement(rnti, &meas);
}

/******************************************************************************
 * agent_interface_rrc.                                                       *
 ******************************************************************************/

void empower_agent::add_user(uint16_t rnti)
{
  std::map<uint16_t, em_ue *>::iterator it;

  all_args_t *  args = enb::get_instance()->get_args();

  pthread_spin_lock(&m_lock);

  it = m_ues.find(rnti);

  if(it == m_ues.end()) {
    m_ues.insert(std::make_pair(rnti, new em_ue()));
    m_nof_ues++;

    m_ues[rnti]->imsi = 0;
    m_ues[rnti]->plmn  = (args->enb.s1ap.mcc & 0x0fff) << 12;
    m_ues[rnti]->plmn |= (args->enb.s1ap.mnc & 0x0fff);

    if(m_uer_feat) {
      m_ues_dirty = 1;
    }
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

    if(m_uer_feat) {
      m_ues_dirty = 1;
    }

    delete it->second;
  }

  pthread_spin_unlock(&m_lock);
}

void empower_agent::report_RRC_measure(
  uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report)
{
  int i;
  int nof_cells = report->meas_results.n_of_neigh_cells;

  LIBLTE_RRC_MEAS_RESULT_EUTRA_STRUCT * cells;

  if(report->meas_results.meas_id > EMPOWER_AGENT_MAX_MEAS) {
    Error("MeasureId %d is too high for agent\n", report->meas_results.meas_id);
    return;
  }

  if(m_ues.count(rnti) != 0) {
    m_ues[rnti]->meas[report->meas_results.meas_id].carrier.rsrp =
      report->meas_results.meas_result_pcell.rsrp_range;
    m_ues[rnti]->meas[report->meas_results.meas_id].carrier.rsrq =
      report->meas_results.meas_result_pcell.rsrq_range;

    m_ues[rnti]->meas[report->meas_results.meas_id].c_dirty = 1;

    if(nof_cells == 0) {
      return;
    }

    cells = report->meas_results.neigh_cells_EUTRA;

    for(i = 0; i < nof_cells && nof_cells < EMPOWER_AGENT_MAX_CELL_MEAS; i++) {
      m_ues[rnti]->meas[report->meas_results.meas_id].neigh[i].pci =  cells[i].pci;
      m_ues[rnti]->meas[report->meas_results.meas_id].neigh[i].rsrp = cells[i].rsrp_range;
      m_ues[rnti]->meas[report->meas_results.meas_id].neigh[i].rsrq = cells[i].rsrq_range;
      m_ues[rnti]->meas[report->meas_results.meas_id].n_dirty[i]    = 1;
    }
  }
}

/******************************************************************************
 * Interaction with controller.                                               *
 ******************************************************************************/

void empower_agent::send_UE_report(void)
{
  std::map<uint16_t, em_ue *>::iterator it;

  int           i;
  char          buf[EMAGE_BUF_SMALL_SIZE];
  int           size;
  ep_ue_details ued[16];

  all_args_t *  args = enb::get_instance()->get_args();

  pthread_spin_lock(&m_lock);

  for(i = 0, it = m_ues.begin(); it != m_ues.end(); ++it, i++)
  {
    ued[i].pci  = (uint16_t)args->enb.pci;
    ued[i].rnti = it->first;
    ued[i].plmn = it->second->plmn;
    ued[i].imsi = it->second->imsi;
  }

  pthread_spin_unlock(&m_lock);

  size = epf_trigger_uerep_rep(
    buf, EMAGE_BUF_SMALL_SIZE,
    m_id,
    (uint16_t)args->enb.pci,
    m_uer_mod,
    i,
    ued);

  if(size < 0) {
    Error("Cannot format UE report reply\n");
    return;
  }

  em_send(m_id, buf, size);
}

/******************************************************************************
 * Private utilities.                                                         *
 ******************************************************************************/

void empower_agent::dirty_ue_check()
{
  /* Check if trigger is still there */
  if(!em_has_trigger(m_id, m_uer_tr, EM_TRIGGER_UE_REPORT)) {
    m_uer_feat = 0;
    return;
  }

  if(m_ues_dirty) {
    send_UE_report();
    m_ues_dirty = 0;
  }
}

void empower_agent::measure_check()
{
  int i;
  std::map<uint16_t, em_ue *>::iterator it;

  for (it = m_ues.begin(); it != m_ues.end(); ++it) {
    for(i = 0; i < EMPOWER_AGENT_MAX_MEAS; i++) {
      if(it->second->meas[i].c_dirty) {
        //send_UE_meas(it->first, i);
      }
    }
  }
}

/******************************************************************************
 * Agent threading context.                                                   *
 ******************************************************************************/

void * empower_agent::agent_loop(void * args)
{
  all_args_t *    enb_args = enb::get_instance()->get_args();
  empower_agent * a        = (empower_agent *)args;

  int i;

  if(em_start(
    a->m_id,
    &empower_agent_ops,
    (char *)enb_args->enb.ctrl_addr.c_str(),
    enb_args->enb.ctrl_port)) {

    return 0;
  }

  a->m_state = AGENT_STATE_STARTED;

  while(a->m_state != AGENT_STATE_STOPPED) {
    if(a->m_uer_feat) {
      a->dirty_ue_check();
    }

    a->measure_check();

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
