#pragma GCC optimize "-O0"

#include "main.h"
#include "pads.h"
#include "speedometer.hpp"
#include "mavlink_local.hpp"
#include "param_registry.hpp"

using namespace chibios_rt;

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */
/* EICU clock frequency (about 50kHz for meaningful results).*/
#define EICU_FREQ             (1000 * 50)
/* speedometer driver needs to drop some first samples because of theirs inaccurate */
#define FIRST_SAMPLES_DROP    3

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */
//extern mavlink_debug_vect_t  mavlink_out_debug_vect_struct;

/*
 ******************************************************************************
 * PROTOTYPES
 ******************************************************************************
 */
void speedometer_cb(EICUDriver *eicup, eicuchannel_t channel, uint32_t w, uint32_t p);

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

static const systime_t timeout = MS2ST((32768 * 1000) / EICU_FREQ);

uint32_t Speedometer::total_path = 0;
uint16_t Speedometer::period_cache = 0;

static const EICUChannelConfig speedometercfg = {
    EICU_INPUT_ACTIVE_LOW,
    EICU_INPUT_EDGE,
    speedometer_cb
};

static const EICUConfig eicucfg = {
    EICU_FREQ,    /* EICU clock frequency in Hz.*/
    {
        &speedometercfg,
        NULL,
        NULL,
        NULL
    },
    0
};

/*
 ******************************************************************************
 ******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************
 ******************************************************************************
 */
/**
 *
 */
void speedometer_cb(EICUDriver *eicup, eicuchannel_t channel, uint32_t w, uint32_t p) {
  (void)eicup;
  (void)channel;
  (void)w;

  Speedometer::total_path++;
  Speedometer::period_cache = p;
//  mavlink_out_debug_vect_struct.y = p;
}

/**
 * @brief   Drop some first (potentially inaccurate) samples.
 *
 * @retval  OSAL_SUCCESS if measurement considered good.
 */
bool Speedometer::check_sample(uint32_t &path_ret,
                               uint16_t &last_pulse_period, float dT) {
  bool ret = OSAL_FAILED;
  uint32_t path; /* cache value for atomicity */

  osalSysLock();
  path = total_path;
  last_pulse_period = period_cache;
  osalSysUnlock();

  /* timeout handling */
  if (total_path_prev == path) { /* no updates sins last call */
    capture_time += US2ST(roundf(dT * 1000000));
    if (capture_time > 3 * timeout) /* prevent wrap and false good result */
      capture_time = 2 * timeout;
  }
  else {
    capture_time = 0;
    total_path_prev = path;

    new_sample_seq++;
    if (new_sample_seq > FIRST_SAMPLES_DROP) /* prevent overflow */
      new_sample_seq = FIRST_SAMPLES_DROP;
  }

  /**/
  switch (sample_state) {
  case SampleCosher::bad:
    if ((capture_time < timeout) && (new_sample_seq >= FIRST_SAMPLES_DROP)) {
      sample_state = SampleCosher::good;
      ret = OSAL_SUCCESS;
    }
    else
      ret = OSAL_FAILED;
    break;

  case SampleCosher::good:
    if (capture_time > timeout) {
      sample_state = SampleCosher::bad;
      new_sample_seq = 0;
      ret = OSAL_FAILED;
    }
    else
      ret = OSAL_SUCCESS;
    break;

  default:
    osalSysHalt("Unhandle case");
    break;
  }

  path_ret = path;
  return ret;
}

/*
 ******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************
 */
/**
 *
 */
void Speedometer::start(void) {

  param_registry.valueSearch("SPD_pulse2m", &pulse2m);

  capture_time = 2 * timeout;
  total_path = 0;
  total_path_prev = 0;
  new_sample_seq = 0;

  sample_state = SampleCosher::bad;

  eicuStart(&EICUD11, &eicucfg);
  eicuEnable(&EICUD11);

  ready = true;
}

/**
 *
 */
void Speedometer::stop(void) {

  ready = false;

  eicuDisable(&EICUD11);
  eicuStop(&EICUD11);
}

/**
 *
 */
void Speedometer::update(float &speed, uint32_t &path, float dT) {
  float pps; /* pulse per second */
  uint16_t last_pulse_period;
  bool status;

  osalDbgCheck(ready);

  status = check_sample(path, last_pulse_period, dT);
  if (OSAL_FAILED == status)
    pps = 0;
  else {
    pps = static_cast<float>(EICU_FREQ) / static_cast<float>(last_pulse_period);
  }

  /* now calculate speed */
  pps = filter_alphabeta(pps);
  speed = *pulse2m * pps;
  //mavlink_out_debug_vect_struct.z = speed * 3.6;
}
