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

#include <stdint.h>
#include <inttypes.h>

#include <emage/emproto.h>

#include "srsenb/hdr/ran/ran.h"

#define Error(fmt, ...)                        \
  do {                                         \
    m_log->error("RAN: "fmt, ##__VA_ARGS__);   \
  } while(0)

#define Warning(fmt, ...)                      \
  do {                                         \
    m_log->warning("RAN: "fmt, ##__VA_ARGS__); \
  } while(0)

#define Info(fmt, ...)                         \
  do {                                         \
    m_log->info("RAN: "fmt, ##__VA_ARGS__);    \
  } while(0)

#define Debug(fmt, ...)                        \
  do {                                         \
    m_log->debug("RAN: "fmt, ##__VA_ARGS__);   \
  } while(0)


namespace srsenb 
{

int ran::init(mac_interface_ran * mac, srslte::log * log) 
{
  ran_set_slice_args mac_args = {0};

  l1_caps = 0;
  l2_caps = EP_RAN_LAYER2_CAP_PRB_SLICING;
  l3_caps = 0;

  m_log = log;
  m_mac = mac;

  // Mac properties for the tenant 
  mac_args.user_sched = 1;
  mac_args.rbg        = 7;

  /* Add the default slice */
  add_slice(9622457614860288L, 0); /* 2463349149404233728L --> 0x00222f9300000000 */
  set_slice(9622457614860288L, &mac_args);
        
  return 0;
}

void ran::release()
{
  return;
}

/*
 * ran_interface_agent
 * Procedures used by the agent to operate on RAN slicing mechanism
 */

/* Returns a list of slices currently registered in the system.
 * Returns 0 if operation is successful, otherwise a negative error code.
 */
int ran::get_slices(uint16_t nof, uint64_t * slices)
{
  int count = 0; /* Counter of processed slices */
  std::map<uint64_t, ran_slice *>::iterator it;

  for(it = m_slices.begin(), count = 0; it != m_slices.end(); ++it) {
    if(count >= nof) {
      break;
    }

    slices[count] = it->first;
    count++;
  }

  return count;
}

/* Returns detailed information of a single slice.
 * Returns 0 if operation is successful, otherwise a negative error code.
 */
int ran::get_slice_info(
  uint64_t   id, 
  uint32_t * sched, 
  uint16_t * prbs, 
  uint16_t * users, 
  uint32_t * nof_users)
{
  uint32_t                 count = 0;
  slice_user_map::iterator it;
  mac_set_slice_args       args  = {0};

  if(m_slices.count(id) == 0) {
    return -1;
  }

  //*sched = m_slices[id]->l2.sched;
  //*prbs  = m_slices[id]->l2.rbg;
  // Retrieve information from the MAC layer
  m_mac->get_slice(id, &args);

  // Update RAN abstraction base of data
  //m_slices[id]->l2.sched = args.user_sched;
  //m_slices[id]->l2.rbg   = args.rbg;

  *sched = args.user_sched;
  *prbs  = args.rbg;

  for(it = m_slices[id]->users.begin(); it != m_slices[id]->users.end(); ++it) {
    if(count >= *nof_users) {
      break;
    }

    users[count] = it->first;
    count++;
  }

  *nof_users = count;

  return 0;
}

/* Adds a new slice inside the RAN slicer mechanism.
 * Returns 0 if operation is successful, otherwise a negative error code.
 */
int ran::add_slice(uint64_t id, uint32_t plmn)
{
  int ret = 0;

  // Some arguments validation before starting
  if(id == 0) {
    Error("Invalid arguments during slice addition, id=%" PRIu64 ", plmn=%u\n", 
      id, plmn);
    return -1;
  }

  if(m_slices.count(id) > 0) {
    Error("Slice %" PRIu64 " already exists\n", id);
    return -1;
  }

  ret = m_mac->add_slice(id);

  if(ret) {
    return ret;
  }

  m_slices.insert(std::make_pair(id, new ran_slice()));
  
  if(m_slices.count(id) == 0) {
    m_mac->rem_slice(id);

    Error("No more memory for new slices!\n");
    return -1;          
  }

  m_slices[id]->id   = id;
  m_slices[id]->plmn = plmn;

printf("Slice created, id=%" PRIu64 " PLMN=%x\n", id, plmn);

  return 0;
}

/* Removes an existing slice from the RAN slicer mechanism.
 */
void ran::rem_slice(uint64_t id)
{
  ran_slice * s;

  std::map<uint64_t, ran_slice *>::iterator it;

  // Some arguments validation before starting
  if(id == 0) {
    Error("Invalid arguments during slice removal, id=%" PRIu64 "\n", id);
    return;
  }

  it = m_slices.find(id);

  if(it == m_slices.end()) {
    Error("Slice %" PRIu64 " not found during removal!\n", id);
    return;
  }

  s = it->second;

  // Removes the netry from the map 
  m_slices.erase(it);
  
  /*
   * Operate on other Layers to remove the slice here
   */

  m_mac->rem_slice(id);

  delete s;

printf("Slice %" PRIu64 " removed!\n", id);

  return;
}

int ran::set_slice(uint64_t id, ran_set_slice_args * args)
{
  unsigned int             i;
  slice_user_map::iterator it;
  mac_set_slice_args       mac_args = { 0 };

  if(id == 0 || !args) {
    Error("Invalid arguments on set_slice, slice=%" PRIu64 ", args=%p\n", 
      id, args);

    return -1;
  }

  if(m_slices.count(id) == 0) {
    Error("Slice %" PRIu64 " not found!\n", id);
    return -1;
  }

  if(args->user_sched > 0) {
  //  m_slices[id]->l2.sched = args->user_sched;
    mac_args.user_sched    = args->user_sched;
  }

  if(args->rbg > 0) {
    //m_slices[id]->l2.rbg   = args->rbg;
    mac_args.rbg           = args->rbg;
  }

  // Set options for that slice
  m_mac->set_slice(id, &mac_args);

rep:
  // Iterate on the current users of the slice
  for(it = m_slices[id]->users.begin(); it != m_slices[id]->users.end(); ++it) {
    // Check the situation arrived
    for(i = 0; i < args->nof_users; i++) {
      if(args->users[i] == it->first) {
        break;
      }
    }

    // The current user is not listed in the arrived one, and should be removed
    if(i == args->nof_users) {
      rem_slice_user(it->first, id);
      // Iterator is not more valid here, since rem_slice_user operate on the 
      // same base of data. Please renew it by jumping at the start
      goto rep;
    }
  }

  // Time to add now new users
  for(i = 0; i < args->nof_users; i++) {
    // User not present?
    if(m_slices[id]->users.count(args->users[i]) == 0) {
      m_slices[id]->users[args->users[i]] = 1;

      // Add and lock it, since the controller asked to interact with such user
      add_slice_user(args->users[i], id, 1);
    }
  }

printf("Slice %" PRIu64 " set to: usched=%d, prbs=%d\n",
    id, args->user_sched, args->rbg);

  return 0;
}

int  ran::add_slice_user(uint16_t rnti, uint64_t slice, int lock)
{
  if(rnti == 0 || slice == 0) {
    Error("Invalid arguments on add_user, rnti=%d, slice=%" PRIu64 "\n", 
      rnti, slice);

    return -1;
  }

  if(m_slices.count(slice) == 0) {
    Error("Slice %" PRIu64 " not found!\n", slice);
    return -1;
  }

  if(m_mac->add_slice_user(rnti, slice, lock)) {
    Error("Failed to MAC user %d in %" PRIu64 "\n", rnti, slice);
    return -1;
  }
  
  m_slices[slice]->users[rnti] = 1;

printf("User %d added to slice %" PRIu64 "\n", rnti, slice);

  return 0;
}

void ran::rem_slice_user(uint16_t rnti, uint64_t slice)
{
  std::map<uint64_t, ran_slice *>::iterator it;
  
  if(rnti == 0) {
    Error("Invalid arguments on rem_user, rnti=%d, slice=%" PRIu64 "\n", 
      rnti, slice);

    return;
  }

  m_mac->rem_slice_user(rnti, slice);

  // Remove any user
  if(slice == 0) {
     for(it = m_slices.begin(); it != m_slices.end(); ++it) { 
       if(it->second->users.count(rnti) > 0) {
         it->second->users.erase(rnti);
printf("Removing user %d from %" PRIu64 "\n", rnti, it->first);
       }
     }
  }
  else {
    if(m_slices.count(slice) == 0) {
      Error("Slice %" PRIu64 " not found!\n", slice);
      return;
    }

    if(m_slices[slice]->users.count(rnti) > 0) {
      m_slices[slice]->users.erase(rnti);
printf("Removing user %d from %" PRIu64 "\n", rnti, slice);
    }
  }
  
  return;
}

void ran::get_user_slices(uint16_t rnti, std::map<uint16_t, std::list<uint64_t> > & users) 
{
  std::map<uint64_t, ran_slice *>::iterator ti;
  slice_user_map::iterator                  ui;
/*
  // Collect info for every active slice in the system 
  for (ti = m_slices.begin(); ti != m_slices.end(); ++ti) {
    for (ui = ti->second->users.begin(); ui != ti->second->users.end(); ++ui) {
      // We are looking for every information in our data
      if(rnti == 0) {
        users[*ui].push_back(ti->first);
      } else {
        // We are looking for a specific RNTI
        if(rnti == *ui) {
          users[*ui].push_back(ti->first);
        }
      }
    }
  }
*/
  return;
}

uint32_t ran::get_slice_sched()
{
  return m_mac->get_slice_sched();
}

}