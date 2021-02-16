/**
 *  @file    ArduinoAlarmDecoder.h
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
#ifndef _ALARMDECODER_API_H
#define _ALARMDECODER_API_H

#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <string>
#include <regex>
#include <bits/stdc++.h>
#include <map>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace std;

// types and defines

enum AD2_PARSER_STATES {
    AD2_PARSER_RESET            = 0,
    AD2_PARSER_SCANNING_START   = 1,
    AD2_PARSER_SCANNING_EOL     = 2,
    AD2_PARSER_PROCESSING       = 3
};

// The actual max is ~90 but leave some room for future.
// WARN: use int8_t so must be <= 127 for overflow protection.
#define ALARMDECODER_MAX_MESSAGE_SIZE 120

#define BIT_ON '1'
#define BIT_OFF '0'
#define BIT_UNDEFINED '-'

/** example Keypad Message.
[10000001000000003A--],008,[f70600ff1008001c08020000000000],"****DISARMED****  Ready to Arm  "
*/
// KPI
#define SECTION_1_START     0
#define SECTION_2_START    23
#define SECTION_3_START    27
#define SECTION_4_START    61
#define AMASK_START        30
#define AMASK_END          38
#define CURSOR_TYPE_POS    SECTION_3_START+19
#define CURSOR_POS         SECTION_3_START+21

// BIT/DATA OFFSETS
#define READY_BYTE          1
#define ARMED_AWAY_BYTE     2
#define ARMED_STAY_BYTE     3
#define BACKLIGHT_BYTE      4
#define PROGMODE_BYTE       5
#define BEEPMODE_BYTE       6
#define BYPASS_BYTE         7
#define ACPOWER_BYTE        8
#define CHIME_BYTE          9
#define ALARMSTICKY_BYTE   10
#define ALARM_BYTE         11
#define LOWBATTERY_BYTE    12
#define ENTRYDELAY_BYTE    13
#define FIRE_BYTE          14
#define SYSISSUE_BYTE      15
#define PERIMETERONLY_BYTE 16
#define SYSSPECIFIC_BYTE   17
#define PANEL_TYPE_BYTE    18
#define UNUSED_1_BYTE      19
#define UNUSED_2_BYTE      20

#define ADEMCO_PANEL       'A'
#define DSC_PANEL          'D'



/**
 * Utility functions/macros.
 */

#define AD2_NTOHL(x) ((((x) & 0xff000000UL) >> 24) | \
                    (((x) & 0x00ff0000UL) >>  8) | \
                    (((x) & 0x0000ff00UL) <<  8) | \
                    (((x) & 0x000000ffUL) << 24))

/**
 * Data structure for each Virtual partition state.
 *
 * https://www.alarmdecoder.com/wiki/index.php/Protocol
 *
 * The AlarmDecoder Alphanumeric Keypad Message has 4 sections.
 * 1) Bit Fields   - [00110011000000003A--]
 * 2) Numeric data - 010
 * 3) Raw Data     - [f70200010010808c18020000000000]
 *                      ^^^^^^^^ <- Address bytes
 * 4) Alpha Data   - "ARMED ***STAY** ZONE BYPASSED "
 *
 * The Partition/Address data is kept in section #3
 * bytes 2-9
 *  https://www.alarmdecoder.com/wiki/index.php/Protocol#Raw_data
 *
 * This 32 bit Little Endian word uses each bit to represent a single
 * address(Ademco) or partition(DSC) from 0 to 31.
 *
 * Ademco example
 *  0x02000100 = This message was for keypads at address 1 and 16.
 *
 * DSC example
 *  0x02000000 = This message was for partition #1
 *
 * To prevent mixing data between partitions a separate instance of this
 * storage structure will be used for each unique mask received from the
 * AlarmDecoder using the 32 bit address mask as a unique key.
 *
 * To determin the actual partition for each structure on Ademco it would be
 * necessary to have a translation between keypad address and partition.
 *
 */

class AD2VirtualPartitionState
{
public:

    // Address mask filter for this partition
    uint32_t address_mask_filter;

    // Partition number(external lookup required for Ademco)
    uint8_t partition;

    // Calculated from section #3(Raw)
    uint8_t display_cursor_type = 0;     // 0[OFF] 1[UNDERLINE] 2[INVERT]
    uint8_t display_cursor_location = 0; // 1-32

    // SECTION #1 data
    //  https://www.alarmdecoder.com/wiki/index.php/Protocol#Bit_field
    bool unknown_state = true;
    bool ready = false;
    bool armed_away = false;
    bool armed_stay = false;
    bool backlight_on = false;
    bool programming_mode = false;
    bool zone_bypassed = false;
    bool ac_power = false;
    bool chime_on = false;
    bool alarm_event_occurred = false;
    bool alarm_sounding = false;
    bool battery_low = false;
    bool entry_delay_off = false;
    bool fire_alarm = false;
    bool system_issue = false;
    bool perimeter_only = false;
    bool exit_now = false;
    uint8_t system_specific = 0;
    uint8_t beeps = 0;
    char panel_type = '?';
    bool unused1 = false;
    bool unused2 = false;

    std::string last_alpha_message = "";
    std::string last_numeric_message = "";

};

/**
 * Virtual partition states.
 * The key is a mask that groups all partitions messages together.
 */
typedef std::map<uint32_t, AD2VirtualPartitionState *> ad2pstates_t;

/**
 * API Subscriber callback function pointer type
 */
typedef void (*AD2ParserCallback_sub_t)(std::string*, AD2VirtualPartitionState*, void *arg);
typedef void (*AD2ParserCallbackRawRXData_sub_t)(uint8_t *, size_t len, void *arg);

/**
 * Callback event classes
 */
typedef enum {
    ON_RAW_MESSAGE = 1, ///< RAW panel message before parsing.
    ON_ARM,                    ///< ARMED
    ON_DISARM,                 ///< DISARMED
    ON_POWER_CHANGE,           ///< AC POWER STATE CHANGE
    ON_READY_CHANGE,           ///< READY STATE CHANGE
    ON_ALARM_CHANGE,           ///< ALARM BELL CHANGE
    ON_FIRE,                   ///< FIRE ALARM
    ON_ZONE_BYPASSED_CHANGE,   ///< BYPASS STATE CHANGE
    ON_BOOT,                   ///< AD2 Firmware boot
    ON_CONFIG_RECEIVED,        ///< AD2 CONFIG RECEIVED
    ON_ZONE_FAULT,             ///< ZONE FAULT EVENT
    ON_ZONE_RESTORE,           ///< ZONE RESTORE EVENT
    ON_LOW_BATTERY,            ///< LOW BATTERY EVENT
    ON_PANIC,                  ///< PANIC EVENT
    ON_CHIME_CHANGE,           ///< Chime state change
    ON_MESSAGE,                ///< ALPHA MESSAGE After parsing
    ON_REL,                    ///< !REL RELAY EVENT
    ON_EXP,                    ///< !EXP Zone Expander message
    ON_LRR,                    ///< !LRR Long Range Contact ID message
    ON_RFX,                    ///< !RFX 5800 RFX event with serial #
    ON_SENDING_RECEIVED,       ///< SEND finished ".done"
    ON_AUI,                    ///< !AUI message received
    ON_KPM,                    ///< !KPM message normal alpha with header.
    ON_KPE,                    ///< !KPE Keypad event message
    ON_CRC,                    ///< !CRC event message
    ON_VER,                    ///< !VER message received
    ON_ERR,                    ///< !ERR error event stats
    ON_EXIT_CHANGE,            ///< Placeholder: EXIT CHANGE event ID
    ON_SEARCH_MATCH,           ///< Placeholder: Pattern match subscriber match
    ON_FIRMWARE_VERSION,       ///< Placeholder: Firmware available event
    ON_RAW_RX_DATA
} ad2_event_t;

/**
 * Message Type ID's
 */
typedef enum {
    UNKOWN_MESSAGE_TYPE = 0,
    ALPHA_MESSAGE_TYPE,
    LRR_MESSAGE_TYPE,
    REL_MESSAGE_TYPE,
    EXP_MESSAGE_TYPE,
    RFX_MESSAGE_TYPE,
    AUI_MESSAGE_TYPE,
    KPM_MESSAGE_TYPE,
    KPE_MESSAGE_TYPE,
    CRC_MESSAGE_TYPE,
    VER_MESSAGE_TYPE,
    ERR_MESSAGE_TYPE,
    EVENT_MESSAGE_TYPE
} ad2_message_t;

/**
 * AD2 STATES enum.
 */
typedef enum {
    AD2_STATE_CLOSED = 0,
    AD2_STATE_OPEN,
    AD2_STATE_FAULT
} ad2_state_t;

/**
 * @brief EVENT Search virtual contact.
 *
 * A Virtual contact that is managed using filters and regular expressions
 * to match specific panel messages and states.
 *
 * - Trigger on message type and specific content of messages.
 *   - Serial numbers of 5800 Wireless or VPLEX device matching.
 *   - Specfic word or phrase in Alpha messages.
 *   - Relay or Expander event matching.
 *
 * - Different triggers for OPEN and CLOSE.
 *   - Useful for Contact ID messages that are often very different messages
 *     for OPEN and CLOSE.
 *
 * - Trigger on Zone tracking algorithm event.
 *   - TODO: Need to integrate zone tracking and work it in here.
 *
 * Example to search for and track 5800 wireless or VPLEX bus device
 *  with a serial number of 0123456.
 *
 *  AD2EventSearch *es = new AD2EventSearch(AD2_STATE_CLOSED, 0);
 *  es->PRE_FILTER_MESAGE_TYPE.push_back(RFX_MESSAGE_TYPE);
 *  es->PRE_FILTER_REGEX = "!RFX:0123456,.*";
 *  es->OPEN_REGEX_LIST.push_back("RFX:0123456,1.......");
 *  es->CLOSED_REGEX_LIST.push_back("RFX:0123456,0.......");
 *  es->FAULT_REGEX_LIST.push_back("RFX:0123456,......1.");
 *  es->OPEN_OUTPUT_FORMAT = "TEST SENSOR OPEN";
 *  es->CLOSED_OUTPUT_FORMAT = "TEST SENSOR CLOSE";
 *  es->FAULT_OUTPUT_FORMAT = "TEST SENSOR FAULT";
 *  AD2Parse.subscribeTo(on_search_match_cb, es);
 *
 */
class AD2EventSearch
{
    ///< Current state.
    int current_state_;

    ///< Default state to set on reset NO/NC set.
    ///< TODO: How will this work.
    int default_state_;

    /**
     * @brief Timeout set to default state when no restore event will happen.
     * In some cases to prevent a split brain situation we need to be sure
     * deafult states are restored over time. A value of 0 disables the timer.
     */
    int reset_time_;

public:
    AD2EventSearch()
        : current_state_(AD2_STATE_CLOSED)
        , default_state_(AD2_STATE_CLOSED)
        , reset_time_( 0 )
    { }

    AD2EventSearch(ad2_state_t default_state, int reset_time_in_ms)
        : current_state_(default_state)
        , default_state_(default_state)
        , reset_time_(reset_time_in_ms)
    { }

    // get/set current_state_
    int getState()
    {
        return current_state_;
    }
    void setState(int state)
    {
        current_state_ = state;
    }

    // get/set reset_time_
    int getResetTime()
    {
        return current_state_;
    }
    void setResetTime(int state)
    {
        current_state_ = state;
    }

    ///< List of MESSAGE TYPES to filter for.
    std::vector<ad2_message_t>
    PRE_FILTER_MESAGE_TYPE;

    /// REGEX search filter to eliminate messages from further tests that do not match.
    std::string PRE_FILTER_REGEX;

    ///< List of REGEX patterns when matched report an OPEN state.
    std::vector<std::string>
    OPEN_REGEX_LIST;

    ///< List of REGEX patterns when matched report a CLOSED state.
    std::vector<std::string>
    CLOSED_REGEX_LIST;

    ///< List of REGEX patterns when matched report a FAULT state.
    std::vector<std::string>
    FAULT_REGEX_LIST;

    ///< Vector for results of any regex groups '()'.
    std::vector<std::string>
    RESULT_GROUPS;

    ///< Output format string to pass group results with macros ${ON_OFF} ${OPEN_CLOSE} and more.
    ///< Must safely deal with mismatch of args to params.
    ///< ex. OPEN: "AUDIBLE ALARM ZONE ${GROUP0}"
    ///< ex. CLOSED: "CANCEL ALARM USER ${GROUP0}"
    ///< ex. OPEN: "FRONT DOOR ${OPEN_CLOSE}"
    ///< ex. CLOSED: "HVAC ${ON_OFF}"
    std::string OPEN_OUTPUT_FORMAT;
    std::string CLOSED_OUTPUT_FORMAT;
    std::string FAULT_OUTPUT_FORMAT;

    ///< Event message. Message that triggered a change.
    std::string last_message;

    ///< Formatted output results from event state change.
    std::string out_message;

    /// user supplied value
    int INT_ARG;
    void *PTR_ARG;
};

/**
 * Subscriber callback container
 */
class AD2SubScriber
{
public:
    void *fn;
    void *varg;
    int   iarg;
    AD2SubScriber(AD2ParserCallback_sub_t infn, void *inarg) : fn((void *)infn), varg(inarg), iarg(0) { }
    AD2SubScriber(AD2ParserCallback_sub_t infn, AD2EventSearch *inarg) : fn((void *)infn), varg((void *)inarg), iarg(0) { }
    AD2SubScriber(AD2ParserCallbackRawRXData_sub_t infn, void *inarg) : fn((void *)infn), varg((void*)(inarg)), iarg(0) { }
};

/**
 * Vector of subscribers type.
 */
typedef std::vector<AD2SubScriber> subscribers_t;

/**
 * Map by EVENT CLASS of lists of subscribers type.
 */
typedef std::map<ad2_event_t, subscribers_t> ad2subs_t;

/**
 * AlarmDecoder protocol parser class.
 *
 * Processes message fragments from AD2* protocol stream parsing
 * complete messages and update the internal state values. Allow
 * subscriptions for events to be called when specific state values
 * change.
 */
class AlarmDecoderParser
{
public:

    AlarmDecoderParser();

    // Subscribe to events by type.
    void subscribeTo(ad2_event_t evt, AD2ParserCallback_sub_t sub, void *arg);

    // Subscribe to events by regex patterns on raw messages and standard event patterns like 'ARMED' or 'READY'.
    // ZONES EVENTS are also tracked and can be used in patterns.
    void subscribeTo(AD2ParserCallback_sub_t fn, AD2EventSearch *event_search);

    // Subscibe to ON_RAW_RX_DATA events.
    void subscribeTo(AD2ParserCallbackRawRXData_sub_t fn, void *arg);

    // Push data into state machine. Events fire if a complete message is
    // received.
    bool put(uint8_t *buf, int8_t len);

    // Reset the parser state machine.
    void reset_parser();

    // Build a bitstring from a binary pointer
    std::string bin_to_binsz(size_t const size, void const * const ptr);

    // build a bitstring from a hex string
    std::string hex_to_binsz(void const * const ptr);

    // get AD2PPState by mask create if flag is set and no match found.
    AD2VirtualPartitionState * getAD2PState(int address, bool update=false);
    AD2VirtualPartitionState * getAD2PState(uint32_t *mask, bool update=false);

    // update firmware version trigger events to any subscribers
    void updateVersion(char *newversion);

    void test();

    // Human readable event id strings.
    std::map<int, const std::string> event_str = {
        {ON_ARM,                "ARMED"},
        {ON_DISARM,             "DISARMED"},
        {ON_POWER_CHANGE,       "POWER"},
        {ON_READY_CHANGE,       "READY"},
        {ON_ALARM_CHANGE,       "ALARM"},
        {ON_FIRE,               "FIRE"},
        /*      {ON_ZONE_BYPASS_CHANGE, "BYPASS"}, */
        /*      {ON_BOOT,               "BOOT"}, */
        /*      {ON_CONFIG_RECEIVED,    "CONFIG"}, */
        /*      {ON_ZONE_FAULT,         "ZONE FAULT"}, */
        /*      {ON_ZONE_RESTORE,       "ZONE RESTORE"}, */
        {ON_LOW_BATTERY,        "LOW BATTERY"},
        /*      {ON_PANIC,              "PANIC"}, */
        {ON_CHIME_CHANGE,       "CHIME"},
        {ON_MESSAGE,            "MESSAGE"},
        {ON_REL,                "RELAY"},
        {ON_EXP,                "EXPANDER"},
        {ON_LRR,                "CONTACT ID"},
        {ON_RFX,                "RFX"},
        /*      {ON_SENDING_RECEIVED,   "SEND ACK"}, */
        {ON_AUI,                "AUI"},
        {ON_KPM,                "KPM"},
        {ON_KPE,                "KPE"},
        {ON_CRC,                "CRC"},
        {ON_VER,                "VER"},
        {ON_ERR,                "ERR"},
    };

    std::map<int, const std::string> state_str = {
        {AD2_STATE_CLOSED,"CLOSED"},
        {AD2_STATE_OPEN,  "OPEN"},
        {AD2_STATE_FAULT, "FAULT"},
    };

    std::map<const std::string, ad2_message_t> message_type_id = {
        {"ALPHA", ALPHA_MESSAGE_TYPE},
        {"LRR",   LRR_MESSAGE_TYPE},
        {"REL",   REL_MESSAGE_TYPE},
        {"EXP",   EXP_MESSAGE_TYPE},
        {"RFX",   RFX_MESSAGE_TYPE},
        {"AUI",   AUI_MESSAGE_TYPE},
        {"KPM",   KPM_MESSAGE_TYPE},
        {"KPE",   KPE_MESSAGE_TYPE},
        {"CRC",   CRC_MESSAGE_TYPE},
        {"VER",   VER_MESSAGE_TYPE},
        {"ERR",   ERR_MESSAGE_TYPE},
        {"EVENT", EVENT_MESSAGE_TYPE},
    };

protected:
    // Track all panel states in separate class.
    ad2pstates_t AD2PStates;

    // Track all subscribers by subscription class.
    ad2subs_t AD2Subscribers;

    // @brief Notify a given subscriber group.
    void notifySubscribers(ad2_event_t ev, std::string &msg, AD2VirtualPartitionState *pstate);

    // @brief Notify raw data subscribers some bytes were received from the AD2*.
    // @note this currently happens before parsing.
    void notifyRawDataSubscribers(uint8_t *data, size_t len);

    // @brief Notify a given subscriber group.
    void notifySearchSubscribers(ad2_message_t mt, std::string &msg, AD2VirtualPartitionState *s);

    // Parser state control starts out as AD2_PARSER_RESET.
    int AD2_Parser_State;

    // ring buffer must be < 128 bytes using signed int8 for pointer variable.
    uint8_t ring_buffer[ALARMDECODER_MAX_MESSAGE_SIZE];
    int8_t ring_qin_position, ring_qout_position;
    uint16_t ring_error_count;

};



// Forward decls.
void ad2test(void);

// Utility functions.
bool is_bit_set(int pos, const char * bitStr);

#endif /* _ALARMDECODER_API_H */

