#include "main.h"
#include "mav_spam_list.hpp"
#include "mavlink_local.hpp"
#include "mav_postman.hpp"

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
static mavlink_status_t status;
static mavlink_message_t rx_msg;
MavSpamList MavPostman::spam_list;

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
void mavPostmanRxLoop(uint8_t c){
  if (mavlink_parse_char(MAVLINK_COMM_0, c, &rx_msg, &status)) {
    MavPostman::spam_list.dispatch(rx_msg);
  }
}

/*
 ******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************
 */