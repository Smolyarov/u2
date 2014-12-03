/*
    ChibiOS/RT - Copyright (C) 2014 Uladzimir Pylinsky aka barthess

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <string.h>

#include "onewire.h"

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */
#define ONEWIRE_ZERO_WIDTH            60
#define ONEWIRE_ONE_WIDTH             6
#define ONEWIRE_SAMPLE_WIDTH          15
#define ONEWIRE_RECOVERY_WIDTH        10
#define ONEWIRE_RESET_LOW_WIDTH       480
#define ONEWIRE_RESET_SAMPLE_WIDTH    550
#define ONEWIRE_RESET_TOTAL_WIDTH     960

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */

onewireDriver OWD1;

/*
 ******************************************************************************
 * PROTOTYPES
 ******************************************************************************
 */
static void ow_reset_cb(PWMDriver *pwmp, onewireDriver *owp);
static void pwm_reset_cb(PWMDriver *pwmp);
static void ow_read_bit_cb(PWMDriver *pwmp, onewireDriver *owp);
static void pwm_read_bit_cb(PWMDriver *pwmp);
static void ow_write_bit_cb(PWMDriver *pwmp, onewireDriver *owp);
static void pwm_write_bit_cb(PWMDriver *pwmp);
static void ow_search_rom_cb(PWMDriver *pwmp, onewireDriver *owp);
static void pwm_search_rom_cb(PWMDriver *pwmp);

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

/*
 * config for fast of all fields initializing
 */
static const PWMConfig pwm_default_cfg = {
  1000000,
  ONEWIRE_RESET_TOTAL_WIDTH,
  NULL,
  {
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  0,
  0
};

/*
 * Look up table for fast CRC calculation
 */
static const uint8_t onewire_crc_table[256] = {
    0x0,  0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83,
    0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
    0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e,
    0x5f, 0x1,  0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
    0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0,
    0xe1, 0xbf, 0x5d, 0x3,  0x80, 0xde, 0x3c, 0x62,
    0xbe, 0xe0, 0x2,  0x5c, 0xdf, 0x81, 0x63, 0x3d,
    0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
    0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5,
    0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x7,
    0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x6,  0x58,
    0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
    0x65, 0x3b, 0xd9, 0x87, 0x4,  0x5a, 0xb8, 0xe6,
    0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
    0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b,
    0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x5,  0xe7, 0xb9,
    0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0xf,
    0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
    0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92,
    0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0xe,  0x50,
    0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c,
    0x6d, 0x33, 0xd1, 0x8f, 0xc,  0x52, 0xb0, 0xee,
    0x32, 0x6c, 0x8e, 0xd0, 0x53, 0xd,  0xef, 0xb1,
    0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
    0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49,
    0x8,  0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
    0x57, 0x9,  0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4,
    0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
    0xe9, 0xb7, 0x55, 0xb,  0x88, 0xd6, 0x34, 0x6a,
    0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
    0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7,
    0xb6, 0xe8, 0xa,  0x54, 0xd7, 0x89, 0x6b, 0x35
};

static time_measurement_t search_rom_tm;

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
static void pwm_reset_cb(PWMDriver *pwmp) {
  ow_reset_cb(pwmp, &OWD1);
}

/**
 *
 */
static void pwm_read_bit_cb(PWMDriver *pwmp) {
  ow_read_bit_cb(pwmp, &OWD1);
}

/**
 *
 */
static void pwm_write_bit_cb(PWMDriver *pwmp) {
  ow_write_bit_cb(pwmp, &OWD1);
}

/**
 *
 */
static void pwm_search_rom_cb(PWMDriver *pwmp) {
  ow_search_rom_cb(pwmp, &OWD1);
}

/**
 *
 */
static void ow_write_bit_I(onewireDriver *owp, uint8_t bit) {
#if ONEWIRE_SYNTH_SEARCH_TEST
  _synth_ow_write_bit(owp, bit);
#else
  osalSysLockFromISR();
  if (0 == bit)
    pwmEnableChannelI(owp->config->pwmd, owp->config->master_channel, ONEWIRE_ZERO_WIDTH);
  else
    pwmEnableChannelI(owp->config->pwmd, owp->config->master_channel, ONEWIRE_ONE_WIDTH);
  osalSysUnlockFromISR();
#endif
}

/*
 * presence pulse callback
 */
static void ow_reset_cb(PWMDriver *pwmp, onewireDriver *owp) {

  owp->reg.slave_present = (PAL_LOW == owp->config->readBitX());

  osalSysLockFromISR();
  pwmDisableChannelI(pwmp, owp->config->sample_channel);
  osalThreadResumeI(&owp->thread, MSG_OK);
  osalSysUnlockFromISR();
}

/**
 *
 */
static void ow_read_bit_cb(PWMDriver *pwmp, onewireDriver *owp) {

  if (true == owp->reg.final_timeslot) {
    osalSysLockFromISR();
    pwmDisableChannelI(pwmp, owp->config->sample_channel);
    osalThreadResumeI(&owp->thread, MSG_OK);
    osalSysUnlockFromISR();
    return;
  }
  else {
    *owp->buf |= owp->config->readBitX() << owp->reg.bit;
    owp->reg.bit++;
    if (8 == owp->reg.bit) {
      owp->reg.bit = 0;
      owp->buf++;
      owp->reg.bytes--;
      if (0 == owp->reg.bytes) {
        owp->reg.final_timeslot = true;
        osalSysLockFromISR();
        /* Note: sample channel must be stopped later because it
           must generate one more interrupt */
        pwmDisableChannelI(pwmp, owp->config->master_channel);
        osalSysUnlockFromISR();
      }
    }
  }
}

/*
 * bit transmission callback
 */
static void ow_write_bit_cb(PWMDriver *pwmp, onewireDriver *owp) {

  if (8 == owp->reg.bit) {
    owp->buf++;
    owp->reg.bit = 0;
    owp->reg.bytes--;

    if (0 == owp->reg.bytes) {
      osalSysLockFromISR();
      pwmDisableChannelI(pwmp, owp->config->master_channel);
      osalSysUnlockFromISR();
      /* special value to signalizes premature stop protector*/
      owp->reg.final_timeslot = true;
      return;
    }
  }

  /* wait until timer generate last pulse */
  if (true == owp->reg.final_timeslot) {
    #if ONEWIRE_USE_PARASITIC_POWER
    if (owp->reg.need_pullup) {
      owp->reg.state = ONEWIRE_PULL_UP;
      owp->config->pullup_assert();
      owp->reg.need_pullup = false;
    }
    #endif

    osalSysLockFromISR();
    osalThreadResumeI(&owp->thread, MSG_OK);
    osalSysUnlockFromISR();
    return;
  }

  ow_write_bit_I(owp, (*owp->buf >> owp->reg.bit) & 1);
  owp->reg.bit++;
}

/**
 * @brief   Helper for collision handler
 */
static void store_bit(onewire_search_rom_t *sr, uint_fast8_t bit) {

  size_t rb = sr->reg.rombit;

  /*            /  8                % 8  */
  sr->retbuf[rb >> 3] |= bit << (rb & 7);
  sr->reg.rombit++;
}

/**
 * @brief   Helper for collision handler
 */
static uint_fast8_t extract_path_bit(const uint8_t *path, uint_fast8_t bit) {
  return (path[bit >> 3] >> (bit & 7)) & 1;
}

/**
 *
 */
static uint_fast8_t collision_handler(onewire_search_rom_t *sr) {

  uint_fast8_t bit;

  switch(sr->reg.search_iter) {
  case ONEWIRE_SEARCH_ROM_NEXT:
    if ((int)sr->reg.rombit < sr->last_zero_branch) {
      bit = extract_path_bit(sr->prev_path, sr->reg.rombit);
      if (0 == bit) {
        sr->prev_zero_branch = sr->reg.rombit;
        sr->reg.result = ONEWIRE_SEARCH_ROM_SUCCESS;
      }
      store_bit(sr, bit);
      return bit;
    }
    else if ((int)sr->reg.rombit == sr->last_zero_branch) {
      sr->last_zero_branch = sr->prev_zero_branch;
      store_bit(sr, 1);
      return 1;
    }
    else {
      /* found next branch some levels deeper */
      sr->prev_zero_branch = sr->last_zero_branch;
      sr->last_zero_branch = sr->reg.rombit;
      store_bit(sr, 0);
      sr->reg.result = ONEWIRE_SEARCH_ROM_SUCCESS;
      return 0;
    }
    break;

  case ONEWIRE_SEARCH_ROM_FIRST:
    /* always take 0-branch */
    sr->prev_zero_branch = sr->last_zero_branch;
    sr->last_zero_branch = sr->reg.rombit;
    store_bit(sr, 0);
    sr->reg.result = ONEWIRE_SEARCH_ROM_SUCCESS;
    return 0;
    break;

  default:
    osalSysHalt("Unhandled case");
    return 0; /* warning supressor */
    break;
  }
}

/**
 *
 */
static void ow_search_rom_cb(PWMDriver *pwmp, onewireDriver *owp) {

  chTMStartMeasurementX(&search_rom_tm);

  onewire_search_rom_t *sr = &owp->search_rom;

  if (0 == sr->reg.bit_step) {                    /* read direct bit */
    sr->reg.bit_buf |= owp->config->readBitX();
    sr->reg.bit_step++;
  }
  else if (1 == sr->reg.bit_step) {               /* read complement bit */
    sr->reg.bit_buf |= owp->config->readBitX() << 1;
    sr->reg.bit_step++;
    switch(sr->reg.bit_buf){
    case 0b11:
      /* no one device on bus */
      sr->reg.result = ONEWIRE_SEARCH_ROM_ERROR;
      goto THE_END;
      break;
    case 0b01:
      /* all slaves have 1 in this position */
      store_bit(sr, 1);
      ow_write_bit_I(owp, 1);
      break;
    case 0b10:
      /* all slaves have 0 in this position */
      store_bit(sr, 0);
      ow_write_bit_I(owp, 0);
      break;
    case 0b00:
      /* collision */
      sr->reg.single_device = false;
      ow_write_bit_I(owp, collision_handler(sr));
      break;
    }
  }
  else {                                      /* start next step */
#if !ONEWIRE_SYNTH_SEARCH_TEST
    ow_write_bit_I(owp, 1);
#endif
    sr->reg.bit_step = 0;
    sr->reg.bit_buf = 0;
  }

  /* one ROM successfully discovered */
  if (64 == sr->reg.rombit) {
    sr->reg.devices_found++;
    sr->reg.search_iter = ONEWIRE_SEARCH_ROM_NEXT;
    if (true == sr->reg.single_device)
      sr->reg.result = ONEWIRE_SEARCH_ROM_LAST;
    goto THE_END;
  }

  /* next search bit iteration */
  chTMStopMeasurementX(&search_rom_tm);
  return;

THE_END:
#if ONEWIRE_SYNTH_SEARCH_TEST
  (void)pwmp;
  return;
#else
  chTMStopMeasurementX(&search_rom_tm);
  osalSysLockFromISR();
  pwmDisableChannelI(pwmp, owp->config->master_channel);
  pwmDisableChannelI(pwmp, owp->config->sample_channel);
  osalThreadResumeI(&(owp)->thread, MSG_OK);
  osalSysUnlockFromISR();
#endif
}

/**
 * @brief   Early reset. Call it once before search rom routine.
 */
static void search_clean_start(onewire_search_rom_t *sr) {

  sr->reg.single_device = true; /* presume simplest way at beginning */
  sr->reg.result = ONEWIRE_SEARCH_ROM_LAST;
  sr->reg.search_iter = ONEWIRE_SEARCH_ROM_FIRST;
  sr->retbuf = NULL;
  sr->reg.devices_found = 0;
  memset(sr->prev_path, 0, 8);

  sr->reg.rombit = 0;
  sr->reg.bit_step = 0;
  sr->reg.bit_buf = 0;
  sr->last_zero_branch = -1;
  sr->prev_zero_branch = -1;
}

/**
 * @brief   Call it the begining of every iteration
 */
static void search_clean_iteration(onewire_search_rom_t *sr) {

  sr->reg.rombit = 0;
  sr->reg.bit_step = 0;
  sr->reg.bit_buf = 0;
  sr->reg.result = ONEWIRE_SEARCH_ROM_LAST;
}

/*
 ******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************
 */

/**
 *
 */
void onewireObjectInit(onewireDriver *owp) {

  osalDbgCheck(NULL != owp);

  owp->config = NULL;
  owp->reg.slave_present = false;
  owp->reg.state = ONEWIRE_STOP;
  owp->thread = NULL;

  owp->reg.bytes = 0;
  owp->reg.bit = 0;
  owp->reg.final_timeslot = false;
  owp->buf = NULL;

  owp->pwmcfg = pwm_default_cfg;

#if ONEWIRE_USE_PARASITIC_POWER
  owp->reg.need_pullup = false;
#endif
}

/**
 *
 */
void onewireStart(onewireDriver *owp, const onewireConfig *config) {

  osalDbgCheck((NULL != owp) && (NULL != config));
  osalDbgCheck(NULL != config->readBitX);
  osalDbgAssert(PWM_STOP == config->pwmd->state,
      "PWM will be started by onewire driver internally");
  osalDbgAssert(ONEWIRE_STOP == owp->reg.state, "Invalid state");
#if ONEWIRE_USE_PARASITIC_POWER
  osalDbgCheck((NULL != config->pullup_assert) &&
               (NULL != config->pullup_assert));
#endif

  owp->config = config;
  owp->reg.state = ONEWIRE_READY;
  chTMObjectInit(&search_rom_tm);
}

/**
 *
 */
void onewireStop(onewireDriver *owp) {
  osalDbgCheck(NULL != owp);
#if ONEWIRE_USE_PARASITIC_POWER
  owp->config->pullup_release();
#endif
  pwmStop(owp->config->pwmd);
  owp->config = NULL;
  owp->reg.state = ONEWIRE_STOP;
}

/**
 *
 */
uint8_t onewireCRC(const uint8_t *buf, size_t len) {
  uint8_t ret = 0;
  size_t i;

  for (i=0; i<len; i++)
    ret = onewire_crc_table[ret ^ buf[i]];

  return ret;
}

/**
 * @brief     Return true if device(s) detected on bus.
 */
bool onewireReset(onewireDriver *owp) {
  PWMDriver *pwmd;

  osalDbgCheck(NULL != owp);
  osalDbgAssert(owp->reg.state == ONEWIRE_READY, "Invalid state");

  /* short circuit on bus or any other device transmit data */
  if (0 == owp->config->readBitX())
    return false;

  pwmd = owp->config->pwmd;

  owp->pwmcfg.period = ONEWIRE_RESET_LOW_WIDTH + ONEWIRE_RESET_SAMPLE_WIDTH;
  owp->pwmcfg.callback = NULL;
  owp->pwmcfg.channels[owp->config->master_channel].callback = NULL;
  owp->pwmcfg.channels[owp->config->master_channel].mode = PWM_OUTPUT_ACTIVE_LOW;
  owp->pwmcfg.channels[owp->config->sample_channel].callback = pwm_reset_cb;
  owp->pwmcfg.channels[owp->config->sample_channel].mode = PWM_OUTPUT_ACTIVE_LOW;

  pwmStart(pwmd, &owp->pwmcfg);
  pwmEnableChannel(pwmd, owp->config->master_channel, ONEWIRE_RESET_LOW_WIDTH);
  pwmEnableChannel(pwmd, owp->config->sample_channel, ONEWIRE_RESET_SAMPLE_WIDTH);
  pwmEnableChannelNotification(pwmd, owp->config->sample_channel);

  osalSysLock();
  osalThreadSuspendS(&owp->thread);
  osalSysUnlock();

  pwmStop(pwmd);

  /* wait until slave release bus to discriminate it from short circuit */
  osalThreadSleepMicroseconds(500);
  return (1 == owp->config->readBitX()) && (true == owp->reg.slave_present);
}

/**
 *
 */
void onewireRead(onewireDriver *owp, uint8_t *rxbuf, size_t rxbytes) {
  PWMDriver *pwmd;

  osalDbgCheck((NULL != owp) && (NULL != rxbuf));
  osalDbgCheck((rxbytes > 0) && (rxbytes < 65536));
  osalDbgAssert(owp->reg.state == ONEWIRE_READY, "Invalid state");

  /* Buffer zeroing. This is important because of driver collects
     bits using |= operation.*/
  memset(rxbuf, 0, rxbytes);

  pwmd = owp->config->pwmd;

  owp->reg.bit = 0;
  owp->reg.final_timeslot = false;
  owp->buf = rxbuf;
  owp->reg.bytes = rxbytes;

  owp->pwmcfg.period = ONEWIRE_ZERO_WIDTH + ONEWIRE_RECOVERY_WIDTH;
  owp->pwmcfg.callback = NULL;
  owp->pwmcfg.channels[owp->config->master_channel].callback = NULL;
  owp->pwmcfg.channels[owp->config->master_channel].mode = PWM_OUTPUT_ACTIVE_LOW;
  owp->pwmcfg.channels[owp->config->sample_channel].callback = pwm_read_bit_cb;
  owp->pwmcfg.channels[owp->config->sample_channel].mode = PWM_OUTPUT_ACTIVE_LOW;

  pwmStart(pwmd, &owp->pwmcfg);
  pwmEnableChannel(pwmd, owp->config->master_channel, ONEWIRE_ONE_WIDTH);
  pwmEnableChannel(pwmd, owp->config->sample_channel, ONEWIRE_SAMPLE_WIDTH);
  pwmEnableChannelNotification(pwmd, owp->config->sample_channel);

  osalSysLock();
  osalThreadSuspendS(&owp->thread);
  osalSysUnlock();

  pwmStop(pwmd);
}

/**
 *
 */
void onewireWrite(onewireDriver *owp, uint8_t *txbuf,
                size_t txbytes, systime_t pullup_time) {
  PWMDriver *pwmd;

  osalDbgCheck((NULL != owp) && (NULL != txbuf));
  osalDbgCheck((txbytes > 0) && (txbytes < 65536));
  osalDbgAssert(owp->reg.state == ONEWIRE_READY, "Invalid state");
#if !ONEWIRE_USE_PARASITIC_POWER
  osalDbgAssert(0 == pullup_time,
      "Non zero time is valid only in parasitic power mode");
#endif

  pwmd = owp->config->pwmd;

  owp->buf = txbuf;
  owp->reg.bit = 0;
  owp->reg.final_timeslot = false;
  owp->reg.bytes = txbytes;

  owp->pwmcfg.period = ONEWIRE_ZERO_WIDTH + ONEWIRE_RECOVERY_WIDTH;
  owp->pwmcfg.callback = pwm_write_bit_cb;
  owp->pwmcfg.channels[owp->config->master_channel].callback = NULL;
  owp->pwmcfg.channels[owp->config->master_channel].mode = PWM_OUTPUT_ACTIVE_LOW;
  owp->pwmcfg.channels[owp->config->sample_channel].callback = NULL;
  owp->pwmcfg.channels[owp->config->sample_channel].mode = PWM_OUTPUT_DISABLED;

#if ONEWIRE_USE_PARASITIC_POWER
  if (pullup_time > 0) {
    owp->reg.state = ONEWIRE_PULL_UP;
    owp->reg.need_pullup = true;
  }
#endif

  pwmStart(pwmd, &owp->pwmcfg);
  pwmEnablePeriodicNotification(pwmd);

  osalSysLock();
  osalThreadSuspendS(&owp->thread);
  osalSysUnlock();

  pwmDisablePeriodicNotification(pwmd);
  pwmStop(pwmd);

#if ONEWIRE_USE_PARASITIC_POWER
  if (pullup_time > 0) {
    osalThreadSleep(pullup_time);
    owp->config->pullup_release();
    owp->reg.state = ONEWIRE_READY;
  }
#endif
}

/**
 * @brief   Perform tree search on bus.
 * @note    This function does internal 1-wire reset calls every search
 *          iteration.
 *
 * @param[in] owp         pointer to a @p OWDriver object
 * @param[out] result     pointer to buffer for founded ROMs
 * @param[in] max_rom_cnt buffer size in ROMs count for overflow prevention
 *
 * @return              Count of discovered ROMs. May be more than max_rom_cnt.
 * @retval 0            no ROMs found or communication error occurred.
 */
size_t onewireSearchRom(onewireDriver *owp, uint8_t *result, size_t max_rom_cnt) {
  PWMDriver *pwmd;
  uint8_t cmd;

  osalDbgCheck(NULL != owp);
  osalDbgAssert(ONEWIRE_READY == owp->reg.state, "Invalid state");
  osalDbgCheck((max_rom_cnt <= 256) && (max_rom_cnt > 0));

  pwmd = owp->config->pwmd;
  cmd = ONEWIRE_CMD_SEARCH_ROM;

  search_clean_start(&owp->search_rom);

  do {
    /* every search must be started from clean state */
    if (false == onewireReset(owp))
      return 0;

    /* initialize buffer to store result */
    if (owp->search_rom.reg.devices_found >= max_rom_cnt)
      owp->search_rom.retbuf = result + 8*(max_rom_cnt-1);
    else
      owp->search_rom.retbuf = result + 8*owp->search_rom.reg.devices_found;
    memset(owp->search_rom.retbuf, 0, 8);

    /* clean iteration state */
    search_clean_iteration(&owp->search_rom);

    /**/
    onewireWrite(&OWD1, &cmd, 1, 0);

    /* Reconfiguration always needed because of previous call onewireWrite.*/
    owp->pwmcfg.period = ONEWIRE_ZERO_WIDTH + ONEWIRE_RECOVERY_WIDTH;
    owp->pwmcfg.callback = NULL;
    owp->pwmcfg.channels[owp->config->master_channel].callback = NULL;
    owp->pwmcfg.channels[owp->config->master_channel].mode = PWM_OUTPUT_ACTIVE_LOW;
    owp->pwmcfg.channels[owp->config->sample_channel].callback = pwm_search_rom_cb;
    owp->pwmcfg.channels[owp->config->sample_channel].mode = PWM_OUTPUT_ACTIVE_LOW;
    pwmStart(pwmd, &owp->pwmcfg);
    pwmEnableChannel(pwmd, owp->config->master_channel, ONEWIRE_ONE_WIDTH);
    pwmEnableChannel(pwmd, owp->config->sample_channel, ONEWIRE_SAMPLE_WIDTH);
    pwmEnableChannelNotification(pwmd, owp->config->sample_channel);

    osalSysLock();
    osalThreadSuspendS(&owp->thread);
    osalSysUnlock();

    pwmStop(pwmd);

    if (ONEWIRE_SEARCH_ROM_ERROR != owp->search_rom.reg.result) {
      /* check CRC and return 0 (error status) if mismatch */
      if (owp->search_rom.retbuf[7] != onewireCRC(owp->search_rom.retbuf, 7))
        return 0;
      /* store cached result for usage in next iteration */
      memcpy(owp->search_rom.prev_path, owp->search_rom.retbuf, 8);
    }
  }
  while (ONEWIRE_SEARCH_ROM_SUCCESS == owp->search_rom.reg.result);

  /**/
  if (ONEWIRE_SEARCH_ROM_ERROR == owp->search_rom.reg.result)
    return 0;
  else
    return owp->search_rom.reg.devices_found;
}

#if ONEWIRE_SYNTH_SEARCH_TEST
#include "onewire_sr_synth.c"
#endif /* ONEWIRE_SYNTH_SEARCH_TEST */
