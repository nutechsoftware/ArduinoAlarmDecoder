/**
 *  @file    ArduinoAlarmDecoder.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    01/15/2020
 *  @version 1.0
 *
 *  @brief AlarmDecoder embedded state machine and parser
 *
 *  @copyright Copyright (C) 2020 Nu Tech Software Solutions, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include "ArduinoAlarmDecoder.h"



AlarmDecoderParser::AlarmDecoderParser() {

  // clear all callback pointers.
  ON_RAW_MESSAGE_CB = 0;
  ON_MESSAGE_CB = 0;
  ON_ARM_CB = 0;
  ON_DISARM_CB = 0;
  ON_POWER_CHANGE_CB = 0;
  ON_READY_CHANGE_CB = 0;
  ON_ALARM_CB = 0;
  ON_ALARM_RESTORED_CB = 0;
  ON_FIRE_CB = 0;
  ON_BYPASS_CB = 0;
  ON_BOOT_CB = 0;
  ON_CONFIG_RECEIVED_CB = 0;
  ON_ZONE_FAULT_CB = 0;
  ON_ZONE_RESTORE_CB = 0;
  ON_LOW_BATTERY_CB = 0;
  ON_PANIC_CB = 0;
  ON_RELAY_CHANGED_CB = 0;
  ON_CHIME_CHANGED_CB = 0;
  ON_MESSAGE_CB = 0;
  ON_EXPANDER_MESSAGE_CB = 0;
  ON_LRR_CB = 0;
  ON_RFX_CB = 0;
  ON_SENDING_RECEIVED_CB = 0;
  ON_AUI_CB = 0;
  ON_KPM_CB = 0;
  ON_KPE_CB = 0;
  ON_CRC_CB = 0;
  ON_VER_CB = 0;
  ON_ERR_CB = 0;

  // Reset the parser on init.
  reset_parser();

}

void AlarmDecoderParser::reset_parser() {
  // Initialize parser state machine state.
  AD2_Parser_State = AD2_PARSER_RESET;
}

/**
 * return a partition state structure by 32bit keypad/partition mask.
 */
AD2VirtualPartitionState * AlarmDecoderParser::getAD2PState(uint32_t *amask, bool update) {
  // Create or return a pointer to our partition storage class.
  AD2VirtualPartitionState *ad2ps = nullptr;

  // look for an exact match.
  if ( AD2PStates.find(*amask) == AD2PStates.end()) {

    // Not found create or find a mask that has at least
    // one bit in common and update its mask to include the new
    // bits. System partition mask is 0 and will skip this logic.
    if (*amask) {
        uint32_t foundkey = *amask;
        for (auto const& x : AD2PStates)
        {
          // Mask has matching bits use it instead.
          if (x.first & *amask) {
            ad2ps = AD2PStates[x.first];
            foundkey = x.first;
            break;
          }
        }

        // Not necessary but seems reasonable.
        // Update key to include new mask if it changed.
        if (foundkey != *amask && update) {
          // remove the old map entry.
          AD2PStates.erase(foundkey);
          // Add new one with mask of original + new.
          *amask |= foundkey;
          AD2PStates[*amask] = ad2ps;
        }
    }

    // Did not find entry. Make new.
    if (!ad2ps && update) {
        ad2ps = AD2PStates[*amask] = new AD2VirtualPartitionState;
        ad2ps->partition = AD2PStates.size();
    }

  } else {
    // Found. Grab the state class ptr.
    ad2ps = AD2PStates[*amask];
  }
  return ad2ps;
}

/**
 * Consume bytes from an AlarmDecoder stream into a small ring buffer
 * for processing.
 *
 * 1) Parse all of the data firing off events upon parsing a full message.
 *   Continue parsing data until all is consumed.
 */
bool AlarmDecoderParser::put(uint8_t *buff, int8_t len) {

  // All AlarmDecoder messages are '\n' terminated.
  // "!boot.....done" is the only state exists that needs notification
  // before the terminator in order to enter the boot loader.
  // If KPM config bit is set then all messages will start with '!'
  // If KPM config bit is not set(the default) then standard keypad state
  // messages start with '['.

  int8_t bytes_left = len;
  uint8_t *bp = buff;

  // Sanity check.
  if (len<=0) {
     return false;
  }

  // Consume all the bytes.
  while (bytes_left>0) {

    uint8_t ch;

    // Update state machine.
    switch (AD2_Parser_State) {

      // Reset the ring buffer state.
      case AD2_PARSER_RESET:

        ring_qin_position = 0;
        ring_qout_position = 0;

        // Initialize AlarmDecoder stream parsing ring buffer memory to 0s.
        memset(ring_buffer, 0, sizeof(ring_buffer));
        AD2_Parser_State = AD2_PARSER_SCANNING_START;
        break;

      // Wait for ALPHA/NUMERIC after an EOL
      case AD2_PARSER_SCANNING_START:
        // store local.
        ch = *bp;

        // start scanning for EOL as soon as we have a printable character.
        if (ch >31 && ch <127) {
          AD2_Parser_State = AD2_PARSER_SCANNING_EOL;
        } else {
          // Dump the byte.
          // Update remaining bytes counter and move ptr.
          bp++;
          bytes_left--;
        }
        break;

      // Consume bytes looking for terminator.
      case AD2_PARSER_SCANNING_EOL:
        // store local.
        ch = *bp;

        // Protection from external leaks or faults. Very unlikely to happen.
        // Do not consume the byte and try again after a reset.
        if (ring_qin_position > ALARMDECODER_MAX_MESSAGE_SIZE) {

          // Next state is RESET.
          AD2_Parser_State = AD2_PARSER_RESET;

          // Done.
          break;
        }

        // Protect from corrupt data skip and reset.
        // All bytes must be CR/LF or printable characters only.
        if (ch != '\r' && ch != '\n') {
          if (ch <32 || ch >126) {

            // Update remaining bytes counter and move ptr.
            bp++;
            bytes_left--;

            // Next state is RESET.
            AD2_Parser_State = AD2_PARSER_RESET;

            // Done.
            break;
          }
        }

        // Update remaining bytes counter and move ptr.
        bp++;
        bytes_left--;

        // Process full messages on CR or LF
        if ( ch == '\n' || ch == '\r') {

          // Next wait for start of next message
          AD2_Parser_State = AD2_PARSER_SCANNING_START;

          // FIXME: Make it better faster less cpu mem etc. String :(
          // Consume all of the data for this message from the ring buffer.
          String msg = "";
          while (ring_qout_position != ring_qin_position) {
            msg += (char)ring_buffer[ring_qout_position];
            // Reset to 0 at the end of the ring.
            if ( ++ring_qout_position >= ALARMDECODER_MAX_MESSAGE_SIZE ) {
              ring_qout_position = 0;
            }
          }

          // call ON_RAW_MESSAGE callback if enabled.
          if (ON_RAW_MESSAGE_CB) {
            ON_RAW_MESSAGE_CB(&msg, nullptr);
          }

          // Detect message type or error.
          // 1) Starts with !
          //     !boot, !EXP, !REL, etc, etc.
          // 2) Starts with [
          //    Standard status message.
          // All other cases are invalid
          //
          if (msg[0] == '!') {
            if (msg.startsWith("!LRR:")) {
              // call ON_LRR callback if enabled.
              if (ON_LRR_CB) {
                ON_LRR_CB(&msg, nullptr);
              }
            } else
            if (msg.startsWith("!REL:") || msg.startsWith("!EXP:")) {
              // call ON_EXPANDER_MESSAGE callback if enabled.
              if (ON_EXPANDER_MESSAGE_CB) {
                ON_EXPANDER_MESSAGE_CB(&msg, nullptr);
              }
            } else
            if (msg.startsWith("!RFX:")) {
              // call ON_RFX callback if enabled.
              if (ON_RFX_CB) {
                ON_RFX_CB(&msg, nullptr);
              }
            } else
            if (msg.startsWith("!AUI:")) {
              // call ON_AUI callback if enabled.
              if (ON_AUI_CB) {
                ON_AUI_CB(&msg, nullptr);
              }
            } else
            if (msg.startsWith("!KPM:")) {
              // FIXME: move parser below to function so it can be called here.
              // call ON_KPM callback if enabled.
              if (ON_KPM_CB) {
                ON_KPM_CB(&msg, nullptr);
              }
            } else
            if (msg.startsWith("!KPE:")) {
              // call ON_KPE callback if enabled.
              if (ON_KPE_CB) {
                ON_KPE_CB(&msg, nullptr);
              }
            } else
            if (msg.startsWith("!CRC:")) {
              // call ON_CRC callback if enabled.
              if (ON_CRC_CB) {
                ON_CRC_CB(&msg, nullptr);
              }
            } else
            if (msg.startsWith("!VER:")) {
              // Parse the version string.
              // call ON_VER callback if enabled.
              if (ON_VER_CB) {
                ON_VER_CB(&msg, nullptr);
              }
            } else
            if (msg.startsWith("!ERR:")) {
              // call ON_ERR callback if enabled.
              if (ON_ERR_CB) {
                ON_ERR_CB(&msg, nullptr);
              }
            }
          } else {
            // http://www.alarmdecoder.com/wiki/index.php/Protocol#Keypad
            if (msg[0] == '[') {

              // Excessive sanity check. Test a few static characters.
              // Length should be 94 bytes and end with ".
              // [00110011000000003A--],010,[f70700000010808c18020000000000],"ARMED ***STAY** ZONE BYPASSED "
              if (msg.length() == 94 && msg[93]=='"' && msg[22] == ',') {

                // First extract the 32 bit address mask from section #3
                // to use it as a storage key for the state.
                uint32_t amask = strtol(msg.substring(AMASK_START,
                                        AMASK_END).c_str(), nullptr, 16);

                amask = AD2_NTOHL(amask);
                // Ademco/DSC: MASK 00000000 = System
                // Ademco 00000001 is keypad address 0
                // Ademco 00000002 is keypad address 1
                // DSC    00000002 is partition 1
                // Ademco 40000000 is keypad address 30

                // Create or return a pointer to our partition storage class.
                AD2VirtualPartitionState *ad2ps = getAD2PState(&amask, true);

                // we should not need to test the validity of ad2ps with update=true
                // the function will return a value.

                // store key internal for easy use.
                ad2ps->address_mask_filter = amask;

                // Update the partition state based upon the new status message.
                // FIXME: Next.

                // Extract state bits from section #1
                ad2ps->ready = is_bit_set(READY_BYTE, msg.c_str());
                ad2ps->armed_away = is_bit_set(ARMED_AWAY_BYTE, msg.c_str());
                ad2ps->armed_home = is_bit_set(ARMED_HOME_BYTE, msg.c_str());
                ad2ps->backlight_on  = is_bit_set(BACKLIGHT_BYTE, msg.c_str());
                ad2ps->programming_mode = is_bit_set(PROGMODE_BYTE, msg.c_str());
                ad2ps->zone_bypassed = is_bit_set(BYPASS_BYTE, msg.c_str());
                ad2ps->ac_power = is_bit_set(ACPOWER_BYTE, msg.c_str());
                ad2ps->chime_on = is_bit_set(CHIME_BYTE, msg.c_str());
                ad2ps->alarm_event_occurred = is_bit_set(ALARMSTICKY_BYTE, msg.c_str());
                ad2ps->alarm_sounding = is_bit_set(ALARM_BYTE, msg.c_str());
                ad2ps->battery_low = is_bit_set(LOWBATTERY_BYTE, msg.c_str());
                ad2ps->entry_delay_off = is_bit_set(ENTRYDELAY_BYTE, msg.c_str());
                ad2ps->fire_alarm = is_bit_set(FIRE_BYTE, msg.c_str());
                ad2ps->system_issue = is_bit_set(SYSISSUE_BYTE, msg.c_str());
                ad2ps->perimeter_only = is_bit_set(PERIMETERONLY_BYTE, msg.c_str());
                ad2ps->system_specific = is_bit_set(SYSSPECIFIC_BYTE, msg.c_str());
                ad2ps->beeps = msg[BEEPMODE_BYTE];
                ad2ps->panel_type = msg[PANEL_TYPE_BYTE];

                // Extract the numeric value from section #2
                ad2ps->last_numeric_message = strtol(msg.substring(SECTION_2_START, SECTION_2_START+3).c_str(), 0, 10);

                // Extract the 32 char Alpha message from section #4.
                ad2ps->last_alpha_message = msg.substring(SECTION_4_START, SECTION_4_START+32);

                // Extract the cursor location and type from section #3
                ad2ps->display_cursor_type = (uint8_t) strtol(msg.substring(CURSOR_TYPE_POS, CURSOR_TYPE_POS+2).c_str(), 0, 16);
                ad2ps->display_cursor_location = (uint8_t) strtol(msg.substring(CURSOR_POS, CURSOR_POS+2).c_str(), 0, 16);

                // look at messages for specific some states.
                // FIXME: Multi language support
                // FIXME: system messages need to be tested. They should go into
                // partition 0 but it needs to be tested.
                ad2ps->exit_now = false;
                if (ad2ps->panel_type == 'A') { // Ademco Vista
                  if(ad2ps->last_alpha_message.indexOf("MAY EXIT NOW") >= 0) {
                    ad2ps->exit_now = true;
                  }
                } else
                if (ad2ps->panel_type == 'D') { // DSC Power Series
                  if(ad2ps->last_alpha_message.indexOf("QUICK EXIT") >= 0 ||
                     ad2ps->last_alpha_message.indexOf("EXIT DELAY") >= 0)
                  {
                    ad2ps->exit_now = true;
                  }
                }

                // FIXME: debugging / testing
                Serial.print("!DBG: SIZE(");
                Serial.print(AD2PStates.size());
                Serial.print(") PID(");
                Serial.print(ad2ps->partition);
                Serial.print(") MASK(");
                Serial.print(amask,BIN);
                Serial.print(") Ready(");
                Serial.print(ad2ps->ready);
                Serial.print(") Armed Away(");
                Serial.print(ad2ps->armed_away);
                Serial.print(") Armed Home(");
                Serial.print(ad2ps->armed_home);
                Serial.print(") Bypassed(");
                Serial.print(ad2ps->zone_bypassed);
                Serial.println(")");

                // call ON_MESSAGE callback if enabled.
                if (ON_MESSAGE_CB) {
                  ON_MESSAGE_CB(&msg, ad2ps);
                }
              }
            } else {
              //FIXME: Error
              Serial.println("!ERR: BAD PROTOCOL PREFIX.");
            }
          }


          // Do not save EOL into the ring. We are done for now.
          break;
        }

        // Still receiving a message.
        // Save this byte to our ring buffer and parse and keep waiting for EOL.
        ring_buffer[ring_qin_position] = ch;

        // Reset to 0 at the end of the ring
        if ( ++ring_qin_position >= ALARMDECODER_MAX_MESSAGE_SIZE ) {
          ring_qin_position = 0;
        }

        // If next qin address is qout then loose a byte and report error.
        if (ring_qin_position == ring_qout_position) {
          ring_error_count++;
          if ( ++ring_qout_position >= ALARMDECODER_MAX_MESSAGE_SIZE ) {
            ring_qout_position = 0;
          }
        }

        break;

      case AD2_PARSER_PROCESSING:
        // FIXME: TBD
        break;

      default:
        // FIXME: TBD
        break;
    }
  }

  return true;
}

/**
 * FIXME: test code
 */
void AlarmDecoderParser::test() {
  for (int x = 0; x < 10000; x++) {
    AD2PStates[1] = new AD2VirtualPartitionState;
    AD2PStates[1]->ready = false;
    delete AD2PStates[1];
  }
}

/**
 * setCB_ON_RAW_MESSAGE
 */
void AlarmDecoderParser::setCB_ON_RAW_MESSAGE(AD2ParserCallback_msg_t cb) {
  ON_RAW_MESSAGE_CB = cb;
}

/**
 * setCB_ON_ARM
 */
void AlarmDecoderParser::setCB_ON_ARM(AD2ParserCallback_msg_t cb) {
  ON_ARM_CB = cb;
}

/**
 * setCB_ON_DISARM
 */
void AlarmDecoderParser::setCB_ON_DISARM(AD2ParserCallback_msg_t cb) {
  ON_DISARM_CB = cb;
}

/**
 * setCB_POWER_CHANGE
 */
void AlarmDecoderParser::setCB_ON_POWER_CHANGE(AD2ParserCallback_msg_t cb) {
  ON_POWER_CHANGE_CB = cb;
}

/**
 * setCB_ON_READY_CHANGE
 */
void AlarmDecoderParser::setCB_ON_READY_CHANGE(AD2ParserCallback_msg_t cb) {
  ON_READY_CHANGE_CB = cb;
}

/**
 * setCB_ON_ALARM
 */
void AlarmDecoderParser::setCB_ON_ALARM(AD2ParserCallback_msg_t cb) {
  ON_ALARM_CB = cb;
}

/**
 * setCB_ON_ALARM_RESTORED
 */
void AlarmDecoderParser::setCB_ON_ALARM_RESTORED(AD2ParserCallback_msg_t cb) {
  ON_ALARM_RESTORED_CB = cb;
}

/**
 * setCB_ON_FIRE
 */
void AlarmDecoderParser::setCB_ON_FIRE(AD2ParserCallback_msg_t cb) {
  ON_FIRE_CB = cb;
}

/**
 * setCB_ON_BYPASS
 */
void AlarmDecoderParser::setCB_ON_BYPASS(AD2ParserCallback_msg_t cb) {
  ON_BYPASS_CB = cb;
}

/**
 * setCB_ON_BOOT
 */
void AlarmDecoderParser::setCB_ON_BOOT(AD2ParserCallback_msg_t cb) {
  ON_BOOT_CB = cb;
}

/**
 * setCB_ON_CONFIG_RECEIVED
 */
void AlarmDecoderParser::setCB_ON_CONFIG_RECEIVED(AD2ParserCallback_msg_t cb) {
  ON_CONFIG_RECEIVED_CB = cb;
}


/**
 * setCB_ON_ZONE_FAULT
 */
void AlarmDecoderParser::setCB_ON_ZONE_FAULT(AD2ParserCallback_msg_t cb) {
  ON_ZONE_FAULT_CB = cb;
}

/**
 * setCB_ON_ZONE_RESTORE
 */
void AlarmDecoderParser::setCB_ON_ZONE_RESTORE(AD2ParserCallback_msg_t cb) {
  ON_ZONE_RESTORE_CB = cb;
}

/**
 * setCB_ON_LOW_BATTERY
 */
void AlarmDecoderParser::setCB_ON_LOW_BATTERY(AD2ParserCallback_msg_t cb) {
  ON_LOW_BATTERY_CB = cb;
}

/**
 * setCB_ON_PANIC
 */
void AlarmDecoderParser::setCB_ON_PANIC(AD2ParserCallback_msg_t cb) {
  ON_PANIC_CB = cb;
}

/**
 * setCB_ON_RELAY_CHANGED
 */
void AlarmDecoderParser::setCB_ON_RELAY_CHANGED(AD2ParserCallback_msg_t cb) {
  ON_RELAY_CHANGED_CB = cb;
}

/**
 * setCB_ON_CHIME_CHANGED
 */
void AlarmDecoderParser::setCB_ON_CHIME_CHANGED(AD2ParserCallback_msg_t cb) {
  ON_CHIME_CHANGED_CB = cb;
}

/**
 * setCB_ON_MESSAGE
 */
void AlarmDecoderParser::setCB_ON_MESSAGE(AD2ParserCallback_msg_t cb) {
  ON_MESSAGE_CB = cb;
}

/**
 * setCB_ON_EXPANDER_MESSAGE
 */
void AlarmDecoderParser::setCB_ON_EXPANDER_MESSAGE(AD2ParserCallback_msg_t cb) {
  ON_EXPANDER_MESSAGE_CB = cb;
}

/**
 * setCB_ON_LRR
 */
void AlarmDecoderParser::setCB_ON_LRR(AD2ParserCallback_msg_t cb) {
  ON_LRR_CB = cb;
}

/**
 * setCB_ON_RFX
 */
void AlarmDecoderParser::setCB_ON_RFX(AD2ParserCallback_msg_t cb) {
  ON_RFX_CB = cb;
}

/**
 * setCB_ON_SENDING_RECEIVED
 */
void AlarmDecoderParser::setCB_ON_SENDING_RECEIVED(AD2ParserCallback_msg_t cb) {
  ON_SENDING_RECEIVED_CB = cb;
}

/**
 * setCB_ON_AUI
 */
void AlarmDecoderParser::setCB_ON_AUI(AD2ParserCallback_msg_t cb) {
  ON_AUI_CB = cb;
}

/**
 * setCB_ON_KPM
 */
void AlarmDecoderParser::setCB_ON_KPM(AD2ParserCallback_msg_t cb) {
  ON_KPM_CB = cb;
}

/**
 * setCB_ON_KPE
 */
void AlarmDecoderParser::setCB_ON_KPE(AD2ParserCallback_msg_t cb) {
  ON_KPE_CB = cb;
}

/**
 * setCB_ON_CRC
 */
void AlarmDecoderParser::setCB_ON_CRC(AD2ParserCallback_msg_t cb) {
  ON_CRC_CB = cb;
}

/**
 * setCB_ON_VER
 */
void AlarmDecoderParser::setCB_ON_VER(AD2ParserCallback_msg_t cb) {
  ON_VER_CB = cb;
}

/**
 * setCB_ON_ERR
 */
void AlarmDecoderParser::setCB_ON_ERR(AD2ParserCallback_msg_t cb) {
  ON_ERR_CB = cb;
}



/**
* function: is_bit_set
* check to see if bits are enabled in status field
*
* in: int
* description: position in buffer to check
*
* in: char *
* description: complete status field buffer
 *
*/
bool is_bit_set(int pos, const char * bitStr)
{
      int set;
      set = 0;

      if( bitStr[pos] == '0' || bitStr[pos] == BIT_UNDEFINED)
              set = 0;
      if( bitStr[pos] == BIT_ON )
              set = 1;

      return set;
}
