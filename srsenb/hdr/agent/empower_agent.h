/**
 * \section AUTHOR
 *
 * Author: Kewin Rausch
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

#ifndef EMPOWER_AGENT_H
#define EMPOWER_AGENT_H

#include <map>
#include <pthread.h>
#include <string>

#include <srslte/common/log_filter.h>
#include <srslte/common/threads.h>

#include "agent/agent.h"

namespace srsenb {

/* State of the agent thread. */
enum agent_state {
  /* Agent processing paused. */
  AGENT_STATE_STOPPED = 0,
  /* Agent processing paused. */
  AGENT_STATE_PAUSED,
  /* Agent processing started. */
  AGENT_STATE_STARTED,
};

/* Features supported by this agent. */
enum agent_feature {
  /* Report the connection/disconnection of UE. */
  AGENT_FEAT_UE_REPORT = 1,
};

class empower_agent :
  public agent
{
public:
  empower_agent();
  ~empower_agent();

  int enable_feature(int feature, int module, int trigger);

  unsigned int get_id();

  int init(int             enb_id,
           srslte::radio * rf,
           srsenb::phy *   phy,
           srsenb::mac *   mac,
           srsenb::rlc *   rlc,
           srsenb::pdcp *  pdcp,
           srsenb::rrc *   rrc,
           srslte::log *   logger);

  /* Release any reserved resource. */
  void release();

  void setup_UE_period_meas(
    uint16_t rnti,
    uint16_t freq,
    uint8_t max_cells,
    int interval,
    uint8_t meas_id,
    uint8_t obj_id,
    uint8_t rep_id);

  /* agent_interface_rrc: */

  void add_user(uint16_t rnti);
  void rem_user(uint16_t rnti);

  /* Thread: */

  void stop();

private:

  class em_ue {
  public:
    uint64_t imsi;
    uint32_t plmn;
  };

  unsigned int    m_id;

  srslte::radio * m_rf;
  srsenb::phy *   m_phy;
  srsenb::mac *   m_mac;
  srsenb::rlc *   m_rlc;
  srsenb::pdcp *  m_pdcp;
  srsenb::rrc *   m_rrc;
  srslte::log *   m_logger;

  int             m_feat_ue;
  unsigned int    m_mod_ue;

  int             m_nof_ues;
  std::map<uint16_t, em_ue *> m_ues;
  int             m_ues_dirty;

  /* Thread: */

  pthread_t       m_thread;
  pthread_spinlock_t m_lock;
  int             m_state;

  /* Thread: */
  static void * agent_loop(void * args);

  /* Interaction with controller: */

  void send_UE_report(void);
};

} /* namespace srsenb */

#endif /* EMPOWER_AGENT_H */
