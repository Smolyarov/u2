#pragma GCC optimize "-O2"
#pragma GCC optimize "-ffast-math"
#pragma GCC optimize "-funroll-loops"
#pragma GCC diagnostic ignored "-Wdouble-promotion"

#include <math.h>
#include "main.h"

#include "navigator_sins.hpp"
#include "kalman_flags.cpp" // dirty hack allowing to not add this file to the Makefile

#include "navi6d_wrapper.hpp"
#include "mavlink_local.hpp"
#include "mav_dbg.hpp"
#include "acs_input.hpp"
#include "geometry.hpp"
#include "time_keeper.hpp"
#include "param_registry.hpp"
#include "mav_logger.hpp"
#include "mav_postman.hpp"

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */

#define KALMAN_DEBUG_LOG          FALSE
#define DEBUG_INDEX_SINS          42

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */
extern mavlink_highres_imu_t          mavlink_out_highres_imu_struct;

#if KALMAN_DEBUG_LOG
extern MavLogger mav_logger;
#endif

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

typedef float klmnfp;
#define KALMAN_STATE_SIZE         15
#define KALMAN_MEASUREMENT_SIZE   10

__CCM__ static NavigatorSins<klmnfp, KALMAN_STATE_SIZE, KALMAN_MEASUREMENT_SIZE> nav_sins;

__CCM__ static mavlink_navi6d_debug_input_t   dbg_in_struct;
__CCM__ static mavlink_navi6d_debug_output_t  dbg_out_struct;
__CCM__ static mavMail dbg_in_mail;
__CCM__ static mavMail dbg_out_mail;

__CCM__ static mavlink_debug_vect_t dbg_gps_vel;
__CCM__ static mavlink_debug_vect_t dbg_acc_bias;
__CCM__ static mavlink_debug_vect_t dbg_gyr_bias;
__CCM__ static mavlink_debug_vect_t dbg_acc_scale;
__CCM__ static mavlink_debug_vect_t dbg_gyr_scale;
__CCM__ static mavMail mail_gps_vel;
__CCM__ static mavMail mail_acc_bias;
__CCM__ static mavMail mail_gyr_bias;
__CCM__ static mavMail mail_acc_scale;
__CCM__ static mavMail mail_gyr_scale;

__CCM__ static mavlink_debug_t dbg_sins_stat;
__CCM__ static mavMail mail_sins_stat;

__CCM__ static mavlink_debug_vect_t dbg_mag_data;
__CCM__ static mavMail mail_mag_data;

/*
 ******************************************************************************
 * PROTOTYPES
 ******************************************************************************
 */

/*
 *******************************************************************************
 *******************************************************************************
 * LOCAL FUNCTIONS
 *******************************************************************************
 *******************************************************************************
 */

/**
 *
 */
static void dbg_in_fill_gnss(const gnss::gnss_data_t &data) {

  dbg_in_struct.gnss_lat        = data.latitude;
  dbg_in_struct.gnss_lon        = data.longitude;
  dbg_in_struct.gnss_alt        = data.altitude;
  dbg_in_struct.gnss_course     = data.course;
  dbg_in_struct.gnss_fix_type   = data.fix;
  dbg_in_struct.gnss_fresh      = data.fresh;
  dbg_in_struct.gnss_speed_type = (uint8_t)data.speed_type;
  dbg_in_struct.gnss_speed      = data.speed;
  for (size_t i=0; i<3; i++) {
    dbg_in_struct.gnss_v[i]     = data.v[i];
  }

  dbg_in_struct.time_boot_ms    = TIME_BOOT_MS;
}

/**
 *
 */
static void dbg_in_fill_other(const baro_data_t &baro,
                              const odometer_data_t &odo,
                              const marg_data_t &marg) {

  dbg_in_struct.baro_alt  = baro.alt;
  dbg_in_struct.odo_speed = odo.speed;
  dbg_in_struct.marg_dt   = marg.dT;
  for (size_t i=0; i<3; i++) {
    dbg_in_struct.marg_acc[i] = marg.acc[i];
    dbg_in_struct.marg_gyr[i] = marg.gyr[i];
    dbg_in_struct.marg_mag[i] = marg.mag[i];
  }

  dbg_in_struct.time_boot_ms    = TIME_BOOT_MS;
}

/**
 *
 */
static void dbg_in_append_log(void) {
#if KALMAN_DEBUG_LOG
  if (dbg_in_mail.free()) {
    dbg_in_mail.fill(&dbg_in_struct, MAV_COMP_ID_ALL, MAVLINK_MSG_ID_NAVI6D_DEBUG_INPUT);
    mav_logger.write(&dbg_in_mail);
  }
#endif
}

/**
 *
 */
static void dbg_out_fill(const NaviData<klmnfp> &data) {

  dbg_out_struct.roll = data.eu_nv[0][0];
  dbg_out_struct.pitch= data.eu_nv[1][0];
  dbg_out_struct.yaw  = data.eu_nv[2][0];

  dbg_out_struct.lat  = data.r[0][0];
  dbg_out_struct.lon  = data.r[1][0];
  dbg_out_struct.alt  = data.r[2][0];

  dbg_out_struct.kalman_state_size  = KALMAN_STATE_SIZE;
  dbg_out_struct.kalman_meas_size   = KALMAN_MEASUREMENT_SIZE;

  dbg_out_struct.time_boot_ms = TIME_BOOT_MS;
}

/**
 *
 */
static void dbg_out_append_log(void) {
#if KALMAN_DEBUG_LOG
  if (dbg_out_mail.free()) {
    dbg_out_mail.fill(&dbg_out_struct, MAV_COMP_ID_ALL, MAVLINK_MSG_ID_NAVI6D_DEBUG_OUTPUT);
    mav_logger.write(&dbg_out_mail);
  }
#endif
}

/**
 *
 */
void Navi6dWrapper::navi2mavlink(void) {

  const NaviData<klmnfp> &data = nav_sins.navi_data;

  mavlink_out_highres_imu_struct.xacc = data.fb_c[0][0];
  mavlink_out_highres_imu_struct.yacc = data.fb_c[1][0];
  mavlink_out_highres_imu_struct.zacc = data.fb_c[2][0];

  mavlink_out_highres_imu_struct.xgyro = data.wb_c[0][0];
  mavlink_out_highres_imu_struct.ygyro = data.wb_c[1][0];
  mavlink_out_highres_imu_struct.zgyro = data.wb_c[2][0];

  mavlink_out_highres_imu_struct.xmag = data.mb_c[0][0];
  mavlink_out_highres_imu_struct.ymag = data.mb_c[1][0];
  mavlink_out_highres_imu_struct.zmag = data.mb_c[2][0];

  mavlink_out_highres_imu_struct.time_usec = TimeKeeper::utc();
}

/**
 *
 */
void Navi6dWrapper::debug2mavlink(float dT) {

  if (*T_debug_vect != TELEMETRY_SEND_OFF) {
    if (debug_vect_decimator < *T_debug_vect) {
      debug_vect_decimator += dT * 1000;
    }
    else {
      debug_vect_decimator = 0;
      uint64_t time = TimeKeeper::utc();

      //
      dbg_gps_vel.time_usec = time;
      dbg_gps_vel.x = round(100 * nav_sins.sensor_data.v_sns[0][0]);
      dbg_gps_vel.y = round(100 * nav_sins.sensor_data.v_sns[1][0]);
      dbg_gps_vel.z = round(100 * nav_sins.sensor_data.v_sns[2][0]);
      mail_gps_vel.fill(&dbg_gps_vel, MAV_COMP_ID_SYSTEM_CONTROL, MAVLINK_MSG_ID_DEBUG_VECT);
      mav_postman.post(mail_gps_vel);

      //
      dbg_acc_bias.time_usec = time;
      dbg_acc_bias.x = nav_sins.navi_data.a_bias[0][0];
      dbg_acc_bias.y = nav_sins.navi_data.a_bias[1][0];
      dbg_acc_bias.z = nav_sins.navi_data.a_bias[2][0];
      mail_acc_bias.fill(&dbg_acc_bias, MAV_COMP_ID_SYSTEM_CONTROL, MAVLINK_MSG_ID_DEBUG_VECT);
      mav_postman.post(mail_acc_bias);

      //
      dbg_gyr_bias.time_usec = time;
      dbg_gyr_bias.x = nav_sins.navi_data.w_bias[0][0];
      dbg_gyr_bias.y = nav_sins.navi_data.w_bias[1][0];
      dbg_gyr_bias.z = nav_sins.navi_data.w_bias[2][0];
      mail_gyr_bias.fill(&dbg_gyr_bias, MAV_COMP_ID_SYSTEM_CONTROL, MAVLINK_MSG_ID_DEBUG_VECT);
      mav_postman.post(mail_gyr_bias);

      //
      dbg_acc_scale.time_usec = time;
      dbg_acc_scale.x = nav_sins.navi_data.a_scale[0][0];
      dbg_acc_scale.y = nav_sins.navi_data.a_scale[1][1];
      dbg_acc_scale.z = nav_sins.navi_data.a_scale[2][2];
      mail_acc_scale.fill(&dbg_acc_scale, MAV_COMP_ID_SYSTEM_CONTROL, MAVLINK_MSG_ID_DEBUG_VECT);
      mav_postman.post(mail_acc_scale);

      //
      dbg_gyr_scale.time_usec = time;
      dbg_gyr_scale.x = nav_sins.navi_data.w_scale[0][0];
      dbg_gyr_scale.y = nav_sins.navi_data.w_scale[1][1];
      dbg_gyr_scale.z = nav_sins.navi_data.w_scale[2][2];
      mail_gyr_scale.fill(&dbg_gyr_scale, MAV_COMP_ID_SYSTEM_CONTROL, MAVLINK_MSG_ID_DEBUG_VECT);
      mav_postman.post(mail_gyr_scale);

      dbg_mag_data.time_usec = time;
      dbg_mag_data.x = nav_sins.navi_data.mag_head_v[0][0];
      dbg_mag_data.y = nav_sins.navi_data.mag_mod;
      dbg_mag_data.z = 0;
      mail_mag_data.fill(&dbg_mag_data, MAV_COMP_ID_SYSTEM_CONTROL, MAVLINK_MSG_ID_DEBUG_VECT);
      mav_postman.post(mail_mag_data);
    }
  }

  if (*T_debug != TELEMETRY_SEND_OFF) {
    if (debug_decimator < *T_debug) {
      debug_decimator += dT * 1000;
    }
    else {
      debug_decimator = 0;

      dbg_sins_stat.value = nav_sins.navi_data.status;
      dbg_sins_stat.ind = DEBUG_INDEX_SINS;
      dbg_sins_stat.time_boot_ms = TIME_BOOT_MS;
      mail_sins_stat.fill(&dbg_sins_stat, MAV_COMP_ID_SYSTEM_CONTROL, MAVLINK_MSG_ID_DEBUG);
      mav_postman.post(mail_sins_stat);
    }
  }
}

/**
 *
 */
void Navi6dWrapper::navi2acs(void) {

  const NaviData<klmnfp> &data = nav_sins.navi_data;

  acs_in.ch[ACS_INPUT_roll] = data.eu_nv[0][0];
  acs_in.ch[ACS_INPUT_pitch]= data.eu_nv[1][0];
  acs_in.ch[ACS_INPUT_yaw]  = data.eu_nv[2][0];

  acs_in.ch[ACS_INPUT_lat] = rad2deg(data.r[0][0]);
  acs_in.ch[ACS_INPUT_lon] = rad2deg(data.r[1][0]);
  acs_in.ch[ACS_INPUT_alt] = data.r[2][0];

  acs_in.ch[ACS_INPUT_vx] = data.v[0][0];
  acs_in.ch[ACS_INPUT_vy] = data.v[1][0];
  acs_in.ch[ACS_INPUT_vz] = data.v[2][0];

  acs_in.ch[ACS_INPUT_q0] = data.qnv[0][0];
  acs_in.ch[ACS_INPUT_q1] = data.qnv[1][0];
  acs_in.ch[ACS_INPUT_q2] = data.qnv[2][0];
  acs_in.ch[ACS_INPUT_q3] = data.qnv[3][0];

  acs_in.ch[ACS_INPUT_ax_body] = data.a_b[0][0];
  acs_in.ch[ACS_INPUT_ay_body] = data.a_b[1][0];
  acs_in.ch[ACS_INPUT_az_body] = data.a_b[2][0];

  acs_in.ch[ACS_INPUT_wx] = data.w_b[0][0];
  acs_in.ch[ACS_INPUT_wy] = data.w_b[1][0];
  acs_in.ch[ACS_INPUT_wz] = data.w_b[2][0];

  acs_in.ch[ACS_INPUT_free_ax] = data.free_accb[0][0];
  acs_in.ch[ACS_INPUT_free_ay] = data.free_accb[1][0];
  acs_in.ch[ACS_INPUT_free_az] = data.free_accb[2][0];
}

/*
 * Some functions was moved to this file to reduce copypasta size
 * between test and main code
 */
#include "navi6d_common.cpp"

/**
 *
 */
void Navi6dWrapper::start_time_measurement(void) {
  chTMStartMeasurementX(&tmeas);
}

/**
 *
 */
void Navi6dWrapper::stop_time_measurement(float dT) {

  chTMStopMeasurementX(&tmeas);
  time_meas_decimator += dT;
  if (tmeas.last / float(STM32_SYSCLK) > dT) {
    time_overrun_cnt++;
    if (time_meas_decimator > 0.25) {
      time_meas_decimator = 0;
      mavlink_dbg_print(MAV_SEVERITY_CRITICAL, "SINS time overrun!", MAV_COMP_ID_ALL);
    }
  }
}

/*
 *******************************************************************************
 * EXPORTED FUNCTIONS
 *******************************************************************************
 */
/**
 *
 */
Navi6dWrapper::Navi6dWrapper(ACSInput &acs_in, gnss::GNSSReceiver &GNSS) :
acs_in(acs_in),
GNSS(GNSS)
{
  chTMObjectInit(&tmeas);
  return;
}

/**
 *
 */
void Navi6dWrapper::start(void) {

  gps.fresh = false;
  GNSS.subscribe(&gps);

  read_settings();
  restart_cache = *restart + 1; // enforce sins restart in first update call

  /* we need to initialize names of fields manually because CCM RAM section
   * set to NOLOAD in chibios linker scripts */
  const size_t N = sizeof(mavlink_debug_vect_t::name);
  strncpy(dbg_gps_vel.name,   "gps_vel",   N);
  strncpy(dbg_acc_bias.name,  "acc_bias",  N);
  strncpy(dbg_gyr_bias.name,  "gyr_bias",  N);
  strncpy(dbg_acc_scale.name, "acc_scale", N);
  strncpy(dbg_gyr_scale.name, "gyr_scale", N);
  strncpy(dbg_mag_data.name,  "mag_data",  N);

  ready = true;
}

/**
 *
 */
void Navi6dWrapper::stop(void) {
  ready = false;
  GNSS.unsubscribe(&gps);
}

/**
 *
 */
void Navi6dWrapper::update(const baro_data_t &baro,
                           const odometer_data_t &odo,
                           const marg_data_t &marg) {
  osalDbgCheck(ready);

  start_time_measurement();

  /* reapply new dT if needed */
 /* if (this->dT_cache != marg.dT) {
    this->dT_cache = marg.dT;
    nav_sins.params.init_params.dT = marg.dT;
    nav_sins.params.init_params.rst_dT = 0.5;
  }*/

  /* restart sins if requested */
  if (*restart != restart_cache) {
    sins_cold_start();
    restart_cache = *restart;
  }

  nav_sins.init_params.dT = marg.dT;
  nav_sins.init_params.rst_dT = 0.1;

  nav_sins.kalman_params.sigma_R.gnss_n = *R_ne_sns; //ne_sns
  nav_sins.kalman_params.sigma_R.gnss_e = *R_ne_sns; //ne_sns
  nav_sins.kalman_params.sigma_R.gnss_d = *R_d_sns; //d_sns

  nav_sins.kalman_params.sigma_R.gnss_vn = *R_v_n_sns; //v_n_sns
  nav_sins.kalman_params.sigma_R.gnss_ve = *R_v_n_sns; //v_n_sns
  nav_sins.kalman_params.sigma_R.gnss_vd = *R_v_n_sns; //v_n_sns

  nav_sins.kalman_params.sigma_R.v_odo_x = *R_odo; //odo
  nav_sins.kalman_params.sigma_R.v_nhl_y = *R_nhl_y; //nonhol
  nav_sins.kalman_params.sigma_R.v_nhl_z = *R_nhl_z; //nonhol

  nav_sins.kalman_params.sigma_R.alt_baro = *R_baro; //baro

  nav_sins.kalman_params.sigma_R.mag_x = *R_mag; //mag
  nav_sins.kalman_params.sigma_R.mag_y = *R_mag; //mag
  nav_sins.kalman_params.sigma_R.mag_z = *R_mag; //mag

  nav_sins.kalman_params.sigma_R.roll  = *R_euler; //roll,pitch,yaw (rad)
  nav_sins.kalman_params.sigma_R.pitch = *R_euler; //roll,pitch,yaw (rad)
  nav_sins.kalman_params.sigma_R.yaw   = *R_euler; //roll,pitch,yaw (rad)

  nav_sins.kalman_params.sigma_R.v_nav_static = *R_v_nav_st; //roll,pitch,yaw (rad)
  nav_sins.kalman_params.sigma_R.v_veh_static = *R_v_veh_st; //roll,pitch,yaw (rad)
  nav_sins.kalman_params.sigma_R.yaw_static   = *R_yaw_st; //roll,pitch,yaw (rad)
  nav_sins.kalman_params.sigma_R.yaw_mag      = *R_mag_yaw; //roll,pitch,yaw (rad)

  nav_sins.kalman_params.sigma_Qm.acc_x = *Qm_acc; //acc
  nav_sins.kalman_params.sigma_Qm.acc_y = *Qm_acc; //acc
  nav_sins.kalman_params.sigma_Qm.acc_z = *Qm_acc; //acc

  nav_sins.kalman_params.sigma_Qm.gyr_x = *Qm_gyr; //gyr
  nav_sins.kalman_params.sigma_Qm.gyr_y = *Qm_gyr; //gyr
  nav_sins.kalman_params.sigma_Qm.gyr_z = *Qm_gyr; //gyr

  nav_sins.kalman_params.sigma_Qm.acc_b_x = *Qm_acc_x; //acc_x
  nav_sins.kalman_params.sigma_Qm.acc_b_y = *Qm_acc_y; //acc_y
  nav_sins.kalman_params.sigma_Qm.acc_b_z = *Qm_acc_z; //acc_z

  nav_sins.kalman_params.sigma_Qm.gyr_b_x = *Qm_gyr_bias; //gyr_bias
  nav_sins.kalman_params.sigma_Qm.gyr_b_y = *Qm_gyr_bias; //gyr_bias
  nav_sins.kalman_params.sigma_Qm.gyr_b_z = *Qm_gyr_bias; //gyr_bias

  nav_sins.kalman_params.sigma_P.n = *P_ned;
  nav_sins.kalman_params.sigma_P.e = *P_ned;
  nav_sins.kalman_params.sigma_P.d = *P_ned;

  nav_sins.kalman_params.Beta_inv.acc_b = *B_acc_b;
  nav_sins.kalman_params.Beta_inv.gyr_b = *B_gyr_b;

  nav_sins.kalman_params.sigma_P.acc_b_x = *P_acc_b;
  nav_sins.kalman_params.sigma_P.acc_b_y = *P_acc_b;
  nav_sins.kalman_params.sigma_P.acc_b_z = *P_acc_b;

  nav_sins.kalman_params.sigma_P.gyr_b_x = *P_gyr_b;
  nav_sins.kalman_params.sigma_P.gyr_b_y = *P_gyr_b;
  nav_sins.kalman_params.sigma_P.gyr_b_z = *P_gyr_b;

  nav_sins.calib_params.ba[0][0] = *acc_bias_x;
  nav_sins.calib_params.ba[1][0] = *acc_bias_y;
  nav_sins.calib_params.ba[2][0] = *acc_bias_z;

  nav_sins.calib_params.bw[0][0] = *gyr_bias_x;
  nav_sins.calib_params.bw[1][0] = *gyr_bias_y;
  nav_sins.calib_params.bw[2][0] = *gyr_bias_z;

  nav_sins.calib_params.sa[0][0] = *acc_scale_x;
  nav_sins.calib_params.sa[1][0] = *acc_scale_y;
  nav_sins.calib_params.sa[2][0] = *acc_scale_z;

  nav_sins.calib_params.sw[0][0] = *gyr_scale_x;
  nav_sins.calib_params.sw[1][0] = *gyr_scale_y;
  nav_sins.calib_params.sw[2][0] = *gyr_scale_z;

  nav_sins.calib_params.no_a[0][0] = *acc_nort_0;
  nav_sins.calib_params.no_a[1][0] = *acc_nort_1;
  nav_sins.calib_params.no_a[2][0] = *acc_nort_2;
  nav_sins.calib_params.no_a[3][0] = *acc_nort_3;
  nav_sins.calib_params.no_a[4][0] = *acc_nort_4;
  nav_sins.calib_params.no_a[5][0] = *acc_nort_5;

  nav_sins.calib_params.no_w[0][0] = *gyr_nort_0;
  nav_sins.calib_params.no_w[1][0] = *gyr_nort_1;
  nav_sins.calib_params.no_w[2][0] = *gyr_nort_2;
  nav_sins.calib_params.no_w[3][0] = *gyr_nort_3;
  nav_sins.calib_params.no_w[4][0] = *gyr_nort_4;
  nav_sins.calib_params.no_w[5][0] = *gyr_nort_5;

  nav_sins.calib_params.bm[0][0] = -3.79611/1000;
  nav_sins.calib_params.bm[1][0] = 15.2098/1000;
  nav_sins.calib_params.bm[2][0] = -5.45266/1000;

  nav_sins.calib_params.m_s[0][0] = 0.916692;
  nav_sins.calib_params.m_s[1][0] = 0.912;
  nav_sins.calib_params.m_s[2][0] = 0.9896;

  nav_sins.calib_params.m_no[0][0] = -0.0031;
  nav_sins.calib_params.m_no[1][0] = 0.0078;
  nav_sins.calib_params.m_no[2][0] = 0.0018;

  nav_sins.ref_params.mag_eu_bs[0][0] = M_PI;
  nav_sins.ref_params.mag_eu_bs[1][0] = 0.0;
  nav_sins.ref_params.mag_eu_bs[2][0] = -M_PI;

  nav_sins.ref_params.eu_vh_base[0][0] = *eu_vh_roll;
  nav_sins.ref_params.eu_vh_base[1][0] = *eu_vh_pitch;
  nav_sins.ref_params.eu_vh_base[2][0] = *eu_vh_yaw;

  nav_sins.ref_params.eu_vh_base[0][0] = *eu_vh_roll;
  nav_sins.ref_params.eu_vh_base[1][0] = *eu_vh_pitch;
  nav_sins.ref_params.eu_vh_base[2][0] = *eu_vh_yaw;

  nav_sins.calib_params.ba[0][0] = *acc_bias_x;
  nav_sins.calib_params.ba[1][0] = *acc_bias_y;
  nav_sins.calib_params.ba[2][0] = *acc_bias_z;

  nav_sins.calib_params.bw[0][0] = *gyr_bias_x;
  nav_sins.calib_params.bw[1][0] = *gyr_bias_y;
  nav_sins.calib_params.bw[2][0] = *gyr_bias_z;

  nav_sins.calib_params.sa[0][0] = *acc_scale_x;
  nav_sins.calib_params.sa[1][0] = *acc_scale_y;
  nav_sins.calib_params.sa[2][0] = *acc_scale_z;

  nav_sins.calib_params.sw[0][0] = *gyr_scale_x;
  nav_sins.calib_params.sw[1][0] = *gyr_scale_y;
  nav_sins.calib_params.sw[2][0] = *gyr_scale_z;

  nav_sins.ref_params.zupt_source = *zupt_src;
  nav_sins.ref_params.glrt_gamma = *gamma;
  nav_sins.ref_params.glrt_acc_sigma = *acc_sigma;
  nav_sins.ref_params.glrt_gyr_sigma = *gyr_sigma;
  nav_sins.ref_params.glrt_n = *samples;
  nav_sins.init_params.fog_en = false;

  dbg_in_fill_gnss(this->gps);
  prepare_data_gnss(this->gps);
  dbg_in_fill_other(baro, odo, marg);
  dbg_in_append_log();
  prepare_data(baro, odo, marg);

  nav_sins.run();

  navi2acs();
  navi2mavlink();
  debug2mavlink(marg.dT);

  dbg_out_fill(nav_sins.navi_data);
  dbg_out_append_log();

  stop_time_measurement(marg.dT);
}

