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

// sq1 設定
bool sq1Enable = false;
int sq1Freq = 76; // 0-2047 << 5
bool sq1Env = false; // Envelope
byte sq1Duty = 2; // 0-3
byte sq1FC = 0; // FreqChange 周波数変更量
bool sq1FCDirection = false; // 周波数変更方向 true->上がっていく
byte sq1FCCount = 0; // 周波数変更カウント数
bool sq1Sweep = false; // スイープ有効フラグ
byte sq1Vol = 00; // 0-63 音量 0-15 * rangeMax / 15
// sq1 カウンタ
byte sq1Pointer = sqrSize; // 波形のどこを再生しているか
int sq1Counter = 0; // 分周器

// sq2 設定
bool sq2Enable = false;
int sq2Freq = 40;//85; // 0-2047 << 5
bool sq2Env = false; // Envelope
byte sq2Duty = 2; // 0-3
byte sq2FC = 0; // FreqChange 周波数変更量
bool sq2FCDirection = false; // 周波数変更方向 true->上がっていく
byte sq2FCCount = 0; // 周波数変更カウント数
bool sq2Sweep = false; // スイープ有効フラグ
byte sq2Vol = 20; // 0-63 音量 0-15 * rangeMax / 15
// sq1 カウンタ
byte sq2Pointer = sqrSize; // 波形のどこを再生しているか
int sq2Counter = 0; // 分周器


// tri 設定
bool triEnable = false;
int triFreq = 10;//85; // 0-2047 << 5
bool triEnv = false; // Envelope
byte triFC = 0; // FreqChange 周波数変更量
bool FCDirection = false; // 周波数変更方向 true->上がっていく
byte triFCCount = 0; // 周波数変更カウント数
bool triSweep = false; // スイープ有効フラグ
// tri カウンタ
byte triPointer = triSize; // 波形のどこを再生しているか
int triCounter = 0; // 分周器


// noise
bool noiseEnable = true;
volatile bool noiseShortFreq = false;
volatile unsigned int noiseReg = 0x8000;
byte noiseCountMax = 0x20;
volatile byte noiseVol = 63; // 音量 0-15 * rangeMax / 15
// noise カウンタ
byte noiseCounter = 0;

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
  OCR2A = 60;
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


byte calcNoise() { // 0 1 で返す
  asm volatile(
// r25:r24 が noiseReg
    "lds r24, noiseReg \n"
    "lds r25, noiseReg+0x1 \n"
// noiseReg >>= 1
    "lsr r25 \n"
    "ror r24 \n"
// r5:r4 も noiseReg
    "movw r4, r24 \n"
// noiseShortFreq ?
    "lds r5, noiseShortFreq \n"
    "and r5, r4 \n"
    "breq LONGFREQ \n"
// noiseReg >> 6 r6:r4 下位一ビットがあれば良い
    "clr r6 \n"
    "lsl r4 \n"
    "rol r5 \n"
    "rol r6 \n"
    "lsl r4 \n"
    "rol r5 \n"
    "rol r6 \n"
    "mov r5, r6 \n"
    "rjmp DONE \n"
// noiseReg >> 1
    "LONGFREQ: lsr r5 \n"
    "ror r4 \n"
// r5:r4 = shift済みreg ^ reg
    "DONE: eor r4, r24 \n"
    "eor r5, r25 \n"
// & 1) << 15
    "clr r5 \n"
    "lsr r4 \n"
    "ror r5 \n"
// 最上位1bitだけ 0 or 1，残りは << 15 の為 0 -> 結果は r5 
// noiseReg r3 |= r5
    "or r25, r5 \n"
    "sts (noiseReg), r24 \n"
    "sts (noiseReg)+0x1, r25 \n"
    "andi r24, 0x01"
    :
    :
    :
  );
}

volatile bool waveChange = false;
volatile bool nextFrame = false;

byte output = 0;
byte currentNoise = 0;
volatile byte foo = 0;

void loop() {
  if (nextFrame) {
    nextFrame = false;
    PORTB = foo = !foo ? 0b100000 : 0;
  }
  
  if (waveChange) {
    waveChange = false;
    register uint8_t output asm("r2");

    if (noiseEnable) {
      if (++noiseCounter == noiseCountMax) {
        asm volatile(
          // r19:r18 が noiseReg
            "lds r18, noiseReg \n"
            "lds r19, noiseReg+0x1 \n"
          // noiseReg >>= 1
            "lsr r19 \n"
            "ror r18 \n"
          // r5:r4 も noiseReg
            "movw r4, r18 \n"
          // noiseShortFreq ?
            "lds r0, noiseShortFreq \n"
            "and r0, r0 \n"
            "breq LONGFREQ \n"
          // noiseReg >> 6 r6:r4 下位一ビットがあれば良い
            "clr r6 \n"
            "lsl r4 \n"
            "rol r5 \n"
            "rol r6 \n"
            "lsl r4 \n"
            "rol r5 \n"
            "rol r6 \n"
            "mov r5, r6 \n"
            "rjmp DONE \n"
          // noiseReg >> 1
            "LONGFREQ: lsr r5 \n"
            "ror r4 \n"
          // r5:r4 = shift済みreg ^ reg
            "DONE: eor r4, r18 \n"
            "eor r5, r19 \n"
          // & 1) << 15
            "clr r5 \n"
            "lsr r4 \n"
            "ror r5 \n"
          // 最上位1bitだけ 0 or 1，残りは << 15 の為 0 -> 結果は r5 
          // noiseReg r19 |= r5
            "or r19, r5 \n"
            "sts noiseReg, r18 \n"
            "sts noiseReg+1, r19 \n"
            "andi r18, 0x1\n"
          // calcNoise() * noiseVol
            "lds r2, noiseVol \n"
            "mul r18, r2 \n"
            "mov r2, r0"
            :"=r"(output)::
        );
        noiseCounter = 0;
        currentNoise = output;
      } else {
        output = currentNoise;
      }
    } else {
      output = 0;
    }
    
    if (sq1Enable) {
      if (++sq1Counter == sq1Freq) {
        sq1Counter = 0;
        sq1Pointer == sqrSize ? sq1Pointer = 0 : ++sq1Pointer;
      }
      output += sqr[sq1Duty][sq1Pointer] * sq1Vol;
    }
    
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
}

ISR(TIMER2_COMPA_vect){
  waveChange = true;
}

ISR(TIMER1_COMPA_vect){
  nextFrame = true;
  //digitalWrite(13, foo = !foo ? HIGH : LOW);
}
