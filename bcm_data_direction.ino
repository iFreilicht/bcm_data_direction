#include "led_ring.h"

//Output buffer
char output[1000];

void setup()
{
    SerialUSB.begin(9600);

    led_ring::init();
}

//Delay in ms after which to repeat the main loop
const uint16_t LOOP_DELAY = 1000;

uint8_t animation_phase = 0;

void loop()
{
    using namespace led_ring;
    //Debugging output
    sprintf(output, "Animation Phase: %3i; Interrupts after %ums: %6u; FPS: %4u\n",
    animation_phase, LOOP_DELAY, interrupt_counter, (uint16_t)(((uint32_t)frame_counter)*1000/LOOP_DELAY/CHARLIE_PINS));
    SerialUSB.write(output);

    led_ring::print_debug_info();

    animation_phase = (animation_phase + 1) % 3;
    for(int i = 0; i < 12; i++){
        switch(animation_phase){
            case 0:
                draw_led(i, {50, 0, 0});
                break;
            case 1:
                draw_led(i, {0, 50, 0});
                break;
            case 2:
                draw_led(i, {0, 0, 50});
        }
    }

    reset_counters();

    delay(LOOP_DELAY);
}