// Copyright 2018-2021 David Conran
/// @file
/// @brief Support for Samsung protocols.
/// Samsung originally added from https://github.com/shirriff/Arduino-IRremote/
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/505
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/621
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/1062
/// @see http://elektrolab.wz.cz/katalog/samsung_protocol.pdf
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/1538 (Checksum)
/// @see https://github.com/crankyoldgit/IRremoteESP8266/issues/1277 (Timers)

// Supports:
//   Brand: Samsung,  Model: UA55H6300 TV (SAMSUNG)
//   Brand: Samsung,  Model: BN59-01178B TV remote (SAMSUNG)
//   Brand: Samsung,  Model: UE40K5510AUXRU TV (SAMSUNG)
//   Brand: Samsung,  Model: DB63-03556X003 remote
//   Brand: Samsung,  Model: DB93-16761C remote
//   Brand: Samsung,  Model: IEC-R03 remote
//   Brand: Samsung,  Model: AK59-00167A Bluray remote (SAMSUNG36)
//   Brand: Samsung,  Model: AH59-02692E Soundbar remote (SAMSUNG36)
//   Brand: Samsung,  Model: HW-J551 Soundbar (SAMSUNG36)
//   Brand: Samsung,  Model: AR09FSSDAWKNFA A/C (SAMSUNG_AC)
//   Brand: Samsung,  Model: AR09HSFSBWKN A/C (SAMSUNG_AC)
//   Brand: Samsung,  Model: AR12KSFPEWQNET A/C (SAMSUNG_AC)
//   Brand: Samsung,  Model: AR12HSSDBWKNEU A/C (SAMSUNG_AC)
//   Brand: Samsung,  Model: AR12NXCXAWKXEU A/C (SAMSUNG_AC)
//   Brand: Samsung,  Model: AR12TXEAAWKNEU A/C (SAMSUNG_AC)
//   Brand: Samsung,  Model: DB93-14195A remote (SAMSUNG_AC)
//   Brand: Samsung,  Model: DB96-24901C remote (SAMSUNG_AC)

#ifndef IR_SAMSUNG_H_
#define IR_SAMSUNG_H_

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#ifndef UNIT_TEST
#include "String.h"
#endif
#include "IRremoteESP8266.h"
#include "IRsend.h"
#ifdef UNIT_TEST
#include "IRsend_test.h"
#endif


#endif  // IR_SAMSUNG_H_
