#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "Arduino.h"

#include <EEPROM.h>

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

    //Unload all cues
    void clear_cues(){
        loaded_cues.clear();
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

    //Unload all schedules
    void clear_schedules(){
        loaded_schedules.clear();
        schedule_indices.clear();
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

    //Calculate size of actual information stored for cues and schedules
    size_t size_in_bytes(){
        return loaded_cues.size() * sizeof(loaded_cues[0]) + loaded_schedules.size() * sizeof(loaded_schedules[0]);
    }

    //Overhead in bytes of cues and schedules when stored in memory
    size_t memory_overhead = 
            sizeof(loaded_cues)      +
            sizeof(loaded_schedules) +
            sizeof(schedule_indices) +
            schedule_indices.size() * sizeof(schedule_indices[0]);

    //Additional info stored in EEPROM
    struct eeprom_header{
        //This value is important to differentiate between byte data of cues and schedules
        size_t number_of_cues;
        size_t number_of_schedule_elements;
    };

    //Stores all cues and schedules to eeprom
    void store_all_in_eeprom(){
        size_t total_size = size_in_bytes() + sizeof(eeprom_header);
        if(total_size > EEPROM.length()){
            printf("ERROR: Can't write %u bytes to EEPROM, it's only %u bytes long.", total_size, EEPROM.length());
            return;
        }

        eeprom_header header = { loaded_cues.size(), loaded_schedules.size() };

        //Iterator for the whole EEPROM
        auto eeprom_iter = EEPROM.begin();

        //Write header to EEPROM
        const uint8_t* header_byte_iter = static_cast<const uint8_t*>(static_cast<void*>(&header));
        auto eeprom_header_end = EEPROM.begin() + sizeof(header);

        for(; eeprom_iter < eeprom_header_end; ++eeprom_iter, ++header_byte_iter){
            (*eeprom_iter).update(*header_byte_iter);
        }

        //Write cues to EEPROM
        const uint8_t* cues_byte_iter = static_cast<const uint8_t*>(static_cast<void*>(&*loaded_cues.begin()));
        auto eeprom_cues_end = eeprom_header_end + sizeof(Cue) * loaded_cues.size();

        for(; eeprom_iter < eeprom_cues_end; ++eeprom_iter, ++cues_byte_iter){
            (*eeprom_iter).update(*cues_byte_iter);
        }

        //Write schedules to EEPROM
        const uint8_t* schedules_byte_iter = static_cast<const uint8_t*>(static_cast<void*>(&*loaded_schedules.begin()));
        auto eeprom_schedules_end = eeprom_cues_end + sizeof(delay_t) * loaded_schedules.size();

        for(; eeprom_iter < eeprom_schedules_end; ++eeprom_iter, ++schedules_byte_iter){
            (*eeprom_iter).update(*schedules_byte_iter);
        }
    }

    //Loads all cues and scheduels stored in EEPROM.
    //WARNING! This will automatically clear cues and schedules!
    void load_all_from_eeprom(){
        clear_cues();
        clear_schedules();

        eeprom_header header;

        //Iterator for the whole EEPROM
        auto eeprom_iter = EEPROM.begin();

        //Read header from EEPROM
        uint8_t* header_byte_iter = static_cast<uint8_t*>(static_cast<void*>(&header));
        auto eeprom_header_end = EEPROM.begin() + sizeof(header);

        for(; eeprom_iter < eeprom_header_end; ++eeprom_iter, ++header_byte_iter){
            *header_byte_iter = **eeprom_iter;
        }

        //Load cues from EEPROM
        auto eeprom_cues_end = eeprom_header_end + sizeof(Cue) * header.number_of_cues;

        while(eeprom_iter < eeprom_cues_end){
            //Build full cue to allow pushing it properly
            Cue cue;
            for(int byte_position = 0; byte_position < sizeof(Cue); ++byte_position, ++eeprom_iter){
                static_cast<uint8_t*>(static_cast<void*>(&cue))[byte_position] = **eeprom_iter;
            }

            push_cue(cue);
        }

        //Write schedules to EEPROM
        auto eeprom_schedules_end = eeprom_cues_end + sizeof(delay_t) * header.number_of_schedule_elements;

        while(eeprom_iter < eeprom_schedules_end){
            //Build full schedule element to allow pushing it properly
            delay_t schedule_element;
            for(int byte_position = 0; byte_position < sizeof(delay_t); ++byte_position, ++eeprom_iter){
                static_cast<uint8_t*>(static_cast<void*>(&schedule_element))[byte_position] = **eeprom_iter;
            }

            push_schedule(schedule_element);
        }
    }
}
}
}