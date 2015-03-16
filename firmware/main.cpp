/*
Hi,
I also had the same problem interfacing to an I2C device. However I solved the issue by changing the relevant outputs to open drain by editing the board.h file. Therefore palSetPadMode(i2c_gpio->sda_gpio_port, i2c_gpio->sda_gpio, PAL_MODE_ALTERNATE(4)); is still correct if your board.h file configurse these pins as opendrain.
However, I set my pins to alternate mode in board.h, which is processed before the call to i2cStart(). This works but is there any negative effect from connecting to a alternative peripheral before activating it?
Pilt

Hi,
Setting the alternate function before activating the peripheral can create problems.
For example, the USART TX line is set to zero until the clock is activated and this creates a spurious start bit if the alternate is set in board.h (before the driver is activated).
The safest way to initialize peripherals like USART and I2C is the following (I2C case).
1) In board.h set the pins as *normal* outputs with open drain and an acceptable idle logic state (idle is one for I2C if I remember well).
2) Start the peripheral clock using i2cStart(), the pins are still normal outputs and keep their idle state.
3) Now switch to alternate(4) with open drain and the pins state will go from idle to idle, no transients.
Giovanni
*/

// TODO: save DCM in bkp for faster startup after panic recovery
// TODO: params in bkp
// TODO: gyro update period in bkp
// TODO: save mission data in bkp for recovery if panic occured during mission

// TODO: cli for format, ls, rm

// TODO: rewrite stab code in general case using aviation formulae.
// TODO: probably migrate from float32 to double in coordinate calculations.

// TODO: correct STOP handling in waypoint algorithm (incorrect realization in QGC)

// TODO: speed autosetting
// TODO: combine barometer and accelerometer in one filter.
// TODO: Power brown out handler (using ADC comparator on power supply pin?) for sync/umout SDC.
// TODO: One more point in dynamic pressure thermal compensation algorithm (at +60 celsius)
// TODO: Rewrite XBee code for use DMA.
// TODO: WDT?

#include "main.h"

#include "global_flags.h"
#include "fault_handlers.h"
#include "mavlink_local.hpp"
#include "gps_eb500.hpp"
#include "sanity.hpp"
#include "i2c_local.hpp"
#include "nvram_local.hpp"
#include "param_receiver.hpp"
#include "time_keeper.hpp"
#include "bmp085.hpp"
#include "tlm_sender.hpp"
#include "link_mgr.hpp"
//#include "controller.hpp"
//#include "mav_dispatcher.hpp"
//#include "cmd_executor.hpp"
#include "blinker.hpp"
#include "waypoint_db.hpp"
#include "mission_receiver.hpp"
#include "mavlink_local.hpp"
#include "endianness.h"
//#include "attitude_unit_rover.hpp"
#include "acs.hpp"
#include "stabilizer/stabilizer.hpp"
#include "drivetrain/drivetrain.hpp"
#include "exti_local.hpp"
#include "ahrs.hpp"
#include "mav_logger.hpp"
#include "adc_local.hpp"
#include "pwr_mgr.hpp"

using namespace chibios_rt;

/* cheat sheet for use in other files */
#pragma GCC optimize "-funroll-loops"
#pragma GCC optimize "-O2"

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */

/* reset all global flags */
GlobalFlags_t GlobalFlags = {0,0,0,0,0,0,0,0,
                             0,0,0,0,0,0,0,0,
                             0,0,0,0,0,0,0,0,
                             0,0,0,0,0,0,0,0};

/* heap for temporarily threads */
memory_heap_t ThdHeap;
static uint8_t link_thd_buf[THREAD_HEAP_SIZE + sizeof(stkalign_t)];

///* save here flags before clear them from MCU register */
//uint32_t LastResetFlags;

/* State vector of system. Calculated mostly in IMU, used mostly in ACS */
StateVector state_vector __attribute__((section(".ccm")));

control::Drivetrain drivetrain;

//MARGRover marg;
//AttitudeUnitRover attitude_unit(0.01f, state_vector);

/* automated control system */
control::ACS acs(drivetrain, state_vector);

///* automated control system */
//SINS sins;
//
///**/
//Drivetrain drivetrain(impact);
//
///**/
//MavDispatcher mav_dispatcher(acs);
//
//CmdExecutor cmd_executor(acs, attitude_unit);

MissionReceiver mission_receiver;
sensor_state_registry_t SensorStateRegistry;
TimeKeeper time_keeper;
TlmSender tlm_sender;
static LinkMgr link_mgr;
MavLogger mav_logger;
Ahrs ahrs;
BMP085 bmp_085(&I2CD_SLOW, bmp085addr);

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

/*
 *******************************************************************************
 *******************************************************************************
 * LOCAL FUNCTIONS
 *******************************************************************************
 *******************************************************************************
 */

#include "maxsonar.hpp"
#include "pps.hpp"
#include "speedometer.hpp"
#include "gps_eb500.hpp"
static MaxSonar maxsonar;

static Speedometer speedometer;
float speed;
uint32_t path;

static gps::gps_data_t gps_data __attribute__((section(".ccm")));
static ahrs_data_t ahrs_data __attribute__((section(".ccm")));

static PPS pps;


int main(void) {

  halInit();
  System::init();

  endianness_test();
  chThdSleepMilliseconds(300);

  /* enable softreset on panic */
  setGlobalFlag(GlobalFlags.allow_softreset);
  if (was_softreset() || was_padreset())
    chThdSleepMilliseconds(1);
  else
    chThdSleepMilliseconds(200);

  /* give power to all needys */
  ADCInitLocal();
  gps_power_on();
  xbee_reset_clear();
  eeprom_power_on();
  osalThreadSleepMilliseconds(10);

  chHeapObjectInit(&ThdHeap, (uint8_t *)MEM_ALIGN_NEXT(link_thd_buf), THREAD_HEAP_SIZE);

  Exti.start();
  time_keeper.start();
  blinker.start();
  SanityControlInit();
  I2CInitLocal();
  NvramInit();
  ParametersInit();   /* read parameters from EEPROM via I2C */

  PwrMgrInit();
  if (PwrMgr6vGood())
    pwr5v_power_on();

  MavlinkInit();      /* mavlink constants initialization must be called after parameters init */
  mission_receiver.start(CONTROLLERPRIO);
//  ControllerInit();
  link_mgr.start();      /* launch after controller to reduce memory fragmentation on thread creation */
//  PwrMgmtInit();
  tlm_sender.start();

//  /**/
//  LastResetFlags = RCC->CSR;
//  clear_reset_flags();
//
//  /* main cycle */
//  attitude_unit.start();
//  acs.start();
//  sins.start(&state_vector);
  bmp_085.start();
  GPSInit();
  mav_logger.start(NORMALPRIO);

  ahrs.start();
  maxsonar.start();
  speedometer.start();
  acs.start();
  pps.start();

  while (TRUE) {
    ahrs.get(ahrs_data, MS2ST(200));
    GPSGetData(gps_data);
    speedometer.update(speed, path, ahrs_data.dt);
    acs.update(ahrs_data.dt);

    PwrMgrUpdate();

    //osalThreadSleepMilliseconds(200);
//    if (ATTITUDE_UNIT_UPDATE_RESULT_OK == attitude_unit.update()){
//      sins.update();
//      if (ACS_STATUS_ERROR == acs.update())
//        chDbgPanic("ACS. Broken.");
//      drivetrain.update();
//    }
  }

  return 0;
}



