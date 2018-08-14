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

#define EMPOWER_AGENT_BUF_SMALL_SIZE          2048

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

#define RSRP_RANGE_TO_VALUE(x)  ((float)x - 140.0f)
#define RSRQ_RANGE_TO_VALUE(x)  (((float)x / 2) - 20.0f)

/* Dif "b-a" two timespec structs and return such value in ms.*/
#define ts_diff_to_ms(a, b)           \
  (((b.tv_sec - a.tv_sec) * 1000) +   \
  ((b.tv_nsec - a.tv_nsec) / 1000000))

namespace srsenb {

/******************************************************************************
 * Agent UE procedures.                                                       *
 ******************************************************************************/

empower_agent::em_ue::em_ue()
{
  m_imsi         = 0;
  m_plmn         = 0;

  m_next_meas_id = 1;
  m_next_obj_id  = 1;
  m_next_rep_id  = 1;

  memset(m_meas,   0, sizeof(ue_meas) * EMPOWER_AGENT_MAX_MEAS);
}

/******************************************************************************
 * Agent callback system.                                                     *
 ******************************************************************************/

static void slice_feedback(uint32_t mod);

/* NOTE: This can hold only one reference of agent. If you plan to go with more
 * consider using a map enb_id --> agent instance.
 */
static empower_agent * em_agent = 0;

static int ea_disconnected()
{
	return em_agent->reset();
}

static int ea_cell_setup(uint32_t mod, uint16_t pci)
{
  char        buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  ep_cell_det cell;
  int         blen;

  all_args_t * args = enb::get_instance()->get_args();

  /* Cell not present; notify the error */
  if(pci != (uint16_t)args->enb.pci) {
    blen = epf_single_ccap_rep_fail(
      buf,
      EMPOWER_AGENT_BUF_SMALL_SIZE,
      em_agent->get_id(),
      (uint16_t)args->enb.pci,
      mod);

    if(blen < 0) {
      return -1;
    }

    return em_send(em_agent->get_id(), buf, blen);
  }

  cell.cap       = EP_CCAP_NOTHING;
  cell.pci       = (uint16_t)args->enb.pci;
  cell.DL_earfcn = (uint16_t)args->rf.dl_earfcn;
  cell.UL_earfcn = (uint16_t)args->rf.ul_earfcn;
  cell.DL_prbs   = (uint8_t) args->enb.n_prb;
  cell.UL_prbs   = (uint8_t) args->enb.n_prb;

  blen = epf_single_ccap_rep(
    buf,
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    cell.pci,
    mod,
    &cell);

  if(blen < 0) {
    return -1;
  }

  return em_send(em_agent->get_id(), buf, blen);
}

static int ea_enb_setup(uint32_t mod)
{
  char         buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  ep_enb_det   enbd;
  int          blen;
  all_args_t * args  = enb::get_instance()->get_args();

  /* This eNB can report and measure UE on its cells */
  enbd.capmask = EP_ECAP_UE_REPORT | EP_ECAP_UE_MEASURE;

  /* The cell can perform MAC layer resource reporting */
  enbd.cells[0].cap       = EP_CCAP_MAC_REPORT;

#ifdef HAVE_RAN_SLICER
  enbd.cells[0].cap       |= EP_CCAP_RAN_SLICING;
#endif

  enbd.cells[0].pci       = (uint16_t)args->enb.pci;
  enbd.cells[0].DL_earfcn = (uint16_t)args->rf.dl_earfcn;
  enbd.cells[0].UL_earfcn = (uint16_t)args->rf.ul_earfcn;
  enbd.cells[0].DL_prbs   = (uint8_t) args->enb.n_prb;
  enbd.cells[0].UL_prbs   = (uint8_t) args->enb.n_prb;
  
  enbd.nof_cells          = 1;

  blen = epf_single_ecap_rep(
    buf, EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    0, /* Response coming from eNB, and not a cell in particular */
    mod,
    &enbd);

  if(blen < 0) {
    return -1;
  }

  return em_send(em_agent->get_id(), buf, blen);
}

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
  char buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int  blen;

  if(em_agent->setup_UE_period_meas(
    measure_id, trig_id, rnti, mod, earfcn, max_cells, max_meas, interval)) 
  {
    blen = epf_trigger_uemeas_rep_fail(
      buf,
      EMPOWER_AGENT_BUF_SMALL_SIZE,
      em_agent->get_id(),
      0,
      mod);

    if(blen > 0) {
      em_send(em_agent->get_id(), buf, blen);
    }
  }

  return 0;
}

static int ea_mac_report(
  uint32_t mod,
  int32_t  interval,
  int      trig_id)
{
  return em_agent->setup_MAC_report(mod, interval, trig_id);
}

static int ea_ue_report(uint32_t mod, int trig_id)
{
  return em_agent->setup_UE_report(mod, trig_id);
}

static int ea_ran_setup_request(uint32_t mod)
{
  char         buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int          blen;
  ep_ran_det   det;
  all_args_t * args  = enb::get_instance()->get_args();

  printf("ea_ran_setup_request\n"); /* ----------------------------------------- Just for debugging; REMOVE! */

#ifdef HAVE_RAN_SLICER
  det.l1_mask = 0;
  det.l2_mask = 0;
  det.l3_mask = 0;
  /* This should retrieved in a way like em_agent->ran_get_slice_id() */
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

#else
  blen = epf_single_ran_setup_ns(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }
#endif

  return 0;
}

static int ea_slice_request(uint32_t mod, uint64_t slice)
{
  char             buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int              blen;
  all_args_t *     args  = enb::get_instance()->get_args();
  
  uint16_t         i;
  uint64_t         slices[32];
  uint16_t         nof_slices;

  ep_ran_slice_det det = {0};

  printf("ea_slice_request %" PRIu64 "\n", slice); /* -------------------------- Just for debugging; REMOVE! */

#ifdef HAVE_RAN_SLICER

  if(slice > 0) {
    det.nof_users = 16;

    em_agent->get_ran()->get_slice_info(
      slice, &det.l2.usched, &det.l2.rbgs, det.users, &det.nof_users);
    
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

    return 0;
  }

  em_agent->setup_RAN_report(mod);
  
  slice_feedback(mod);
/*
  nof_slices = em_agent->get_ran()->get_slices(32, slices);

  if(nof_slices > 0) {
    for(i = 0; i < nof_slices; i++) {
      det.nof_users = 16;

      if(em_agent->get_ran()->get_slice_info(
        slices[i], &det.l2.usched, &det.l2.rbgs, det.users, &det.nof_users)) 
      {
          continue;
      }

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
  */
#else
  blen = epf_single_ran_slice_ns(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }
#endif
  return 0;
}

int ea_slice_add(uint32_t mod, uint64_t slice, em_RAN_conf * conf)
{
  char               buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int                blen;
  int                i;
  uint16_t           usr[32] = { 0 };
  all_args_t *       args    = enb::get_instance()->get_args();
  ran_set_slice_args ran_args= { 0 };

  ep_ran_slice_det   sdet;

  printf("ea_slice_add %" PRIu64 "\n", slice); /* ------------------------------ Just for debugging; REMOVE! */

#ifdef HAVE_RAN_SLICER
  // PLMN is used in the slice ID for this moment
  if(em_agent->get_ran()->add_slice(slice, ((slice >> 32) & 0x00ffffff))) {
printf(">>> Slice already exists... out\n");
    return 0;
  }

  slice_feedback(mod);
/*
  ran_args.user_sched = conf->l2.user_sched;
  ran_args.rbg        = conf->l2.rbg;

  for(i = 0; i < conf->nof_users; i++) {
    usr[i] = conf->users[i];
  }
  
  ran_args.users     = usr;
  ran_args.nof_users = conf->nof_users;

  if(em_agent->get_ran()->set_slice(slice, &ran_args)) {
    return 0;
  }

  // Get up to 16 users
  sdet.nof_users = 16;

  em_agent->get_ran()->get_slice_info(
    slice, &sdet.l2.usched, &sdet.l2.rbgs, sdet.users, &sdet.nof_users);

  blen = epf_single_ran_slice_rep(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod,
    slice,
    &sdet);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }
*/
#else // HAVE_RAN_SLICER
  blen = epf_single_ran_slice_ns(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }
#endif // HAVE_RAN_SLICER
  return 0;
}

static int ea_slice_rem(uint32_t mod, uint64_t slice) 
{
  char               buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int                blen;

  printf("ea_slice_rem %" PRIu64 "\n", slice); /* ------------------------------ Just for debugging; REMOVE! */

#ifdef HAVE_RAN_SLICER
  em_agent->get_ran()->rem_slice(slice);
  
  slice_feedback(mod);
  /*
  blen = epf_single_ran_slice_ns(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }  
  */
#else // HAVE_RAN_SLICER
  blen = epf_single_ran_slice_ns(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }
#endif // HAVE_RAN_SLICER
  return 0;
}

static int ea_slice_conf(uint32_t mod, uint64_t slice, em_RAN_conf * conf)
{
  char               buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int                blen;
  int                i;
  uint16_t           usr[32] = { 2 };
  all_args_t *       args    = enb::get_instance()->get_args();
  ran_set_slice_args ran_args;

  uint64_t           slices[32];
  uint16_t           nof_slices;

  ep_ran_slice_det   sdet;

  printf("slice_conf %" PRIu64 "\n", slice); /* -------------------------------- Just for debugging; REMOVE! */

#ifdef HAVE_RAN_SLICER
  
  ran_args.user_sched = conf->l2.user_sched;
  ran_args.rbg        = conf->l2.rbg;

  for(i = 0; i < conf->nof_users; i++) {
    //em_agent->get_ran()->add_slice_user(conf->users[i], slice, 1);
    usr[i] = conf->users[i];
  }
  
  ran_args.users     = usr;
  ran_args.nof_users = conf->nof_users;

  if(em_agent->get_ran()->set_slice(slice, &ran_args)) {
    return 0;
  }

  slice_feedback(mod);
/*
  // Get up to 16 users
  sdet.nof_users = 16;

  nof_slices = em_agent->get_ran()->get_slices(32, slices);

  if(nof_slices > 0) {
    for(i = 0; i < nof_slices; i++) {
      sdet.nof_users = 16;

      em_agent->get_ran()->get_slice_info(
        slices[i], &sdet.l2.usched, &sdet.l2.rbgs, sdet.users, &sdet.nof_users);

      blen = epf_single_ran_slice_rep(
        buf, 
        EMPOWER_AGENT_BUF_SMALL_SIZE,
        em_agent->get_id(),
        (uint16_t)args->enb.pci,
        mod,
        slices[i],
        &sdet);

      if(blen > 0) {
        em_send(em_agent->get_id(), buf, blen);
      }
    }
  }
*/
  /*
  em_agent->get_ran()->get_slice_info(
    slice, &sdet.l2.usched, &sdet.l2.rbgs, sdet.users, &sdet.nof_users);

  blen = epf_single_ran_slice_rep(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod,
    slice,
    &sdet);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }
  */
#else // HAVE_RAN_SLICER
  blen = epf_single_ran_slice_ns(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    mod);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }
#endif // HAVE_RAN_SLICER
  return 0;
}

// Send situation of all the slices
static void slice_feedback(uint32_t mod)
{ 
  char             buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int              blen;
  all_args_t *     args    = enb::get_instance()->get_args();

  int              i;
  uint64_t         slices[32];
  uint16_t         nof_slices;
  
  ep_ran_slice_det det;

  nof_slices = em_agent->get_ran()->get_slices(32, slices);

  if(nof_slices > 0) {
    for(i = 0; i < nof_slices; i++) {
      det.nof_users = 16;

      if(em_agent->get_ran()->get_slice_info(
        slices[i], &det.l2.usched, &det.l2.rbgs, det.users, &det.nof_users)) 
      {
          continue;
      }

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

static struct em_agent_ops empower_agent_ops = {
  0,                      /* init */
  0,                      /* release */
  ea_disconnected,        /* disconnected*/
  ea_cell_setup,          /* cell_setup_request*/
  ea_enb_setup,           /* enb_setup_request*/
  ea_ue_report,           /* ue_report*/
  ea_ue_measure,          /* UE measurement */
  0,                      /* handover_UE*/
  ea_mac_report,          /* mac_report*/

  /*
   * RAN Request operations
   */
  {
    ea_ran_setup_request,   /* ran.setup_request */
    ea_slice_request,       /* slice_request */
    ea_slice_add,           /* slice_add */
    ea_slice_rem,           /* slice_rem */
    ea_slice_conf           /* slice_conf */
  }
};

/******************************************************************************
 * Constructor/destructors.                                                   *
 ******************************************************************************/

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

  memset(m_macrep, 0, sizeof(macrep) *  EMPOWER_AGENT_MAX_MACREP);
  m_DL_prbs_used = 0;
  m_DL_sf        = 0;
  m_UL_prbs_used = 0;
  m_UL_sf        = 0;

  m_RAN_feat     = 0;
  m_RAN_def_dirty= 0;
  m_RAN_mod      = 0;

  m_thread       = 0;
}

empower_agent::~empower_agent()
{
  release();
}

/******************************************************************************
 * Generic purposes procedures.                                               *
 ******************************************************************************/

unsigned int empower_agent::get_id()
{
  return m_id;
}

ran_interface_agent * empower_agent::get_ran()
{
  return m_ran;
}

int empower_agent::init(
  int enb_id, 
  rrc_interface_agent * rrc, 
  ran_interface_agent * ran, 
  srslte::log * logger)
{
  if(!rrc || !logger) {
    return -EINVAL;
  }

  m_id = enb_id;

  m_rrc = rrc;
  m_ran = ran;
  m_logger= logger;

  m_args = enb::get_instance()->get_args();

  pthread_spin_init(&m_lock, 0);

  /* NOTE:
   * This is no more valid if you initialize more than one empower_agent!!!
   *
   * The callback systems will be redirected always on the least initialized
   * agent, this way. The problem does not arise for the moment since the eNB
   * just need one agent, and no more than one (for now).
   *
   * A proper connection between c callback system and C++ class must be put
   * in place, like for example:
   *
   * 	b_id --> class pointer to use
   */
  em_agent = this;

  /* Create a new thread; we don't use the given thread library since we don't
   * want RT capabilities for this thread, which will run with low priority.
   */
  pthread_create(&m_thread, 0, empower_agent::agent_loop, this);

  return 0;
}

void empower_agent::release()
{

}

int empower_agent::reset()
{
  int      i;
  uint16_t rnti;

  Debug("Resetting the state of the Agent\n");

  /* Reset any UE report */
  m_uer_mod  = 0;
  m_uer_tr   = 0;
  m_uer_feat = 0;

  pthread_spin_lock(&m_lock);

  /* Reset any MAC report */
  for(i = 0; i < EMPOWER_AGENT_MAX_MACREP; i++) {
    m_macrep[i].trigger_id = 0;
  }

  /* Reset any UE RRC state */
  for(rnti = 0; rnti < 0xffff; rnti++) {
    if(m_ues.count(rnti) > 0) {
      /* Invalidate the measure */
      for(i = 0; i < EMPOWER_AGENT_MAX_MEAS; i++) {
        m_ues[rnti]->m_meas[i].id      = 0;
        m_ues[rnti]->m_meas[i].mod_id  = 0;
        m_ues[rnti]->m_meas[i].trig_id = 0;
      }

      m_ues[rnti]->m_next_meas_id = 1;
      m_ues[rnti]->m_next_obj_id  = 1;
      m_ues[rnti]->m_next_rep_id  = 1;
    }
  }

  pthread_spin_unlock(&m_lock);
  return 0;
}

int empower_agent::setup_UE_report(uint32_t mod_id, int trig_id)
{
  m_uer_mod  = mod_id;
  m_uer_tr   = trig_id;
  m_uer_feat = 1;

  Debug("UE report ready; reporting to module %d\n", mod_id);

  return 0;
}

int empower_agent::setup_MAC_report(
  uint32_t mod_id, uint32_t interval, int trig_id)
{
  int         i;
  int         m = -1;
  char        buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int         blen;

  for(i = 0; i < EMPOWER_AGENT_MAX_MACREP; i++) {
    if(!m_macrep[i].trigger_id) {
      m = i;
    }

    /* Measure is already there... */
    if(m_macrep[i].mod_id == mod_id) {
      m_macrep[i].interval = interval;
      return 0;
    }
  }

  if(m < 0) {
    blen = epf_trigger_macrep_rep_fail(
      buf,
      EMPOWER_AGENT_BUF_SMALL_SIZE,
      em_agent->get_id(),
      0,
      mod_id);

    if(blen > 0) {
      em_send(em_agent->get_id(), buf, blen);
    }

    Debug("New MAC report from module %d ready\n", mod_id);

    return 0;
  }

  /* Setup the MAC report request here: */

  m_macrep[m].mod_id     = mod_id;
  m_macrep[m].interval   = interval;
  m_macrep[m].trigger_id = trig_id;
  m_macrep[m].DL_acc     = 0;
  m_macrep[m].UL_acc     = 0;
  clock_gettime(CLOCK_REALTIME, &m_macrep[m].last);

  return 0;
}

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
  int i;
  int j;
  int n = 0;

  LIBLTE_RRC_MEAS_CONFIG_STRUCT   meas;
  LIBLTE_RRC_REPORT_INTERVAL_ENUM rep_int;

  all_args_t * args = (all_args_t *)m_args;

  /*
   * Setup agent mechanism:
   */
  if(m_ues.count(rnti) == 0) {
    Error("No %x RNTI known\n", rnti);
    return -1;
  }

  for(i = 0; i < EMPOWER_AGENT_MAX_MEAS; i++) {
    if(m_ues[rnti]->m_meas[i].mod_id == 0) {
      break;
    }
  }

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
    max_cells > EMPOWER_AGENT_MAX_CELL_MEAS ? EMPOWER_AGENT_MAX_CELL_MEAS : max_cells;
  m_ues[rnti]->m_meas[i].max_meas    =
    max_meas > EMPOWER_AGENT_MAX_MEAS ? EMPOWER_AGENT_MAX_MEAS : max_meas;

  m_ues[rnti]->m_meas[i].meas_id     = m_ues[rnti]->m_next_meas_id++;
  m_ues[rnti]->m_meas[i].obj_id      = m_ues[rnti]->m_next_obj_id++;
  m_ues[rnti]->m_meas[i].rep_id      = m_ues[rnti]->m_next_rep_id++;

  Debug("Setting up RRC measurement %d-->%d for RNTI %x\n", m_ues[rnti]->m_meas[i].id, m_ues[rnti]->m_meas[i].meas_id, rnti);

  /*
   * Prepare RRC request to send to the UE.
   */

  /* NOTE: This has probably to be setup every time we request a measure to the
   * UE, with the list of measurements to do. Probably old measurements are
   * discarded and only the last one maintained... need to test...
   */

  bzero(&meas, sizeof(LIBLTE_RRC_MEAS_CONFIG_STRUCT));

  for(j = 0; j < EMPOWER_AGENT_MAX_MEAS; j++) {
    if(m_ues[rnti]->m_meas[j].mod_id == 0) {
      continue;
    }

    if(m_ues[rnti]->m_meas[j].interval <= 120) {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS120;
    } else if(m_ues[rnti]->m_meas[j].interval > 120 && m_ues[rnti]->m_meas[j].interval <= 240) {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS240;
    } else if(m_ues[rnti]->m_meas[j].interval > 240 && m_ues[rnti]->m_meas[j].interval <= 480) {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS480;
    } else if(m_ues[rnti]->m_meas[j].interval > 480 && m_ues[rnti]->m_meas[j].interval <= 640) {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS640;
    } else if(m_ues[rnti]->m_meas[j].interval > 640 && m_ues[rnti]->m_meas[j].interval <= 1024) {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS1024;
    } else if(m_ues[rnti]->m_meas[j].interval > 1024 && m_ues[rnti]->m_meas[j].interval <= 2048) {
      rep_int = LIBLTE_RRC_REPORT_INTERVAL_MS2048;
    } else if(m_ues[rnti]->m_meas[j].interval > 2048 && m_ues[rnti]->m_meas[j].interval <= 5120) {
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

    // Measurement object:

    meas.meas_obj_to_add_mod_list.N_meas_obj++;
    n = meas.meas_obj_to_add_mod_list.N_meas_obj - 1;

    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_id                                       = m_ues[rnti]->m_meas[j].obj_id;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_type                                     = LIBLTE_RRC_MEAS_OBJECT_TYPE_EUTRA;

    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.offset_freq_not_default            = false;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.presence_ant_port_1                = true;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.cells_to_remove_list_present       = false;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.black_cells_to_remove_list_present = false;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.cell_for_which_to_rep_cgi_present  = false;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.N_black_cells_to_add_mod           = 0;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.N_cells_to_add_mod                 = 0;

    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.allowed_meas_bw                    = LIBLTE_RRC_ALLOWED_MEAS_BANDWIDTH_MBW25;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.offset_freq                        = LIBLTE_RRC_Q_OFFSET_RANGE_DB_0;
    meas.meas_obj_to_add_mod_list.meas_obj_list[n].meas_obj_eutra.carrier_freq                       = m_ues[rnti]->m_meas[j].freq;

    // Measurement report:

    meas.rep_cnfg_to_add_mod_list.N_rep_cnfg++;
    n = meas.rep_cnfg_to_add_mod_list.N_rep_cnfg - 1;

    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_id                                       = m_ues[rnti]->m_meas[j].rep_id;
    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_type                                     = LIBLTE_RRC_REPORT_CONFIG_TYPE_EUTRA;

    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_eutra.trigger_type                       = LIBLTE_RRC_TRIGGER_TYPE_EUTRA_PERIODICAL;
    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_eutra.trigger_quantity                   = LIBLTE_RRC_TRIGGER_QUANTITY_RSRQ;
    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_eutra.periodical.purpose                 = LIBLTE_RRC_PURPOSE_EUTRA_REPORT_STRONGEST_CELL;
    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_eutra.report_amount                      = LIBLTE_RRC_REPORT_AMOUNT_INFINITY;
    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_eutra.report_quantity                    = LIBLTE_RRC_REPORT_QUANTITY_BOTH;
    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_eutra.report_interval                    = rep_int;
    meas.rep_cnfg_to_add_mod_list.rep_cnfg_list[n].rep_cnfg_eutra.max_report_cells                   = m_ues[rnti]->m_meas[j].max_cells;

    // Measurement IDs:

    meas.meas_id_to_add_mod_list.N_meas_id                                                           = n + 1;
    meas.meas_id_to_add_mod_list.meas_id_list[n].meas_id                                             = m_ues[rnti]->m_meas[j].meas_id;
    meas.meas_id_to_add_mod_list.meas_id_list[n].meas_obj_id                                         = m_ues[rnti]->m_meas[j].obj_id;
    meas.meas_id_to_add_mod_list.meas_id_list[n].rep_cnfg_id                                         = m_ues[rnti]->m_meas[j].rep_id;
  }

  Debug("Sending to %x a new RRC reconfiguration for %d measurement(s)\n", rnti, n + 1);

  m_rrc->setup_ue_measurement(rnti, &meas);

  return 0;
}

int empower_agent::setup_RAN_report(uint32_t mod)
{
  m_RAN_feat      = 1;
  m_RAN_def_dirty = 0;
  m_RAN_mod       = mod;

  return 0;
}

/******************************************************************************
 * agent_interface_mac.                                                       *
 ******************************************************************************/

void empower_agent::process_DL_results(
  uint32_t tti, sched_interface::dl_sched_res_t * sched_result)
{
  uint32_t i;
  int      prbs = 0;

  all_args_t * args = (all_args_t *)m_args;

  for(i = 0; i < sched_result->nof_bc_elems; i++) {
    prbs += prbs_from_dci(&sched_result->bc[i].dci, 1, args->enb.n_prb);
  }

  for(i = 0; i < sched_result->nof_rar_elems; i++) {
    prbs += prbs_from_dci(&sched_result->rar[i].dci, 1, args->enb.n_prb);
  }

  for(i = 0; i < sched_result->nof_data_elems; i++) {
    prbs += prbs_from_dci(&sched_result->data[i].dci, 1, args->enb.n_prb);
  }

  m_DL_prbs_used += prbs;
  m_DL_sf++;
}

void empower_agent::process_UL_results(
  uint32_t tti, sched_interface::ul_sched_res_t * sched_result)
{
  uint32_t i;
  int      prbs = 0;

  all_args_t * args = (all_args_t *)m_args;

  for(i = 0; i < sched_result->nof_dci_elems; i++) {
    prbs += prbs_from_dci(&sched_result->pusch[i].dci, 0, args->enb.n_prb);
  }

  m_UL_prbs_used += prbs;
  m_UL_sf++;
}

/******************************************************************************
 * agent_interface_rrc.                                                       *
 ******************************************************************************/

void empower_agent::add_user(uint16_t rnti)
{
  std::map<uint16_t, em_ue *>::iterator it;

  all_args_t * args = (all_args_t *)m_args;

  pthread_spin_lock(&m_lock);

  it = m_ues.find(rnti);

  if(it == m_ues.end()) {
    m_ues.insert(std::make_pair(rnti, new em_ue()));
    m_nof_ues++;

    m_ues[rnti]->m_plmn  = (args->enb.s1ap.mcc & 0x0fff) << 12;
    m_ues[rnti]->m_plmn |= (args->enb.s1ap.mnc & 0x0fff);

    m_ues[rnti]->m_next_meas_id = 1;
    m_ues[rnti]->m_next_obj_id  = 1;
    m_ues[rnti]->m_next_rep_id  = 1;

    /* Clean up measurements*/
    memset(m_ues[rnti]->m_meas, 0, sizeof(em_ue::ue_meas) * EMPOWER_AGENT_MAX_MEAS);

#ifdef HAVE_RAN_SLICER
    /* Add the user to the default slice */
    m_ran->add_slice_user(rnti, 9622457614860288L, 0);
    m_RAN_def_dirty = 1;
#endif

    if(m_uer_feat) {
      m_ues_dirty = 1;
    }

    Debug("Added user %x (PLMN:%x)\n", rnti, m_ues[rnti]->m_plmn);
  }

  pthread_spin_unlock(&m_lock);
}

void empower_agent::rem_user(uint16_t rnti)
{
  em_ue * ue;
  std::map<uint16_t, em_ue *>::iterator it;

  pthread_spin_lock(&m_lock);

  it = m_ues.find(rnti);

  if(it != m_ues.end()) {
    Debug("Removing user %x\n", rnti);

    ue = it->second;

    m_nof_ues--;
    m_ues.erase(it);

    if(m_uer_feat) {
      m_ues_dirty = 1;
    }

    delete it->second;

#ifdef HAVE_RAN_SLICER
    /* Add the user to the default slice */
    m_ran->rem_slice_user(rnti, 0);
    m_RAN_def_dirty = 1;
#endif
  }

  pthread_spin_unlock(&m_lock);
}

void empower_agent::report_RRC_measure(
  uint16_t rnti, LIBLTE_RRC_MEASUREMENT_REPORT_STRUCT * report)
{
  int i;
  int j;
  int nof_cells = 0;

  LIBLTE_RRC_MEAS_RESULT_EUTRA_STRUCT * cells;

  if (report->have_meas_result_neigh_cells && 
    report->meas_result_neigh_cells_choice == LIBLTE_RRC_MEAS_RESULT_LIST_EUTRA) {

    nof_cells = report->meas_result_neigh_cells.eutra.n_result;
  }

  for(i = 0; i < EMPOWER_AGENT_MAX_MEAS; i++) {
    if(m_ues[rnti]->m_meas[i].meas_id == report->meas_id) {
      break;
    }
  }

  /* NOTE: Should we try to revoke the measure if is not managed? */
  if(i == EMPOWER_AGENT_MAX_MEAS) {
    Error("Measure %d of RNTI %x not found! Index=%d\n", report->meas_id, rnti, i);
    return;
  }

  if(m_ues.count(rnti) != 0) {
    Debug("Saving received RRC measure %d from user %x\n", m_ues[rnti]->m_meas[i].id, rnti);

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
 * Interaction with controller.                                               *
 ******************************************************************************/

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
    Error("Cannot format MAC report reply\n");
    return;
  }

  Debug("Sending MAC report\n");

  em_send(m_id, buf, size);
}

void empower_agent::send_UE_report(void)
{
  std::map<uint16_t, em_ue *>::iterator it;

  int           i;
  char          buf[EMPOWER_AGENT_BUF_SMALL_SIZE];
  int           size;
  ep_ue_details ued[16];

  all_args_t * args = (all_args_t *)m_args;

  pthread_spin_lock(&m_lock);

  for(i = 0, it = m_ues.begin(); it != m_ues.end(); ++it, i++)
  {
    ued[i].pci  = (uint16_t)args->enb.pci;
    ued[i].rnti = it->first;
    ued[i].plmn = it->second->m_plmn;
    ued[i].imsi = it->second->m_imsi;
  }

  pthread_spin_unlock(&m_lock);

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

void empower_agent::send_UE_meas(em_ue::ue_meas * m)
{
  int           i;
  int           j;
  char          buf[EMPOWER_AGENT_BUF_SMALL_SIZE];
  int           size;

  all_args_t * args = (all_args_t *)m_args;
  ep_ue_measure epm[EMPOWER_AGENT_MAX_MEAS];

  epm[0].meas_id = m->id;
  epm[0].pci     = m->carrier.pci;
  epm[0].rsrp    = m->carrier.rsrp;
  epm[0].rsrq    = m->carrier.rsrq;

  for(i = 0, j = 1; i < EMPOWER_AGENT_MAX_CELL_MEAS; i++) {
    if(m->neigh[i].dirty) {
      epm[j].meas_id = m->id;
      epm[j].pci     = m->neigh[i].pci;
      epm[j].rsrp    = m->neigh[i].rsrp;
      epm[j].rsrq    = m->neigh[i].rsrq;

      m->neigh[i].dirty = 0;

      j++;
    }
  }

  size = epf_trigger_uemeas_rep(
    buf,
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    m_id,
    (uint16_t)args->enb.pci,
    m->mod_id,
    j > m->max_meas ? m->max_meas : j,
    m->max_meas,
    epm);

  if(size < 0) {
    Error("Cannot format UE measurement reply\n");
    return;
  }

  em_send(m_id, buf, size);
}


/******************************************************************************
 * Private utilities.                                                         *
 ******************************************************************************/

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
    if(ts_diff_to_ms(m_macrep[i].last, now) >= m_macrep[i].interval) {
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

void empower_agent::ran_check()
{
  char             buf[EMPOWER_AGENT_BUF_SMALL_SIZE] = {0};
  int              blen;
  uint16_t         i;
  uint64_t         slices[32];
  uint16_t         nof_slices;
  ep_ran_slice_det det;
  all_args_t *     args = (all_args_t *)m_args;

  pthread_spin_lock(&m_lock);
  if(!m_RAN_def_dirty) {
    pthread_spin_unlock(&m_lock);
    return;
  }
  pthread_spin_unlock(&m_lock);

#ifdef HAVE_RAN_SLICER

  det.nof_users = 16;

  m_ran->get_slice_info(
    9622457614860288L, &det.l2.usched, &det.l2.rbgs, det.users, &det.nof_users);
  
  blen = epf_single_ran_slice_rep(
    buf, 
    EMPOWER_AGENT_BUF_SMALL_SIZE,
    em_agent->get_id(),
    (uint16_t)args->enb.pci,
    m_RAN_mod,
    9622457614860288L,
    &det);

  if(blen > 0) {
    em_send(em_agent->get_id(), buf, blen);
  }
#endif

  pthread_spin_lock(&m_lock);
  m_RAN_def_dirty = 0;
  pthread_spin_unlock(&m_lock);

  return;
}

/* Extracts the PRBS used from a certain DCI */
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

/* Extracts the PRBS used from a bits-mask or RIV field */
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
 * Agent threading context.                                                   *
 ******************************************************************************/

void * empower_agent::agent_loop(void * args)
{
  all_args_t *    enb_args = enb::get_instance()->get_args();
  empower_agent * a        = (empower_agent *)args;

  int i;

  if(em_start(
    a->m_id,
    &empower_agent_ops,
    (char *)enb_args->enb.ctrl_addr.c_str(),
    enb_args->enb.ctrl_port)) {

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
    a->macrep_check();

    sleep(1);
  }

  em_terminate_agent(a->m_id);

  return 0;
}

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
