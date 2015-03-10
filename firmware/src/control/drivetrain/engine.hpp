#ifndef ENGINE_HPP_
#define ENGINE_HPP_

#include "drivetrain_impact.hpp"
#include "drivetrain/drivetrain_pwm.hpp"

namespace control {

class Engine {
public:
  Engine(PWM &pwm);
  void start(void);
  void stop(void);
  void update(const DrivetrainImpact &impact);
private:
  PWM &pwm;
  bool ready = false;
  const uint32_t *thr_min = nullptr, *thr_mid = nullptr, *thr_max = nullptr;
};

} /* namespace */

#endif /* ENGINE_HPP_ */
