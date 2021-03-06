//Define all types and functions to encode cues
#pragma once

#include <Arduino.h>

#include "color.h"

#include <pb_encode.h>
#include <pb_decode.h>
namespace pb{
    #include "iris.pb.h"
}

namespace freilite{
namespace iris{
    // This definition needs to be equivalent
    // protobuf definition
    // We re-define it because the enum defined by nanopb cannot be
    // guaranteed to be just one byte long
    enum class RampType : uint8_t{
        jump = 0,
        linearRGB = 1,
        linearHSL = 2
    };

    struct Cue{
        uint16_t channels : 12; //bitmask, currently unused
        bool reverse : 1;
        bool wrap_hue : 1;
        uint8_t time_divisor;
        uint16_t delay; //currently unused
        uint32_t duration; //in ms
        RampType ramp_type;
        uint32_t ramp_parameter; //Maximum is equal to duration

        Color start_color;
        Color end_color;
        Color offset_color; //currently unused

        Cue() :
            channels(0b111111111111),
            reverse(false),
            wrap_hue(false),
            time_divisor(12),
            delay(0),
            duration(1000),
            ramp_type(RampType::jump),
            ramp_parameter(1000),

            start_color{0,0,0},
            end_color{0,0,0},
            offset_color{0,0,0}
        {}

        Color interpolate(uint32_t time, uint8_t channel){
            channel = reverse ? channel : 11 - channel;
            time += ( duration / time_divisor ) * channel;

            //effect will restart
            time = time % duration;

            Color result{};
            switch(ramp_type){
                case RampType::jump:
                    if(time > ramp_parameter){
                        return end_color;
                    } else {
                        return start_color;
                    }
                case RampType::linearHSL:
                    //NOT IMPLEMENTED YET!
                    return {255, 255, 255};
                case RampType::linearRGB:
                    result.R = linear_transition(start_color.R, end_color.R, time);
                    result.G = linear_transition(start_color.G, end_color.G, time);
                    result.B = linear_transition(start_color.B, end_color.B, time);
                    return result;
            }
        }

        // Return as protobuf-defined Cue
        pb::Cue as_pb_cue(){
            using namespace pb;

            pb::Cue pb_cue = Cue_init_default;

            pb_cue.channels = {};
            pb_cue.channels.funcs.encode = &encode_channels;
            pb_cue.channels.arg = this;

            pb_cue.reverse = this->reverse;
            pb_cue.wrap_hue = this->wrap_hue;
            pb_cue.time_divisor = this->time_divisor;
            pb_cue.delay = this->delay;
            pb_cue.duration = this->duration;
            pb_cue.ramp_type = pb::Cue_RampType(this->ramp_type);
            pb_cue.reverse = this->reverse;

            pb_cue.start_color = color_to_pb_color(this->start_color);
            pb_cue.end_color = color_to_pb_color(this->end_color);
            pb_cue.offset_color = color_to_pb_color(this->offset_color);

            return pb_cue;
        }

        private:
            //Calculate point on linear transition between two values
            //It is required that (time <= duration)
            uint32_t linear_transition(uint32_t start, uint32_t end, uint32_t time){
                //we need to make sure that for each calculation,
                //the intermediate result is a positive integer
                uint32_t delta = start < end ? end - start : start - end;

                uint32_t summand = time < ramp_parameter ?
                    (delta * time) / ramp_parameter :
                    delta - (delta * (time - ramp_parameter))/(duration - ramp_parameter);

                return start < end ? start + summand : start - summand;
            }

            // Encode channels and write them to output stream
            static bool encode_channels(pb_ostream_t* stream,
                                 const pb_field_t* field,
                                 void* const* arg){
                const Cue* cue = static_cast<const Cue*>(*arg);
                // In proto3, all repeated fields are packed
                if(!pb_encode_tag(stream, PB_WT_STRING, field->tag))
                    return false;
                // Each value is either 0 or one, so it will only be 
                // one byte long
                if(!pb_encode_varint(stream, 12))
                    return false;

                for(int i = 0; i < 12; i++){
                    if(!pb_encode_varint(stream, bitRead(cue->channels, i)))
                        return false;
                }
                return true;
            }
    };

    //Storage for cues
    namespace Cues{
        namespace{
            //Storage for all cues currently loaded
            std::vector<Cue> loaded_cues;
        }

        //Return const iterator to begin of Cue storage
        static std::vector<Cue>::const_iterator begin(){
            return loaded_cues.begin();
        }

        //Return const iterator to end of Cue storage
        static std::vector<Cue>::const_iterator end(){
            return loaded_cues.end();
        }

        //Load a cue
        static void push(const Cue& cue){
            loaded_cues.push_back(cue);
        }

        //Unload all cues
        static void clear(){
            loaded_cues.clear();
        }

        //Return reference to cue with ID cue_id
        static Cue& get(size_t cue_id){
            return loaded_cues[cue_id];
        }

        //Return number of loaded cues
        static size_t count(){
            return loaded_cues.size();
        }

        //Calculate size of actual information stored for schedules
        static size_t size_in_bytes(){
            return loaded_cues.size() *
                    sizeof(decltype(loaded_cues)::value_type);
        }

        //Calculate overhead in bytes of schedules when stored in memory
        static size_t memory_overhead(){
            return sizeof(loaded_cues);
        }
    }
}
}
