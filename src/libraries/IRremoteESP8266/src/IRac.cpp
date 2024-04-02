// Copyright 2019 David Conran

// Provide a universal/standard interface for sending A/C nessages.
// It does not provide complete and maximum granular control but tries
// to offer most common functionality across all supported devices.

#include "IRac.h"
//#ifndef UNIT_TEST
#include "String.h"
//#endif
//#include <string.h>
#ifndef ARDUINO
//#include <string>
#endif
#include <math.h>
#include "IRsend.h"
#include "IRremoteESP8266.h"
#include "IRtext.h"
#include "IRutils.h"
#include "ir_Daikin.h"

// On the ESP8266 platform we need to use a special version of string handling
// functions to handle the strings stored in the flash address space.
#ifndef STRCASECMP
#if defined(ESP8266)
#define STRCASECMP(LHS, RHS) \
    strcasecmp_P(LHS, reinterpret_cast<const char*>(RHS))
#else  // ESP8266
#define STRCASECMP(LHS, RHS) strcasecmp(LHS, RHS)
#endif  // ESP8266
#endif  // STRCASECMP

#ifndef UNIT_TEST
#define OUTPUT_DECODE_RESULTS_FOR_UT(ac)
#else
/* NOTE: THIS IS NOT A DOXYGEN COMMENT (would require ENABLE_PREPROCESSING-YES)
/// If compiling for UT *and* a test receiver @c IRrecv is provided via the
/// @c _utReceived param, this injects an "output" gadget @c _lastDecodeResults
/// into the @c IRAc::sendAc method, so that the UT code may parse the "sent"
/// value and drive further assertions
///
/// @note The @c decode_results "returned" is a shallow copy (empty rawbuf),
///       mostly b/c the class does not have a custom/deep copy c-tor
///       and defining it would be an overkill for this purpose
/// @note For future maintainers: If @c IRAc class is ever refactored to use
///       polymorphism (static or dynamic)... this macro should be removed
///       and replaced with proper GMock injection.
*/
#define OUTPUT_DECODE_RESULTS_FOR_UT(ac)                        \
  {                                                             \
    if (_utReceiver) {                                          \
      _lastDecodeResults = nullptr;                             \
      (ac)._irsend.makeDecodeResult();                          \
      if (_utReceiver->decode(&(ac)._irsend.capture)) {         \
        _lastDecodeResults = std::unique_ptr<decode_results>(   \
          new decode_results((ac)._irsend.capture));            \
        _lastDecodeResults->rawbuf = nullptr;                   \
      }                                                         \
    }                                                           \
  }
#endif  // UNIT_TEST

/// Class constructor
/// @param[in] pin Gpio pin to use when transmitting IR messages.
/// @param[in] inverted true, gpio output defaults to high. false, to low.
/// @param[in] use_modulation true means use frequency modulation. false, don't.
IRac::IRac(const uint16_t pin, const bool inverted, const bool use_modulation) {
  _pin = pin;
  _inverted = inverted;
  _modulation = use_modulation;
  this->markAsSent();
}

/// Initialise the given state with the supplied settings.
/// @param[out] state A Ptr to where the settings will be stored.
/// @param[in] vendor The vendor/protocol type.
/// @param[in] model The A/C model if applicable.
/// @param[in] power The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] celsius Temperature units. True is Celsius, False is Fahrenheit.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingv The vertical swing setting.
/// @param[in] swingh The horizontal swing setting.
/// @param[in] quiet Run the device in quiet/silent mode.
/// @param[in] turbo Run the device in turbo/powerful mode.
/// @param[in] econo Run the device in economical mode.
/// @param[in] light Turn on the LED/Display mode.
/// @param[in] filter Turn on the (ion/pollen/etc) filter mode.
/// @param[in] clean Turn on the self-cleaning mode. e.g. Mould, dry filters etc
/// @param[in] beep Enable/Disable beeps when receiving IR messages.
/// @param[in] sleep Nr. of minutes for sleep mode.
///  -1 is Off, >= 0 is on. Some devices it is the nr. of mins to run for.
///  Others it may be the time to enter/exit sleep mode.
///  i.e. Time in Nr. of mins since midnight.
/// @param[in] clock The time in Nr. of mins since midnight. < 0 is ignore.
void IRac::initState(stdAc::state_t *state,
                     const decode_type_t vendor, const int16_t model,
                     const bool power, const stdAc::opmode_t mode,
                     const float degrees, const bool celsius,
                     const stdAc::fanspeed_t fan,
                     const stdAc::swingv_t swingv, const stdAc::swingh_t swingh,
                     const bool quiet, const bool turbo, const bool econo,
                     const bool light, const bool filter, const bool clean,
                     const bool beep, const int16_t sleep,
                     const int16_t clock) {
  state->protocol = vendor;
  state->model = model;
  state->power = power;
  state->mode = mode;
  state->degrees = degrees;
  state->celsius = celsius;
  state->fanspeed = fan;
  state->swingv = swingv;
  state->swingh = swingh;
  state->quiet = quiet;
  state->turbo = turbo;
  state->econo = econo;
  state->light = light;
  state->filter = filter;
  state->clean = clean;
  state->beep = beep;
  state->sleep = sleep;
  state->clock = clock;
}

/// Initialise the given state with the supplied settings.
/// @param[out] state A Ptr to where the settings will be stored.
/// @note Sets all the parameters to reasonable base/automatic defaults.
void IRac::initState(stdAc::state_t *state) {
  stdAc::state_t def;
  *state = def;
}

/// Get the current internal A/C climate state.
/// @return A Ptr to a state containing the current (to be sent) settings.
stdAc::state_t IRac::getState(void) { return next; }

/// Get the previous internal A/C climate state that should have already been
/// sent to the device. i.e. What the A/C unit should already be set to.
/// @return A Ptr to a state containing the previously sent settings.
stdAc::state_t IRac::getStatePrev(void) { return _prev; }

/// Is the given protocol supported by the IRac class?
/// @param[in] protocol The vendor/protocol type.
/// @return true if the protocol is supported by this class, otherwise false.
bool IRac::isProtocolSupported(const decode_type_t protocol) {
  switch (protocol) {
#if SEND_DAIKIN
    case decode_type_t::DAIKIN:
#endif
#if SEND_DAIKIN128
    case decode_type_t::DAIKIN128:
#endif
#if SEND_DAIKIN152
    case decode_type_t::DAIKIN152:
#endif
#if SEND_DAIKIN160
    case decode_type_t::DAIKIN160:
#endif
#if SEND_DAIKIN176
    case decode_type_t::DAIKIN176:
#endif
#if SEND_DAIKIN2
    case decode_type_t::DAIKIN2:
#endif
#if SEND_DAIKIN216
    case decode_type_t::DAIKIN216:
#endif
#if SEND_DAIKIN64
    case decode_type_t::DAIKIN64:
#endif
      return true;
    default:
      return false;
  }
}

#if SEND_DAIKIN
/// Send a Daikin A/C message with the supplied settings.
/// @param[in, out] ac A Ptr to an IRDaikinESP object to use.
/// @param[in] on The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingv The vertical swing setting.
/// @param[in] swingh The horizontal swing setting.
/// @param[in] quiet Run the device in quiet/silent mode.
/// @param[in] turbo Run the device in turbo/powerful mode.
/// @param[in] econo Run the device in economical mode.
/// @param[in] clean Turn on the self-cleaning mode. e.g. Mould, dry filters etc
void IRac::daikin(IRDaikinESP *ac,
                  const bool on, const stdAc::opmode_t mode,
                  const float degrees, const stdAc::fanspeed_t fan,
                  const stdAc::swingv_t swingv, const stdAc::swingh_t swingh,
                  const bool quiet, const bool turbo, const bool econo,
                  const bool clean) {
  ac->begin();
  ac->setPower(on);
  ac->setMode(ac->convertMode(mode));
  ac->setTemp(degrees);
  ac->setFan(ac->convertFan(fan));
  ac->setSwingVertical((int8_t)swingv >= 0);
  ac->setSwingHorizontal((int8_t)swingh >= 0);
  ac->setQuiet(quiet);
  // No Light setting available.
  // No Filter setting available.
  ac->setPowerful(turbo);
  ac->setEcono(econo);
  ac->setMold(clean);
  // No Beep setting available.
  // No Sleep setting available.
  // No Clock setting available.
  ac->send();
}
#endif  // SEND_DAIKIN

#if SEND_DAIKIN128
/// Send a Daikin 128-bit A/C message with the supplied settings.
/// @param[in, out] ac A Ptr to an IRDaikin128 object to use.
/// @param[in] on The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingv The vertical swing setting.
/// @param[in] quiet Run the device in quiet/silent mode.
/// @param[in] turbo Run the device in turbo/powerful mode.
/// @param[in] light Turn on the LED/Display mode.
/// @param[in] econo Run the device in economical mode.
/// @param[in] sleep Nr. of minutes for sleep mode. -1 is Off, >= 0 is on.
/// @param[in] clock The time in Nr. of mins since midnight. < 0 is ignore.
void IRac::daikin128(IRDaikin128 *ac,
                  const bool on, const stdAc::opmode_t mode,
                  const float degrees, const stdAc::fanspeed_t fan,
                  const stdAc::swingv_t swingv,
                  const bool quiet, const bool turbo, const bool light,
                  const bool econo, const int16_t sleep, const int16_t clock) {
  ac->begin();
  ac->setPowerToggle(on);
  ac->setMode(ac->convertMode(mode));
  ac->setTemp(degrees);
  ac->setFan(ac->convertFan(fan));
  ac->setSwingVertical((int8_t)swingv >= 0);
  // No Horizontal Swing setting avaliable.
  ac->setQuiet(quiet);
  ac->setLightToggle(light ? kDaikin128BitWall : 0);
  // No Filter setting available.
  ac->setPowerful(turbo);
  ac->setEcono(econo);
  // No Clean setting available.
  // No Beep setting available.
  ac->setSleep(sleep > 0);
  if (clock >= 0) ac->setClock(clock);
  ac->send();
}
#endif  // SEND_DAIKIN128

#if SEND_DAIKIN152
/// Send a Daikin 152-bit A/C message with the supplied settings.
/// @param[in, out] ac A Ptr to an IRDaikin152 object to use.
/// @param[in] on The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingv The vertical swing setting.
/// @param[in] quiet Run the device in quiet/silent mode.
/// @param[in] turbo Run the device in turbo/powerful mode.
/// @param[in] econo Run the device in economical mode.
void IRac::daikin152(IRDaikin152 *ac,
                  const bool on, const stdAc::opmode_t mode,
                  const float degrees, const stdAc::fanspeed_t fan,
                  const stdAc::swingv_t swingv,
                  const bool quiet, const bool turbo, const bool econo) {
  ac->begin();
  ac->setPower(on);
  ac->setMode(ac->convertMode(mode));
  ac->setTemp(degrees);
  ac->setFan(ac->convertFan(fan));
  ac->setSwingV((int8_t)swingv >= 0);
  // No Horizontal Swing setting avaliable.
  ac->setQuiet(quiet);
  // No Light setting available.
  // No Filter setting available.
  ac->setPowerful(turbo);
  ac->setEcono(econo);
  // No Clean setting available.
  // No Beep setting available.
  // No Sleep setting available.
  // No Clock setting available.
  ac->send();
}
#endif  // SEND_DAIKIN152

#if SEND_DAIKIN160
/// Send a Daikin 160-bit A/C message with the supplied settings.
/// @param[in, out] ac A Ptr to an IRDaikin160 object to use.
/// @param[in] on The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingv The vertical swing setting.
void IRac::daikin160(IRDaikin160 *ac,
                     const bool on, const stdAc::opmode_t mode,
                     const float degrees, const stdAc::fanspeed_t fan,
                     const stdAc::swingv_t swingv) {
  ac->begin();
  ac->setPower(on);
  ac->setMode(ac->convertMode(mode));
  ac->setTemp(degrees);
  ac->setFan(ac->convertFan(fan));
  ac->setSwingVertical(ac->convertSwingV(swingv));
  ac->send();
}
#endif  // SEND_DAIKIN160

#if SEND_DAIKIN176
/// Send a Daikin 176-bit A/C message with the supplied settings.
/// @param[in, out] ac A Ptr to an IRDaikin176 object to use.
/// @param[in] on The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingh The horizontal swing setting.
void IRac::daikin176(IRDaikin176 *ac,
                     const bool on, const stdAc::opmode_t mode,
                     const float degrees, const stdAc::fanspeed_t fan,
                     const stdAc::swingh_t swingh) {
  ac->begin();
  ac->setPower(on);
  ac->setMode(ac->convertMode(mode));
  ac->setTemp(degrees);
  ac->setFan(ac->convertFan(fan));
  ac->setSwingHorizontal(ac->convertSwingH(swingh));
  ac->send();
}
#endif  // SEND_DAIKIN176

#if SEND_DAIKIN2
/// Send a Daikin2 A/C message with the supplied settings.
/// @param[in, out] ac A Ptr to an IRDaikin2 object to use.
/// @param[in] on The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingv The vertical swing setting.
/// @param[in] swingh The horizontal swing setting.
/// @param[in] quiet Run the device in quiet/silent mode.
/// @param[in] turbo Run the device in turbo/powerful mode.
/// @param[in] light Turn on the LED/Display mode.
/// @param[in] econo Run the device in economical mode.
/// @param[in] filter Turn on the (ion/pollen/etc) filter mode.
/// @param[in] clean Turn on the self-cleaning mode. e.g. Mould, dry filters etc
/// @param[in] beep Enable/Disable beeps when receiving IR messages.
/// @param[in] sleep Nr. of minutes for sleep mode. -1 is Off, >= 0 is on.
/// @param[in] clock The time in Nr. of mins since midnight. < 0 is ignore.
void IRac::daikin2(IRDaikin2 *ac,
                   const bool on, const stdAc::opmode_t mode,
                   const float degrees, const stdAc::fanspeed_t fan,
                   const stdAc::swingv_t swingv, const stdAc::swingh_t swingh,
                   const bool quiet, const bool turbo, const bool light,
                   const bool econo, const bool filter, const bool clean,
                   const bool beep, const int16_t sleep, const int16_t clock) {
  ac->begin();
  ac->setPower(on);
  ac->setMode(ac->convertMode(mode));
  ac->setTemp(degrees);
  ac->setFan(ac->convertFan(fan));
  ac->setSwingVertical(ac->convertSwingV(swingv));
  ac->setSwingHorizontal(ac->convertSwingH(swingh));
  ac->setQuiet(quiet);
  ac->setLight(light ? 1 : 3);  // On/High is 1, Off is 3.
  ac->setPowerful(turbo);
  ac->setEcono(econo);
  ac->setPurify(filter);
  ac->setMold(clean);
  ac->setClean(true);  // Hardwire auto clean to be on per request (@sheppy99)
  ac->setBeep(beep ? 2 : 3);  // On/Loud is 2, Off is 3.
  if (sleep > 0) ac->enableSleepTimer(sleep);
  if (clock >= 0) ac->setCurrentTime(clock);
  ac->send();
}
#endif  // SEND_DAIKIN2

#if SEND_DAIKIN216
/// Send a Daikin 216-bit A/C message with the supplied settings.
/// @param[in, out] ac A Ptr to an IRDaikin216 object to use.
/// @param[in] on The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingv The vertical swing setting.
/// @param[in] swingh The horizontal swing setting.
/// @param[in] quiet Run the device in quiet/silent mode.
/// @param[in] turbo Run the device in turbo/powerful mode.
void IRac::daikin216(IRDaikin216 *ac,
                     const bool on, const stdAc::opmode_t mode,
                     const float degrees, const stdAc::fanspeed_t fan,
                     const stdAc::swingv_t swingv, const stdAc::swingh_t swingh,
                     const bool quiet, const bool turbo) {
  ac->begin();
  ac->setPower(on);
  ac->setMode(ac->convertMode(mode));
  ac->setTemp(degrees);
  ac->setFan(ac->convertFan(fan));
  ac->setSwingVertical((int8_t)swingv >= 0);
  ac->setSwingHorizontal((int8_t)swingh >= 0);
  ac->setQuiet(quiet);
  ac->setPowerful(turbo);
  ac->send();
}
#endif  // SEND_DAIKIN216

#if SEND_DAIKIN64
/// Send a Daikin 64-bit A/C message with the supplied settings.
/// @param[in, out] ac A Ptr to an IRDaikin64 object to use.
/// @param[in] on The power setting.
/// @param[in] mode The operation mode setting.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] fan The speed setting for the fan.
/// @param[in] swingv The vertical swing setting.
/// @param[in] quiet Run the device in quiet/silent mode.
/// @param[in] turbo Run the device in turbo/powerful mode.
/// @param[in] sleep Nr. of minutes for sleep mode. -1 is Off, >= 0 is on.
/// @param[in] clock The time in Nr. of mins since midnight. < 0 is ignore.
void IRac::daikin64(IRDaikin64 *ac,
                  const bool on, const stdAc::opmode_t mode,
                  const float degrees, const stdAc::fanspeed_t fan,
                  const stdAc::swingv_t swingv,
                  const bool quiet, const bool turbo,
                  const int16_t sleep, const int16_t clock) {
  ac->begin();
  ac->setPowerToggle(on);
  ac->setMode(ac->convertMode(mode));
  ac->setTemp(degrees);
  ac->setFan(ac->convertFan(fan));
  ac->setSwingVertical((int8_t)swingv >= 0);
  ac->setTurbo(turbo);
  ac->setQuiet(quiet);
  ac->setSleep(sleep >= 0);
  if (clock >= 0) ac->setClock(clock);
  ac->send();
}
#endif  // SEND_DAIKIN64


/// Create a new state base on the provided state that has been suitably fixed.
/// @note This is for use with Home Assistant, which requires mode to be off if
///   the power is off.
/// @param[in] state The state_t structure describing the desired a/c state.
/// @return A stdAc::state_t with the needed settings.
stdAc::state_t IRac::cleanState(const stdAc::state_t state) {
  stdAc::state_t result = state;
  // A hack for Home Assistant, it appears to need/want an Off opmode.
  // So enforce the power is off if the mode is also off.
  if (state.mode == stdAc::opmode_t::kOff) result.power = false;
  return result;
}

/// Create a new state base on desired & previous states but handle
/// any state changes for options that need to be toggled.
/// @param[in] desired The state_t structure describing the desired a/c state.
/// @param[in] prev A Ptr to the previous state_t structure.
/// @return A stdAc::state_t with the needed settings.
stdAc::state_t IRac::handleToggles(const stdAc::state_t desired,
                                   const stdAc::state_t *prev) {
  stdAc::state_t result = desired;
  // If we've been given a previous state AND the it's the same A/C basically.
  if (prev != NULL && desired.protocol == prev->protocol &&
      desired.model == prev->model) {
    // Check if we have to handle toggle settings for specific A/C protocols.
    switch (desired.protocol) {
      case decode_type_t::DAIKIN128:
        result.power = desired.power ^ prev->power;
        result.light = desired.light ^ prev->light;
        break;
      case decode_type_t::DAIKIN64:
        result.power = desired.power ^ prev->power;
        break;
      default:
        {};
    }
  }
  return result;
}

/// Send A/C message for a given device using common A/C settings.
/// @param[in] vendor The vendor/protocol type.
/// @param[in] model The A/C model if applicable.
/// @param[in] power The power setting.
/// @param[in] mode The operation mode setting.
/// @note Changing mode from "Off" to something else does NOT turn on a device.
/// You need to use `power` for that.
/// @param[in] degrees The temperature setting in degrees.
/// @param[in] celsius Temperature units. True is Celsius, False is Fahrenheit.
/// @param[in] fan The speed setting for the fan.
/// @note The following are all "if supported" by the underlying A/C classes.
/// @param[in] swingv The vertical swing setting.
/// @param[in] swingh The horizontal swing setting.
/// @param[in] quiet Run the device in quiet/silent mode.
/// @param[in] turbo Run the device in turbo/powerful mode.
/// @param[in] econo Run the device in economical mode.
/// @param[in] light Turn on the LED/Display mode.
/// @param[in] filter Turn on the (ion/pollen/etc) filter mode.
/// @param[in] clean Turn on the self-cleaning mode. e.g. Mould, dry filters etc
/// @param[in] beep Enable/Disable beeps when receiving IR messages.
/// @param[in] sleep Nr. of minutes for sleep mode.
///  -1 is Off, >= 0 is on. Some devices it is the nr. of mins to run for.
///  Others it may be the time to enter/exit sleep mode.
///  i.e. Time in Nr. of mins since midnight.
/// @param[in] clock The time in Nr. of mins since midnight. < 0 is ignore.
/// @return True, if accepted/converted/attempted etc. False, if unsupported.
bool IRac::sendAc(const decode_type_t vendor, const int16_t model,
                  const bool power, const stdAc::opmode_t mode,
                  const float degrees, const bool celsius,
                  const stdAc::fanspeed_t fan,
                  const stdAc::swingv_t swingv, const stdAc::swingh_t swingh,
                  const bool quiet, const bool turbo, const bool econo,
                  const bool light, const bool filter, const bool clean,
                  const bool beep, const int16_t sleep, const int16_t clock) {
  stdAc::state_t to_send;
  initState(&to_send, vendor, model, power, mode, degrees, celsius, fan, swingv,
            swingh, quiet, turbo, econo, light, filter, clean, beep, sleep,
            clock);
  return this->sendAc(to_send, &to_send);
}

/// Send A/C message for a given device using state_t structures.
/// @param[in] desired The state_t structure describing the desired new ac state
/// @param[in] prev A Ptr to the state_t structure containing the previous state
/// @note Changing mode from "Off" to something else does NOT turn on a device.
/// You need to use `power` for that.
/// @return True, if accepted/converted/attempted etc. False, if unsupported.
bool IRac::sendAc(const stdAc::state_t desired, const stdAc::state_t *prev) {
  // Convert the temp from Fahrenheit to Celsius if we are not in Celsius mode.
  float degC __attribute__((unused)) =
      desired.celsius ? desired.degrees : fahrenheitToCelsius(desired.degrees);
  // Convert the sensorTemp from Fahrenheit to Celsius if we are not in Celsius
  // mode.
  float sensorTempC __attribute__((unused)) =
      desired.sensorTemperature ? desired.sensorTemperature
          : fahrenheitToCelsius(desired.sensorTemperature);
  // special `state_t` that is required to be sent based on that.
  stdAc::state_t send = this->handleToggles(this->cleanState(desired), prev);
  // Some protocols expect a previous state for power.
  // Construct a pointer-safe previous power state incase prev is NULL/NULLPTR.

  switch (send.protocol) {
#if SEND_DAIKIN
    case DAIKIN:
    {
      IRDaikinESP ac(_pin, _inverted, _modulation);
      daikin(&ac, send.power, send.mode, degC, send.fanspeed, send.swingv,
             send.swingh, send.quiet, send.turbo, send.econo, send.clean);
      break;
    }
#endif  // SEND_DAIKIN
#if SEND_DAIKIN128
    case DAIKIN128:
    {
      IRDaikin128 ac(_pin, _inverted, _modulation);
      daikin128(&ac, send.power, send.mode, degC, send.fanspeed, send.swingv,
                send.quiet, send.turbo, send.light, send.econo, send.sleep,
                send.clock);
      break;
    }
#endif  // SEND_DAIKIN2
#if SEND_DAIKIN152
    case DAIKIN152:
    {
      IRDaikin152 ac(_pin, _inverted, _modulation);
      daikin152(&ac, send.power, send.mode, degC, send.fanspeed, send.swingv,
                send.quiet, send.turbo, send.econo);
      break;
    }
#endif  // SEND_DAIKIN152
#if SEND_DAIKIN160
    case DAIKIN160:
    {
      IRDaikin160 ac(_pin, _inverted, _modulation);
      daikin160(&ac, send.power, send.mode, degC, send.fanspeed, send.swingv);
      break;
    }
#endif  // SEND_DAIKIN160
#if SEND_DAIKIN176
    case DAIKIN176:
    {
      IRDaikin176 ac(_pin, _inverted, _modulation);
      daikin176(&ac, send.power, send.mode, degC, send.fanspeed, send.swingh);
      break;
    }
#endif  // SEND_DAIKIN176
#if SEND_DAIKIN2
    case DAIKIN2:
    {
      IRDaikin2 ac(_pin, _inverted, _modulation);
      daikin2(&ac, send.power, send.mode, degC, send.fanspeed, send.swingv,
              send.swingh, send.quiet, send.turbo, send.light, send.econo,
              send.filter, send.clean, send.beep, send.sleep, send.clock);
      break;
    }
#endif  // SEND_DAIKIN2
#if SEND_DAIKIN216
    case DAIKIN216:
    {
      IRDaikin216 ac(_pin, _inverted, _modulation);
      daikin216(&ac, send.power, send.mode, degC, send.fanspeed, send.swingv,
                send.swingh, send.quiet, send.turbo);
      break;
    }
#endif  // SEND_DAIKIN216
#if SEND_DAIKIN64
    case DAIKIN64:
    {
      IRDaikin64 ac(_pin, _inverted, _modulation);
      daikin64(&ac, send.power, send.mode, degC, send.fanspeed, send.swingv,
               send.quiet, send.turbo, send.sleep, send.clock);
      break;
    }
#endif  // SEND_DAIKIN64
    default:
      return false;  // Fail, didn't match anything.
  }
  return true;  // Success.
}  // NOLINT(readability/fn_size)

/// Update the previous state to the current one.
void IRac::markAsSent(void) {
  _prev = next;
}

/// Send an A/C message based soley on our internal state.
/// @return True, if accepted/converted/attempted. False, if unsupported.
bool IRac::sendAc(void) {
  bool success = this->sendAc(next, &_prev);
  if (success) this->markAsSent();
  return success;
}

/// Compare two AirCon states.
/// @note The comparison excludes the clock.
/// @param a A state_t to be compared.
/// @param b A state_t to be compared.
/// @return True if they differ, False if they don't.
bool IRac::cmpStates(const stdAc::state_t a, const stdAc::state_t b) {
  return a.protocol != b.protocol || a.model != b.model || a.power != b.power ||
      a.mode != b.mode || a.degrees != b.degrees || a.celsius != b.celsius ||
      a.fanspeed != b.fanspeed || a.swingv != b.swingv ||
      a.swingh != b.swingh || a.quiet != b.quiet || a.turbo != b.turbo ||
      a.econo != b.econo || a.light != b.light || a.filter != b.filter ||
      a.clean != b.clean || a.beep != b.beep || a.sleep != b.sleep ||
      a.command != b.command || a.sensorTemperature != b.sensorTemperature ||
      a.iFeel != b.iFeel;
}

/// Check if the internal state has changed from what was previously sent.
/// @note The comparison excludes the clock.
/// @return True if it has changed, False if not.
bool IRac::hasStateChanged(void) { return cmpStates(next, _prev); }

/// Convert the supplied str into the appropriate enum.
/// @param[in] str A Ptr to a C-style string to be converted.
/// @param[in] def The enum to return if no conversion was possible.
/// @return The equivalent enum.
stdAc::ac_command_t IRac::strToCommandType(const char *str,
                                           const stdAc::ac_command_t def) {
  if (!STRCASECMP(str, kControlCommandStr))
    return stdAc::ac_command_t::kControlCommand;
  else if (!STRCASECMP(str, kIFeelReportStr) ||
           !STRCASECMP(str, kIFeelStr))
    return stdAc::ac_command_t::kSensorTempReport;
  else if (!STRCASECMP(str, kSetTimerCommandStr) ||
           !STRCASECMP(str, kTimerStr))
    return stdAc::ac_command_t::kTimerCommand;
  else if (!STRCASECMP(str, kConfigCommandStr))
    return stdAc::ac_command_t::kConfigCommand;
  else
    return def;
}

/// Convert the supplied str into the appropriate enum.
/// @param[in] str A Ptr to a C-style string to be converted.
/// @param[in] def The enum to return if no conversion was possible.
/// @return The equivalent enum.
stdAc::opmode_t IRac::strToOpmode(const char *str,
                                  const stdAc::opmode_t def) {
  if (!STRCASECMP(str, kAutoStr) ||
      !STRCASECMP(str, kAutomaticStr))
    return stdAc::opmode_t::kAuto;
  else if (!STRCASECMP(str, kOffStr) ||
           !STRCASECMP(str, kStopStr))
    return stdAc::opmode_t::kOff;
  else if (!STRCASECMP(str, kCoolStr) ||
           !STRCASECMP(str, kCoolingStr))
    return stdAc::opmode_t::kCool;
  else if (!STRCASECMP(str, kHeatStr) ||
           !STRCASECMP(str, kHeatingStr))
    return stdAc::opmode_t::kHeat;
  else if (!STRCASECMP(str, kDryStr) ||
           !STRCASECMP(str, kDryingStr) ||
           !STRCASECMP(str, kDehumidifyStr))
    return stdAc::opmode_t::kDry;
  else if (!STRCASECMP(str, kFanStr) ||
          // The following Fans strings with "only" are required to help with
          // HomeAssistant & Google Home Climate integration.
          // For compatibility only.
          // Ref: https://www.home-assistant.io/integrations/google_assistant/#climate-operation-modes
           !STRCASECMP(str, kFanOnlyStr) ||
           !STRCASECMP(str, kFan_OnlyStr) ||
           !STRCASECMP(str, kFanOnlyWithSpaceStr) ||
           !STRCASECMP(str, kFanOnlyNoSpaceStr))
    return stdAc::opmode_t::kFan;
  else
    return def;
}

/// Convert the supplied str into the appropriate enum.
/// @param[in] str A Ptr to a C-style string to be converted.
/// @param[in] def The enum to return if no conversion was possible.
/// @return The equivalent enum.
stdAc::fanspeed_t IRac::strToFanspeed(const char *str,
                                      const stdAc::fanspeed_t def) {
  if (!STRCASECMP(str, kAutoStr) ||
      !STRCASECMP(str, kAutomaticStr))
    return stdAc::fanspeed_t::kAuto;
  else if (!STRCASECMP(str, kMinStr) ||
           !STRCASECMP(str, kMinimumStr) ||
           !STRCASECMP(str, kLowestStr))
    return stdAc::fanspeed_t::kMin;
  else if (!STRCASECMP(str, kLowStr) ||
           !STRCASECMP(str, kLoStr))
    return stdAc::fanspeed_t::kLow;
  else if (!STRCASECMP(str, kMedStr) ||
           !STRCASECMP(str, kMediumStr) ||
           !STRCASECMP(str, kMidStr))
    return stdAc::fanspeed_t::kMedium;
  else if (!STRCASECMP(str, kHighStr) ||
           !STRCASECMP(str, kHiStr))
    return stdAc::fanspeed_t::kHigh;
  else if (!STRCASECMP(str, kMaxStr) ||
           !STRCASECMP(str, kMaximumStr) ||
           !STRCASECMP(str, kHighestStr))
    return stdAc::fanspeed_t::kMax;
  else if (!STRCASECMP(str, kMedHighStr))
    return stdAc::fanspeed_t::kMediumHigh;
  else
    return def;
}

/// Convert the supplied str into the appropriate enum.
/// @param[in] str A Ptr to a C-style string to be converted.
/// @param[in] def The enum to return if no conversion was possible.
/// @return The equivalent enum.
stdAc::swingv_t IRac::strToSwingV(const char *str,
                                  const stdAc::swingv_t def) {
  if (!STRCASECMP(str, kAutoStr) ||
      !STRCASECMP(str, kAutomaticStr) ||
      !STRCASECMP(str, kOnStr) ||
      !STRCASECMP(str, kSwingStr))
    return stdAc::swingv_t::kAuto;
  else if (!STRCASECMP(str, kOffStr) ||
           !STRCASECMP(str, kStopStr))
    return stdAc::swingv_t::kOff;
  else if (!STRCASECMP(str, kMinStr) ||
           !STRCASECMP(str, kMinimumStr) ||
           !STRCASECMP(str, kLowestStr) ||
           !STRCASECMP(str, kBottomStr) ||
           !STRCASECMP(str, kDownStr))
    return stdAc::swingv_t::kLowest;
  else if (!STRCASECMP(str, kLowStr))
    return stdAc::swingv_t::kLow;
  else if (!STRCASECMP(str, kMidStr) ||
           !STRCASECMP(str, kMiddleStr) ||
           !STRCASECMP(str, kMedStr) ||
           !STRCASECMP(str, kMediumStr) ||
           !STRCASECMP(str, kCentreStr))
    return stdAc::swingv_t::kMiddle;
  else if (!STRCASECMP(str, kUpperMiddleStr))
    return stdAc::swingv_t::kUpperMiddle;
  else if (!STRCASECMP(str, kHighStr) ||
           !STRCASECMP(str, kHiStr))
    return stdAc::swingv_t::kHigh;
  else if (!STRCASECMP(str, kHighestStr) ||
           !STRCASECMP(str, kMaxStr) ||
           !STRCASECMP(str, kMaximumStr) ||
           !STRCASECMP(str, kTopStr) ||
           !STRCASECMP(str, kUpStr))
    return stdAc::swingv_t::kHighest;
  else
    return def;
}

/// Convert the supplied str into the appropriate enum.
/// @param[in] str A Ptr to a C-style string to be converted.
/// @param[in] def The enum to return if no conversion was possible.
/// @return The equivalent enum.
stdAc::swingh_t IRac::strToSwingH(const char *str,
                                  const stdAc::swingh_t def) {
  if (!STRCASECMP(str, kAutoStr) ||
      !STRCASECMP(str, kAutomaticStr) ||
      !STRCASECMP(str, kOnStr) || !STRCASECMP(str, kSwingStr))
    return stdAc::swingh_t::kAuto;
  else if (!STRCASECMP(str, kOffStr) ||
           !STRCASECMP(str, kStopStr))
    return stdAc::swingh_t::kOff;
  else if (!STRCASECMP(str, kLeftMaxNoSpaceStr) ||              // "LeftMax"
           !STRCASECMP(str, kLeftMaxStr) ||                     // "Left Max"
           !STRCASECMP(str, kMaxLeftNoSpaceStr) ||              // "MaxLeft"
           !STRCASECMP(str, kMaxLeftStr))                       // "Max Left"
    return stdAc::swingh_t::kLeftMax;
  else if (!STRCASECMP(str, kLeftStr))
    return stdAc::swingh_t::kLeft;
  else if (!STRCASECMP(str, kMidStr) ||
           !STRCASECMP(str, kMiddleStr) ||
           !STRCASECMP(str, kMedStr) ||
           !STRCASECMP(str, kMediumStr) ||
           !STRCASECMP(str, kCentreStr))
    return stdAc::swingh_t::kMiddle;
  else if (!STRCASECMP(str, kRightStr))
    return stdAc::swingh_t::kRight;
  else if (!STRCASECMP(str, kRightMaxNoSpaceStr) ||              // "RightMax"
           !STRCASECMP(str, kRightMaxStr) ||                     // "Right Max"
           !STRCASECMP(str, kMaxRightNoSpaceStr) ||              // "MaxRight"
           !STRCASECMP(str, kMaxRightStr))                       // "Max Right"
    return stdAc::swingh_t::kRightMax;
  else if (!STRCASECMP(str, kWideStr))
    return stdAc::swingh_t::kWide;
  else
    return def;
}

/// Convert the supplied str into the appropriate enum.
/// @note Assumes str is the model code or an integer >= 1.
/// @param[in] str A Ptr to a C-style string to be converted.
/// @param[in] def The enum to return if no conversion was possible.
/// @return The equivalent enum.
/// @note After adding a new model you should update modelToStr() too.
int16_t IRac::strToModel(const char *str, const int16_t def) {
    int16_t number = atoi(str);
    if (number > 0)
      return number;
    else
      return def;
}

/// Convert the supplied str into the appropriate boolean value.
/// @param[in] str A Ptr to a C-style string to be converted.
/// @param[in] def The boolean value to return if no conversion was possible.
/// @return The equivalent boolean value.
bool IRac::strToBool(const char *str, const bool def) {
  if (!STRCASECMP(str, kOnStr) ||
      !STRCASECMP(str, k1Str) ||
      !STRCASECMP(str, kYesStr) ||
      !STRCASECMP(str, kTrueStr))
    return true;
  else if (!STRCASECMP(str, kOffStr) ||
           !STRCASECMP(str, k0Str) ||
           !STRCASECMP(str, kNoStr) ||
           !STRCASECMP(str, kFalseStr))
    return false;
  else
    return def;
}

/// Convert the supplied boolean into the appropriate String.
/// @param[in] value The boolean value to be converted.
/// @return The equivalent String for the locale.
String IRac::boolToString(const bool value) {
  return value ? kOnStr : kOffStr;
}

/// Convert the supplied operation mode into the appropriate String.
/// @param[in] cmdType The enum to be converted.
/// @return The equivalent String for the locale.
String IRac::commandTypeToString(const stdAc::ac_command_t cmdType) {
  switch (cmdType) {
    case stdAc::ac_command_t::kControlCommand:    return kControlCommandStr;
    case stdAc::ac_command_t::kSensorTempReport: return kIFeelReportStr;
    case stdAc::ac_command_t::kTimerCommand:      return kSetTimerCommandStr;
    case stdAc::ac_command_t::kConfigCommand:     return kConfigCommandStr;
    default:                                      return kUnknownStr;
  }
}

/// Convert the supplied operation mode into the appropriate String.
/// @param[in] mode The enum to be converted.
/// @param[in] ha A flag to indicate we want GoogleHome/HomeAssistant output.
/// @return The equivalent String for the locale.
String IRac::opmodeToString(const stdAc::opmode_t mode, const bool ha) {
  switch (mode) {
    case stdAc::opmode_t::kOff:  return kOffStr;
    case stdAc::opmode_t::kAuto: return kAutoStr;
    case stdAc::opmode_t::kCool: return kCoolStr;
    case stdAc::opmode_t::kHeat: return kHeatStr;
    case stdAc::opmode_t::kDry:  return kDryStr;
    case stdAc::opmode_t::kFan:  return ha ? kFan_OnlyStr : kFanStr;
    default:                     return kUnknownStr;
  }
}

/// Convert the supplied fan speed enum into the appropriate String.
/// @param[in] speed The enum to be converted.
/// @return The equivalent String for the locale.
String IRac::fanspeedToString(const stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kAuto:       return kAutoStr;
    case stdAc::fanspeed_t::kMax:        return kMaxStr;
    case stdAc::fanspeed_t::kHigh:       return kHighStr;
    case stdAc::fanspeed_t::kMedium:     return kMediumStr;
    case stdAc::fanspeed_t::kMediumHigh: return kMedHighStr;
    case stdAc::fanspeed_t::kLow:        return kLowStr;
    case stdAc::fanspeed_t::kMin:        return kMinStr;
    default:                             return kUnknownStr;
  }
}

/// Convert the supplied enum into the appropriate String.
/// @param[in] swingv The enum to be converted.
/// @return The equivalent String for the locale.
String IRac::swingvToString(const stdAc::swingv_t swingv) {
  switch (swingv) {
    case stdAc::swingv_t::kOff:          return kOffStr;
    case stdAc::swingv_t::kAuto:         return kAutoStr;
    case stdAc::swingv_t::kHighest:      return kHighestStr;
    case stdAc::swingv_t::kHigh:         return kHighStr;
    case stdAc::swingv_t::kMiddle:       return kMiddleStr;
    case stdAc::swingv_t::kUpperMiddle:  return kUpperMiddleStr;
    case stdAc::swingv_t::kLow:          return kLowStr;
    case stdAc::swingv_t::kLowest:       return kLowestStr;
    default:                             return kUnknownStr;
  }
}

/// Convert the supplied enum into the appropriate String.
/// @param[in] swingh The enum to be converted.
/// @return The equivalent String for the locale.
String IRac::swinghToString(const stdAc::swingh_t swingh) {
  switch (swingh) {
    case stdAc::swingh_t::kOff:      return kOffStr;
    case stdAc::swingh_t::kAuto:     return kAutoStr;
    case stdAc::swingh_t::kLeftMax:  return kLeftMaxStr;
    case stdAc::swingh_t::kLeft:     return kLeftStr;
    case stdAc::swingh_t::kMiddle:   return kMiddleStr;
    case stdAc::swingh_t::kRight:    return kRightStr;
    case stdAc::swingh_t::kRightMax: return kRightMaxStr;
    case stdAc::swingh_t::kWide:     return kWideStr;
    default:                         return kUnknownStr;
  }
}

namespace IRAcUtils {
  /// Display the human readable state of an A/C message if we can.
  /// @param[in] result A Ptr to the captured `decode_results` that contains an
  ///   A/C mesg.
  /// @return A string with the human description of the A/C message.
  ///   An empty string if we can't.
  String resultAcToString(const decode_results * const result) {
    switch (result->decode_type) {
#if DECODE_DAIKIN
      case decode_type_t::DAIKIN: {
        IRDaikinESP ac(kGpioUnused);
        ac.setRaw(result->state);
        return ac.toString();
      }
#endif  // DECODE_DAIKIN
#if DECODE_DAIKIN128
      case decode_type_t::DAIKIN128: {
        IRDaikin128 ac(kGpioUnused);
        ac.setRaw(result->state);
        return ac.toString();
      }
#endif  // DECODE_DAIKIN128
#if DECODE_DAIKIN152
      case decode_type_t::DAIKIN152: {
        IRDaikin152 ac(kGpioUnused);
        ac.setRaw(result->state);
        return ac.toString();
      }
#endif  // DECODE_DAIKIN152
#if DECODE_DAIKIN160
      case decode_type_t::DAIKIN160: {
        IRDaikin160 ac(kGpioUnused);
        ac.setRaw(result->state);
        return ac.toString();
      }
#endif  // DECODE_DAIKIN160
#if DECODE_DAIKIN176
      case decode_type_t::DAIKIN176: {
        IRDaikin176 ac(kGpioUnused);
        ac.setRaw(result->state);
        return ac.toString();
      }
#endif  // DECODE_DAIKIN160
#if DECODE_DAIKIN2
      case decode_type_t::DAIKIN2: {
        IRDaikin2 ac(kGpioUnused);
        ac.setRaw(result->state);
        return ac.toString();
      }
#endif  // DECODE_DAIKIN2
#if DECODE_DAIKIN216
      case decode_type_t::DAIKIN216: {
        IRDaikin216 ac(kGpioUnused);
        ac.setRaw(result->state);
        return ac.toString();
      }
#endif  // DECODE_DAIKIN216
#if DECODE_DAIKIN64
      case decode_type_t::DAIKIN64: {
        IRDaikin64 ac(kGpioUnused);
        ac.setRaw(result->value);  // Daikin64 uses value instead of state.
        return ac.toString();
      }
#endif  // DECODE_DAIKIN64
      default:
        return "";
    }
  }

  /// Convert a valid IR A/C remote message that we understand enough into a
  /// Common A/C state.
  /// @param[in] decode A PTR to a successful raw IR decode object.
  /// @param[in] result A PTR to a state structure to store the result in.
  /// @param[in] prev A PTR to a state structure which has the prev. state.
  /// @return A boolean indicating success or failure.
  bool decodeToState(const decode_results *decode, stdAc::state_t *result,
                     const stdAc::state_t *prev
/// @cond IGNORE
// *prev flagged as "unused" due to potential compiler warning when some
// protocols that use it are disabled. It really is used.
                                                __attribute__((unused))
/// @endcond
                    ) {
    if (decode == NULL || result == NULL) return false;  // Safety check.
    switch (decode->decode_type) {
#if DECODE_DAIKIN
      case decode_type_t::DAIKIN: {
        IRDaikinESP ac(kGpioUnused);
        ac.setRaw(decode->state);
        *result = ac.toCommon();
        break;
      }
#endif  // DECODE_DAIKIN
#if DECODE_DAIKIN128
      case decode_type_t::DAIKIN128: {
        IRDaikin128 ac(kGpioUnused);
        ac.setRaw(decode->state);
        *result = ac.toCommon(prev);
        break;
      }
#endif  // DECODE_DAIKIN128
#if DECODE_DAIKIN152
      case decode_type_t::DAIKIN152: {
        IRDaikin152 ac(kGpioUnused);
        ac.setRaw(decode->state);
        *result = ac.toCommon();
        break;
      }
#endif  // DECODE_DAIKIN152
#if DECODE_DAIKIN160
      case decode_type_t::DAIKIN160: {
        IRDaikin160 ac(kGpioUnused);
        ac.setRaw(decode->state);
        *result = ac.toCommon();
        break;
      }
#endif  // DECODE_DAIKIN160
#if DECODE_DAIKIN176
      case decode_type_t::DAIKIN176: {
        IRDaikin176 ac(kGpioUnused);
        ac.setRaw(decode->state);
        *result = ac.toCommon();
        break;
      }
#endif  // DECODE_DAIKIN160
#if DECODE_DAIKIN2
      case decode_type_t::DAIKIN2: {
        IRDaikin2 ac(kGpioUnused);
        ac.setRaw(decode->state);
        *result = ac.toCommon();
        break;
      }
#endif  // DECODE_DAIKIN2
#if DECODE_DAIKIN216
      case decode_type_t::DAIKIN216: {
        IRDaikin216 ac(kGpioUnused);
        ac.setRaw(decode->state);
        *result = ac.toCommon();
        break;
      }
#endif  // DECODE_DAIKIN216
#if DECODE_DAIKIN64
      case decode_type_t::DAIKIN64: {
        IRDaikin64 ac(kGpioUnused);
        ac.setRaw(decode->value);  // Uses value instead of state.
        *result = ac.toCommon(prev);
        break;
      }
#endif  // DECODE_DAIKIN64
      default:
        return false;
    }
    return true;
  }
}  // namespace IRAcUtils
