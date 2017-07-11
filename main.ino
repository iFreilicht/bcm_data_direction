#define BCM_RESOLUTION 8

volatile uint8_t bcm_frames [BCM_RESOLUTION] = {
    0x00, //bit0
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};

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

volatile uint8_t frame_index = 0;

//Storage for output over serial connection
uint16_t counts[BCM_RESOLUTION];
uint16_t compares[BCM_RESOLUTION];

uint16_t last_interrupt = 0;


//If one of these values is changed, the other NEEDS to be changed accordingly!
const uint8_t prescaler_setting = 0b00000010;
const uint16_t prescaler_factor = 8;

void setup()
{
    SerialUSB.begin(9600);

    PORTB = 0x00; //Pullups disabled and output at LOW
    DDRB = 0x00; //Everything set as input initially

    //Clear all Timer configuration flags, in case Arduino set them
    //This stops the timer, disconnects all output pins and sets mode to Normal
    TCCR3A &= 0x00;
    TCCR3B &= 0x00;
    TCCR3C &= 0x00;
    TIMSK3 &= 0x00;

    //Start timer and set prescaler
    TCCR3B |= prescaler_setting; //Set precaler to 1/64

    //Initial delay
    OCR3A = 1;

    // Enable Output Compare A Match Interrupt
    bitSet(TIMSK3, OCIE3A);
}

uint16_t brightness = 0;

uint16_t interrupt_counter = 0;

//Output buffer
char output[1000];

void loop()
{
    sprintf(output, "Brightness: %3i; Interrupts after 100ms: %6u;\n"
    "TCNT3: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
    "BrtMp: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
    "bcm_f: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
    "corrc: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n",
    brightness, interrupt_counter, 
    //counts is shifted by one byte as it is written to after frame_index was already advanced
    counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], counts[7], counts[0],
    bcm_brightness_map[0], bcm_brightness_map[1], bcm_brightness_map[2], bcm_brightness_map[3],
    bcm_brightness_map[4], bcm_brightness_map[5], bcm_brightness_map[6], bcm_brightness_map[7],
    bcm_frames[0], bcm_frames[1], bcm_frames[2], bcm_frames[3], bcm_frames[4], bcm_frames[5], bcm_frames[6], bcm_frames[7],
    bcm_delay_correction_offset[0], bcm_delay_correction_offset[1], bcm_delay_correction_offset[2], bcm_delay_correction_offset[3],
    bcm_delay_correction_offset[4], bcm_delay_correction_offset[5], bcm_delay_correction_offset[6], bcm_delay_correction_offset[7]);

    SerialUSB.write(output);

    set_brightness(brightness+1);
    interrupt_counter = 0;

    delay(1000);
}

void set_brightness(int value){
    brightness = value;
    if (brightness >= 0xFFFF >> (16 - BCM_RESOLUTION)) brightness = 0;

    for(int i = 0; i < BCM_RESOLUTION; i++){
        bcm_frames[i] = bitRead(brightness, i) == LOW ? 0x00: 0xFF;
    }

    //frame_index = 0;
}

ISR( TIMER3_COMPA_vect ){
    int old_sreg = SREG;
    cli(); //pause interrupts

    //debugging
    ++interrupt_counter;

    //advance frame index
    frame_index = (frame_index + 1) % BCM_RESOLUTION;

    //Loop unrolling
    if(frame_index == 0){
        while(frame_index < bcm_loop_unroll_amount){
            //draw frame
            DDRB = bcm_frames[frame_index];

            //log time for previous frame index and reset timer
            counts[frame_index] = TCNT3;
            TCNT3 = 0;

            //busy delay. the loop_2 function executes 4 cycles per iteration
            _delay_loop_2(
                (bcm_brightness_map[frame_index]) * (prescaler_factor/4) 
                - bcm_delay_correction_offset[frame_index]
            );

            frame_index++;
        }
    }

    //set delay for next bit (subtracting a correction amount to compensate
    //for "wasted" instructions inside this interrupt handler)
    OCR3A = bcm_brightness_map[frame_index] - bcm_delay_correction_offset[frame_index];

    //draw frame
    DDRB = bcm_frames[frame_index];

    counts[frame_index] = TCNT3; //log time for previous frame index
    TCNT3 = 0; //reset timer. This needs to happen directly after time logging to guarantee accurate results
    SREG = old_sreg; //turn on interrupts again

    //Adjust delay correction
    if(frame_index + 1 == BCM_RESOLUTION){
        for(uint8_t i = 0; i < BCM_RESOLUTION; i++){
            //counts is shifted by one byte as it is written to after frame_index was already advanced
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
                ++(bcm_delay_correction_offset[i]);
            }
        }
    }
}