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
byte sq0[triSize]; // 三角波データ
byte sq1[triSize]; // 三角波データ
byte sq2[triSize]; // 三角波データ
byte sq3[triSize]; // 三角波データ
volatile byte triPointer = 0;

//
PROGMEM const char dataSq1[] = {1,2,3};

// sq1 設定
volatile bool sq1Enable = true;
volatile int sq1Freq = 125; // 0-2047 << 5
volatile bool sq1Env = false; // Envelope
volatile byte sq1Duty = 2; // 0-3
volatile byte sq1FC = 0; // FreqChange 周波数変更量
volatile bool sq1FCDirection = false; // 周波数変更方向 true->上がっていく
volatile byte sq1FCCount = 0; // 周波数変更カウント数
volatile bool sq1Sweep = false; // スイープ有効フラグ
// sq1 カウンタ
volatile byte sq1Pointer = sqrSize; // 波形のどこを再生しているか
volatile int sq1Counter = 0; // 分周器

// noise
volatile bool noiseEnable = false;
volatile bool noiseShortFreq = false;
volatile unsigned int noiseReg = 0x8000;
volatile byte noiseCounter = 0;
volatile byte noiseCountMax = 0x2;

void setup() {
  for (int i = 0; i < triSize; i++) {
    tri[i] = range * (i < triSize / 2 ? i : triSize - i - 1) / (triSize / 2);
    saw[i] = range * i / triSize;
  }
  for (int i = 0; i < sqrSize; i++) {
    sqr[0][i] = i == 0 ? 1 : i == sqrSize / 8 ? -1 : 0;
    sqr[1][i] = i == 0 ? 1 : i == sqrSize / 4 ? -1 : 0;
    sqr[2][i] = i == 0 ? 1 : i == sqrSize / 2 ? -1 : 0;
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

///*
  // TIMER2: 出力の強さを変更するための PWM
  // CTC
  TCCR2A = 0b00000010;
  TCCR2B &= ~_BV(WGM02);
  // 分周 1/1
  TCCR2B = (TCCR2B & 0b11111000) | 0b00000001;
  // compare register
  OCR2A = 71;
  // interrupt when TCNT1 == OCR1A
  TIMSK2 = _BV(OCIE2A);
//*/

///*
  // CTC (ICR1)
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

  // 1/8
  TCCR1B = (TCCR1B & ~_BV(CS11)) |_BV(CS12)|_BV(CS10);
 // TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS10))) |_BV(CS11);
  
  // compare register
  // 16 MHz / 割り込み周波数 (Hz)
  OCR1A = 800;//8332;
  
  // 割り込み
  TIMSK1 |= _BV(OCIE1A);
  //*/
  
  
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(6, OUTPUT);
  sei();    // 割り込み許可
  
}

void loop() {
}

#define calcNoise() ((noiseReg = (noiseReg >> 1) | ((((noiseReg >> 1) ^ (noiseReg >> (noiseShortFreq ? 7 : 2))) & 1) << 15)) & 1)
/*
byte calcNoise() { // 0 1 で返す
  noiseReg >>= 1;
  noiseReg |= ((noiseReg ^ (noiseReg >> (noiseShortFreq ? 6 : 1))) & 1) << 15;
  return noiseReg & 1;
}*/

volatile byte c = 0;
volatile int i = 0;
volatile byte output = 0;
volatile bool lastNoise = false;

ISR(TIMER2_COMPA_vect){
  //output = 0;
  ///*
  output = 0;
  if (noiseEnable && ++noiseCounter == noiseCountMax) {
    noiseCounter = 0;
    output = calcNoise() * rangeMax; //tri[triPointer == triSize ? triPointer = 0 : ++triPointer];
  }//*/
  ///*
  if (sq1Enable && ++sq1Counter == sq1Freq) {
    sq1Counter = 0;
    
    OCR0A += sqr[sq1Duty][sq1Pointer == sqrSize ? sq1Pointer = 0 : ++sq1Pointer] * rangeMax;
    //output = ((sq1Pointer == 8 ? sq1Pointer = 0 : ++sq1Pointer) >= (sq1Duty << 1)) * rangeMax;
    //OCR0A += (sq1Pointer == 0 ? 1 : ((sq1Pointer == 8 ? sq1Pointer = 0 : ++sq1Pointer) << 1 == sq1Duty) * -1) * rangeMax;
    //sq1Pointer == 8 ? sq1Pointer = 0 : ++sq1Pointer;

    //Serial.println(OCR0A);
    
  }//*/
//  OCR0A = output;
}
volatile byte foo = 0;
ISR(TIMER1_COMPA_vect){
  
  //digitalWrite(13, foo = !foo ? HIGH : LOW);
  PORTB = foo = !foo ? 0b100000 : 0;
  //PORTB |= _BV(5);
  /*
  if (foo < 2) {
    ++foo;
  } else {
    //foo = 0;
    sq1Enable = true;
    
  }*/
}
