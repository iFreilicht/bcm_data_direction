//All functionality regarding the led_ring
#pragma once

#include <stdint.h>
#include "Arduino.h"

namespace led_ring{
    const uint8_t BCM_RESOLUTION = 8;
    const uint8_t CHARLIE_PINS = 7;
    const uint8_t NUM_CHANNELS = 12; //each channel has three LEDs

    //If one of these values is changed, the other NEEDS to be changed accordingly!
    const uint8_t prescaler_setting = 0b00000010;
    const uint16_t prescaler_factor = 8;

    //Storage for output over serial connection
    uint16_t counts[BCM_RESOLUTION];
    uint16_t compares[BCM_RESOLUTION];
    uint16_t interrupt_counter = 0;
    uint16_t frame_counter = 0;

    void reset_counters(){
        interrupt_counter = 0;
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

    //Brightness for demo animation
    uint16_t brightness = 0;

    //BCM Frames for one single frame of an animation
    //The first index is equivalent to the active source pin,
    //The second index to the active BCM bit.
    //Storing the values this way allows to just write one byte to 
    //the pin port each time a new bit starts in BCM
    volatile uint8_t bcm_frames [CHARLIE_PINS][BCM_RESOLUTION] = {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
    };

    //Indices for accessing bcm_frames:
    //First index, maximum is 6
    volatile uint8_t frame_index = 0;
    //Second index, maximum is BCM_RESOLUTION-1
    volatile uint8_t bit_index = 0;

    //Mapping of bits to time the bit is taking up in the Bit Code Modulation schedule
    //Normally bcm_brightness_map[bit] == 1 << bit, but for gamma correction
    //this can be adjusted
    const uint16_t bcm_brightness_map [BCM_RESOLUTION] = {
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

    //Struct for storing colors as simple RGB values
    struct Color{
        uint8_t R;
        uint8_t G;
        uint8_t B;
    };

    //Allowed second index values for color_channel_frame_map
    enum ColorIndex{
        Min,
        Red = Min,
        Green,
        Blue,
        Max = Blue
    };

    //Allowed thrid index values for color_channel_frame_map
    enum PinIndex{
        Sink,
        Source
    };

    //For each channel and colour, store the sink and source pin
    const uint8_t color_channel_frame_map [NUM_CHANNELS][3][2] = {
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

    //Draw colour to a single RGB LED
    void draw_led(uint8_t channel, Color color){
        //Unpack color components into array for easier acccess
        uint8_t color_components[3] = { [Red]=color.R, [Green]=color.G, [Blue]=color.B };

        for (uint8_t color_i = ColorIndex::Min; color_i <= ColorIndex::Max; color_i++){
            uint8_t sink_pin = color_channel_frame_map[channel][color_i][Sink];
            uint8_t source_pin = color_channel_frame_map[channel][color_i][Source];

            //Write data to bcm_frames
            for(uint8_t bit = 0; bit < BCM_RESOLUTION; bit++){
                bitWrite(bcm_frames[sink_pin][bit], source_pin, bitRead(color_components[color_i], bit));
            }
        }
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
    //specified in bcm_brightness_map
    const uint8_t bcm_loop_unroll_amount = 3;

    //Set the brightness of three of the six connected LEDs 
    //while leaving the other three off
    void set_brightness(int value){
        brightness = value;
        if (brightness >= 0xFFFF >> (16 - BCM_RESOLUTION)){
            brightness = 0;
        } 

        for(int i = 0; i < BCM_RESOLUTION; i++){
            if(bitRead(brightness, i) == HIGH){
                bcm_frames[0][i] = 0xff;
                bcm_frames[1][i] = 0xff;
                bcm_frames[2][i] = 0xff;
                bcm_frames[3][i] = 0xff;
                bcm_frames[4][i] = 0xff;
                bcm_frames[5][i] = 0xff;
                bcm_frames[6][i] = 0xff;
            } else {
                bcm_frames[0][i] = 0x00;
                bcm_frames[1][i] = 0x00;
                bcm_frames[2][i] = 0x00;
                bcm_frames[3][i] = 0x00;
                bcm_frames[4][i] = 0x00;
                bcm_frames[5][i] = 0x00;
                bcm_frames[6][i] = 0x00;
            }
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

        //Self-test, light all LEDs
        #if 0
        delay(1000);
        turn_on_all_leds(0); delay(1000); //5 LEDs on
        turn_on_all_leds(1); delay(1000); //5 LEDs on
        turn_on_all_leds(2); delay(1000); //5 LEDs on
        turn_on_all_leds(3); delay(1000); //5 LEDs on
        turn_on_all_leds(4); delay(1000); //5 LEDs on
        turn_on_all_leds(5); delay(1000); //5 LEDs on
        turn_on_all_leds(6); delay(1000); //6 LEDs on
        #endif

        //Start timer and set prescaler
        TCCR1B |= prescaler_setting; //Set precaler to 1/64

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

        //advance frame index
        bit_index = (bit_index + 1) % BCM_RESOLUTION;

        //Loop unrolling
        if(bit_index == 0){
            ++frame_counter;
            frame_index = (frame_index + 1) % CHARLIE_PINS;
            set_sink_pin(frame_index);

            while(bit_index < bcm_loop_unroll_amount){
                //draw frame
                PORTB = sink_mask_port & bcm_frames[frame_index][bit_index];
                DDRB = sink_mask_ddr | bcm_frames[frame_index][bit_index];

                //log time for previous frame index and reset timer
                counts[bit_index] = TCNT1;
                TCNT1 = 0;

                //busy delay. the loop_2 function executes 4 cycles per iteration
                _delay_loop_2(
                    (bcm_brightness_map[bit_index] - bcm_delay_correction_offset[bit_index]) 
                    * (prescaler_factor/4)
                );

                bit_index++;
            }
        }

        //set delay for next bit (subtracting a correction amount to compensate
        //for "wasted" instructions inside this interrupt handler)
        OCR1A = bcm_brightness_map[bit_index] - bcm_delay_correction_offset[bit_index];

        //draw frame
        PORTB = sink_mask_port & bcm_frames[frame_index][bit_index];
        DDRB = sink_mask_ddr | bcm_frames[frame_index][bit_index];

        counts[bit_index] = TCNT1; //log time for previous frame index
        TCNT1 = 0; //reset timer. This needs to happen directly after time logging to guarantee accurate results
        SREG = old_sreg; //turn on interrupts again

        //Adjust delay correction
        if(bit_index + 1 == BCM_RESOLUTION){
            for(uint8_t i = 0; i < BCM_RESOLUTION; i++){
                //counts is shifted by one byte as it is written to after bit_index was already advanced
                uint16_t measured_count = counts[(i+1) % BCM_RESOLUTION];
                uint16_t target_count = bcm_brightness_map[i];

                //Underflow doesn't need to be checked for, it can not occur
                //We only increment/decrement as the delay correction is updated very frequently
                //That way they also work for interrupt und loop unrolled bits

                if(target_count > measured_count){
                    //correction is subtracted from timer/delay values,
                    //so if we measure too few counts, the correction is too high
                    --(bcm_delay_correction_offset[i]);
                } else if (target_count < measured_count){
                    //if we measure to many counts, the correction is too low
                    if(bcm_delay_correction_offset[i] + 1 < bcm_brightness_map[i])
                        ++(bcm_delay_correction_offset[i]);
                }
            }
        }
    }
}