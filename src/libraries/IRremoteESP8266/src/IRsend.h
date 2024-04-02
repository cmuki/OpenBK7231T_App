// Copyright 2009 Ken Shirriff
// Copyright 2015 Mark Szabo
// Copyright 2017 David Conran
#ifndef IRSEND_H_
#define IRSEND_H_

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "IRremoteESP8266.h"

// Originally from https://github.com/shirriff/Arduino-IRremote/
// Updated by markszabo (https://github.com/crankyoldgit/IRremoteESP8266) for
// sending IR code on ESP8266


// Constants
// Offset (in microseconds) to use in Period time calculations to account for
// code excution time in producing the software PWM signal.
#if defined(ESP32)
// Calculated on a generic ESP-WROOM-32 board with v3.2-18 SDK @ 240MHz
const int8_t kPeriodOffset = -2;
#elif (defined(ESP8266) && F_CPU == 160000000L)  // NOLINT(whitespace/parens)
// Calculated on an ESP8266 NodeMCU v2 board using:
// v2.6.0 with v2.5.2 ESP core @ 160MHz
const int8_t kPeriodOffset = -2;
#else  // (defined(ESP8266) && F_CPU == 160000000L)
// Calculated on ESP8266 Wemos D1 mini using v2.4.1 with v2.4.0 ESP core @ 40MHz
const int8_t kPeriodOffset = -5;
#endif  // (defined(ESP8266) && F_CPU == 160000000L)




const uint8_t kDutyDefault = 50;  // Percentage
const uint8_t kDutyMax = 100;     // Percentage
// delayMicroseconds() is only accurate to 16383us.
// Ref: https://www.arduino.cc/en/Reference/delayMicroseconds
const uint16_t kMaxAccurateUsecDelay = 16383;
//  Usecs to wait between messages we don't know the proper gap time.
const uint32_t kDefaultMessageGap = 100000;
/// Placeholder for missing sensor temp value
/// @note Not using "-1" as it may be a valid external temp
const float kNoTempValue = -100.0;

/// Enumerators and Structures for the Common A/C API.
namespace stdAc {
/// Common A/C settings for A/C operating modes.
enum class opmode_t {
  kOff  = -1,
  kAuto =  0,
  kCool =  1,
  kHeat =  2,
  kDry  =  3,
  kFan  =  4,
  // Add new entries before this one, and update it to point to the last entry
  kLastOpmodeEnum = kFan,
};

/// Common A/C settings for Fan Speeds.
enum class fanspeed_t {
  kAuto =       0,
  kMin =        1,
  kLow =        2,
  kMedium =     3,
  kHigh =       4,
  kMax =        5,
  kMediumHigh = 6,
  // Add new entries before this one, and update it to point to the last entry
  kLastFanspeedEnum = kMediumHigh,
};

/// Common A/C settings for Vertical Swing.
enum class swingv_t {
  kOff =    -1,
  kAuto =    0,
  kHighest = 1,
  kHigh =    2,
  kMiddle =  3,
  kLow =     4,
  kLowest =  5,
  kUpperMiddle = 6,
  // Add new entries before this one, and update it to point to the last entry
  kLastSwingvEnum = kUpperMiddle,
};

/// @brief Tyoe of A/C command (if the remote uses different codes for each)
/// @note Most remotes support only a single command or aggregate multiple
///       into one (e.g. control+timer). Use @c kControlCommand in such case
enum class ac_command_t {
  kControlCommand = 0,
  kSensorTempReport = 1,
  kTimerCommand = 2,
  kConfigCommand = 3,
  // Add new entries before this one, and update it to point to the last entry
  kLastAcCommandEnum = kConfigCommand,
};

/// Common A/C settings for Horizontal Swing.
enum class swingh_t {
  kOff =     -1,
  kAuto =     0,  // a.k.a. On.
  kLeftMax =  1,
  kLeft =     2,
  kMiddle =   3,
  kRight =    4,
  kRightMax = 5,
  kWide =     6,  // a.k.a. left & right at the same time.
  // Add new entries before this one, and update it to point to the last entry
  kLastSwinghEnum = kWide,
};

/// Structure to hold a common A/C state.
struct state_t {
  decode_type_t protocol = decode_type_t::UNKNOWN;
  int16_t model = -1;  // `-1` means unused.
  bool power = false;
  stdAc::opmode_t mode = stdAc::opmode_t::kOff;
  float degrees = 25;
  bool celsius = true;
  stdAc::fanspeed_t fanspeed = stdAc::fanspeed_t::kAuto;
  stdAc::swingv_t swingv = stdAc::swingv_t::kOff;
  stdAc::swingh_t swingh = stdAc::swingh_t::kOff;
  bool quiet = false;
  bool turbo = false;
  bool econo = false;
  bool light = false;
  bool filter = false;
  bool clean = false;
  bool beep = false;
  int16_t sleep = -1;  // `-1` means off.
  int16_t clock = -1;  // `-1` means not set.
  stdAc::ac_command_t command = stdAc::ac_command_t::kControlCommand;
  bool iFeel = false;
  float sensorTemperature = kNoTempValue;  // `kNoTempValue` means not set.
};
};  // namespace stdAc

// Classes

/// Class for sending all basic IR protocols.
/// @note Originally from https://github.com/shirriff/Arduino-IRremote/
///  Updated by markszabo (https://github.com/crankyoldgit/IRremoteESP8266) for
///  sending IR code on ESP8266
class IRsend {
 public:
  explicit IRsend(uint16_t IRsendPin, bool inverted = false,
                  bool use_modulation = true);
  void begin();
  virtual void enableIROut(uint32_t freq, uint8_t duty = kDutyDefault);

  virtual void _delayMicroseconds(uint32_t usec);
  virtual uint16_t mark(uint16_t usec);
  virtual void space(uint32_t usec);

  int8_t calibrate(uint16_t hz = 38000U);
  void sendRaw(const uint16_t buf[], const uint16_t len, const uint16_t hz);
  void sendData(uint16_t onemark, uint32_t onespace, uint16_t zeromark,
                uint32_t zerospace, uint64_t data, uint16_t nbits,
                bool MSBfirst = true);
  void sendManchesterData(const uint16_t half_period, const uint64_t data,
                          const uint16_t nbits, const bool MSBfirst = true,
                          const bool GEThomas = true);
  void sendManchester(const uint16_t headermark, const uint32_t headerspace,
                      const uint16_t half_period, const uint16_t footermark,
                      const uint32_t gap, const uint64_t data,
                      const uint16_t nbits, const uint16_t frequency = 38,
                      const bool MSBfirst = true,
                      const uint16_t repeat = kNoRepeat,
                      const uint8_t dutycycle = kDutyDefault,
                      const bool GEThomas = true);
  void sendGeneric(const uint16_t headermark, const uint32_t headerspace,
                   const uint16_t onemark, const uint32_t onespace,
                   const uint16_t zeromark, const uint32_t zerospace,
                   const uint16_t footermark, const uint32_t gap,
                   const uint64_t data, const uint16_t nbits,
                   const uint16_t frequency, const bool MSBfirst,
                   const uint16_t repeat, const uint8_t dutycycle);
  void sendGeneric(const uint16_t headermark, const uint32_t headerspace,
                   const uint16_t onemark, const uint32_t onespace,
                   const uint16_t zeromark, const uint32_t zerospace,
                   const uint16_t footermark, const uint32_t gap,
                   const uint32_t mesgtime, const uint64_t data,
                   const uint16_t nbits, const uint16_t frequency,
                   const bool MSBfirst, const uint16_t repeat,
                   const uint8_t dutycycle);
  void sendGeneric(const uint16_t headermark, const uint32_t headerspace,
                   const uint16_t onemark, const uint32_t onespace,
                   const uint16_t zeromark, const uint32_t zerospace,
                   const uint16_t footermark, const uint32_t gap,
                   const uint8_t *dataptr, const uint16_t nbytes,
                   const uint16_t frequency, const bool MSBfirst,
                   const uint16_t repeat, const uint8_t dutycycle);
  static uint16_t minRepeats(const decode_type_t protocol);
  static uint16_t defaultBits(const decode_type_t protocol);
  bool send(const decode_type_t type, const uint64_t data,
            const uint16_t nbits, const uint16_t repeat = kNoRepeat);
  bool send(const decode_type_t type, const uint8_t *state,
            const uint16_t nbytes);
#if SEND_NEC
  void sendNEC(uint64_t data, uint16_t nbits = kNECBits,
               uint16_t repeat = kNoRepeat);
  uint32_t encodeNEC(uint16_t address, uint16_t command);
#endif
#if SEND_SAMSUNG
  void sendSAMSUNG(const uint64_t data, const uint16_t nbits = kSamsungBits,
                   const uint16_t repeat = kNoRepeat);
  uint32_t encodeSAMSUNG(const uint8_t customer, const uint8_t command);
#endif
#if SEND_SAMSUNG36
  void sendSamsung36(const uint64_t data, const uint16_t nbits = kSamsung36Bits,
                     const uint16_t repeat = kNoRepeat);
#endif
#if SEND_DAIKIN
  void sendDaikin(const unsigned char data[],
                  const uint16_t nbytes = kDaikinStateLength,
                  const uint16_t repeat = kDaikinDefaultRepeat);
#endif
#if SEND_DAIKIN64
  void sendDaikin64(const uint64_t data, const uint16_t nbits = kDaikin64Bits,
                    const uint16_t repeat = kDaikin64DefaultRepeat);
#endif  // SEND_DAIKIN64
#if SEND_DAIKIN128
  void sendDaikin128(const unsigned char data[],
                     const uint16_t nbytes = kDaikin128StateLength,
                     const uint16_t repeat = kDaikin128DefaultRepeat);
#endif  // SEND_DAIKIN128
#if SEND_DAIKIN152
  void sendDaikin152(const unsigned char data[],
                     const uint16_t nbytes = kDaikin152StateLength,
                     const uint16_t repeat = kDaikin152DefaultRepeat);
#endif  // SEND_DAIKIN152
#if SEND_DAIKIN160
  void sendDaikin160(const unsigned char data[],
                     const uint16_t nbytes = kDaikin160StateLength,
                     const uint16_t repeat = kDaikin160DefaultRepeat);
#endif  // SEND_DAIKIN160
#if SEND_DAIKIN176
  void sendDaikin176(const unsigned char data[],
                     const uint16_t nbytes = kDaikin176StateLength,
                     const uint16_t repeat = kDaikin176DefaultRepeat);
#endif  // SEND_DAIKIN176
#if SEND_DAIKIN2
  void sendDaikin2(const unsigned char data[],
                   const uint16_t nbytes = kDaikin2StateLength,
                   const uint16_t repeat = kDaikin2DefaultRepeat);
#endif
#if SEND_DAIKIN200
  void sendDaikin200(const unsigned char data[],
                     const uint16_t nbytes = kDaikin200StateLength,
                     const uint16_t repeat = kDaikin200DefaultRepeat);
#endif  // SEND_DAIKIN200
#if SEND_DAIKIN216
  void sendDaikin216(const unsigned char data[],
                     const uint16_t nbytes = kDaikin216StateLength,
                     const uint16_t repeat = kDaikin216DefaultRepeat);
#endif  // SEND_DAIKIN216
#if SEND_DAIKIN312
  void sendDaikin312(const unsigned char data[],
                     const uint16_t nbytes = kDaikin312StateLength,
                     const uint16_t repeat = kDaikin312DefaultRepeat);
#endif  // SEND_DAIKIN312

 protected:
#ifdef UNIT_TEST
#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif
#endif  // UNIT_TEST
  uint8_t outputOn;
  uint8_t outputOff;
  virtual void ledOff();
  virtual void ledOn();
#ifndef UNIT_TEST

 private:
#else
  uint32_t _freq_unittest;
#endif  // UNIT_TEST
  uint16_t onTimePeriod;
  uint16_t offTimePeriod;
  uint16_t IRpin;
  int8_t periodOffset;
  uint8_t _dutycycle;
  bool modulation;
  uint32_t calcUSecPeriod(uint32_t hz, bool use_offset = true);
#if SEND_SONY
  void _sendSony(const uint64_t data, const uint16_t nbits,
                 const uint16_t repeat, const uint16_t freq);
#endif  // SEND_SONY
};

#endif  // IRSEND_H_
