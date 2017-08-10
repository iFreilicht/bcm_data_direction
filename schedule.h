//Define all types and functions to encode schedules
#pragma once

#include "cue.h"

namespace freilite{
namespace iris{

    /* Logically, this is the structure we want to represent:

        struct Period{
            uint8_t cue_id;
            uint16_t delays[];
        }

        struct Schedule{
            Period periods[];
        }
    */
    //However, the unknown size of Schedules and Periods makes byte structure tricky
    //(the overhead of an std::vector is 7 bytes, too much for our liking)
    //So they are implemented as an array of unions, which can look something like this
    //(each non-whitespace character is equivalent to one byte):

    // SC || || || PC || || SC || SC || PC || ||

    // P := start of Period
    // S := start of Schedule (and of the first period of that Schedule)
    // C := cue ID
    // | := delay


    const uint8_t  MAXIMUM_CUE_ID   =   0xFE;
    const uint8_t  INVALID_CUE_ID   =   0xFF;
    const uint16_t MAXIMUM_DELAY    = 0xFDFE;
    const uint16_t INVALID_DELAY    = 0xFDFF;
    //The delay values could be a little larger, 
    //but the first two bytes being FD guarantees even INVALID_DELAY
    //to not be ambiguous with a period or schedule delimiter

    enum class delimiter_flag_t : uint8_t{
        period   = 0xFE,
        schedule = 0xFF
    };

    struct delay_t{
        private:
            union{
                struct{
                    delimiter_flag_t flag;
                    uint8_t cue_id;
                } delimiter;
                uint16_t delay;
            } _value;

        public:
            bool is_schedule_delimiter() const{
                return _value.delimiter.flag == delimiter_flag_t::schedule;
            }

            bool is_period_delimiter() const{
                return _value.delimiter.flag == delimiter_flag_t::period;
            }

            bool is_delimiter() const{
                return is_period_delimiter() || is_schedule_delimiter();
            }

            bool is_delay() const{
                return !is_delimiter();
            }

            uint8_t cue_id() const{
                return is_delimiter() ? _value.delimiter.cue_id : INVALID_CUE_ID;
            }

            uint16_t delay() const{
                return is_delay() ? _value.delay : INVALID_DELAY;
            }

            //Constructor for delimiter. Calling with no arguments yields 
            //a special delimiter that acts like a null-terminator
            delay_t(
                delimiter_flag_t delimiter_flag = delimiter_flag_t::schedule, 
                uint8_t cue_id = INVALID_CUE_ID
            ){
                _value.delimiter.flag = delimiter_flag;
                _value.delimiter.cue_id = cue_id <= MAXIMUM_CUE_ID ? cue_id : MAXIMUM_CUE_ID;
            }

            //Constructor for delay
            delay_t(uint16_t delay){
                _value.delay = delay;
            }
    };

}
}