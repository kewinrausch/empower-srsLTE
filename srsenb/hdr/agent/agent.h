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

#ifndef AGENT_H
#define AGENT_H

#include <srslte/common/log.h>
#include <srslte/common/log_filter.h>
#include <srslte/interfaces/enb_interfaces.h>
#include <srslte/radio/radio.h>

#include "srsenb/hdr/phy/phy.h"
#include "srsenb/hdr/mac/mac.h"
#include "srsenb/hdr/upper/rrc.h"
#include "srsenb/hdr/upper/rlc.h"
#include "srsenb/hdr/upper/pdcp.h"
#include "srsenb/hdr/ran/ran.h"

namespace srsenb {

class agent :
  public agent_interface_rrc,
  public agent_interface_mac,
  public agent_interface_ran
{
public:
  /* Initializes the agent with the layers to interact with.
   * Returns 0 on success, otherwise a negative error code.
   */
  virtual int init(
    int enb_id,
    rrc_interface_agent * rrc,
    ran_interface_agent * ran,
    srslte::log * logger) = 0;

  /* Stops the agent processing and terminates it. */
  virtual void stop() = 0;
};

}

#endif
