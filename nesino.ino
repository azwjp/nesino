// enable設定
volatile byte enableFlag = 0b00001110; // 000 noiseShortFrag sq1 sq2 tri noi

// sq1 設定
volatile int sq1Freq = 45; // 0-2047 << 5
volatile bool sq1Env = false; // Envelope
volatile byte sq1Duty = 2; // 0-3
volatile byte sq1FC = 0; // FreqChange Freqency change amount
volatile bool sq1FCDirection = false; // Frequency change direction: true -> up
volatile byte sq1FCCount = 120; // Freqency change count
volatile bool sq1Sweep = false; // Flag of sweep enabled
volatile byte sq1Vol = 6; // Volume 0-15
// sq1 カウンタ
volatile byte sq1Pointer = -1; // Current phase
volatile int sq1Counter = 0; // divider

// sq2 設定
volatile int sq2Freq = 20;//85; // 0-2047 << 5
volatile bool sq2Env = false; // Envelope
volatile byte sq2Duty = 1; // 0-3
volatile byte sq2FC = 0; // FreqChange Freqency change amount
volatile bool sq2FCDirection = false; // Frequency change direction: true -> up
volatile byte sq2FCCount = 0; // Freqency change count
volatile bool sq2Sweep = false; // Flag of sweep enabled
volatile byte sq2Vol = 7; // Volume 0-15
// sq1 カウンタ
volatile byte sq2Pointer = -1; // Current phase
volatile int sq2Counter = 0; // divider


// tri 設定
volatile bool triEnable = false;
volatile int triFreq = 10;//85; // 0-2047 << 5
volatile bool triEnv = false; // Envelope
volatile byte triFC = 0; // FreqChange Freqency change amount
volatile bool FCDirection = false; // Frequency change direction: true -> up
volatile byte triFCCount = 0; // Freqency change count
volatile bool triSweep = false; // Flag of sweep enabled
// tri カウンタ
volatile byte triPointer = -1; // Current phase
volatile int triCounter = 0; // divider

// noise
volatile unsigned int noiseReg = 0x8000;
volatile int noiseCountMax = 0x20;
volatile byte noiseVol = 10; // Volume 0-15
// noise カウンタ
volatile int noiseCounter = 0;

volatile byte currentNoise = 0;

void setup() {
  cli();

  /* TIMER0: PWM for change the output level
     As faster as possible.
     The top is OCR0A, and it is set at 63 because the resolution needs 64.
     This means 250 kHz PWM.
     Change the value of OCR0B to change the output signal's level.
     OCR0B should be 0 to 63.
  */
  // PD6 (pin6) invalid, PD5 (pin5) valid. fast PWM. Compare match then HIGH to LOW. Top is OCR0A，
  TCCR0A = 0b10100011;
  TCCR0B |= _BV(WGM02);
  // no division
  TCCR0B = (TCCR0B & 0b11111000) | 0b00000001;
  // Top
  OCR0A = 63;
  // compare register -> output level
  OCR0B = 0;
  // no interrupts
  TIMSK0 = 0;

  /* TIMER2: interrupts when change the output level
     OCR2A should be 8.9 to set the frequency of NES.
     If too fast, the TIMER1 interrupts will fail.
  */
  // CTC
  TCCR2A = 0b00000010;
  TCCR2B &= ~_BV(WGM02);
  // no devision
  TCCR2B = (TCCR2B & 0b11111000) | 0b00000001;
  // compare register
  OCR2A = 75;
  // interrupt when TCNT2 == OCR2A
  TIMSK2 = _BV(OCIE2A);

  /* TIMER1: chenge the parameters of sound
   * NES is 240 fps
   */
  
  // CTC (ICR1)
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

  // 1/8
  TCCR1B = (TCCR1B & ~_BV(CS11)) | _BV(CS12) | _BV(CS10); // 1/1024
  // TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS10))) |_BV(CS11);

  // compare register
  // 16 MHz / frequency of interrupt (Hz)
  OCR1A = 8332;

  // interrupt when TCNT1 == OCR1A
  TIMSK1 = _BV(OCIE1A);

  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);
  sei();

/*
 * global register
 * r1: zero register
 * r2: output
 * r20: interrupt or enable flag, counterReflesh changeSound newFrame noiseShortFrag sq1Enabled sq2Enabled triangleEnabled noiseEnabled
 * 
 * used register
 * NOISE
 * r5, r6, r7, r16, r17, r18,r19 r24, r25
 * SQ1, 2
 * r5, r6, r7, r16, r24, r25
 * TRI
 * r6, r7, r16, r24, r25
 */

  asm volatile(
    "lds r20, enableFlag \n"
    
  "STARTLOOP: "
    "sbrs r20, 6 \n"
    "rjmp SOUNDCHANGE \n"
    "addi r20, 0xF0 \n"

  "SOUNDCHANGE"
    "sbrs r20, 7 \n"
    "rjmp STARTLOOP \n"

    "clr r2 \n"

// if noise is enabled {
    // if noise isn't enable, then branch
    "sbrs r20, 0 \n"
    "rjmp SQ1 \n"

// if (++noiseCounter == noiseCountMax)
    // [r25:r24 noiseCounter, r7:r6 noiseCountMax]
    "lds r24, noiseCounter \n"
    "lds r25, noiseCounter+1 \n"
    "adiw r24, 1 \n"
    "lds r6, noiseCountMax \n"
    "lds r7, noiseCountMax+1 \n"
    "cp r24, r6 \n"
    "cpc r25, r7 \n"
    "brne NOISE_NOT_UPDATE \n"

    // [r19:r18 noiseReg]
    "lds r18, noiseReg \n"
    "lds r19, noiseReg+0x1 \n"
    // noiseReg >>= 1
    "lsr r19 \n"
    "ror r18 \n"
    // noiseShortFreq ?
    "sbrs r20, 4 \n"
    "rjmp LONGFREQ \n"
    // noiseReg >> 6. Only least significant bit is needed -> write 1 bit on r17
    // [r17 (noiseReg >> 6) & 1, r19:r18 noiseReg]
    "clr r17 \n"
    "sbrc r18, 6 \n" // If 6th bit is 0, then skip. 1 then don't skip and r17 is 1.
    "ldi r17, 1 \n"
    "rjmp DONE \n"
    // noiseReg >> 1
  "LONGFREQ:"
    // [r17 ((noiseReg >> 1)^noiseReg)&1, r19:r18 noiseReg]
    "mov r17, r18 \n"
    "lsr r17 \n"
  "DONE:"
    "eor r17, r18 \n"
    // & 1) << 15
    // [r16 15shifted, r17 last 1 bit, r19:r18 noiseReg]
    "clr r16 \n"
    "lsr r17 \n"
    "ror r16 \n"
    // [r16 15-shifted, r19:r18 noiseReg]
    // Only the most significant bit is to 0 or 1. The rest is 0 because of "<< 15". The result is r17
    // noiseReg r19 |= r16
    "or r19, r16 \n"
    "sts noiseReg, r18 \n"
    "sts noiseReg+1, r19 \n"
    // if roiseReg & 1, then output noiseVol. otherwise 0;
    "sbrs r18, 0 \n"
    "lds r2, noiseVol \n"

    "sts noiseCounter, r1 \n" // noiseCounter = 0
    "sts noiseCounter+1, r1 \n"
    "sts currentNoise, r2 \n" // currentNoise = output

    "rjmp SQ1 \n"

// if noise counter is not max
  "NOISE_NOT_UPDATE:"
    // [r25:r24 noiseCounter]
    "sts noiseCounter, r24 \n"
    "sts noiseCounter+1, r25 \n"
    "lds r2, currentNoise \n"
    "rjmp SQ1 \n"

  
// if sq1 is enabled
  "SQ1: "
    "sbrs r20, 3 \n"
    "rjmp SQ2 \n"

    // [r16 sq1Pointer]
    "lds r16, sq1Pointer \n"
//  if (++sq1Counter == sq1Freq) {
    // [r16 sq1Pointer, r25:r24 sq1Counter, r7:r6 sq1Freq]
    "lds r24, sq1Counter \n"
    "lds r25, sq1Counter+1 \n"
    "adiw r24, 1 \n"
    "lds r6, sq1Freq \n"
    "lds r7, sq1Freq+1 \n"
    "cp r24, r6 \n"
    "cpc r25, r7 \n"
    // [r16 sq1Pointer, r25:r24 sq1Counter]
    "breq SQ1_UPDATE \n"
    "sts sq1Counter, r24 \n"
    "sts sq1Counter+1, r25 \n"
    "rjmp SQ1_ENABLE \n"
  "SQ1_UPDATE: "
    // [r16 sq1Pointer]
    "sts sq1Counter, r1 \n"
    "sts sq1Counter+1, r1 \n"
    "inc r16 \n"
    "sts sq1Pointer, r16 \n"
    // [r16 sq1Pointer, r5 sq1Duty]
  "SQ1_ENABLE:"
    // ((pointer & 0b111) <= (duty == 0 ? duty << 1 : duty << 1 - 1)) ? 1 : 0
    "andi r16, 0b111 \n" // pointer's lowest 3 bit
    "lds r5, sq1Duty \n"
    "lsl r5 \n"
    "cpse r5, r1 \n"
    "dec r5 \n"
    "cp r5, r16 \n" // duty < pointer
    "brlo SQ2 \n"
    // [r16 sq1Pointer, r5 sq1Vol]
    "lds r5, sq1Vol \n"
    "add r2, r5 \n"


// if sq2 is enabled
  "SQ2: "
    "sbrs r20, 2 \n"
    "rjmp TRI \n"

    // [r16 sq2Pointer]
    "lds r16, sq2Pointer \n"
//  if (++sq2Counter == sq2Freq) {
    // [r16 sq2Pointer, r25:r24 sq2Counter, r7:r6 sq2Freq]
    "lds r24, sq2Counter \n"
    "lds r25, sq2Counter+1 \n"
    "adiw r24, 1 \n"
    "lds r6, sq2Freq \n"
    "lds r7, sq2Freq+1 \n"
    "cp r24, r6 \n"
    "cpc r25, r7 \n"
    // [r16 sq2Pointer, r25:r24 sq2Counter]
    "breq SQ2_UPDATE \n"
    "sts sq2Counter, r24 \n"
    "sts sq2Counter+1, r25 \n"
    "rjmp SQ2_ENABLE \n"
  "SQ2_UPDATE: "
    // [r16 sq2Pointer]
    "sts sq2Counter, r1 \n"
    "sts sq2Counter+1, r1 \n"
    "inc r16 \n"
    "sts sq2Pointer, r16 \n"
    // [r16 sq2Pointer, r5 sq2Duty]
  "SQ2_ENABLE:"
    // ((pointer & 0b111) <= (duty == 0 ? duty << 1 : duty << 1 - 1)) ? 1 : 0
    "andi r16, 0b111 \n" // pointer's lowest 3 bit
    "lds r5, sq2Duty \n"
    "lsl r5 \n"
    "cpse r5, r1 \n"
    "dec r5 \n"
    "cp r5, r16 \n" // duty < pointer
    "brlo TRI \n"
    // [r16 sq1Pointer, r5 sq2Vol]
    "lds r5, sq2Vol \n"
    "add r2, r5 \n"


// if tri is enabled
  "TRI: "
    "sbrs r20, 1 \n"
    "rjmp OUTPUT \n"
    
    // [r16 triPointer]
    "lds r16, triPointer \n"
//  if (++triCounter == triFreq) {
    // [r16 triPointer, r25:r24 triCounter, r7:r6 triFreq]
    "lds r24, triCounter \n"
    "lds r25, triCounter+1 \n"
    "adiw r24, 1 \n"
    "lds r6, triFreq \n"
    "lds r7, triFreq+1 \n"
    "cp r24, r6 \n"
    "cpc r25, r7 \n"
    // [r16 triPointer, r25:r24 triCounter]
    "breq TRI_UPDATE \n"
    "sts triCounter, r24 \n"
    "sts triCounter+1, r25 \n"
    "rjmp TRI_ENABLE \n"
  "TRI_UPDATE: "
    // [r16 triPointer]
    "sts triCounter, r1 \n"
    "sts triCounter+1, r1 \n"
    "inc r16 \n"
    "sts triPointer, r16 \n"
  "TRI_ENABLE:"
    // [r16 triPointer]
    "andi r16, 0b11111 \n"
    "cpi r16, 16 \n"
    "brlo TRI_OUTPUT \n"
  // pointer is 16-31 -> 15,14,13,...,1,0
    "neg r16 \n"
    "subi r16, -31 \n"
  "TRI_OUTPUT:"
    "add r2, r16 \n"


  "OUTPUT:"
    "out 0x28, r2 \n" // OCR0B

    "andi r20, 0b11111110 \n"
    "rjmp STARTLOOP \n"
    :::
  );
}

ISR(TIMER2_COMPA_vect, ISR_NAKED) {
  asm volatile(
    "ori r20, 6 \n"
    "reti \n"
  );
}

ISR(TIMER1_COMPA_vect, ISR_NAKED) {
  asm volatile(
    "ori r20, 5 \n"
    "reti \n"
  );
}

void loop() {
}

