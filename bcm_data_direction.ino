#include <ArduinoSTL.h>
#include <EEPROM.h>

#include "led_ring.h"
#include "storage.h"

uint8_t cue_index = 0;

using namespace freilite::iris;

void setup()
{
    #if 0
    Cue white_black_rgb = Cue();
    white_black_rgb.ramp_type = RampType::linearRGB;
    white_black_rgb.start_color = {255, 255, 255};
    white_black_rgb.end_color = {0, 0, 0};
    storage::push_cue(white_black_rgb);
    storage::push_schedule(delay_t(delimiter_flag_t::schedule, 0));

    Cue blue_orange_jump = Cue();
    blue_orange_jump.duration = 500;
    blue_orange_jump.ramp_parameter = 250;
    blue_orange_jump.ramp_type = RampType::jump;
    blue_orange_jump.time_divisor = 6;
    blue_orange_jump.reverse = true;
    blue_orange_jump.start_color = {255, 50, 0};
    blue_orange_jump.end_color = {0, 50, 255};
    storage::push_cue(blue_orange_jump);
    storage::push_schedule(delay_t(delimiter_flag_t::schedule, 1));

    Cue cyan_yellow_rgb = Cue();
    cyan_yellow_rgb.duration = 2000;
    cyan_yellow_rgb.ramp_parameter = 1000;
    cyan_yellow_rgb.ramp_type = RampType::linearRGB;
    cyan_yellow_rgb.start_color = {0x00, 0xF3, 0xF3};
    cyan_yellow_rgb.end_color = {0xEE, 0xF3, 0x00};
    storage::push_cue(cyan_yellow_rgb);
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