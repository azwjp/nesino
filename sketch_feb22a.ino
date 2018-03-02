constexpr byte dacResolution = 8;
constexpr byte channel = 1;
constexpr byte triSize = 32;
#define DIVIDE 64
constexpr int range = (1 << dacResolution) / channel;
constexpr int rangeMax = range - 1;
byte tri[triSize]; // 三角波データ
byte saw[triSize]; // 三角波データ
byte sq0[triSize]; // 三角波データ
byte sq1[triSize]; // 三角波データ
byte sq2[triSize]; // 三角波データ
byte sq3[triSize]; // 三角波データ
volatile byte triPointer = 0;

byte freqSq1 = 126;


word counterSq1Reset = 0; // counterSq1 のリセット値
word counterSq1 = 0; // 周波数制御の為の分周 max 2047
byte cycleSq1 = 0; // 0-16 0_0000

// noise
volatile bool shortFreq = false;
volatile unsigned int reg = 0x8000;

void setup() {
  for (int i = 0; i < triSize; i++) {
    tri[i] = range * (i < triSize / 2 ? i : triSize - i - 1) / (triSize / 2);
    saw[i] = range * i / triSize;
    sq0[i] = i < triSize / 8 ? rangeMax : 0;
    sq1[i] = i < triSize / 4 ? rangeMax : 0;
    sq2[i] = i < triSize / 2 ? rangeMax : 0;
  }
  /*
  Serial.begin(9600);
  for (int i = 0; i < 128; i++) {
    Serial.print(calcNoise());
  }//*/

  cli();      // 割り込み禁止
  
  ///*
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

  // TIMER2: 出力の強さを変更するための PWM
  // CTC
  TCCR2A = 0b00000010;
  TCCR2B &= ~_BV(WGM02);

  // 分周 1/1024
  TCCR2B = (TCCR2B & 0b11111000) | 0b00000010;
  // compare register
  OCR2A = 128;
  // interrupt when TCNT1 == OCR1A
  TIMSK2 = _BV(OCIE2A);

  // CTC (ICR1)
  TCCR1B = 0b00011010;
  TCCR1A = 0;
  
  // compare register
  // 16 MHz / 割り込み周波数 (Hz)
  ICR1 = 8332;
  
  // 捕獲割り込み
  TIMSK1 = _BV(ICIE1);
  
  sei();    // 割り込み許可
  //*/
  
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(6, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
}

byte calcNoise() { // 0 1 で返す
  reg >>= 1;
  reg |= ((reg ^ (reg >> (shortFreq ? 6 : 1))) & 1) << 15;
  return reg & 1;
}

volatile byte c = 0;
volatile int i = 0;

ISR(TIMER2_COMPA_vect){
/*
  if (c == 1) {
    c = 0;
  } else {
    c = 1;
  }
  if (c == 1) {
    OCR0A = 0xff;
      PORTB |= _BV(4);
  } else {
    OCR0A = 0;
      PORTB &= ~_BV(4);
  }//*/
 ///*
  ;
  OCR0A = calcNoise() * rangeMax; //tri[triPointer == triSize ? triPointer = 0 : ++triPointer];
  //*/
}

ISR(TIMER1_CAPT_vect) 
{
}
