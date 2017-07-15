#define BCM_RESOLUTION 8
#define CHARLIE_PINS 7

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
//First index, maximum is 3 (TODO: Increase this to 7)
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

//If one of these values is changed, the other NEEDS to be changed accordingly!
const uint8_t prescaler_setting = 0b00000010;
const uint16_t prescaler_factor = 8;

//Storage for output over serial connection
uint16_t counts[BCM_RESOLUTION];
uint16_t compares[BCM_RESOLUTION];
uint16_t interrupt_counter = 0;
uint16_t frame_counter = 0;

//Output buffer
char output[1000];

void setup()
{
    //Disconnect unused pins
    pinMode(12, INPUT);
    pinMode(7, INPUT);
    pinMode(6, INPUT);
    pinMode(5, INPUT);
    pinMode(4, INPUT);
    pinMode(3, INPUT);
    pinMode(2, INPUT);

    SerialUSB.begin(9600);

    PORTB = 0x00; //Pullups disabled and output at LOW
    DDRB = 0x00; //Everything set as input initially

    //Clear all Timer configuration flags, in case Arduino set them
    //This stops the timer, disconnects all output pins and sets mode to Normal
    TCCR3A &= 0x00;
    TCCR3B &= 0x00;
    TCCR3C &= 0x00;
    TIMSK3 &= 0x00;

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
    TCCR3B |= prescaler_setting; //Set precaler to 1/64

    //Initial delay
    OCR3A = 1;

    //Enable Output Compare A Match Interrupt
    bitSet(TIMSK3, OCIE3A);
}

//Brightness for demo animation
uint16_t brightness = 0;
uint8_t active_led_set = 0; //TODO: REMOVE THIS! Only for 3pin testing

//Set the brightness of three of the six connected LEDs 
//while leaving the other three off
void set_brightness(int value){
    brightness = value;
    if (brightness >= 0xFFFF >> (16 - BCM_RESOLUTION)){
        brightness = 0;
        active_led_set = !active_led_set;
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

//turn on all leds that have the specified pin wired as their sink pin
//only in effect until next timer interrupt!
void turn_on_all_leds(uint8_t pin){
    //set all pins as output
    DDRB = 0xff;
    //set sink pin to low, all others to high
    PORTB = ~(1 << pin);
}

//Delay in ms after which to repeat the main loop
const uint16_t loop_delay = 30;

void loop()
{
    //Debugging output
    sprintf(output, "Brightness: %3i; Interrupts after %ums: %6u; FPS: %4u\n"
    "TCNT3: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
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
    set_brightness(brightness + 1);

    //Reset counters 
    interrupt_counter = 0;
    frame_counter = 0;

    delay(loop_delay);
}

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

//Main interrupt for executing Bit Code Modulation
ISR( TIMER3_COMPA_vect ){
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
            counts[bit_index] = TCNT3;
            TCNT3 = 0;

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
    OCR3A = bcm_brightness_map[bit_index] - bcm_delay_correction_offset[bit_index];

    //draw frame
    PORTB = sink_mask_port & bcm_frames[frame_index][bit_index];
    DDRB = sink_mask_ddr | bcm_frames[frame_index][bit_index];

    counts[bit_index] = TCNT3; //log time for previous frame index
    TCNT3 = 0; //reset timer. This needs to happen directly after time logging to guarantee accurate results
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