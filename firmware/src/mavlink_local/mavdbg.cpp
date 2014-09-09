#include <stdio.h>

#include "main.h"
#include "message.hpp"

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
extern mavlink_statustext_t mavlink_out_statustext_struct;

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
 * Send debug message.
 *
 * severity[in]   severity of message
 * text[in]       text to send
 */
void mavlink_dbg_print(uint8_t severity, const char *text, MAV_COMPONENT comp){
  uint32_t n = sizeof(mavlink_out_statustext_struct.text);

  mavlink_out_statustext_struct.severity = severity;
  memset(mavlink_out_statustext_struct.text, 0, n);
  memcpy(mavlink_out_statustext_struct.text, text, n);

  StatustextSend(&mavlink_out_statustext_struct, comp);
}

