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

/* This is the no-operation agent. */

#ifndef DUMMY_AGENT_H
#define DUMMY_AGENT_H

#include <srslte/common/log_filter.h>

#include "agent/agent.h"

namespace srsenb {

/* "Do-nothing" implementation for the agent subsystem. */
class dummy_agent : public agent
{
public:
  int init(int             enb_id,
           srslte::radio * rf,
           srsenb::phy *   phy,
           srsenb::mac *   mac,
           srsenb::rlc *   rlc,
           srsenb::pdcp *  pdcp,
           srsenb::rrc *   rrc,
           srslte::log *   logger);

  void stop();

  /* agent_interface_rrc: */

  void add_user(uint16_t rnti);
  void rem_user(uint16_t rnti);

private:
};

}

#endif /* DUMMY_AGENT_H */
