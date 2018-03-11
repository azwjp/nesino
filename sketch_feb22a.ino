// enable設定
volatile byte enableFlag = 0b00001110; // 000 noiseShortFrag sq1 sq2 tri noi

// sq1 設定
volatile int sq1Freq = 45; // 0-2047 << 5
volatile bool sq1Env = false; // Envelope
volatile byte sq1Duty = 2; // 0-3
volatile byte sq1FC = 0; // FreqChange 周波数変更量
volatile bool sq1FCDirection = false; // 周波数変更方向 true->上がっていく
volatile byte sq1FCCount = 0; // 周波数変更カウント数
volatile bool sq1Sweep = false; // スイープ有効フラグ
volatile byte sq1Vol = 6; // 音量 0-15
// sq1 カウンタ
volatile byte sq1Pointer = -1; // 波形のどこを再生しているか
volatile int sq1Counter = 0; // 分周器

// sq2 設定
volatile int sq2Freq = 20;//85; // 0-2047 << 5
volatile bool sq2Env = false; // Envelope
volatile byte sq2Duty = 1; // 0-3
volatile byte sq2FC = 0; // FreqChange 周波数変更量
volatile bool sq2FCDirection = false; // 周波数変更方向 true->上がっていく
volatile byte sq2FCCount = 0; // 周波数変更カウント数
volatile bool sq2Sweep = false; // スイープ有効フラグ
volatile byte sq2Vol = 7; // 音量 0-15
// sq1 カウンタ
volatile byte sq2Pointer = -1; // 波形のどこを再生しているか
volatile int sq2Counter = 0; // 分周器


// tri 設定
volatile bool triEnable = false;
volatile int triFreq = 10;//85; // 0-2047 << 5
volatile bool triEnv = false; // Envelope
volatile byte triFC = 0; // FreqChange 周波数変更量
volatile bool FCDirection = false; // 周波数変更方向 true->上がっていく
volatile byte triFCCount = 0; // 周波数変更カウント数
volatile bool triSweep = false; // スイープ有効フラグ
// tri カウンタ
volatile byte triPointer = -1; // 波形のどこを再生しているか
volatile int triCounter = 0; // 分周器

// noise
volatile unsigned int noiseReg = 0x8000;
volatile int noiseCountMax = 0x20;
volatile byte noiseVol = 10; // 音量 0-15 * rangeMax / 15
// noise カウンタ
volatile int noiseCounter = 0;

volatile byte currentNoise = 0;

void setup() {
  cli();

  /* TIMER0: 出力の強さを制御するための PWM
     できるだけ高速で回す
     Top 255 なので 62500 Hz の PWM
     OCR0A の値を変えることで制御できる．
  */
  // pin6有効，pin9なし，高速PWM，コンペアマッチでLOW，TOPは0xFF，
  TCCR0A = 0b10100011;
  TCCR0B |= _BV(WGM02);
  // 分周 1/1
  TCCR0B = (TCCR0B & 0b11111000) | 0b00000001;
  // compare register -> 音を鳴らすときに毎回変わる
  OCR0A = 64;
  OCR0B = 0;
  // 割り込み無し
  TIMSK0 = 0;

  /* TIMER2: 波形を変更するタイミング
     本当は OCR2A = 8.9 にしたい．
     早すぎるとTIMER1の割り込みに失敗するので 71
  */
  // CTC
  TCCR2A = 0b00000010;
  TCCR2B &= ~_BV(WGM02);
  // 分周 1/1
  TCCR2B = (TCCR2B & 0b11111000) | 0b00000001;
  // compare register
  OCR2A = 80;
  // interrupt when TCNT1 == OCR1A
  TIMSK2 = _BV(OCIE2A);

  // TIMER1: 音の出力の設定を変更する
  // CTC (ICR1)
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

  // 1/8
  TCCR1B = (TCCR1B & ~_BV(CS11)) | _BV(CS12) | _BV(CS10);
  // TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS10))) |_BV(CS11);

  // compare register
  // 16 MHz / 割り込み周波数 (Hz)
  OCR1A = 8332;//8332;

  // 割り込み
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
 * r20: interrupt flag
 * 
 * used register
 * r3 :enable flag
 * NOISE
 * r5, r6, r7, r16, r17, r18,r19 r24, r25
 * SQ1, 2
 * r5, r6, r7, r16, r24, r25
 */

  asm volatile(
  "STARTLOOP: "
    "sbrs r20, 0 \n"
    "rjmp STARTLOOP \n"
    
    "clr r2 \n"

// if noise is enabled {
    "lds r3, enableFlag \n"
    // noiseがenableでなければ分岐
    "sbrs r3, 0 \n"
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
    "sbrs r3, 4 \n"
    "rjmp LONGFREQ \n"
    // noiseReg >> 6  下位 1 bit があれば良い -> r17に 1 bitだけ
    // [r17 (noiseReg >> 6) & 1, r19:r18 noiseReg]
    "clr r17 \n"
    "sbrc r18, 6 \n" // 6bit目が0ならスキップ，1ならスキップせず1
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
    // [r16 15shifted, r19:r18 noiseReg]
    // 最上位1bitだけ 0 or 1，残りは << 15 の為 0 -> 結果は r17
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
    "sbrs r3, 3 \n"
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
    "andi r16, 0b111 \n" // pointer の下位3バイト
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
    "sbrs r3, 2 \n"
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
    "andi r16, 0b111 \n" // pointer の下位3バイト
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
    "sbrs r3, 1 \n"
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
    "ori r20, 1 \n"
    "reti \n"
  );
}

ISR(TIMER1_COMPA_vect, ISR_NAKED) {
  asm volatile(
    "ori r20, 2 \n"
    "reti \n"
  );
}

void loop() {
}

