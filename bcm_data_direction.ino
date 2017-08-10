#include <ArduinoSTL.h>
#include <EEPROM.h>

#include "led_ring.h"
#include "storage.h"

uint8_t cue_index = 0;

using namespace freilite::iris;

void setup()
{
    #if 0
    Cue new_cue= Cue();
    new_cue.ramp_type = RampType::linearRGB;
    new_cue.start_color = {255, 255, 255};
    new_cue.end_color = {0, 0, 0};
    storage::push_cue(new_cue);
    storage::push_schedule(delay_t(delimiter_flag_t::schedule, 0));

    new_cue = Cue();
    new_cue.duration = 500;
    new_cue.ramp_parameter = 250;
    new_cue.ramp_type = RampType::jump;
    new_cue.time_divisor = 6;
    new_cue.reverse = true;
    new_cue.start_color = {255, 50, 0};
    new_cue.end_color = {0, 50, 255};
    storage::push_cue(new_cue);
    storage::push_schedule(delay_t(delimiter_flag_t::schedule, 1));

    new_cue = Cue();
    new_cue.duration = 2000;
    new_cue.ramp_parameter = 1000;
    new_cue.ramp_type = RampType::linearRGB;
    new_cue.start_color = {0x00, 0xF3, 0xF3};
    new_cue.end_color = {0xEE, 0xF3, 0x00};
    storage::push_cue(new_cue);
    storage::push_schedule(delay_t(delimiter_flag_t::schedule, 2));

    storage::store_all_in_eeprom();
    #else
    storage::load_all_from_eeprom();
    #endif

    SerialUSB.begin(9600);

    led_ring::init();
}

//Delay in ms after which to repeat the main loop
const uint16_t LOOP_DELAY = 20;

void loop()
{
    //Debugging output
    std::printf("Interrupts after %ums: %6u; cue_index: %u; FPS: %4u\n",
    LOOP_DELAY, led_ring::interrupt_counter, cue_index, (uint16_t)(((uint32_t)led_ring::frame_counter)*1000/LOOP_DELAY/led_ring::CHARLIE_PINS));

    std::printf("Loaded cues: %u; Loaded schedules; %u\n",
    storage::number_of_cues(), storage::number_of_schedules());


    if(millis() % 3000 <= LOOP_DELAY){
        cue_index = (cue_index + 1) % storage::number_of_cues();
    }
 
    led_ring::print_debug_info();

    led_ring::reset_counters();

    //led_ring::draw_cue(cue_index, millis());

    led_ring::draw_schedule(cue_index, millis());

    delay(LOOP_DELAY);
}