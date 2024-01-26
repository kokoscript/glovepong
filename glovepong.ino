#include <Adafruit_DRV2605.h>
#include <SoftwareSerial.h>
#define BLE_HOST 1 // 0 for client, 1 for host
#if BLE_HOST
  #define ANODE_TYPE 1
#else
  #define ANODE_TYPE 0
#endif
#include "display.h"

// Pin assignments
// HM-10
#define BLE_TX 12
#define BLE_RX 13
// Display
// (See display.h)
// Accelerometer
#define ACC_X A0
#define ACC_Y A1
#define ACC_Z A2
// Speaker
#define SPK 3

// Debug settings
// Test with these off BEFORE presentation, as they can affect weird things like hitreg sensitivity. Don't ask me why, I don't know
#define BLE_DBG 0   // use serial monitor to talk to the BLE module
#define ACCEL_DBG 0 // print accelerometer values to serial
#define COMM_DBG 1  // show messages being sent/recv'd

// Bluetooth vars
SoftwareSerial bleSerial(BLE_TX, BLE_RX); // RX, TX

#define HOST_HANDSHAKE_INTERVAL 1000 // In milliseconds, the interval between handshake messages sent by the host before connection
#define BLE_ROBUSTNESS 10 // In "Magic Values" (TM, patent pending). The higher the number, the less chance of messages arriving broken into pieces
bool connectStatus = false;
typedef struct ble_buf {
  char dat[20];
  int lastIndex = 0;
} ble_buf;
ble_buf buffer;

// Game vars
#define BALL_MAX_TIME 4000 // In milliseconds. Ball speed scales accordingly
#if BLE_HOST
  #define HITREG_SENSE 35  // The higher, the less sensitive hitreg is. max is 1023 (for no hitreg)
#else
  #define HITREG_SENSE 80
#endif
#define SERVE_INDIC_TIME 500 // In milliseconds, how fast the serving indication flashes

// This is sent to the other side each time a player makes a hit (or misses)
// Use send_state() and recv_state() to transfer it over BLE.
typedef struct game_state {
  int ball_speed = -1; // Calculated then sent on hit; how long, in ms, does the ball take to get to the other side?
  int miss_state = 0; // If this is true, then the other player just missed the ball; they now serve. Reset to false.
} game_state;
game_state current_state;

uint32_t ball_hit_ref;
bool waiting_for_ball = false;
bool bounce1 = false;
bool bounce2 = false;

// Tells if this glove is able to serve.
// Initial value is controlled based on host/client.
#if BLE_HOST
  boolean am_serving = true;
#else
  boolean am_serving = false;
#endif
int my_score = 0;
int other_score = 0;

// Acceleration vars
// Raw Ranges: X: 405-610, Y: 403-609, Z: 413-619 (probably not needed, we just need the deltas)
int xRaw, yRaw, zRaw;
int xLast, yLast, zLast;

Adafruit_DRV2605 haptic;

// Bluetooth buffer utils

// Write a single char into the buffer, using lastIndex as an offset.
void write_buffer(char in) {
  buffer.dat[buffer.lastIndex] = in;
  buffer.lastIndex == 19 ? buffer.lastIndex = 0 : buffer.lastIndex++;
}
// Clears what's in the buffer and resets lastIndex back to 0.
void clear_buffer() {
  for (int i = 0; i < 20; i++) {
    buffer.dat[i] = 0;
  }
  buffer.lastIndex = 0;
}
void clear_serial() {
  while (bleSerial.available()) {
    bleSerial.read();
  }
}
// Wait for data to be read into the buffer
// Remember: clear the buffer after processing its contents!
void wait_buffer() {
  while (!bleSerial.available());
  while (bleSerial.available()) {
    write_buffer(bleSerial.read());
    delay(BLE_ROBUSTNESS); // Otherwise the message comes in broken...
  }
}
// Get the contents of the buffer, skipping if nothing's there
// Remember: clear the buffer after processing its contents!
void immediate_buffer() {
  while (bleSerial.available()) {
    write_buffer(bleSerial.read()); 
    delay(BLE_ROBUSTNESS); // Otherwise the message comes in broken...
  }
}

void handshake() {
  // Host side: continually poke client & check for response in the buffer
  #if BLE_HOST
    #if COMM_DBG
      Serial.println("Send handshake to client");
    #endif
    bleSerial.write("hey client");
    immediate_buffer();
    #if COMM_DBG
      Serial.print("Handshake check: ");
      Serial.println(buffer.dat);
    #endif
    if (strstr(buffer.dat, "hey host")) {
      #if COMM_DBG
        Serial.println("Got handshake from client");
      #endif
      connectStatus = true;
    }
    clear_buffer();
    delay(HOST_HANDSHAKE_INTERVAL);
  // Client side: stop and wait for a response
  #else
    wait_buffer();
    #if COMM_DBG
      Serial.print("Handshake check: ");
      Serial.println(buffer.dat);
    #endif
    if (strstr(buffer.dat, "hey client")) {
      #if COMM_DBG
        Serial.println("Send handshake to host");
      #endif
      bleSerial.write("hey host");
      connectStatus = true;
    }
    clear_buffer();
  #endif
}

// Gamestate serialization
void send_state() {
  // maybe replace with char[]s since string can be slow
  String toSend = String(current_state.ball_speed) + " " + String(current_state.miss_state);
  #if COMM_DBG
    Serial.println("send: " + toSend);
  #endif
  bleSerial.write(toSend.c_str());
}
void recv_state() {
  wait_buffer();
  char* tab_loc = strtok(buffer.dat, " ");
  int new_ball_speed = atoi(tab_loc);
  tab_loc = strtok(0, " ");
  int new_miss_state = atoi(tab_loc);
  clear_buffer();
  
  #if COMM_DBG
    Serial.print("recv: ");
    Serial.println(buffer.dat);
    Serial.println("-- parse recvd state --");
    Serial.println("ball speed");
    Serial.println(new_ball_speed);
    Serial.println("miss state");
    Serial.println(new_miss_state);
  #endif

  // Check for an invalid state, most likely due to an incomplete message
  // A value of -1 is valid, just tells us the ball isn't in play!
  if ((new_ball_speed <= 999 && new_ball_speed > -1) || new_miss_state > 1) {
    #if COMM_DBG
      Serial.print("invalid state recieved, discarding");
    #endif
    return;
  }

  // Otherwise it's valid, grab the new state
  current_state.ball_speed = new_ball_speed;
  current_state.miss_state = new_miss_state;
}

// warning: i block the cpu! :)
void speaker_sweep(int startTone, int delta, int len, int num) {
  for (int i = 0; i < num; i++) {
    tone(SPK, startTone + (i * delta), len);
    delay(len);
  }
}

int get_accel_change() {
  int xChange = abs(xRaw - xLast);
  int yChange = abs(yRaw - yLast);
  int zChange = abs(zRaw - zLast);
  int largest = xChange;
  if (largest < yChange)
    largest = yChange;
  if (largest < zChange)
    largest = zChange;
  return largest;
}

void display_current_score() {
  switch(my_score) {
    case 0:
      digit0();
      break;
    case 1:
      digit1();
      break;
    case 2:
      digit2();
      break;
    case 3:
      digit3();
      break;
    case 4:
      digit4();
      break;
    case 5:
      digit5();
      break;
    case 6:
      digit6();
      break;
    case 7:
      digit7();
      break;
    case 8:
      digit8();
      break;
    case 9:
      digit9();
      break;
    default:
      digitL(); // should hopefully never get here
      break;
  }
}

void check_hit_ball() {
  delay(15);
  xRaw = analogRead(ACC_X);
  yRaw = analogRead(ACC_Y);
  zRaw = analogRead(ACC_Z);

  if (get_accel_change() > HITREG_SENSE) {
    // Determine ball speed from accel change
    current_state.ball_speed = ((1023 - get_accel_change()) / 1023.0) * BALL_MAX_TIME;
    send_state();
    playHaptic(1);
    // tone, delta, time, num
    speaker_sweep(250, 25, 20, 7);
    waiting_for_ball = false;
    wait_for_other_side();
  }
}

void wait_for_other_side() {
  // wait until we get some data
  recv_state();
  // If the other player missed...
  if (current_state.miss_state) {
    // tone, delta, time, num
    speaker_sweep(500, 100, 250, 3);
    my_score++;
    if (my_score == 10) {
      winAnim();
    } else {
      display_current_score();
      wait_for_other_side();
    }
  } else if(current_state.ball_speed >= 1000) {
    ball_hit_ref = millis();
    waiting_for_ball = true;
  }
}

void do_ball() {
  if (!bounce1 && millis() - ball_hit_ref >= current_state.ball_speed * .25) {
    playHaptic(3);
    // tone, delta, time, num
    speaker_sweep(300, -10, 5, 6);
    bounce1 = true;
  } else if (!bounce2 && millis() - ball_hit_ref >= current_state.ball_speed * .75) {
    playHaptic(2);
    // tone, delta, time, num
    speaker_sweep(320, -10, 10, 8);
    bounce2 = true;
  } else if (millis() - ball_hit_ref >= current_state.ball_speed * 7/8 && millis() - ball_hit_ref <= current_state.ball_speed * 9/8) {
    xLast = analogRead(ACC_X);
    yLast = analogRead(ACC_Y);
    zLast = analogRead(ACC_Z);
    check_hit_ball();
  // if we miss the ball
  } else if (millis() - ball_hit_ref > current_state.ball_speed * 9/8) {
    current_state.ball_speed = -1;
    current_state.miss_state = 1;
    send_state();
    other_score++;
    // tone, delta, time, num
    speaker_sweep(500, -100, 250, 3);
    if (other_score == 10) {
      digitL();
    } else {
      bounce1 = false;
      bounce2 = false;
      waiting_for_ball = false;
      current_state.miss_state = 0;
      am_serving = true;
    }
  }
}

void playHaptic(int num) {
  haptic.setWaveform(0, num);  // load haptic 
  haptic.setWaveform(1, 0);  // end haptic
  haptic.go();
}

void setup() {
  analogReference(EXTERNAL);
  Serial.begin(9600);
  bleSerial.begin(9600);
  pinMode(DISP_1, OUTPUT);
  pinMode(DISP_2, OUTPUT);
  pinMode(DISP_3, OUTPUT);
  pinMode(DISP_4, OUTPUT);
  pinMode(DISP_5, OUTPUT);
  pinMode(DISP_6, OUTPUT);
  pinMode(DISP_7, OUTPUT);
  pinMode(DISP_8, OUTPUT);
  
  haptic.begin();
  haptic.selectLibrary(1);
  haptic.setMode(DRV2605_MODE_INTTRIG);
  

  #if BLE_HOST
    digitH();
  #else
    digitC();
  #endif
  
  // Wait a bit so the user knows if this glove is client or host
  // (the modules connect pretty fast)
  delay(1500);
  clear_buffer();
  
  // Handshake to verify connection
  while(!connectStatus) {
    handshake();
  }

  // When the connection starts, show initial score on both sides
  playHaptic(1);
  // tone, delta, time, num
  speaker_sweep(500, 100, 250, 2);
  digit0();
}

void loop() {
  // Debug bluetooth control
  #if BLE_DBG
  if (bleSerial.available()) {
    Serial.write(bleSerial.read());
  }
  if (Serial.available()) {
    bleSerial.write(Serial.read());
  }
  #endif

  display_current_score();

  if (am_serving) {
    // Set up number flashing stuff
    uint32_t flash_ref = millis();
    boolean flash_state = true;
    
    // Get current accel readings
    xRaw = analogRead(ACC_X);
    yRaw = analogRead(ACC_Y);
    zRaw = analogRead(ACC_Z);
    // Avoid large initial spike
    xLast = xRaw;
    yLast = yRaw;
    zLast = zRaw;
    
    // Wait for the player to serve
    while(get_accel_change() < HITREG_SENSE) {
      // Flash display every half second to indicate player is serving
      if (millis() - flash_ref >= SERVE_INDIC_TIME) {
        if (flash_state) {
          clearDigit();
          flash_state = false;
        } else {
          display_current_score();
          flash_state = true;
        }
        flash_ref = millis();
      }
      
      xLast = xRaw;
      yLast = yRaw;
      zLast = zRaw;
      xRaw = analogRead(ACC_X);
      yRaw = analogRead(ACC_Y);
      zRaw = analogRead(ACC_Z);

      #if ACCEL_DBG
      Serial.print(xRaw);
      Serial.print(" ");
      Serial.print(yRaw);
      Serial.print(" ");
      Serial.print(zRaw);
      Serial.print("\r\n");
      #endif
    }
    // Determine ball speed from accel change
    current_state.ball_speed = ((1023 - get_accel_change()) / 1023.0) * BALL_MAX_TIME;
    send_state();
    display_current_score();
    am_serving = false;
    playHaptic(1);
    // tone, delta, time, num
    speaker_sweep(250, 25, 20, 7);
    wait_for_other_side();
  } else if (!waiting_for_ball) {
    clear_serial();
    wait_for_other_side();
  } else if (waiting_for_ball) {
    do_ball();
  }
}
