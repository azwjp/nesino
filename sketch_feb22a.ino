#include <avr/pgmspace.h>

constexpr byte dacResolution = 8;
constexpr byte channel = 4;
constexpr byte triSize = 32;
constexpr byte sqrSize = 8;
constexpr int range = (1 << dacResolution) / channel;
constexpr int rangeMax = range - 1;

byte tri[triSize]; // 三角波データ
byte saw[triSize]; // 三角波データ
char sqr[4][triSize];
volatile byte triPointer = 0;

//
PROGMEM const char dataSq1[] = {1,2,3};

// sq1 設定
bool sq1Enable = true;
int sq1Freq = 107; // 0-2047 << 5
bool sq1Env = false; // Envelope
byte sq1Duty = 2; // 0-3
byte sq1FC = 0; // FreqChange 周波数変更量
bool sq1FCDirection = false; // 周波数変更方向 true->上がっていく
byte sq1FCCount = 0; // 周波数変更カウント数
bool sq1Sweep = false; // スイープ有効フラグ
byte sq1Vol = 63; // 音量 0-15 * rangeMax / 15
// sq1 カウンタ
byte sq1Pointer = sqrSize; // 波形のどこを再生しているか
int sq1Counter = 0; // 分周器

// sq2 設定
bool sq2Enable = true;
int sq2Freq = 85; // 0-2047 << 5
bool sq2Env = false; // Envelope
byte sq2Duty = 2; // 0-3
byte sq2FC = 0; // FreqChange 周波数変更量
bool sq2FCDirection = false; // 周波数変更方向 true->上がっていく
byte sq2FCCount = 0; // 周波数変更カウント数
bool sq2Sweep = false; // スイープ有効フラグ
byte sq2Vol = 63; // 音量 0-15 * rangeMax / 15
// sq1 カウンタ
byte sq2Pointer = sqrSize; // 波形のどこを再生しているか
int sq2Counter = 0; // 分周器

// noise
volatile bool noiseEnable = false;
bool noiseShortFreq = false;
unsigned int noiseReg = 0x8000;
byte noiseCountMax = 0x20;
byte noiseVol = 10; // 音量 0-15 * rangeMax / 15
// noise カウンタ
byte noiseCounter = 0;

void setup() {
  for (int i = 0; i < triSize; i++) {
    tri[i] = range * (i < triSize / 2 ? i : triSize - i - 1) / (triSize / 2);
    saw[i] = range * i / triSize;
  }
  for (int i = 0; i < sqrSize; i++) {
    sqr[0][i] = i < sqrSize / 8 ? 1 : 0;
    sqr[1][i] = i < sqrSize / 4 ? 1 : 0;
    sqr[2][i] = i < sqrSize / 2 ? 1 : 0;
  }
  /*
  Serial.begin(9600);
    Serial.print(rangeMax);
  
  for (int i = 0; i < 8; i++) {
    Serial.print((int)sqr[1][i]);
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

  // TIMER2: 波形を変更するタイミング
  // CTC
  TCCR2A = 0b00000010;
  TCCR2B &= ~_BV(WGM02);
  // 分周 1/1
  TCCR2B = (TCCR2B & 0b11111000) | 0b00000001;
  // compare register
  OCR2A = 71;
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

#define calcNoise() ((noiseReg = (noiseReg >> 1) | ((((noiseReg >> 1) ^ (noiseReg >> (noiseShortFreq ? 7 : 2))) & 1) << 15)) & 1)
/*
byte calcNoise() { // 0 1 で返す
  noiseReg >>= 1;
  noiseReg |= ((noiseReg ^ (noiseReg >> (noiseShortFreq ? 6 : 1))) & 1) << 15;
  return noiseReg & 1;
}*/

volatile bool waveChange = false;

byte output = 0;
byte currentNoise = 0;

void loop() {
  if (waveChange) {
    waveChange = false;

    if (noiseEnable) {
      if (++noiseCounter == noiseCountMax) {
        noiseCounter = 0;
        currentNoise = calcNoise() * noiseVol;
      }
      output = currentNoise;
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
    
    OCR0A = output;
  }
}

ISR(TIMER2_COMPA_vect){
  waveChange = true;
}

volatile byte foo = 0;
ISR(TIMER1_COMPA_vect){
  //digitalWrite(13, foo = !foo ? HIGH : LOW);
  PORTB = foo = !foo ? 0b100000 : 0;
}
