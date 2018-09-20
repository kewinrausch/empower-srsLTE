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

/* This is the no-operation agent. */

#ifndef __DUMMY_AGENT_H
#define __DUMMY_AGENT_H

#include <srslte/common/log_filter.h>

#include "srsenb/hdr/agent/agent.h"

namespace srsenb {

/* "Dummy agent provides an empty agent. Such agents is passive, and do not 
 * reacts to any events. Layer asking for service will receive a standard return
 * value, but nothing will be performed.
 */
class dummy_agent : public agent
{
public:
  /* Initialize and prepare the agent to do nothing */
  int init(
    int                    enb_id,
    rrc_interface_agent *  rrc,
    ran_interface_common * ran,
    srslte::log *          logger);

  /* Does not stop anything, since nothing is running */
  void stop();

  /* Interface for the MAC */

  void process_DL_results(
    uint32_t tti, sched_interface::dl_sched_res_t * sched_result);
  void process_UL_results(
    uint32_t tti, sched_interface::ul_sched_res_t * sched_result);

  /* Interface for RRC */

  void add_user(uint16_t rnti);
  void rem_user(uint16_t rnti);

  void report_RRC_measure(
    uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report);
};

}

#endif /* __DUMMY_AGENT_H */
