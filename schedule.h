//Define all types and functions to encode schedules
#pragma once

#include <ArduinoSTL.h>

#include <pb_encode.h>
#include <pb_decode.h>
namespace pb{
    #include "iris.pb.h"
}

namespace freilite{
namespace iris{

    /* Logically, this is the structure we want to represent:

        struct Period{
            uint8_t cue_id;
            uint16_t delays[];
        }

        struct Schedule{
            uint16_t duration;
            Period periods[];
        }
    */
    //However, the unknown size of Schedules and Periods makes byte structure tricky
    //(the overhead of an std::vector is 7 bytes, too much for our liking)
    //So they are implemented as an array of unions, which can look something like this
    //(each non-whitespace character is equivalent to one byte):

    // SC DD || || PC || || SC SC PC SC DD ||

    // P := start of Period
    // S := start of Schedule (and of the first period of that Schedule)
    // D := Duration of schedule
    // C := cue ID
    // | := delay

    //When SC is followed by another SC or DC, this schedules duration is not specified, see below
    //When SC is followed by DD, that is interpreted to be its duration (16 bits)
    //When SC is followed by 0, the duration is not specified and the cues will loop on their own
    //SC can not be followed by ||, it would be interpreted as DD.

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
                uint16_t delay; //Duration of schedule if previous element was a schedule delimiter
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

    //Storage for schedules
    namespace Schedules{
        namespace{
            //Storage for all schedules currently loaded
            std::vector<delay_t> loaded_schedules;

            //Index map for schedules
            //For a schedule_id it stores the index where that schedule starts in loaded_schedules
            std::vector<uint8_t> schedule_indices;
        }

        //Return const iterator to starting schedule delimiter of schedule with ID schedule_id
        //Will return iterator to end of loaded_schedules if schedule_id is too large
        static std::vector<delay_t>::const_iterator begin_by_id(size_t schedule_id){
            if(schedule_id >= schedule_indices.size()){
                return loaded_schedules.end();
            }
            return loaded_schedules.begin() + schedule_indices[schedule_id];
        }

        //Return const iterator pointing directly after end of schedule with ID schedule_id
        static std::vector<delay_t>::const_iterator end_by_id(size_t schedule_id){
            //All schedules end before the beginning of the next schedule
            return begin_by_id(schedule_id + 1);
        }

        //Load a schedule element
        static void push_element(delay_t schedule_element){
            //If a schedule delimiter is pushed, its index is added to the index map
            if (schedule_element.is_schedule_delimiter()){
                schedule_indices.push_back(loaded_schedules.size());
            }
            loaded_schedules.push_back(schedule_element);
        }

        //Unload all schedules
        static void clear(){
            loaded_schedules.clear();
            schedule_indices.clear();
        }

        //Return number of loaded schedules
        static size_t count(){
            return schedule_indices.size();
        }

        //Return number of schedule elements
        static size_t element_count(){
            return loaded_schedules.size();
        }

        //Calculate size of actual information stored for schedules
        static size_t size_in_bytes(){
            return loaded_schedules.size() *
                    sizeof(decltype(loaded_schedules)::value_type);
        }

        //Calculate overhead in bytes of schedules when stored in memory
        static size_t memory_overhead(){
            return sizeof(loaded_schedules) +
                    sizeof(schedule_indices) +
                    schedule_indices.size() *
                    sizeof(decltype(schedule_indices)::value_type);
        }
    }

    //Structure describing a single schedule
    struct Schedule{
        private:
            size_t id;

            //Holds begin and end iterator for one Period
            struct period_range_t{
                std::vector<delay_t>::const_iterator begin;
                std::vector<delay_t>::const_iterator end;
            };

            //Encode Periods inside a schedule as a nanopb callback
            static bool encode_periods(pb_ostream_t* stream,
                                       const pb_field_t* field,
                                       void* const* arg){
                using namespace pb;

                const Schedule* schedule = static_cast<const Schedule*>(*arg);
                auto iter = schedule->begin();
                auto end = schedule->end();

                //Prepare period for encoding delays
                pb::Schedule_Period pb_period = Schedule_Period_init_default;
                pb_period.cue_id = iter->cue_id();

                //Advance iterator
                if (schedule->duration() == INVALID_DELAY){
                    //Jump over Schedule delimiter
                    iter += 1;
                } else {
                    //Jump over Schedule delimiter and duration
                    iter += 2;
                }

                //Create range
                period_range_t period_range = {iter, iter};
                pb_period.delays.funcs.encode = &encode_delays;
                pb_period.delays.arg = &period_range;

                while(true){
                    //Reached end of this period
                    if(iter >= end || iter->is_delimiter()){
                        //Send period as message
                        period_range.end = iter;

                        if(!pb_encode_tag_for_field(stream, field))
                            return false;
                        if(!pb_encode_submessage(stream, pb::Schedule_Period_fields, &pb_period))
                            return false;

                        //Stop when schedule is over
                        if(iter >= end || iter->is_schedule_delimiter()){
                            break;
                        }

                        //Reset range for next period
                        period_range.begin = iter + 1; //Skip delimiter
                        pb_period.cue_id = iter->cue_id();
                    }

                    //Increment iterator
                    ++iter;
                }

                return true;
            }

            //Encode Periods inside a period as a nanopb callback
            static bool encode_delays(pb_ostream_t* stream,
                                      const pb_field_t* field,
                                      void* const* arg){
                const period_range_t* period_range = static_cast<const period_range_t*>(*arg);
                for(auto iter = period_range->begin; iter < period_range->end; ++iter){
                    //Use non-packed repeated field for now
                    if(!pb_encode_tag_for_field(stream, field))
                        return false;
                    //Encode actual delay
                    if(!pb_encode_varint(stream, iter->delay()))
                        return false;
                }
            }

        public: //non-static
            Schedule(size_t id) : id(id){}

            //Return duration of this schedule
            uint16_t duration() const{
                //If there is a duration specified, it's directly after
                //the schedule delimiter
                auto iter = begin() + 1;
                if (iter != end() && iter->is_delay()){
                    return iter->delay();
                }
                //It is important to know whether the duration was
                //explicitly 0 or not set at all.
                else return INVALID_DELAY;
            }

            //Return true if this schedule is loaded
            inline bool exists() const{
                return id < Schedules::count();
            }

            //Return const iterator to starting schedule delimiter of schedule
            inline std::vector<delay_t>::const_iterator begin() const{
                return Schedules::begin_by_id(id);
            }

            //Return const iterator pointing directly after end of schedule
            inline std::vector<delay_t>::const_iterator end() const{
                return Schedules::end_by_id(id);
            }

            //Return as protobuf defined Schedule
            pb::Schedule as_pb_schedule(){
                using namespace pb;

                pb::Schedule pb_schedule = Schedule_init_default;

                auto duration = this->duration();
                pb_schedule.duration = duration == INVALID_DELAY ? 0 : duration;

                pb_schedule.periods = {};
                pb_schedule.periods.funcs.encode = &encode_periods;
                pb_schedule.periods.arg = this;

                return pb_schedule;
            }

            typedef void (draw_callback_t)(size_t cue_id, uint32_t time, uint8_t draw_disabled_channels);
            void draw(draw_callback_t* draw_cue, uint32_t time) const{
                auto iter = this->begin();
                auto end_iter = this->end();

                size_t current_cue_id = iter->cue_id();
                uint32_t current_delay = 0;
                bool currently_on = true;
                bool relevant_delay_found = false;

                //If schedule duration is specified, the effect is looped
                uint32_t schedule_duration = this->duration();
                if (schedule_duration == INVALID_DELAY){
                    //Jump over Schedule delimiter
                    iter += 1;
                } else {
                    //Loop the
                    if (schedule_duration != 0){
                        time = time % schedule_duration;
                    }
                    //Jump over Schedule delimiter and duration
                    iter += 2;
                }

                while (true){
                    //Prepare for parsing next period's delays
                    if (iter->is_period_delimiter()){
                        if (currently_on){
                            (*draw_cue)(current_cue_id, time, false);
                        }

                        current_cue_id = iter->cue_id();
                        current_delay = 0;
                        currently_on = true;
                        relevant_delay_found = false;
                    }
                    //Draw and end loop if end of schedule is reached
                    else if (iter >= end_iter){
                        if (currently_on){
                            (*draw_cue)(current_cue_id, time, false);
                        }

                        break;
                    }
                    //Seek the next delimiter
                    else if (relevant_delay_found){
                        continue;
                    }
                    //Found a delay
                    else {
                        current_delay += iter->delay();

                        //Found the relevant delay, now we know
                        //whether cue is on or off at this point in time
                        if (current_delay > time){
                            relevant_delay_found = true;
                        }
                        else{
                            //toggle state of cue
                            currently_on = !currently_on;
                        }
                    }

                    //Increment iterator
                    ++iter;
                };
            }
    };
}
}