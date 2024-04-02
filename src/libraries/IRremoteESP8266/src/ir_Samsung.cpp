// Copyright 2009 Ken Shirriff
// Copyright 2017-2021 David Conran
/// @file
/// @brief Support for Samsung protocols.
/// Samsung originally added from https://github.com/shirriff/Arduino-IRremote/
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/505
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/621
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/1062
/// @see http://elektrolab.wz.cz/katalog/samsung_protocol.pdf
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/1538 (Checksum)

#include "ir_Samsung.h"
//// #include <algorithm>
#include <string.h>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRtext.h"
#include "IRutils.h"
#include "minmax.h"

// Constants
const uint16_t kSamsungTick = 560;
const uint16_t kSamsungHdrMarkTicks = 8;
const uint16_t kSamsungHdrMark = kSamsungHdrMarkTicks * kSamsungTick;
const uint16_t kSamsungHdrSpaceTicks = 8;
const uint16_t kSamsungHdrSpace = kSamsungHdrSpaceTicks * kSamsungTick;
const uint16_t kSamsungBitMarkTicks = 1;
const uint16_t kSamsungBitMark = kSamsungBitMarkTicks * kSamsungTick;
const uint16_t kSamsungOneSpaceTicks = 3;
const uint16_t kSamsungOneSpace = kSamsungOneSpaceTicks * kSamsungTick;
const uint16_t kSamsungZeroSpaceTicks = 1;
const uint16_t kSamsungZeroSpace = kSamsungZeroSpaceTicks * kSamsungTick;
const uint16_t kSamsungRptSpaceTicks = 4;
const uint16_t kSamsungRptSpace = kSamsungRptSpaceTicks * kSamsungTick;
const uint16_t kSamsungMinMessageLengthTicks = 193;
const uint32_t kSamsungMinMessageLength =
    kSamsungMinMessageLengthTicks * kSamsungTick;
const uint16_t kSamsungMinGapTicks =
    kSamsungMinMessageLengthTicks -
    (kSamsungHdrMarkTicks + kSamsungHdrSpaceTicks +
     kSamsungBits * (kSamsungBitMarkTicks + kSamsungOneSpaceTicks) +
     kSamsungBitMarkTicks);
const uint32_t kSamsungMinGap = kSamsungMinGapTicks * kSamsungTick;

// Data from https://github.com/crankyoldgit/IRremoteESP8266/issues/1220
// Values calculated based on the average of ten messages.
const uint16_t kSamsung36HdrMark = 4515;  /// < uSeconds
const uint16_t kSamsung36HdrSpace = 4438;  /// < uSeconds
const uint16_t kSamsung36BitMark = 512;  /// < uSeconds
const uint16_t kSamsung36OneSpace = 1468;  /// < uSeconds
const uint16_t kSamsung36ZeroSpace = 490;  /// < uSeconds

using irutils::addBoolToString;
using irutils::addFanToString;
using irutils::addIntToString;
using irutils::addLabeledString;
using irutils::addModeToString;
using irutils::addTempToString;
using irutils::addToggleToString;
using irutils::minsToString;

#if SEND_SAMSUNG
/// Send a 32-bit Samsung formatted message.
/// Status: STABLE / Should be working.
/// @param[in] data The message to be sent.
/// @param[in] nbits The number of bits of message to be sent.
/// @param[in] repeat The number of times the command is to be repeated.
/// @see http://elektrolab.wz.cz/katalog/samsung_protocol.pdf
/// @note Samsung has a separate message to indicate a repeat, like NEC does.
/// @todo Confirm that is actually how Samsung sends a repeat.
///   The refdoc doesn't indicate it is true.
void IRsend::sendSAMSUNG(const uint64_t data, const uint16_t nbits,
                         const uint16_t repeat) {
  sendGeneric(kSamsungHdrMark, kSamsungHdrSpace, kSamsungBitMark,
              kSamsungOneSpace, kSamsungBitMark, kSamsungZeroSpace,
              kSamsungBitMark, kSamsungMinGap, kSamsungMinMessageLength, data,
              nbits, 38, true, repeat, 33);
}

/// Construct a raw Samsung message from the supplied customer(address) &
/// command.
/// Status: STABLE / Should be working.
/// @param[in] customer The customer code. (aka. Address)
/// @param[in] command The command code.
/// @return A raw 32-bit Samsung message suitable for `sendSAMSUNG()`.
uint32_t IRsend::encodeSAMSUNG(const uint8_t customer, const uint8_t command) {
  uint8_t revcustomer = reverseBits(customer, sizeof(customer) * 8);
  uint8_t revcommand = reverseBits(command, sizeof(command) * 8);
  return ((revcommand ^ 0xFF) | (revcommand << 8) | (revcustomer << 16) |
          (revcustomer << 24));
}
#endif

#if DECODE_SAMSUNG
/// Decode the supplied Samsung 32-bit message.
/// Status: STABLE
/// @note Samsung messages whilst 32 bits in size, only contain 16 bits of
///   distinct data. e.g. In transmition order:
///   customer_byte + customer_byte(same) + address_byte + invert(address_byte)
/// @param[in,out] results Ptr to the data to decode & where to store the result
/// @param[in] offset The starting index to use when attempting to decode the
///   raw data. Typically/Defaults to kStartOffset.
/// @param[in] nbits The number of data bits to expect.
/// @param[in] strict Flag indicating if we should perform strict matching.
/// @return True if it can decode it, false if it can't.
/// @note LG 32bit protocol appears near identical to the Samsung protocol.
///   They differ on their compliance criteria and how they repeat.
/// @see http://elektrolab.wz.cz/katalog/samsung_protocol.pdf
bool IRrecv::decodeSAMSUNG(decode_results *results, uint16_t offset,
                           const uint16_t nbits, const bool strict) {
  if (strict && nbits != kSamsungBits)
    return false;  // We expect Samsung to be 32 bits of message.

  uint64_t data = 0;

  // Match Header + Data + Footer
  if (!matchGeneric(results->rawbuf + offset, &data,
                    results->rawlen - offset, nbits,
                    kSamsungHdrMark, kSamsungHdrSpace,
                    kSamsungBitMark, kSamsungOneSpace,
                    kSamsungBitMark, kSamsungZeroSpace,
                    kSamsungBitMark, kSamsungMinGap, true)) return false;
  // Compliance
  // According to the spec, the customer (address) code is the first 8
  // transmitted bits. It's then repeated. Check for that.
  uint8_t address = data >> 24;
  if (strict && address != ((data >> 16) & 0xFF)) return false;
  // Spec says the command code is the 3rd block of transmitted 8-bits,
  // followed by the inverted command code.
  uint8_t command = (data & 0xFF00) >> 8;
  if (strict && command != ((data & 0xFF) ^ 0xFF)) return false;

  // Success
  results->bits = nbits;
  results->value = data;
  results->decode_type = SAMSUNG;
  // command & address need to be reversed as they are transmitted LSB first,
  results->command = reverseBits(command, sizeof(command) * 8);
  results->address = reverseBits(address, sizeof(address) * 8);
  return true;
}
#endif

#if SEND_SAMSUNG36
/// Send a Samsung 36-bit formatted message.
/// Status: STABLE / Works on real devices.
/// @param[in] data The message to be sent.
/// @param[in] nbits The number of bits of message to be sent.
/// @param[in] repeat The number of times the command is to be repeated.
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/621
void IRsend::sendSamsung36(const uint64_t data, const uint16_t nbits,
                           const uint16_t repeat) {
  if (nbits < 16) return;  // To small to send.
  for (uint16_t r = 0; r <= repeat; r++) {
    // Block #1 (16 bits)
    sendGeneric(kSamsung36HdrMark, kSamsung36HdrSpace,
                kSamsung36BitMark, kSamsung36OneSpace,
                kSamsung36BitMark, kSamsung36ZeroSpace,
                kSamsung36BitMark, kSamsung36HdrSpace,
                data >> (nbits - 16), 16, 38, true, 0, kDutyDefault);
    // Block #2 (The rest, typically 20 bits)
    sendGeneric(0, 0,  // No header
                kSamsung36BitMark, kSamsung36OneSpace,
                kSamsung36BitMark, kSamsung36ZeroSpace,
                kSamsung36BitMark, kSamsungMinGap,  // Gap is just a guess.
                // Mask off the rest of the bits.
                data & ((1ULL << (nbits - 16)) - 1),
                nbits - 16, 38, true, 0, kDutyDefault);
  }
}
#endif  // SEND_SAMSUNG36

#if DECODE_SAMSUNG36
/// Decode the supplied Samsung36 message.
/// Status: STABLE / Expected to work.
/// @param[in,out] results Ptr to the data to decode & where to store the result
/// @param[in] offset The starting index to use when attempting to decode the
///   raw data. Typically/Defaults to kStartOffset.
/// @param[in] nbits The number of data bits to expect.
/// @param[in] strict Flag indicating if we should perform strict matching.
/// @return True if it can decode it, false if it can't.
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/621
bool IRrecv::decodeSamsung36(decode_results *results, uint16_t offset,
                             const uint16_t nbits, const bool strict) {
  if (results->rawlen < 2 * nbits + kHeader + kFooter * 2 - 1 + offset)
    return false;  // Can't possibly be a valid Samsung message.
  // We need to be looking for > 16 bits to make sense.
  if (nbits <= 16) return false;
  if (strict && nbits != kSamsung36Bits)
    return false;  // We expect nbits to be 36 bits of message.

  uint64_t data = 0;

  // Match Header + Data + Footer
  uint16_t used;
  used = matchGeneric(results->rawbuf + offset, &data,
                      results->rawlen - offset, 16,
                      kSamsung36HdrMark, kSamsung36HdrSpace,
                      kSamsung36BitMark, kSamsung36OneSpace,
                      kSamsung36BitMark, kSamsung36ZeroSpace,
                      kSamsung36BitMark, kSamsung36HdrSpace, false);
  if (!used) return false;
  offset += used;
  // Data (Block #2)
  uint64_t data2 = 0;
  if (!matchGeneric(results->rawbuf + offset, &data2,
                    results->rawlen - offset, nbits - 16,
                    0, 0,
                    kSamsung36BitMark, kSamsung36OneSpace,
                    kSamsung36BitMark, kSamsung36ZeroSpace,
                    kSamsung36BitMark, kSamsungMinGap, true)) return false;
  data <<= (nbits - 16);
  data += data2;

  // Success
  results->bits = nbits;
  results->value = data;
  results->decode_type = SAMSUNG36;
  results->command = data & ((1ULL << (nbits - 16)) - 1);
  results->address = data >> (nbits - 16);
  return true;
}
#endif  // DECODE_SAMSUNG36