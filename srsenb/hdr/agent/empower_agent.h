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

#define EMPOWER_AGENT_MAX_MEAS          32
#define EMPOWER_AGENT_MAX_CELL_MEAS     8

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
    uint32_t mod_id,
    uint16_t freq,
    uint8_t  max_cells,
    int      interval,
    uint8_t  meas_id,
    uint8_t  obj_id,
    uint8_t  rep_id);

  /* agent_interface_rrc: */

  void add_user(uint16_t rnti);
  void rem_user(uint16_t rnti);

  void report_RRC_measure(
    uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report);

  /* Thread: */

  void stop();

private:

  class em_ue {
  public:
    typedef struct {
      uint16_t pci;
      uint8_t  rsrp;
      uint8_t  rsrq;
    }ue_cell_meas;

    typedef struct {
      uint32_t mod_id;

      ue_cell_meas carrier;
      int c_dirty;

      ue_cell_meas neigh[EMPOWER_AGENT_MAX_CELL_MEAS];
      int n_dirty[EMPOWER_AGENT_MAX_CELL_MEAS];
    }ue_meas;

    uint64_t imsi;
    uint32_t plmn;

    ue_meas  meas[EMPOWER_AGENT_MAX_MEAS];
  }; /* class em_ue */

  unsigned int       m_id;

  srslte::radio *    m_rf;
  srsenb::phy *      m_phy;
  srsenb::mac *      m_mac;
  srsenb::rlc *      m_rlc;
  srsenb::pdcp *     m_pdcp;
  srsenb::rrc *      m_rrc;
  srslte::log *      m_logger;

  int                m_uer_feat;
  int                m_uer_tr;
  unsigned int       m_uer_mod;

  int                m_nof_ues;
  std::map<uint16_t, em_ue *> m_ues;
  int                m_ues_dirty;

  /* Thread: */

  pthread_t          m_thread;
  pthread_spinlock_t m_lock;
  int                m_state;

  /* Utilities */

  void dirty_ue_check();
  void measure_check();

  /* Thread: */

  static void * agent_loop(void * args);

  /* Interaction with controller: */

  void send_UE_report(void);
  void send_UE_meas(uint16_t rnti, int m);

};

} /* namespace srsenb */

#endif /* EMPOWER_AGENT_H */
