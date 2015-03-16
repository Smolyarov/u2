#include "main.h"
#include "pps.hpp"
#include "pads.h"
#include "mavlink_local.hpp"

using namespace chibios_rt;

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */
extern mavlink_debug_vect_t  mavlink_out_debug_vect_struct;

/*
 ******************************************************************************
 * PROTOTYPES
 ******************************************************************************
 */

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

static void pps_cb(EICUDriver *eicup, eicuchannel_t channel, uint32_t w, uint32_t p) {
  (void)eicup;
  (void)channel;
  (void)w;

  int32_t result = p;
  result -= eicup->clock;

  mavlink_out_debug_vect_struct.x = result;
}

static const EICUChannelConfig ppscfg = {
    EICU_INPUT_ACTIVE_HIGH,
    EICU_INPUT_EDGE,
    pps_cb
};

static const EICUConfig eicucfg = {
    (84 * 1000 * 1000),      /* EICU clock frequency (Hz).*/
    {
        &ppscfg,
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

/*
 ******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************
 */
/**
 *
 */
void PPS::start(void) {

  eicuStart(&EICUD5, &eicucfg);
  eicuEnable(&EICUD5);
}

/**
 *
 */
void PPS::stop(void) {

  eicuDisable(&EICUD5);
  eicuStop(&EICUD5);
}


