//Define all types and functions to encode schedules
#pragma once

#include <ArduinoSTL.h>

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

    // SC DD || || PC || || SC SC PC SC DD ||

    // P := start of Period
    // S := start of Schedule (and of the first period of that Schedule)
    // D := Duration of schedule
    // C := cue ID
    // | := delay

    //When SC is followed by another SC or DC, this schedules duration is set to that of its first cue
    //When SC is followed by DD, that is interpreted to be its duration (16 bits)
    //When SC is followed by 0, the following two elements are interpreted to be its duration (32 bits)
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
        public: //non-static
            Schedule(size_t id) : id(id){}

            //Return true if this schedule is loaded
            inline bool exists(){
                return id < Schedules::count();
            }

            //Return const iterator to starting schedule delimiter of schedule
            inline std::vector<delay_t>::const_iterator begin(){
                return Schedules::begin_by_id(id);
            }

            //Return const iterator pointing directly after end of schedule
            inline std::vector<delay_t>::const_iterator end(){
                return Schedules::end_by_id(id);
            }

            typedef void (draw_callback_t)(size_t cue_id, uint32_t time, uint8_t draw_disabled_channels);
            void draw(draw_callback_t* draw_cue, uint32_t time){
                auto iter = this->begin();
                auto end_iter = this->end();

                size_t current_cue_id = iter->cue_id();
                uint32_t current_delay = 0;
                bool currently_on = true;
                bool relevant_delay_found = false;

                //Advance itertor to read schedule duration
                ++iter;

                //If schedule duration is specified, the effect is looped
                if (iter != end_iter && iter->is_delay() && iter->delay() != 0){
                    time = time % iter->delay();
                }

                while (true){
                    //Seek the next delimiter
                    if (relevant_delay_found){
                        continue;
                    }
                    //Prepare for parsing next period's delays
                    else if (iter->is_period_delimiter()){
                        current_cue_id = iter->cue_id();
                        current_delay = 0;
                        currently_on = true;
                        relevant_delay_found = false;
                    }
                    //Draw and end loop if end of schedule is reached
                    else if (iter == end_iter){
                        if (currently_on){
                            (*draw_cue)(current_cue_id, time, false);
                        }

                        break;
                    }
                    else {
                        current_delay += iter->delay();

                        //Found the relevant delay, now we know
                        //whether cue is on or off at this point in time
                        if (current_delay > time){
                            relevant_delay_found = true;

                            if (currently_on){
                                (*draw_cue)(current_cue_id, time, false);
                            }
                        }

                        currently_on = !currently_on;   //toggle state of cue
                    }

                    //Increment iterator
                    ++iter;
                };
            }
    };

}
}