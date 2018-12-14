/**
 * \section AUTHOR
 *
 * Author: Kewin Rausch
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2018 Software Radio Systems Limited
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

#ifndef __EMPOWER_AGENT_H
#define __EMPOWER_AGENT_H

#include <map>
#include <pthread.h>
#include <string>

#include <srslte/common/log_filter.h>
#include <srslte/common/threads.h>

#include <emage/emproto.h>

#include "srsenb/hdr/agent/agent.h"

namespace srsenb {

#define EMPOWER_AGENT_MAX_UE            32
#define EMPOWER_AGENT_MAX_MEAS          32
#define EMPOWER_AGENT_MAX_CELL_MEAS     8
#define EMPOWER_AGENT_MAX_MACREP        8

// State of the agent
enum agent_state {
  // Agent is not processing 
  AGENT_STATE_STOPPED = 0,
  // Agent task processing is paused 
  AGENT_STATE_PAUSED,
  // Agent is processing data/tasks 
  AGENT_STATE_STARTED,
};

// Redefinition of timespec in a shorter way
typedef struct timespec em_time;

/* The Physical Resource Block report.
 *
 * Contains logic which identifies and store values valid for such reports.
 * This class contains logic for the measurements and calculations too.
 */
class em_prb_report {
public:
  mod_id_t m_module_id;  // Module ID bound to this measurement
  int      m_trigger_id; // Trigger ID bound to this measurement
  uint32_t m_interval;   // Interval in ms
  uint32_t m_DL;         // Downlink resources accumulator
  uint32_t m_UL;         // Uplink resources accumulator
  em_time  m_last;       // Last time the measurement has been computed

  em_prb_report();
  ~em_prb_report();

  // Perform computations over the single PRB report
  int compute();

  // Reset the cell to accomodate a starting state
  int reset();
private:
}; /* class em_prb_report */

/* The MAC layer definition for a signle Empower Cell.
 *
 * Contains logic valid for the Empower Agent which is related to the MAC layer
 * of the LTE stack.
 */
class em_mac {
public:
  // Number of Physical Resource Blocks used by the cell
  int           m_prbs;
  // PRBs reports contexts
  em_prb_report m_prb_ctx;

  em_mac();
  ~em_mac();

  // Perform computations over the MAC context
  int compute();

  // Reset the cell to accomodate a starting state
  int reset();
private:
}; // class em_mac

/* The Cell definition for Empower Agent.
 *
 * It aggregates logic and measurement that are related to a cell, and have
 * a meaning within the cell only.
 */
class em_cell {
public:
  uint16_t m_pci; // Physical Cell Id
  em_mac   m_mac; // MAC layer context for the cell

  em_cell();
  ~em_cell();

  // Perform computations over the cell context
  int compute();

  // Reset the cell to accomodate a starting state
  int reset();
private:
}; /* class em_cell */

/* EmPOWER Agent UE class.

  * This organizes data and procedures which are relative to a certain UE from
  * an Empower-agent perspective. since this class is only internally used, all
  * its elements are public.
  */
class em_ue {
public:
  // UE measurement container for a single cell
  typedef struct {
    int      dirty; // Data is new?
    uint16_t pci;   // Physical Cell ID
    uint8_t  rsrp;  // Signal power
    uint8_t  rsrq;  // signal quality
  } ue_cell_meas;

  // UE measurement container for a requesting module
  typedef struct {
    uint32_t     id;      // ID assigned by agent-controller circuit
    uint32_t     mod_id;  // ID of the requesting module
    int          trig_id; // ID of the trigger assigned

    uint32_t     meas_id; // Measure Id on the UE-eNB circuit
    uint32_t     obj_id;  // Object Id on the UE-eNB circuit
    uint32_t     rep_id;  // Reprot Id on the UE-eNB circuit

    uint16_t     freq;    // Frequency to measure, EARFCN
    uint16_t     max_cells; // Max cell to report*/
    uint16_t     max_meas;// Max measure to take
    int          interval;// Measurement interval

    ue_cell_meas carrier; // Report of the carrier signal
    int          c_dirty; // Carrier signal dirty?

    // Reports of all the other cells
    ue_cell_meas neigh[EMPOWER_AGENT_MAX_CELL_MEAS];
  } ue_meas;

  uint8_t  m_state; // State of the UE
  int      m_state_dirty; // State has to be updated?

  uint64_t m_imsi; // International Mobile Subscriber Identity
  uint32_t m_plmn; // Public Land Mobile Network
  uint32_t m_tmsi; // Temporary Mobile Subscriber Identity
  int      m_id_dirty; // Identity has to be updated?

  uint32_t m_next_meas_id; // Next Id for UE ue_meas.meas_id
  uint32_t m_next_obj_id;  // Next Id for UE ue_meas.obj_id
  uint32_t m_next_rep_id;  // Next Id for UE ue_meas.rep_id

  ue_meas  m_meas[EMPOWER_AGENT_MAX_MEAS]; // Measurements

  em_ue();
  ~em_ue();
}; // class em_ue

/* The EmPOWER Agent.
 *
 * This Agent has the objective to exchange information with an EmPOWER 
 * controller, and react to feedback incoming from it. This will be done by
 * using EmPOWER protocols for the communication with the controller.
 */
class empower_agent : public agent
{
public:
  // Maximum amount of managed cell for a single agent
  static const int MAX_CELLS = 4;

  uint32_t m_RAN_def_dirty;

  empower_agent();
  ~empower_agent();

  // Get th ID of the agent
  unsigned int           get_id();

  // Get a reference to the RAN interface
  ran_interface_common * get_ran();

  /* Initialize the agent and prepare it for handling controller and stack
   * events.
   */
  int  init(
    int                    enb_id,
    rrc_interface_agent *  rrc,
    ran_interface_common * ran,
    srslte::log *          logger);

  void stop();

  // Release any reserved resource
  void release();

  // Reset agent data and state machines to their starting values
  int  reset();

  // Request an UE report to the agent
  int  setup_UE_report(uint32_t mod_id, int trig_id);

  // Request a cell measurement to the agent
  int  setup_cell_measurement(
   uint16_t cell_id, uint32_t mod_id, uint32_t interval, int trig_id);

  // Request an UE measurement to the agent
  int  setup_UE_period_meas(
    uint32_t id,
    int      trigger_id,
    uint16_t rnti,
    uint32_t mod_id,
    uint16_t freq,
    uint16_t max_cells,
    uint16_t max_meas,
    int      interval);

  // Request a RAN report to the agent
  int setup_RAN_report(uint32_t mod);

  /* 
   *
   * Interface for the MAC layer:
   * 
   */

  // Process Downlink scheduling result incoming from the MAC layer
  void process_DL_results(
    uint32_t tti, sched_interface::dl_sched_res_t * sched_result);

  // Process Uplink scheduling result incoming from the MAC layer
  void process_UL_results(
    uint32_t tti, sched_interface::ul_sched_res_t * sched_result);

  /* 
   *
   * Interface for the RRC layer:
   * 
   */

  // Add an user into the agent subsystem
  void add_user(uint16_t rnti);

  // Remove an user from the agent subsystem
  void rem_user(uint16_t rnti);

  // Update identity information for a certain UE
  void update_user_ID(
    uint16_t rnti, uint32_t plmn, uint64_t imsi, uint32_t tmsi);

  // Report an RRC measurement arrived from an UE
  void report_RRC_measure(
    uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report);

private: // class empower_agent

  unsigned int           m_id; // ID of the agent/eNB

  rrc_interface_agent  * m_rrc; // Pointer to RRC interface
  ran_interface_common * m_ran; // Pointer to RAN interface

  srslte::log *          m_logger; // Pointer to Agent logger instance

  void *                 m_args; // eNB arguments

  em_cell                m_cells[MAX_CELLS]; // Cells contexts

  int                    m_uer_feat; // UE reporting feature enabled?
  int                    m_uer_tr; // UE reporting feature trigger
  unsigned int           m_uer_mod; // UE reporting feature module ID

  // UE-report related variables

  int                    m_nof_ues;
  std::map<uint16_t, em_ue *> m_ues; // Map of User Equipments
  int                    m_ues_dirty; // Are there modifications to report?

  // Cell measurement related variables
  
  int                    m_cm_feat; // Cell measurement feature is enabled?

  // RAN-related variables

  uint32_t               m_RAN_feat; // RAN feature enabled?
  //uint32_t               m_RAN_def_dirty; // Modifications at RAN level?
  uint32_t               m_RAN_mod; // RAN module ID to use

  // Threading-related variables

  pthread_t              m_thread; // Agent reporting and servicing thread
  pthread_spinlock_t     m_lock; // Lock for the thread
  int                    m_state; // State of the agent thread

  /* 
   * 
   * Generic utilities for the Agent 
   * 
   */

  // Perform a check on UE reporting mechanism
  void dirty_ue_check();

  // Perform a check on UE measurement reporting mechanism
  void measure_check();

  // Perform a check on Cell PRBs measurements
  //void cellprb_check();

  // Perform a check on RAN reporting mechanism
  void ran_check();

  // Get number of PRBs used from DCI
  int  prbs_from_dci(void * DCI, int dl, uint32_t cell_prbs);
  
  // Get number of PRBs used from bitmask
  int  prbs_from_mask(int RA_format, uint32_t mask, uint32_t cell_prbs);

  // Thread main loop
  static void * agent_loop(void * args);

  /* 
   * 
   * Interaction with controller
   * 
   */

  // Send a MAC report to the controller
  //void send_MAC_report(uint32_t mod_id, ep_macrep_det * det);

  // Send an UE report to the controller
  void send_UE_report(void);

  // Send an UE measurement report to the controller
  void send_UE_meas(em_ue::ue_meas * m);

#ifdef HAVE_RAN_SLICER
  // Send a slices feedback to the controller
  void send_slice_feedback(uint32_t mod);
#endif

}; // class empower_agent

} // namespace srsenb

#endif // __EMPOWER_AGENT_H 
