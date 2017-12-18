#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "Arduino.h"

#include <EEPROM.h>

#include <vector>

#include "cue.h"
#include "schedule.h"
#include "communication.h"

namespace freilite{
namespace iris{
namespace storage{
    //Calculate size of actual information stored for cues and schedules
    size_t size_in_bytes(){
        return Cues::size_in_bytes() + Schedules::size_in_bytes();
    }

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
            communication::printf(F("ERROR: Can't write %u bytes to EEPROM, it's only %u bytes long."), total_size, EEPROM.length());
            return;
        }

        header_t header = { Cues::count(), Schedules::count() };

        //Iterator for the whole range
        iteratorT store_iter = begin;

        //Write header to range
        const uint8_t* header_byte_iter = static_cast<const uint8_t*>(static_cast<void*>(&header));

        for(int store_byte = 0; store_byte < sizeof(header); ++store_byte, ++store_iter, ++header_byte_iter){
            *store_iter = *header_byte_iter;
        }

        //Write cues to range
        const uint8_t* cues_byte_iter = static_cast<const uint8_t*>(static_cast<const void*>(&*Cues::begin()));

        for(int store_byte = 0; store_byte < sizeof(Cue) * Cues::count(); ++store_byte, ++store_iter, ++cues_byte_iter){
            *store_iter = *cues_byte_iter;
        }

        //Write schedules to range
        const uint8_t* schedules_byte_iter = static_cast<const uint8_t*>(static_cast<const void*>(&*Schedules::begin_by_id(0)));

        for(int store_byte = 0; store_byte < sizeof(delay_t) * Schedules::count(); ++store_byte, ++store_iter, ++schedules_byte_iter){
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
        Cues::clear();
        Schedules::clear();

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

            Cues::push(cue);
        }

        //Write schedules to range
        for(int store_byte = 0; store_byte < sizeof(delay_t) * header.number_of_schedule_elements;){
            //Build full schedule element to allow pushing it properly
            delay_t schedule_element;
            for(int schedule_byte = 0; schedule_byte < sizeof(delay_t); ++store_byte, ++schedule_byte, ++store_iter){
                if(store_iter >= end) return;
                static_cast<uint8_t*>(static_cast<void*>(&schedule_element))[schedule_byte] = *store_iter;
            }

            Schedules::push_element(schedule_element);
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