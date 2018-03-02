#include <avr/pgmspace.h>

constexpr byte dacResolution = 8;
constexpr byte channel = 4;
constexpr byte triSize = 32;
constexpr byte triRange = 15; // triのデータに書かれている最大の音量
constexpr byte sqrSize = 8;
constexpr int range = (1 << dacResolution) / channel;
constexpr int rangeMax = range - 1;

byte tri[triSize]; // 三角波データ
byte saw[triSize]; // 三角波データ
char sqr[4][triSize];

//
PROGMEM const char dataSq1[] = {1,2,3};

// enable設定
volatile byte enableFlag = 0b00010001; // 000 noiseShortFrag sq1 sq2 tri noi

// sq1 設定
volatile bool sq1Enable = false;
volatile int sq1Freq = 76; // 0-2047 << 5
volatile bool sq1Env = false; // Envelope
volatile byte sq1Duty = 2; // 0-3
volatile byte sq1FC = 0; // FreqChange 周波数変更量
volatile bool sq1FCDirection = false; // 周波数変更方向 true->上がっていく
volatile byte sq1FCCount = 0; // 周波数変更カウント数
volatile bool sq1Sweep = false; // スイープ有効フラグ
volatile byte sq1Vol = 20; // 0-63 音量 0-15 * rangeMax / 15
// sq1 カウンタ
volatile byte sq1Pointer = sqrSize; // 波形のどこを再生しているか
volatile int sq1Counter = 0; // 分周器

// sq2 設定
volatile bool sq2Enable = false;
volatile int sq2Freq = 40;//85; // 0-2047 << 5
volatile bool sq2Env = false; // Envelope
volatile byte sq2Duty = 2; // 0-3
volatile byte sq2FC = 0; // FreqChange 周波数変更量
volatile bool sq2FCDirection = false; // 周波数変更方向 true->上がっていく
volatile byte sq2FCCount = 0; // 周波数変更カウント数
volatile bool sq2Sweep = false; // スイープ有効フラグ
volatile byte sq2Vol = 20; // 0-63 音量 0-15 * rangeMax / 15
// sq1 カウンタ
volatile byte sq2Pointer = sqrSize; // 波形のどこを再生しているか
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
byte triPointer = triSize; // 波形のどこを再生しているか
volatile int triCounter = 0; // 分周器


// noise
volatile byte noiseEnable = 1; // 0b00: disable, 0b11: enable  (Short), 0b01: enable (Long)
volatile bool noiseShortFreq = false;
volatile unsigned int noiseReg = 0x8000;
volatile int noiseCountMax = 0x20;
volatile byte noiseVol = 32; // 音量 0-15 * rangeMax / 15
// noise カウンタ
volatile int noiseCounter = 0;

void setup() {
  for (int i = 0; i < triSize; i++) {
    tri[i] = range * 16.0 / 15.0 * (i < triSize / 2 ? i : triSize - i - 1) / (triSize / 2);
    saw[i] = range * i / triSize;
  }
  for (int i = 0; i < sqrSize; i++) {
    sqr[0][i] = i < sqrSize / 8 ? 1 : 0;
    sqr[1][i] = i < sqrSize / 4 ? 1 : 0;
    sqr[2][i] = i < sqrSize / 2 ? 1 : 0;
  }
  /*
  Serial.begin(9600);//
  
  for (int i = 0; i < triSize; i++) {
    Serial.println((int)tri[i]);
  }//*/

  cli();      // 割り込み禁止
  
  /* TIMER0: 出力の強さを制御するための PWM
   * できるだけ高速で回す
   * Top 255 なので 62500 Hz の PWM
   * OCR0A の値を変えることで制御できる．
   */
  // pin6有効，pin9なし，高速PWM，コンペアマッチでLOW，TOPは0xFF，
  TCCR0A = 0b10000011;
  TCCR0B &= ~_BV(WGM02);
  // 分周 1/1
  TCCR0B = (TCCR0B & 0b11111000) | 0b00000001;
  // compare register
  OCR0A = 0;
  // 割り込み無し
  TIMSK0 = 0;


  /* TIMER2: 波形を変更するタイミング
   * 本当は OCR2A = 8.9 にしたい．
   * 早すぎるとTIMER1の割り込みに失敗するので 71
   */
  // CTC
  TCCR2A = 0b00000010;
  TCCR2B &= ~_BV(WGM02);
  // 分周 1/1
  TCCR2B = (TCCR2B & 0b11111000) | 0b00000001;
  // compare register
  OCR2A = 100;
  // interrupt when TCNT1 == OCR1A
  TIMSK2 = _BV(OCIE2A);

  // TIMER1: 音の出力の設定を変更する
  // CTC (ICR1)
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

  // 1/8
  TCCR1B = (TCCR1B & ~_BV(CS11)) |_BV(CS12)|_BV(CS10);
 // TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS10))) |_BV(CS11);
  
  // compare register
  // 16 MHz / 割り込み周波数 (Hz)
  OCR1A = 8332;//8332;
  
  // 割り込み
  TIMSK1 |= _BV(OCIE1A);
    
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(6, OUTPUT);
  sei();    // 割り込み許可
  
}

volatile bool waveChange = false;
volatile bool nextFrame = false;

byte output = 0;
volatile byte currentNoise = 0;
volatile byte foo = 0;

void loop() {
  if (nextFrame) {
    nextFrame = false;
    PORTB = foo = !foo ? 0b100000 : 0;
  }
  
  if (waveChange) {
    waveChange = false;
  }
}

ISR(TIMER2_COMPA_vect){
    register uint8_t output asm("r2");
    register uint8_t enable asm("r3");

    asm volatile(
// if noise is enabled {    
        "lds r3, enableFlag \n"
      // noiseがenableでなければ分岐
        "sbrs r3, 0 \n"
        "rjmp NOISEDISABLE \n"

// // if (++noiseCounter == noiseCountMax)
      // [r24, r25, r6, r7]
        "lds r24, noiseCounter \n"
        "lds r25, noiseCounter+1 \n"
        "adiw r24, 1 \n"
        "sts noiseCounter, r24 \n"
        "sts noiseCounter+1, r25 \n"
        "lds r6, noiseCountMax \n"
        "lds r7, noiseCountMax+1 \n"
        "cp r24, r6 \n"
        "cpc r25, r7 \n"
        "brne NOISE_NOT_UPDATE \n"

          // [r18, r19]
          // r19:r18 が noiseReg
            "lds r18, noiseReg \n"
            "lds r19, noiseReg+0x1 \n"
          // noiseReg >>= 1
            "lsr r19 \n"
            "ror r18 \n"
          // noiseShortFreq ?
            "sbrs r3, 4 \n"
            "rjmp LONGFREQ \n"
          // noiseReg >> 6  下位 1 bit があれば良い -> r17に 1 bitだけ
          // [r17, r18, r19]
            "clr r17 \n"
            "sbrc r18, 6 \n" // 6bit目が0ならスキップ，1ならスキップせず1
            "ldi r17, 1 \n"
            "rjmp DONE \n"
          // noiseReg >> 1
            "LONGFREQ:"
            "mov r17, r18 \n"
            "lsr r17 \n"
          // r17 = shift済みreg ^ reg
            "DONE: eor r17, r18 \n"
          // & 1) << 15
          // [r16, r17, r18, r19]
            "clr r16 \n"
            "lsr r17 \n"
            "ror r16 \n"
          // 最上位1bitだけ 0 or 1，残りは << 15 の為 0 -> 結果は r17 
          // noiseReg r19 |= r16
            "or r19, r16 \n"
            "sts noiseReg, r18 \n"
            "sts noiseReg+1, r19 \n"
            "andi r18, 0x1\n"
          // calcNoise() * noiseVol
            "lds r2, noiseVol \n"
            "mul r18, r2 \n"
            "mov r2, r0 \n"
            
            "sts noiseCounter, r1 \n" // noiseCounter = 0
            "sts currentNoise, r2 \n" // currentNoise = output

            "rjmp SQ1 \n"

// if noise counter is not max
        "NOISE_NOT_UPDATE: clr r2 \n"
        "lds r2, currentNoise \n"
        "rjmp SQ1 \n"
        
// } else {
        "NOISEDISABLE: clr r2 \n"
// }
// if sq1 is enabled
        "SQ1: "
        "sbrs r3, 3 \n"
        "rjmp SQ2 \n"
        :"=&r"(output)::
    );
      if (++sq1Counter == sq1Freq) {
        sq1Counter = 0;
        sq1Pointer == sqrSize ? sq1Pointer = 0 : ++sq1Pointer;
      }
      output += sqr[sq1Duty][sq1Pointer] * sq1Vol;
    
    asm volatile(
        "SQ2: "
        );
    if (sq2Enable) {
      if (++sq2Counter == sq2Freq) {
        sq2Counter = 0;
        sq2Pointer == sqrSize ? sq2Pointer = 0 : ++sq2Pointer;
      }
      output += sqr[sq2Duty][sq2Pointer] * sq2Vol;
    }

    if (triEnable) {
      if (++triCounter == triFreq) {
        triCounter = 0;
        triPointer == triSize ? triPointer = 0 : ++triPointer;
      }
      output += tri[triPointer];
    }
    
    OCR0A = output;
}

ISR(TIMER1_COMPA_vect){
  nextFrame = true;
  //if (enableFlag & 0b10000) enableFlag &= 0b11101111;
  //else enableFlag |= 0b10000;
  //if (enableFlag & 0b10000) enableFlag = 0b00000001;
  //else enableFlag = 0b10001;
  //digitalWrite(13, foo = !foo ? HIGH : LOW);
}
