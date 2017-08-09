#include "led_ring.h"

//Output buffer
char output[100];

uint8_t cue_index = 0;

void setup()
{
    Cue white_black_rgb = Cue();
    white_black_rgb.ramp_type = RampType::linearRGB;
    white_black_rgb.start_color = {255, 255, 255};
    white_black_rgb.end_color = {0, 0, 0};
    led_ring::active_cue = led_ring::loaded_cues[0] = white_black_rgb;

    Cue blue_orange_jump = Cue();
    blue_orange_jump.duration = 500;
    blue_orange_jump.ramp_parameter = 250;
    blue_orange_jump.ramp_type = RampType::jump;
    blue_orange_jump.time_divisor = 6;
    blue_orange_jump.reverse = true;
    blue_orange_jump.start_color = {255, 50, 0};
    blue_orange_jump.end_color = {0, 50, 255};
    led_ring::loaded_cues[1] = blue_orange_jump;

    Cue cyan_yellow_rgb = Cue();
    cyan_yellow_rgb.duration = 2000;
    cyan_yellow_rgb.ramp_parameter = 1000;
    cyan_yellow_rgb.ramp_type = RampType::linearRGB;
    cyan_yellow_rgb.start_color = {0x00, 0xF3, 0xF3};
    cyan_yellow_rgb.end_color = {0xEE, 0xF3, 0x00};
    led_ring::loaded_cues[2] = cyan_yellow_rgb;

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
        cue_index = (cue_index + 1) % led_ring::MAX_NUM_CUES;
        led_ring::active_cue = led_ring::loaded_cues[cue_index];
    }
 
    led_ring::print_debug_info();

    led_ring::reset_counters();

    //led_ring::draw_cue(led_ring::active_cue, millis());

    led_ring::draw_schedule(&led_ring::loaded_schedules[cue_index], millis());

    delay(LOOP_DELAY);
}