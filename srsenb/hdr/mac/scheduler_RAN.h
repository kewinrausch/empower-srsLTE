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

#ifndef SCHED_METRIC_RAN_H
#define SCHED_METRIC_RAN_H

#include <stdint.h>
#include <list>
#include <pthread.h>

#include "mac/scheduler.h"

/* Decorators for arguments */
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

#define RAN_TENANT_INVALID      0
#define RAN_TENANT_STARTING     1
#define RAN_USER_INVALID        0

/* Define/undefine this symbol to trace the RAN */
#define RAN_TRACE

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

/* Meaninful statistics for a specific user */
typedef struct __ran_trace_users {
  /* There has been some activities? */
  bool active;

  /* Allocated mask setup in the DL */
  uint32_t dl_rbg_masks[RTRACE_NOF_UMASKS];
  /* How many time that mask was allocated */
  uint32_t dl_rbg_count[RTRACE_NOF_UMASKS];
  /* MCS adopted when sending data on the DL */
  int dl_rbg_mcs[RTRACE_NOF_MCS];

} rt_user;

/* Statistics of the RAN scheduler */
typedef struct __ran_trace_stats {
  /* Number of TTIs for this data to consider */
  uint32_t nof_tti;

  /* Users-related statistics */
  std::map<uint16_t, rt_user> users;
} rt_stats;

/* Master container for RAN tracing */
typedef struct __ran_trace_data {
  /* Logger used to dump stats */
  srslte::log * logger;

  /* Useful statistics */
  rt_stats stats;
} rt_data;

void ran_trace_tti(rt_data * rtd);
void ran_trace_DL_mask(rt_data * rtd, uint16_t rnti, uint32_t mask, int mcs);

#define rtrace_new_tti(r)               ran_trace_tti(r)
#define rtrace_dl_mask(r, n, m, c)      ran_trace_DL_mask(r, n, m, c)

#else

#define rtrace_new_tti(r)               /* ...into nothing */
#define rtrace_dl_mask(r, n, m, c)      /* ...into nothing */
#endif /* RAN_TRACE */

namespace srsenb {

/******************************************************************************
 *                                                                            *
 * Generic purposes strutures and classes:                                    *
 *                                                                            *
 ******************************************************************************/

class ran_user_scheduler;

/* Single user of the RAN schedulers */
class ran_user {
public:
  /* How many TTI's ago did we saw this one? */
  uint32_t last_seen;

  /* The amount of bytes exchanged in DL at MAC level */
  uint64_t DL_data;
  /* The amount of bytes exchanged in DL at MAC level during last TTI */
  uint32_t DL_data_delta;
  /* The amount of PRBG used in DL at MAC level during last TTI */
  uint32_t DL_rbg_delta;
};

/* Type which describes the map of users information stored by the RAN
 * subsystem.
 */
typedef std::map<uint32_t, ran_user> user_map_t;

/* How a Tenant is organized for the RAN scheduler logic */
class ran_tenant {
public:
  /* PLMN associated to the tenant */
  uint32_t             plmn;

  /* User scheduler associated with this tenant */
  ran_user_scheduler * sched_user;

  /* Users of this tenant */
  std::list<uint16_t>  users;
};

/* Type which describes the map of tenant information stored by the RAN 
 * subsystem.
 */
typedef std::map<uint64_t, ran_tenant> tenant_map_t;

/******************************************************************************
 *                                                                            *
 * Schedulers shapes for all the algorithms:                                  *
 *                                                                            *
 ******************************************************************************/

/* Provides the root class for all the schedulers at RAN level, of any level */
class ran_scheduler {
public:
  /* Gets a parameter value from the scheduler.
   *
   * NOTE: Value must be a pointer to an already allocated area of memory.
   *    Passing an invalid pointer can result in unknown behavior and will 
   *    likely terminate with the eNB crashing.
   *
   * Returns the length of the value string, or a negative value on error.
   */
  virtual int get_param(
    /* Name of the parameter that need to be queried */
    __ran_in_  const char *       name,
    /* Size of the name parameter */
    __ran_in_  const unsigned int nlen,
    /* Buffer where the value will be written */
    __ran_out_ char *             value,
    /* Maximum buffer size of the value arguments */
    __ran_in_  const unsigned int vlen) = 0;
  
  /* Sets a parameter value of the scheduler.
   *
   * NOTE: Value must be a pointer to an already allocated area of memory.
   *    Passing an invalid pointer can result in unknown behavior and will 
   *    likely terminate with the eNB crashing.
   *
   * Returns 0 on success, or a negative value on error.
   */
  virtual int set_param(
    /* Name of the parameter that need to be queried */
    __ran_in_  const char *       name,
    /* Size of the name parameter */
    __ran_in_  const unsigned int nlen,
    /* Buffer where the value will be written */
    __ran_out_ const char *       value,
    /* Maximum buffer size of the value arguments */
    __ran_in_  const unsigned int vlen) = 0;
};

/* Provides the common shape for an User scheduler at RAN level.
 *
 * User schedulers are invoked after the tenant one, and organize users which 
 * belong to a common tenant.
 *
 * Inputs:
 *    - TTI,    which subframe is scheduled.
 *    - tenant, Tenant which is currently scheduled.
 *    - rgb,    array of the usable PRBG in this subframe for the Tenant.
 *
 * Output:
 *    The Tenant scheduler uses the given input arguments to polish the 'ret'
 *    array. This variable identifies group per group assignment of resources to
 *    User Equipments RNTI.
 */
class ran_user_scheduler : public ran_scheduler {
public:
  /* Schedule Users following the implemented strategy */
  virtual void schedule(
    /* The current Transmission Time Interval */
    __ran_in_  const uint32_t     tti,
    /* Tenant which is scheduling its users */
    __ran_in_  ran_tenant *       tenant,
    /* Map with active RAN users */
    __ran_in_  user_map_t *       umap,
    /* Boolens (1, 0) array of the resources available:
     * 1 means in use, 0 meas still available.
     */
    __ran_in_  bool               rbg[RAN_DL_MAX_RGB],
    /* Array of the assignment of resources to User Equipments */
    __ran_out_ uint16_t           ret[RAN_DL_MAX_RGB]) = 0;
};

/* Provides the commong shape for a Tenant scheduler at RAN level.
 *
 * Tenant scheduler is invoked as the first one and organize the spectrum for
 * all the registered tenants of RAN subsystem.
 *
 * Inputs:
 *    - TTI,  which subframe is scheduled.
 *    - tmap, map of the active tenants in the RAN subsystem.
 *    - rgb,  array of the usable PRBG in this subframe (the eNB can reserve 
 *            some resources for SIB or RAR messaging).
 *
 * Output:
 *    The Tenant scheduler uses the given input arguments to polish the 'ret'
 *    array. This variable identifies group per group assignment of resources to
 *    User Equipments RNTI.
 */
class ran_tenant_scheduler : public ran_scheduler {
public:
  /* Schedule Tenants following the implemented strategy */
  virtual void schedule(
    /* The current Transmission Time Interval */
    __ran_in_  const uint32_t       tti,
    /* Map with active RAN tenants */
    __ran_in_        tenant_map_t * tmap,
    /* Map with active RAN users */
    __ran_in_        user_map_t *   umap,
    /* Boolens (1, 0) array of the resources available:
     * 1 means in use, 0 meas still available.
     */
    __ran_in_  bool                 rbg[RAN_DL_MAX_RGB],
    /* Array of the assignment of resources to User Equipments */
    __ran_out_ uint16_t             ret[RAN_DL_MAX_RGB]) = 0;
};

/******************************************************************************
 *                                                                            *
 * Schedulers actually implemented for the RAN core:                          *
 *                                                                            *
 ******************************************************************************/

/* Static Tenant resources assignment scheduler; see source for more info */
class ran_static_tsched : public ran_tenant_scheduler {
public:
  ran_static_tsched();

  /* Schedule the RAN Tenants following a given static map */
  void schedule(
    __ran_in_  const uint32_t     tti,
    __ran_in_  tenant_map_t *     tmap,
    __ran_in_  user_map_t *       umap,
    __ran_in_  bool               rbg[RAN_DL_MAX_RGB],
    __ran_out_ uint16_t           ret[RAN_DL_MAX_RGB]);

  /* Get a parameter from this scheduler */
  int get_param(
    __ran_in_  const char *       name,
    __ran_in_  const unsigned int nlen,
    __ran_out_ char *             value,
    __ran_in_  const unsigned int vlen);

  /* Set a parameter of this scheduler */
  int set_param(
    __ran_in_  const char *       name,
    __ran_in_  const unsigned int nlen,
    __ran_out_ const char *       value,
    __ran_in_  const unsigned int vlen);

private:
  /* For multi-threading access race conditions between agent and scheduler */
  pthread_mutex_t m_lock;
  /* Map of tenants recognized */
  uint64_t *      m_tenant_matrix;
  /* Window in number of subframes */
  uint32_t        m_win;
};

/* Static Tenant resources assignment scheduler; see source for more info */
class ran_duodynamic_tsched : public ran_tenant_scheduler {
public:
  ran_duodynamic_tsched();

  /* Schedule the RAN Tenants following a given static map */
  void schedule(
    __ran_in_  const uint32_t     tti,
    __ran_in_  tenant_map_t *     tmap,
    __ran_in_  user_map_t *       umap,
    __ran_in_  bool               rbg[RAN_DL_MAX_RGB],
    __ran_out_ uint16_t           ret[RAN_DL_MAX_RGB]);

  /* Get a parameter from this scheduler */
  int get_param(
    __ran_in_  const char *       name,
    __ran_in_  const unsigned int nlen,
    __ran_out_ char *             value,
    __ran_in_  const unsigned int vlen);

  /* Set a parameter of this scheduler */
  int set_param(
    __ran_in_  const char *       name,
    __ran_in_  const unsigned int nlen,
    __ran_out_ const char *       value,
    __ran_in_  const unsigned int vlen);

private:
  uint32_t m_rbg_max;

  /* This delimits which PRB group belongs to tenant B. By default tenant A
   * starts from PRBG 0, and owns resources until switch, which identify where
   * tenant B resources starts.
   *
   * Tenant B then owns all the resources until the end of the spectrum.
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

  /* Minimum amount of PRBG that a Tenant is granted to have at any time.
   */
  uint32_t m_limit;

  /* Window in number of subframes */
  uint32_t m_win;

  /* Tenant A ID */
  uint64_t m_tenA;
  /* Tenant B ID */
  uint64_t m_tenB;

  /* Slot for load monitoring */
  uint32_t m_win_slot;

  /* Number of PRBG userd by tenant A within window monitoring slot */
  uint32_t m_tenA_rbg;
  /* Number of PRBG userd by tenant B within window monitoring slot */
  uint32_t m_tenB_rbg;
};

class ran_rr_usched : public ran_user_scheduler {
public:
  ran_rr_usched();

  /* Schedule the Tenant users in a RR fashion */
  void schedule(
    __ran_in_  const uint32_t     tti,
    __ran_in_  ran_tenant *       tenant,
    __ran_in_  user_map_t *       umap,
    __ran_in_  bool               rbg[RAN_DL_MAX_RGB],
    __ran_out_ uint16_t           ret[RAN_DL_MAX_RGB]);

  /* Get a parameter from this scheduler */
  int get_param(
    __ran_in_  const char *       name,
    __ran_in_  const unsigned int nlen,
    __ran_out_ char *             value,
    __ran_in_  const unsigned int vlen);

  /* Set a parameter of this scheduler */
  int set_param(
    __ran_in_  const char *       name,
    __ran_in_  const unsigned int nlen,
    __ran_out_ const char *       value,
    __ran_in_  const unsigned int vlen);

private:
  /* Last scheduled RNTI/User */
  uint16_t m_last;
};

/* DL RAN scheduler for MAC.
 */
class dl_metric_ran : public sched::metric_dl
{
public:

#ifdef RAN_TRACE
  /* Data useful for tracing operation of the scheduler */
  rt_data  m_rtd;
#endif /* RAN_TRACE */

  dl_metric_ran();

  /* Perform initial setup of the scheduler */
  void init(srslte::log * log_handle);

  void new_tti(
    std::map<uint16_t, sched_ue> &ue_db,
    uint32_t                      start_rbg,
    uint32_t                      nof_rbg,
    uint32_t                      nof_ctrl_symbols,
    uint32_t                      tti);

  dl_harq_proc * get_user_allocation(sched_ue * user);

private:

  /* Pointer to a loc mechanism to use for feedback */
  srslte::log *          m_log;

  /* Current TTI index */
  uint32_t               m_tti;
  /* Absolute TTI starting from when started; this is an always incresing
   * value, so consider overflows.
   */
  uint32_t               m_tti_abs;
  /* The RBG to start consider allocation from (reserved by someone else) */
  uint32_t               m_tti_rbg_start;
  /* Mask of the available PRBG in the current TTI */
  uint32_t               m_tti_rbg_mask;
  /* Total amount of RB available in the current TTI */
  uint32_t               m_tti_rbg_total;
  /* Mask of RBG status for the current TTI: 'true' in use, 'false' not used */
  bool                   m_tti_rbg[RAN_DL_MAX_RGB];
  /* Available RBGs for this TTI */
  uint32_t               m_tti_rbg_left;

  /* Maximum number of RBG; this depends on the BW */
  uint32_t               m_max_rbg;
  /* size of the RBG */
  uint32_t               m_rbg_size;

  /* Tenant scheduler currently running */
  ran_tenant_scheduler * m_tenant_sched;

  /* Tenant map */
  tenant_map_t           m_tenant_map;
  /* Next tenant id */
  uint32_t               m_tenant_id;

  /* User map */
  user_map_t             m_user_map;

  /* Array of TTI PRBG organization per user; OUTPUT of the schedulers.
   * It is filled with the organization, in RNTI terms, of the current subframe.
   */
  uint16_t               m_tti_users[RAN_DL_MAX_RGB];

  /* Number of control symbols */
  uint32_t               m_ctrl_sym;

  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   * EXTREMELY IMPORTANT:
   *
   * For some reason if the class is not big enough boost fail launching an 
   * exception over an unintialized mutex, so keep this until the problem is
   * solved.
   *
   * There is some serious issue with memory here, but this class does not seem
   * to be the culprit, since this variable does NOTHING!
   * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   */
  uint32_t               m_pad[RAN_DL_MAX_RGB];

  /* Add a new tenant to the managed ones */
  uint32_t add_tenant(uint32_t plmnid);

  /* Removes a tenant from the scheduler */
  int      rem_tenant(uint64_t plmnid);
  
  /* Associate an user to a specific tenant */
  int      add_user(uint16_t rnti, uint64_t tenant);
  
  /* Removes an user from a specific tenant */
  int      rem_user(uint16_t rnti, uint64_t tenant);

  /* Compute RBG mask from array of booleans */
  uint32_t calc_rbg_mask(bool mask[RAN_DL_MAX_RGB]);

  /* Check if is possible to allocate mask inside a base RGB setup */
  bool     allocation_is_valid(uint32_t base, uint32_t mask);

  /* Count how many groups are in use in the given mask */
  uint32_t count_rbg(uint32_t mask);

  /* Fit some RBG in a mask and return the result */
  int      new_allocation(
    uint32_t nof_rbg, bool rbg_mask[RAN_DL_MAX_RGB], uint32_t * final_mask);

}; /* class dl_metric_ran */

/* UL RAN scheduler for MAC.
 *
 * This is actually just a copy of the default Round-Robin one.
 */
class ul_metric_ran : public sched::metric_ul
{
public:
  /* Prepare the scheduler for a new TTI */
  void           new_tti(
    std::map<uint16_t,sched_ue> &ue_db, uint32_t nof_rb, uint32_t tti);
  
  /* Get user allocation for the current TTI */
  ul_harq_proc * get_user_allocation(sched_ue * user); 

  /* Update the UL allocation */
  void           update_allocation(ul_harq_proc::ul_alloc_t alloc);
private:
  
  const static int MAX_PRB = 100; 
  
  bool new_allocation(uint32_t L, ul_harq_proc::ul_alloc_t *alloc);
  bool allocation_is_valid(ul_harq_proc::ul_alloc_t alloc); 

  uint32_t nof_users_with_data; 

  bool used_rb[MAX_PRB]; 
  uint32_t current_tti; 
  uint32_t nof_rb; 
  uint32_t available_rb;
};

  
}

#endif

