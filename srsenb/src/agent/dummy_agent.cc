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


#include "srsenb/hdr/agent/dummy_agent.h"

namespace srsenb {

/******************************************************************************
 * Generic purposes procedures.                                               *
 ******************************************************************************/

int dummy_agent::init(int             enb_id,
                      srslte::radio * rf,
                      srsenb::phy *   phy,
                      srsenb::mac *   mac,
                      srsenb::rlc *   rlc,
                      srsenb::pdcp *  pdcp,
                      srsenb::rrc *   rrc,
                      srslte::log *   logger)
{
  return 0;
}

void dummy_agent::stop()
{
  return;
}

/******************************************************************************
 * agent_interface_mac.                                                       *
 ******************************************************************************/

void dummy_agent::process_DL_results(
  uint32_t tti, sched_interface::dl_sched_res_t * sched_result)
{
  return;
}

void dummy_agent::process_UL_results(
  uint32_t tti, sched_interface::ul_sched_res_t * sched_result)
{
  return;
}

/******************************************************************************
 * agent_interface_rrc.                                                       *
 ******************************************************************************/

void dummy_agent::add_user(uint16_t rnti)
{
  return;
}

void dummy_agent::rem_user(uint16_t rnti)
{
  return;
}

void dummy_agent::report_RRC_measure(uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report)
{
  return;
}

} /* namespace srsenb */
