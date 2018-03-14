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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "srslte/common/log.h"
#include "srslte/phy/common/phy_common.h"
#include "srslte/phy/phch/cqi.h"
#include "srslte/phy/utils/vector.h"

#include "mac/scheduler_harq.h"
#include "mac/scheduler_RAN.h"

#define Error(fmt, ...)                             \
  do {                                              \
    m_log->error("RAN: "fmt, ##__VA_ARGS__);        \
  } while(0)

#define Warning(fmt, ...)                           \
  do {                                              \
    m_log->warning("RAN: "fmt, ##__VA_ARGS__);      \
  } while(0)

#define Info(fmt, ...)                              \
  do {                                              \
    m_log->info("RAN: "fmt, ##__VA_ARGS__);         \
  } while(0)

#define Debug(fmt, ...)                             \
  do {                                              \
    m_log->debug("RAN: "fmt, ##__VA_ARGS__);        \
  } while(0)

namespace srsenb {

/******************************************************************************
 *                                                                            *
 *                         User schedulers for RAN                            *
 *                                                                            *
 ******************************************************************************/

/*
 *
 * ROUND-ROBIN RESOURCE ALLOCATION FOR TENANT USERS
 *
 */

ran_rr_usched::ran_rr_usched()
{
  m_last = 0;
}

/* Scheduler:
 *    Round Robin scheduler for Users
 *
 * Type:
 *    User level
 *
 * Behavior:
 *    Maintains the RNTI of the last scheduled user, and loop through users 
 *    associated with the tenant to select the next one. At each selected user
 *    is given the use of the whole spectrum for that subframe.
 *
 * Assumptions:
 *    It assumes that the tenant has at least one PRBG assigned to itself during
 *    the given TTI.
 *
 * Output:
 *    The 'ret' arguments is organized to contains that UEs which are allowed
 *    for transmission during this subframe.
 */
void ran_rr_usched::schedule(
  const uint32_t     tti,
  ran_tenant *       tenant,
  user_map_t *       umap,
  bool               rbg[RAN_DL_MAX_RGB],
  uint16_t           ret[RAN_DL_MAX_RGB]) 
{
  uint16_t first = 0;
  uint16_t rnti  = 0;

  std::list<uint16_t>::iterator i;
  int j;

   /* Select the next candidate RNTI */
  for(i = tenant->users.begin(); i != tenant->users.end(); ++i) {
    /* Last RNTI not selected yet? */
    if(!m_last) {
      rnti = *i;
      break;
    }

    /* Save the first valid RNTI in case we reach the end of the list */
    if (!first) {
      first = *i;
    }

    /* Ok, this was the last selected */
    if(*i == m_last) {
      i++;

      /* If we are at the end of the list, select the first again */
      if(i == tenant->users.end()) {
        rnti = first;
        break;
      } 
      /* Otherwise  */
      else {
        rnti = *i;
        break;
      }
    }
  }

  /* In the case we reached the end but no RNTI has been selected yet */
  if (!rnti && i == tenant->users.end()) {
    rnti = first;
  }

  /* Assign the groups of this tenant to the designed RNTI */
  for(j = 0; j < RAN_DL_MAX_RGB; j++) {
    /* This group is NOT in use, so free for me :) */
    if (!rbg[j]) {
      ret[j] = rnti;
    }
  }

  return;
}

int ran_rr_usched::get_param(
  const char *       name,
  const unsigned int nlen,
  char *             value,
  const unsigned int vlen) 
{
  /* Currently not allowed to get parameters */
  return -1;
}

int ran_rr_usched::set_param(
  const char *       name,
  const unsigned int nlen,
  const char *       value,
  const unsigned int vlen)
{
  /* Currently not allowed to set parameters */
  return -1;
}

/******************************************************************************
 *                                                                            *
 *                        Tenant schedulers for RAN                           *
 *                                                                            *
 ******************************************************************************/

/*
 *
 * "STATIC" TENANT-ASSIGNMENT SCHEDULER
 *
 */

/* Constructor for the scheduler class */
ran_static_tsched::ran_static_tsched()
{
  pthread_mutex_init(&m_lock, 0);

  m_win           = 10;
  m_tenant_matrix = (uint64_t *)malloc(
    sizeof(uint64_t) * m_win * RAN_DL_MAX_RGB);

  /* Clear the memory area */
  memset(m_tenant_matrix, 0, sizeof(uint64_t) * m_win * RAN_DL_MAX_RGB);
  
  /* Subframe 0 assigned to tenant 1, the initial one for all the UEs */

  m_tenant_matrix[0]  = RAN_TENANT_STARTING;
  m_tenant_matrix[1]  = RAN_TENANT_STARTING;
  m_tenant_matrix[2]  = RAN_TENANT_STARTING;
  m_tenant_matrix[3]  = RAN_TENANT_STARTING;
  m_tenant_matrix[4]  = RAN_TENANT_STARTING;
  m_tenant_matrix[5]  = RAN_TENANT_STARTING;
  m_tenant_matrix[6]  = RAN_TENANT_STARTING;
  m_tenant_matrix[7]  = RAN_TENANT_STARTING;
  m_tenant_matrix[8]  = RAN_TENANT_STARTING;
  m_tenant_matrix[9]  = RAN_TENANT_STARTING;

  m_tenant_matrix[10] = RAN_TENANT_STARTING;
  m_tenant_matrix[11] = RAN_TENANT_STARTING;
  m_tenant_matrix[12] = RAN_TENANT_STARTING;
  m_tenant_matrix[13] = RAN_TENANT_STARTING;
  m_tenant_matrix[14] = RAN_TENANT_STARTING;
  m_tenant_matrix[15] = RAN_TENANT_STARTING;
  m_tenant_matrix[16] = RAN_TENANT_STARTING;
  m_tenant_matrix[17] = RAN_TENANT_STARTING;
  m_tenant_matrix[18] = RAN_TENANT_STARTING;
  m_tenant_matrix[19] = RAN_TENANT_STARTING;

  m_tenant_matrix[20] = RAN_TENANT_STARTING;
  m_tenant_matrix[21] = RAN_TENANT_STARTING;
  m_tenant_matrix[22] = RAN_TENANT_STARTING;
  m_tenant_matrix[23] = RAN_TENANT_STARTING;
  m_tenant_matrix[24] = RAN_TENANT_STARTING;
}

int ran_static_tsched::get_param(
  const char *         name,
  const unsigned int   nlen,
  char *               value,
  const unsigned int   vlen)
{
  unsigned int i;
  unsigned int vl = 0;

  /* TTI window request; do not count null-terminator */
  if(strncmp(name, "tti_window", 10) == 0) {
    /* Since no-one is going to modify this element while the agent read it (
     * is a scheduler property!), we can avoid to lock the resource.
     */
    return sprintf(value, "%d", m_win);
  }

  /* Tenant matrix request; do not count null-terminator */
  if (strncmp(name, "tenant_map", 10) == 0) {
    /* Since no-one is going to modify this element while the agent read it (
     * is a scheduler property!), we can avoid to lock the resource.
     */
    for(i = 0; i < (m_win * RAN_DL_MAX_RGB) && vl < vlen; i++) {
      vl += sprintf(value, "%ld,", m_tenant_matrix[i]);
    }

    return vl;
  }

  /* Parameter does not exists */
  return -1;
}

int ran_static_tsched::set_param(
  const char *       name,
  const unsigned int nlen,
  const char *       value,
  const unsigned int vlen)
{
  /* Error, for the moment */
  return -1;
}

/* Scheduler:
 *    Static scheduler for Tenants
 *
 * Type:
 *    Tenant level
 *
 * Behavior:
 *    Maintains a map of the associated resources which are linked to Tenant 
 *    IDs. This maps is acquired and updated by the controller. Resources passed
 *    by 'rbg' variable are polished and organized between Tenants, then each 
 *    Tenants has the possibility to run their own user scheduler to prepare the
 *    subframe organization of RNTIs.
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
void ran_static_tsched::schedule(
  const uint32_t       tti,
  tenant_map_t *       tmap,
  user_map_t *         umap,
  bool                 rbg[RAN_DL_MAX_RGB],
  uint16_t             ret[RAN_DL_MAX_RGB])
{
  int        i;
  int        s; /* Something for this tenant? */

  uint32_t   tti_idx = tti % m_win;
  uint64_t * tslice  = m_tenant_matrix + (tti_idx * RAN_DL_MAX_RGB);
  bool       trbg[RAN_DL_MAX_RGB] = {1};

  tenant_map_t::iterator t;

  for (t = tmap->begin(); t != tmap->end(); ++t) {
    s = 0;

    for (i = 0; i < RAN_DL_MAX_RGB; i++) {
      /* Skip groups already in use */
      if(rbg[i]) {
        continue;
      }

      /* Mark as 'not in use' the PRBG assigned to this tenant */
      if (t->first == tslice[i]) {
        trbg[i] = 0;
        s       = 1;
      } else {
        trbg[i] = 1;
      }
    }

    /* If at least one group is allocated to this tenant, schedule it's users */
    if (s && t->second.sched_user) {
      t->second.sched_user->schedule(tti, &t->second, umap, trbg, ret);
    }
  }

  return;
}

/*
 *
 * "DUO-DYNAMIC" TENANT-ASSIGNMENT SCHEDULER
 *
 */

/* Constructor for the scheduler class */
ran_duodynamic_tsched::ran_duodynamic_tsched()
{
  /* Tenant A area starts (including) from PRBG 0 */
  /* Tenant B area starts (including) from PRBG 6 */
  m_switch   = 6;
  /* Limit is 3, leaving 2 fixed PRBG per Tenant as backup resources */
  m_limit    = 3;
  /* Window to consider is one frame (10 subframes) */
  m_win      = 10;
  /* Tenant A ID */
  m_tenA     = 2; /* <------------ NOTE: Hardcoded for testing purposes */
  /* Tenant B ID */
  m_tenB     = 3; /* <------------ NOTE: Hardcoded for testing purposes */
  /* Slot of TTIs used for scheduler monitoring */
  m_win_slot = 0;
  /* Amount of PRBG used by tenant A */
  m_tenA_rbg = 0;
  /* Amount of PRBG used by tenant B */
  m_tenB_rbg = 0;
  /* Number of PRBG per TTI */
  m_rbg_max    = 13; /* <----------- NOTE: Hardcoded for testing purposes */
}

int ran_duodynamic_tsched::get_param(
  const char *         name,
  const unsigned int   nlen,
  char *               value,
  const unsigned int   vlen)
{
  /* Parameters getting is not supported right now */
  return -1;
}

int ran_duodynamic_tsched::set_param(
  const char *       name,
  const unsigned int nlen,
  const char *       value,
  const unsigned int vlen)
{
  /* Parameters setting is not supported right now */
  return -1;
}

/* Scheduler:
 *    Duo Dynamic scheduler for Tenants
 *
 * Type:
 *    Tenant level
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
void ran_duodynamic_tsched::schedule(
  const uint32_t       tti,
  tenant_map_t *       tmap,
  user_map_t *         umap,
  bool                 rbg[RAN_DL_MAX_RGB],
  uint16_t             ret[RAN_DL_MAX_RGB])
{
  int          i;

  uint64_t     ten;
  uint64_t     ten_load = 0;
  uint32_t     tti_idx  = tti % m_win;
  bool         trbg[RAN_DL_MAX_RGB] = { 1 };

  ran_tenant * sched_t = 0;

  tenant_map_t::iterator        t;
  user_map_t::iterator          u;
  std::list<uint16_t>::iterator l;

  /* Perform scheduling tenant per tenant */
  for (t = tmap->begin(); t != tmap->end(); ++t) {
    /* Monitor the usage of Tenant A */
    if(t->first == m_tenA) {
      for(l = t->second.users.begin(); l != t->second.users.end(); ++l) {
        if(umap->count(*l) > 0) {
          u = umap->find(*l);
          m_tenA_rbg += u->second.DL_rbg_delta;
        }
      }

      /* Skip groups already in use and prepare the map for Tenant A */
      for (i = 0; i < RAN_DL_MAX_RGB; i++) {
        if (rbg[i]) {
          trbg[i] = 1;
        } else {
          /* Tenant A lies in the lower part of the spectrum */
          if (i < m_switch) {
            trbg[i] = 0;
          } else {
            trbg[i] = 1;
          }
        }
      }

      /* Invoke this tenant user-level scheduler */
      if(t->second.sched_user) {
        t->second.sched_user->schedule(tti, &t->second, umap, trbg, ret);
      }
    }

    /* Monitor the usage of Tenant B */
    if (t->first == m_tenB) {
      for (l = t->second.users.begin(); l != t->second.users.end(); ++l) {
        if (umap->count(*l) > 0) {
          u = umap->find(*l);
          m_tenB_rbg += u->second.DL_rbg_delta;
        }
      }

      /* Skip groups already in use and prepare the map for Tenant A */
      for (i = 0; i < RAN_DL_MAX_RGB; i++) {
        if (rbg[i]) {
          trbg[i] = 1;
        }
        else {
          /* Tenant B lies in the upper part of the spectrum */
          if (i >= m_switch) {
            trbg[i] = 0;
          }
          else {
            trbg[i] = 1;
          }
        }
      }

      /* Invoke this tenant user-level scheduler */
      if (t->second.sched_user) {
        t->second.sched_user->schedule(tti, &t->second, umap, trbg, ret);
      }
    }
  }

  /* Reset the monitoring load */

  m_win_slot++;

  /* 5 seconds routine */
  if(m_win_slot == 5000) {
      printf(
        "A: %d - %d\n"
        "B: %d - %d\n", 
        m_tenA_rbg, (m_switch) * 5000, 
        m_tenB_rbg, (m_rbg_max - m_switch) * 5000);

      /* Reset */
      m_win_slot  = 0;
      m_tenA_rbg  = 0;
      m_tenB_rbg  = 0;
  }

  return;
}

/******************************************************************************
 *                                                                            *
 *                          TRACING PART FOR RAN                              *
 *                                                                            *
 ******************************************************************************/

/* RAN tracing capabilities:
 *
 * This set of procedure and data structures are here with the only purpose to 
 * provides non-invasive (for MAC scheduler) statistics over what is happening 
 * in the RAN scheduler.
 *
 * Undefine RAN_TRACE to disable.
 */
#ifdef RAN_TRACE

#define rtrace_new_tti(r)               ran_trace_tti(r)
#define rtrace_dl_mask(r, n, m, c)      ran_trace_DL_mask(r, n, m, c)

/* Trace and log data, eventually */
void ran_trace_tti(dl_metric_ran * ran)
{
  int  i;  
  int  j;
  bool r[32];

  srslte::log * m_log = ran->m_rtd.logger;

  std::map<uint16_t, rt_user>::iterator ui;

  ran->m_rtd.stats.nof_tti++;

  /* Dump the stats! */
  if (ran->m_rtd.stats.nof_tti >= RTRACE_INTERVAL) {
    Warning("*** Dumping statistics ***************************************\n");
    Warning("N.of elapsed TTIs: %d\n", ran->m_rtd.stats.nof_tti);

    for (ui = ran->m_rtd.stats.users.begin(); ui != ran->m_rtd.stats.users.end(); ++ui) {
      if(!ui->second.active) {
        continue;
      }

      Warning("RAN user %x\n", ui->first);

      Warning("    MCS --> "
        "%d %d %d %d %d %d %d %d %d | "          /* QPSK */
        "%d %d %d %d %d %d %d |"                 /* 16-QAM */
        "%d %d %d %d %d %d %d %d %d %d %d %d | " /* 64-QAM */
        "%d %d %d\n",                            /* Reserved */
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
        ui->second.dl_rbg_mcs[30],
        ui->second.dl_rbg_mcs[31]);

      /* Reset MCS statistics */
      memset(ui->second.dl_rbg_mcs, 0, sizeof(int) * RTRACE_NOF_MCS);

      for (i = 0; i < RTRACE_NOF_UMASKS; i++) {
        if (!ui->second.dl_rbg_masks[i]) {
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

    ran->m_rtd.stats.nof_tti = 0;
  }
}

/* Trace single user allocation to show it later */
void ran_trace_DL_mask(dl_metric_ran * ran, uint16_t rnti, uint32_t mask, int mcs)
{
  int  i;
  int  j  = -1;

  ran->m_rtd.stats.users[rnti].active = true;
  ran->m_rtd.stats.users[rnti].dl_rbg_mcs[mcs]++;

  for (i = 0; i < 32; i++) {
      if (j < 0 && ran->m_rtd.stats.users[rnti].dl_rbg_masks[i] == 0) {
        j = i;
      }

      /* Found this mask... again... */
      if (mask == ran->m_rtd.stats.users[rnti].dl_rbg_masks[i]) {
        ran->m_rtd.stats.users[rnti].dl_rbg_count[i] += 1;
        j = -1;

        break;
      }
  }

  if (j >= 0) {
    ran->m_rtd.stats.users[rnti].dl_rbg_count[j] = 1;
    ran->m_rtd.stats.users[rnti].dl_rbg_masks[j] = mask;
  }
}

#else  /* RAN_TRACE */
#define rtrace_new_tti(r)               /* ...into nothing */
#define rtrace_dl_mask(r, n, m, c)      /* ...into nothing */
#endif /* RAN_TRACE */

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
  m_ctrl_sym      = 0;
  m_tenant_id     = 1;
  m_log           = 0;
  m_tenant_sched  = 0;
  
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    m_tti_rbg[i]  = 0;
    m_tti_users[i]= 0;
  }
}

void dl_metric_ran::init(srslte::log * log_handle)
{
  m_log = log_handle;

#ifdef RAN_TRACE
  m_rtd.logger = log_handle;
#endif /* RAN_TRACE */

  /* Tenant scheduler is the static one by default */
  // m_tenant_sched = new ran_static_tsched();
  m_tenant_sched = new ran_duodynamic_tsched();

  /* Adds the special tenant 1.
   * All UE belongs to tenant 1 at the start, and this allows them to complete
   * connection procedures.
   */
  m_tenant_map[RAN_TENANT_STARTING].plmn       = 0x000000;
  m_tenant_map[RAN_TENANT_STARTING].sched_user = new ran_rr_usched();


/* TO REMOVE --------------------------------------------------------------------------> */
  /* NOTE:
   * This part of the code is here to test duodynamic Tenant scheduler without
   * any controller support. This part is hard-coded.
   */

   m_tenant_map[2].plmn = 0x222f93;
   m_tenant_map[2].sched_user = new ran_rr_usched();

   m_tenant_map[3].plmn = 0x222f93;
   m_tenant_map[3].sched_user = new ran_rr_usched();
/* <------------------------------------------------------------------------- UNTIL HERE */
}

uint16_t ue_a = 0;
uint16_t ue_b = 0;

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

  tenant_map_t::iterator                 ti;
  std::list<uint16_t>::iterator          ui;
  std::map<uint16_t, sched_ue>::iterator iter;

  sched_ue * user;

  m_tti_abs++;
  m_tti_rbg_start = start_rbg;
  m_tti_rbg_left  = nof_rbg - start_rbg;
  m_tti_rbg_total = nof_rbg;
  m_tti           = tti;
  m_ctrl_sym      = nof_ctrl_sym;
  
  /* Prepare not the status of this TTI PRGB allocation.
   *
   * The basic operation is removing those PRB Groups that are before the given
   * start.
   *
   * Additional operations which can exclude groups can happen here.
   */
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    if (i < start_rbg) {
      /* In use */
      m_tti_rbg[i] = true;
    } else {
      /* Out of given RBG */
      if (i > m_tti_rbg_total - 1) {
        m_tti_rbg[i] = true;
      } else {
        /* Not in use */
        m_tti_rbg[i] = false;
      }
    }
  }

  /* Reset the users of the starting tenant */
  m_tenant_map[RAN_TENANT_STARTING].users.clear();

  /* Reset the situation of the current subframe */
  memset(m_tti_users, 0, sizeof(uint32_t) * RAN_DL_MAX_RGB);

  /* Save for each user its own RNTI */
  for (iter = ue_db.begin(); iter != ue_db.end(); ++iter) {
    user = (sched_ue *)&iter->second;

    /* Save this user RNTI */
    user->ue_idx = (uint32_t)iter->first;

    has_data = user->get_pending_dl_new_data(m_tti);
    has_harq = user->get_pending_dl_harq(m_tti);

    /* Add or update an UE entry */
    if (m_user_map.count(iter->first) == 0) {
      m_user_map[iter->first].last_seen     = m_tti_abs;
      m_user_map[iter->first].DL_data       = 0;
      m_user_map[iter->first].DL_data_delta = 0;
    } else {
      /* 5 seconds timeout, then free that user resources */
      if (m_tti_abs - m_user_map[iter->first].last_seen > 5000) {
        m_user_map.erase(iter->first);
      }
      else {
        /* Save the TTI where we saw this user last time */
        m_user_map[iter->first].last_seen = m_tti_abs;
      }
    }

    /* Everyone belongs to starting tenant */
    //m_tenant_map[RAN_TENANT_STARTING].users.push_back(iter->first);

    /* Perform cleanup operations to remove RNTIs which are no more handled by 
     * the MAC layer
     */
    for (ti = m_tenant_map.begin(); ti != m_tenant_map.end(); ++ti) {
      for (ui = ti->second.users.begin()++; ui != ti->second.users.end(); ) {
        if (ue_db.count(*ui) == 0) {
/* TO REMOVE --------------------------------------------------------------------------> */
/* Static configuration of duodynamic scheduler */
          if (*ui == ue_a) {
            ue_a = 0;
          }

          if (*ui == ue_b) {
            ue_b = 0;
          }
/* <------------------------------------------------------------------------- UNTIL HERE */
          ti->second.users.erase(ui++);
        } else {
          ui++;
        }
      }
    }  
    
/* TO REMOVE --------------------------------------------------------------------------> */
/* Static configuration of duodynamic scheduler */
    /* Assign the UE_a if not already stored in UE_b*/
    if (!ue_a && iter->first != ue_b) {
      ue_a = iter->first;
      m_tenant_map[2].users.push_back(ue_a);
      Warning("UE %x assigned to tenant %ld\n", ue_a, 2);
    } else {
      if (!ue_b && iter->first != ue_a) {
        ue_b = iter->first;
        m_tenant_map[3].users.push_back(ue_b);
        Warning("UE %x assigned to tenant %ld\n", ue_b, 3);
      }
    }
/* <------------------------------------------------------------------------- UNTIL HERE */
  }

  if (m_tenant_sched) {
    /* Finally run the schedulers */
    m_tenant_sched->schedule(
      m_tti, &m_tenant_map, &m_user_map, m_tti_rbg, m_tti_users);
  }

  /* Tracing mechanism triggered, eventually... */
  rtrace_new_tti(this);
}

dl_harq_proc * dl_metric_ran::get_user_allocation(sched_ue * user)
{
  int            i;
  int            p         = 0;
  uint16_t       rnti      = (uint16_t) user->ue_idx;
  uint32_t       h_mask    = 0;
  uint32_t       rbg_mask  = 0;
  uint32_t       nof_rbg   = 0;
  uint32_t       nof_h_rbg;
  uint32_t       dsize;

  /* WARNING:
   * This MCS is modified after getting the user allocation, to technically 
   * this is the MCS of the previous TTI, not of this one. Still is an 
   * interesting statistics which can be stored.
   */
  int            mcs = user->get_dl_mcs();

  bool           ualloc[RAN_DL_MAX_RGB];;

  dl_harq_proc * harq;

  /* Prepare the mask where this user has the right to allocate data */
  for (i = 0; i < RAN_DL_MAX_RGB; i++) {
    if (m_tti_users[i] == rnti) {
      ualloc[i] = true;
      p         = 1; /* There's something to write here */
      nof_rbg++;     /* Count the number of RBG in the meantime. */
    } else {
      ualloc[i] = false;
    }
  }

  /* This user is not present, so stop here */
  if (!p) {
    m_user_map[rnti].DL_data_delta = 0;
    m_user_map[rnti].DL_rbg_delta  = 0;
    return NULL;
  }

  rbg_mask = calc_rbg_mask(ualloc);
//nof_rbg  = count_rbg(rbg_mask);
  harq     = user->get_pending_dl_harq(m_tti);

  /* Process any active HARQ first */
  if (harq) {
    h_mask = harq->get_rbgmask();

    /* Similar slots are available, use them */
    if (allocation_is_valid(rbg_mask, h_mask)) {
      rtrace_dl_mask(this, rnti, h_mask, mcs);
      return harq;
    }

    nof_h_rbg = count_rbg(h_mask);

    /* Enough space for this harq to be done */
    if (nof_h_rbg < nof_rbg) {
      if(new_allocation(nof_h_rbg, ualloc, &h_mask)) {
        harq->set_rbgmask(h_mask);
        
        rtrace_dl_mask(this, rnti, h_mask, mcs);
        return harq;
      }
    }
  }

  /* No re-tx to be done; proceed with new data */
  harq = user->get_empty_dl_harq();

  if (harq) {
    dsize = user->get_pending_dl_new_data(m_tti);

    /* Is there some new data? */
    if (dsize) {
      nof_h_rbg = user->get_required_prb_dl(dsize, m_ctrl_sym);

      /* Use the available RBGs if we require too much of them */
      if (nof_h_rbg > nof_rbg) {
        nof_h_rbg = nof_rbg;
        /* return NULL; */ /* This was the original decision here */
      }

      m_user_map[rnti].DL_data      += dsize;
      m_user_map[rnti].DL_data_delta = dsize;
      m_user_map[rnti].DL_rbg_delta  = nof_h_rbg;

      new_allocation(nof_h_rbg, ualloc, &h_mask);

      if (h_mask) {
        harq->set_rbgmask(h_mask);
        
        rtrace_dl_mask(this, rnti, h_mask, mcs);
        return harq;
      }
    }
  }

  m_user_map[rnti].DL_data_delta = 0;
  m_user_map[rnti].DL_rbg_delta  = 0;

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

/* Count how many PRBG are in use in a bits-mask. */
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
 * Returns 1 if all the rbg are consumed, 0 otherwise.
 */
bool dl_metric_ran::new_allocation(
    uint32_t nof_rbg, bool rbg_mask[RAN_DL_MAX_RGB], uint32_t * final_mask)
{
    uint32_t i;

    /* Operate on the existing mask of PRBG. */
    for (i = 0; i < RAN_DL_MAX_RGB && nof_rbg > 0; i++) {
        /* Not eligible for allocation, skip it */
        if (rbg_mask[i]) {
            nof_rbg--;
        }
    }

    if (final_mask) {
        *final_mask = calc_rbg_mask(rbg_mask);
    }

    /* Have we consumed all the PRBG? */
    return (nof_rbg == 0);
}

uint32_t dl_metric_ran::add_tenant(uint32_t plmnid)
{
  uint32_t r = m_tenant_id + 1;

  /* Next addition will be done with different id */
  m_tenant_id++;

  /* Yes, I do handle multiple Tenants with the same PLMN id */
  m_tenant_map[r].plmn = plmnid;

  Info("Adding tenant %d, PLMN ID=%x\n", r, m_tenant_map[r].plmn);
  
  return r;
}

int dl_metric_ran::rem_tenant(uint64_t plmnid)
{
  tenant_map_t::iterator i;

  /* Removes any tenant with such PLMN ID */
  for (i = m_tenant_map.begin(); i != m_tenant_map.end(); ++i) {
    if(i->second.plmn == plmnid) {
      //Info("Removing tenant %d\n", i->first);
      m_tenant_map.erase(i);
    }
  }

  return 0;
}

int dl_metric_ran::add_user(uint16_t rnti, uint64_t tenant)
{
  std::list<uint16_t>::iterator i;

  if (m_tenant_map.count(tenant) == 0) {
    Error("Cannot add user %x, Tenant %d does not exists\n", rnti, tenant);
    return -1;
  }

  /* Do not add duplicates */
  for (i = m_tenant_map[tenant].users.begin(); i != m_tenant_map[tenant].users.end(); ++i) {
    if (*i == rnti) {
      return 0;
    }
  }

  m_tenant_map[tenant].users.push_back(rnti);

  //Warning("User %x added to tenant %d\n", rnti, tenant);

  return 0;
}

int dl_metric_ran::rem_user(uint16_t rnti, uint64_t tenant)
{
  std::list<uint16_t>::iterator i;

  if (m_tenant_map.count(tenant) == 0) {
    Error("Cannot remove user %x, Tenant %d does not exists\n", rnti, tenant);
    return -1;
  }

  for (i = m_tenant_map[tenant].users.begin(); i != m_tenant_map[tenant].users.end(); ++i) {
    /* Delete the first element with such RNTI value */
    if(*i == rnti) {
      break;
    }
  }

  if (i != m_tenant_map[tenant].users.end()) {
    m_tenant_map[tenant].users.erase(i);
  }

  //Info("User %x removed from tenant %d\n", rnti, tenant);

  return 0;
}

/******************************************************************************
 *                                                                            *
 *                        UL part of RAN scheduler                            *
 *                                                                            *
 ******************************************************************************/

/*
 * Private procedures:
 */

/* Check whatever a proposed allocation is possible or not */
bool ul_metric_ran::allocation_is_valid(ul_harq_proc::ul_alloc_t alloc)
{
  uint32_t n;

  /* Does it fit? */
  if (alloc.RB_start + alloc.L > nof_rb) {
    return false; 
  }

  /* Some PRBG are already in use? */
  for (n = alloc.RB_start; n < alloc.RB_start + alloc.L; n++) {
    if (used_rb[n]) {
      return false; 
    }
  }

  return true; 
}

/* Check whatever is possible to allocate L PRBs inside an UL allocation.
 * Returns 1 is all the L blocks can be fit, othersize 0.
 */
bool ul_metric_ran::new_allocation(uint32_t L, ul_harq_proc::ul_alloc_t * alloc)
{
  uint32_t n;

  bzero(alloc, sizeof(ul_harq_proc::ul_alloc_t));

  for (n = 0; n < nof_rb && alloc->L < L; n++) {
    /* Select the first not used PRB as start */
    if (!used_rb[n] && alloc->L == 0) {
      alloc->RB_start = n; 
    }

    /* Keep incrementing the allocation space as you find free slots... */
    if (!used_rb[n]) {
      alloc->L++; 
    /* ... or else ... */
    } else if (alloc->L > 0) {
      /* Allocation is too small; start again seeking a bigger space */
      if (n < 3) {
        alloc->RB_start = 0; 
        alloc->L = 0; 
      } 
      /* Allocation has already started, and do not fragment it: stop! */
      else {
        break;
      }
    }
  }

  /* Not possible to allocate something? */
  if (!alloc->L) {
    return 0; 
  }
  
  /* Make sure L is allowed by SC-FDMA modulation */ 
  while (!srslte_dft_precoding_valid_prb(alloc->L)) {
    alloc->L--;
  }

  /* Can we fit the entire desired space? */
  return alloc->L == L; 
}

/*
 * Public procedures:
 */

/* Update current UL metric user RB taking in account the given allocation */
void ul_metric_ran::update_allocation(ul_harq_proc::ul_alloc_t alloc)
{
  uint32_t n;

  /* Does not fit */
  if (alloc.L > available_rb) {
    return; 
  }

  /* Not enough resources */
  if (alloc.RB_start + alloc.L > nof_rb) {
    return; 
  }

  for (n = alloc.RB_start; n < alloc.RB_start + alloc.L; n++) {
    used_rb[n] = true;
  }

  available_rb -= alloc.L; 
}

/* Prepare for a new UL TTI computation */
void ul_metric_ran::new_tti(
  std::map<uint16_t,sched_ue> & ue_db, 
  uint32_t                      nof_rb_, 
  uint32_t                      tti)
{
  std::map<uint16_t, sched_ue>::iterator iter;
  sched_ue * user;

  current_tti  = tti;
  nof_rb       = nof_rb_;
  available_rb = nof_rb_;

  bzero(used_rb, nof_rb * sizeof(bool));

  nof_users_with_data = 0;

  /* Assign to users with re-tx or data an index */
  for(iter = ue_db.begin(); iter != ue_db.end(); ++iter) {
    user = (sched_ue *)&iter->second;

    if (user->get_pending_ul_new_data(current_tti) ||
      !user->get_ul_harq(current_tti)->is_empty(0)) {

      user->ue_idx = nof_users_with_data;
      nof_users_with_data++;
    }
  }
}

/* Prepare an UL HARQ slot for the current user */
ul_harq_proc * ul_metric_ran::get_user_allocation(sched_ue * user)
{
  uint32_t       pending_data = user->get_pending_ul_new_data(current_tti); 
  ul_harq_proc * h            = user->get_ul_harq(current_tti);

  ul_harq_proc::ul_alloc_t alloc;

  uint32_t       pending_rb;

  /* Using the previously assigned index, perform round-robin strategy based on
   * the TTI.
   */
  if (pending_data || !h->is_empty(0)) {
    if (nof_users_with_data) {
      if ((current_tti % nof_users_with_data) != user->ue_idx) {
        return NULL; 
      }    
    }    
  }

  /* Is there any HARQ slot still pending to re-tx? */
  if (!h->is_empty(0)) {
    alloc = h->get_alloc();
    
    /* Is it possible to use the same mask? */
    if (allocation_is_valid(alloc)) {
      update_allocation(alloc);

      return h;
    }
    
    /* Find out a new way to organize the data */
    if (new_allocation(alloc.L, &alloc)) {
      update_allocation(alloc);
      h->set_alloc(alloc);

      return h;
    }
  /* No pending re-tx, go for the new data */
  } else {  
    if (pending_data) {
      pending_rb = user->get_required_prb_ul(pending_data);

      new_allocation(pending_rb, &alloc);

      if (alloc.L) {
        update_allocation(alloc);
        h->set_alloc(alloc);

        return h;
      }
    }
  }
  return NULL; 
}

} /* namespace srsenb */
