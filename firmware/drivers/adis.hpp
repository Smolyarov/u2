#ifndef ADIS_HPP_
#define ADIS_HPP_

#include "sensor.hpp"

typedef struct {
  float acc[3];
  float gyr[3];
  float mag[3];
  float baro;
  float quat[4];
  float euler[3];
  float temp;
  uint16_t errors;
} adis_data_t;

class Adis : public Sensor {
public:
  Adis(chibios_rt::BinarySemaphore &data_ready_sem);
  void stop(void);
  void sleep(void);
  sensor_state_t start(void);
  sensor_state_t wakeup(void);
  sensor_state_t get(adis_data_t *result);
  float dt(void);
  static void extiISR(EXTDriver *extp, expchannel_t channel);
  msg_t wait(systime_t timeout);

private:
  friend THD_FUNCTION(AdisThread, arg);
  void set_sample_rate(void);
  void param_update(void);
  void acquire_data(void);
  bool hw_init_fast(void);
  bool hw_init_full(void);
  void set_lock(void);
  void release_lock(void);

  time_measurement_t tm;
  chibios_rt::BinarySemaphore protect_sem;
  static chibios_rt::BinarySemaphore isr_sem;
  chibios_rt::BinarySemaphore &data_ready_sem;
  thread_t *worker;
  adis_data_t measurement;
  const uint32_t *smplrtdiv = NULL;
  uint8_t smplrtdiv_prev;
};

#endif /* ADIS_HPP_ */
