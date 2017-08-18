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
    struct header_t{
        //This value is important to differentiate between byte data of cues and schedules
        size_t number_of_cues;
        size_t number_of_schedule_elements;
    };


    //Write all cues and schedules as binary data starting at an iterator
    //If maximum_size is specified, writing will only take place if the data fits
    template<typename iteratorT>
    void store_all(iteratorT begin, size_t maximum_size = 0){
        size_t total_size = size_in_bytes() + sizeof(header_t);
        if(maximum_size && total_size > maximum_size){
            printf("ERROR: Can't write %u bytes to EEPROM, it's only %u bytes long.", total_size, EEPROM.length());
            return;
        }

        header_t header = { loaded_cues.size(), loaded_schedules.size() };

        //Iterator for the whole range
        iteratorT store_iter = begin;

        //Write header to range
        const uint8_t* header_byte_iter = static_cast<const uint8_t*>(static_cast<void*>(&header));

        for(int store_byte = 0; store_byte < sizeof(header); ++store_byte, ++store_iter, ++header_byte_iter){
            *store_iter = *header_byte_iter;
        }

        //Write cues to range
        const uint8_t* cues_byte_iter = static_cast<const uint8_t*>(static_cast<void*>(&*loaded_cues.begin()));

        for(int store_byte = 0; store_byte < sizeof(Cue) * loaded_cues.size(); ++store_byte, ++store_iter, ++cues_byte_iter){
            *store_iter = *cues_byte_iter;
        }

        //Write schedules to range
        const uint8_t* schedules_byte_iter = static_cast<const uint8_t*>(static_cast<void*>(&*loaded_schedules.begin()));

        for(int store_byte = 0; store_byte < sizeof(delay_t) * loaded_schedules.size(); ++store_byte, ++store_iter, ++schedules_byte_iter){
            *store_iter  = *schedules_byte_iter;
        }
    }

    //Stores all cues and schedules to eeprom
    void store_all_in_eeprom(){
        store_all(EEPROM.begin(), EEPROM.length());
    }

    //Load all cues and schedules from binary data between two input-iterators
    template<typename iteratorT>
    void load_all(iteratorT begin, iteratorT end){
        clear_cues();
        clear_schedules();

        //Iterator for the whole range
        iteratorT store_iter = begin;

        //Read header from range
        header_t header;
        uint8_t* header_byte_iter = static_cast<uint8_t*>(static_cast<void*>(&header));

        for(int store_byte = 0; store_byte < sizeof(header); ++store_byte, ++store_iter, ++header_byte_iter){
            if(store_iter >= end) return;
            *header_byte_iter = *store_iter;
        }

        //Load cues from range
        for(int store_byte = 0; store_byte < sizeof(Cue) * header.number_of_cues;){
            //Build full cue to allow pushing it properly
            Cue cue;
            for(int cue_byte = 0; cue_byte < sizeof(Cue); ++store_byte, ++cue_byte, ++store_iter){
                if(store_iter >= end) return;
                static_cast<uint8_t*>(static_cast<void*>(&cue))[cue_byte] = *store_iter;
            }

            push_cue(cue);
        }

        //Write schedules to range
        for(int store_byte = 0; store_byte < sizeof(delay_t) * header.number_of_schedule_elements;){
            //Build full schedule element to allow pushing it properly
            delay_t schedule_element;
            for(int schedule_byte = 0; schedule_byte < sizeof(delay_t); ++store_byte, ++schedule_byte, ++store_iter){
                if(store_iter >= end) return;
                static_cast<uint8_t*>(static_cast<void*>(&schedule_element))[schedule_byte] = *store_iter;
            }

            push_schedule(schedule_element);
        }
    }

        //Loads all cues and scheduels stored in EEPROM.
    //WARNING! This will automatically clear cues and schedules!
    void load_all_from_eeprom(){
        load_all(EEPROM.begin(), EEPROM.end());
    }
}
}
}