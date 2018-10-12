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


#include "srsenb/hdr/agent/dummy_agent.h"

namespace srsenb {

/******************************************************************************
 * Generic purposes procedures.                                               *
 ******************************************************************************/

/* Routine:
 *    dummy_agent::init
 * 
 * Abstract:
 *    Initializes the dummy_agent class instance. 
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - enb_id, ID of the eNB
 *    - rrc, pointer to RRC interface to be used
 *    - ran, pointer to RAN interface to be used
 *    - logger, pointer to logger instance to be used
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int dummy_agent::init(
    int                    enb_id,
    rrc_interface_agent *  rrc,
    ran_interface_common * ran,
    srslte::log *          logger)
{
  return 0;
}

/* Routine:
 *    dummy_agent::stop
 * 
 * Abstract:
 *    Stops any context associated with the agents. This operation leads to the
 *    releasing of the allocated resources too.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    ---
 */
void dummy_agent::stop()
{
  return;
}

/******************************************************************************
 * MAC interactions with the agent:                                           *
 ******************************************************************************/

/* Routine:
 *    dummy_agent::process_DL_results
 * 
 * Abstract:
 *    Process Downlink scheduling allocation results. This procedure does
 *    nothing in the dummy agent implementation.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - tti, current transmission time interval
 *    - sched_result, pointer to results to evaluate
 * 
 * Returns:
 *    ---
 */
void dummy_agent::process_DL_results(
  uint32_t tti, sched_interface::dl_sched_res_t * sched_result)
{
  return;
}

/* Routine:
 *    dummy_agent::process_UL_results
 * 
 * Abstract:
 *    Process Uplink scheduling allocation results. This procedure does
 *    nothing in the dummy agent implementation.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - tti, current transmission time interval
 *    - sched_result, pointer to results to evaluate
 * 
 * Returns:
 *    ---
 */
void dummy_agent::process_UL_results(
  uint32_t tti, sched_interface::ul_sched_res_t * sched_result)
{
  return;
}

/******************************************************************************
 * RRC interactions with the agent:                                           *
 ******************************************************************************/

/* Routine:
 *    dummy_agent::add_user
 * 
 * Abstract:
 *    Notifies the creation of an user.
 *    In dummy implementation this has no effects.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, Radio Network Temporary Identifier of the user
 * 
 * Returns:
 *    ---
 */
void dummy_agent::add_user(uint16_t rnti)
{
  return;
}

/* Routine:
 *    dummy_agent::rem_user
 * 
 * Abstract:
 *    Notifies the removal of an user.
 *    In dummy implementation this has no effects.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, Radio Network Temporary Identifier of the user
 * 
 * Returns:
 *    ---
 */
void dummy_agent::rem_user(uint16_t rnti)
{
  return;
}

/* Routine:
 *    empower_agent::update_user_ID
 * 
 * Abstract:
 *    RRC layer report an update in the UE identity through additional checks
 *    performed during message exchange with the Core Network.
 *    In dummy implementation this has no effects.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, ID of the user to update
 *    - plmn, PLMN ID of the user
 *    - imsi, Subscriber identity of the user
 *    - tmsi, Temporary identity of the user
 * 
 * Returns:
 *    ---
 */
void dummy_agent::update_user_ID(
  uint16_t rnti, uint32_t plmn, uint64_t imsi, uint32_t tmsi) 
{
  return;
}

/* Routine:
 *    dummy_agent::report_RRC_measure
 * 
 * Abstract:
 *    Notifies that a new measurement has been collected from UE.
 *    In dummy implementation this has no effects.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, Radio Network Temporary Identifier of the user
 *    - report, the measurement report
 * 
 * Returns:
 *    ---
 */
void dummy_agent::report_RRC_measure(
  uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report)
{
  return;
}

} /* namespace srsenb */
