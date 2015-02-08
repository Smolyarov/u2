#ifndef NMEA_HPP_
#define NMEA_HPP_

/* From standard: A sentence may contain up to 80 characters plus "$" and CR/LF */
#define GPS_MSG_LEN           84
#define GPS_TOKEN_MAP_LEN     24
#define GPS_MAX_TOKEN_LEN     16

namespace gps {

/**
 *
 */
typedef struct {
  double  latitude;
  double  longitude;
  float   altitude;
  float   hdop;
  float   geoid;
  uint8_t satellites;
  uint8_t fix;
} nmea_gga_t;

/**
 *
 */
typedef struct {
  struct tm time;
  float     speed;
  float     course;
} nmea_rmc_t;

/**
 *
 */
enum class collect_status_t {
  EMPTY,
  GPGGA,
  GPRMC,
  UNKNOWN
};

/**
 *
 */
enum class collect_state_t {
  START,      /* wait '$' sign */
  DATA,       /* collect data until '*' sign */
  CHECKSUM1,  /* collect 1st byte of checksum */
  CHECKSUM2,  /* collect 2nd byte of checksum */
  EOL_CR,     /* wait CR symbol */
  EOL_LF      /* wait LF symbol */
};

/**
 *
 */
class NmeaParser {
public:
  NmeaParser(void);
  collect_status_t collect(uint8_t byte);
  void unpack(nmea_rmc_t *result);
  void unpack(nmea_gga_t *result);
private:
  void reset_collector(void);
  const char* token(char *result, size_t number);
  collect_status_t verify_sentece(void);
  collect_status_t get_name(const char *name);
  size_t tip;
  size_t maptip;
  collect_state_t state;
  uint8_t buf[GPS_MSG_LEN];
  uint8_t token_map[GPS_TOKEN_MAP_LEN];
};

} /* namespace */

//gps::collect_status_t nmea_test_gga(void);
//gps::collect_status_t nmea_test_rmc(void);
//gps::collect_status_t nmea_benchmark(void);

#endif /* NMEA_HPP_ */
