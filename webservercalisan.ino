/*
 * ═══════════════════════════════════════════════════════════
 *              OTTO SUPER ROBOT v5.0
 *      Otonom + Muzikli Dans + Oyunlar + Eglence
 * ═══════════════════════════════════════════════════════════
 *  Yeni Ozellikler:
 *  - Olum hareketi
 *  - Game Over efekti
 *  - Anneni Seviyorsan Alkisla
 *  - Baby Shark dansi
 *  - If You're Happy dansi
 *  - Tum buton renkleri duzeltildi
 * ═══════════════════════════════════════════════════════════
 */

#include <Oscillator.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ═══════════════════ WIFI AYARLARI ═══════════════════
const char* WIFI_SSID = "FiberHGW_TP8830";
const char* WIFI_PASS = "PVP3pvgmEk7A";

// ═══════════════════ PIN TANIMLARI ═══════════════════
#define PIN_YL 0
#define PIN_YR 12
#define PIN_RL 2
#define PIN_RR 13
#define PIN_BUZZER 14
#define PIN_TRIGGER 15
#define PIN_ECHO 3
#define PIN_SDA 4
#define PIN_SCL 5

// ═══════════════════ SABITLER ═══════════════════
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C
#define OBSTACLE_DIST 25

// ═══════════════════ NOTALAR ═══════════════════
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784
#define NOTE_A5 880
#define NOTE_B5 988
#define NOTE_C6 1047

// ═══════════════════ GLOBAL NESNELER ═══════════════════
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
ESP8266WebServer server(80);
Oscillator servo[4];

// ═══════════════════ DURUM DEGISKENLERI ═══════════════════
enum Mode { MODE_IDLE, MODE_AUTO, MODE_DANCE, MODE_PARTY, MODE_SLEEP, MODE_MUSIC_DANCE };
Mode currentMode = MODE_IDLE;

int moveSpeed = 1000;
unsigned long lastBlink = 0;
unsigned long lastObstacle = 0;
unsigned long lastDance = 0;
bool isMoving = false;

struct Eye { int h, w, x, y; };
Eye eyeL, eyeR;
int eyeRadius = 10;

// ═══════════════════════════════════════════════════════════
//                         SES FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

void beep(int frequency, int duration) {
  if (frequency > 0) {
    tone(PIN_BUZZER, frequency, duration);
    delay(duration);
    noTone(PIN_BUZZER);
  } else {
    delay(duration);
  }
}

void sndHappy() { beep(NOTE_C5,100); beep(NOTE_E5,100); beep(NOTE_G5,150); }
void sndSad() { beep(NOTE_G4,200); beep(NOTE_E4,300); }
void sndAngry() { beep(200,100); beep(200,100); beep(200,100); }
void sndSurprise() { for(int i=300; i<1200; i+=100) beep(i,20); }
void sndCuddly() { beep(NOTE_C5,100); beep(NOTE_E5,100); beep(NOTE_G5,100); beep(NOTE_C6,200); }
void sndFart() { for(int i=50; i<300; i+=10) { tone(PIN_BUZZER,i,10); delay(10); } noTone(PIN_BUZZER); }
void sndStart() { beep(NOTE_C5,100); beep(NOTE_G5,150); }
void sndR2D2() { beep(2000,50); beep(1500,50); beep(2500,50); beep(1000,100); beep(2000,50); }
void sndLaser() { for(int i=2000; i>200; i-=50) { tone(PIN_BUZZER,i,5); delay(5); } noTone(PIN_BUZZER); }

void sndPolice() {
  for(int j=0; j<3; j++) {
    for(int i=800; i<1200; i+=50) { tone(PIN_BUZZER,i,30); delay(30); }
    for(int i=1200; i>800; i-=50) { tone(PIN_BUZZER,i,30); delay(30); }
    yield();
  }
  noTone(PIN_BUZZER);
}

// GAME OVER sesi
void sndGameOver() {
  beep(NOTE_C5, 200);
  beep(NOTE_B4, 200);
  beep(NOTE_A4, 200);
  beep(NOTE_G4, 300);
  delay(100);
  beep(NOTE_G4, 150);
  beep(NOTE_A4, 150);
  beep(NOTE_G4, 150);
  beep(NOTE_E4, 400);
  delay(100);
  for(int i=400; i>100; i-=20) {
    tone(PIN_BUZZER, i, 30);
    delay(30);
  }
  noTone(PIN_BUZZER);
}

// Olum sesi
void sndDeath() {
  for(int i=1000; i>100; i-=30) {
    tone(PIN_BUZZER, i, 20);
    delay(25);
  }
  noTone(PIN_BUZZER);
  delay(200);
  beep(100, 500);
}

// Alkis sesi
void sndClap() {
  for(int i=0; i<3; i++) {
    beep(1500, 30);
    delay(70);
  }
}

void playMario() {
  int melody[] = {NOTE_E5,NOTE_E5,0,NOTE_E5,0,NOTE_C5,NOTE_E5,NOTE_G5};
  int noteDurations[] = {150,150,150,150,150,150,150,400};
  for(int noteIndex=0; noteIndex<8; noteIndex++) { beep(melody[noteIndex],noteDurations[noteIndex]); delay(30); yield(); }
}

void playStarWars() {
  int melody[] = {NOTE_A4,NOTE_A4,NOTE_A4,NOTE_F4,NOTE_C5,NOTE_A4,NOTE_F4,NOTE_C5,NOTE_A4};
  int noteDurations[] = {500,500,500,350,150,500,350,150,1000};
  for(int noteIndex=0; noteIndex<9; noteIndex++) { beep(melody[noteIndex],noteDurations[noteIndex]); delay(50); yield(); }
}

void playTetris() {
  int melody[] = {NOTE_E5,NOTE_B4,NOTE_C5,NOTE_D5,NOTE_C5,NOTE_B4,NOTE_A4};
  int noteDurations[] = {400,200,200,400,200,200,400};
  for(int noteIndex=0; noteIndex<7; noteIndex++) { beep(melody[noteIndex],noteDurations[noteIndex]); delay(50); yield(); }
}

void playPirates() {
  int melody[] = {NOTE_E4,NOTE_G4,NOTE_A4,NOTE_A4,0,NOTE_A4,NOTE_B4,NOTE_C5};
  int noteDurations[] = {150,150,300,150,150,150,150,300};
  for(int noteIndex=0; noteIndex<8; noteIndex++) { beep(melody[noteIndex],noteDurations[noteIndex]); delay(30); yield(); }
}

void playHappy() {
  int melody[] = {NOTE_G4,NOTE_G4,NOTE_A4,NOTE_G4,NOTE_C5,NOTE_B4};
  int noteDurations[] = {200,200,400,400,400,800};
  for(int noteIndex=0; noteIndex<6; noteIndex++) { beep(melody[noteIndex],noteDurations[noteIndex]); delay(50); yield(); }
}

void playVictory() {
  beep(NOTE_C5,150); beep(NOTE_C5,150); beep(NOTE_C5,150); beep(NOTE_C5,400);
  beep(NOTE_G4,400); beep(NOTE_A4,400); beep(NOTE_C5,200); beep(NOTE_A4,100); beep(NOTE_C5,500);
}

// ═══════════════════════════════════════════════════════════
//                       GOZ FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

int safeRadius(int radius, int width, int height) {
  if (width < 2*(radius+1)) radius = width/2 - 1;
  if (height < 2*(radius+1)) radius = height/2 - 1;
  return (radius < 0) ? 0 : radius;
}

void drawEyes() {
  int leftRadius = safeRadius(eyeRadius, eyeL.w, eyeL.h);
  int rightRadius = safeRadius(eyeRadius, eyeR.w, eyeR.h);
  oled.fillRoundRect(eyeL.x - eyeL.w/2, eyeL.y - eyeL.h/2, eyeL.w, eyeL.h, leftRadius, WHITE);
  oled.fillRoundRect(eyeR.x - eyeR.w/2, eyeR.y - eyeR.h/2, eyeR.w, eyeR.h, rightRadius, WHITE);
}

void updateDisplay() {
  oled.clearDisplay();
  drawEyes();
  oled.display();
}

void resetEyes() {
  eyeL.h = eyeR.h = 40;
  eyeL.w = eyeR.w = 40;
  eyeL.x = 39; eyeR.x = 89;
  eyeL.y = eyeR.y = 32;
  eyeRadius = 10;
  updateDisplay();
}

void eyeBlink() {
  for (int i = 0; i < 3; i++) {
    eyeL.h -= 12; eyeR.h -= 12;
    eyeL.w += 3; eyeR.w += 3;
    eyeRadius = max(1, eyeL.h/4);
    updateDisplay(); delay(20);
  }
  for (int i = 0; i < 3; i++) {
    eyeL.h += 12; eyeR.h += 12;
    eyeL.w -= 3; eyeR.w -= 3;
    eyeRadius = max(1, eyeL.h/4);
    updateDisplay(); delay(20);
  }
  resetEyes();
}

void eyeHappy() {
  resetEyes();
  for (int i = 0; i < 10; i++) {
    int off = 20 - i*2;
    oled.fillTriangle(eyeL.x-22, eyeL.y+off, eyeL.x+22, eyeL.y+off+5, eyeL.x-22, eyeL.y+40, BLACK);
    oled.fillTriangle(eyeR.x+22, eyeR.y+off, eyeR.x-22, eyeR.y+off+5, eyeR. x+22, eyeR. y+40, BLACK);
    oled.display(); delay(20);
  }
  delay(500);
}

void eyeSad() {
  resetEyes();
  for (int i = 0; i < 10; i++) {
    int off = -20 + i*2;
    oled.fillTriangle(eyeL.x+22, eyeL.y+off, eyeL.x-22, eyeL.y+off-5, eyeL.x+22, eyeL.y-40, BLACK);
    oled.fillTriangle(eyeR.x-22, eyeR.y+off, eyeR.x+22, eyeR.y+off-5, eyeR.x-22, eyeR.y-40, BLACK);
    oled.display(); delay(20);
  }
  delay(500);
}

void eyeAngry() {
  resetEyes();
  oled.fillTriangle(eyeL.x-22, eyeL.y-25, eyeL.x+22, eyeL.y-15, eyeL.x+22, eyeL.y-25, BLACK);
  oled. fillTriangle(eyeR. x+22, eyeR.y-25, eyeR.x-22, eyeR.y-15, eyeR.x-22, eyeR.y-25, BLACK);
  oled.display();
  delay(500);
}

void eyeSurprised() {
  for (int i = 0; i < 8; i++) {
    eyeL.h += 3; eyeL.w += 3;
    eyeR.h += 3; eyeR.w += 3;
    eyeRadius += 2;
    updateDisplay(); delay(30);
  }
  delay(400);
  resetEyes();
}

void eyeLove() {
  oled.clearDisplay();
  oled.fillCircle(30, 26, 12, WHITE);
  oled.fillCircle(48, 26, 12, WHITE);
  oled.fillTriangle(18, 32, 60, 32, 39, 55, WHITE);
  oled. fillCircle(80, 26, 12, WHITE);
  oled.fillCircle(98, 26, 12, WHITE);
  oled.fillTriangle(68, 32, 110, 32, 89, 55, WHITE);
  oled.display();
  delay(1000);
}

void eyeSleepy() {
  eyeL.h = eyeR.h = 4;
  eyeRadius = 0;
  updateDisplay();
}

void eyeCool() {
  resetEyes();
  oled.fillRect(0, 0, 128, 27, BLACK);
  oled.fillRect(54, 29, 20, 6, WHITE);
  oled.display();
  delay(500);
}

void eyeSkull() {
  oled.clearDisplay();
  oled.fillRoundRect(24, 5, 80, 50, 20, WHITE);
  oled.fillCircle(44, 28, 10, BLACK);
  oled.fillCircle(84, 28, 10, BLACK);
  oled.fillTriangle(64, 35, 58, 48, 70, 48, BLACK);
  for (int i = 0; i < 5; i++) oled.fillRect(36+i*12, 55, 8, 8, WHITE);
  oled.display();
  delay(500);
}

void eyeDizzy() {
  oled.clearDisplay();
  for (int r = 5; r < 20; r += 4) {
    oled.drawCircle(eyeL.x, eyeL.y, r, WHITE);
    oled.drawCircle(eyeR.x, eyeR.y, r, WHITE);
  }
  oled.display();
  for (int i = 0; i < 5; i++) {
    delay(150); oled.invertDisplay(true);
    delay(150); oled.invertDisplay(false);
  }
}

void eyeWink() {
  resetEyes();
  for (int i = 0; i < 5; i++) { eyeR.h -= 8; updateDisplay(); delay(30); }
  delay(200);
  for (int i = 0; i < 5; i++) { eyeR.h += 8; updateDisplay(); delay(30); }
  resetEyes();
}

void eyeLookLeft() {
  for (int i = 0; i < 8; i++) { eyeL. x -= 2; eyeR.x -= 2; updateDisplay(); delay(30); }
  delay(300);
  resetEyes();
}

void eyeLookRight() {
  for (int i = 0; i < 8; i++) { eyeL.x += 2; eyeR.x += 2; updateDisplay(); delay(30); }
  delay(300);
  resetEyes();
}

void eyeThinking() {
  for (int i = 0; i < 8; i++) {
    eyeL.x--; eyeL.y--;
    eyeR.x--; eyeR.y--;
    updateDisplay(); delay(50);
  }
  delay(800);
  resetEyes();
}

// X gozler (olu)
void eyeDead() {
  oled.clearDisplay();
  // Sol X
  oled.drawLine(eyeL.x-15, eyeL.y-15, eyeL.x+15, eyeL.y+15, WHITE);
  oled.drawLine(eyeL. x+15, eyeL.y-15, eyeL.x-15, eyeL.y+15, WHITE);
  oled.drawLine(eyeL.x-14, eyeL.y-15, eyeL.x+16, eyeL.y+15, WHITE);
  oled. drawLine(eyeL.x+16, eyeL.y-15, eyeL.x-14, eyeL.y+15, WHITE);
  // Sag X
  oled.drawLine(eyeR.x-15, eyeR.y-15, eyeR.x+15, eyeR.y+15, WHITE);
  oled. drawLine(eyeR.x+15, eyeR.y-15, eyeR.x-15, eyeR.y+15, WHITE);
  oled.drawLine(eyeR.x-14, eyeR.y-15, eyeR.x+16, eyeR.y+15, WHITE);
  oled.drawLine(eyeR.x+16, eyeR.y-15, eyeR.x-14, eyeR.y+15, WHITE);
  oled.display();
}

// Game Over ekrani
void showGameOver() {
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  oled.setCursor(10, 10);
  oled.println("GAME");
  oled.setCursor(10, 35);
  oled.println("OVER!");
  oled.display();
}

void showText(const char* line1, const char* line2, const char* line3) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println(line1);
  if (strlen(line2) > 0) oled.println(line2);
  if (strlen(line3) > 0) oled. println(line3);
  oled.display();
}

// ═══════════════════════════════════════════════════════════
//                     HAREKET FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

void initServos() {
  servo[0].attach(PIN_YL);
  servo[1].attach(PIN_YR);
  servo[2].attach(PIN_RL);
  servo[3].attach(PIN_RR);
}

void home() {
  for (int i = 0; i < 4; i++) servo[i].SetPosition(90);
  delay(200);
  isMoving = false;
}

void oscillate(int steps, int period, int amplitudes[], int offsets[], double phases[]) {
  for (int servoIndex = 0; servoIndex < 4; servoIndex++) {
    servo[servoIndex].SetO(offsets[servoIndex]);
    servo[servoIndex].SetA(amplitudes[servoIndex]);
    servo[servoIndex].SetPh(phases[servoIndex]);
    servo[servoIndex].SetT(period);
  }
  unsigned long referenceTime = millis();
  for (int stepIndex = 0; stepIndex < steps; stepIndex++) {
    while (millis() - referenceTime < (unsigned long)period) {
      for (int servoIndex = 0; servoIndex < 4; servoIndex++) servo[servoIndex].refresh();
      yield();
    }
    referenceTime = millis();
  }
}

void walk(int steps, int direction) {
  isMoving = true;
  int amplitudes[] = {15, 15, 30, 30};
  int offsets[] = {0, 0, 0, 0};
  double phases[] = {0, 0, DEG2RAD(direction*-90), DEG2RAD(direction*-90)};
  oscillate(steps, moveSpeed, amplitudes, offsets, phases);
  isMoving = false;
}

void turn(int steps, int direction) {
  isMoving = true;
  int amplitudes[] = {20, 20, 15, 15};
  int offsets[] = {0, 0, 0, 0};
  double phases[] = {0, 0, DEG2RAD(direction*-90), DEG2RAD(direction*-90)};
  oscillate(steps, moveSpeed, amplitudes, offsets, phases);
  isMoving = false;
}

void jump(int times) {
  for (int i = 0; i < times; i++) {
    servo[2].SetPosition(150);
    servo[3].SetPosition(30);
    delay(250);
    servo[2].SetPosition(90);
    servo[3].SetPosition(90);
    delay(250);
    yield();
  }
}

void bend(int direction) {
  servo[2].SetPosition(90 + direction*40);
  servo[3].SetPosition(90 - direction*40);
  delay(400);
  home();
}

void shakeLeg(int legIndex) {
  int footServo = (legIndex == 0) ? 2 : 3;
  servo[(legIndex == 0) ? 3 : 2].SetPosition(legIndex == 0 ? 60 : 120);
  delay(150);
  for (int shakeCount = 0; shakeCount < 5; shakeCount++) {
    servo[footServo].SetPosition(60); delay(80);
    servo[footServo].SetPosition(120); delay(80);
    yield();
  }
  home();
}

void kickLeft() {
  servo[2].SetPosition(50);
  servo[3].SetPosition(50);
  delay(150);
  servo[0].SetPosition(50);
  delay(150);
  servo[0].SetPosition(130);
  delay(200);
  home();
}

void kickRight() {
  servo[2].SetPosition(130);
  servo[3].SetPosition(130);
  delay(150);
  servo[1].SetPosition(130);
  delay(150);
  servo[1].SetPosition(50);
  delay(200);
  home();
}

// Alkis hareketi
void clap(int times) {
  for (int i = 0; i < times; i++) {
    servo[0].SetPosition(50);
    servo[1].SetPosition(130);
    delay(150);
    servo[0].SetPosition(90);
    servo[1].SetPosition(90);
    sndClap();
    delay(150);
    yield();
  }
}

// Olum hareketi
void deathMove() {
  eyeSurprised();
  sndSurprise();
  delay(300);
  
  // Titreme
  for (int i = 0; i < 5; i++) {
    servo[0].SetPosition(85);
    servo[1].SetPosition(95);
    delay(50);
    servo[0].SetPosition(95);
    servo[1].SetPosition(85);
    delay(50);
  }
  
  // Yavas yavas dusme
  eyeDead();
  sndDeath();
  
  for (int angle = 90; angle <= 150; angle += 5) {
    servo[2].SetPosition(angle);
    servo[3].SetPosition(180 - angle);
    delay(50);
  }
  
  // Yerde yatma
  servo[0].SetPosition(90);
  servo[1].SetPosition(90);
  servo[2].SetPosition(150);
  servo[3].SetPosition(30);
  
  delay(2000);
  
  // Canlanma
  beep(NOTE_C5, 200);
  beep(NOTE_E5, 200);
  beep(NOTE_G5, 300);
  
  home();
  resetEyes();
  sndHappy();
}

// Game Over hareketi
void gameOverMove() {
  showGameOver();
  sndGameOver();
  
  // Uzgun hareket
  for (int i = 0; i < 3; i++) {
    servo[2].SetPosition(70);
    servo[3].SetPosition(110);
    delay(200);
    servo[2].SetPosition(110);
    servo[3].SetPosition(70);
    delay(200);
    yield();
  }
  
  eyeDead();
  delay(1000);
  
  // Dusme
  servo[2].SetPosition(140);
  servo[3].SetPosition(40);
  delay(500);
  
  // Flash efekti
  for (int i = 0; i < 5; i++) {
    oled.invertDisplay(true);
    delay(100);
    oled.invertDisplay(false);
    delay(100);
  }
  
  delay(1500);
  home();
  resetEyes();
}

// ═══════════════════════════════════════════════════════════
//                      DANS FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

void moonwalk(int steps) {
  int amplitudes[] = {25, 25, 25, 25};
  int offsets[] = {-15, 15, 0, 0};
  double phases[] = {0, DEG2RAD(300), DEG2RAD(-90), DEG2RAD(90)};
  oscillate(steps, moveSpeed, amplitudes, offsets, phases);
}

void swing(int steps) {
  int amplitudes[] = {25, 25, 20, 20};
  int offsets[] = {-15, 15, 0, 0};
  double phases[] = {0, 0, DEG2RAD(-90), DEG2RAD(90)};
  oscillate(steps, moveSpeed, amplitudes, offsets, phases);
}

void crusaito(int steps, int direction) {
  int amplitudes[] = {25, 25, 20, 20};
  int offsets[] = {-15*direction, 15*direction, 0, 0};
  double phases[] = {0, DEG2RAD(180), DEG2RAD(-90*direction), DEG2RAD(90*direction)};
  oscillate(steps, moveSpeed, amplitudes, offsets, phases);
}

void flapping(int steps) {
  int amplitudes[] = {15, 15, 20, 20};
  int offsets[] = {0, 0, -20, 20};
  double phases[] = {0, DEG2RAD(180), DEG2RAD(-90), DEG2RAD(90)};
  oscillate(steps, moveSpeed, amplitudes, offsets, phases);
}

void jitter(int steps) {
  int amplitudes[] = {20, 20, 0, 0};
  int offsets[] = {0, 0, 0, 0};
  double phases[] = {0, DEG2RAD(180), 0, 0};
  oscillate(steps, moveSpeed/2, amplitudes, offsets, phases);
}

void tiptoe(int steps) {
  int amplitudes[] = {20, 20, 20, 20};
  int offsets[] = {0, 0, 20, -20};
  double phases[] = {0, DEG2RAD(180), 0, DEG2RAD(180)};
  oscillate(steps, moveSpeed, amplitudes, offsets, phases);
}

void upDown(int steps) {
  for (int i = 0; i < steps; i++) {
    servo[2].SetPosition(70); servo[3].SetPosition(110); delay(250);
    servo[2].SetPosition(110); servo[3]. SetPosition(70); delay(250);
    yield();
  }
}

void shake(int steps) {
  int amplitudes[] = {0, 0, 15, 15};
  int offsets[] = {0, 0, 0, 0};
  double phases[] = {0, 0, 0, DEG2RAD(180)};
  oscillate(steps, moveSpeed/3, amplitudes, offsets, phases);
}

void gangnam(int steps) {
  for (int i = 0; i < steps; i++) {
    servo[0].SetPosition(70); servo[1].SetPosition(110);
    servo[2].SetPosition(110); servo[3].SetPosition(70);
    delay(200);
    home(); delay(100);
    servo[0].SetPosition(110); servo[1]. SetPosition(70);
    servo[2].SetPosition(70); servo[3].SetPosition(110);
    delay(200);
    home(); delay(100);
    yield();
  }
}

void dab(int direction) {
  servo[0]. SetPosition(90 + direction*35);
  servo[1].SetPosition(90 + direction*35);
  servo[2].SetPosition(90 + direction*45);
  servo[3].SetPosition(90 - direction*45);
  delay(500);
  home();
}

void twist(int steps) {
  for (int i = 0; i < steps; i++) {
    servo[0].SetPosition(60); servo[1].SetPosition(120); delay(200);
    servo[0].SetPosition(120); servo[1]. SetPosition(60); delay(200);
    yield();
  }
  home();
}

void wave(int steps) {
  for (int i = 0; i < steps; i++) {
    servo[2].SetPosition(50); delay(150);
    servo[2].SetPosition(130); delay(150);
    yield();
  }
  home();
}

void twerk(int steps) {
  for (int i = 0; i < steps; i++) {
    servo[2].SetPosition(50); servo[3].SetPosition(130); delay(100);
    servo[2].SetPosition(130); servo[3]. SetPosition(50); delay(100);
    yield();
  }
  home();
}

void robot(int steps) {
  for (int i = 0; i < steps; i++) {
    servo[0].SetPosition(70); delay(200);
    servo[0].SetPosition(90); delay(100);
    servo[1].SetPosition(110); delay(200);
    servo[1].SetPosition(90); delay(100);
    servo[2].SetPosition(70); delay(200);
    servo[2].SetPosition(90); delay(100);
    servo[3].SetPosition(110); delay(200);
    servo[3].SetPosition(90); delay(100);
    yield();
  }
}

void randomDance() {
  int danceIndex = random(0, 15);
  switch (danceIndex) {
    case 0: moonwalk(3); break;
    case 1: swing(3); break;
    case 2: crusaito(3, 1); break;
    case 3: flapping(3); break;
    case 4: jitter(5); break;
    case 5: tiptoe(3); break;
    case 6: upDown(4); break;
    case 7: shake(6); break;
    case 8: gangnam(3); break;
    case 9: dab(random(0,2)==0 ? 1 : -1); break;
    case 10: twist(3); break;
    case 11: wave(4); break;
    case 12: twerk(5); break;
    case 13: robot(2); break;
    case 14: kickLeft(); kickRight(); break;
  }
  home();
}

void partyMode() {
  eyeHappy();
  randomDance();
  
  int soundIndex = random(0, 6);
  switch(soundIndex) {
    case 0: playMario(); break;
    case 1: sndR2D2(); break;
    case 2: sndHappy(); break;
    case 3: playVictory(); break;
    case 4: sndLaser(); break;
    case 5: beep(random(500,2000), 100); break;
  }
  
  resetEyes();
}

// ═══════════════════════════════════════════════════════════
//                   SARKILI DANS FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

// ANNENI SEVIYORSAN ALKISLA
void danceAnneniSeviyorsan() {
  showText("Anneni Seviyorsan", "Alkisla!", "");
  eyeLove();
  delay(500);
  
  // "Anneni seviyorsan alkisla" - 2 kez alkis
  beep(NOTE_C5, 200); beep(NOTE_C5, 200); beep(NOTE_D5, 200); beep(NOTE_E5, 200);
  clap(2);
  
  // "Anneni seviyorsan alkisla" - 2 kez alkis
  beep(NOTE_E5, 200); beep(NOTE_E5, 200); beep(NOTE_D5, 200); beep(NOTE_C5, 200);
  clap(2);
  
  // "Anneni seviyorsan bak halinden belli" - dans
  beep(NOTE_C5, 200); beep(NOTE_D5, 200); beep(NOTE_E5, 200); beep(NOTE_F5, 200);
  gangnam(1);
  
  // "Anneni seviyorsan alkisla" - 2 kez alkis
  beep(NOTE_E5, 200); beep(NOTE_D5, 200); beep(NOTE_C5, 400);
  clap(2);
  
  // Tekrar
  eyeHappy();
  beep(NOTE_C5, 200); beep(NOTE_C5, 200); beep(NOTE_D5, 200); beep(NOTE_E5, 200);
  clap(2);
  
  beep(NOTE_E5, 200); beep(NOTE_E5, 200); beep(NOTE_D5, 200); beep(NOTE_C5, 200);
  clap(2);
  
  eyeWink();
  sndHappy();
  home();
  resetEyes();
}

// IF YOU'RE HAPPY AND YOU KNOW IT
void danceIfYoureHappy() {
  showText("If You're Happy", "Clap Your Hands!", "");
  eyeHappy();
  delay(500);
  
  // If you're happy and you know it clap your hands
  beep(NOTE_C5, 250); beep(NOTE_C5, 250); 
  beep(NOTE_E5, 250); beep(NOTE_E5, 250);
  beep(NOTE_G5, 250); beep(NOTE_G5, 250);
  beep(NOTE_E5, 500);
  clap(2);
  
  // If you're happy and you know it clap your hands
  beep(NOTE_D5, 250); beep(NOTE_D5, 250);
  beep(NOTE_F5, 250); beep(NOTE_F5, 250);
  beep(NOTE_A5, 250); beep(NOTE_A5, 250);
  beep(NOTE_G5, 500);
  clap(2);
  
  // If you're happy and you know it and you really want to show it
  beep(NOTE_C5, 200); beep(NOTE_C5, 200);
  beep(NOTE_E5, 200); beep(NOTE_E5, 200);
  beep(NOTE_G5, 200); beep(NOTE_G5, 200);
  gangnam(1);
  
  // If you're happy and you know it clap your hands
  beep(NOTE_E5, 250); beep(NOTE_D5, 250);
  beep(NOTE_C5, 500);
  clap(2);
  
  eyeWink();
  home();
  resetEyes();
}

// BABY SHARK
void danceBabyShark() {
  showText("Baby Shark", "Doo doo doo!", "");
  eyeHappy();
  delay(500);
  
  // Baby shark doo doo doo doo doo doo
  for (int i = 0; i < 2; i++) {
    beep(NOTE_G4, 150);
    servo[0].SetPosition(60);
    delay(100);
    beep(NOTE_G4, 150);
    servo[0]. SetPosition(90);
    delay(100);
    beep(NOTE_G4, 150);
    servo[1].SetPosition(120);
    delay(100);
    beep(NOTE_G4, 150);
    servo[1].SetPosition(90);
    delay(100);
    beep(NOTE_G4, 150);
    servo[0].SetPosition(60);
    delay(100);
    beep(NOTE_G4, 150);
    servo[0].SetPosition(90);
    delay(100);
    
    beep(NOTE_G4, 400);
    // Agiz acma hareketi
    servo[2].SetPosition(70);
    servo[3].SetPosition(110);
    delay(200);
    servo[2].SetPosition(90);
    servo[3].SetPosition(90);
    delay(200);
    yield();
  }
  
  // Mommy shark
  eyeLove();
  for (int i = 0; i < 2; i++) {
    beep(NOTE_A4, 150);
    swing(1);
    beep(NOTE_A4, 150);
    beep(NOTE_A4, 150);
    beep(NOTE_A4, 150);
    beep(NOTE_A4, 150);
    beep(NOTE_A4, 150);
    clap(1);
    yield();
  }
  
  // Run away! 
  eyeSurprised();
  beep(NOTE_C5, 300);
  beep(NOTE_D5, 300);
  beep(NOTE_E5, 300);
  beep(NOTE_F5, 300);
  beep(NOTE_G5, 600);
  
  // Kacma hareketi
  for (int i = 0; i < 3; i++) {
    walk(1, 1);
    yield();
  }
  
  sndHappy();
  home();
  resetEyes();
}

// HEAD SHOULDERS KNEES AND TOES
void danceHeadShoulders() {
  showText("Head, Shoulders", "Knees and Toes!", "");
  eyeHappy();
  delay(500);
  
  // Head
  beep(NOTE_C5, 300);
  servo[2].SetPosition(60); servo[3].SetPosition(120);
  delay(300);
  
  // Shoulders
  beep(NOTE_D5, 300);
  servo[0].SetPosition(60); servo[1].SetPosition(120);
  delay(300);
  servo[0].SetPosition(90); servo[1].SetPosition(90);
  
  // Knees
  beep(NOTE_E5, 300);
  servo[2]. SetPosition(120); servo[3].SetPosition(60);
  delay(300);
  
  // And toes
  beep(NOTE_F5, 300);
  servo[2].SetPosition(140); servo[3].SetPosition(40);
  delay(300);
  
  // Knees and toes
  beep(NOTE_E5, 300);
  servo[2].SetPosition(120); servo[3].SetPosition(60);
  delay(200);
  beep(NOTE_F5, 400);
  servo[2].SetPosition(140); servo[3]. SetPosition(40);
  delay(400);
  
  home();
  
  // Tekrar hizli
  beep(NOTE_C5, 200); bend(1);
  beep(NOTE_D5, 200); twist(1);
  beep(NOTE_E5, 200); upDown(1);
  beep(NOTE_F5, 200); jump(1);
  beep(NOTE_E5, 200); beep(NOTE_F5, 300);
  shake(2);
  
  sndHappy();
  home();
  resetEyes();
}

// Muzikli danslar - digerleri
void danceMario() {
  eyeHappy();
  beep(NOTE_E5, 150); jump(1);
  beep(NOTE_E5, 150); jump(1);
  beep(0, 150);
  beep(NOTE_E5, 150); shake(2);
  beep(0, 150);
  beep(NOTE_C5, 150); twist(1);
  beep(NOTE_E5, 150); moonwalk(1);
  beep(NOTE_G5, 400); gangnam(1);
  home();
  resetEyes();
}

void danceDisco() {
  eyeLove();
  for (int i = 0; i < 4; i++) {
    beep(NOTE_C5, 100);
    servo[0].SetPosition(60); servo[1].SetPosition(120);
    servo[2].SetPosition(70); servo[3].SetPosition(110);
    delay(150);
    beep(NOTE_G4, 100);
    servo[0].SetPosition(120); servo[1].SetPosition(60);
    servo[2]. SetPosition(110); servo[3].SetPosition(70);
    delay(150);
    beep(NOTE_E5, 100);
    dab(1);
    beep(NOTE_D5, 100);
    dab(-1);
    yield();
  }
  home();
  resetEyes();
}

void danceSalsa() {
  eyeHappy();
  for (int i = 0; i < 3; i++) {
    beep(NOTE_A4, 150); 
    servo[2].SetPosition(60); servo[3].SetPosition(120);
    delay(100);
    beep(NOTE_C5, 150);
    servo[2].SetPosition(120); servo[3].SetPosition(60);
    delay(100);
    beep(NOTE_E5, 150);
    crusaito(1, 1);
    beep(NOTE_A4, 150);
    crusaito(1, -1);
    beep(NOTE_G4, 200);
    swing(1);
    yield();
  }
  sndHappy();
  home();
  resetEyes();
}

void danceRock() {
  eyeAngry();
  for (int i = 0; i < 4; i++) {
    beep(200, 100);
    servo[0].SetPosition(50); servo[1].SetPosition(50);
    delay(100);
    beep(250, 100);
    servo[0].SetPosition(130); servo[1].SetPosition(130);
    delay(100);
    beep(300, 150);
    shake(2);
    beep(200, 100);
    jitter(2);
    yield();
  }
  home();
  resetEyes();
}

void danceStarWars() {
  eyeSkull();
  beep(NOTE_A4, 500);
  robot(1);
  beep(NOTE_A4, 500);
  moonwalk(2);
  beep(NOTE_F4, 350);
  dab(1);
  beep(NOTE_C5, 150);
  dab(-1);
  beep(NOTE_A4, 1000);
  twist(2);
  home();
  resetEyes();
}

void danceTetris() {
  eyeHappy();
  beep(NOTE_E5, 400); upDown(1);
  beep(NOTE_B4, 200); twist(1);
  beep(NOTE_C5, 200); shake(2);
  beep(NOTE_D5, 400); gangnam(1);
  beep(NOTE_C5, 200); flapping(1);
  beep(NOTE_B4, 200); jitter(2);
  beep(NOTE_A4, 400); moonwalk(1);
  home();
  resetEyes();
}

void danceHipHop() {
  eyeCool();
  for (int i = 0; i < 3; i++) {
    beep(NOTE_G4, 100);
    robot(1);
    moonwalk(1);
    beep(NOTE_A4, 100);
    wave(2);
    beep(NOTE_E4, 200);
    dab(random(0,2) == 0 ? 1 : -1);
    twerk(2);
    yield();
  }
  home();
  resetEyes();
}

void randomMusicDance() {
  int danceIndex = random(0, 12);
  switch (danceIndex) {
    case 0: danceMario(); break;
    case 1: danceDisco(); break;
    case 2: danceSalsa(); break;
    case 3: danceRock(); break;
    case 4: danceHipHop(); break;
    case 5: danceStarWars(); break;
    case 6: danceTetris(); break;
    case 7: danceAnneniSeviyorsan(); break;
    case 8: danceIfYoureHappy(); break;
    case 9: danceBabyShark(); break;
    case 10: danceHeadShoulders(); break;
    case 11: playVictory(); gangnam(2); break;
  }
}

// ═══════════════════════════════════════════════════════════
//                    ULTRASONIK SENSOR
// ═══════════════════════════════════════════════════════════

long getDistance() {
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);
  long pulseDuration = pulseIn(PIN_ECHO, HIGH, 25000);
  long distanceCm = pulseDuration * 0.034 / 2;
  if (distanceCm == 0 || distanceCm > 300) distanceCm = 300;
  return distanceCm;
}

bool isObstacle() {
  return getDistance() < OBSTACLE_DIST;
}

// ═══════════════════════════════════════════════════════════
//                      OTONOM MOD
// ═══════════════════════════════════════════════════════════

void autonomousMode() {
  if (currentMode != MODE_AUTO) return;
  if (millis() - lastObstacle < 150) return;
  lastObstacle = millis();
  
  if (isObstacle()) {
    home();
    eyeSurprised();
    sndSurprise();
    delay(200);
    eyeSad();
    walk(2, -1);
    home();
    delay(100);
    if (random(0, 2) == 0) {
      eyeLookLeft();
      turn(3, 1);
    } else {
      eyeLookRight();
      turn(3, -1);
    }
    home();
    resetEyes();
    sndHappy();
  } else {
    eyeHappy();
    walk(1, 1);
  }
}

// ═══════════════════════════════════════════════════════════
//                        DEMO MODU
// ═══════════════════════════════════════════════════════════

void demoMode() {
  showText("DEMO MODE", "Basliyor.. .", "");
  delay(800);
  
  eyeHappy(); sndHappy();
  resetEyes(); delay(300);
  
  showText("Yurume", "", "");
  walk(3, 1);
  walk(2, -1);
  home();
  
  showText("Ziplama", "", "");
  eyeSurprised();
  jump(2);
  home();
  
  showText("Dans: Gangnam", "", "");
  eyeHappy();
  gangnam(2);
  home();
  
  showText("Sarkili Dans:", "Anneni Seviyorsan", "");
  danceAnneniSeviyorsan();
  
  showText("Olum Hareketi", "", "");
  deathMove();
  
  showText("Game Over!", "", "");
  gameOverMove();
  
  resetEyes();
  home();
  playVictory();
  
  showText("DEMO BITTI!", "Otto hazir!", "");
  delay(2000);
  resetEyes();
}

// ═══════════════════════════════════════════════════════════
//                      WEB ARAYUZU
// ═══════════════════════════════════════════════════════════

const char WEBPAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Otto Robot v5</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial;background:linear-gradient(135deg,#1a1a2e,#16213e);min-height:100vh;color:#fff;padding:10px}
.hdr{text-align:center;padding:15px;background:rgba(255,255,255,. 1);border-radius:15px;margin-bottom:15px}
h1{font-size:22px}
. status{font-size:12px;opacity:.7;margin-top:8px}
.modes{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin-bottom:15px}
.mode{padding:12px 18px;border-radius:25px;border:none;font-size:13px;font-weight:700;cursor:pointer;color:#fff}
.m1{background:linear-gradient(45deg,#11998e,#38ef7d)}
.m2{background:linear-gradient(45deg,#2196F3,#21CBF3)}
.m3{background:linear-gradient(45deg,#8E2DE2,#4A00E0)}
.m4{background:linear-gradient(45deg,#f46b45,#eea849)}
.m5{background:linear-gradient(45deg,#cb2d3e,#ef473a)}
.m6{background:linear-gradient(45deg,#ff0844,#ffb199)}
.demo{width:100%;padding:15px;font-size:18px;background:linear-gradient(45deg,#ff0844,#ffb199);border:none;border-radius:12px;color:#fff;font-weight:700;cursor:pointer;margin-bottom:15px}
.tabs{display:flex;gap:5px;margin-bottom:15px;justify-content:center;flex-wrap:wrap}
. tab{padding:10px 15px;background:rgba(255,255,255,.1);border:none;border-radius:8px;color:#fff;cursor:pointer;font-size:12px}
.tab.active,. tab:hover{background:linear-gradient(45deg,#00d2ff,#3a7bd5)}
.panel{display:none;background:rgba(255,255,255,.05);border-radius:15px;padding:15px}
.panel.active{display:block}
.sec{margin-bottom:18px}
.sec h3{color:#00d2ff;font-size:15px;margin-bottom:10px;border-bottom:1px solid rgba(0,210,255,. 3);padding-bottom:8px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(75px,1fr));gap:8px}
.btn{padding:12px 8px;border:none;border-radius:10px;font-size:11px;font-weight:700;cursor:pointer;color:#fff}
.btn:active{transform:scale(. 95)}
.g{background:linear-gradient(45deg,#11998e,#38ef7d)}
.b{background:linear-gradient(45deg,#2196F3,#21CBF3)}
. o{background:linear-gradient(45deg,#f46b45,#eea849)}
.p{background:linear-gradient(45deg,#8E2DE2,#4A00E0)}
.pk{background:linear-gradient(45deg,#ee0979,#ff6a00)}
.r{background:linear-gradient(45deg,#cb2d3e,#ef473a)}
.c{background:linear-gradient(45deg,#00d2ff,#928DAB)}
.gd{background:linear-gradient(45deg,#f7971e,#ffd200);color:#000}
.mg{background:linear-gradient(45deg,#ff416c,#ff4b2b)}
.dr{background:linear-gradient(45deg,#434343,#000000)}
.joy{display:flex;justify-content:center;margin:15px 0}
.jg{display:grid;grid-template-columns:repeat(3,55px);grid-template-rows:repeat(3,55px);gap:5px}
.jg . btn{font-size:18px;padding:0;display:flex;align-items:center;justify-content:center}
.jc{background:linear-gradient(45deg,#667eea,#764ba2);border-radius:50%}
</style>
</head>
<body>
<div class="hdr">
<h1>🤖 Otto Super Robot v5. 0</h1>
<div class="status" id="st">Mod: Bekleme</div>
</div>

<div class="modes">
<button class="mode m1" onclick="c('idle')">😊 Bekleme</button>
<button class="mode m2" onclick="c('auto')">🚗 Otonom</button>
<button class="mode m3" onclick="c('dance')">💃 Dans</button>
<button class="mode m4" onclick="c('party')">🎉 Parti</button>
<button class="mode m6" onclick="c('mdance')">🎵 Muzikli</button>
<button class="mode m5" onclick="c('sleep')">😴 Uyku</button>
</div>

<button class="demo" onclick="c('demo')">🎬 DEMO BASLAT</button>

<div class="tabs">
<button class="tab active" onclick="sw('p1',this)">🎮 Kontrol</button>
<button class="tab" onclick="sw('p2',this)">💃 Dans</button>
<button class="tab" onclick="sw('p3',this)">🎵 Sarkilar</button>
<button class="tab" onclick="sw('p4',this)">👀 Gozler</button>
<button class="tab" onclick="sw('p5',this)">🔊 Sesler</button>
<button class="tab" onclick="sw('p6',this)">🎮 Oyunlar</button>
</div>

<div class="panel active" id="p1">
<div class="sec">
<h3>🕹️ Joystick</h3>
<div class="joy"><div class="jg">
<div></div><button class="btn g" onclick="c('fwd')">⬆️</button><div></div>
<button class="btn b" onclick="c('left')">⬅️</button>
<button class="btn jc" onclick="c('stop')">⏹️</button>
<button class="btn b" onclick="c('right')">➡️</button>
<div></div><button class="btn g" onclick="c('back')">⬇️</button><div></div>
</div></div>
</div>
<div class="sec">
<h3>🦵 Hareketler</h3>
<div class="grid">
<button class="btn o" onclick="c('jump')">🦘 Zipla</button>
<button class="btn o" onclick="c('bend')">🙇 Egil</button>
<button class="btn o" onclick="c('sleft')">🦵 Sol Salla</button>
<button class="btn o" onclick="c('sright')">🦵 Sag Salla</button>
<button class="btn o" onclick="c('kleft')">🦶 Sol Tekme</button>
<button class="btn o" onclick="c('kright')">🦶 Sag Tekme</button>
<button class="btn pk" onclick="c('clap')">👏 Alkisla</button>
</div>
</div>
</div>

<div class="panel" id="p2">
<div class="sec">
<h3>💃 Dans Hareketleri</h3>
<div class="grid">
<button class="btn p" onclick="c('moon')">🌙 Moonwalk</button>
<button class="btn p" onclick="c('swing')">🎷 Swing</button>
<button class="btn p" onclick="c('crus')">💫 Crusaito</button>
<button class="btn p" onclick="c('flap')">🦅 Flapping</button>
<button class="btn p" onclick="c('jit')">⚡ Jitter</button>
<button class="btn p" onclick="c('tip')">🩰 Tiptoe</button>
<button class="btn p" onclick="c('updn')">↕️ Up-Down</button>
<button class="btn p" onclick="c('shake')">🫨 Shake</button>
<button class="btn p" onclick="c('gang')">🕺 Gangnam</button>
<button class="btn p" onclick="c('dab')">🙆 Dab</button>
<button class="btn p" onclick="c('twist')">🌀 Twist</button>
<button class="btn p" onclick="c('wave')">👋 Wave</button>
<button class="btn p" onclick="c('twerk')">🍑 Twerk</button>
<button class="btn p" onclick="c('robot')">🤖 Robot</button>
<button class="btn gd" onclick="c('rdance')">🎲 Rastgele</button>
</div>
</div>
</div>

<div class="panel" id="p3">
<div class="sec">
<h3>🎵 Cocuk Sarkilari</h3>
<div class="grid">
<button class="btn mg" onclick="c('danne')">❤️ Anneni Sev</button>
<button class="btn mg" onclick="c('dhappy')">😊 If Happy</button>
<button class="btn mg" onclick="c('dshark')">🦈 Baby Shark</button>
<button class="btn mg" onclick="c('dhead')">👤 Head Shou. </button>
</div>
</div>
<div class="sec">
<h3>🎶 Muzikli Danslar</h3>
<div class="grid">
<button class="btn mg" onclick="c('dmario')">🍄 Mario</button>
<button class="btn mg" onclick="c('ddisco')">🪩 Disco</button>
<button class="btn mg" onclick="c('dsalsa')">💃 Salsa</button>
<button class="btn mg" onclick="c('drock')">🎸 Rock</button>
<button class="btn mg" onclick="c('dhiphop')">🎤 Hip-Hop</button>
<button class="btn mg" onclick="c('dstarw')">⭐ Star Wars</button>
<button class="btn mg" onclick="c('dtetris')">🧱 Tetris</button>
<button class="btn gd" onclick="c('rmdance')">🎲 Rastgele</button>
</div>
</div>
</div>

<div class="panel" id="p4">
<div class="sec">
<h3>👀 Goz Ifadeleri</h3>
<div class="grid">
<button class="btn c" onclick="c('enorm')">😐 Normal</button>
<button class="btn c" onclick="c('ehappy')">😊 Mutlu</button>
<button class="btn c" onclick="c('esad')">😢 Uzgun</button>
<button class="btn c" onclick="c('eangry')">😠 Kizgin</button>
<button class="btn c" onclick="c('esurp')">😲 Saskin</button>
<button class="btn c" onclick="c('elove')">😍 Asik</button>
<button class="btn c" onclick="c('esleep')">😴 Uykulu</button>
<button class="btn c" onclick="c('ecool')">😎 Cool</button>
<button class="btn c" onclick="c('eskull')">💀 Iskelet</button>
<button class="btn c" onclick="c('edizzy')">😵 Sersem</button>
<button class="btn c" onclick="c('ewink')">😉 Goz Kirp</button>
<button class="btn c" onclick="c('ethink')">🤔 Dusunce</button>
<button class="btn r" onclick="c('edead')">❌ Olu</button>
</div>
</div>
<div class="sec">
<h3>👁️ Goz Hareketleri</h3>
<div class="grid">
<button class="btn b" onclick="c('blink')">😑 Kirp</button>
<button class="btn b" onclick="c('eleft')">👈 Sola Bak</button>
<button class="btn b" onclick="c('eright')">👉 Saga Bak</button>
</div>
</div>
</div>

<div class="panel" id="p5">
<div class="sec">
<h3>🔊 Kisa Sesler</h3>
<div class="grid">
<button class="btn pk" onclick="c('shappy')">😊 Mutlu</button>
<button class="btn pk" onclick="c('ssad')">😢 Uzgun</button>
<button class="btn pk" onclick="c('sangry')">😠 Kizgin</button>
<button class="btn pk" onclick="c('ssurp')">😲 Saskin</button>
<button class="btn pk" onclick="c('sfart')">💨 Osuruk</button>
<button class="btn pk" onclick="c('scuddly')">🥰 Tatli</button>
<button class="btn pk" onclick="c('spolice')">🚔 Polis</button>
<button class="btn pk" onclick="c('sr2d2')">🤖 R2D2</button>
<button class="btn pk" onclick="c('slaser')">⚡ Lazer</button>
<button class="btn r" onclick="c('sgameover')">💀 Game Over</button>
<button class="btn r" onclick="c('sdeath')">☠️ Olum</button>
<button class="btn o" onclick="c('sclap')">👏 Alkis</button>
</div>
</div>
<div class="sec">
<h3>🎵 Melodiler</h3>
<div class="grid">
<button class="btn gd" onclick="c('mmario')">🍄 Mario</button>
<button class="btn gd" onclick="c('mstar')">⭐ Star Wars</button>
<button class="btn gd" onclick="c('mtetris')">🧱 Tetris</button>
<button class="btn gd" onclick="c('mpirate')">🏴‍☠️ Pirates</button>
<button class="btn gd" onclick="c('mhappy')">😊 Happy</button>
<button class="btn gd" onclick="c('mvic')">🏆 Victory</button>
</div>
</div>
</div>

<div class="panel" id="p6">
<div class="sec">
<h3>🎮 Oyun Efektleri</h3>
<div class="grid">
<button class="btn dr" onclick="c('death')">💀 Olum</button>
<button class="btn dr" onclick="c('gameover')">🎮 Game Over</button>
<button class="btn g" onclick="c('victory')">🏆 Zafer</button>
<button class="btn o" onclick="c('hit')">💥 Vuruldu</button>
<button class="btn b" onclick="c('levelup')">⬆️ Level Up</button>
<button class="btn p" onclick="c('powerup')">⚡ Power Up</button>
</div>
</div>
<div class="sec">
<h3>🎭 Ozel Hareketler</h3>
<div class="grid">
<button class="btn mg" onclick="c('hello')">👋 Merhaba</button>
<button class="btn mg" onclick="c('bye')">👋 Gule Gule</button>
<button class="btn mg" onclick="c('yes')">✅ Evet</button>
<button class="btn mg" onclick="c('no')">❌ Hayir</button>
<button class="btn mg" onclick="c('confused')">🤷 Anlamadim</button>
<button class="btn mg" onclick="c('love')">❤️ Seni Seviyorum</button>
</div>
</div>
</div>

<script>
function c(x){fetch('/c?q='+x).then(r=>r.text()).then(t=>document.getElementById('st').innerText=t)}
function sw(id,el){
document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
document. getElementById(id).classList.add('active');
el.classList.add('active');
}
</script>
</body>
</html>
)rawliteral";

// ═══════════════════════════════════════════════════════════
//                    OZEL HAREKETLER
// ═══════════════════════════════════════════════════════════

void sayHello() {
  showText("Merhaba!", "", "");
  eyeHappy();
  sndHappy();
  
  // El sallama
  for (int i = 0; i < 4; i++) {
    servo[0].SetPosition(60);
    delay(150);
    servo[0].SetPosition(120);
    delay(150);
    yield();
  }
  servo[0].SetPosition(90);
  
  home();
  resetEyes();
}

void sayBye() {
  showText("Gule Gule!", "", "");
  eyeSad();
  sndSad();
  
  // El sallama yavas
  for (int i = 0; i < 3; i++) {
    servo[0].SetPosition(50);
    delay(200);
    servo[0].SetPosition(130);
    delay(200);
    yield();
  }
  
  // Egil
  bend(1);
  
  home();
  resetEyes();
}

void sayYes() {
  showText("Evet!", "", "");
  eyeHappy();
  sndHappy();
  
  // Kafa sallama (evet)
  for (int i = 0; i < 3; i++) {
    servo[2].SetPosition(70);
    servo[3].SetPosition(110);
    delay(150);
    servo[2].SetPosition(110);
    servo[3].SetPosition(70);
    delay(150);
    yield();
  }
  
  home();
  resetEyes();
}

void sayNo() {
  showText("Hayir!", "", "");
  eyeAngry();
  sndAngry();
  
  // Kafa sallama (hayir)
  for (int i = 0; i < 3; i++) {
    servo[0].SetPosition(60);
    servo[1].SetPosition(60);
    delay(150);
    servo[0].SetPosition(120);
    servo[1].SetPosition(120);
    delay(150);
    yield();
  }
  
  home();
  resetEyes();
}

void sayConfused() {
  showText("Anlamadim? ", "", "");
  eyeThinking();
  
  beep(NOTE_E4, 150);
  beep(NOTE_G4, 100);
  beep(NOTE_E4, 150);
  
  // Omuz silkme
  servo[2].SetPosition(70);
  servo[3].SetPosition(110);
  delay(300);
  servo[2].SetPosition(110);
  servo[3].SetPosition(70);
  delay(300);
  
  home();
  resetEyes();
}

void sayLove() {
  showText("Seni", "Seviyorum!", "<3");
  eyeLove();
  sndCuddly();
  
  // Kalp atisi hareketi
  for (int i = 0; i < 4; i++) {
    servo[2].SetPosition(80);
    servo[3].SetPosition(100);
    delay(150);
    servo[2].SetPosition(100);
    servo[3].SetPosition(80);
    delay(150);
    yield();
  }
  
  // Sarılma hareketi
  servo[0].SetPosition(50);
  servo[1].SetPosition(130);
  delay(500);
  servo[0].SetPosition(130);
  servo[1].SetPosition(50);
  delay(500);
  
  home();
  resetEyes();
}

// Vuruldu efekti
void hitEffect() {
  eyeSurprised();
  sndSurprise();
  
  // Geri sekme
  servo[2].SetPosition(60);
  servo[3].SetPosition(120);
  delay(100);
  
  // Titreme
  for (int i = 0; i < 5; i++) {
    servo[0].SetPosition(85);
    servo[1].SetPosition(95);
    delay(30);
    servo[0].SetPosition(95);
    servo[1].SetPosition(85);
    delay(30);
  }
  
  home();
  eyeSad();
  delay(500);
  resetEyes();
}

// Level Up efekti
void levelUpEffect() {
  showText("LEVEL UP!", "", "");
  
  // Yukari bakma
  servo[2].SetPosition(60);
  servo[3].SetPosition(120);
  
  // Ses efekti
  for (int i = 300; i < 1000; i += 50) {
    beep(i, 30);
  }
  beep(1200, 200);
  
  eyeHappy();
  jump(2);
  sndHappy();
  
  home();
  resetEyes();
}

// Power Up efekti
void powerUpEffect() {
  showText("POWER UP!", "", "");
  eyeCool();
  
  // Titresim + ses
  for (int i = 0; i < 10; i++) {
    servo[2].SetPosition(80 + random(-10, 10));
    servo[3].SetPosition(100 + random(-10, 10));
    beep(500 + i * 100, 50);
    yield();
  }
  
  // Patlama
  sndLaser();
  jump(1);
  
  // Guc pozu
  servo[0].SetPosition(50);
  servo[1].SetPosition(130);
  servo[2].SetPosition(70);
  servo[3].SetPosition(110);
  delay(500);
  
  sndHappy();
  home();
  resetEyes();
}

// Zafer hareketi
void victoryMove() {
  showText("ZAFER!", "Kazandik!", "");
  eyeHappy();
  playVictory();
  
  // Zafer dansi
  jump(2);
  gangnam(2);
  
  // Kol kaldirma
  servo[2].SetPosition(50);
  servo[3].SetPosition(130);
  delay(300);
  
  for (int i = 0; i < 4; i++) {
    servo[0].SetPosition(60);
    servo[1]. SetPosition(120);
    delay(150);
    servo[0].SetPosition(120);
    servo[1]. SetPosition(60);
    delay(150);
    yield();
  }
  
  home();
  resetEyes();
}

// ═══════════════════════════════════════════════════════════
//                    WEB HANDLER'LAR
// ═══════════════════════════════════════════════════════════

void handleRoot() {
  server.send(200, "text/html", WEBPAGE);
}

void handleCmd() {
  String command = server.arg("q");
  String response = "OK";
  
  // Modlar
  if (command == "idle") { currentMode = MODE_IDLE; home(); resetEyes(); response = "Mod: Bekleme"; }
  else if (command == "auto") { currentMode = MODE_AUTO; response = "Mod: Otonom"; }
  else if (command == "dance") { currentMode = MODE_DANCE; response = "Mod: Dans"; }
  else if (command == "party") { currentMode = MODE_PARTY; response = "Mod: Parti"; }
  else if (command == "mdance") { currentMode = MODE_MUSIC_DANCE; response = "Mod: Muzikli Dans"; }
  else if (command == "sleep") { currentMode = MODE_SLEEP; eyeSleepy(); home(); response = "Mod: Uyku"; }
  else if (command == "demo") { demoMode(); response = "Demo bitti! "; }
  
  // Hareketler
  else if (command == "fwd") { eyeHappy(); walk(2,1); home(); resetEyes(); response = "Ileri"; }
  else if (command == "back") { eyeBlink(); walk(2,-1); home(); resetEyes(); response = "Geri"; }
  else if (command == "left") { eyeBlink(); turn(2,1); home(); resetEyes(); response = "Sol"; }
  else if (command == "right") { eyeBlink(); turn(2,-1); home(); resetEyes(); response = "Sag"; }
  else if (command == "stop") { home(); resetEyes(); response = "Durdu"; }
  else if (command == "jump") { eyeSurprised(); jump(2); home(); resetEyes(); response = "Zipladi"; }
  else if (command == "bend") { bend(1); response = "Egildi"; }
  else if (command == "sleft") { shakeLeg(0); response = "Sol salladi"; }
  else if (command == "sright") { shakeLeg(1); response = "Sag salladi"; }
  else if (command == "kleft") { kickLeft(); response = "Sol tekme"; }
  else if (command == "kright") { kickRight(); response = "Sag tekme"; }
  else if (command == "clap") { clap(3); response = "Alkisladi"; }
  
  // Danslar
  else if (command == "moon") { eyeHappy(); moonwalk(3); home(); resetEyes(); response = "Moonwalk"; }
  else if (command == "swing") { eyeHappy(); swing(3); home(); resetEyes(); response = "Swing"; }
  else if (command == "crus") { eyeHappy(); crusaito(3,1); home(); resetEyes(); response = "Crusaito"; }
  else if (command == "flap") { eyeHappy(); flapping(3); home(); resetEyes(); response = "Flapping"; }
  else if (command == "jit") { jitter(5); home(); response = "Jitter"; }
  else if (command == "tip") { eyeHappy(); tiptoe(3); home(); resetEyes(); response = "Tiptoe"; }
  else if (command == "updn") { upDown(4); home(); response = "Up-Down"; }
  else if (command == "shake") { shake(6); home(); response = "Shake"; }
  else if (command == "gang") { eyeHappy(); gangnam(3); home(); resetEyes(); response = "Gangnam! "; }
  else if (command == "dab") { dab(1); response = "Dab! "; }
  else if (command == "twist") { twist(3); response = "Twist"; }
  else if (command == "wave") { wave(4); response = "Wave"; }
  else if (command == "twerk") { twerk(5); response = "Twerk! "; }
  else if (command == "robot") { robot(2); response = "Robot"; }
  else if (command == "rdance") { randomDance(); response = "Rastgele dans! "; }
  
  // Cocuk Sarkilari
  else if (command == "danne") { danceAnneniSeviyorsan(); response = "Anneni Seviyorsan!"; }
  else if (command == "dhappy") { danceIfYoureHappy(); response = "If You're Happy! "; }
  else if (command == "dshark") { danceBabyShark(); response = "Baby Shark!"; }
  else if (command == "dhead") { danceHeadShoulders(); response = "Head Shoulders! "; }
  
  // Muzikli Danslar
  else if (command == "dmario") { danceMario(); response = "Mario Dance!"; }
  else if (command == "ddisco") { danceDisco(); response = "Disco!"; }
  else if (command == "dsalsa") { danceSalsa(); response = "Salsa! "; }
  else if (command == "drock") { danceRock(); response = "Rock!"; }
  else if (command == "dhiphop") { danceHipHop(); response = "Hip-Hop!"; }
  else if (command == "dstarw") { danceStarWars(); response = "Star Wars!"; }
  else if (command == "dtetris") { danceTetris(); response = "Tetris!"; }
  else if (command == "rmdance") { randomMusicDance(); response = "Rastgele muzikli! "; }
  
  // Gozler
  else if (command == "enorm") { resetEyes(); response = "Normal"; }
  else if (command == "ehappy") { eyeHappy(); resetEyes(); response = "Mutlu"; }
  else if (command == "esad") { eyeSad(); resetEyes(); response = "Uzgun"; }
  else if (command == "eangry") { eyeAngry(); resetEyes(); response = "Kizgin"; }
  else if (command == "esurp") { eyeSurprised(); response = "Saskin"; }
  else if (command == "elove") { eyeLove(); resetEyes(); response = "Asik"; }
  else if (command == "esleep") { eyeSleepy(); response = "Uykulu"; }
  else if (command == "ecool") { eyeCool(); resetEyes(); response = "Cool"; }
  else if (command == "eskull") { eyeSkull(); resetEyes(); response = "Iskelet"; }
  else if (command == "edizzy") { eyeDizzy(); resetEyes(); response = "Sersem"; }
  else if (command == "ewink") { eyeWink(); response = "Goz kirpti"; }
  else if (command == "ethink") { eyeThinking(); response = "Dusunuyor"; }
  else if (command == "edead") { eyeDead(); response = "Olu gozler"; }
  else if (command == "blink") { eyeBlink(); response = "Kirpti"; }
  else if (command == "eleft") { eyeLookLeft(); response = "Sola bakti"; }
  else if (command == "eright") { eyeLookRight(); response = "Saga bakti"; }
  
  // Sesler
  else if (command == "shappy") { sndHappy(); response = "Mutlu ses"; }
  else if (command == "ssad") { sndSad(); response = "Uzgun ses"; }
  else if (command == "sangry") { sndAngry(); response = "Kizgin ses"; }
  else if (command == "ssurp") { sndSurprise(); response = "Saskin ses"; }
  else if (command == "sfart") { sndFart(); response = "Prrrt!"; }
  else if (command == "scuddly") { sndCuddly(); response = "Tatli ses"; }
  else if (command == "spolice") { sndPolice(); response = "Polis! "; }
  else if (command == "sr2d2") { sndR2D2(); response = "R2D2!"; }
  else if (command == "slaser") { sndLaser(); response = "Pew pew!"; }
  else if (command == "sgameover") { sndGameOver(); response = "Game Over!"; }
  else if (command == "sdeath") { sndDeath(); response = "Olum sesi"; }
  else if (command == "sclap") { sndClap(); response = "Alkis sesi"; }
  
  // Melodiler
  else if (command == "mmario") { playMario(); response = "Mario! "; }
  else if (command == "mstar") { playStarWars(); response = "Star Wars!"; }
  else if (command == "mtetris") { playTetris(); response = "Tetris!"; }
  else if (command == "mpirate") { playPirates(); response = "Pirates!"; }
  else if (command == "mhappy") { playHappy(); response = "Happy!"; }
  else if (command == "mvic") { playVictory(); response = "Victory!"; }
  
  // Oyun Efektleri
  else if (command == "death") { deathMove(); response = "Oldu! "; }
  else if (command == "gameover") { gameOverMove(); response = "Game Over!"; }
  else if (command == "victory") { victoryMove(); response = "Zafer!"; }
  else if (command == "hit") { hitEffect(); response = "Vuruldu!"; }
  else if (command == "levelup") { levelUpEffect(); response = "Level Up!"; }
  else if (command == "powerup") { powerUpEffect(); response = "Power Up!"; }
  
  // Ozel Hareketler
  else if (command == "hello") { sayHello(); response = "Merhaba!"; }
  else if (command == "bye") { sayBye(); response = "Gule gule!"; }
  else if (command == "yes") { sayYes(); response = "Evet!"; }
  else if (command == "no") { sayNo(); response = "Hayir!"; }
  else if (command == "confused") { sayConfused(); response = "Anlamadim?"; }
  else if (command == "love") { sayLove(); response = "Seni seviyorum!"; }
  
  server.send(200, "text/plain", response);
}

// ═══════════════════════════════════════════════════════════
//                         SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== OTTO SUPER ROBOT v5.0 ===");
  
  // Servolar
  initServos();
  home();
  
  // Buzzer
  pinMode(PIN_BUZZER, OUTPUT);
  sndStart();
  
  // Ultrasonik
  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  
  // OLED
  Wire.begin(PIN_SDA, PIN_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED HATA!");
  }
  
  oled.clearDisplay();
  oled.setTextSize(1);
  oled. setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println("Otto Super Robot");
  oled. println("v5.0");
  oled.println("");
  oled.println("Yeni Ozellikler:");
  oled.println("- Cocuk Sarkilari");
  oled.println("- Game Over");
  oled.println("- Olum Hareketi");
  oled. display();
  delay(2000);
  
  // WiFi
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println("WiFi baglaniyor...");
  oled. println(WIFI_SSID);
  oled.display();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int connectionAttempt = 0;
  while (WiFi.status() != WL_CONNECTED && connectionAttempt < 30) {
    delay(500);
    Serial.print(".");
    connectionAttempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("WiFi Baglandi!");
    oled.println("");
    oled.println("IP Adresi:");
    oled.println(WiFi.localIP());
    oled.display();
    
    sndHappy();
  } else {
    Serial.println("\nAP Modu.. .");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("OttoRobot", "otto1234");
    
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("AP Modu Aktif");
    oled.println("");
    oled.println("SSID: OttoRobot");
    oled.println("Sifre: otto1234");
    oled.println("");
    oled.println("IP: 192.168.4.1");
    oled. display();
    
    sndSad();
  }
  
  delay(2500);
  
  // Web Server
  server.on("/", handleRoot);
  server.on("/c", handleCmd);
  server.begin();
  Serial.println("Web server hazir!");
  
  // Baslangic
  resetEyes();
  sndHappy();
}

// ═══════════════════════════════════════════════════════════
//                          LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  
  // Otonom mod
  if (currentMode == MODE_AUTO) {
    autonomousMode();
  }
  
  // Dans modu - her 5 saniyede rastgele dans
  if (currentMode == MODE_DANCE) {
    if (millis() - lastDance > 5000) {
      randomDance();
      lastDance = millis();
    }
  }
  
  // Parti modu - surekli parti! 
  if (currentMode == MODE_PARTY) {
    if (millis() - lastDance > 4000) {
      partyMode();
      lastDance = millis();
    }
  }
  
  // Muzikli Dans modu - her 8 saniyede muzikli dans
  if (currentMode == MODE_MUSIC_DANCE) {
    if (millis() - lastDance > 8000) {
      randomMusicDance();
      lastDance = millis();
    }
  }
  
  // Otomatik goz kirpma (uyku ve otonom harici)
  if (currentMode != MODE_SLEEP && currentMode != MODE_AUTO) {
    if (millis() - lastBlink > 5000) {
      eyeBlink();
      lastBlink = millis();
    }
  }
  
  yield();
}
