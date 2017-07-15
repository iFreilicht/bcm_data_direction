#include "led_ring.h"

//Output buffer
char output[1000];

void setup()
{
    SerialUSB.begin(9600);

    led_ring::init();
}

//Delay in ms after which to repeat the main loop
const uint16_t loop_delay = 1000;

void loop()
{
    using namespace led_ring;
    //Debugging output
    sprintf(output, "Brightness: %3i; Interrupts after %ums: %6u; FPS: %4u\n"
    "TCNT1: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
    "BrtMp: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
    "bcm_f: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
    "corrc: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n",
    brightness, loop_delay, interrupt_counter, frame_counter*1000/loop_delay/CHARLIE_PINS,
    //counts is shifted by one byte as it is written to after bit_index was already advanced
    counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], counts[7], counts[0],
    bcm_brightness_map[0], bcm_brightness_map[1], bcm_brightness_map[2], bcm_brightness_map[3],
    bcm_brightness_map[4], bcm_brightness_map[5], bcm_brightness_map[6], bcm_brightness_map[7],
    bcm_frames[0][0], bcm_frames[0][1], bcm_frames[0][2], bcm_frames[0][3], bcm_frames[0][4], bcm_frames[0][5], bcm_frames[0][6], bcm_frames[0][7],
    bcm_delay_correction_offset[0], bcm_delay_correction_offset[1], bcm_delay_correction_offset[2], bcm_delay_correction_offset[3],
    bcm_delay_correction_offset[4], bcm_delay_correction_offset[5], bcm_delay_correction_offset[6], bcm_delay_correction_offset[7]);

    SerialUSB.write(output);

    //Advance animation
    //set_brightness(brightness + 1);

    brightness = (brightness + 1) % 3;
    for(int i = 0; i < 12; i++){
        switch(brightness){
            case 0:
                draw_led(i, {255, 0, 0});
                break;
            case 1:
                draw_led(i, {0, 255, 0});
                break;
            case 2:
                draw_led(i, {0, 0, 255});
        }
    }

    reset_counters();

    delay(loop_delay);
}