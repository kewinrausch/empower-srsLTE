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

#ifndef __AGENT_H
#define __AGENT_H

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

/* Generic Agent class.
 *
 * This class is just a generic back-bone class to give a shape to future 
 * alternative agents. An agent, in general view, is expected to interact with
 * the different levels of the base stations. The Agent class, as you can see,
 * is in fact extending public interfaces present in the library.
 * 
 * These interfaces provides how the Agent reacts to layers orders, NOT the 
 * opposite.
 */
class agent :
  public agent_interface_rrc, /* Agent interface for RRC layer */
  public agent_interface_mac, /* Agent interface for MAC layer */
  public agent_interface_ran  /* Agent interface for RAN manager */
{
public:
  /* Initializes the agent and prepare it for being used */
  virtual int init(
    int                    enb_id,
    rrc_interface_agent *  rrc,
    ran_interface_common * ran,
    srslte::log *          logger) = 0;

  /* Stop the agent functionalities and releases its resources */
  virtual void stop() = 0;
};

}

#endif /* __AGENT_H */
