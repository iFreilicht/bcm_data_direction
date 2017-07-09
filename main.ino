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
    [0] = 2,
    [1] = 4,
    [2] = 8,
    [3] = 16,
    [4] = 32,
    [5] = 64,
    [6] = 128,
    [7] = 256
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
const uint8_t prescaler_setting = 0b00000011;
const uint16_t prescaler_factor = 64;

void setup()
{
    SerialUSB.begin(9600);

    PORTC = 0x00; //Pullups disabled and output at LOW
    DDRC = 0x00; //Everything set as input initially

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
    "OCR3A: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n"
    "bcm_f: %6u, %6u, %6u, %6u, %6u, %6u, %6u, %6u\n", 
    brightness, interrupt_counter, 
    counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], counts[7],
    compares[0], compares[1], compares[2], compares[3], compares[4], compares[5], compares[6], compares[7],
    bcm_frames[0], bcm_frames[1], bcm_frames[2], bcm_frames[3], bcm_frames[4], bcm_frames[5], bcm_frames[6], bcm_frames[7]);

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
    TCCR3B &= 0b11111000; //stop timer
    int old_sreg = SREG;
    cli(); //pause interrupts

    //debugging
    ++interrupt_counter;
    counts[frame_index] = TCNT3;
    compares[frame_index] = OCR3A;

    //advance frame index
    frame_index = (frame_index + 1) % BCM_RESOLUTION;

    //set delay for next bit
    OCR3A = bcm_brightness_map[frame_index] - 1;

    //draw frame
    DDRC = bcm_frames[frame_index];

    //Reset timer again to ensure that no match has been skipped
    TCNT3 = 0;
    SREG = old_sreg; //turn on interrupts again
    TCCR3B |= prescaler_setting; //start timer again
}