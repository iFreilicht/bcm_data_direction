#include "led_ring.h"

//Output buffer
char output[100];

Cue test_cue = Cue();

void setup()
{
    test_cue.duration = 500;
    test_cue.ramp_parameter = 250;
    test_cue.ramp_type = RampType::jump;
    test_cue.time_divisor = 12;
    test_cue.start_color = {255, 50, 0};
    test_cue.end_color = {0, 50, 255}; 

    led_ring::active_cue = test_cue;

    SerialUSB.begin(9600);

    led_ring::init();
}

//Delay in ms after which to repeat the main loop
const uint16_t LOOP_DELAY = 1000;

void loop()
{
    //Debugging output
    sprintf(output, "Interrupts after %ums: %6u; FPS: %4u\n",
    LOOP_DELAY, led_ring::interrupt_counter, (uint16_t)(((uint32_t)led_ring::frame_counter)*1000/LOOP_DELAY/led_ring::CHARLIE_PINS));
    SerialUSB.write(output);

    led_ring::print_debug_info();

    led_ring::reset_counters();

    delay(LOOP_DELAY);
}