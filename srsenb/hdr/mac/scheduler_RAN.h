/**
 * \section AUTHOR
 *
 * Author: Kewin Rausch
 *
 * \section MODULE
 *
 * Radio Access Network sharing module.
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

#ifndef __SCHED_METRIC_RAN_H
#define __SCHED_METRIC_RAN_H

#include <stdint.h>
#include <list>
#include <map>
#include <pthread.h>

#include "srsenb/hdr/ran/ran.h"
#include "srsenb/hdr/mac/scheduler.h"

// Decorators for arguments
#define __ran_in_
#define __ran_out_

#define RAN_PRB_1_4             6
#define RAN_PRB_3               15
#define RAN_PRB_5               25
#define RAN_PRB_10              50
#define RAN_PRB_15              75
#define RAN_PRB_20              100

#define RAN_DL_RGS_1_4          1
#define RAN_DL_RGS_3            2
#define RAN_DL_RGS_5            2
#define RAN_DL_RGS_10           3
#define RAN_DL_RGS_15           4
#define RAN_DL_RGS_20           4

#define RAN_DL_MAX_RGB          25

#define RAN_SLICE_INVALID       0
#define RAN_SLICE_STARTING      RAN_DEFAULT_SLICE
#define RAN_USER_INVALID        0

// Define/undefine this symbol to trace the RAN
//#define RAN_TRACE

/* RAN tracing capabilities:
 *
 * This set of procedure and data structures are here with the only purpose to
 * provides non-invasive (for MAC scheduler) statistics over what is happening
 * in the RAN scheduler.
 *
 * Undefine RAN_TRACE to disable.
 */
#ifdef RAN_TRACE

#define RTRACE_INTERVAL                 1000

#define RTRACE_NOF_UMASKS               32
#define RTRACE_NOF_MCS                  32

// Meaninful statistics for a specific user
typedef struct __ran_trace_users {
  // There has been some activities?
  bool active;

  // Allocated mask setup in the DL
  uint32_t dl_rbg_masks[RTRACE_NOF_UMASKS];
  // How many time that mask was allocated
  uint32_t dl_rbg_count[RTRACE_NOF_UMASKS];
  // MCS adopted when sending data on the DL
  int dl_rbg_mcs[RTRACE_NOF_MCS];

} rt_user;

// Statistics of the RAN scheduler
typedef struct __ran_trace_stats {
  // Number of TTIs for this data to consider
  uint32_t nof_tti;

  // Users-related statistics
  std::map<uint16_t, rt_user> users;
} rt_stats;

// Master container for RAN tracing
typedef struct __ran_trace_data {
  // Logger used to dump stats
  srslte::log * logger;

  // Useful statistics
  rt_stats stats;
} rt_data;

void ran_trace_tti(rt_data * rtd);
void ran_trace_DL_mask(rt_data * rtd, uint16_t rnti, uint32_t mask, int mcs);

#define rtrace_new_tti(r)               ran_trace_tti(r)
#define rtrace_dl_mask(r, n, m, c)      ran_trace_DL_mask(r, n, m, c)

#else

#define rtrace_new_tti(r)               // ...into nothing
#define rtrace_dl_mask(r, n, m, c)      // ...into nothing
#endif // RAN_TRACE

namespace srsenb {

/******************************************************************************
 *                                                                            *
 * Generic purposes strutures and classes:                                    *
 *                                                                            *
 ******************************************************************************/

class ran_user_scheduler;

// Single user of the RAN schedulers
class ran_mac_user {
public:
  // Managend locally by the scheduler
  int      self_m;

  // How many TTI's ago did we saw this one?
  uint32_t last_seen;

  // Has data that should be tx/re-tx? This can be used by schedulers
  int      has_data;

  // The amount of bytes exchanged in DL at MAC level
  uint64_t DL_data;
  // The amount of bytes exchanged in DL at MAC level during last TTI
  uint32_t DL_data_delta;
  // The amount of PRBG used in DL at MAC level during last TTI
  uint32_t DL_rbg_delta;
};

/* Type which describes the map of users information stored by the RAN
 * subsystem.
 */
typedef std::map<uint16_t, ran_mac_user> user_map_t;

// How a Slice is organized for the RAN scheduler logic
class ran_mac_slice {
public:
  // User scheduler associated with this slice
  ran_user_scheduler *    sched_user;

  /* Users of this slice
   *
   * IMPORTANT:
   * To avoid conflicts this should be organized in TTI views, since multiple 
   * workers can access the same data (ASSUMPTION). Since also ue_db is shared 
   * and no conflicts happens maybe everything is already managed somewhere in 
   * the code, but worth noticing this in case of future errors.
   */
  std::map<uint16_t, int> users;
};

/* Type which describes the map of slices information stored by the RAN 
 * subsystem.
 */
typedef std::map<uint64_t, ran_mac_slice> slice_map_t;

/******************************************************************************
 *                                                                            *
 * Schedulers shapes for all the algorithms:                                  *
 *                                                                            *
 ******************************************************************************/

// Provides the root class for all the schedulers at RAN level, of any level
class ran_scheduler {
public:
  virtual ~ran_scheduler() 
  {
    // Do nothing
  }

  /* ID for the scheduler.
   *
   * Please notes that User-level scheduler for slices start with the most 
   * significant bit as 1, while slice schedulers have it set to 0. This means 
   * that user-level schedulers will be organized like 0x8.... , while 
   * slice-level scheduler are organized as 0x0....
   * 
   * At the end this ID must be sync between controller and base station.
   */
  uint32_t m_id;
};

/* Provides the common shape for an User scheduler at RAN level.
 *
 * User schedulers are invoked after the slice one, and organize users which 
 * belong to a common slice.
 *
 * Inputs:
 *    - TTI,    which subframe is scheduled.
 *    - slice,  Slice which is currently scheduled.
 *    - rgb,    array of the usable PRBG in this subframe for the Slice.
 *
 * Output:
 *    The Slice scheduler uses the given input arguments to polish the 'ret'
 *    array. This variable identifies group per group assignment of resources to
 *    User Equipments RNTI.
 */
class ran_user_scheduler : public ran_scheduler {
public:
  virtual ~ran_user_scheduler() 
  {
    // Do nothing
  }

  // Schedule Users following the implemented strategy
  virtual void schedule(
    // The current Transmission Time Interval
    const uint32_t     tti,
    // Slice which is scheduling its users
    ran_mac_slice *    slice,
    // Map with active RAN users
    user_map_t *       umap,
    /* Boolens (1, 0) array of the resources available:
     * 1 means in use, 0 meas still available.
     */
    bool               rbg[RAN_DL_MAX_RGB],
    // Array of the assignment of resources to User Equipments
    uint16_t           ret[RAN_DL_MAX_RGB]) = 0;
};

/* Provides the commong shape for a Slice scheduler at RAN level.
 *
 * Slice scheduler is invoked as the first one and organize the spectrum for all
 * the registered slices of RAN subsystem.
 *
 * Inputs:
 *    - TTI,  which subframe is scheduled.
 *    - tmap, map of the active slices in the RAN subsystem.
 *    - rgb,  array of the usable PRBG in this subframe (the eNB can reserve 
 *            some resources for SIB or RAR messaging).
 *
 * Output:
 *    The Slice scheduler uses the given input arguments to polish the 'ret'
 *    array. This variable identifies group per group assignment of resources to
 *    User Equipments RNTI.
 */
class ran_slice_scheduler : public ran_scheduler {
public:
  virtual ~ran_slice_scheduler() 
  {
    // Do nothing
  }

  // Query the resources associated with a slice
  virtual void get_resources(
    // Id of the slice to operate on 
    uint64_t id,
    // Time resources in TTI
    int *    tti,
    // Physical resources like PRBG or PRBs, depending on the scheduler type
    int *    res) = 0;

  // Schedule Tenants following the implemented strategy
  virtual void schedule(
    // The current Transmission Time Interval
    const uint32_t tti,
    // Map with active RAN tenants
    slice_map_t *  smap,
    // Map with active RAN users
    user_map_t *   umap,
    /* Boolens (1, 0) array of the resources available:
     * 1 means in use, 0 meas still available.
     */
    bool           rbg[RAN_DL_MAX_RGB],
    // Array of the assignment of resources to User Equipments
    uint16_t       ret[RAN_DL_MAX_RGB]) = 0;

  // Assign the resources associated with a slice
  virtual int set_resources(
    // Id of the slice to operate on 
    uint64_t id,
    // Time resources in TTI
    int      tti,
    // Physical resources like PRBG or PRBs, depending on the scheduler type
    int      res) = 0;
};

/******************************************************************************
 *                                                                            *
 * Schedulers actually implemented for the RAN core:                          *
 *                                                                            *
 ******************************************************************************/

#define RAN_MULTI_DEF_TTI   0
#define RAN_MULTI_DEF_RES   0

// Slice scheduler for multiple slices; see source for more info
class ran_multi_ssched : public ran_slice_scheduler {
public:
  // Per-slice information and state
  typedef struct {
    int tti_credit; // Available TTI credit left
    int tti_org;    // Original TTI credit requested
    int tti_last;   // Last time the slice has been processed
    int res_credit; // Available resources credit left
    int res_org;    // Original resources credit requested
  } rms_slice_data;

  ran_multi_ssched();
  ~ran_multi_ssched();

  // Query the resources associated with a slice
  void get_resources(uint64_t id, int * tti, int * res);

  // Schedule the RAN Slices
  void schedule(
    const uint32_t tti,
    slice_map_t *  smap,
    user_map_t *   umap,
    bool           rbg[RAN_DL_MAX_RGB],
    uint16_t       ret[RAN_DL_MAX_RGB]);

  // Assign the resources associated with a slice
  int set_resources(uint64_t id, int tti, int res);

  // Pointer to a loc mechanism to use for feedback
  srslte::log *                      m_log;

  // Bandwidth of the cell in the DL
  int                                m_bw;
  // Slice informations relative to the scheduler
  std::map<uint64_t, rms_slice_data> m_slices;
};

// Dynamic Slice resources assignment scheduler; see source for more info
class ran_duodynamic_ssched : public ran_slice_scheduler {
public:
  ran_duodynamic_ssched();
  ~ran_duodynamic_ssched();

  // Query the resources associated with a slice
  void get_resources(uint64_t id, int * tti, int * res);

  // Schedule the RAN Slices following a given static map
  void schedule(
    const uint32_t tti,
    slice_map_t *  smap,
    user_map_t *   umap,
    bool           rbg[RAN_DL_MAX_RGB],
    uint16_t       ret[RAN_DL_MAX_RGB]);

  // Assign the resources associated with a slice
  int set_resources(uint64_t id, int tti, int res);

  uint32_t m_rbg_max;

  /* This delimits which PRB group belongs to slice B. By default slice A
   * starts from PRBG 0, and owns resources until switch, which identify where
   * slice B resources starts.
   *
   * slice B then owns all the resources until the end of the spectrum.
   *
   * Just to have a picture of its behavior in mind:
   *
   *          PRBG  0   1   2   3   4   5   6   7   8   9
   *              +---+---+---+---+---+---+---+---+---+---+
   * Sub-frame  0 | A | A | A | A | A | B | B | B | B | B |
   *              +---+---+---+---+---+---+---+---+---+---+
   *                                    ^
   *                                    |
   *                            This is the 'switch'
   *
   * Incrementing the switch allows A to take over B area, decrementing the 
   * switch allows the inverse operation.
   */
  uint32_t m_switch;

  /* Lock/unlock the dynamic shifting behavior of the switch. Setting this to
   * true will freeze the current situation of the balance between tenants.
   */
  bool     m_lock;

  // Minimum amount of PRBG that a slice is granted to have at any time.
  uint32_t m_limit;

  // Window in number of subframes
  uint32_t m_win;

  // Slice A ID
  uint64_t m_tenA;
  // Slice B ID
  uint64_t m_tenB;

  // Slot for load monitoring
  uint32_t m_win_slot;

  // Number of PRBG userd by Slice A within window monitoring slot
  uint32_t m_tenA_rbg;
  // Number of PRBG userd by Slice B within window monitoring slot
  uint32_t m_tenB_rbg;
};

class ran_rr_usched : public ran_user_scheduler {
public:
  ran_rr_usched();
  ~ran_rr_usched();

  // Schedule the Slice users in a RR fashion
  void schedule(
    const uint32_t  tti,
    ran_mac_slice * slice,
    user_map_t *    umap,
    bool            rbg[RAN_DL_MAX_RGB],
    uint16_t        ret[RAN_DL_MAX_RGB]);

private:
  // Last scheduled RNTI/User
  uint16_t m_last;
};

/******************************************************************************
 *                                                                            *
 * RAN metric interface:                                                      *
 *                                                                            *
 ******************************************************************************/

/* DL RAN scheduler for MAC.
 */
class dl_metric_ran : public sched::metric_dl
{
public:

#ifdef RAN_TRACE
  // Data useful for tracing operation of the scheduler
  rt_data  m_rtd;
#endif // RAN_TRACE

  dl_metric_ran();

  // Perform initial setup of the scheduler
  void init(srslte::log * log_handle);

  // Adds a new slice inside the MAC scheduler
  int  add_slice(uint64_t id);
  // Removes a slice from the MAC scheduler
  void rem_slice(uint64_t id);
  // Set the properties of a slice
  int set_slice(uint64_t id, mac_set_slice_args * args);
  // Adds a new slice user inside the MAC scheduler
  int  add_slice_user(uint16_t rnti, uint64_t slice, int lock);
  // Removes a slice user from the MAC scheduler
  void rem_slice_user(uint16_t rnti, uint64_t slice);
  // Returns the ID of the slice scheduler
  uint32_t get_slice_sched_id();
  // Returns information about a slice at MAC layer
  int  get_slice_info(uint64_t id,  mac_set_slice_args * args);

  /*
   * metric_dl inherited functionalities
   */

  void new_tti(
    std::map<uint16_t, sched_ue> &ue_db,
    uint32_t                      start_rbg,
    uint32_t                      nof_rbg,
    uint32_t                      nof_ctrl_symbols,
    uint32_t                      tti);

  dl_harq_proc * get_user_allocation(sched_ue * user);

private:

  // Pointer to a loc mechanism to use for feedback
  srslte::log *          m_log;

  // Current TTI index
  uint32_t               m_tti;
  /* Absolute TTI starting from when started; this is an always incresing
   * value, so consider overflows.
   */
  uint32_t               m_tti_abs;
  // The RBG to start consider allocation from (reserved by someone else)
  uint32_t               m_tti_rbg_start;
  // Mask of the available PRBG in the current TTI
  uint32_t               m_tti_rbg_mask;
  // Total amount of RB available in the current TTI
  uint32_t               m_tti_rbg_total;
  // Mask of RBG status for the current TTI: 'true' in use, 'false' not used
  bool                   m_tti_rbg[RAN_DL_MAX_RGB];
  // Available RBGs for this TTI
  uint32_t               m_tti_rbg_left;

  // Maximum number of RBG; this depends on the BW
  uint32_t               m_max_rbg;
  // size of the RBG
  uint32_t               m_rbg_size;

  // Slice scheduler currently running
  ran_slice_scheduler *  m_slice_sched;

  // Slice map
  slice_map_t            m_slice_map;

  // User map
  user_map_t             m_user_map;

  /* Array of TTI PRBG organization per user; OUTPUT of the schedulers.
   * It is filled with the organization, in RNTI terms, of the current subframe.
   */
  uint16_t               m_tti_users[RAN_DL_MAX_RGB];

  // Number of control symbols
  uint32_t               m_ctrl_sym;

  // Lock synchronizing the criticial parts.
  pthread_spinlock_t     m_lock;

  // Compute RBG mask from array of booleans
  uint32_t calc_rbg_mask(bool mask[RAN_DL_MAX_RGB]);

  // Check if is possible to allocate mask inside a base RGB setup
  bool     allocation_is_valid(uint32_t base, uint32_t mask);

  // Count how many groups are in use in the given mask
  uint32_t count_rbg(uint32_t mask);

  // Fit some RBG in a mask and return the resul
  int      new_allocation(
    uint32_t nof_rbg, bool rbg_mask[RAN_DL_MAX_RGB], uint32_t * final_mask);

}; // class dl_metric_ran
  
}

#endif /* __SCHED_METRIC_RAN_H */