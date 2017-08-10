#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "Arduino.h"

#include <vector>

#include "cue.h"
#include "schedule.h"

namespace freilite{
namespace iris{
namespace storage{
    namespace{
        //Storage for all cues currently loaded
        std::vector<Cue> loaded_cues;

        //Storage for all schedules currently loaded
        std::vector<delay_t> loaded_schedules;

        //Index map for schedules
        //For a schedule_id it stores the index where that schedule starts in loaded_schedules
        std::vector<uint8_t> schedule_indices;
    }

    //Load a cue
    void push_cue(const Cue& cue){
        loaded_cues.push_back(cue);
    }

    //Return reference to cue with ID cue_id
    Cue& get_cue(size_t cue_id){
        return loaded_cues[cue_id];
    }

    //Return number of loaded cues
    size_t number_of_cues(){
        return loaded_cues.size();
    }

    //Load a schedule element
    void push_schedule(delay_t schedule_element){
        //If a schedule delimiter is pushed, its index is added to the index map
        if (schedule_element.is_schedule_delimiter()){
            schedule_indices.push_back(loaded_schedules.size());
        }

        loaded_schedules.push_back(schedule_element);
    }

    //Return const iterator to starting schedule delimiter of schedule with ID schedule_id
    //Will return iterator to end of loaded_schedules if schedule_id is too large
    std::vector<delay_t>::const_iterator schedule_begin(size_t schedule_id){
        if(schedule_id >= schedule_indices.size()){
            return loaded_schedules.end();
        }
        return loaded_schedules.begin() + schedule_indices[schedule_id];
    }

    //Return const iterator pointing directly after end of schedule with ID schedule_id
    std::vector<delay_t>::const_iterator schedule_end(size_t schedule_id){
        //All schedules end before the beginning of the next schedule
        return schedule_begin(schedule_id + 1);
    }

    //Return number of loaded schedules
    size_t number_of_schedules(){
        return schedule_indices.size();
    }
}
}
}