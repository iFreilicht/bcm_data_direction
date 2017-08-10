#include <ArduinoSTL.h>

#include "led_ring.h"
#include "storage.h"

//Output buffer
char output[100];

uint8_t cue_index = 0;

using namespace freilite::iris;

void setup()
{
    storage::loaded_cues.resize(3);
    storage::loaded_schedules.resize(3);

    Cue white_black_rgb = Cue();
    white_black_rgb.ramp_type = RampType::linearRGB;
    white_black_rgb.start_color = {255, 255, 255};
    white_black_rgb.end_color = {0, 0, 0};
    storage::loaded_cues[0] = white_black_rgb;
    storage::loaded_schedules[0] = delay_t(delimiter_flag_t::schedule, 0);

    Cue blue_orange_jump = Cue();
    blue_orange_jump.duration = 500;
    blue_orange_jump.ramp_parameter = 250;
    blue_orange_jump.ramp_type = RampType::jump;
    blue_orange_jump.time_divisor = 6;
    blue_orange_jump.reverse = true;
    blue_orange_jump.start_color = {255, 50, 0};
    blue_orange_jump.end_color = {0, 50, 255};
    storage::loaded_cues[1] = blue_orange_jump;
    storage::loaded_schedules[1] = delay_t(delimiter_flag_t::schedule, 1);

    Cue cyan_yellow_rgb = Cue();
    cyan_yellow_rgb.duration = 2000;
    cyan_yellow_rgb.ramp_parameter = 1000;
    cyan_yellow_rgb.ramp_type = RampType::linearRGB;
    cyan_yellow_rgb.start_color = {0x00, 0xF3, 0xF3};
    cyan_yellow_rgb.end_color = {0xEE, 0xF3, 0x00};
    storage::loaded_cues[2] = cyan_yellow_rgb;
    storage::loaded_schedules[2] = delay_t(delimiter_flag_t::schedule, 2);

    SerialUSB.begin(9600);

    led_ring::init();
}

//Delay in ms after which to repeat the main loop
const uint16_t LOOP_DELAY = 10;

void loop()
{
    //Debugging output
    sprintf(output, "Interrupts after %ums: %6u; FPS: %4u\n",
    LOOP_DELAY, led_ring::interrupt_counter, (uint16_t)(((uint32_t)led_ring::frame_counter)*1000/LOOP_DELAY/led_ring::CHARLIE_PINS));
    SerialUSB.write(output);

    if(millis() % 3000 <= LOOP_DELAY){
        cue_index = (cue_index + 1) % storage::loaded_cues.size();
    }
 
    led_ring::print_debug_info();

    led_ring::reset_counters();

    //led_ring::draw_cue(cue_index, millis());

    led_ring::draw_schedule(cue_index, millis());

    delay(LOOP_DELAY);
}