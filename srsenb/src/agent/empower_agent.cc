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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <emage/emage.h>
#include <emage/emproto.h>

#include "srslte/srslte.h"
#include "srslte/asn1/liblte_rrc.h"

#include "srsenb/hdr/enb.h"
#include "srsenb/hdr/agent/empower_agent.h"

#define EMPOWER_AGENT_BUF_SMALL_SIZE                2048

#define Error(fmt, ...)                             \
  do {                                              \
    m_logger->error("AGENT: "fmt, ##__VA_ARGS__);   \
  } while(0)

#define Warning(fmt, ...)                           \
  do {                                              \
    m_logger->warning("AGENT: "fmt, ##__VA_ARGS__); \
  } while(0)

#define Info(fmt, ...)                              \
  do {                                              \
    m_logger->info("AGENT: "fmt, ##__VA_ARGS__);    \
  } while(0)

#define Debug(fmt, ...)                             \
  do {                                              \
    m_logger->debug("AGENT: "fmt, ##__VA_ARGS__);   \
  } while(0)

#define Lock(x)                                     \
  do {                                              \
    pthread_spin_lock(x);                           \
  } while(0)

#define Unlock(x)                                   \
  do {                                              \
    pthread_spin_unlock(x);                         \
  } while(0)

/******************************************************************************
 *                                                                            *
 *                              Generic routines                              *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    time_diff
 * 
 * Abstract:
 *    Performs the difference between two 'timespec' structures. The result is
 *    translated to milliseconds.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - a, first timespec structure
 *    - b, second timespec structure
 * 
 * Returns:
 *    The difference of the operation 'b-a', translated from nanoseconds to
 *    milliseconds.
 */
#define time_diff(a, b)               \
  (((b.tv_sec - a.tv_sec) * 1000) +   \
  ((b.tv_nsec - a.tv_nsec) / 1000000))

namespace srsenb {
  
/******************************************************************************
 *                                                                            *
 *                              Agent callbacks                               *
 *                                                                            *
 ******************************************************************************/

/* Variable:
 *    em_agent
 * 
 * Abstract:
 *    Provides static pointer to singleton instance of the Agent. Using more
 *    than one agent in srsLTE is currently not supported and makes no sense.
 *    This pointer is initialized during 'empower_agent' initialization stage.
 * 
 *    This element is protected by static 'em_agent_lock', which is used only
 *    for this purpose.
 * 
 * Assumptions:
 *    Initialized once.
 */
static empower_agent * em_agent = 0;
pthread_mutex_t        em_agent_lock = PTHREAD_MUTEX_INITIALIZER;

/* Routine:
 *    ea_disconnected
 * 
 * Abstract:
 *    Performs operations that must be executed in case of disconnection with
 *    the controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_disconnected()
{
	return em_agent->reset();
}

/* Routine:
 *    ea_enb_setup
 * 
 * Abstract:
 *    Performs operations that must be executed in case of eNB setup request 
 *    from the controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which requested the report
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_enb_setup(uint32_t mod)
{
  char         buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  ep_enb_det   enbd;
  int          blen;
  all_args_t * args  = enb::get_instance()->get_args();

  enbd.cells[0].feat      = 
    EP_CCAP_UE_REPORT | EP_CCAP_UE_MEASURE | EP_CCAP_CELL_MEASURE;

  enbd.cells[0].pci       = (uint16_t)args->enb.pci;
  enbd.cells[0].DL_earfcn = (uint16_t)args->rf.dl_earfcn;
  enbd.cells[0].UL_earfcn = (uint16_t)args->rf.ul_earfcn;
  enbd.cells[0].DL_prbs   = (uint8_t) args->enb.n_prb;
  enbd.cells[0].UL_prbs   = (uint8_t) args->enb.n_prb;
  enbd.cells[0].max_ues   = 2;

  enbd.nof_cells          = 1;

#ifdef HAVE_RAN_SLICER
  enbd.ran[0].pci                = (uint16_t)args->enb.pci;
  enbd.ran[0].l1_mask            = 0;
  enbd.ran[0].l2_mask            = EP_RAN_LAYER2_CAP_RBG_SLICING;
  enbd.ran[0].l3_mask            = 0;
  enbd.ran[0].l2.mac.slice_sched = em_agent->get_ran()->get_slice_sched();
  enbd.ran[0].max_slices         = 8;

  enbd.nof_ran                   = 1;
#endif

  blen = epf_single_ecap_rep(
    buf, EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    0, // Response coming from eNB, and not from a cell in particular
    mod,
    &enbd);

  if(blen < 0) {
    return -1;
  }

  return em_send(em_agent->get_id(), buf, blen);
}

/* Routine:
 *    ea_ue_measure
 * 
 * Abstract:
 *    Performs operations that must be executed in case of UE measurement from
 *    the controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which requested the report
 *    - trig_id, ID of the assigned trigger
 *    - measure_id, ID of the measurement
 *    - rnti, target UE which should perform the measurements
 *    - earfcn, frequency where to operate the measurement
 *    - interval, interval of the measurement in ms
 *    - max_cells, maximum amount of cell to consider
 *    - max_meas, maximum amount of measurements to send back
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_ue_measure(
  uint32_t mod,
  int      trig_id,
  uint8_t  measure_id,
  uint16_t rnti,
  uint16_t earfcn,
  uint16_t interval,
  int16_t  max_cells,
  int16_t  max_meas)
{
  if(em_agent->setup_UE_period_meas(
    measure_id, trig_id, rnti, mod, earfcn, max_cells, max_meas, interval)) 
  {
    /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * TODO:
     * Due the inability of the controller to handle errors, error reporting
     * here is suppressed. This NEEDS to be changed, but we need controller
     * support first!
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */
    return 0;
  }

  return 0;
}

/* Routine:
 *    ea_mac_report
 * 
 * Abstract:
 *    Performs operations that must be executed in case of MAC report from
 *    the controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which requested the report
 *    - interval, interval of the measurement in ms
 *    - trig_id, ID of the assigned trigger
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_cell_measure(
  uint16_t cell_id,
  uint32_t mod,
  int32_t  interval,
  int      trig_id)
{
  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   * TODO:
   * Due the inability of the controller to handle errors, error reporting
   * here is suppressed. This NEEDS to be changed, but we need controller
   * support first!
   * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   */
  return em_agent->setup_cell_measurement(cell_id, mod, interval, trig_id);
}

/* Routine:
 *    ea_ue_report
 * 
 * Abstract:
 *    Performs operations that must be executed in case of UE report from the
 *    controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which requested the report
 *    - trig_id, ID of the assigned trigger
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_ue_report(uint32_t mod, int trig_id)
{
  /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   * TODO:
   * Due the inability of the controller to handle errors, error reporting
   * here is suppressed. This NEEDS to be changed, but we need controller
   * support first!
   * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   */
  return em_agent->setup_UE_report(mod, trig_id);
}

#ifdef HAVE_RAN_SLICER

/* Routine:
 *    slice_feedback
 * 
 * Abstract:
 *    Send a complete report of all the slices currently registered in the RAN
 *    subsystem, and about user associated with them.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which will receive the report
 * 
 * Returns:
 *    ---
 */
static void slice_feedback(uint32_t mod)
{ 
  char             buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int              blen;
  all_args_t *     args = enb::get_instance()->get_args();

  int              i;
  uint64_t         slices[32];
  uint16_t         nof_slices;
  
  ep_ran_slice_det det;
  ran_interface_common::slice_args slice_inf;

  // User memory allocated for det, this way we directly save them there
  slice_inf.users     = det.users;

  nof_slices = em_agent->get_ran()->get_slices(32, slices);

  if(nof_slices > 0) {
    for(i = 0; i < nof_slices; i++) {
      /* Do not report the default slice */
      if(slices[i] == RAN_DEFAULT_SLICE) {
        continue;
      }

      slice_inf.nof_users = EP_RAN_USERS_MAX;

      if(em_agent->get_ran()->get_slice_info(slices[i], &slice_inf)) {
        continue;
      }

      // Update values that will be sent to controller 
      det.l2.usched = slice_inf.l2.mac.user_sched;
      det.l2.rbgs   = slice_inf.l2.mac.rbg;
      det.nof_users = slice_inf.nof_users;

      blen = epf_single_ran_slice_rep(
        buf, 
        EMPOWER_AGENT_BUF_SMALL_SIZE,
        em_agent->get_id(),
        (uint16_t)args->enb.pci,
        mod,
        slices[i],
        &det);

      if(blen > 0) {
        em_send(em_agent->get_id(), buf, blen);
      }
    }
  }
}

#if 0
/* Routine:
 *    ea_ran_setup_request
 * 
 * Abstract:
 *    Performs operations that must be executed in case of RAN setup from the
 *    controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which will receive the report
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_ran_setup_request(uint32_t mod)
{
  char         buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int          blen;
  ep_ran_det   det;
  all_args_t * args  = enb::get_instance()->get_args();

  det.l1_mask = 0;
  // We can perform PRB slicing at MAC layer
  det.l2_mask = EP_RAN_LAYER2_CAP_PRB_SLICING;
  det.l3_mask = 0;
  // This should retrieved in a way like em_agent->ran_get_slice_id()
  det.l2.mac.slice_sched = 1;

  blen = epf_single_ran_setup_rep(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod,
    &det);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }

  return 0;
}
#endif
/* Routine:
 *    ea_slice_request
 * 
 * Abstract:
 *    Performs operations that must be executed in case of RAN slice from the
 *    controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which will receive the report
 *    - slice, Id of the slice requested
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_slice_request(uint32_t mod, uint64_t slice)
{
  char             buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int              blen;
  all_args_t *     args  = enb::get_instance()->get_args();
  
  uint16_t         i;
  uint64_t         slices[32];
  uint16_t         nof_slices;

  ep_ran_slice_det det;
  ran_interface_common::slice_args slice_inf;

  em_agent->setup_RAN_report(mod);

  // Request all the slices setup
  if(slice == 0) {
    // Straight send the slices statuses, regardless of the ID requested
    em_agent->m_RAN_def_dirty = 1;

    return 0;
  }

  // Request a particular slice which is not the default one
  if(slice != RAN_DEFAULT_SLICE) {
    det.nof_users = 16;
    
    // User memory allocated for det, this way we directly save them there
    slice_inf.users     = det.users;
    slice_inf.nof_users = det.nof_users;

    em_agent->get_ran()->get_slice_info(slice, &slice_inf);
    
    // Update values that will be sent to controller 
    det.l2.usched = slice_inf.l2.mac.user_sched;
    det.l2.rbgs   = slice_inf.l2.mac.rbg;
    det.nof_users = slice_inf.nof_users;

    blen = epf_single_ran_slice_rep(
      buf, 
      EMPOWER_AGENT_BUF_SMALL_SIZE,
      em_agent->get_id(),
      (uint16_t)args->enb.pci,
      mod,
      slice,
      &det);

    if(blen > 0) {
      em_send(em_agent->get_id(), buf, blen);
    }
  }

  return 0;
}

/* Routine:
 *    ea_slice_add
 * 
 * Abstract:
 *    Performs operations that must be executed in case of RAN slice addition
 *    from the controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which will receive the report
 *    - slice, ID of the slice
 *    - conf, slice configuration
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
int ea_slice_add(uint32_t mod, uint64_t slice, em_RAN_conf * conf)
{
  char               buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int                blen;
  int                i;
  uint16_t           usr[32] = { 0 };
  all_args_t *       args    = enb::get_instance()->get_args();

  ep_ran_slice_det   sdet;

  ran_interface_common::slice_args slice_inf;

  slice_inf.l2.mac.user_sched = conf->l2.user_sched;
  slice_inf.l2.mac.rbg        = conf->l2.rbg;
  slice_inf.l2.mac.time       = 1; // 1 subframe decisions
  slice_inf.nof_users         = 0;

  for(i = 0; i < conf->nof_users && i < 32; i++) {
    usr[i] = conf->users[i];
    slice_inf.nof_users++;
  }
  
  slice_inf.users     = usr;

  // PLMN is used in the slice ID for this moment
  if(em_agent->get_ran()->add_slice(slice, ((slice >> 32) & 0x00ffffff))) {
    /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * TODO:
     * Due the inability of the controller to handle errors, error reporting
     * here is suppressed. This NEEDS to be changed, but we need controller
     * support first!
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */
    return 0;
  }
  // Set the slice with it's starting configuration
  if(em_agent->get_ran()->set_slice(slice, &slice_inf)) {
    /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * TODO:
     * Due the inability of the controller to handle errors, error reporting
     * here is suppressed. This NEEDS to be changed, but we need controller
     * support first!
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */
    return 0;
  }

  em_agent->m_RAN_def_dirty = 1;

  return 0;
}

/* Routine:
 *    ea_slice_rem
 * 
 * Abstract:
 *    Performs operations that must be executed in case of RAN slice removal
 *    from the controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which will receive the report
 *    - slice, ID of the slice
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_slice_rem(uint32_t mod, uint64_t slice) 
{
  char               buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int                blen;

  em_agent->get_ran()->rem_slice(slice);
  em_agent->m_RAN_def_dirty = 1;

  return 0;
}

/* Routine:
 *    ea_slice_conf
 * 
 * Abstract:
 *    Performs operations that must be executed in case of RAN slice
 *    configuration from the controller.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which will receive the report
 *    - slice, ID of the slice
 *    - conf, new configuration for the slice
 * 
 * Returns:
 *    See emage.h for return value behavior
 */
static int ea_slice_conf(uint32_t mod, uint64_t slice, em_RAN_conf * conf)
{
  char               buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int                blen;
  int                i;
  uint16_t           usr[32] = { 0 };
  all_args_t *       args    = enb::get_instance()->get_args();

  ran_interface_common::slice_args slice_inf;

  slice_inf.l2.mac.user_sched = (uint32_t)conf->l2.user_sched;
  slice_inf.l2.mac.rbg        = (uint16_t)conf->l2.rbg;
  slice_inf.l2.mac.time       = 1;  // 1 subframe decisions

  for(i = 0; i < conf->nof_users; i++) {
    usr[i] = conf->users[i];
  }
  
  slice_inf.users     = usr;
  slice_inf.nof_users = (uint32_t)conf->nof_users;

  // This is getting ridiculous... set for doing everything...
  em_agent->get_ran()->add_slice(slice, ((slice >> 32) & 0x00ffffff));

  if(em_agent->get_ran()->set_slice(slice, &slice_inf)) {
    /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * TODO:
     * Due the inability of the controller to handle errors, error reporting
     * here is suppressed. This NEEDS to be changed, but we need controller
     * support first!
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */
    return 0;
  }

  em_agent->m_RAN_def_dirty = 1;

  return 0;
}

#endif // HAVE_RAN_SLICER

/* Variable:
 *    empower_agent_ops
 * 
 * Abstract:
 *    Provides callback of how this agent implementation reacts to events 
 *    incoming from the controller.
 * 
 * Assumptions:
 *    ---
 */
static struct em_agent_ops empower_agent_ops = {
  0,                      /* init */
  0,                      /* release */
  ea_disconnected,        /* disconnected */
  ea_enb_setup,           /* enb_setup_request */
  ea_ue_report,           /* ue_report */
  ea_ue_measure,          /* UE measurement */
  0,                      /* handover_UE */
  ea_cell_measure,        /* cell_measure */
  {
#ifdef HAVE_RAN_SLICER
    //ea_ran_setup_request, /* ran.setup_request */
    0,
    ea_slice_request,     /* ran.slice_request */
    ea_slice_add,         /* ran.slice_add */
    ea_slice_rem,         /* ran.slice_rem */
    ea_slice_conf         /* ran.slice_conf */
#else // HAVE_RAN_SLICER
    0,                    /* ran.setup_request */
    0,                    /* ran.slice_request */
    0,                    /* ran.slice_add */
    0,                    /* ran.slice_rem */
    0                     /* ran.slice_conf */
#endif // HAVE_RAN_SLICER
  }
};

/******************************************************************************
 *                                                                            *
 *                            Empower PRB reports                             *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    em_prb_report::em_prb_report
 * 
 * Abstract:
 *    Initializes a class instance.
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
em_prb_report::em_prb_report()
{
  m_module_id    = 0;
  m_trigger_id   = -1;
  m_interval     = 1000;
  m_DL           = 0;
  m_UL           = 0;
  m_last.tv_sec  = 0;
  m_last.tv_nsec = 0;
}

/* Routine:
 *    em_prb_report::~em_prb_report
 * 
 * Abstract:
 *    Releases a class instance resources.
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
em_prb_report::~em_prb_report()
{
  // Nothing
}

/* Routine:
 *    em_prb_report::reset
 * 
 * Abstract:
 *    Releases a class instance resources.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int em_prb_report::reset()
{
  m_module_id    = 0;
  m_trigger_id   = -1;
  m_interval     = 1000;
  m_DL           = 0;
  m_UL           = 0;
  m_last.tv_sec  = 0;
  m_last.tv_nsec = 0;

  return 0;
}

/******************************************************************************
 *                                                                            *
 *                            Empower MAC context                             *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    em_mac::em_mac
 * 
 * Abstract:
 *    Initializes a class instance.
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
em_mac::em_mac()
{
  // Nothing
}

/* Routine:
 *    em_mac::~em_mac
 * 
 * Abstract:
 *    Releases a class instance resources.
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
em_mac::~em_mac()
{
  // Nothing
}

/* Routine:
 *    em_mac::reset
 * 
 * Abstract:
 *    Releases a class instance resources.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int em_mac::reset()
{
  return m_prb_ctx.reset();
}

/******************************************************************************
 *                                                                            *
 *                           Empower Cell context                             *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    em_cell::em_cell
 * 
 * Abstract:
 *    Initializes a class instance.
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
em_cell::em_cell()
{
  m_pci = 0xffff;
}

/* Routine:
 *    em_cell::~em_cell
 * 
 * Abstract:
 *    Releases a class instance resources.
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
em_cell::~em_cell()
{
  // Nothing
}

/* Routine:
 *    em_cell::reset
 * 
 * Abstract:
 *    Releases a class instance resources.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int em_cell::reset()
{
  return m_mac.reset();
}

/******************************************************************************
 *                                                                            *
 *                            Agent UE procedures                             *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    em_ue::em_ue
 * 
 * Abstract:
 *    Performs initialization of an em_ue class instance. The class is used
 *    internally in 'empower_agent' context and should not be exposed.
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
em_ue::em_ue()
{
  m_state        = 0;
  m_state_dirty  = 0;

  m_imsi         = 0;
  m_plmn         = 0;
  m_tmsi         = 0;
  m_id_dirty     = 0;

  m_next_meas_id = 1;
  m_next_obj_id  = 1;
  m_next_rep_id  = 1;

  memset(m_meas,   0, sizeof(ue_meas) * EMPOWER_AGENT_MAX_MEAS);
}

em_ue::~em_ue()
{
  // Nothing
}

/******************************************************************************
 *                                                                            *
 *                                Agent class                                 *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    empower_agent::empower_agent
 * 
 * Abstract:
 *    Performs operation on a new instance of the agent.
 *    Mainly variable initialization.
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
empower_agent::empower_agent()
{
  m_id           = -1;
  m_state        = AGENT_STATE_STOPPED;

  m_rrc          = 0;
  m_ran          = 0;
  m_logger       = 0;

  m_args         = 0;

  m_uer_mod      = 0;
  m_uer_feat     = 0;
  m_uer_tr       = 0;

  m_ues_dirty    = 0;
  m_nof_ues      = 0;

  m_RAN_feat     = 0;
  m_RAN_def_dirty= 0;
  m_RAN_mod      = 0;

  m_thread       = 0;
}

/* Routine:
 *    empower_agent::~empower_agent
 * 
 * Abstract:
 *    Performs operation to release an instance of the agent
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
empower_agent::~empower_agent()
{
  release();
}

/* Routine:
 *    empower_agent::get_id
 * 
 * Abstract:
 *    Get this agent instance id.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    ID of the agent.
 */
unsigned int empower_agent::get_id()
{
  return m_id;
}

/* Routine:
 *    empower_agent::get_ran
 * 
 * Abstract:
 *    Get this agent RAN interface
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    A pointer to the RAN subsystem, otherwise a null pointer on errors
 */
ran_interface_common * empower_agent::get_ran()
{
  return m_ran;
}

/* Routine:
 *    empower_agent::init
 * 
 * Abstract:
 *    Initializes the agent instance and its subsystems. This call allow the 
 *    agent to become operational and response to local and network events.
 * 
 *    Initialization steps also fills static singleton variables.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    0 on success, otherwise a negative error code
 */
int empower_agent::init(
  int                    enb_id, 
  rrc_interface_agent *  rrc, 
  ran_interface_common * ran, 
  srslte::log *          logger)
{
  if(!rrc || !ran || !logger) {
    printf("ERROR: while initializing the agent; some arguments are NULL!\n");
    printf("    RRC=%p, RAN=%p, LOGGER=%p\n", rrc, ran, logger);

    return -1;
  }

  if(enb_id <= 0) {
    printf("ERROR: agent eNB_id is %d\n", enb_id);
    return -1;
  }

  m_id    = enb_id;
  m_rrc   = rrc;
  m_ran   = ran;
  m_logger= logger;
  m_args  = enb::get_instance()->get_args();

  pthread_spin_init(&m_lock, 0);

  // srs supports one cell only, so the valid cell is always at position 0
  m_cells[0].m_pci        = (uint16_t)((all_args_t *)m_args)->enb.pci;
  m_cells[0].m_mac.m_prbs = (int)((all_args_t *)m_args)->enb.n_prb;

  /* Done once, it updates the static instance of Agent that will be used by
   * the callback wrapper implementation.
   */
  pthread_mutex_lock(&em_agent_lock);
  if(!em_agent) {
    em_agent = this;
  }
  pthread_mutex_unlock(&em_agent_lock);

  /* Create a new thread; we don't use the given thread library since we don't
   * want RT capabilities for this thread, which will run with low priority.
   */
  pthread_create(&m_thread, 0, empower_agent::agent_loop, this);

  return 0;
}

/* Routine:
 *    empower_agent::release
 * 
 * Abstract:
 *    Releases resources that have been initialized during agent startup.
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
void empower_agent::release()
{
  /* Nothing right now; since all the resources are mainly used in the thread
   * context of the agent, the thread itself is in charge of releasing all at
   * termination.
   */
}

/* Routine:
 *    empower_agent::reset
 * 
 * Abstract:
 *    Resets the state machines and variables of the agent. This operation 
 *    happens usually after a disconnection event, to align the agent to a known
 *    state.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    ---
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int empower_agent::reset()
{
  int      i;
  uint16_t rnti;
  em_ue *  ue;

  std::map<uint16_t, em_ue *>::iterator it;

  Debug("Resetting the state of the Agent\n");
  Lock(&m_lock);

  // Reset any UE report
  m_uer_mod  = 0;
  m_uer_tr   = 0;
  m_uer_feat = 0;

  // Reset any UE RRC state
  //for(rnti = 0; rnti < 0xffff; rnti++) {
  for(it = m_ues.begin(); it != m_ues.end(); ++it) {
    ue = it->second;

    ue->m_id_dirty          = 1;
    ue->m_state_dirty       = 1;

    // Invalidate the measure
    for(i = 0; i < EMPOWER_AGENT_MAX_MEAS; i++) {
      ue->m_meas[i].id      = 0;
      ue->m_meas[i].mod_id  = 0;
      ue->m_meas[i].trig_id = 0;

      /* TODO:
        * What about the measurements that are ongoing in the cell phone? They
        * are not resetted now, so they will keep going. We can still intercept
        * them probably in the 'report_RRC_measure' call.
        * 
        * We should probably send an empty RRC reconfiguration to reset 
        * everything, but need to check the specs about that.
        */
    }

    ue->m_next_meas_id = 1;
    ue->m_next_obj_id  = 1;
    ue->m_next_rep_id  = 1;
  }

  // Reset the contexts of any cell registered in the eNB
  for(i = 0; i < MAX_CELLS; i++) {
    m_cells[i].reset();
  }

  Unlock(&m_lock);

  return 0;
}

/* Routine:
 *    empower_agent::setup_UE_report
 * 
 * Abstract:
 *    Setup the agent to handle UE reporting
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - mod_id, module ID to report to
 *    - trig_id, trigger id assigned to this operation
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int empower_agent::setup_UE_report(uint32_t mod_id, int trig_id)
{
  m_uer_mod  = mod_id;
  m_uer_tr   = trig_id;
  m_uer_feat = 1;
  m_ues_dirty= 1;

  Debug("UE report ready; reporting to module %d\n", mod_id);

  return 0;
}

/* Routine:
 *    empower_agent::setup_cell_measurement
 * 
 * Abstract:
 *    Setup the agent to handle cell measurements
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - cell_id, ID of the selected cell to measure
 *    - mod_id, module ID to report to
 *    - interval, interval in ms between the reports
 *    - trig_id, trigger id assigned to this operation
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int empower_agent::setup_cell_measurement(
  uint16_t cell_id, uint32_t mod_id, uint32_t interval, int trig_id)
{
  int          i;
  int          j;
  char         buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int          blen;

  ep_cell_rep  rep;

  if(trig_id > 0) {
    Error("Trigger MAC reports not supported right now!\n");
    return 0;
  }

  /* Enable the feature if it was not there; once enabled it will be persisting
   * right now. This must be modified in the future.
   */
  if(!m_cm_feat) {
    m_cm_feat = 1;
  }

  // Look for the right cell to report
  for(i = 0; i < MAX_CELLS; i++) {
    if(cell_id != m_cells[i].m_pci) {
      continue;
    }

    rep.prb.DL_prbs      = (uint8_t)m_cells[i].m_mac.m_prbs;
    rep.prb.DL_prbs_used = m_cells[i].m_mac.m_prb_ctx.m_DL;
    rep.prb.UL_prbs      = (uint8_t)m_cells[i].m_mac.m_prbs;
    rep.prb.UL_prbs_used = m_cells[i].m_mac.m_prb_ctx.m_UL;

    blen = epf_sched_cell_meas_rep(
      buf,
      EMPOWER_AGENT_BUF_SMALL_SIZE,
      m_id,
      m_cells[i].m_pci,
      mod_id,
      interval,
      &rep);

    if(blen < 0)
    {
      Error("Cannot format cell measurement message!\n");
      return 0;
    }

    em_send(m_id, buf, blen);
  }

  return 0;
}

/* Routine:
 *    empower_agent::setup_UE_period_meas
 * 
 * Abstract:
 *    Setup the agent to handle UE measurement reporting
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - mod, Module ID which requested the report
 *    - trig_id, ID of the assigned trigger
 *    - measure_id, ID of the measurement
 *    - rnti, target UE which should perform the measurements
 *    - earfcn, frequency where to operate the measurement
 *    - interval, interval of the measurement in ms
 *    - max_cells, maximum amount of cell to consider
 *    - max_meas, maximum amount of measurements to send back
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int empower_agent::setup_UE_period_meas(
  uint32_t id,
  int      trigger_id,
  uint16_t rnti,
  uint32_t mod_id,
  uint16_t freq,
  uint16_t max_cells,
  uint16_t max_meas,
  int      interval)
{
  int i;    // Index
  int j;    // Index
  int n = 0;// Index

  int bw;   // Bandwidth

  LIBLTE_RRC_MEAS_CONFIG_STRUCT                meas;
  LIBLTE_RRC_REPORT_INTERVAL_ENUM              rep_int;
  LIBLTE_RRC_MEAS_OBJECT_TO_ADD_MOD_STRUCT *   mobj;
  LIBLTE_RRC_REPORT_CONFIG_TO_ADD_MOD_STRUCT * mrep;
  LIBLTE_RRC_MEAS_ID_TO_ADD_MOD_STRUCT *       mid;

  all_args_t * args = (all_args_t *)m_args;

  /*
   * Setup agent mechanism:
   */
  if(m_ues.count(rnti) == 0) {
    Error("No %x RNTI known\n", rnti);
    return -1;
  }

  /* NOTES: The 'bw' indicates the maximum allowed measurement bandwidth to 
   * detect. Having a too permessive scan can consume lot of the UE resources,
   * but scanning only 'smaller' signals reduce the overall performances with 
   * time (UE will only see cells with fewer resources).
   */
  if(args->enb.n_prb) {
    bw = LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_MBW100;
  } else {
    switch(args->enb.n_prb) {
    case 75:
      bw = LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_MBW75;
      break;
    case 50:
      bw = LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_MBW50;
      break;
    case 25:
      bw = LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_MBW25;
      break;
    case 15:
      bw = LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_MBW15;
      break;
    default:
      bw = LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_MBW6;
      break;
    }

    /* This gives the opportunity to the controller to locate cells with larger
     * BW and order a handover to them, promoting the UE on a larger cell.
     */
    bw++;
  }

  Lock(&m_lock);

  for(i = 0; i < EMPOWER_AGENT_MAX_MEAS; i++) {
    if(m_ues[rnti]->m_meas[i].mod_id == 0) {
      break;
    }
  }

  Unlock(&m_lock);

  if(i == EMPOWER_AGENT_MAX_MEAS) {
    return -1;
  }

  m_ues[rnti]->m_meas[i].id          = id;
  m_ues[rnti]->m_meas[i].trig_id     = trigger_id;
  m_ues[rnti]->m_meas[i].mod_id      = mod_id;

  m_ues[rnti]->m_meas[i].interval    = interval;
  m_ues[rnti]->m_meas[i].freq        = freq;
  m_ues[rnti]->m_meas[i].carrier.pci = (uint16_t)args->enb.pci;

  m_ues[rnti]->m_meas[i].max_cells   =
    max_cells > EMPOWER_AGENT_MAX_CELL_MEAS ? 
      EMPOWER_AGENT_MAX_CELL_MEAS : 
      max_cells;

  m_ues[rnti]->m_meas[i].max_meas    =
    max_meas > EMPOWER_AGENT_MAX_MEAS ? 
      EMPOWER_AGENT_MAX_MEAS : 
      max_meas;

  m_ues[rnti]->m_meas[i].meas_id     = m_ues[rnti]->m_next_meas_id++;
  m_ues[rnti]->m_meas[i].obj_id      = m_ues[rnti]->m_next_obj_id++;
  m_ues[rnti]->m_meas[i].rep_id      = m_ues[rnti]->m_next_rep_id++;

  Debug("Setting up RRC measurement %d-->%d for RNTI %x\n",
    m_ues[rnti]->m_meas[i].id, m_ues[rnti]->m_meas[i].meas_id, rnti);

  /*
   * Prepare RRC request to send to the UE.
   * 
   * NOTE: This has probably to be setup every time we request a measure to the
   * UE, with the list of measurements to do. Probably old measurements are
   * discarded and only the last one maintained... need to test...
   */

  bzero(&meas, sizeof(LIBLTE_RRC_MEAS_CONFIG_STRUCT));

  /* Prepare the RRC configuration message with all the measurements that has
   * been set up.
   */ 
  for(j = 0; j < EMPOWER_AGENT_MAX_MEAS; j++) {
    // Skip if the measurement slot is not valid
    if(m_ues[rnti]->m_meas[j].mod_id == 0) {
      continue;
    }

    if(m_ues[rnti]->m_meas[j].interval <= 120) {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS120;
    } else if(
      m_ues[rnti]->m_meas[j].interval > 120 && 
      m_ues[rnti]->m_meas[j].interval <= 240) 
    {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS240;
    } else if(
      m_ues[rnti]->m_meas[j].interval > 240 && 
      m_ues[rnti]->m_meas[j].interval <= 480) 
    {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS480;
    } else if(
      m_ues[rnti]->m_meas[j].interval > 480 && 
      m_ues[rnti]->m_meas[j].interval <= 640) 
    {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS640;
    } else if(
      m_ues[rnti]->m_meas[j].interval > 640 && 
      m_ues[rnti]->m_meas[j].interval <= 1024) 
    {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS1024;
    } else if(
      m_ues[rnti]->m_meas[j].interval > 1024 && 
      m_ues[rnti]->m_meas[j].interval <= 2048) 
    {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS2048;
    } else if(
      m_ues[rnti]->m_meas[j].interval > 2048 && 
      m_ues[rnti]->m_meas[j].interval <= 5120) 
    {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS5120;
    } else {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS10240;
    }

    meas.meas_obj_to_add_mod_list_present = true;
    meas.rep_cnfg_to_add_mod_list_present = true;
    meas.meas_id_to_add_mod_list_present  = true;
    meas.quantity_cnfg_present            = false;
    meas.meas_gap_cnfg_present            = false;
    meas.s_meas_present                   = false;
    meas.pre_reg_info_hrpd_present        = false;
    meas.speed_state_params_present       = false;
    meas.N_meas_id_to_remove              = 0;
    meas.N_meas_obj_to_remove             = 0;
    meas.N_rep_cnfg_to_remove             = 0;

    // Prepare the measurement Object

    mobj = meas.meas_obj_to_add_mod_list.meas_obj_list + n;

    mobj->meas_obj_id   = m_ues[rnti]->m_meas[j].obj_id;
    mobj->meas_obj_type = LIBLTE_RRC_MEAS_OBJECT_TYPE_EUTRA;

    mobj->meas_obj_eutra.offset_freq_not_default            = false;
    mobj->meas_obj_eutra.presence_ant_port_1                = true;
    mobj->meas_obj_eutra.cells_to_remove_list_present       = false;
    mobj->meas_obj_eutra.black_cells_to_remove_list_present = false;
    mobj->meas_obj_eutra.cell_for_which_to_rep_cgi_present  = false;
    mobj->meas_obj_eutra.N_black_cells_to_add_mod           = 0;
    mobj->meas_obj_eutra.N_cells_to_add_mod                 = 0;

    // NOTE: This indicates the maximum allowed measurement bandwidth
    mobj->meas_obj_eutra.allowed_meas_bw = 
      (LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_ENUM)bw;
    mobj->meas_obj_eutra.offset_freq     = 
      LIBLTE_RRC_Q_OFFSET_RANGE_DB_0;
    mobj->meas_obj_eutra.carrier_freq    = m_ues[rnti]->m_meas[j].freq;

    meas.meas_obj_to_add_mod_list.N_meas_obj++; // One more object

    // Prepare the measurement Report

    mrep = meas.rep_cnfg_to_add_mod_list.rep_cnfg_list + n;

    mrep->rep_cnfg_id   = m_ues[rnti]->m_meas[j].rep_id;
    mrep->rep_cnfg_type = LIBLTE_RRC_REPORT_CONFIG_TYPE_EUTRA;

    mrep->rep_cnfg_eutra.trigger_type     = 
      LIBLTE_RRC_TRIGGER_TYPE_EUTRA_PERIODICAL;
    mrep->rep_cnfg_eutra.trigger_quantity = 
      LIBLTE_RRC_TRIGGER_QUANTITY_RSRQ;
    mrep->rep_cnfg_eutra.periodical.purpose = 
      LIBLTE_RRC_PURPOSE_EUTRA_REPORT_STRONGEST_CELL;
    mrep->rep_cnfg_eutra.report_amount    = 
      LIBLTE_RRC_REPORT_AMOUNT_INFINITY;
    mrep->rep_cnfg_eutra.report_quantity  = 
      LIBLTE_RRC_REPORT_QUANTITY_BOTH;
    mrep->rep_cnfg_eutra.report_interval  = rep_int;
    mrep->rep_cnfg_eutra.max_report_cells = m_ues[rnti]->m_meas[j].max_cells;

    meas.rep_cnfg_to_add_mod_list.N_rep_cnfg++; // One more report

    // Measurement IDs:

    mid = meas.meas_id_to_add_mod_list.meas_id_list + n;

    mid->meas_id     = m_ues[rnti]->m_meas[j].meas_id;
    mid->meas_obj_id = m_ues[rnti]->m_meas[j].obj_id;
    mid->rep_cnfg_id = m_ues[rnti]->m_meas[j].rep_id;
    
    meas.meas_id_to_add_mod_list.N_meas_id++; // One more ID

    n++; // Increment the index to access RRc meas. structures
  }

  Debug("Sending to %x a new RRC reconfiguration for %d measurement(s)\n",
    rnti, n);

  m_rrc->setup_ue_measurement(rnti, &meas);

  return 0;
}

/* Routine:
 *    empower_agent::setup_RAN_report
 * 
 * Abstract:
 *    Setup the agent to handle RAN reporting
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - mod_id, module ID to report to
 * 
 * Returns:
 *    0 on success, otherwise a negative error code.
 */
int empower_agent::setup_RAN_report(uint32_t mod)
{
  m_RAN_feat      = 1;
  m_RAN_def_dirty = 0;
  m_RAN_mod       = mod;

  return 0;
}

/******************************************************************************
 *                                                                            *
 *                         Agent interface for MAC                            *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    empower_agent::process_DL_results
 * 
 * Abstract:
 *    MAC layer has scheduled the Downwlink and is reporting the result to us.
 *    This procedure is quick and lightweight, or otherwise can impact on the
 *    scheduler performances.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - tti, Transmission Time Interval
 *    - sced_results, the results
 * 
 * Returns:
 *    ---
 */
void empower_agent::process_DL_results(
  uint32_t tti, sched_interface::dl_sched_res_t * sched_result)
{
  uint32_t     i;
  int          prbs = 0;

  // Immediately exit if no measurement of the DL has been setup
  if(!m_cm_feat) {
    return;
  }

  for(i = 0; i < sched_result->nof_bc_elems; i++) {
    prbs += prbs_from_dci(
      &sched_result->bc[i].dci, 1, m_cells[0].m_mac.m_prbs);
  }

  for(i = 0; i < sched_result->nof_rar_elems; i++) {
    prbs += prbs_from_dci(
      &sched_result->rar[i].dci, 1, m_cells[0].m_mac.m_prbs);
  }

  for(i = 0; i < sched_result->nof_data_elems; i++) {
    prbs += prbs_from_dci(
      &sched_result->data[i].dci, 1, m_cells[0].m_mac.m_prbs);
  }

  Lock(&m_lock);
  // Update the number of used resource blocks
  m_cells[0].m_mac.m_prb_ctx.m_DL += prbs;
  Unlock(&m_lock);
}

/* Routine:
 *    empower_agent::process_UL_results
 * 
 * Abstract:
 *    MAC layer has scheduled the Uplink and is reporting the result to us.
 *    This procedure is quick and lightweight, or otherwise can impact on the
 *    scheduler performances.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - tti, Transmission Time Interval
 *    - sced_results, the results
 * 
 * Returns:
 *    ---
 */
void empower_agent::process_UL_results(
  uint32_t tti, sched_interface::ul_sched_res_t * sched_result)
{
  uint32_t     i;
  int          prbs = 0;
  
  // Immediately exit if no measurement of the DL has been setup
  if(!m_cm_feat) {
    return;
  }

  for(i = 0; i < sched_result->nof_dci_elems; i++) {
    prbs += prbs_from_dci(
      &sched_result->pusch[i].dci, 0, m_cells[0].m_mac.m_prbs);
  }

  Lock(&m_lock);
  // Update the number of used resource blocks
  m_cells[0].m_mac.m_prb_ctx.m_UL += prbs;
  Unlock(&m_lock);
}

/******************************************************************************
 *                                                                            *
 *                         Agent interface for RRC                            *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    empower_agent::add_user
 * 
 * Abstract:
 *    RRC layer reporting that Radio Resources for a new user have been
 *    allocated, and thus we have to consider it too. This operations should be
 *    performed in a quick way, since the execution context is the PRACH one.
 *    This procedure does not mark the user to be reported to the controller.
 * 
 * Assumptions:
 *    Lock hold by the caller.
 * 
 * Arguments:
 *    - rnti, ID of the user to add
 * 
 * Returns:
 *    ---
 */
void empower_agent::add_user(uint16_t rnti)
{
  std::map<uint16_t, em_ue *>::iterator it;
  all_args_t * args = (all_args_t *)m_args;

  // Locking disabled since the procedure should be hold by the caller
  //Lock(&m_lock);

  it = m_ues.find(rnti);

  if(it == m_ues.end()) {
    m_ues.insert(std::make_pair(rnti, new em_ue()));
    m_nof_ues++;

    m_ues[rnti]->m_plmn  = (args->enb.s1ap.mcc & 0x0fff) << 12;
    m_ues[rnti]->m_plmn |= (args->enb.s1ap.mnc & 0x0fff);

    m_ues[rnti]->m_state = UE_STATUS_CONNECTED;

    m_ues[rnti]->m_next_meas_id = 1;
    m_ues[rnti]->m_next_obj_id  = 1;
    m_ues[rnti]->m_next_rep_id  = 1;

    /* Clean up measurements */
    memset(
      m_ues[rnti]->m_meas, 
      0, 
      sizeof(em_ue::ue_meas) * EMPOWER_AGENT_MAX_MEAS);

    if(m_uer_feat) {
      m_ues_dirty = 1;
    }

#ifdef HAVE_RAN_SLICER
    // Creation of user triggers modification at RAN layer for the agent
    m_RAN_def_dirty = 1;
#endif

    Debug("Added user %x (PLMN:%x)\n", rnti, m_ues[rnti]->m_plmn);
  }

  // Locking disabled since the procedure should be hold by the caller
  //Unlock(&m_lock);
}

/* Routine:
 *    empower_agent::rem_user
 * 
 * Abstract:
 *    RRC layer reporting that Radio Resources for an user will be removed from
 *    the eNB stack. 
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, ID of the user to remove
 * 
 * Returns:
 *    ---
 */
void empower_agent::rem_user(uint16_t rnti)
{
  em_ue * ue;
  std::map<uint16_t, em_ue *>::iterator it;

  Lock(&m_lock);

  it = m_ues.find(rnti);

  if(it != m_ues.end()) {
    Debug("Removing user %x\n", rnti);

    m_ues[rnti]->m_state       = UE_STATUS_DISCONNECTED;
    m_ues[rnti]->m_state_dirty = 1; // Mark as to update

    if(m_uer_feat) {
      m_ues_dirty = 1;
    }

#ifdef HAVE_RAN_SLICER
    m_RAN_def_dirty = 1;
#endif
  }

  Unlock(&m_lock);
}

/* Routine:
 *    empower_agent::update_user_ID
 * 
 * Abstract:
 *    RRC layer report an update in the UE identity through additional checks
 *    performed during message exchange with the Core Network.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, ID of the user to update
 *    - plmn, PLMN ID of the user
 *    - imsi, Subscriber identity of the user
 *    - tmsi, Temporary identity of the user
 * 
 * Returns:
 *    ---
 */
void empower_agent::update_user_ID(
    uint16_t rnti, uint32_t plmn, uint64_t imsi, uint32_t tmsi) 
{
  em_ue * ue;
  std::map<uint16_t, em_ue *>::iterator it;

  Lock(&m_lock);

  // First attempt, check if the user is already there
  it = m_ues.find(rnti);

  if(it == m_ues.end()) {
    add_user(rnti);

    // Second attempt, the user should be added by now
    it = m_ues.find(rnti);
  }

  if(it != m_ues.end()) {
    Debug("Updating user %x identity\n", rnti);

    if(plmn) {
      m_ues[rnti]->m_plmn = plmn;
    }
    
    if(imsi) {
      m_ues[rnti]->m_imsi = imsi;
    }
    
    if(tmsi) {
      m_ues[rnti]->m_tmsi = tmsi;
    }
  }

  for(it = m_ues.begin(); it != m_ues.end(); ++it) {
    ue = it->second;

    /* UE renewed its RNTI, but subscription info are still the same */
    if(imsi != 0 && ue->m_imsi == imsi && it->first != rnti) {
      /* Reset personal fields to avoid controller problems */
      ue->m_imsi = 0;
      ue->m_tmsi = 0;
    }

    /* UE renewed its RNTI, but temporary info are still the same */
    if(tmsi != 0 && ue->m_tmsi == tmsi && it->first != rnti) {
      ue->m_tmsi = 0;
    }
  }

  Unlock(&m_lock);
}

/* Routine:
 *    empower_agent::update_user_ID
 * 
 * Abstract:
 *    RRC layer informs me to report this user to the management layer
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, ID of the user to report
 * 
 * Returns:
 *    ---
 */
void empower_agent::report_user(uint16_t rnti) 
{
  em_ue * ue;
  std::map<uint16_t, em_ue *>::iterator it;

  Lock(&m_lock);

  // Check if the user exists
  it = m_ues.find(rnti);

  if(it != m_ues.end()) {
    m_ues[rnti]->m_state = UE_STATUS_CONNECTED;
    m_ues[rnti]->m_state_dirty = 1;
    m_ues[rnti]->m_id_dirty = 1;

    if(m_uer_feat) {
      m_ues_dirty = 1;
    }
    
#ifdef HAVE_RAN_SLICER
    m_RAN_def_dirty = 1;
#endif
  }

  Unlock(&m_lock);
}

/* Routine:
 *    empower_agent::report_RRC_measure
 * 
 * Abstract:
 *    RRC layer reporting that a measurement has been collected from an UE.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - rnti, ID of the UE reporting
 *    - report, information about UE measurements reported
 * 
 * Returns:
 *    ---
 */
void empower_agent::report_RRC_measure(
  uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report)
{
  int i;
  int j;
  int nof_cells = 0;

  LIBLTE_RRC_MEAS_RESULT_EUTRA_STRUCT * cells;

  if (report->have_meas_result_neigh_cells && 
    report->meas_result_neigh_cells_choice == 
      LIBLTE_RRC_MEAS_RESULT_LIST_EUTRA)
  {
    nof_cells = report->meas_result_neigh_cells.eutra.n_result;
  }

  for(i = 0; i < EMPOWER_AGENT_MAX_MEAS; i++) {
    if(m_ues[rnti]->m_meas[i].meas_id == report->meas_id) {
      break;
    }
  }

  // NOTE: Should we try to revoke the measure if is not managed?
  if(i == EMPOWER_AGENT_MAX_MEAS) {
    Error("Measure %d of RNTI %x not found! Index=%d\n",
      report->meas_id, rnti, i);

    return;
  }

  if(m_ues.count(rnti) != 0) {
    Debug("Received RRC measure %d from user %x\n",
      m_ues[rnti]->m_meas[i].id, rnti);

    m_ues[rnti]->m_meas[i].carrier.rsrp = report->pcell_rsrp_result;
    m_ues[rnti]->m_meas[i].carrier.rsrq = report->pcell_rsrq_result;
    m_ues[rnti]->m_meas[i].c_dirty      = 1;

    if(nof_cells == 0) {
      return;
    }

    cells = report->meas_result_neigh_cells.eutra.result_eutra_list;

    for(j = 0; j < nof_cells && j < EMPOWER_AGENT_MAX_CELL_MEAS; j++) {
      m_ues[rnti]->m_meas[i].neigh[j].pci  = cells[j].phys_cell_id;
      m_ues[rnti]->m_meas[i].neigh[j].rsrp = cells[j].meas_result.rsrp_result;
      m_ues[rnti]->m_meas[i].neigh[j].rsrq = cells[j].meas_result.rsrq_result;
      m_ues[rnti]->m_meas[i].neigh[j].dirty= 1;
    }
  }
}

/******************************************************************************
 *                                                                            *
 *                    Agent interaction with controller                       *
 *                                                                            *
 ******************************************************************************/
#if 0
/* Routine:
 *    empower_agent::send_MAC_report
 * 
 * Abstract:
 *    Send a MAC report message to the controller.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - mod_id, ID of the target module
 *    - det, MAC info to send
 * 
 * Returns:
 *    ---
 */
void empower_agent::send_MAC_report(uint32_t mod_id, ep_macrep_det * det)
{
  int i;
  int          size;
  char         buf[EMPOWER_AGENT_BUF_SMALL_SIZE];

  all_args_t * args = (all_args_t *)m_args;

  size = epf_trigger_macrep_rep(
    buf,
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    m_id,
    args->enb.pci,
    mod_id,
    det);

  if(size <= 0) {
    Error("Cannot format MAC report reply, error %d\n", size);
    return;
  }

  Debug("Sending MAC report\n");

  em_send(m_id, buf, size);
}
#endif
/* Routine:
 *    empower_agent::send_UE_report
 * 
 * Abstract:
 *    Send an UE report message to the controller.
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
void empower_agent::send_UE_report(void)
{
  std::map<uint16_t, em_ue *>::iterator it;
  
  em_ue *       ue;
  int           i = 0;
  int           r; /* will be reported? */
  char          buf[EMPOWER_AGENT_BUF_SMALL_SIZE];
  int           size;
  ep_ue_details ued[16];
  int           uel = 16;

  all_args_t * args = (all_args_t *)m_args;

  memset(ued, 0, sizeof(ep_ue_details));

  Lock(&m_lock);

  for(it = m_ues.begin(); i < uel && it !=  m_ues.end(); /* Nothing */) {
    ue = it->second;
    r  = 0;

    // State first; if the UE disconnects the identity is not interesting
    if(ue->m_id_dirty || ue->m_state_dirty) {
      ued[i].rnti  = it->first;
      ued[i].rnti  = it->first;
      ued[i].plmn  = ue->m_plmn;
      ued[i].imsi  = ue->m_imsi;
      ued[i].tmsi  = ue->m_tmsi;
      ued[i].state = ue->m_state;

      ue->m_state_dirty = 0;
      ue->m_id_dirty    = 0;

      r = 1;

      // We are reporting the UE going offline
      if(ue->m_state == UE_STATUS_DISCONNECTED) {
        m_ues.erase(it++); // Increment iterator
        m_nof_ues--;

        delete ue;// Remove allocated UE descriptor

        i++;      // Next reporting slot
        continue; // Next UE to report
      }
    }

    if(r) {
      i++;// Next reporting slot
    }

    ++it; // Increment iterator
  }

  Unlock(&m_lock);

  if(i == uel) {
    Warning("Too much UEs to report; current limit set to %d\n", uel);
  }

  size = epf_trigger_uerep_rep(
    buf,
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    m_id,
    (uint16_t)args->enb.pci,
    m_uer_mod,
    i,
    EMPOWER_AGENT_MAX_UE,
    ued);

  if(size < 0) {
    Error("Cannot format UE report reply\n");
    return;
  }

  em_send(m_id, buf, size);
}

/* Routine:
 *    empower_agent::send_UE_meas
 * 
 * Abstract:
 *    Send an UE measurement report message to the controller.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - m, measurements to send
 * 
 * Returns:
 *    ---
 */
void empower_agent::send_UE_meas(em_ue::ue_meas * m)
{
  int           i;
  int           j;
  char          buf[EMPOWER_AGENT_BUF_SMALL_SIZE];
  int           size;

  all_args_t * args = (all_args_t *)m_args;
  ep_ue_report epr;

  memset(&epr, 0, sizeof(epr));

  // Fill in the carrier first
  epr.rrc[0].meas_id = m->id;
  epr.rrc[0].pci     = m->carrier.pci;
  epr.rrc[0].rsrp    = m->carrier.rsrp;
  epr.rrc[0].rsrq    = m->carrier.rsrq;

  epr.nof_rrc++;

  for(
    i = 0, j = 1; 
    i < EMPOWER_AGENT_MAX_CELL_MEAS && j < EP_UE_RRC_MEAS_MAX; 
    i++) 
  {
    // Fill in any other dirty measurement
    if(m->neigh[i].dirty) {
      epr.rrc[j].meas_id = m->id;
      epr.rrc[j].pci     = m->neigh[i].pci;
      epr.rrc[j].rsrp    = m->neigh[i].rsrp;
      epr.rrc[j].rsrq    = m->neigh[i].rsrq;

      m->neigh[i].dirty  = 0;

      j++;
    }
  }

  size = epf_trigger_uemeas_rep(
    buf,
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    m_id,
    (uint16_t)args->enb.pci,
    m->mod_id,
    &epr);

  if(size < 0) {
    Error("Cannot format UE measurement reply\n");
    return;
  }

  em_send(m_id, buf, size);
}

#ifdef HAVE_RAN_SLICER

/* Routine:
 *    empower_agent::slice_feedback
 * 
 * Abstract:
 *    Send a complete report of all the slices currently registered in the RAN
 *    subsystem, and about user associated with them.
 * 
 * Assumptions:
 *    'em_agent' pointer is valid. No atomic operations are necessary here.
 * 
 * Arguments:
 *    - mod, Module ID which will receive the report
 * 
 * Returns:
 *    ---
 */
void empower_agent::send_slice_feedback(uint32_t mod)
{ 
  char             buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int              blen;
  all_args_t *     args = enb::get_instance()->get_args();

  int              i;
  uint64_t         slices[32];
  uint16_t         nof_slices;
  
  ep_ran_slice_det det;
  ran_interface_common::slice_args slice_inf;

  // User memory allocated for det, this way we directly save them there
  slice_inf.users     = det.users;

  nof_slices = em_agent->get_ran()->get_slices(32, slices);

  if(nof_slices > 0) {
    for(i = 0; i < nof_slices; i++) {
      /* Do not report the default slice */
      if(slices[i] == RAN_DEFAULT_SLICE) {
        continue;
      }

      slice_inf.nof_users = EP_RAN_USERS_MAX;

      if(em_agent->get_ran()->get_slice_info(slices[i], &slice_inf)) {
        continue;
      }

      // Update values that will be sent to controller 
      det.l2.usched = slice_inf.l2.mac.user_sched;
      det.l2.rbgs   = slice_inf.l2.mac.rbg;
      det.nof_users = slice_inf.nof_users;

      blen = epf_single_ran_slice_rep(
        buf, 
        EMPOWER_AGENT_BUF_SMALL_SIZE,
        em_agent->get_id(),
        (uint16_t)args->enb.pci,
        mod,
        slices[i],
        &det);

      if(blen > 0) {
        em_send(em_agent->get_id(), buf, blen);
      }
    }
  }
}

#endif // HAVE_RAN_SLICER

/******************************************************************************
 *                                                                            *
 *                            Generic utilities                               *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    empower_agent::dirty_ue_check
 * 
 * Abstract:
 *    Check if UEs status changed, and if it's needed to report it.
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
void empower_agent::dirty_ue_check()
{
  Debug("Checking for changes in the UE status\n");

  /* Check if trigger is still there */
  if(!em_has_trigger(m_id, m_uer_tr)) {
    m_uer_feat = 0;
    return;
  }

  if(m_ues_dirty) {
    Debug("Sending UE report\n");

    send_UE_report();
    m_ues_dirty = 0;
  }
}

/* Routine:
 *    empower_agent::measure_check
 * 
 * Abstract:
 *    Check if UEs measurement status changed, and if it's needed to report it.
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
void empower_agent::measure_check()
{
  int i;
  std::map<uint16_t, em_ue *>::iterator it;

  Debug("Checking for changes in the UE RRC measurements status\n");

  for (it = m_ues.begin(); it != m_ues.end(); ++it) {
    for(i = 0; i < EMPOWER_AGENT_MAX_MEAS; i++) {
      if(it->second->m_meas[i].trig_id == 0) {
        continue;
      }

      /* No more there... remove from agent. */
      if(!em_has_trigger(m_id, it->second->m_meas[i].trig_id)) {
        it->second->m_meas[i].trig_id = 0;
        it->second->m_meas[i].mod_id  = 0;
        it->second->m_meas[i].meas_id = 0;

        Debug("RRC measurement %d removed\n", m_id);
        continue;
      }

      if(it->second->m_meas[i].c_dirty) {
        Debug("Sending RRC measurement for UE %x\n", it->first);

        send_UE_meas(&it->second->m_meas[i]);
        it->second->m_meas[i].c_dirty = 0;
      }
    }
  }
}
#if 0
/* Routine:
 *    empower_agent::macrep_check
 * 
 * Abstract:
 *    Check if MAC status changed, and if it's needed to report it.
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
void empower_agent::macrep_check()
{
  int             i;
  ep_macrep_det   mac;
  struct timespec now;

  all_args_t *    args = (all_args_t *)m_args;

  Debug("Checking for changes in the MAC status\n");

  for(i = 0; i < EMPOWER_AGENT_MAX_MACREP; i++) {
    if(!m_macrep[i].trigger_id) {
      continue;
    }

    /* Trigger no more there; mark as free slot */
    if(!em_has_trigger(m_id, m_macrep[i].trigger_id)) {
      m_macrep[i].trigger_id = 0;

      Debug("MAC report for module %d revoked\n", m_macrep[i].mod_id);
    }

    clock_gettime(CLOCK_REALTIME, &now);

    /* Interval given elapsed? */
    if(time_diff(m_macrep[i].last, now) >= m_macrep[i].interval) {
      mac.DL_prbs_total        = (uint8_t)args->enb.n_prb;
      mac.DL_prbs_used         = m_DL_prbs_used;

      mac.UL_prbs_total        = (uint8_t)args->enb.n_prb;
      mac.UL_prbs_used         = m_UL_prbs_used;

      m_macrep[i].last.tv_sec  = now.tv_sec;
      m_macrep[i].last.tv_nsec = now.tv_nsec;

      Debug("Sending MAC report for module %d\n", m_macrep[i].mod_id);
      send_MAC_report(m_macrep[i].mod_id, &mac);

      m_DL_sf                  = 0;
      m_DL_prbs_used           = 0;
      m_UL_sf                  = 0;
      m_UL_prbs_used           = 0;
    }
  }
}
#endif
/* Routine:
 *    empower_agent::ran_check
 * 
 * Abstract:
 *    Check if RAN status changed, and if it's needed to report it.
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
void empower_agent::ran_check()
{
#ifdef HAVE_RAN_SLICER

  ran_interface_common::slice_args slice_inf;

  char             buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int              blen;
  ep_ran_slice_det det;
  all_args_t *     args = (all_args_t *)m_args;

  /* I do not care of the lock here; if we miss the update now, then do it at 
   * the next round of RAN_check. This variable is set in one place only.
   */
  if(m_RAN_def_dirty == 0) {
    return;
  }

  memset(&det, 0, sizeof(ep_ran_slice_det));

#ifdef HAVE_RAN_SLICER
  // Send feedback of all the slices!
  slice_feedback(m_RAN_mod);
#endif

  // Now since we want to set this, we need to lock it!
  Lock(&m_lock);
  m_RAN_def_dirty = 0;
  Unlock(&m_lock);

#endif // HAVE_RAN_SLICER

  return;
}

/* Routine:
 *    empower_agent::prbs_from_dci
 * 
 * Abstract:
 *    Extracts the number of PRBs used from a given PCI; this operation can be
 *    performed in both DL and UL.
 * 
 *    This procedure has been copied from srsLTE library routines.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - dci, The DCI structure to analyse
 *    - dl, operation on a Downlink DCI?
 *    - cell_prbs, number of total PRBs of the cell
 * 
 * Returns:
 *    The number of PRBs used by the DCI
 */
int empower_agent::prbs_from_dci(void * dci, int dl, uint32_t cell_prbs)
{
  srslte_ra_dl_dci_t * dld;
  srslte_ra_ul_dci_t * uld;

  if(dl) {
    dld = (srslte_ra_dl_dci_t *)dci;

    if(dld->alloc_type == SRSLTE_RA_ALLOC_TYPE0) {
      return prbs_from_mask(
        dld->alloc_type, dld->type0_alloc.rbg_bitmask, cell_prbs);
    } else if(dld->alloc_type == SRSLTE_RA_ALLOC_TYPE1) {
      return prbs_from_mask(
        dld->alloc_type, dld->type1_alloc.vrb_bitmask, cell_prbs);
    } else {
      return prbs_from_mask(
        dld->alloc_type, dld->type2_alloc.riv, cell_prbs);
    }
  } else {
    uld = (srslte_ra_ul_dci_t *)dci;

    /* Sub-frames are used entirely by a single UE per time */
    return prbs_from_mask(
      SRSLTE_RA_ALLOC_TYPE2, uld->type2_alloc.riv, cell_prbs) * cell_prbs;
  }

  return 0;
}

/* Routine:
 *    empower_agent::prbs_from_mask
 * 
 * Abstract:
 *    Extracts the number of PRBs used from a certain bitmask.
 *    This procedure has been copied from srsLTE library routines.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - RA_format, Resource Allocation Format
 *    - mask, the bitmask to analyse
 *    - cell_prbs, total resources used by the cell
 * 
 * Returns:
 *    The number of PRBs used by the DCI
 */
int empower_agent::prbs_from_mask(
  int RA_format, uint32_t mask, uint32_t cell_prbs)
{
  int ret     = 0;
  uint32_t i;
  uint32_t P  = srslte_ra_type0_P(cell_prbs);

  switch(RA_format) {
  case SRSLTE_RA_ALLOC_TYPE0:
    for(i = 0; i < sizeof(uint32_t) * 8; i++) {
      if(mask & (1 << i)) {
        ret += P;
      }
    }
    break;
  case SRSLTE_RA_ALLOC_TYPE1:
    for(i = 0; i < sizeof(uint32_t) * 8; i++) {
      if(mask & (1 << i)) {
        ret += 1;
      }
    }
    break;
  case SRSLTE_RA_ALLOC_TYPE2:
    ret = (int)floor((double)mask / (double)cell_prbs) + 1;
    break;
  }

  return ret;
}

/******************************************************************************
 *                                                                            *
 *                         Agent threading context                            *
 *                                                                            *
 ******************************************************************************/

/* Routine:
 *    empower_agent::agent_loop
 * 
 * Abstract:
 *    Perform agent operation in a loop. The thread is stopped when the state
 *    is switched to the right value, and resources are freed at its end.
 * 
 * Assumptions:
 *    ---
 * 
 * Arguments:
 *    - args, the instance of the agent to operate on
 * 
 * Returns:
 *    Null pointer
 */
void * empower_agent::agent_loop(void * args)
{
  all_args_t *    enb_args = enb::get_instance()->get_args();
  empower_agent * a        = (empower_agent *)args;

  int i;

  if(em_start(
    a->m_id,
    &empower_agent_ops,
    (char *)enb_args->enb.ctrl_addr.c_str(),
    enb_args->enb.ctrl_port)) 
  {
    return 0;
  }

  a->m_state = AGENT_STATE_STARTED;

  /* Loop of feedbacks which interacts with controller */

  while(a->m_state != AGENT_STATE_STOPPED) {
    if(a->m_uer_feat) {
      a->dirty_ue_check();
    }

    if(a->m_RAN_feat) {
      a->ran_check();
    }

    a->measure_check();
    //a->macrep_check();

    usleep(100000); // Sleep for 100 ms
  }

  em_terminate_agent(a->m_id);

  return 0;
}

/* Routine:
 *    empower_agent::stop
 * 
 * Abstract:
 *    Stops the agent thread and trigger the release of its resources.
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
void empower_agent::stop()
{
  if(m_state != AGENT_STATE_STOPPED) {
    m_state = AGENT_STATE_STOPPED;

    pthread_join(m_thread, 0);
    Debug("Agent stopped!\n");
  }

  release();
}

} /* namespace srsenb */
