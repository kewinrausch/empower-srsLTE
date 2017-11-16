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

#include <emage/emproto.h>
#include "agent/agent.h"

namespace srsenb {

#define EMPOWER_AGENT_MAX_UE            32
#define EMPOWER_AGENT_MAX_MEAS          32
#define EMPOWER_AGENT_MAX_CELL_MEAS     8
#define EMPOWER_AGENT_MAX_MACREP        8

/* State of the agent thread. */
enum agent_state {
  /* Agent processing paused. */
  AGENT_STATE_STOPPED = 0,
  /* Agent processing paused. */
  AGENT_STATE_PAUSED,
  /* Agent processing started. */
  AGENT_STATE_STARTED,
};

class empower_agent :
  public agent
{
public:
  /* MAC report details */
  typedef struct mac_report {
    uint32_t        mod_id;
    int             trigger_id;
    uint32_t        interval;
    uint64_t        DL_acc;
    uint64_t        UL_acc;
    struct timespec last;
  } macrep;

  empower_agent();
  ~empower_agent();

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

  int reset();

  int setup_UE_report(uint32_t mod_id, int trig_id);
  int setup_MAC_report(uint32_t mod_id, uint32_t interval, int trig_id);

  int setup_UE_period_meas(
    uint32_t id,
    int      trigger_id,
    uint16_t rnti,
    uint32_t mod_id,
    uint16_t freq,
    uint16_t max_cells,
    uint16_t max_meas,
    int      interval);

  /* agent_interface_mac: */

  void process_DL_results(
    uint32_t tti, sched_interface::dl_sched_res_t * sched_result);
  void process_UL_results(
    uint32_t tti, sched_interface::ul_sched_res_t * sched_result);

  /* agent_interface_rrc: */

  void add_user(uint16_t rnti);
  void rem_user(uint16_t rnti);

  void report_RRC_measure(
    uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report);

  /* Thread: */

  void stop();

private:
  /*
   * EmPOWER UE class
   */

  class em_ue {

  public:
    typedef struct {
      int      dirty;
      uint16_t pci;
      uint8_t  rsrp;
      uint8_t  rsrq;
    }ue_cell_meas;

    typedef struct {
      uint32_t     id;     /* Id assigned by agent-controller circuit */
      uint32_t     mod_id;
      int          trig_id;

      uint32_t     meas_id;/* Id for UE-eNB circuit */
      uint32_t     obj_id;
      uint32_t     rep_id;

      uint16_t     freq;
      uint16_t     max_cells;
      uint16_t     max_meas;
      int          interval;

      ue_cell_meas carrier;
      int          c_dirty;

      ue_cell_meas neigh[EMPOWER_AGENT_MAX_CELL_MEAS];
    }ue_meas;

    uint64_t m_imsi;
    uint32_t m_plmn;

    uint32_t m_next_meas_id;
    uint32_t m_next_obj_id;
    uint32_t m_next_rep_id;
    ue_meas  m_meas[EMPOWER_AGENT_MAX_MEAS];

    em_ue();
  };

  /*
   * End class em_ue
   */

  unsigned int       m_id;

  srslte::radio *    m_rf;
  srsenb::phy *      m_phy;
  srsenb::mac *      m_mac;
  srsenb::rlc *      m_rlc;
  srsenb::pdcp *     m_pdcp;
  srsenb::rrc *      m_rrc;
  srslte::log *      m_logger;

  void *             m_args;

  int                m_uer_feat;
  int                m_uer_tr;
  unsigned int       m_uer_mod;

  int                m_nof_ues;
  std::map<uint16_t, em_ue *> m_ues;
  int                m_ues_dirty;

  macrep             m_macrep[EMPOWER_AGENT_MAX_MACREP];

  uint32_t           m_DL_sf;
  uint32_t           m_DL_prbs_used;

  uint32_t           m_UL_sf;
  uint32_t           m_UL_prbs_used;

  /* Thread: */

  pthread_t          m_thread;
  pthread_spinlock_t m_lock;
  int                m_state;

  /* Utilities */

  void dirty_ue_check();
  void measure_check();
  void macrep_check();
  int  prbs_from_dci(void * DCI, int dl, uint32_t cell_prbs);
  int  prbs_from_mask(int RA_format, uint32_t mask, uint32_t cell_prbs);

  /* Thread: */

  static void * agent_loop(void * args);

  /* Interaction with controller: */

  void send_MAC_report(uint32_t mod_id, ep_macrep_det * det);
  void send_UE_report(void);
  void send_UE_meas(em_ue::ue_meas * m);

};

} /* namespace srsenb */

#endif /* EMPOWER_AGENT_H */
