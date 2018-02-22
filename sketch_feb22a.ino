const byte dacResolution = 8;
const byte channel = 4;
const byte triSize = 32;
#define DIVIDE 64

byte tri[triSize] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0}; // 三角波データ

volatile byte triPointer = 0;

byte freqSq1 = 126;

unsigned long sq1 = 0; // sq1 = 0b00000_00000000000_0_000_0_000_00_0_0_0000;
word counterSq1Reset = 0; // counterSq1 のリセット値
word counterSq1 = 0; // 周波数制御の為の分周 max 2047
byte cycleSq1 = 0; // 0-16 0_0000


void setup() {

  ///*
  // put your setup code here, to run once:
  // setup timer1
  cli();      // 割り込み禁止
  // 高速PWM，コンペアマッチでLOW，TOPは0xFF，pin6有効，pin9なし
  TCCR0A = 0b10100011;
  TCCR0B &= ~_BV(WGM02);
  // 分周 1/1
  TCCR0B = (TCCR0B & 0b11111000) | 0b00000001;
  // compare register
  OCR0A = 0x7F;
  // 割り込み無し
  TIMSK0 = 0;
  
  // set CTC mode(clear timer on compare match)
  TCCR2A = 0b00000010;
  TCCR2B &= ~_BV(WGM02);
  /*
  TCCR2A |= _BV(WGM11);
  TCCR2A &= ~_BV(WGM10);
  TCCR2B &= ~_BV(WGM22);*/
  
  // 1/1024
  TCCR2B = (TCCR2B & 0b11111000) | 0b00000101;
  
  // compare register
  OCR2A = 100;
  
  // interrupt when TCNT2 == OCR2A
  TIMSK2 = _BV(OCIE2A);

  sei();
  //*/
  /*
  cli();      // 割り込み禁止
  // CTC mode
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));
  
  // prescaler
  TCCR1B = 0b00001101;
  
  // compare register
  OCR1A = 20;  // 16e6 / 割り込み周波数
  
  // interrupt when TCNT1 == OCR1A
  TIMSK1 |= _BV(OCIE1A);
  
  sei();    // 割り込み許可
  //*/
  
  //周波数から分周率の計算
  sq1 = 0b00000000000010000000000000000000;
  counterSq1Reset = (sq1 && 0b111111111110000000000000000) >> 16;
  pinMode(13, OUTPUT);
    pinMode(12, OUTPUT);
    pinMode(6, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
}
volatile byte c = 0;
volatile int i = 0;
ISR(TIMER2_COMPA_vect){
///*
  if (c == 1) {
    c = 0;
  } else {
    c = 1;
  }
  if (c == 1) {
    OCR0A = 0x7f;
      PORTB |= _BV(4);
  } else {
    OCR0A = 0;
      PORTB &= ~_BV(4);
  }//*/
 /*
 if (c == 15) {
   i++;
   if (i % 1024 == 0)  triPointer == 31 ? triPointer = 0 : ++triPointer;
    c = 1;
  } else {
    ++c;
  }
  
  if (c < tri[triPointer]) {
     // PORTB  |= _BV(4);
  } else {
      PORTB  &= ~_BV(4);
  }//*/
}
