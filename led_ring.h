//All functionality regarding the led_ring
#pragma once

#include <stdint.h>
#include "Arduino.h"

#include "cue.h"
#include "schedule.h"

#include "storage.h"

namespace freilite{
namespace iris{
namespace led_ring{
    const uint8_t BCM_RESOLUTION = 8;
    const uint8_t CHARLIE_PINS = 7;
    const uint8_t NUM_CHANNELS = 12; //each channel has three LEDs

    //If one of these values is changed, the other NEEDS to be changed accordingly!
    const uint8_t PRESCALER_SETTING = 0b00000010;
    const uint16_t PRESCALER_FACTOR = 8;

    //Storage for output over serial connection
    uint16_t counts[BCM_RESOLUTION];
    uint16_t compares[BCM_RESOLUTION];
    uint16_t interrupt_counter = 0;
    uint16_t line_counter = 0;
    uint16_t frame_counter = 0;

    void reset_counters(){
        interrupt_counter = 0;
        line_counter = 0;
        frame_counter = 0;
    }

    //turn on all leds that have the specified pin wired as their sink pin
    //only in effect until next timer interrupt!
    void turn_on_all_leds(uint8_t pin){
        //set all pins as output
        DDRB = 0xff;
        //set sink pin to low, all others to high
        PORTB = ~(1 << pin);
    }

    namespace {
        //these masks are applied before setting the data direction and port registers to prevent switching the sink pin
        uint8_t sink_mask_ddr = 0x00;
        uint8_t sink_mask_port = 0x00;

        //Set pin that'll sink current for the charlieplex display. Maximum value is 7
        void set_sink_pin(int value){
            //sink pin needs to be the only output pin, all others should be input
            sink_mask_ddr = 1 << value;
            //sink pin needs to be the only pin at LOW level, all others should be HIGH/input
            sink_mask_port = ~sink_mask_ddr;

            //set sink pin as output, all others as input
            DDRB = sink_mask_ddr;
            //set all pins to LOW/pullup deactivated
            PORTB = 0;
        }
    }

    //One single frame of an animation
    //The first index is equivalent to the active source pin,
    //The second index to the active BCM bit.
    //Storing the values this way allows to just write one byte to 
    //the pin port each time a new bit starts in BCM
    volatile uint8_t displayed_frame [CHARLIE_PINS][BCM_RESOLUTION] = {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
    };

    //Indices for accessing displayed_frame:
    //First index, maximum is 6
    volatile uint8_t line_index = 0;
    //Second index, maximum is BCM_RESOLUTION-1
    volatile uint8_t bit_index = 0;

    //Mapping of bits to time the bit is taking up in the Bit Code Modulation schedule
    //Normally BCM_BRIGHTNESS_MAP[bit] == 1 << bit, but for gamma correction
    //this can be adjusted
    const uint16_t BCM_BRIGHTNESS_MAP [BCM_RESOLUTION] = {
      //bit | delay in clock-cycles
        [0] = 8,
        [1] = 16,
        [2] = 32,
        [3] = 64,
        [4] = 128,
        [5] = 256,
        [6] = 512,
        [7] = 1024
    };

    namespace {
        //Allowed second index values for COLOR_CHANNEL_PIN_MAP
        enum ColorIndex{
            Min,
            Red = Min,
            Green,
            Blue,
            Max = Blue
        };

        //Allowed thrid index values for COLOR_CHANNEL_PIN_MAP
        enum PinIndex{
            Sink,
            Source
        };

        //For each channel and colour, store the sink and source pin
        const PROGMEM uint8_t COLOR_CHANNEL_PIN_MAP [NUM_CHANNELS][3][2] = {
            [0] = { 
                [Red]   = { [Sink]=0, [Source]=1 },
                [Green] = { [Sink]=1, [Source]=0 },
                [Blue]  = { [Sink]=5, [Source]=2 },
            },
            [1] = { 
                [Red]   = { [Sink]=6, [Source]=1 },
                [Green] = { [Sink]=2, [Source]=0 },
                [Blue]  = { [Sink]=0, [Source]=2 },
            },
            [2] = { 
                [Red]   = { [Sink]=2, [Source]=1 },
                [Green] = { [Sink]=3, [Source]=0 },
                [Blue]  = { [Sink]=1, [Source]=2 },
            },
            [3] = { 
                [Red]   = { [Sink]=3, [Source]=1 },
                [Green] = { [Sink]=4, [Source]=0 },
                [Blue]  = { [Sink]=6, [Source]=2 },
            },
            [4] = { 
                [Red]   = { [Sink]=4, [Source]=1 },
                [Green] = { [Sink]=5, [Source]=0 },
                [Blue]  = { [Sink]=3, [Source]=2 },
            },
            [5] = { 
                [Red]   = { [Sink]=5, [Source]=1 },
                [Green] = { [Sink]=6, [Source]=0 },
                [Blue]  = { [Sink]=4, [Source]=2 },
            },
            [6] = { 
                [Red]   = { [Sink]=3, [Source]=4 },
                [Green] = { [Sink]=4, [Source]=3 },
                [Blue]  = { [Sink]=2, [Source]=5 },
            },
            [7] = { 
                [Red]   = { [Sink]=6, [Source]=4 },
                [Green] = { [Sink]=5, [Source]=3 },
                [Blue]  = { [Sink]=3, [Source]=5 },
            },
            [8] = { 
                [Red]   = { [Sink]=5, [Source]=4 },
                [Green] = { [Sink]=0, [Source]=3 },
                [Blue]  = { [Sink]=4, [Source]=5 },
            },
            [9] = { 
                [Red]   = { [Sink]=0, [Source]=4 },
                [Green] = { [Sink]=1, [Source]=3 },
                [Blue]  = { [Sink]=6, [Source]=5 },
            },
            [10] = { 
                [Red]   = { [Sink]=1, [Source]=4 },
                [Green] = { [Sink]=2, [Source]=3 },
                [Blue]  = { [Sink]=0, [Source]=5 },
            },
            [11] = { 
                [Red]   = { [Sink]=2, [Source]=4 },
                [Green] = { [Sink]=6, [Source]=3 },
                [Blue]  = { [Sink]=1, [Source]=5 },
            }
        };

        inline uint8_t get_sink_pin(uint8_t channel, ColorIndex color_index){
            return pgm_read_byte( &( COLOR_CHANNEL_PIN_MAP[channel][color_index][Sink] ) );
        }

        inline uint8_t get_source_pin(uint8_t channel, ColorIndex color_index){
            return pgm_read_byte( &( COLOR_CHANNEL_PIN_MAP[channel][color_index][Source] ) );
        }
    }

    //Draw colour to a single RGB LED
    void draw_led(uint8_t channel, Color color){
        //Unpack color components into array for easier acccess
        uint8_t color_components[3] = { [Red]=color.R, [Green]=color.G, [Blue]=color.B };

        for (uint8_t color_i = ColorIndex::Min; color_i <= ColorIndex::Max; color_i++){
            uint8_t sink_pin = get_sink_pin(channel, (ColorIndex)color_i);
            uint8_t source_pin = get_source_pin(channel, (ColorIndex)color_i);

            //Write data to displayed_frame
            for(uint8_t bit = 0; bit < BCM_RESOLUTION; bit++){
                bitWrite(displayed_frame[sink_pin][bit], source_pin, bitRead(color_components[color_i], bit));
            }
        }
    }

    //Write a single line of cue to displayed_frame for the current timestep
    void draw_cue(size_t cue_id, uint32_t time, uint8_t draw_disabled_channels = true){
        if(cue_id >= storage::number_of_cues()) return;

        auto cue = storage::get_cue(cue_id);

        for(uint8_t channel = 0; channel < NUM_CHANNELS; channel++){
            //Only get non-black color if current channel is active
            if(bitRead(cue.channels, channel)){
                draw_led(channel, cue.interpolate(time, channel));
            }
            else if(draw_disabled_channels){
                //If desired, draw disabled channels as black
                draw_led(channel, {0,0,0});
            }
        }
    }

    //Write a single line of a Schedule starting at schedule_begin to displayed_frame for the current timestep
    void draw_schedule(size_t schedule_id, uint32_t time){
        Schedule schedule = Schedule(schedule_id);
        if (!schedule.exists()) return;

        schedule.draw(&draw_cue, time);
    }

    //Stores correction values to be subtracted from the counter values in the brightness map
    //They will be changed periodically if necessary
    //uint8 would probably be enough, u16 is used to prevent potential overflow
    uint16_t bcm_delay_correction_offset [BCM_RESOLUTION] = {
        0, 0, 0, 0, 0, 0, 0, 0
    };

    //specify how many of the first bits in BCM are displayed
    //in a single call. This is important when using high clock frequencies
    //as the timer interrupt might fire at a much later point than the one
    //the timer has actually crossed the output compare register at
    //
    //This value should be set experimentally to the lowest amount at which
    //all measured delays (in clockcycles) per bit are equal to those
    //specified in BCM_BRIGHTNESS_MAP
    const uint8_t BCM_LOOP_UNROLL_AMOUNT = 3;

    //Output buffer for string formatting
    char output_buffer[100];

    //Write debugging information to SerialUSB connection
    void print_debug_info(){
        std::printf(
            "TCNT1: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
            "BrtMp: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
            "Corrc: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n",
            //counts is shifted by one byte as it is written to after bit_index was already advanced
            counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], counts[7], counts[0],
            BCM_BRIGHTNESS_MAP[0], BCM_BRIGHTNESS_MAP[1], BCM_BRIGHTNESS_MAP[2], BCM_BRIGHTNESS_MAP[3],
            BCM_BRIGHTNESS_MAP[4], BCM_BRIGHTNESS_MAP[5], BCM_BRIGHTNESS_MAP[6], BCM_BRIGHTNESS_MAP[7],
            bcm_delay_correction_offset[0], bcm_delay_correction_offset[1], bcm_delay_correction_offset[2], bcm_delay_correction_offset[3],
            bcm_delay_correction_offset[4], bcm_delay_correction_offset[5], bcm_delay_correction_offset[6], bcm_delay_correction_offset[7]
        );
    }

    //draw one colour to all LEDs
    void draw_all_leds(Color color){
        for(int i = 0; i < 12; i++){
            draw_led(i, color);
        }
    }

    //Initialise pins and timers for LED ring
    void init(){
        PORTB = 0x00; //Pullups disabled and output at LOW
        DDRB = 0x00; //Everything set as input initially

        //Clear all Timer configuration flags, in case Arduino set them
        //This stops the timer, disconnects all output pins and sets mode to Normal
        TCCR1A &= 0x00;
        TCCR1B &= 0x00;
        TCCR1C &= 0x00;
        TIMSK1 &= 0x00;

        //Self-test
        #if 0
        delay(1000);
        //Test hardware
        turn_on_all_leds(0); delay(1000); //5 LEDs on
        turn_on_all_leds(1); delay(1000); //5 LEDs on
        turn_on_all_leds(2); delay(1000); //5 LEDs on
        turn_on_all_leds(3); delay(1000); //5 LEDs on
        turn_on_all_leds(4); delay(1000); //5 LEDs on
        turn_on_all_leds(5); delay(1000); //5 LEDs on
        turn_on_all_leds(6); delay(1000); //6 LEDs on
        //Test COLOR_CHANNEL_PIN_MAP
        draw_all_leds({50, 0, 0}); delay(1000); //All LEDs red
        draw_all_leds({0, 50, 0}); delay(1000); //All LEDs green
        draw_all_leds({0, 0, 50}); delay(1000); //All LEDs blue
        #endif

        //Start timer and set prescaler
        TCCR1B |= PRESCALER_SETTING; //Set precaler to 1/64

        //Initial delay
        OCR1A = 1;

        //Enable Output Compare A Match Interrupt
        bitSet(TIMSK1, OCIE1A);
    }

    //Main interrupt for executing Bit Code Modulation
    ISR( TIMER1_COMPA_vect ){
        int old_sreg = SREG;
        cli(); //pause interrupts

        //debugging
        ++interrupt_counter;

        //advance bit index
        bit_index = (bit_index + 1) % BCM_RESOLUTION;

        //Loop unrolling
        if(bit_index == 0){
            ++line_counter;
            ++line_index;
            //after all bits have been rendered,
            //draw the next line
            if(line_index == CHARLIE_PINS){
                line_index = 0;
                ++frame_counter;
            }
            set_sink_pin(line_index);

            while(bit_index < BCM_LOOP_UNROLL_AMOUNT){
                //draw line
                PORTB = sink_mask_port & displayed_frame[line_index][bit_index];
                DDRB = sink_mask_ddr | displayed_frame[line_index][bit_index];

                //log time for previous line index and reset timer
                counts[bit_index] = TCNT1;
                TCNT1 = 0;

                //busy delay. the loop_2 function executes 4 cycles per iteration
                _delay_loop_2(
                    (BCM_BRIGHTNESS_MAP[bit_index] - bcm_delay_correction_offset[bit_index]) 
                    * (PRESCALER_FACTOR/4)
                );

                bit_index++;
            }
        }

        //set delay for next bit (subtracting a correction amount to compensate
        //for "wasted" instructions inside this interrupt handler)
        OCR1A = BCM_BRIGHTNESS_MAP[bit_index] - bcm_delay_correction_offset[bit_index];

        //draw line
        PORTB = sink_mask_port & displayed_frame[line_index][bit_index];
        DDRB = sink_mask_ddr | displayed_frame[line_index][bit_index];

        counts[bit_index] = TCNT1; //log time for previous line index
        TCNT1 = 0; //reset timer. This needs to happen directly after time logging to guarantee accurate results
        SREG = old_sreg; //turn on interrupts again

        //Adjust delay correction
        if(bit_index + 1 == BCM_RESOLUTION){
            for(uint8_t i = 0; i < BCM_RESOLUTION; i++){
                //counts is shifted by one byte as it is written to after bit_index was already advanced
                uint16_t measured_count = counts[(i+1) % BCM_RESOLUTION];
                uint16_t target_count = BCM_BRIGHTNESS_MAP[i];

                //Underflow doesn't need to be checked for, it can not occur
                //We only increment/decrement as the delay correction is updated very frequently
                //That way they also work for interrupt und loop unrolled bits

                if(target_count > measured_count){
                    //correction is subtracted from timer/delay values,
                    //so if we measure too few counts, the correction is too high
                    --(bcm_delay_correction_offset[i]);
                } else if (target_count < measured_count){
                    //if we measure to many counts, the correction is too low
                    if(bcm_delay_correction_offset[i] + 1 < BCM_BRIGHTNESS_MAP[i])
                        ++(bcm_delay_correction_offset[i]);
                }
            }
        }
    }
}
}
}