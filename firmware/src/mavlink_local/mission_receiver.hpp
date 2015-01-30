#ifndef MISSION_RECEIVER_HPP_
#define MISSION_RECEIVER_HPP_

class MissionReceiver : public chibios_rt::BaseStaticThread<512> {
public:
  MissionReceiver(void);
  msg_t main(void);

private:
  msg_t main_impl(void);
};

#endif /* MISSION_RECEIVER_HPP_ */
