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
    printf("RAN: "fmt, ##__VA_ARGS__);   \
  } while(0)


namespace srsenb 
{

#ifdef HAVE_RAN_SLICER

/* Routine:
 *    ran::init
 * 
 * Abstract:
 *    Initializes the RAN slicing manager and prepare it to operate. During this
 *    stage also the 'default' slice is created. The dafult slice is in charge 
 *    of providing initial access to conneting UEs.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - mac, interface with the MAC layer
 *    - log, interface with logging mechanisms 
 * 
 * Returns:
 *    Zero on success, otherwise a negative error code.
 */
int ran::init(mac_interface_ran * mac, srslte::log * log) 
{
  l1_caps = 0;
  l2_caps = EP_RAN_LAYER2_CAP_PRB_SLICING;
  l3_caps = 0;

  m_log   = log;
  m_mac   = mac;

#if 0
  slice_args sargs;

  memset(&sargs, 0, sizeof(sargs));

  /*
   *
   * Default Slice configuration and addition into the RAN subsystem:
   * 
   */
  sargs.l2.mac.user_sched = 1;
  sargs.l2.mac.rbg        = 0;
  sargs.l2.mac.time       = -1;

  add_slice(RAN_DEFAULT_SLICE, 0);
  set_slice(RAN_DEFAULT_SLICE, &sargs);
#endif
  return 0;
}

/* Routine:
 *    ran::release
 * 
 * Abstract:
 *    Releases all the resources previously allocated during initialization.
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
void ran::release()
{
  return;
}

/* Routine:
 *    ran::id_to_plmn
 * 
 * Abstract:
 *   Translates a slice ID into PLMN ID components
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    id  - ID of the slice to analyze
 *    mcc - mobile country code
 *    mnc - mobile network code
 * 
 * Returns:
 *    Zero on success, otherwise a negative error code
 */
int ran::id_to_plmn(uint64_t id, int * mcc, int * mnc)
{
  if(!mcc || !mnc) {
    return -1;
  }

  *mnc = (id >> 32) & 0xfff;
  *mcc = (id >> 44) & 0xfff;

  return 0;
}

/* Routine:
 *    ran::plmn_to_id
 * 
 * Abstract:
 *   Translates a PLMN ID to a slice ID.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    mcc - mobile country code
 *    mnc - mobile network code
 * 
 * Returns:
 *    The slice ID relative to the given PLMN
 */
uint64_t ran::plmn_to_id(int mcc, int mnc)
{
  return ((((uint64_t)mcc & 0xfff) << 12) | ((uint64_t)mnc & 0xfff)) << 32;
}

/******************************************************************************
 *                                                                            *
 *                    Implementation of common interface                      *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    ran::get_slices
 * 
 * Abstract:
 *    Gets the current active slices in the subsystem up to 'nof'. Their IDs are
 *    saved in the given array.
 * 
 * Assumptions:
 *    Array is correctly set on a valid area of memory.
 * 
 * Arguments:
 *    - nof, Maximum amount of slices to store in the 'slices' array.
 *    - slices, Arrays which will contains the slices IDs.
 * 
 * Returns:
 *    Zero on success, otherwise a negative error code.
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

/* Routine:
 *    ran::get_slice_info
 * 
 * Abstract:
 *    Gets the current state of a particular slice.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to query.
 *    - info, pointer to a structure which will holds the results.
 * 
 * Returns:
 *    Zero on success, otherwise a negative error code.
 */
int ran::get_slice_info(uint64_t id, slice_args * info)
{
  uint32_t                 count = 0;
  slice_user_map::iterator it;
  mac_set_slice_args       args;

  if(m_slices.count(id) == 0) {
    Error("Cannot get info; slice %" PRIu64 " not found!\n", id);
    return -1;
  }

  /*
   * Retrieve information from the MAC layer
   */
  m_mac->get_slice(id, &args);
  info->l2.mac.user_sched = args.user_sched;
  info->l2.mac.rbg        = args.rbg;

  for(it = m_slices[id]->users.begin(); it != m_slices[id]->users.end(); ++it) {
    if(count >= info->nof_users) {
      break;
    }

    info->users[count] = it->first;
    count++;
  }

  info->nof_users = count;

  return 0;
}

/* Routine:
 *    ran::add_slice
 * 
 * Abstract:
 *    Adds a new slice to the RAN subsystem.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to add.
 *    - plmn, Public Land Mobile Network id associated with the slice.
 * 
 * Returns:
 *    Zero on success, otherwise a negative error code.
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

  Debug("Slice created, id=%" PRIu64 " PLMN=%x\n", id, plmn);

  return 0;
}

/* Routine:
 *    ran::rem_slice
 * 
 * Abstract:
 *    Removes an existing slice from the RAN subsystem.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to remove.
 * 
 * Returns:
 *    ---
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

  Debug("Slice %" PRIu64 " removed!\n", id);

  return;
}

/* Routine:
 *    ran::set_slice
 * 
 * Abstract:
 *    Set the configuration of a newly created slice to change its default
 *    behavior.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to remove.
 *    - info, information of the slice to apply.
 * 
 * Returns:
 *    Zero on success, otherwise a negative error code.
 */
int ran::set_slice(uint64_t id, slice_args * info)
{
  unsigned int             i;
  slice_user_map::iterator it;
  mac_set_slice_args       mac_args;

  if(id == 0 || !info) {
    Error("Invalid arguments on set_slice, slice=%" PRIu64 ", info=%p\n", 
      id, info);

    return -1;
  }

  if(m_slices.count(id) == 0) {
    Error("Slice %" PRIu64 " not found!\n", id);
    return -1;
  }

  memset(&mac_args, 0, sizeof(mac_set_slice_args));

  if(info->l2.mac.user_sched > 0) {
    mac_args.user_sched = info->l2.mac.user_sched;
  }

  if(info->l2.mac.rbg > 0) {
    mac_args.rbg = info->l2.mac.rbg;
  }

  if(info->l2.mac.time > 0) {
    mac_args.time = info->l2.mac.time;
  }

  // Set options for that slice
  m_mac->set_slice(id, &mac_args);

rep:
  // Remove users which are no more part of the slice here
  for(it = m_slices[id]->users.begin(); it != m_slices[id]->users.end(); ++it) {
    // Check the situation arrived
    for(i = 0; i < info->nof_users; i++) {
      if(info->users[i] == it->first) {
        break;
      }
    }

    // User not found in the arrived report; remove it
    if(i == info->nof_users) {
      rem_slice_user(it->first, id);

      /* IMPORTANT: 
       * Iterator is not more valid here, since rem_slice_user operate on the 
       * same base of data. Please renew it by jumping at the start.
       * 
       * Does incrementing it works, by the way?
       */
      goto rep;
    }
  }

  // Time to add now new users
  for(i = 0; i < info->nof_users; i++) {
    // User not present?
    if(m_slices[id]->users.count(info->users[i]) == 0) {
      m_slices[id]->users[info->users[i]] = 1;

      // Add and lock it, since the controller asked to interact with such user
      add_slice_user(info->users[i], id, 1);
    }
  }

  return 0;
}

/* Routine:
 *    ran::add_slice_user
 * 
 * Abstract:
 *    Adds an user association with a well identified slice. If the slice does
 *    not exists, it is created anew using default values; this means that
 *    adding users to slice usually terminate with success.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, Radio Network Temporary Identifier of the user.
 *    - slice, ID of the slice to associate with.
 *    - lock, Is the scheduler able to take personal decision on the user 
 *            allocation? 0 means free to move/operate the user, 1 is locked.
 * 
 * Returns:
 *    Zero on success, otherwise a negative error code.
 */
int  ran::add_slice_user(uint16_t rnti, uint64_t slice, int lock)
{
  int        mnc;
  int        mcc;
  slice_args sargs;

  if(rnti == 0) {
    Error("Invalid arguments on add_user, rnti=%d, slice=%" PRIu64 "\n", 
      rnti, slice);

    return -1;
  }

  // If slice is not specified, use the default one
  if(slice == 0) {
    slice = RAN_DEFAULT_SLICE;
  }

  // If slice is not present, add it
  if(m_slices.count(slice) == 0) {
    memset(&sargs, 0, sizeof(sargs));

    // Some standard values which allows RRC connection (1.4 MHz base)
    sargs.l2.mac.user_sched = RAN_MAC_USER_RR;  // Round-robin
    // 6 RBG per sub-frame
    sargs.l2.mac.rbg        = 60;
    sargs.l2.mac.time       = 10;

    ran::id_to_plmn(slice, &mcc, &mnc);

    if(add_slice(slice, ((mcc << 12) | mnc))) {
      return -1;
    }

    // Set the newly created slice
    set_slice(slice, &sargs);
  }

  if(m_mac->add_slice_user(rnti, slice, lock)) {
    Error("Failed to MAC user %d in %" PRIu64 "\n", rnti, slice);
    return -1;
  }
  
  m_slices[slice]->users[rnti] = 1;

  Debug("User %d added to slice %" PRIu64 "\n", rnti, slice);

  return 0;
}

/* Routine:
 *    ran::rem_slice_user
 * 
 * Abstract:
 *    Removes an user association with a well identified slice.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, Radio Network Temporary Identifier of the user.
 *    - slice, ID of the slice to remove the user from. Setting this to 0 will
 *             cause the removal of ANY user association with slices.
 * 
 * Returns:
 *    ---
 */
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
          Debug("Removing user %d from %" PRIu64 "\n", rnti, it->first);
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
      Debug("Removing user %d from %" PRIu64 "\n", rnti, slice);
    }
  }
  
  return;
}

/* Routine:
 *    ran::get_slice_sched
 * 
 * Abstract:
 *    Gets the ID of the current active slice scheduler.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    The ID of the slice scheduler currently processing.
 */
uint32_t ran::get_slice_sched()
{
  return m_mac->get_slice_sched();
}

#else  // HAVE_RAN_SLICER

// Placeholder; see comments of the real implementation here at the top
int ran::init(mac_interface_ran * mac, srslte::log * log) 
{
  m_log   = log;
  m_mac   = mac;

  Error("The RAN slicer is disabled!\n");

  return 0;
}

// Placeholder; see comments of the real implementation here at the top
void ran::release()
{
  return;
}

// Placeholder; see comments of the real implementation here at the top
int ran::get_slices(uint16_t nof, uint64_t * slices)
{
  return 0;
}

// Placeholder; see comments of the real implementation here at the top
int ran::get_slice_info(uint64_t   id, slice_args * info)
{
  return -1;
}

// Placeholder; see comments of the real implementation here at the top
int ran::add_slice(uint64_t id, uint32_t plmn)
{
  return -1;
}

// Placeholder; see comments of the real implementation here at the top
void ran::rem_slice(uint64_t id)
{
  return;
}

// Placeholder; see comments of the real implementation here at the top
int ran::set_slice(uint64_t id, slice_args * info)
{
  return -1;
}

// Placeholder; see comments of the real implementation here at the top
int  ran::add_slice_user(uint16_t rnti, uint64_t slice, int lock)
{
  return -1;
}

// Placeholder; see comments of the real implementation here at the top
void ran::rem_slice_user(uint16_t rnti, uint64_t slice)
{
  return;
}

// Placeholder; see comments of the real implementation here at the top
uint32_t ran::get_slice_sched()
{
  return 0;
}

#endif // HAVE_RAN_SLICER

}