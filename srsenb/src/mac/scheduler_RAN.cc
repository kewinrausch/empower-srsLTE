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

ran_rr_usched::ran_rr_usched()
{
  m_id   = 0x80000001;
  m_last = 0;
}

ran_rr_usched::~ran_rr_usched()
{
  // Nothing
}

/* Scheduler:
 *    Round Robin scheduler for Users
 *
 * Type:
 *    User level
 *
 * Behavior:
 *    Maintains the RNTI of the last scheduled user, and loop through users 
 *    associated with the slice to select the next one. At each selected user
 *    is given the use of the whole spectrum for that subframe.
 *
 * Assumptions:
 *    It assumes that the slice has at least one PRBG assigned to itself during
 *    the given TTI.
 *
 * Output:
 *    The 'ret' arguments is organized to contains that UEs which are allowed
 *    for transmission during this subframe.
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
  if (!rnti && i == slice->users.end()) {
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

int ran_rr_usched::get_param(
  char *       name,
  unsigned int nlen,
  char *             value,
  unsigned int vlen) 
{
  // Currently not allowed to get parameters
  return -1;
}

int ran_rr_usched::set_param(
  char *       name,
  unsigned int nlen,
  char *       value,
  unsigned int vlen)
{
  // Currently not allowed to set parameters
  return -1;
}

/******************************************************************************
 *                                                                            *
 *                         Slice schedulers for RAN                           *
 *                                                                            *
 ******************************************************************************/

/*
 *
 * "DUO-DYNAMIC" SLICE-ASSIGNMENT SCHEDULER
 *
 */

// Constructor for the scheduler class
ran_duodynamic_ssched::ran_duodynamic_ssched()
{
  m_id       = 0x00000002;

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
#ifdef RAN_STATIC  /* <---------------------------------------------------------  No Controller static setup */
  m_tenB     = 2L;
#else
  m_tenB     = 0;
#endif /* <------------------------------------------------------------------------------------------------- */
  
  // Slot of TTIs used for scheduler monitoring
  m_win_slot = 0;
  // Amount of PRBG used by slice A
  m_tenA_rbg = 0;
  // Amount of PRBG used by slice B
  m_tenB_rbg = 0;
  // Number of PRBG per TTI
  m_rbg_max  = 13; // <--------------------------------------------------------- NOTE: Hardcoded for testing purposes
}

ran_duodynamic_ssched::~ran_duodynamic_ssched() 
{
  // Nothing
}

int ran_duodynamic_ssched::get_param(
  char *         name,
  unsigned int   nlen,
  char *         value,
  unsigned int   vlen)
{
  uint32_t rbg;

  uint64_t slice_id;
  char *   slice;

  // Handles RBG assignment 
  if(strcmp(name, "rbg") == 0) {
    slice = strtok(value, ",");
    
    if(!slice) {
      return -1;
    }

    slice_id = strtoull(slice, 0, 10);

    if(slice_id == m_tenA) {
      return (int)m_switch;
    } else if(slice_id == m_tenB) {
      return (int)(m_rbg_max - m_switch);
    }

    // Other tenants have no resources
    return 0;
  }

  return -1;
}

int ran_duodynamic_ssched::set_param(
  char *       name,
  unsigned int nlen,
  char *       value,
  unsigned int vlen)
{
  uint32_t rbg;

  uint64_t slice_id;
  char *   slice;

  char *   val;
  uint16_t val_rbg;

  // Handles RBG assignment 
  if(strcmp(name, "rbg") == 0) {
    slice = strtok(value, ",");
    val = strtok(NULL, ",");
    
    if(!slice || !val) {
      return -1;
    }

    slice_id = strtoull(slice, 0, 10);
    val_rbg  = (uint16_t)atoi(val);

    /* Case Slice A, allocation from 0 to switch:
     *    The allocation requested is the switch itself. This means that checks
     *    can be done directly using the given value 'val_rbg'.
     * 
     *    e.g: If A want 10 RBG, switch should be moved to 10.
     */
    if(m_tenA == slice_id) {
      if(val_rbg > m_rbg_max - m_limit) {
        m_switch = m_rbg_max - m_limit;
      } else if(val_rbg < m_limit) {
        m_switch = m_limit;
      } else {
        m_switch = val_rbg;
      }
    } 
    /* Case Slice B, allocation from switch to the max:
     *    Allocation here happens from switch to max. This means that the target
     *    switch value is 'max - val_rbg'
     * 
     *    e.g: If B want 10 RBG, switch should be moved to 3. 
     *         This means 3 = 13(max) - 10(requested)
     */
    else if(m_tenB == slice_id) {
      rbg = m_rbg_max - val_rbg;
      
      if(rbg > m_rbg_max - m_limit) {
        m_switch = m_rbg_max - m_limit;
      } else if(rbg < m_limit) {
        m_switch = m_limit;
      } else {
        m_switch = rbg;
      }
    }
  }

  // Parameters setting is not supported right now
  return -1;
}

/* Scheduler:
 *    Duo Dynamic scheduler for Tenants
 *
 * Type:
 *    Slice level
 *
 * Behavior:
 *    The scheduler keeps a 'barrier' switch between the two Tenants, which 
 *    identifies where the resources of the first terminates and start the ones
 *    of the second.
 *
 * Assumptions:
 *    Does not enforce any security check after all the User schedulers have
 *    been ran. This means that miss-behaving user schedulers can still mess
 *    with the spectrum if poorly implemented.
 *
 * Output:
 *    The 'ret' arguments is organized to contains that UEs which are allowed
 *    for transmission during this subframe.
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

  ran_mac_slice *       sched_t = 0;

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

void dl_metric_ran::init(srslte::log * log_handle)
{
  m_log        = log_handle;
#ifdef RAN_TRACE
  m_rtd.logger = log_handle;
#endif // RAN_TRACE

  pthread_spin_init(&m_lock, 0);

  m_slice_sched = new ran_duodynamic_ssched();

  /* Adds the special slice 1.
   * All UE belongs to slice 1 at the start, and this allows them to complete
   * connection procedures.
   */
  //m_slice_map[RAN_SLICE_STARTING].sched_user = new ran_rr_usched();

#ifdef RAN_STATIC /* <----------------------------------------------------------  No Controller static setup */
  m_slice_map[2L].sched_user = new ran_rr_usched();
#endif /* <------------------------------------------------------------------------------------------------- */
}

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
  if(((ran_duodynamic_ssched *)m_slice_sched)->m_tenA != id) {
    ((ran_duodynamic_ssched *)m_slice_sched)->m_tenB = id;
  }

//------------------------------------------------------------------------------ TEMPORARY!

  pthread_spin_unlock(&m_lock);

  Info("Slice %" PRIu64 " added to RAN MAC scheduler\n", id);

  return 0;
}

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
  if(((ran_duodynamic_ssched *)m_slice_sched)->m_tenB == id) {
    ((ran_duodynamic_ssched *)m_slice_sched)->m_tenB = 0L;
  }
//------------------------------------------------------------------------------ TEMPORARY!

  us = it->second.sched_user;
  m_slice_map.erase(it);

  pthread_spin_unlock(&m_lock);

  // Delete the class instance 
  delete us;

  Info("Slice %" PRIu64 " removed from RAN MAC scheduler\n", id);

  return;
}

int dl_metric_ran::set_slice(uint64_t id, mac_set_slice_args * args)
{
  char value[64] = { 0 };

  /* 
   *
   * IMPORTANT: This is specific to Dynamic Duo!
   * 
   */

  sprintf(value, "%" PRIu64 ",%d", id, args->rbg);

  // Feed the argument to the scheduler 
  return m_slice_sched->set_param(
    (char *)"rbg", 4, value, strnlen(value, 64));
}

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
  m_user_map[rnti].self_m = !lock;
  m_slice_map[slice].users[rnti] = 1;

  Info("User %d associated to slice %" PRIu64 "\n", rnti, slice);

  pthread_spin_unlock(&m_lock);

  return 0;
}

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
#if 0
void dl_metric_ran::get_user_info(
  uint16_t rnti, std::map<uint16_t, std::list<uint64_t> > & users)
{
  slice_map_t::iterator         ti;
  std::list<uint16_t>::iterator ui;

  pthread_rwlock_rdlock(&m_lock);

  // Collect info for every active slice in the system 
  for (ti = m_slice_map.begin(); ti != m_slice_map.end(); ++ti) {
    for (ui = ti->second.users.begin(); ui != ti->second.users.end(); ++ui) {
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
  
  pthread_rwlock_unlock(&m_lock);

  return;
}
#endif
uint32_t dl_metric_ran::get_slice_sched_id()
{
  return m_slice_sched->m_id;
}

int dl_metric_ran::get_slice_info(uint64_t id,  mac_set_slice_args * args)
{
  char value[64] = { 0 };

  /* 
   *
   * IMPORTANT: This is specific to Dynamic Duo!
   * 
   */

  sprintf(value, "%" PRIu64 ",", id);

  if(m_slice_map.count(id) == 0) {
    Error("Slice %" PRIu64 " not found in the MAC scheduler\n", id);
    return -1;
  }

  args->user_sched = m_slice_map[id].sched_user->m_id;
  args->rbg        = (uint16_t)m_slice_sched->get_param(
    (char *)"rbg", 4, value, strnlen(value, 64));

  // Do not handle users; will be set by upper layers
  args->nof_users  = 0;

  return 0;
}

#ifdef RAN_STATIC /* <----------------------------------------------------------  No Controller static setup */
uint16_t ue_a = 0;
uint16_t ue_b = 0;
#endif /* <------------------------------------------------------------------------------------------------- */

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
  
  /* Guess the cell used BW; this is likely to be called just 2 or 3 times in
   * the entire life of the scheduler, because BW will not change on runtime.
   */
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

  /* Prepare not the status of this TTI PRGB allocation.
   *
   * The basic operation is removing those PRB Groups that are before the given
   * start.
   *
   * Additional operations which can exclude groups can happen here.
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

  

#ifdef RAN_STATIC /* <----------------------------------------------------------  No Controller static setup */
  // Nothing, do not remove users...
#else
  // Reset the users of the starting slice
  //m_slice_map[RAN_SLICE_STARTING].users.clear();
#endif /* <------------------------------------------------------------------------------------------------- */

  // Reset the situation of the current sub-frame
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    m_tti_users[i] = 0;
  }

  // Save for each user its own RNTI
  for (iter = ue_db.begin(); iter != ue_db.end(); ++iter) {
    user = (sched_ue *)&iter->second;

    // Save this user RNTI
    user->ue_idx = (uint32_t)iter->first;

    has_data = user->get_pending_dl_new_data(m_tti);
    has_harq = user->get_pending_dl_harq(m_tti);

    pthread_spin_lock(&m_lock);

    // Add or update an UE entry
    if (m_user_map.count(iter->first) == 0) {
      m_user_map[iter->first].self_m        = 1;
      m_user_map[iter->first].last_seen     = m_tti_abs;
      m_user_map[iter->first].DL_data       = 0;
      m_user_map[iter->first].DL_data_delta = 0;
    } else {
      // Out for 5 frames? Consider it as out
      if (m_tti_abs - m_user_map[iter->first].last_seen > 5000) {
        m_user_map[iter->first].self_m        = 1;
        m_user_map[iter->first].last_seen     = m_tti_abs;
        m_user_map[iter->first].DL_data       = 0;
        m_user_map[iter->first].DL_data_delta = 0;
      }
      else {
        // Save the TTI where we saw this user last time
        m_user_map[iter->first].last_seen = m_tti_abs;
      }
    }

#ifdef RAN_STATIC /* <----------------------------------------------------------  No Controller static setup */
    /* Perform cleanup operations to remove RNTIs which are no more handled by 
     * the MAC layer
     */
    for (ti = m_slice_map.begin(); ti != m_slice_map.end(); ++ti) {
      for (ui = ti->second.users.begin(); ui != ti->second.users.end(); ++ui) {
        if (ue_db.count(*ui) == 0) {
          if (*ui == ue_a) {
            ue_a = 0;
          }
          if (*ui == ue_b) {
            ue_b = 0;
          }
          Warning("UE %x removed from tenant %" PRIu64 "\n", *ui, ti->first);
          ui = ti->second.users.erase(ui);
        }
      }
    }  

    /* Assign the UE_a if not already stored in UE_b*/
    if (!ue_a && iter->first != ue_b) {
      ue_a = iter->first;
      m_slice_map[RAN_SLICE_STARTING].users.push_back(ue_a);
      Warning("UE %x assigned to tenant %" PRIu64 "\n", ue_a, RAN_SLICE_STARTING);
    } else {
      if (!ue_b && iter->first != ue_a) {
        ue_b = iter->first;
        m_slice_map[2L].users.push_back(ue_b);
        Warning("UE %x assigned to tenant %" PRIu64 "\n", ue_b, 2L);
      }
    }
#else
    /*
    // Everyone which is not managed belongs to the default slice 
    if(m_user_map[iter->first].self_m) {
      for(
        ui = m_slice_map[RAN_SLICE_STARTING].users.begin();
        ui = m_slice_map[RAN_SLICE_STARTING].users.end();
        ++ui) 
        {
          if(ui == iter->first) {
            break;
          }
        }

      // Add only whoever is not already present thus
      if(ui == m_slice_map[RAN_SLICE_STARTING].users.end()){
        m_slice_map[RAN_SLICE_STARTING].users.push_back(iter->first);
      }
    }
    */
  if(m_user_map.count(iter->first) == 0) {
    m_user_map[iter->first].self_m = 1;
    m_slice_map[RAN_SLICE_STARTING].users[iter->first] = 1;
  }
  
  pthread_spin_unlock(&m_lock);

#endif /* <------------------------------------------------------------------------------------------------- */
  }

  if (m_slice_sched) {
    // Finally run the schedulers
    m_slice_sched->schedule(
      m_tti, &m_slice_map, &m_user_map, m_tti_rbg, m_tti_users);
  }

  rtrace_new_tti(&this->m_rtd);
}

dl_harq_proc * dl_metric_ran::get_user_allocation(sched_ue * user)
{
  int            i;
  uint16_t       rnti      = (uint16_t)user->ue_idx;
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

/* Build up the PRBG bits-mask starting from a boolean array.
 *
 * Every element set as 'true' will mark a bit in the same position into the
 * mask.
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

/* Does the mask fit in the mask of the used PRBG?
 *
 * Returns 1 only is the mask matches.
 */
bool dl_metric_ran::allocation_is_valid(uint32_t base, uint32_t mask)
{
        return (mask == base);
}

// Count how many PRBG are in use in a bits-mask.
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
