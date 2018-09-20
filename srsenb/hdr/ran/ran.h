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

#include <list>
#include <map>

#ifndef __RADIO_ACCESS_NETWORK_SLICING_H
#define __RADIO_ACCESS_NETWORK_SLICING_H

#include <srslte/common/log.h>
#include <srslte/common/log_filter.h>
#include <srslte/interfaces/enb_interfaces.h>

// Default slice for all the new, unlocked, users
#define RAN_DEFAULT_SLICE     9622457614860288L // 0x00.222f93.00.000000

namespace srsenb {

// Map to identify users in the slice
typedef std::map<uint16_t, int> slice_user_map;

// This class contains the high-level description for a RAN slice.
class ran_slice {
public:
  uint64_t       id;    // Slice identifier
  uint32_t       plmn;  // PLMN this slice belongs to
  slice_user_map users; // List of users belonging to this slice
};

/* This class is the Radio Access Network (RAN) Manager for eNB equipment.
 *
 * The RAN Manager centralize the behavior of Slicing among all the different
 * layers which are part of the LTE stack, and will provide a gentle abstraction
 * and synchronization service to eNB modules (well, mainly towards the agent).
 * 
 * The RAN manager organizes procedures and data, but does not have any
 * threading context. This means that it will steal processing time to the 
 * caller context; be careful if you are accessing its functionalities from 
 * workers for Physical channels.
 */
class ran : public ran_interface_common
{
public:
  uint32_t l1_caps;
  uint32_t l2_caps;
  uint32_t l3_caps;

  // Initialize the RAN Manager internals
  int init(mac_interface_ran * mac, srslte::log * log);

  // Releases any allocated resource in a graceful way
  void release();

  /*
   * ran_interface_common
   * Procedures used by multiple layers to operate on RAN slicing mechanism
   */

  // Gets the number of existing slices
  int      get_slices(uint16_t nof, uint64_t * slices);
  // Gets the information for a single slice 
  int      get_slice_info(uint64_t   id, slice_args * args);
  // Adds a new slice inside the RAN mechanism
  int      add_slice(uint64_t id, uint32_t plmn);
  // Removess an existing slice from the RAN mechanism
  void     rem_slice(uint64_t id);
  // Set a slice properties 
  int      set_slice(uint64_t id, slice_args * args);
  // Adds a new user association inside the RAN mechanism
  int      add_slice_user(uint16_t rnti, uint64_t slice, int lock);
  // Removess an existing user association from the RAN mechanism
  void     rem_slice_user(uint16_t rnti, uint64_t slice);
  // Retrieves list of the user and their association with slices
  void     get_user_slices(uint16_t rnti, std::map<uint16_t, std::list<uint64_t> > & users);
  // Retrieves the current enabled RAN MAC slice scheduler
  uint32_t get_slice_sched();

private:
  // Interface for MAC communication 
  mac_interface_ran *             m_mac;

  // Logger class for the RAN
  srslte::log *                   m_log;

  // Slices currently present into the map
  std::map<uint64_t, ran_slice *> m_slices;
};

} // class ran

#endif // __RADIO_ACCESS_NETWORK_SLICING_H
