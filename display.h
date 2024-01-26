#define DISP_1 4
#define DISP_2 5
#define DISP_3 6
#define DISP_4 7
#define DISP_5 8
#define DISP_6 9
#define DISP_7 10
#define DISP_8 11

/*
 *    =5=
 * |2|   |6|
 *    =1=
 * |3|   |7|
 *    =4=  [8]  
 */

// 1 = low is on, 0 = high is on
#if ANODE_TYPE
  #define HIGH_STATE LOW
  #define LOW_STATE HIGH
#else
  #define HIGH_STATE HIGH
  #define LOW_STATE LOW
#endif

const int d0[] = {DISP_2, DISP_3, DISP_4, DISP_5, DISP_6, DISP_7};
const int d1[] = {DISP_6, DISP_7};
const int d2[] = {DISP_1, DISP_3, DISP_4, DISP_5, DISP_6};
const int d3[] = {DISP_1, DISP_4, DISP_5, DISP_6, DISP_7};
const int d4[] = {DISP_1, DISP_2, DISP_6, DISP_7};
const int d5[] = {DISP_1, DISP_2, DISP_4, DISP_5, DISP_7};
const int d6[] = {DISP_1, DISP_2, DISP_3, DISP_4, DISP_5, DISP_7};
const int d7[] = {DISP_5, DISP_6, DISP_7};
const int d8[] = {DISP_1, DISP_2, DISP_3, DISP_4, DISP_5, DISP_6, DISP_7};
const int d9[] = {DISP_1, DISP_2, DISP_5, DISP_6, DISP_7};
const int dH[] = {DISP_1, DISP_2, DISP_3, DISP_6, DISP_7};
const int dC[] = {DISP_2, DISP_3, DISP_4, DISP_5};
const int dL[] = {DISP_2, DISP_3, DISP_4};

// Common anode = LOW = ON, HIGH = OFF
void clearDigit() { 
  for (int i = 0; i < sizeof(d8)/sizeof(int); i++) {
    digitalWrite(d8[i], LOW_STATE);
  }
}

void writeDigit(int arr[], int size) {
  for (int i = 0; i < size; i++) {
    digitalWrite(arr[i], HIGH_STATE);
  }
}

void digit0() {
  clearDigit();
  writeDigit(d0, sizeof(d0)/sizeof(int));
}
void digit1() {
  clearDigit();
  writeDigit(d1, sizeof(d1)/sizeof(int));
}
void digit2() {
  clearDigit();
  writeDigit(d2, sizeof(d2)/sizeof(int));
}
void digit3() {
  clearDigit();
  writeDigit(d3, sizeof(d3)/sizeof(int));
}
void digit4() {
  clearDigit();
  writeDigit(d4, sizeof(d4)/sizeof(int));
}
void digit5() {
  clearDigit();
  writeDigit(d5, sizeof(d5)/sizeof(int));
}
void digit6() {
  clearDigit();
  writeDigit(d6, sizeof(d6)/sizeof(int));
}
void digit7() {
  clearDigit();
  writeDigit(d7, sizeof(d7)/sizeof(int));
}
void digit8() {
  clearDigit();
  writeDigit(d8, sizeof(d8)/sizeof(int));
}
void digit9() {
  clearDigit();
  writeDigit(d9, sizeof(d9)/sizeof(int));
}

// Letters
void digitH() {
  clearDigit();
  writeDigit(dH, sizeof(dH)/sizeof(int));
}
void digitC() {
  clearDigit();
  writeDigit(dC, sizeof(dC)/sizeof(int));
}

// End-game digits; basically stop execution here
void digitL() {
  clearDigit();
  writeDigit(dL, sizeof(dL)/sizeof(int));
  while (true) {
  }
}
// 5,6,7,4,3,2
void winAnim() {
  while(true){
  	clearDigit();
  	digitalWrite(DISP_5, HIGH_STATE);
  	delay(100);
  	clearDigit();
  	digitalWrite(DISP_6, HIGH_STATE);
  	delay(100);
  	clearDigit();
  	digitalWrite(DISP_7, HIGH_STATE);
  	delay(100);
  	clearDigit();
  	digitalWrite(DISP_4, HIGH_STATE);
  	delay(100);
  	clearDigit();
  	digitalWrite(DISP_3, HIGH_STATE);
  	delay(100);
  	clearDigit();
  	digitalWrite(DISP_2, HIGH_STATE);
  	delay(100);
  }
}
