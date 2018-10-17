/**
 *
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

#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "srslte/common/log.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/phch/cqi.h"
#include "srslte/phy/utils/vector.h"

#include "srsenb/hdr/mac/scheduler_harq.h"
#include "srsenb/hdr/mac/scheduler_RAN.h"

#define Error(fmt, ...)                             \
  do {                                              \
    m_log->error("SCHED_RAN: "fmt, ##__VA_ARGS__);  \
  } while(0)

#define Warning(fmt, ...)                           \
  do {                                              \
    m_log->warning("SCHED_RAN: "fmt, ##__VA_ARGS__);\
  } while(0)

#define Info(fmt, ...)                              \
  do {                                              \
    m_log->info("SCHED_RAN: "fmt, ##__VA_ARGS__);   \
  } while(0)

#define Debug(fmt, ...)                             \
  do {                                              \
    m_log->debug("SCHED_RAN: "fmt, ##__VA_ARGS__);  \
  } while(0)

/******************************************************************************
 *                                                                            *
 *                          TRACING PART FOR RAN                              *
 *                                                                            *
 ******************************************************************************/

//#define RAN_STATIC

/* RAN tracing capabilities:
 *
 * This set of procedure and data structures are here with the only purpose to
 * provides non-invasive (for MAC scheduler) statistics over what is happening
 * in the RAN scheduler.
 *
 * Undefine RAN_TRACE to disable.
 */
#ifdef RAN_TRACE

// Trace and log data, eventually 
void ran_trace_tti(rt_data * rtd)
{
  int  i;
  int  j;
  bool r[32];

  srslte::log * m_log = rtd->logger;

  std::map<uint16_t, rt_user>::iterator ui;

  rtd->stats.nof_tti++;

  // Dump the stats! 
  if (rtd->stats.nof_tti >= RTRACE_INTERVAL) {
    Warning("*** Dumping statistics ***************************************\n");
    Warning("N.of elapsed TTIs: %d\n", rtd->stats.nof_tti);

    for (ui = rtd->stats.users.begin(); ui != rtd->stats.users.end(); ++ui) {
      if(!ui->second.active) {
        continue;
      }

      Warning("RAN user %x\n", ui->first);

      Warning("    DL_MCS --> "
        "%d %d %d %d %d %d %d %d %d | "          // QPSK 
        "%d %d %d %d %d %d %d | "                // 16-QAM 
        "%d %d %d %d %d %d %d %d %d %d %d %d | " // 64-QAM 
        "%d %d %d\n",                            // Reserved 
        ui->second.dl_rbg_mcs[0],
        ui->second.dl_rbg_mcs[1],
        ui->second.dl_rbg_mcs[2],
        ui->second.dl_rbg_mcs[3],
        ui->second.dl_rbg_mcs[4],
        ui->second.dl_rbg_mcs[5],
        ui->second.dl_rbg_mcs[6],
        ui->second.dl_rbg_mcs[7],
        ui->second.dl_rbg_mcs[8],
        ui->second.dl_rbg_mcs[9],
        ui->second.dl_rbg_mcs[10],
        ui->second.dl_rbg_mcs[11],
        ui->second.dl_rbg_mcs[12],
        ui->second.dl_rbg_mcs[13],
        ui->second.dl_rbg_mcs[14],
        ui->second.dl_rbg_mcs[15],
        ui->second.dl_rbg_mcs[16],
        ui->second.dl_rbg_mcs[17],
        ui->second.dl_rbg_mcs[18],
        ui->second.dl_rbg_mcs[19],
        ui->second.dl_rbg_mcs[20],
        ui->second.dl_rbg_mcs[21],
        ui->second.dl_rbg_mcs[22],
        ui->second.dl_rbg_mcs[23],
        ui->second.dl_rbg_mcs[24],
        ui->second.dl_rbg_mcs[25],
        ui->second.dl_rbg_mcs[26],
        ui->second.dl_rbg_mcs[27],
        ui->second.dl_rbg_mcs[28],
        ui->second.dl_rbg_mcs[29],
        ui->second.dl_rbg_mcs[30]);

      // Reset MCS statistics 
      memset(ui->second.dl_rbg_mcs, 0, sizeof(int) * RTRACE_NOF_MCS);

      for (i = 0; i < RTRACE_NOF_UMASKS; i++) {
        if (ui->second.dl_rbg_count[i] == 0) {
          continue;
        }

        for (j = 0; j < 32; j++) {
          r[j] = (ui->second.dl_rbg_masks[i] & 1) == 1 ? true : false;
          ui->second.dl_rbg_masks[i] >>= 1;
        }

        Warning("    Mask count %05d --> "
          "%d %d %d %d %d | %d %d %d %d %d | "
          "%d %d %d %d %d | %d %d %d %d %d | "
          "%d %d %d %d %d | %d %d %d %d %d | "
          "%d %d\n",
          ui->second.dl_rbg_count[i],
          r[0],  r[1],  r[2],  r[3],  r[4],  r[5],  r[6],  r[7],
          r[8],  r[9],  r[10], r[11], r[12], r[13], r[14], r[15],
          r[16], r[17], r[18], r[19], r[20], r[21], r[22], r[23],
          r[24], r[25], r[26], r[27], r[28], r[29], r[30], r[31]);

        ui->second.dl_rbg_masks[i] = 0;
        ui->second.dl_rbg_count[i] = 0;
      }

      ui->second.active = false;
    }

    rtd->stats.nof_tti = 0;
  }
}

// Trace single user allocation to show it later
void ran_trace_DL_mask(rt_data * rtd, uint16_t rnti, uint32_t mask, int mcs)
{
  int  i;
  int  j  = -1;

  rtd->stats.users[rnti].active = true;
  rtd->stats.users[rnti].dl_rbg_mcs[mcs]++;

  for (i = 0; i < 32; i++) {
      if (j < 0 && rtd->stats.users[rnti].dl_rbg_count[i] == 0) {
        j = i;
      }

      // Found this mask... again...
      if (mask == rtd->stats.users[rnti].dl_rbg_masks[i]) {
        rtd->stats.users[rnti].dl_rbg_count[i] += 1;
        j = -1;

        break;
      }
  }

  if (j >= 0) {
    rtd->stats.users[rnti].dl_rbg_count[j] = 1;
    rtd->stats.users[rnti].dl_rbg_masks[j] = mask;
  }
}

#endif // RAN_TRACE

namespace srsenb {

/******************************************************************************
 *                                                                            *
 *                         User schedulers for RAN                            *
 *                                                                            *
 ******************************************************************************/

/*
 *
 * ROUND-ROBIN RESOURCE ALLOCATION FOR SLICE USERS
 *
 */

/* Routine:
 *    ran_rr_usched::ran_rr_usched
 * 
 * Abstract:
 *    Initializes the RAN slice User-level scheduler with Round-Robin policy
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
ran_rr_usched::ran_rr_usched()
{
  m_id   = RAN_MAC_USER_RR;
  m_last = 0; // Last RNTI scheduled
}

/* Routine:
 *    ran_rr_usched::~ran_rr_usched
 * 
 * Abstract:
 *    Releases resources associated with this scheduler
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
ran_rr_usched::~ran_rr_usched()
{
  // Nothing
}

/* Routine:
 *    ran_rr_usched::schedule
 *
 * Abstract:
 *    Maintains the RNTI of the last scheduled user, and loop through users 
 *    associated with the slice to select the next one. At each selected user
 *    is given the use of the whole spectrum for that subframe.
 *
 * Assumptions:
 *    It assumes that the slice has at least one PRBG assigned to itself during
 *    the given TTI.
 * 
 * Arguments:
 *    - tti, The TTI where we are operating on
 *    - slice, Slice to consider
 *    - umap, map of the currently connected users to consider
 *    - rbg, array of groups that is possible to allocate
 *    - ret, array matching rbg with the assigned user
 *
 * Returns:
 *    ---
 */
void ran_rr_usched::schedule(
  const uint32_t     tti,
  ran_mac_slice *    slice,
  user_map_t *       umap,
  bool               rbg[RAN_DL_MAX_RGB],
  uint16_t           ret[RAN_DL_MAX_RGB]) 
{
  uint16_t first = 0;
  uint16_t rnti  = 0;

  std::map<uint16_t, int>::iterator i;
  int j;

   // Select the next candidate RNTI
  for(i = slice->users.begin(); i != slice->users.end(); ++i) {
    // Save the first valid RNTI in case we reach the end of the list
    if (!first) {
      first = i->first;
    }

    // Skip UE which has nothing to tx/re-tx
    //if((*umap)[i->first].has_data == 0) {
    //  continue;
    //}

    // Last RNTI not selected yet?
    if(!m_last) {
      rnti = i->first;
      break;
    }

    // Ok, this was the last selected
    if(i->first == m_last) {
      i++;

      // If we are at the end of the list, select the first again
      if(i == slice->users.end()) {
        rnti = first;
        break;
      } 
      // Otherwise 
      else {
        rnti = i->first;
        break;
      }
    }
  }

  // No users present in this slice
  if(first == 0) {
    return;
  }

  // In the case we reached the end but no RNTI has been selected yet
  if (!rnti) {
    rnti = first;
  }

  m_last = rnti;

  // Assign the groups of this slice to the designed RNTI
  for(j = 0; j < RAN_DL_MAX_RGB; j++) {
    // This group is NOT in use, so free for me :)
    if (!rbg[j]) {
      ret[j] = rnti;
    }
  }

  return;
}

/******************************************************************************
 *                                                                            *
 *                         Slice schedulers for RAN                           *
 *                                                                            *
 ******************************************************************************/

/*
 *
 * "MULTI-SLICEs" SLICE SCHEDULER
 *
 */

/* Routine:
 *    ran_multi_ssched::ran_multi_ssched()
 *
 * Abstract:
 *    Initializes the resources needed for slice scheduler
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
ran_multi_ssched::ran_multi_ssched()
{
  m_bw = 0;
}

/* Routine:
 *    ran_multi_ssched::~ran_multi_ssched()
 *
 * Abstract:
 *    Initializes the resources needed for slice scheduler
 * 
 *    WARNING:
 *    The algorithm choosen thus has the bad property to accumulate lot of PRBs
 *    at the end of the time frame (due to division with integers, which does
 *    not count decimals). Needs to be tested!!
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
ran_multi_ssched::~ran_multi_ssched()
{

}

/* Routine:
 *    ran_multi_ssched::get_resources()
 *
 * Abstract:
 *    Gets information about allocations of a specific slice.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to query
 *    - tti, pointer to time resources
 *    - res, pointer to space resources
 *
 * Returns:
 *    ---
 */
void ran_multi_ssched::get_resources(uint64_t id, int * tti, int * res)
{
  /* Retrieve original time and space requested allocations */
  if(m_slices.count(id) > 0) {
    if(tti) {
      *tti = m_slices[id].tti_org;
    }
    if(res) {
      *res = m_slices[id].res_org;
    }
  }
}

/* Routine:
 *    ran_multi_ssched::schedule()
 *
 * Abstract:
 *    Schedule the resurces for this TTI downlink. The scheduling is done over
 *    the sclices which have been assigned some resources in time and space.
 * 
 *    Allocation is performed by assigning, at each TTI, a portion of the total
 *    requested resources. This portion depends on both the total amount of
 *    resources and the given time.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - tti, pointer to time resources
 *    - smap, maps of the slices
 *    - umap, map of the users
 *    - rbg, boolean array identifying which resource is already in use
 *    - ret, rnti array which provides per-UE allocation of resources
 *
 * Returns:
 *    'ret' arguments is filled with the correct allocation for this subframe.
 *    This variable is used then during real harq allocation.
 */
void ran_multi_ssched::schedule(
  const uint32_t tti,
  slice_map_t *  smap,
  user_map_t *   umap,
  bool           rbg[RAN_DL_MAX_RGB],
  uint16_t       ret[RAN_DL_MAX_RGB])
{
  int       i;      // Index
  
  uint64_t  sid;    // Slice id
  int       res;    // Number of resources that shall be granted
  int       tot;    // Total resources consumed

  // Allocation for the single slice
  bool      user[RAN_DL_MAX_RGB];

  slice_map_t::iterator sit; // Slices iterator

  // Operate on all the slices
  for(sit = smap->begin(); sit != smap->end(); ++sit) {
    sid = sit->first;
    tot = 0;
    res = 0;

    // Slice not yet set, so no time/space is dedicated for it
    if(m_slices.count(sid) == 0) {
      continue; // Next slice
    }

    // Time given expired
    if(m_slices[sid].tti_credit == 0) {
      // but resources are not!!
      if(m_slices[sid].res_credit > 0) {
        /* NOTE: 
         * Renew the time credit for the moment. This error should trigger
         * something more extreme as resolving policy.
         * 
         * For balancing purposes can we subtracts remaining credit to original
         * fields?
         */
        m_slices[sid].tti_credit = m_slices[sid].tti_org;
        continue; // Next slice
      }

      // Negative time is consumed and never renewed
      if(m_slices[sid].tti_org < 0) {
        continue; // Next slice
      }

      // If even resources were expired, renew them
      m_slices[sid].tti_credit = m_slices[sid].tti_org;
      m_slices[sid].res_credit = m_slices[sid].res_org;
//Warning("Renew for slice %" PRIu64 " resources, res=%d, time=%d\n", sid, m_slices[sid].res_credit, m_slices[sid].tti_credit);
    }

    // Are there resources to be consumed?
    if(m_slices[sid].res_credit > 0) {
      // Time credit is more than 0?
      if(m_slices[sid].tti_credit > 0) {
        // How many resources we have to expend for this frame
        res = m_slices[sid].res_credit / m_slices[sid].tti_credit;
      } 
      /* Zero case is handled in previous if; if this is triggered it means that
       * TTI is negative, which I interprets as 'once'.
       */
      else {
        res = m_slices[sid].res_credit / (m_slices[sid].tti_credit * -1);
//Warning("%d one-time res left for slice %" PRIu64 "\n", res, sid);
      }
    } 
    // No more resources for this slice
    else {
      continue; // Next slice
    }

    // Set the groups which can are free for the user scheduler to allocate
    for(i = 0; i < RAN_DL_MAX_RGB; i++) {
      // In use?
      if(rbg[i]) {
        user[i] = true; // In use
      }
      // Not in use?
      else {
        if(res > 0) {
          user[i] = false; // Not in use, free for allocation...
          rbg[i]  = true;  // ...aaand now this RBG is in use
          res--;           // Less resources to consume
          tot++;           // More consumed
        } else {
          user[i] = true;  // No more credit, don't use
        }
      }
    }

    // Scheduler users for this slice, filling 'ret'
    if (sit->second.sched_user) {
      sit->second.sched_user->schedule(tti, &sit->second, umap, user, ret);
    }

    // Consume the resource credit; 'res' resources allocated
    m_slices[sid].res_credit -= tot;

    // Positive time?
    if(m_slices[sid].tti_credit > 0) {
      m_slices[sid].tti_credit--;
    } 
    // Negative time?
    else {
      m_slices[sid].tti_credit++;
    }
  }

  return; // Allocations done
}

/* Routine:
 *    ran_multi_ssched::set_resources()
 *
 * Abstract:
 *    Set the resources for a particular slice. If resources on both time and
 *    space are set to -1, the slice is removed from the scheduler database.
 *
 * Assumptions:
 *    Synchronization over resources is performed outside this context. I expect
 *    the DL scheduler to synchronize against 'schedule' and set/get op.
 * 
 * Arguments:
 *    - id, ID of the slice
 *    - tti, Transmission Time Interval resources
 *    - res, physical resources
 *
 * Returns:
 *    Zero on success, otherwise a negative error code
 */
int ran_multi_ssched::set_resources(uint64_t id, int tti, int res)
{
  if(id == RAN_SLICE_INVALID) {
    return -1;
  }

  // Remove element from the scheduler, since no resources are associated to it
  if(tti == -1 && res == -1) {
    m_slices.erase(id);
    return 0;
  }

  /*
   * TODO: Resource check for over-commitment is performed here!
   */

  // This renews the credits, since we have to compute with updates value
  m_slices[id].tti_org    = tti;
  m_slices[id].tti_credit = tti;
  m_slices[id].res_org    = res;
  m_slices[id].res_credit = res;

//printf("Slice %" PRIu64 " resources set to space=%d, time=%d\n", id, res, tti);

  return 0;
}

/*
 *
 * "DUO-DYNAMIC" SLICE-ASSIGNMENT SCHEDULER
 *
 */

/* Routine:
 *    ran_duodynamic_ssched::ran_duodynamic_ssched()
 *
 * Abstract:
 *    Initializes the resources needed for slice scheduler for 2 slice
 *    instances.
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
ran_duodynamic_ssched::ran_duodynamic_ssched()
{
  m_id       = RAN_MAC_SLICE_DUO;

  // Slice A area starts (including) from PRBG 0
  // Slice B area starts (including) from PRBG 7
  m_switch   = 7;
  //m_switch   = 10; // 5Mbps in the big one
  // Is the switch locked or free to dynamically adapt?
  m_lock     = 1;
  // Granted amount of PRBG per Slice is 3
  m_limit    = 3;
  // Window to consider is one frame (10 subframes)
  m_win      = 10;
  // Slice A ID
  m_tenA     = RAN_SLICE_STARTING; // <----------------------------------------- NOTE: Hardcoded for testing purposes
  // Slice B ID
  m_tenB     = 0;
  // Slot of TTIs used for scheduler monitoring
  m_win_slot = 0;
  // Amount of PRBG used by slice A
  m_tenA_rbg = 0;
  // Amount of PRBG used by slice B
  m_tenB_rbg = 0;
  // Number of PRBG per TTI
  m_rbg_max  = 13; // <--------------------------------------------------------- NOTE: Hardcoded for testing purposes
}

/* Routine:
 *    ran_duodynamic_ssched::~ran_duodynamic_ssched
 * 
 * Abstract:
 *    Releases resources associated with this scheduler
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
ran_duodynamic_ssched::~ran_duodynamic_ssched() 
{
  // Nothing
}

/* Routine:
 *    ran_duodynamic_ssched::get_resources
 * 
 * Abstract:
 *    Get the resources allocations associated with a certain slice
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, slice ID to consider
 *    - tti, pointer to the variable populated with time resource
 *    - res, pointer to the variable populated with space resource 
 * 
 * Returns:
 *    ---
 */
void ran_duodynamic_ssched::get_resources(uint64_t id, int * tti, int * res)
{
  if(res) {
    if(id == m_tenA) {
      *res = (int)m_switch;
    } else if(id == m_tenB) {
      *res = (int)(m_rbg_max - m_switch);
    }
  }
}

/* Routine:
 *    ran_duodynamic_ssched::set_resources
 * 
 * Abstract:
 *    Set the resources allocations associated with a certain slice
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, slice ID to consider
 *    - tti, time resource to associate with the slice
 *    - res, space resource to associate with the slice
 * 
 * Returns:
 *    Zero on success, otherwise a negative error code
 */
int ran_duodynamic_ssched::set_resources(uint64_t id, int tti, int res)
{
  uint32_t rbg = (uint32_t)res;

  if(res < 0 || tti < 0) {
    return -1;
  }

  /* Case Slice A, allocation from 0 to switch:
   *    The allocation requested is the switch itself. This means that checks
   *    can be done directly using the given value 'res'.
   * 
   *    e.g: If A want 10 RBG, switch should be moved to 10.
   */
  if(m_tenA == id) {
    if(rbg > m_rbg_max - m_limit) {
      m_switch = m_rbg_max - m_limit;
    } else if(rbg < m_limit) {
      m_switch = m_limit;
    } else {
      m_switch = rbg;
    }
  } 
  /* Case Slice B, allocation from switch to the max:
    *    Allocation here happens from switch to max. This means that the target
    *    switch value is 'max - res'
    * 
    *    e.g: If B want 10 RBG, switch should be moved to 3. 
    *         This means 3 = 13(max) - 10(requested)
    */
  else if(m_tenB == id) {
    rbg = m_rbg_max - res;
    
    if(rbg > m_rbg_max - m_limit) {
      m_switch = m_rbg_max - m_limit;
    } else if(rbg < m_limit) {
      m_switch = m_limit;
    } else {
      m_switch = rbg;
    }
  }

  return 0;
}

/* Routine:
 *    ran_duodynamic_ssched::schedule
 *
 * Abstract:
 *    The scheduler keeps a 'barrier' switch between the two Tenants, which 
 *    identifies where the resources of the first terminates and start the ones
 *    of the second.
 *
 * Assumptions:
 *    Does not enforce any security check after all the User schedulers have
 *    been ran. This means that miss-behaving user schedulers can still mess
 *    with the spectrum if poorly implemented.
 *
 * Arguments:
 *    - tti, current TTI where when allocation happens
 *    - smap, map of the slices registered
 *    - umap, map of the users registered
 *    - rbg, array of the possible RBG slots that are in use or free
 *    - ret, final outcome of slots assigned to single RNTIs
 * 
 * Output:
 *    ---
 */
void ran_duodynamic_ssched::schedule(
  const uint32_t tti,
  slice_map_t *  smap,
  user_map_t *   umap,
  bool           rbg[RAN_DL_MAX_RGB],
  uint16_t       ret[RAN_DL_MAX_RGB])
{
  unsigned int          i;

  //uint64_t     ten;
  //uint64_t     ten_load = 0;
  //uint32_t     tti_idx  = tti % m_win;
  bool                  trbg_A[RAN_DL_MAX_RGB];
  bool                  trbg_B[RAN_DL_MAX_RGB];

  slice_map_t::iterator s; // Slice iterator
  user_map_t::iterator  u; // User map iterator
  std::map<uint16_t, int>::iterator l; // User list iterator

  uint32_t              tot_A;     // Total RBG for A
  int                   load_A = 0;// Is A loaded with data?
  uint32_t              tot_B;     // Total RBG for B
  int                   load_B = 0;// Is B loaded with data?

  // Skip groups already in use and prepare the map for Slice A
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    if (rbg[i]) {
      trbg_A[i] = 1;    // Invalid group
      trbg_B[i] = 1;    // Invalid group
    } else {
      // Slice A lies in the lower part of the spectrum
      if (i < m_switch) {
        trbg_A[i] = 0;  // Valid group  
        trbg_B[i] = 1;  // Invalid group
      } else {
        trbg_A[i] = 1;  // Invalid group
        trbg_B[i] = 0;  // Valid group  
      }
    }
  }

  // Perform scheduling slice per slice
  for (s = smap->begin(); s != smap->end(); ++s) {
    // Monitor the usage of Slice A or B
    if(s->first == m_tenA) {
      for(l = s->second.users.begin(); l != s->second.users.end(); ++l) {
       if(umap->count(l->first) > 0) {
          m_tenA_rbg += umap->find(l->first)->second.DL_rbg_delta;
        }
      }
    } else if(s->first == m_tenB) {
      for(l = s->second.users.begin(); l != s->second.users.end(); ++l) {
        if(umap->count(l->first) > 0) {
          m_tenB_rbg += umap->find(l->first)->second.DL_rbg_delta;
        }
      }
    }

    if (s->second.sched_user) {
      s->second.sched_user->schedule(
        tti,
        &s->second,
        umap,
        s->first == m_tenA ? trbg_A : trbg_B,
        ret);
    }
  }

  // If switching behavior is locked, bypass this logic
  if(m_lock) {
    return;
  }

  /*
   *
   * Decide, based on the loads, if to move the switch
   *
   */

  m_win_slot++;

  // 1 seconds routine; decide what to do now...
  if(m_win_slot == 1000) {
    tot_A = (m_switch) * 1000;
    tot_B = (m_rbg_max - m_switch) * 1000;

    /*
     * In which state are we?
     */

    // Is A loaded with data?
    if(m_tenA_rbg >= (tot_A / 10) * 8) {
      load_A = 1;
    }

    // Is B loaded with data?
    if(m_tenB_rbg >= (tot_B / 10) * 8) {
      load_B = 1;
    }

    /*
     * Take decision of what to do now...
     */

    // A and B are not loaded
    if(!load_A && !load_B) {
      // Reset to 50/50 situation
      m_switch = 7;
      // What to do? Stay still?
      goto cont;
    }

    // A is loaded and B not
    if(load_A && !load_B) {
      if(m_switch < m_rbg_max - m_limit) {
        m_switch++;
      }

      goto cont;
    }

    // B is loaded and A not
    if(load_B && !load_A) {
      if(m_switch > m_limit) {
        m_switch--;
      }

      goto cont;
    }

    // Both are loaded
    if(load_A && load_B) {
      // Reset to 50/50 situation
      m_switch = 7;
    }

cont:

    // Reset
    m_win_slot  = 0;
    m_tenA_rbg  = 0;
    m_tenB_rbg  = 0;
  }

  return;
}

/******************************************************************************
 *                                                                            *
 *                        DL part of RAN scheduler                            *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    dl_metric_ran::dl_metric_ran()
 *
 * Abstract:
 *    Initializes the static resources needed for the DL metrics.
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
dl_metric_ran::dl_metric_ran()
{
  int i;

  m_tti           = 0;
  m_tti_abs       = 0;
  m_tti_rbg_mask  = 0;
  m_tti_rbg_total = 0;
  m_tti_rbg_left  = 0;
  m_tti_rbg_start = 0;
  m_ctrl_sym      = 0;
  m_log           = 0;
  m_slice_sched   = 0;
  m_max_rbg       = 0;
  m_rbg_size      = 0;
  
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    m_tti_rbg[i]  = false;
    m_tti_users[i]= 0;
  }
}

/* Routine:
 *    dl_metric_ran::init()
 *
 * Abstract:
 *    Creates the resources necessary for logging and synchronizing all the 
 *    elements of the Downlink scheduler.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - log_handle, Handle to the logging system bound to the metric
 *
 * Returns:
 *    ---
 */
void dl_metric_ran::init(srslte::log * log_handle)
{
  m_log        = log_handle;
#ifdef RAN_TRACE
  m_rtd.logger = log_handle;
#endif // RAN_TRACE

  pthread_spin_init(&m_lock, 0);

  //m_slice_sched = new ran_duodynamic_ssched();
  m_slice_sched = new ran_multi_ssched();
  ((ran_multi_ssched *)(m_slice_sched))->m_log = log_handle; // <-------------------------------------------------------
}

/* Routine:
 *    dl_metric_ran::add_slice()
 *
 * Abstract:
 *    Adds a new slice in a compatible way into the MAC slicing subsystem
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to add
 *
 * Returns:
 *    Zero on success, otherwise a negative error number
 */
int  dl_metric_ran::add_slice(uint64_t id)
{
  pthread_spin_lock(&m_lock);

  if(m_slice_map.count(id) != 0) {
    Error("Slice %" PRIu64 " already existing in the MAC scheduler\n", id);
    
    pthread_spin_unlock(&m_lock);  
    return -1;
  }

  // Creates the slice and assign a default RR user scheduler to it
  m_slice_map[id].sched_user = new ran_rr_usched();

//------------------------------------------------------------------------------ TEMPORARY!
//  if(((ran_duodynamic_ssched *)m_slice_sched)->m_tenA != id) {
//    ((ran_duodynamic_ssched *)m_slice_sched)->m_tenB = id;
//  }
//------------------------------------------------------------------------------ TEMPORARY!

  pthread_spin_unlock(&m_lock);

  Info("Slice %" PRIu64 " added to RAN MAC scheduler\n", id);

  return 0;
}

/* Routine:
 *    dl_metric_ran::rem_slice()
 *
 * Abstract:
 *    Removes a slice from the MAC slicing subsystem
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to remove
 *
 * Returns:
 *    ---
 */
void dl_metric_ran::rem_slice(uint64_t id)
{
  ran_user_scheduler *  us;
  slice_map_t::iterator it;

  if(id == RAN_SLICE_STARTING) {
    Error("Cannot remove the default slice\n");
    return;
  }

  pthread_spin_lock(&m_lock);

  it = m_slice_map.find(id);

  if(it == m_slice_map.end()) {
    Error("Slice %" PRIu64 "not found in the MAC scheduler\n", id);

    pthread_spin_unlock(&m_lock);
    return;
  }

//------------------------------------------------------------------------------ TEMPORARY!
//  if(((ran_duodynamic_ssched *)m_slice_sched)->m_tenB == id) {
//    ((ran_duodynamic_ssched *)m_slice_sched)->m_tenB = 0L;
//  }
//------------------------------------------------------------------------------ TEMPORARY!

  us = it->second.sched_user;
  m_slice_map.erase(it);

  pthread_spin_unlock(&m_lock);

  // Delete the class instance 
  delete us;

  Info("Slice %" PRIu64 " removed from RAN MAC scheduler\n", id);

  return;
}

/* Routine:
 *    dl_metric_ran::set_slice()
 *
 * Abstract:
 *    Configures a slice to behave according to a new configuration provided
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to configure
 *    - args, arguments with new configuration to apply
 *
 * Returns:
 *    Zero on success, otherwise a negative error code
 */
int dl_metric_ran::set_slice(uint64_t id, mac_set_slice_args * args)
{
  // Feed the argument to the scheduler 
  return m_slice_sched->set_resources(id, (int)args->time, (int)args->rbg);
}

/* Routine:
 *    dl_metric_ran::add_slice_user()
 *
 * Abstract:
 *    Associate an user with a slice. The user can be associated with a locked
 *    or unlocked state; unlocked state allows the scheduler to apply some
 *    custom optimization on it.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, ID of the user to associate
 *    - slice, ID of the slice
 *    - lock, rnti strictly follows given slice configuration?
 *
 * Returns:
 *    Zero on success, otherwise a negative error code
 */
int  dl_metric_ran::add_slice_user(uint16_t rnti, uint64_t slice, int lock)
{
  std::list<uint16_t>::iterator it;

  pthread_spin_lock(&m_lock);

  if(m_slice_map.count(slice) == 0) {
    Error("Slice %" PRIu64 " does not exists in the MAC scheduler\n", slice);
    
    pthread_spin_unlock(&m_lock);  
    return -1;
  }

  // The user has been associated by the agent, so do not handle by yourself
  m_user_map[rnti].self_m        = !lock;
  m_slice_map[slice].users[rnti] = 1;

  /* If the element is inserted in the 'default' tenant, also some new resources
   * should be given to the slice, since they are usually consumed for initial
   * connection with EPC.
   */
  //if(slice == RAN_DEFAULT_SLICE) {
    // Give 6 PRBg per TTI for the next 1 (non renewable) seconds
  //  m_slice_sched->set_resources(slice, -1000, 6000);
  //}
  // User is being associated to a slice, so remove it form the default one
  //else {
  //  m_slice_map[RAN_DEFAULT_SLICE].users.erase(rnti);
  //}

  Info("User %d associated to slice %" PRIu64 "\n", rnti, slice);

  pthread_spin_unlock(&m_lock);

  return 0;
}

/* Routine:
 *    dl_metric_ran::rem_slice_user()
 *
 * Abstract:
 *    Removes an association of an user with a slice.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, ID of the user to remove from the slice
 *    - slice, ID of the slice
 *
 * Returns:
 *    Zero on success, otherwise a negative error code
 */
void dl_metric_ran::rem_slice_user(uint16_t rnti, uint64_t slice)
{
  //std::map<uint16_t>::iterator it;
  slice_map_t::iterator        si;
  pthread_spin_lock(&m_lock);

  // Remove from any slice
  if(slice == 0) {
    // Remove any instance of that user 
    for(si = m_slice_map.begin(); si != m_slice_map.end(); ++si) {
      if(si->second.users.count(rnti) > 0) {
        si->second.users.erase(rnti);
      }
    }
  }
  // Remove from a specific slice
  else {
    m_slice_map[slice].users.erase(rnti);
  }

  if(m_user_map.count(rnti) > 0) {
    // The user has been associated by the agent, so do not handle by yourself
    //m_user_map[rnti].self_m = !lock;
    m_user_map.erase(rnti);
    m_slice_map[slice].users.erase(rnti);

    Info("Slice %d removed from slice %" PRIu64 "\n", rnti, slice);
  }

  pthread_spin_unlock(&m_lock);

  return;
}

/* Routine:
 *    dl_metric_ran::get_slice_sched_id()
 *
 * Abstract:
 *    Returns the ID of the slicer scheduler currently running in the system
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 *
 * Returns:
 *    ID of the scheduler
 */
uint32_t dl_metric_ran::get_slice_sched_id()
{
  return m_slice_sched->m_id;
}

/* Routine:
 *    dl_metric_ran::get_slice_info()
 *
 * Abstract:
 *    Query a slice for its current configuration
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - id, ID of the slice to query
 *    - args, information filled with the slice configuration
 *
 * Returns:
 *    Zero on success, otherwise a negative error code
 */
int dl_metric_ran::get_slice_info(uint64_t id,  mac_set_slice_args * args)
{
  int res = 0;

  if(m_slice_map.count(id) == 0) {
    Error("Slice %" PRIu64 " not found in the MAC scheduler\n", id);
    return -1;
  }

  args->user_sched = m_slice_map[id].sched_user->m_id;
  m_slice_sched->get_resources(id, 0, &res);
  args->rbg = res;

  // Do not handle users; will be set by upper layers
  args->nof_users  = 0;

  return 0;
}

/* Routine:
 *    dl_metric_ran::new_tti()
 *
 * Abstract:
 *    Organize this new Downlink TTI. During this stage information will be
 *    collected, users will ahve their properties changed due to specific
 *    policies and the slicing mechanism associate RBGs with users.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - ue_db, UE database of the MAC layer
 *    - start_rgb, RBG from which is possible to allocate resources
 *    - nof_rbg, number of RBGs which is possible to allocate from the start
 *    - nof_ctrl_sym, number of control symbols per subframe
 *    - tti, TTI number
 *
 * Returns:
 *    ---
 */
void dl_metric_ran::new_tti(
  std::map<uint16_t, sched_ue> &ue_db,
  uint32_t                      start_rbg,
  uint32_t                      nof_rbg,
  uint32_t                      nof_ctrl_sym,
  uint32_t                      tti)
{
  uint32_t i;

  uint32_t       has_data = 0;
  dl_harq_proc * has_harq = 0;

  slice_map_t::iterator                  ti;
  std::list<uint16_t>::iterator          ui;
  std::map<uint16_t, sched_ue>::iterator iter;

  sched_ue * user;

  m_tti_abs++;
  m_tti_rbg_start = start_rbg;
  m_tti_rbg_left  = nof_rbg - start_rbg;
  m_tti_rbg_total = nof_rbg;
  m_tti           = tti;
  m_ctrl_sym      = nof_ctrl_sym;
  
  // Guess the BW of the cell from the given resources
  if(m_max_rbg < nof_rbg) {
    if(nof_rbg <= 6) {
      m_max_rbg  = 6;
      m_rbg_size = 1;
    } else if(nof_rbg > 6 && nof_rbg <= 8) {
      m_max_rbg  = 8;
      m_rbg_size = 2;
    } else if(nof_rbg > 8 && nof_rbg <= 13) {
      m_max_rbg  = 13;
      m_rbg_size = 2;
    } else if(nof_rbg > 13 && nof_rbg <= 17) {
      m_max_rbg  = 17;
      m_rbg_size = 3;
    } else if(nof_rbg > 17 && nof_rbg <= 19) {
      m_max_rbg  = 19;
      m_rbg_size = 4;
    } else if(nof_rbg > 19 && nof_rbg <= 25) {
      m_max_rbg  = 25;
      m_rbg_size = 4;
    }
  }

  /* Prepare and array boolean elements which describes the situation of the
   * RBGs in this transmission interval. Groups allocated from the system are
   * excluded from the ones which is possible to allocate.
   */
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    if (i < start_rbg) {
      m_tti_rbg[i] = true;   // In use
    } else {
      if (i >= start_rbg + nof_rbg) {
        m_tti_rbg[i] = true; // In use
      } else {
        m_tti_rbg[i] = false;// Can be used
      }
    }
  }

  // Reset the situation of the current sub-frame
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    m_tti_users[i] = 0;
  }

  /* Apply changes with per-user basis. Depending on various policies the state
   * of users can change with time. This is done here.
   */
  for (iter = ue_db.begin(); iter != ue_db.end(); ++iter) {
    user = (sched_ue *)&iter->second;

    // User has new or re-tx data?
    has_data = user->get_pending_dl_new_data(m_tti);
    has_harq = user->get_pending_dl_harq(m_tti);

    pthread_spin_lock(&m_lock);

    // Add or update an UE entry
    if (m_user_map.count(user->rnti) == 0) {
      m_user_map[user->rnti].self_m        = 1;
      m_user_map[user->rnti].last_seen     = m_tti_abs;
      m_user_map[user->rnti].DL_data       = 0;
      m_user_map[user->rnti].DL_data_delta = 0;
    } else {
      // Out for 5 seconds? Consider it as out
      if (m_tti_abs - m_user_map[user->rnti].last_seen > 5000) {
        m_user_map[user->rnti].self_m        = 1;
        m_user_map[user->rnti].last_seen     = m_tti_abs;
        m_user_map[user->rnti].DL_data       = 0;
        m_user_map[user->rnti].DL_data_delta = 0;
      } else {
        // Save the TTI where we saw this user last time
        m_user_map[user->rnti].last_seen = m_tti_abs;
      }
    }

    // Regardless of what happens, register if it has data or not
    if(has_data > 0 || has_harq) {
      m_user_map[user->rnti].has_data = 1;
    } else {
      m_user_map[user->rnti].has_data = 0;
    }    

  pthread_spin_unlock(&m_lock);
  }

  // Has a slice scheduler associated? 
  if (m_slice_sched) {
    // Finally run the schedulers
    m_slice_sched->schedule(
      m_tti, &m_slice_map, &m_user_map, m_tti_rbg, m_tti_users);
  }

  rtrace_new_tti(&this->m_rtd);
}

/* Routine:
 *    dl_metric_ran::get_user_allocation()
 *
 * Abstract:
 *    After the TTI has been organized, now the system asks if a particular user
 *    has some possible allocation in the given TTI. This procedure look for
 *    the previously organized allocation.
 *
 * Assumptions:
 *    'm_tti_users' variable filled with useful information which are on a 
 *    per-rnti basis.
 * 
 * Arguments:
 *    - user, MAC information of the user *    
 *
 * Returns:
 *    A valid DL HARQ pointer on success, otherwise a null pointer to inform
 *    that no valid allocation for this user are possible.
 */
dl_harq_proc * dl_metric_ran::get_user_allocation(sched_ue * user)
{
  int            i;
  uint16_t       rnti      = (uint16_t)user->rnti;
  uint32_t       h_mask    = 0;
  uint32_t       rbg_mask  = 0;
  uint32_t       nof_rbg   = 0;
  uint32_t       nof_h_rbg = 0;
  uint32_t       rbg_valid = 0;
  uint32_t       dsize     = 0;
  /* WARNING:
   * This MCS is modified after getting the user allocation, to technically
   * this is the MCS of the previous TTI, not of this one. Still is an
   * interesting statistics which can be stored.
   */
  int            mcs       = user->get_dl_mcs();
  bool           ualloc[RAN_DL_MAX_RGB];
  dl_harq_proc * harq_new;
  dl_harq_proc * harq_ret  = user->get_pending_dl_harq(m_tti);

  // Prepare the mask where this user has the right to allocate data in
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    if (m_tti_users[i] == rnti) {
      ualloc[i] = true;
      nof_rbg++; // Count the number of RBG in the meantime.
    } else {
      ualloc[i] = false;
    }
  }

  // This user is not present, so stop here
  if (nof_rbg == 0) {
    m_user_map[rnti].DL_rbg_delta  = 0;

    rtrace_dl_mask(&this->m_rtd, rnti, 1 << 29, mcs);
    return NULL;
  }

  rbg_mask = calc_rbg_mask(ualloc);

  /*
   *
   * Case: Pending, old, data should be sent.
   *
   */

  // Process any active HARQ first
#if ASYNC_DL_SCHED
  if (harq_ret) {
#else
  if (harq_ret && !harq_ret->is_empty()) {
#endif
    h_mask = harq_ret->get_rbgmask();

    /*
     *
     * Case: HARQ mask CAN fit in the given mask!
     *
     */

    if (allocation_is_valid(rbg_mask, h_mask)) {
      m_user_map[rnti].DL_rbg_delta = nof_rbg;

      rtrace_dl_mask(&this->m_rtd, rnti, h_mask, mcs);
      return harq_ret;
    }

    // Slots are not similar, so count how many RBG we need
    nof_h_rbg = count_rbg(h_mask);

    /*
     *
     * Case: HARQ mask CANNOT fit in the given mask, but there are enough
     * resources to create a new one.
     *
     */

    if (nof_h_rbg <= nof_rbg) {
      rbg_valid = new_allocation(nof_h_rbg, ualloc, &h_mask);

      // Accumulate how many RBG have been consumed
      m_user_map[rnti].DL_rbg_delta = rbg_valid;

      harq_ret->set_rbgmask(h_mask);

      rtrace_dl_mask(&this->m_rtd, rnti, h_mask, mcs);
      return harq_ret;
    }

    /*
     *
     * Case: HARQ mask cannot fit in the given resources!
     *
     */

    Error("HARQ: %x, avail %x, Not possible to schedule HARQ\n", h_mask, rbg_mask);

    rtrace_dl_mask(&this->m_rtd, rnti, 0, mcs);
    return NULL;
  }

  /*
   *
   * Case: New data can be sent?
   *
   */

  harq_new  = user->get_empty_dl_harq();

#if ASYNC_DL_SCHED
  if (harq_new) {
#else
  if (harq_new && harq_new->is_empty()) {
#endif

    /*
     *
     * Case: Is there data from upper layers that should be sent?
     *
     */

    dsize = user->get_pending_dl_new_data(m_tti);

    if (dsize > 0) {
      nof_h_rbg = user->get_required_prb_dl(dsize, m_ctrl_sym);

      /* NOTE: This is valid only until get_required_prb_dl returns PRB and not
       * RBG size.
       */
      nof_h_rbg = (nof_h_rbg / m_rbg_size) + 1;

      h_mask    = 0;
      rbg_valid = new_allocation(nof_h_rbg, ualloc, &h_mask);

      /*
       *
       * Case: Is it possible to use a valid set of RBG?
       *
       */

      if (h_mask) {
        harq_new->set_rbgmask(h_mask);
        m_user_map[rnti].DL_rbg_delta = rbg_valid;

        rtrace_dl_mask(&this->m_rtd, rnti, h_mask, mcs);
        return harq_new;
      }
    }
  }

  m_user_map[rnti].DL_rbg_delta = 0;
  rtrace_dl_mask(&this->m_rtd, rnti, 0, mcs);

  return NULL; 
}

/* Routine:
 *    dl_metric_ran::calc_rbg_mask()
 *
 * Abstract:
 *    Build up a bits-mask starting from the given boolean array. Every element
 *    set as true will be marked as 1 in the mask.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - mask, array of boolean values   
 *
 * Returns:
 *    The bitmask
 */
uint32_t dl_metric_ran::calc_rbg_mask(bool mask[RAN_DL_MAX_RGB])
{
  uint32_t n;
  uint32_t rbg_bitmask = 0;

  for (n = 0; n < m_tti_rbg_total; n++) {
    if (mask[n]) {
      rbg_bitmask |= (1 << n);
    }
  }

  return rbg_bitmask;
}

/* Routine:
 *    dl_metric_ran::allocation_is_valid()
 *
 * Abstract:
 *    Check if the given allocation is valid for a given bitmask.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - base, allocation to check
 *    - mask, bitmask of resources
 *
 * Returns:
 *    True or false depending if the allocation is valid or not
 */
bool dl_metric_ran::allocation_is_valid(uint32_t base, uint32_t mask)
{
  return (mask == base);
}

/* Routine:
 *    dl_metric_ran::count_rbg()
 *
 * Abstract:
 *    Starting from a bitmask, count how many RBG are used.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - mask, mask to consider for the count
 *
 * Returns:
 *    Number of groups allocated in the mask
 */
uint32_t dl_metric_ran::count_rbg(uint32_t mask)
{
  uint32_t count = 0; 

  while(mask > 0) {
    if ((mask & 1) == 1) {
      count++; 
    }

    mask >>= 1;
  }

  return count;
}

/* Allocates a number of RBG in the slots given by rbg_mask, returning the
 * adjusted mask in 'final_mask'.
 *
 * Returns the number of RBGs that is possible to allocate.
 */

/* Routine:
 *    dl_metric_ran::new_allocation()
 *
 * Abstract:
 *    Starting from the number of RBG which the system desire to allocate, and
 *    boolean array of groups where is possible to allocate, generate a new 
 *    allocation bitmask.
 *
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - nof_rbg, number of RBG which is requested to allocate
 *    - rbg_mask, mask where is currently possible to allocate elements
 *    - final_mask, pointer to the recomputed bitmask
 *
 * Returns:
 *    Number of RBGs that is possible to allocate.
 */
int dl_metric_ran::new_allocation(
    uint32_t nof_rbg, bool rbg_mask[RAN_DL_MAX_RGB], uint32_t * final_mask)
{
  uint32_t i;
  int      t;

  // Operate on the existing mask of PRBG.
  for (i = 0, t = 0; i < RAN_DL_MAX_RGB; i++) {
      // If can be used, then mark a possible PRBG as consumed
      if (rbg_mask[i]) {
        // We need the RBG?
        if(nof_rbg > 0) {
          t++;
          nof_rbg--;
        }
      }
  }

  if (final_mask) {
      *final_mask = calc_rbg_mask(rbg_mask);
  }

  // How many PRBG have been selected?
  return t;
}

} // namespace srsenb 
