/*
 * ═══════════════════════════════════════════════════════════
 *              OTTO SUPER ROBOT v6.1 - ESP32-S3
 *      GitHub Token Desteği + Smooth Servo + Tüm Özellikler
 * ═══════════════════════════════════════════════════════════
 *  ÖNEMLİ: Bu token TEST İÇİNDİR. Kullanımdan sonra GitHub'dan iptal edin!
 *  Token: https://github.com/settings/tokens adresinden iptal edebilirsiniz.
 * ═══════════════════════════════════════════════════════════
 */

#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ═══════════════════ WIFI AYARLARI ═══════════════════
// AYAR 1: EV WIFI'SI (Internet için)
const char* STA_SSID = "3dPrinter";      // DEĞİŞTİR: Kendi WiFi adın
const char* STA_PASS = "muradaras1";    // DEĞİŞTİR: Kendi WiFi şifren

// AYAR 2: AP MODU (Internet yoksa)
#define AP_SSID "OttoRobot-AP"
#define AP_PASS "12345678"            // İsteğe bağlı şifre

// WiFi durumları için sabitler
#define WIFI_STATE_OFF 0
#define WIFI_STATE_STA 1
#define WIFI_STATE_AP 2
#define WIFI_STATE_STA_FAILED 3

// ═══════════════════ GITHUB AYARLARI ═══════════════════
// TEST TOKEN - TESTTEN SONRA HEMEN İPTAL EDİN!
// https://github.com/settings/tokens → "OttoRobot" → Revoke
#define GITHUB_TOKEN "Your token"

#define GITHUB_REPO "muradaras161616/otto-dances"
#define GITHUB_BRANCH "main"
#define GITHUB_API_URL "https://api.github.com/repos/" GITHUB_REPO "/contents/"
#define GITHUB_RAW_URL "https://raw.githubusercontent.com/" GITHUB_REPO "/" GITHUB_BRANCH "/"

// ═══════════════════ SMOOTH SERVO AYARLARI ═══════════════════
#define SERVO_DELAY 15     // Servo hareket hızı (ms) - 15ms yumuşak hareket
#define SMOOTH_STEPS 3    // Yumuşak geçiş adım sayısı

// ═══════════════════ PIN TANIMLARI ═══════════════════
#define PIN_YL 39       // Sol bacak yukarı
#define PIN_YR 38       // Sağ bacak yukarı
#define PIN_RL 37       // Sol bacak aşağı
#define PIN_RR 36       // Sağ bacak aşağı
#define PIN_BUZZER 7   
#define PIN_BUTTON 0    // BOOT Butonu (GPIO 0)

// TFT Ekran Pinleri
#define TFT_CS     10
#define TFT_DC     4
#define TFT_RST    5
#define TFT_SCK    12
#define TFT_MOSI   11

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

// ═══════════════════ SABİTLER ═══════════════════
#define SCREEN_W 160
#define SCREEN_H 128
#define OBSTACLE_DIST 25

// ═══════════════════ SMOOTH SERVO SINIFI ═══════════════════
class SmoothServo {
private:
  Servo servo;
  int pin;
  int currentPos;
  int targetPos;
  bool isMoving;
  bool attachedState;
  
public:
  SmoothServo() : currentPos(90), targetPos(90), isMoving(false), attachedState(false) {}
  
  void attach(int servoPin) {
    pin = servoPin;
    servo.attach(pin);
    servo.write(90);
    currentPos = 90;
    targetPos = 90;
    attachedState = true;
    delay(50);
  }
  
  void detach() {
    if (attachedState) {
      servo.detach();
      attachedState = false;
    }
    isMoving = false;
  }
  
  bool isAttached() {
    return attachedState;
  }
  
  void write(int pos) {
    if (!attachedState) attach(pin);
    targetPos = constrain(pos, 0, 180);
    if (!isMoving) {
      moveToTarget();
    }
  }
  
  void moveToTarget() {
    if (!attachedState || currentPos == targetPos) {
      isMoving = false;
      return;
    }
    
    isMoving = true;
    int step = (targetPos > currentPos) ? 1 : -1;
    int stepsNeeded = abs(targetPos - currentPos);
    int steps = min(stepsNeeded, SMOOTH_STEPS);
    
    for (int i = 0; i < steps; i++) {
      currentPos += step;
      servo.write(currentPos);
      delay(SERVO_DELAY);
    }
    
    isMoving = false;
  }
  
  bool isAtTarget() { 
    if (!attachedState) return true;
    return abs(currentPos - targetPos) <= 2; 
  }
  
  int getPosition() { return currentPos; }
  
  void forcePosition(int pos) {
    if (!attachedState) attach(pin);
    currentPos = constrain(pos, 0, 180);
    targetPos = currentPos;
    servo.write(currentPos);
    delay(SERVO_DELAY);
  }
};

// ═══════════════════ GLOBAL NESNELER ═══════════════════
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

// Smooth servo nesneleri
SmoothServo smoothYL, smoothYR, smoothRL, smoothRR;

// ═══════════════════ DURUM DEĞİŞKENLERİ ═══════════════════
enum Mode { MODE_IDLE, MODE_AUTO, MODE_DANCE, MODE_PARTY, MODE_SLEEP, MODE_MUSIC_DANCE, MODE_PERFORMING };
Mode currentMode = MODE_IDLE;

int currentWiFiState = WIFI_STATE_OFF;
bool hasInternet = false;
int moveSpeed = 1000;
unsigned long lastBlink = 0;
unsigned long lastDance = 0;
bool isMoving = false;

// Buton kontrol değişkenleri
unsigned long lastButtonPress = 0;
bool lastButtonState = false;

// GitHub için değişkenler
String danceFiles[50];
String musicFiles[50];
String eyeFiles[50];
String movementFiles[50];
String gameFiles[50];

int danceFileCount = 0;
int musicFileCount = 0;
int eyeFileCount = 0;
int movementFileCount = 0;
int gameFileCount = 0;

bool isDownloadingContent = false;
bool githubInitialized = false;
String lastGitHubError = "";
unsigned long lastGitHubFetch = 0;

struct Eye { int h, w, x, y; };
Eye eyeL, eyeR;
int eyeRadius = 10;

// ═══════════════════════════════════════════════════════════
//                    SMOOTH SERVO FONKSİYONLARI
// ═══════════════════════════════════════════════════════════

void smoothServoAttachAll() {
  smoothYL.attach(PIN_YL);
  smoothYR.attach(PIN_YR);
  smoothRL.attach(PIN_RL);
  smoothRR.attach(PIN_RR);
  delay(100);
}

void smoothServoDetachAll() {
  smoothYL.detach();
  smoothYR.detach();
  smoothRL.detach();
  smoothRR.detach();
  delay(50);
}

void setServosSmooth(int yl, int yr, int rl, int rr) {
  yl = constrain(yl, 0, 180);
  yr = constrain(yr, 0, 180);
  rl = constrain(rl, 0, 180);
  rr = constrain(rr, 0, 180);
  
  smoothYL.write(yl);
  smoothYR.write(yr);
  smoothRL.write(rl);
  smoothRR.write(rr);
  
  // Tüm servolar hedefe ulaşana kadar bekle (maksimum 1 saniye)
  unsigned long startTime = millis();
  while ((!smoothYL.isAtTarget() || !smoothYR.isAtTarget() || 
          !smoothRL.isAtTarget() || !smoothRR.isAtTarget()) && 
         (millis() - startTime < 1000)) {
    smoothYL.moveToTarget();
    smoothYR.moveToTarget();
    smoothRL.moveToTarget();
    smoothRR.moveToTarget();
    delay(1);
  }
}

void ottoHome() {
  // Yumuşak hareketle home pozisyonuna git
  setServosSmooth(90, 90, 90, 90);
  delay(100);
  
  // Motorları tamamen durdur (gücü kes)
  smoothServoDetachAll();
}

void servoEmergencyStop() {
  // Acil durdurma - hemen gücü kes
  smoothYL.detach();
  smoothYR.detach();
  smoothRL.detach();
  smoothRR.detach();
}

// ═══════════════════════════════════════════════════════════
//                    HAREKET FONKSİYONLARI
// ═══════════════════════════════════════════════════════════

void ottoWalk(int steps, int dir) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(90, 90, 70, 70); delay(200);
    setServosSmooth(120, 120, 70, 70); delay(200);
    setServosSmooth(120, 120, 110, 110); delay(200);
    setServosSmooth(60, 60, 110, 110); delay(200);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoTurn(int steps, int dir) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(90, 90, 70, 70); delay(200);
    setServosSmooth(120, 60, 70, 70); delay(200);
    setServosSmooth(120, 60, 110, 110); delay(200);
    setServosSmooth(90, 90, 110, 110); delay(200);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoJump(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(90, 90, 60, 120); delay(200);
    setServosSmooth(90, 90, 120, 60); delay(100);
    ottoHome(); delay(300);
    yield();
  }
  currentMode = MODE_IDLE;
}

void ottoShake(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(90, 90, 70, 70); delay(200);
    setServosSmooth(90, 90, 110, 110); delay(200);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoSwing(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(70, 110, 90, 90); delay(300);
    setServosSmooth(110, 70, 90, 90); delay(300);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoBend(int dir) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  setServosSmooth(90, 90, 90 + dir*40, 90 - dir*40);
  delay(400);
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoKickLeft() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  setServosSmooth(90, 90, 50, 50);
  delay(150);
  setServosSmooth(50, 90, 50, 50);
  delay(150);
  setServosSmooth(130, 90, 50, 50);
  delay(200);
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoKickRight() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  setServosSmooth(90, 90, 130, 130);
  delay(150);
  setServosSmooth(90, 130, 130, 130);
  delay(150);
  setServosSmooth(90, 50, 130, 130);
  delay(200);
  ottoHome();
  currentMode = MODE_IDLE;
}

// Alkış hareketi
void ottoClap(int times) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < times; i++) {
    setServosSmooth(50, 130, 90, 90);
    delay(150);
    setServosSmooth(90, 90, 90, 90);
    delay(150);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

// Ölüm hareketi
void ottoDeath() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  // Şaşırma
  eyeSurprised();
  delay(300);
  
  // Titreme
  for (int i = 0; i < 5; i++) {
    setServosSmooth(85, 95, 90, 90);
    delay(50);
    setServosSmooth(95, 85, 90, 90);
    delay(50);
  }
  
  // Yavaş düşme
  eyeDead();
  for (int angle = 90; angle <= 150; angle += 5) {
    setServosSmooth(90, 90, angle, 180 - angle);
    delay(50);
  }
  
  // Yerde yatma
  setServosSmooth(90, 90, 150, 30);
  delay(2000);
  
  // Canlanma
  setServosSmooth(90, 90, 90, 90);
  delay(200);
  ottoHome();
  resetEyes();
  currentMode = MODE_IDLE;
}

// ═══════════════════════════════════════════════════════════
//                      DANS FONKSİYONLARI
// ═══════════════════════════════════════════════════════════

void ottoMoonwalk(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(60, 120, 70, 70); delay(200);
    setServosSmooth(120, 60, 110, 110); delay(200);
    setServosSmooth(60, 120, 110, 110); delay(200);
    setServosSmooth(120, 60, 70, 70); delay(200);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoGangnam(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(70, 110, 110, 70); delay(200);
    ottoHome(); delay(100);
    setServosSmooth(110, 70, 70, 110); delay(200);
    ottoHome(); delay(100);
    yield();
  }
  currentMode = MODE_IDLE;
}

void ottoDab(int dir) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  setServosSmooth(90 + dir*35, 90 + dir*35, 90 + dir*45, 90 - dir*45);
  delay(500);
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoTwist(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(60, 120, 90, 90); delay(200);
    setServosSmooth(120, 60, 90, 90); delay(200);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoWave(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(60, 90, 90, 90); delay(150);
    setServosSmooth(120, 90, 90, 90); delay(150);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoTwerk(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(90, 90, 50, 130); delay(100);
    setServosSmooth(90, 90, 130, 50); delay(100);
    yield();
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void ottoRobot(int steps) {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  for (int i = 0; i < steps; i++) {
    setServosSmooth(70, 90, 90, 90); delay(200);
    setServosSmooth(90, 90, 90, 90); delay(100);
    setServosSmooth(90, 110, 90, 90); delay(200);
    setServosSmooth(90, 90, 90, 90); delay(100);
    setServosSmooth(90, 90, 70, 90); delay(200);
    setServosSmooth(90, 90, 90, 90); delay(100);
    setServosSmooth(90, 90, 90, 110); delay(200);
    setServosSmooth(90, 90, 90, 90); delay(100);
    yield();
  }
  currentMode = MODE_IDLE;
}

void randomDance() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  int d = random(0, 15);
  switch (d) {
    case 0: ottoMoonwalk(3); break;
    case 1: ottoSwing(3); break;
    case 2: ottoWalk(3, 1); break;
    case 3: ottoShake(5); break;
    case 4: ottoJump(2); break;
    case 5: ottoBend(1); break;
    case 6: ottoWalk(2, -1); break;
    case 7: ottoShake(6); break;
    case 8: ottoGangnam(3); break;
    case 9: ottoDab(random(0,2)==0 ? 1 : -1); break;
    case 10: ottoTwist(3); break;
    case 11: ottoWave(4); break;
    case 12: ottoTwerk(5); break;
    case 13: ottoRobot(2); break;
    case 14: ottoKickLeft(); ottoKickRight(); break;
  }
  ottoHome();
  currentMode = MODE_IDLE;
}

void partyMode() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  eyeHappy();
  randomDance();
  
  int r = random(0, 6);
  switch(r) {
    case 0: playMario(); break;
    case 1: sndR2D2(); break;
    case 2: sndHappy(); break;
    case 3: playVictory(); break;
    case 4: sndLaser(); break;
    case 5: beep(random(500,2000), 100); break;
  }
  
  resetEyes();
  currentMode = MODE_IDLE;
}

// ═══════════════════════════════════════════════════════════
//                   ŞARKILI DANS FONKSİYONLARI
// ═══════════════════════════════════════════════════════════

// ANNENİ SEVİYORSAN ALKIŞLA
void danceAnneniSeviyorsan() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  showText("Anneni Seviyorsan", "Alkışla!", "");
  eyeLove();
  delay(500);
  
  beep(NOTE_C5, 200); beep(NOTE_C5, 200); beep(NOTE_D5, 200); beep(NOTE_E5, 200);
  ottoClap(2);
  
  beep(NOTE_E5, 200); beep(NOTE_E5, 200); beep(NOTE_D5, 200); beep(NOTE_C5, 200);
  ottoClap(2);
  
  beep(NOTE_C5, 200); beep(NOTE_D5, 200); beep(NOTE_E5, 200); beep(NOTE_F5, 200);
  ottoGangnam(1);
  
  beep(NOTE_E5, 200); beep(NOTE_D5, 200); beep(NOTE_C5, 400);
  ottoClap(2);
  
  eyeHappy();
  beep(NOTE_C5, 200); beep(NOTE_C5, 200); beep(NOTE_D5, 200); beep(NOTE_E5, 200);
  ottoClap(2);
  
  beep(NOTE_E5, 200); beep(NOTE_E5, 200); beep(NOTE_D5, 200); beep(NOTE_C5, 200);
  ottoClap(2);
  
  eyeWink();
  sndHappy();
  ottoHome();
  resetEyes();
  currentMode = MODE_IDLE;
}

// IF YOU'RE HAPPY AND YOU KNOW IT
void danceIfYoureHappy() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  showText("If You're Happy", "Clap Your Hands!", "");
  eyeHappy();
  delay(500);
  
  beep(NOTE_C5, 250); beep(NOTE_C5, 250); 
  beep(NOTE_E5, 250); beep(NOTE_E5, 250);
  beep(NOTE_G5, 250); beep(NOTE_G5, 250);
  beep(NOTE_E5, 500);
  ottoClap(2);
  
  beep(NOTE_D5, 250); beep(NOTE_D5, 250);
  beep(NOTE_F5, 250); beep(NOTE_F5, 250);
  beep(NOTE_A5, 250); beep(NOTE_A5, 250);
  beep(NOTE_G5, 500);
  ottoClap(2);
  
  beep(NOTE_C5, 200); beep(NOTE_C5, 200);
  beep(NOTE_E5, 200); beep(NOTE_E5, 200);
  beep(NOTE_G5, 200); beep(NOTE_G5, 200);
  ottoGangnam(1);
  
  beep(NOTE_E5, 250); beep(NOTE_D5, 250);
  beep(NOTE_C5, 500);
  ottoClap(2);
  
  eyeWink();
  ottoHome();
  resetEyes();
  currentMode = MODE_IDLE;
}

// BABY SHARK
void danceBabyShark() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  showText("Baby Shark", "Doo doo doo!", "");
  eyeHappy();
  delay(500);
  
  for (int i = 0; i < 2; i++) {
    beep(NOTE_G4, 150);
    setServosSmooth(60, 90, 90, 90);
    delay(100);
    beep(NOTE_G4, 150);
    setServosSmooth(90, 90, 90, 90);
    delay(100);
    beep(NOTE_G4, 150);
    setServosSmooth(90, 120, 90, 90);
    delay(100);
    beep(NOTE_G4, 150);
    setServosSmooth(90, 90, 90, 90);
    delay(100);
    beep(NOTE_G4, 150);
    setServosSmooth(60, 90, 90, 90);
    delay(100);
    beep(NOTE_G4, 150);
    setServosSmooth(90, 90, 90, 90);
    delay(100);
    
    beep(NOTE_G4, 400);
    setServosSmooth(90, 90, 70, 110);
    delay(200);
    setServosSmooth(90, 90, 90, 90);
    delay(200);
    yield();
  }
  
  eyeLove();
  for (int i = 0; i < 2; i++) {
    beep(NOTE_A4, 150);
    ottoSwing(1);
    beep(NOTE_A4, 150);
    beep(NOTE_A4, 150);
    beep(NOTE_A4, 150);
    beep(NOTE_A4, 150);
    beep(NOTE_A4, 150);
    ottoClap(1);
    yield();
  }
  
  eyeSurprised();
  beep(NOTE_C5, 300);
  beep(NOTE_D5, 300);
  beep(NOTE_E5, 300);
  beep(NOTE_F5, 300);
  beep(NOTE_G5, 600);
  
  for (int i = 0; i < 3; i++) {
    ottoWalk(1, 1);
    yield();
  }
  
  sndHappy();
  ottoHome();
  resetEyes();
  currentMode = MODE_IDLE;
}

// HEAD SHOULDERS KNEES AND TOES
void danceHeadShoulders() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  showText("Head, Shoulders", "Knees and Toes!", "");
  eyeHappy();
  delay(500);
  
  beep(NOTE_C5, 300);
  setServosSmooth(90, 90, 60, 120);
  delay(300);
  
  beep(NOTE_D5, 300);
  setServosSmooth(60, 120, 90, 90);
  delay(300);
  setServosSmooth(90, 90, 90, 90);
  
  beep(NOTE_E5, 300);
  setServosSmooth(90, 90, 120, 60);
  delay(300);
  
  beep(NOTE_F5, 300);
  setServosSmooth(90, 90, 140, 40);
  delay(300);
  
  beep(NOTE_E5, 300);
  setServosSmooth(90, 90, 120, 60);
  delay(200);
  beep(NOTE_F5, 400);
  setServosSmooth(90, 90, 140, 40);
  delay(400);
  
  ottoHome();
  
  beep(NOTE_C5, 200); ottoBend(1);
  beep(NOTE_D5, 200); ottoTwist(1);
  beep(NOTE_E5, 200); ottoShake(2);
  beep(NOTE_F5, 200); ottoJump(1);
  beep(NOTE_E5, 200); beep(NOTE_F5, 300);
  ottoShake(2);
  
  sndHappy();
  ottoHome();
  resetEyes();
  currentMode = MODE_IDLE;
}

// Müzikli danslar - diğerleri
void danceMario() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  eyeHappy();
  beep(NOTE_E5, 150); ottoJump(1);
  beep(NOTE_E5, 150); ottoJump(1);
  beep(0, 150);
  beep(NOTE_E5, 150); ottoShake(2);
  beep(0, 150);
  beep(NOTE_C5, 150); ottoTwist(1);
  beep(NOTE_E5, 150); ottoMoonwalk(1);
  beep(NOTE_G5, 400); ottoGangnam(1);
  ottoHome();
  resetEyes();
  currentMode = MODE_IDLE;
}

void danceDisco() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  eyeLove();
  for (int i = 0; i < 4; i++) {
    beep(NOTE_C5, 100);
    setServosSmooth(60, 120, 70, 110);
    delay(150);
    beep(NOTE_G4, 100);
    setServosSmooth(120, 60, 110, 70);
    delay(150);
    beep(NOTE_E5, 100);
    ottoDab(1);
    beep(NOTE_D5, 100);
    ottoDab(-1);
    yield();
  }
  ottoHome();
  resetEyes();
  currentMode = MODE_IDLE;
}

void randomMusicDance() {
  if (currentMode == MODE_PERFORMING) return;
  currentMode = MODE_PERFORMING;
  smoothServoAttachAll();
  
  int d = random(0, 12);
  switch (d) {
    case 0: danceMario(); break;
    case 1: danceDisco(); break;
    case 2: ottoSwing(3); break;
    case 3: ottoShake(4); break;
    case 4: ottoWave(3); break;
    case 5: ottoMoonwalk(2); break;
    case 6: danceAnneniSeviyorsan(); break;
    case 7: danceIfYoureHappy(); break;
    case 8: danceBabyShark(); break;
    case 9: danceHeadShoulders(); break;
    case 10: playVictory(); ottoGangnam(2); break;
    case 11: sndHappy(); randomDance(); break;
  }
  currentMode = MODE_IDLE;
}

// ═══════════════════════════════════════════════════════════
//                      SES FONKSİYONLARI
// ═══════════════════════════════════════════════════════════

void beep(int freq, int dur) {
  if (freq > 0) {
    ledcAttach(PIN_BUZZER, freq, 8);
    ledcWrite(PIN_BUZZER, 128);
    delay(dur);
    ledcWrite(PIN_BUZZER, 0);
    ledcDetach(PIN_BUZZER);
  } else {
    delay(dur);
  }
}

void sndHappy() { beep(NOTE_C5,100); beep(NOTE_E5,100); beep(NOTE_G5,150); }
void sndSad() { beep(NOTE_G4,200); beep(NOTE_E4,300); }
void sndAngry() { beep(200,100); beep(200,100); beep(200,100); }
void sndSurprise() { for(int i=300; i<1200; i+=100) beep(i,20); }
void sndCuddly() { beep(NOTE_C5,100); beep(NOTE_E5,100); beep(NOTE_G5,100); beep(NOTE_C6,200); }
void sndFart() { for(int i=50; i<300; i+=10) { beep(i,10); delay(10); } }
void sndStart() { beep(NOTE_C5,100); beep(NOTE_G5,150); }
void sndR2D2() { beep(2000,50); beep(1500,50); beep(2500,50); beep(1000,100); beep(2000,50); }
void sndLaser() { for(int i=2000; i>200; i-=50) { beep(i,5); delay(5); } }
void sndClap() { for(int i=0; i<3; i++) { beep(1500,30); delay(70); } }

void sndPolice() {
  for(int j=0; j<3; j++) {
    for(int i=800; i<1200; i+=50) { beep(i,30); delay(30); }
    for(int i=1200; i>800; i-=50) { beep(i,30); delay(30); }
    yield();
  }
}

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
  for(int i=400; i>100; i-=20) { beep(i,30); delay(30); }
}

void sndDeath() {
  for(int i=1000; i>100; i-=30) { beep(i,20); delay(25); }
  delay(200);
  beep(100, 500);
}

void sndWiFiConnected() { beep(800,100); beep(1200,100); beep(1600,150); }
void sndWiFiFailed() { beep(400,200); beep(300,200); }
void sndAPMode() { beep(600,100); delay(100); beep(600,100); }

void playMario() {
  int mel[] = {NOTE_E5,NOTE_E5,0,NOTE_E5,0,NOTE_C5,NOTE_E5,NOTE_G5};
  int dur[] = {150,150,150,150,150,150,150,400};
  for(int i=0; i<8; i++) { beep(mel[i],dur[i]); delay(30); yield(); }
}

void playStarWars() {
  int mel[] = {NOTE_A4,NOTE_A4,NOTE_A4,NOTE_F4,NOTE_C5,NOTE_A4,NOTE_F4,NOTE_C5,NOTE_A4};
  int dur[] = {500,500,500,350,150,500,350,150,1000};
  for(int i=0; i<9; i++) { beep(mel[i],dur[i]); delay(50); yield(); }
}

void playTetris() {
  int mel[] = {NOTE_E5,NOTE_B4,NOTE_C5,NOTE_D5,NOTE_C5,NOTE_B4,NOTE_A4};
  int dur[] = {400,200,200,400,200,200,400};
  for(int i=0; i<7; i++) { beep(mel[i],dur[i]); delay(50); yield(); }
}

void playPirates() {
  int mel[] = {NOTE_E4,NOTE_G4,NOTE_A4,NOTE_A4,0,NOTE_A4,NOTE_B4,NOTE_C5};
  int dur[] = {150,150,300,150,150,150,150,300};
  for(int i=0; i<8; i++) { beep(mel[i],dur[i]); delay(30); yield(); }
}

void playHappy() {
  int mel[] = {NOTE_G4,NOTE_G4,NOTE_A4,NOTE_G4,NOTE_C5,NOTE_B4};
  int dur[] = {200,200,400,400,400,800};
  for(int i=0; i<6; i++) { beep(mel[i],dur[i]); delay(50); yield(); }
}

void playVictory() {
  beep(NOTE_C5,150); beep(NOTE_C5,150); beep(NOTE_C5,150); beep(NOTE_C5,400);
  beep(NOTE_G4,400); beep(NOTE_A4,400); beep(NOTE_C5,200); beep(NOTE_A4,100); beep(NOTE_C5,500);
}

// ═══════════════════════════════════════════════════════════
//                       GÖZ FONKSİYONLARI
// ═══════════════════════════════════════════════════════════

void drawEyes() {
  tft.fillScreen(ST77XX_BLACK);
  
  int xL = eyeL.x - eyeL.w/2;
  int yL = eyeL.y - eyeL.h/2;
  tft.fillRoundRect(xL, yL, eyeL.w, eyeL.h, eyeRadius, ST77XX_WHITE);
  
  int xR = eyeR.x - eyeR.w/2;
  int yR = eyeR.y - eyeR.h/2;
  tft.fillRoundRect(xR, yR, eyeR.w, eyeR.h, eyeRadius, ST77XX_WHITE);
}

void resetEyes() {
  eyeL.h = eyeR.h = 40;
  eyeL.w = eyeR.w = 40;
  eyeL.x = 50;
  eyeL.y = 64;
  eyeR.x = 110;
  eyeR.y = 64;
  eyeRadius = 10;
  drawEyes();
}

void eyeBlink() {
  for (int i = 0; i < 3; i++) {
    eyeL.h -= 12; eyeR.h -= 12;
    eyeL.w += 3; eyeR.w += 3;
    eyeRadius = max(1, eyeL.h/4);
    drawEyes(); delay(20);
  }
  for (int i = 0; i < 3; i++) {
    eyeL.h += 12; eyeR.h += 12;
    eyeL.w -= 3; eyeR.w -= 3;
    eyeRadius = max(1, eyeL.h/4);
    drawEyes(); delay(20);
  }
  resetEyes();
}

void eyeHappy() {
  resetEyes();
  delay(100);
  tft.fillTriangle(eyeL.x-20, eyeL.y+15, eyeL.x+20, eyeL.y+15, eyeL.x, eyeL.y+35, ST77XX_BLACK);
  tft.fillTriangle(eyeR.x-20, eyeR.y+15, eyeR.x+20, eyeR.y+15, eyeR.x, eyeR.y+35, ST77XX_BLACK);
  delay(800);
  resetEyes();
}

void eyeSad() {
  resetEyes();
  delay(100);
  tft.fillTriangle(eyeL.x-20, eyeL.y-20, eyeL.x+20, eyeL.y-20, eyeL.x, eyeL.y-5, ST77XX_BLACK);
  tft.fillTriangle(eyeR.x-20, eyeR.y-20, eyeR.x+20, eyeR.y-20, eyeR.x, eyeR.y-5, ST77XX_BLACK);
  delay(800);
  resetEyes();
}

void eyeAngry() {
  resetEyes();
  tft.fillTriangle(eyeL.x-20, eyeL.y-25, eyeL.x+20, eyeL.y-15, eyeL.x+20, eyeL.y-25, ST77XX_BLACK);
  tft.fillTriangle(eyeR.x+20, eyeR.y-25, eyeR.x-20, eyeR.y-15, eyeR.x-20, eyeR.y-25, ST77XX_BLACK);
  delay(500);
  resetEyes();
}

void eyeSurprised() {
  for (int i = 0; i < 8; i++) {
    eyeL.h += 3; eyeL.w += 3;
    eyeR.h += 3; eyeR.w += 3;
    eyeRadius += 2;
    drawEyes(); delay(30);
  }
  delay(400);
  resetEyes();
}

void eyeLove() {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillCircle(30, 26, 12, ST77XX_WHITE);
  tft.fillCircle(48, 26, 12, ST77XX_WHITE);
  tft.fillTriangle(18, 32, 60, 32, 39, 55, ST77XX_WHITE);
  tft.fillCircle(80, 26, 12, ST77XX_WHITE);
  tft.fillCircle(98, 26, 12, ST77XX_WHITE);
  tft.fillTriangle(68, 32, 110, 32, 89, 55, ST77XX_WHITE);
  delay(1000);
}

void eyeSleepy() {
  eyeL.h = eyeR.h = 4;
  eyeRadius = 0;
  drawEyes();
}

void eyeCool() {
  resetEyes();
  tft.fillRect(0, 0, 160, 27, ST77XX_BLACK);
  tft.fillRect(54, 29, 20, 6, ST77XX_WHITE);
  delay(500);
  resetEyes();
}

void eyeSkull() {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRoundRect(24, 5, 80, 50, 20, ST77XX_WHITE);
  tft.fillCircle(44, 28, 10, ST77XX_BLACK);
  tft.fillCircle(84, 28, 10, ST77XX_BLACK);
  tft.fillTriangle(64, 35, 58, 48, 70, 48, ST77XX_BLACK);
  for (int i = 0; i < 5; i++) tft.fillRect(36+i*12, 55, 8, 8, ST77XX_WHITE);
  delay(500);
  resetEyes();
}

void eyeWink() {
  resetEyes();
  for (int i = 0; i < 5; i++) { eyeR.h -= 8; drawEyes(); delay(30); }
  delay(200);
  for (int i = 0; i < 5; i++) { eyeR.h += 8; drawEyes(); delay(30); }
  resetEyes();
}

void eyeLookLeft() {
  for (int i = 0; i < 8; i++) { eyeL.x -= 2; eyeR.x -= 2; drawEyes(); delay(30); }
  delay(300);
  resetEyes();
}

void eyeLookRight() {
  for (int i = 0; i < 8; i++) { eyeL.x += 2; eyeR.x += 2; drawEyes(); delay(30); }
  delay(300);
  resetEyes();
}

void eyeThinking() {
  for (int i = 0; i < 8; i++) {
    eyeL.x--; eyeL.y--;
    eyeR.x--; eyeR.y--;
    drawEyes(); delay(50);
  }
  delay(800);
  resetEyes();
}

// X gözler (ölü)
void eyeDead() {
  tft.fillScreen(ST77XX_BLACK);
  // Sol X
  tft.drawLine(eyeL.x-15, eyeL.y-15, eyeL.x+15, eyeL.y+15, ST77XX_WHITE);
  tft.drawLine(eyeL.x+15, eyeL.y-15, eyeL.x-15, eyeL.y+15, ST77XX_WHITE);
  tft.drawLine(eyeL.x-14, eyeL.y-15, eyeL.x+16, eyeL.y+15, ST77XX_WHITE);
  tft.drawLine(eyeL.x+16, eyeL.y-15, eyeL.x-14, eyeL.y+15, ST77XX_WHITE);
  // Sağ X
  tft.drawLine(eyeR.x-15, eyeR.y-15, eyeR.x+15, eyeR.y+15, ST77XX_WHITE);
  tft.drawLine(eyeR.x+15, eyeR.y-15, eyeR.x-15, eyeR.y+15, ST77XX_WHITE);
  tft.drawLine(eyeR.x-14, eyeR.y-15, eyeR.x+16, eyeR.y+15, ST77XX_WHITE);
  tft.drawLine(eyeR.x+16, eyeR.y-15, eyeR.x-14, eyeR.y+15, ST77XX_WHITE);
}

// Game Over ekranı
void showGameOver() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 40);
  tft.println("GAME");
  tft.setCursor(30, 70);
  tft.println("OVER!");
}

void showText(const char* l1, const char* l2, const char* l3) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 40);
  tft.println(l1);
  if (strlen(l2) > 0) { tft.setCursor(10, 60); tft.println(l2); }
  if (strlen(l3) > 0) { tft.setCursor(10, 80); tft.println(l3); }
}

void showBootScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(30, 40);
  tft.println("OTTO");
  tft.setTextSize(1);
  tft.setCursor(20, 70);
  tft.println("WiFi Bağlanıyor...");
}

void showWiFiStatus() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  
  if (currentWiFiState == WIFI_STATE_STA && hasInternet) {
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(10, 20);
    tft.println("WiFi: BAĞLANDI");
    tft.setCursor(10, 35);
    tft.print("SSID: ");
    tft.println(STA_SSID);
    tft.setCursor(10, 50);
    tft.print("IP: ");
    tft.println(WiFi.localIP());
    tft.setCursor(10, 65);
    tft.println("Internet: VAR");
    tft.setCursor(10, 80);
    tft.print("GitHub: ");
    tft.println(githubInitialized ? "Hazır" : "Hazır Değil");
    tft.setCursor(10, 95);
    tft.print("Dans: ");
    tft.print(danceFileCount);
    tft.println(" adet");
  }
  else if (currentWiFiState == WIFI_STATE_STA && !hasInternet) {
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(10, 20);
    tft.println("WiFi: BAĞLANDI");
    tft.setCursor(10, 35);
    tft.print("SSID: ");
    tft.println(STA_SSID);
    tft.setCursor(10, 50);
    tft.println("Internet: YOK");
    tft.setCursor(10, 65);
    tft.println("GitHub çalışmaz");
  }
  else if (currentWiFiState == WIFI_STATE_AP) {
    tft.setTextColor(ST77XX_ORANGE);
    tft.setCursor(10, 20);
    tft.println("AP MODU");
    tft.setCursor(10, 35);
    tft.print("SSID: ");
    tft.println(AP_SSID);
    tft.setCursor(10, 50);
    tft.print("IP: ");
    tft.println(WiFi.softAPIP());
    tft.setCursor(10, 65);
    tft.print("Şifre: ");
    tft.println(AP_PASS);
    tft.setCursor(10, 80);
    tft.println("Internet: YOK");
  }
  
  tft.setCursor(10, 110);
  tft.println("Web: 192.168.4.1");
}

// ═══════════════════════════════════════════════════════════
//                 GITHUB TOKEN TEST FONKSİYONU
// ═══════════════════════════════════════════════════════════

void checkGitHubToken() {
  Serial.println("\n=== GITHUB TOKEN TESTİ ===");
  Serial.print("Token uzunluğu: ");
  Serial.println(strlen(GITHUB_TOKEN));
  
  // Token'ın ilk 4 karakterini kontrol et (ghp_ ile başlamalı)
  if (strncmp(GITHUB_TOKEN, "ghp_", 4) == 0) {
    Serial.println("✅ Token formatı doğru (ghp_)");
  } else {
    Serial.println("⚠️  Token formatı farklı");
    return;
  }
  
  // TEST: Token ile basit bir API çağrısı
  HTTPClient http;
  http.begin("https://api.github.com/user");
  http.addHeader("User-Agent", "ESP32-Otto-Test");
  http.addHeader("Authorization", "Bearer " GITHUB_TOKEN);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    Serial.println("✅ Token geçerli!");
    
    // Kullanıcı bilgilerini al
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      Serial.print("GitHub Kullanıcı: ");
      Serial.println(doc["login"].as<String>());
      Serial.print("Repo sayısı: ");
      Serial.println(doc["public_repos"].as<int>());
    }
    
  } else if (httpCode == 401) {
    Serial.println("❌ Token geçersiz!");
    lastGitHubError = "Token geçersiz";
  } else if (httpCode == 403) {
    Serial.println("⚠️  Rate limit veya yetki sorunu");
    lastGitHubError = "Rate limit";
  } else {
    Serial.print("HTTP Hatası: ");
    Serial.println(httpCode);
    lastGitHubError = "HTTP: " + String(httpCode);
  }
  
  http.end();
}

// ═══════════════════════════════════════════════════════════
//                    GITHUB TÜM KLASÖRLERİ ÇEKME
// ═══════════════════════════════════════════════════════════

bool fetchGitHubFolderContents(const char* folder, String fileList[], int &fileCount, const char* extension = ".json") {
  if (isDownloadingContent || !hasInternet) {
    Serial.println("Internet yok veya zaten indiriliyor");
    return false;
  }
  
  isDownloadingContent = true;
  Serial.print("GitHub klasörü çekiliyor: ");
  Serial.println(folder);
  
  HTTPClient http;
  String url = String(GITHUB_API_URL) + folder;
  
  http.begin(url);
  http.addHeader("User-Agent", "ESP32-Otto-Robot");
  http.addHeader("Authorization", "Bearer " GITHUB_TOKEN);
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  String payload = "";
  
  if (httpCode > 0) {
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);
    
    if (httpCode == 200) {
      payload = http.getString();
    } else if (httpCode == 403) {
      lastGitHubError = "Rate limit exceeded";
      Serial.println("GitHub Rate Limit!");
    } else if (httpCode == 404) {
      lastGitHubError = "Folder not found";
      Serial.println("Klasör bulunamadı!");
    } else {
      lastGitHubError = "HTTP Error: " + String(httpCode);
    }
  } else {
    lastGitHubError = "Connection failed";
    Serial.println("Bağlantı hatası!");
  }
  
  if (httpCode == 200 && payload.length() > 0) {
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      fileCount = 0;
      
      for (JsonObject file : doc.as<JsonArray>()) {
        String fileName = file["name"].as<String>();
        String type = file["type"].as<String>();
        
        if (type == "file" && fileName.endsWith(extension)) {
          String name = fileName.substring(0, fileName.length() - strlen(extension));
          fileList[fileCount] = name;
          fileCount++;
          
          Serial.print("  - ");
          Serial.println(name);
          
          if (fileCount >= 50) break;
        }
      }
      
      // Alfabetik sırala
      for (int i = 0; i < fileCount - 1; i++) {
        for (int j = i + 1; j < fileCount; j++) {
          if (fileList[i] > fileList[j]) {
            String temp = fileList[i];
            fileList[i] = fileList[j];
            fileList[j] = temp;
          }
        }
      }
      
      Serial.print(folder);
      Serial.print(" klasöründen ");
      Serial.print(fileCount);
      Serial.println(" dosya yüklendi");
      
      http.end();
      isDownloadingContent = false;
      return true;
    } else {
      Serial.print("JSON parse hatası: ");
      Serial.println(error.c_str());
      lastGitHubError = "JSON Parse Error";
    }
  }
  
  http.end();
  isDownloadingContent = false;
  return false;
}

// Tüm GitHub içeriğini çek
bool fetchAllGitHubContent() {
  if (!hasInternet) {
    Serial.println("Internet yok, GitHub erişilemez");
    return false;
  }
  
  Serial.println("GitHub'dan tüm içerik çekiliyor...");
  lastGitHubFetch = millis();
  
  bool success = true;
  
  // Danslar
  if (fetchGitHubFolderContents("dances", danceFiles, danceFileCount, ".json")) {
    Serial.print("Dans dosyaları: ");
    Serial.println(danceFileCount);
  } else {
    success = false;
    Serial.println("Dans dosyaları çekilemedi");
  }
  
  delay(500);
  
  // Müzikler
  if (fetchGitHubFolderContents("music", musicFiles, musicFileCount, ".json")) {
    Serial.print("Müzik dosyaları: ");
    Serial.println(musicFileCount);
  } else {
    success = false;
    Serial.println("Müzik dosyaları çekilemedi");
  }
  
  delay(500);
  
  // Göz ifadeleri
  if (fetchGitHubFolderContents("eyes", eyeFiles, eyeFileCount, ".json")) {
    Serial.print("Göz ifadesi dosyaları: ");
    Serial.println(eyeFileCount);
  } else {
    success = false;
    Serial.println("Göz dosyaları çekilemedi");
  }
  
  delay(500);
  
  // Hareketler
  if (fetchGitHubFolderContents("movements", movementFiles, movementFileCount, ".json")) {
    Serial.print("Hareket dosyaları: ");
    Serial.println(movementFileCount);
  } else {
    success = false;
    Serial.println("Hareket dosyaları çekilemedi");
  }
  
  delay(500);
  
  // Oyunlar
  if (fetchGitHubFolderContents("games", gameFiles, gameFileCount, ".json")) {
    Serial.print("Oyun dosyaları: ");
    Serial.println(gameFileCount);
  } else {
    success = false;
    Serial.println("Oyun dosyaları çekilemedi");
  }
  
  githubInitialized = success;
  return success;
}

// GitHub dosyasını oynat
bool playGitHubFile(const char* folder, const char* filename, const char* extension = ".json") {
  if (!hasInternet || currentMode == MODE_PERFORMING || isDownloadingContent) {
    Serial.println("GitHub dosyası oynatılamıyor");
    return false;
  }
  
  currentMode = MODE_PERFORMING;
  Serial.print("GitHub dosyası oynatılıyor: ");
  Serial.print(folder);
  Serial.print("/");
  Serial.println(filename);
  
  HTTPClient http;
  String url = String(GITHUB_RAW_URL) + folder + "/" + filename + extension;
  
  http.begin(url);
  http.addHeader("User-Agent", "ESP32-Otto-Robot");
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      String type = doc["type"] | "dance";
      String name = doc["name"] | "Unknown";
      
      showText("GitHub", name.c_str(), folder);
      
      if (type == "dance") {
        smoothServoAttachAll();
        JsonArray steps = doc["steps"];
        
        for (JsonObject step : steps) {
          int duration = step["duration"] | 200;
          int yl = step["yl"] | 90;
          int yr = step["yr"] | 90;
          int rl = step["rl"] | 90;
          int rr = step["rr"] | 90;
          
          setServosSmooth(yl, yr, rl, rr);
          
          if (step.containsKey("tone")) {
            int freq = step["freq"] | 0;
            int toneDur = step["tone_duration"] | 100;
            if (freq > 0) beep(freq, toneDur);
          } else {
            delay(duration);
          }
          
          yield();
        }
        
      } else if (type == "music") {
        JsonArray notes = doc["notes"];
        
        for (JsonObject note : notes) {
          int freq = note["freq"] | 0;
          int duration = note["duration"] | 200;
          
          if (freq > 0) beep(freq, duration);
          else delay(duration);
          
          yield();
        }
        
      } else if (type == "eye") {
        String expression = doc["expression"] | "normal";
        
        if (expression == "happy") eyeHappy();
        else if (expression == "sad") eyeSad();
        else if (expression == "angry") eyeAngry();
        else if (expression == "surprised") eyeSurprised();
        else if (expression == "love") eyeLove();
        else if (expression == "sleep") eyeSleepy();
        else if (expression == "cool") eyeCool();
        else if (expression == "wink") eyeWink();
        else resetEyes();
        
        delay(doc["duration"] | 1000);
        resetEyes();
        
      } else if (type == "movement") {
        smoothServoAttachAll();
        JsonArray moves = doc["moves"];
        
        for (JsonObject move : moves) {
          String moveType = move["type"] | "walk";
          int count = move["count"] | 1;
          int speed = move["speed"] | 200;
          
          if (moveType == "walk") ottoWalk(count, 1);
          else if (moveType == "jump") ottoJump(count);
          else if (moveType == "shake") ottoShake(count);
          else if (moveType == "swing") ottoSwing(count);
          else if (moveType == "clap") ottoClap(count);
          else if (moveType == "turn") ottoTurn(count, 1);
          
          delay(speed);
          yield();
        }
      }
      
      ottoHome();
      http.end();
      currentMode = MODE_IDLE;
      Serial.println("GitHub dosyası başarıyla oynatıldı");
      return true;
    } else {
      Serial.print("JSON parse hatası: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("HTTP hatası: ");
    Serial.println(httpCode);
  }
  
  http.end();
  currentMode = MODE_IDLE;
  return false;
}

// ═══════════════════════════════════════════════════════════
//                    AKILLI WIFI YÖNETİMİ
// ═══════════════════════════════════════════════════════════

bool connectToWiFi() {
  Serial.println("WiFi'ye bağlanılıyor...");
  Serial.print("SSID: ");
  Serial.println(STA_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    currentWiFiState = WIFI_STATE_STA;
    
    Serial.println("\nWiFi bağlandı!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    
    // İnternet bağlantısını test et
    if (testInternetConnection()) {
      Serial.println("İnternet bağlantısı var!");
      hasInternet = true;
      return true;
    } else {
      Serial.println("WiFi var ama internet yok!");
      hasInternet = false;
      return true;
    }
  }
  
  Serial.println("\nWiFi bağlantısı başarısız!");
  currentWiFiState = WIFI_STATE_STA_FAILED;
  hasInternet = false;
  return false;
}

bool testInternetConnection() {
  HTTPClient http;
  
  // GitHub API'yi test et
  http.begin("https://api.github.com");
  http.setTimeout(5000);
  http.addHeader("User-Agent", "ESP32-Otto-Test");
  
  int httpCode = http.GET();
  http.end();
  
  if (httpCode == 200 || httpCode == 403) {
    Serial.println("Internet bağlantısı: VAR");
    return true;
  }
  
  // Alternatif test
  http.begin("http://connectivitycheck.gstatic.com/generate_204");
  http.setTimeout(3000);
  httpCode = http.GET();
  http.end();
  
  Serial.print("Internet test HTTP: ");
  Serial.println(httpCode);
  
  return (httpCode == 204);
}

void startAPMode() {
  Serial.println("AP Modu başlatılıyor...");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  
  currentWiFiState = WIFI_STATE_AP;
  hasInternet = false;
  
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("AP Şifre: ");
  Serial.println(AP_PASS);
}

// ═══════════════════════════════════════════════════════════
//                        DEMO MODU
// ═══════════════════════════════════════════════════════════

void demoMode() {
  showText("DEMO MODE", "Başlıyor...", "");
  delay(800);
  
  eyeHappy(); sndHappy();
  resetEyes(); delay(300);
  
  showText("Yürüme", "", "");
  ottoWalk(3, 1);
  ottoWalk(2, -1);
  ottoHome();
  
  showText("Zıplama", "", "");
  eyeSurprised();
  ottoJump(2);
  ottoHome();
  
  showText("Dans: Gangnam", "", "");
  eyeHappy();
  ottoGangnam(2);
  ottoHome();
  
  showText("Şarkılı Dans:", "Anneni Seviyorsan", "");
  danceAnneniSeviyorsan();
  
  showText("Ölüm Hareketi", "", "");
  ottoDeath();
  
  showText("Game Over!", "", "");
  showGameOver();
  sndGameOver();
  delay(2000);
  
  resetEyes();
  ottoHome();
  playVictory();
  
  showText("DEMO BİTTİ!", "Otto hazır!", "");
  delay(2000);
  resetEyes();
}

// ═══════════════════════════════════════════════════════════
//                      BUTON YÖNETİMİ
// ═══════════════════════════════════════════════════════════

void handleButton() {
  // GPIO 0 için LOW = Basıldı (pull-up)
  bool btnState = !digitalRead(PIN_BUTTON);
  
  if (btnState && !lastButtonState) {
    if (millis() - lastButtonPress > 500) {
      lastButtonPress = millis();
      
      switch (currentMode) {
        case MODE_IDLE:
          currentMode = MODE_AUTO;
          eyeHappy();
          showText("Mod:", "Otonom", "");
          sndHappy();
          break;
          
        case MODE_AUTO:
          currentMode = MODE_DANCE;
          ottoHome();
          eyeCool();
          showText("Mod:", "Dans", "");
          sndHappy();
          break;
          
        case MODE_DANCE:
          currentMode = MODE_PARTY;
          ottoHome();
          eyeHappy();
          showText("Mod:", "Parti", "");
          sndHappy();
          break;
          
        case MODE_PARTY:
          currentMode = MODE_SLEEP;
          ottoHome();
          eyeSleepy();
          showText("Mod:", "Uyku", "");
          break;
          
        default:
          currentMode = MODE_IDLE;
          ottoHome();
          resetEyes();
          showText("Mod:", "Bekleme", "");
          sndStart();
          break;
      }
      delay(500);
    }
  }
  lastButtonState = btnState;
}

// ═══════════════════════════════════════════════════════════
//                      WEB ARAYÜZÜ
// ═══════════════════════════════════════════════════════════

String generateMainPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Otto Robot v6.1</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial;background:linear-gradient(135deg,#1a1a2e,#16213e);min-height:100vh;color:#fff;padding:10px}
.hdr{text-align:center;padding:15px;background:rgba(255,255,255,.1);border-radius:15px;margin-bottom:15px}
h1{font-size:22px}
.status{font-size:12px;opacity:.7;margin-top:8px}
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
.tab{padding:10px 15px;background:rgba(255,255,255,.1);border:none;border-radius:8px;color:#fff;cursor:pointer;font-size:12px}
.tab.active,.tab:hover{background:linear-gradient(45deg,#00d2ff,#3a7bd5)}
.panel{display:none;background:rgba(255,255,255,.05);border-radius:15px;padding:15px}
.panel.active{display:block}
.sec{margin-bottom:18px}
.sec h3{color:#00d2ff;font-size:15px;margin-bottom:10px;border-bottom:1px solid rgba(0,210,255,.3);padding-bottom:8px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(75px,1fr));gap:8px}
.btn{padding:12px 8px;border:none;border-radius:10px;font-size:11px;font-weight:700;cursor:pointer;color:#fff}
.btn:active{transform:scale(.95)}
.g{background:linear-gradient(45deg,#11998e,#38ef7d)}
.b{background:linear-gradient(45deg,#2196F3,#21CBF3)}
.o{background:linear-gradient(45deg,#f46b45,#eea849)}
.p{background:linear-gradient(45deg,#8E2DE2,#4A00E0)}
.pk{background:linear-gradient(45deg,#ee0979,#ff6a00)}
.r{background:linear-gradient(45deg,#cb2d3e,#ef473a)}
.c{background:linear-gradient(45deg,#00d2ff,#928DAB)}
.gd{background:linear-gradient(45deg,#f7971e,#ffd200);color:#000}
.mg{background:linear-gradient(45deg,#ff416c,#ff4b2b)}
.dr{background:linear-gradient(45deg,#434343,#000000)}
.joy{display:flex;justify-content:center;margin:15px 0}
.jg{display:grid;grid-template-columns:repeat(3,55px);grid-template-rows:repeat(3,55px);gap:5px}
.jg .btn{font-size:18px;padding:0;display:flex;align-items:center;justify-content:center}
.jc{background:linear-gradient(45deg,#667eea,#764ba2);border-radius:50%}
.dance-section{margin:20px 0}
.dance-btn{background:linear-gradient(45deg,#FF9800,#FF5722);padding:15px;margin:5px;border-radius:8px;font-weight:bold;}
.refresh-btn{background:linear-gradient(45deg,#2196F3,#21CBF3);padding:10px 15px;border-radius:8px;color:white;border:none;cursor:pointer;margin:5px;}
.token-warning{background:linear-gradient(45deg,#ff0000,#ff6b6b);padding:10px;border-radius:8px;margin:10px 0;text-align:center;font-weight:bold;}
</style>
</head>
<body>
<div class="hdr">
<h1>🤖 Otto Super Robot v6.1</h1>
<div class="status" id="st">Mod: Bekleme</div>
</div>

<div class="token-warning">
⚠️ TEST TOKEN AKTİF! Kullanımdan sonra GitHub'dan iptal edin!
</div>

<div class="modes">
<button class="mode m1" onclick="sendCmd('idle')">😊 Bekleme</button>
<button class="mode m2" onclick="sendCmd('auto')">🚗 Otonom</button>
<button class="mode m3" onclick="sendCmd('dance')">💃 Dans</button>
<button class="mode m4" onclick="sendCmd('party')">🎉 Parti</button>
<button class="mode m6" onclick="sendCmd('mdance')">🎵 Müzikli</button>
<button class="mode m5" onclick="sendCmd('sleep')">😴 Uyku</button>
</div>

<button class="demo" onclick="sendCmd('demo')">🎬 DEMO BAŞLAT</button>

<div class="tabs">
<button class="tab active" onclick="showTab('p1',this)">🎮 Kontrol</button>
<button class="tab" onclick="showTab('p2',this)">💃 Dans</button>
<button class="tab" onclick="showTab('p3',this)">🎵 Şarkılar</button>
<button class="tab" onclick="showTab('p4',this)">👀 Gözler</button>
<button class="tab" onclick="showTab('p5',this)">🔊 Sesler</button>
<button class="tab" onclick="showTab('p6',this)">🎮 Oyunlar</button>
<button class="tab" onclick="showTab('p7',this)">🌐 Tüm İçerik</button>
<button class="tab" onclick="showTab('p8',this)">⚙️ Ayarlar</button>
<button class="tab" onclick="showTab('p9',this)">🔐 Token Test</button>
</div>

<div class="panel active" id="p1">
<div class="sec">
<h3>🕹️ Joystick</h3>
<div class="joy"><div class="jg">
<div></div><button class="btn g" onclick="sendCmd('fwd')">⬆️</button><div></div>
<button class="btn b" onclick="sendCmd('left')">⬅️</button>
<button class="btn jc" onclick="sendCmd('stop')">⏹️</button>
<button class="btn b" onclick="sendCmd('right')">➡️</button>
<div></div><button class="btn g" onclick="sendCmd('back')">⬇️</button><div></div>
</div></div>
</div>
<div class="sec">
<h3>🦵 Hareketler</h3>
<div class="grid">
<button class="btn o" onclick="sendCmd('jump')">🦘 Zıpla</button>
<button class="btn o" onclick="sendCmd('bend')">🙇 Eğil</button>
<button class="btn o" onclick="sendCmd('kleft')">🦶 Sol Tekme</button>
<button class="btn o" onclick="sendCmd('kright')">🦶 Sağ Tekme</button>
<button class="btn pk" onclick="sendCmd('clap')">👏 Alkışla</button>
<button class="btn r" onclick="sendCmd('stopmotors')">🛑 Motor Durdur</button>
</div>
</div>
</div>

<div class="panel" id="p2">
<div class="sec">
<h3>💃 Dans Hareketleri</h3>
<div class="grid">
<button class="btn p" onclick="sendCmd('moon')">🌙 Moonwalk</button>
<button class="btn p" onclick="sendCmd('swing')">🎷 Swing</button>
<button class="btn p" onclick="sendCmd('shake')">🫨 Shake</button>
<button class="btn p" onclick="sendCmd('gang')">🕺 Gangnam</button>
<button class="btn p" onclick="sendCmd('dab')">🙆 Dab</button>
<button class="btn p" onclick="sendCmd('twist')">🌀 Twist</button>
<button class="btn p" onclick="sendCmd('wave')">👋 Wave</button>
<button class="btn p" onclick="sendCmd('twerk')">🍑 Twerk</button>
<button class="btn p" onclick="sendCmd('robot')">🤖 Robot</button>
<button class="btn gd" onclick="sendCmd('rdance')">🎲 Rastgele</button>
</div>
</div>
</div>

<div class="panel" id="p3">
<div class="sec">
<h3>🎵 Çocuk Şarkıları</h3>
<div class="grid">
<button class="btn mg" onclick="sendCmd('danne')">❤️ Anneni Sev</button>
<button class="btn mg" onclick="sendCmd('dhappy')">😊 If Happy</button>
<button class="btn mg" onclick="sendCmd('dshark')">🦈 Baby Shark</button>
<button class="btn mg" onclick="sendCmd('dhead')">👤 Head Shou.</button>
</div>
</div>
<div class="sec">
<h3>🎶 Müzikli Danslar</h3>
<div class="grid">
<button class="btn mg" onclick="sendCmd('dmario')">🍄 Mario</button>
<button class="btn mg" onclick="sendCmd('ddisco')">🪩 Disco</button>
<button class="btn gd" onclick="sendCmd('rmdance')">🎲 Rastgele</button>
</div>
</div>
</div>

<div class="panel" id="p4">
<div class="sec">
<h3>👀 Göz İfadeleri</h3>
<div class="grid">
<button class="btn c" onclick="sendCmd('enorm')">😐 Normal</button>
<button class="btn c" onclick="sendCmd('ehappy')">😊 Mutlu</button>
<button class="btn c" onclick="sendCmd('esad')">😢 Üzgün</button>
<button class="btn c" onclick="sendCmd('eangry')">😠 Kızgın</button>
<button class="btn c" onclick="sendCmd('esurp')">😲 Şaşkın</button>
<button class="btn c" onclick="sendCmd('elove')">😍 Aşık</button>
<button class="btn c" onclick="sendCmd('esleep')">😴 Uykulu</button>
<button class="btn c" onclick="sendCmd('ecool')">😎 Cool</button>
<button class="btn c" onclick="sendCmd('eskull')">💀 İskelet</button>
<button class="btn c" onclick="sendCmd('ewink')">😉 Göz Kırp</button>
<button class="btn c" onclick="sendCmd('ethink')">🤔 Düşünce</button>
<button class="btn r" onclick="sendCmd('edead')">❌ Ölü</button>
</div>
</div>
<div class="sec">
<h3>👁️ Göz Hareketleri</h3>
<div class="grid">
<button class="btn b" onclick="sendCmd('blink')">😑 Kırp</button>
<button class="btn b" onclick="sendCmd('eleft')">👈 Sola Bak</button>
<button class="btn b" onclick="sendCmd('eright')">👉 Sağa Bak</button>
</div>
</div>
</div>

<div class="panel" id="p5">
<div class="sec">
<h3>🔊 Kısa Sesler</h3>
<div class="grid">
<button class="btn pk" onclick="sendCmd('shappy')">😊 Mutlu</button>
<button class="btn pk" onclick="sendCmd('ssad')">😢 Üzgün</button>
<button class="btn pk" onclick="sendCmd('sangry')">😠 Kızgın</button>
<button class="btn pk" onclick="sendCmd('ssurp')">😲 Şaşkın</button>
<button class="btn pk" onclick="sendCmd('sfart')">💨 Osuruk</button>
<button class="btn pk" onclick="sendCmd('scuddly')">🥰 Tatlı</button>
<button class="btn pk" onclick="sendCmd('spolice')">🚔 Polis</button>
<button class="btn pk" onclick="sendCmd('sr2d2')">🤖 R2D2</button>
<button class="btn pk" onclick="sendCmd('slaser')">⚡ Lazer</button>
<button class="btn r" onclick="sendCmd('sgameover')">💀 Game Over</button>
<button class="btn r" onclick="sendCmd('sdeath')">☠️ Ölüm</button>
<button class="btn o" onclick="sendCmd('sclap')">👏 Alkış</button>
</div>
</div>
<div class="sec">
<h3>🎵 Melodiler</h3>
<div class="grid">
<button class="btn gd" onclick="sendCmd('mmario')">🍄 Mario</button>
<button class="btn gd" onclick="sendCmd('mstar')">⭐ Star Wars</button>
<button class="btn gd" onclick="sendCmd('mtetris')">🧱 Tetris</button>
<button class="btn gd" onclick="sendCmd('mpirate')">🏴‍☠️ Pirates</button>
<button class="btn gd" onclick="sendCmd('mhappy')">😊 Happy</button>
<button class="btn gd" onclick="sendCmd('mvic')">🏆 Victory</button>
</div>
</div>
</div>

<div class="panel" id="p6">
<div class="sec">
<h3>🎮 Oyun Efektleri</h3>
<div class="grid">
<button class="btn dr" onclick="sendCmd('death')">💀 Ölüm</button>
<button class="btn dr" onclick="sendCmd('gameover')">🎮 Game Over</button>
<button class="btn g" onclick="sendCmd('victory')">🏆 Zafer</button>
<button class="btn o" onclick="sendCmd('hit')">💥 Vuruldu</button>
<button class="btn b" onclick="sendCmd('levelup')">⬆️ Level Up</button>
<button class="btn p" onclick="sendCmd('powerup')">⚡ Power Up</button>
</div>
</div>
<div class="sec">
<h3>🎭 Özel Hareketler</h3>
<div class="grid">
<button class="btn mg" onclick="sendCmd('hello')">👋 Merhaba</button>
<button class="btn mg" onclick="sendCmd('bye')">👋 Güle Güle</button>
<button class="btn mg" onclick="sendCmd('yes')">✅ Evet</button>
<button class="btn mg" onclick="sendCmd('no')">❌ Hayır</button>
<button class="btn mg" onclick="sendCmd('confused')">🤷 Anlamadım</button>
<button class="btn mg" onclick="sendCmd('love')">❤️ Seni Seviyorum</button>
</div>
</div>
</div>

<div class="panel" id="p7">
<div class="sec">
<h3>🌐 GitHub Tüm İçerik</h3>
<div id="githubAllStatus" style="padding:10px;background:rgba(255,255,255,0.1);border-radius:8px;margin-bottom:15px;">
📡 GitHub durumu kontrol ediliyor...
</div>

<div class="sec">
<h3>💃 Danslar</h3>
<button class="refresh-btn" onclick="loadGitHubFolder('dances', 'danceListAll')">🔄 Yenile</button>
<div id="danceListAll" class="grid">
<!-- Danslar buraya eklenecek -->
</div>
</div>

<div class="sec">
<h3>🎵 Müzikler</h3>
<button class="refresh-btn" onclick="loadGitHubFolder('music', 'musicList')">🔄 Yenile</button>
<div id="musicList" class="grid">
<!-- Müzikler buraya eklenecek -->
</div>
</div>

<div class="sec">
<h3>👀 Göz İfadeleri</h3>
<button class="refresh-btn" onclick="loadGitHubFolder('eyes', 'eyeList')">🔄 Yenile</button>
<div id="eyeList" class="grid">
<!-- Göz ifadeleri buraya eklenecek -->
</div>
</div>

<div class="sec">
<h3>🦵 Hareketler</h3>
<button class="refresh-btn" onclick="loadGitHubFolder('movements', 'movementList')">🔄 Yenile</button>
<div id="movementList" class="grid">
<!-- Hareketler buraya eklenecek -->
</div>
</div>

<div class="sec">
<h3>🎮 Oyunlar</h3>
<button class="refresh-btn" onclick="loadGitHubFolder('games', 'gameList')">🔄 Yenile</button>
<div id="gameList" class="grid">
<!-- Oyunlar buraya eklenecek -->
</div>
</div>
</div>

<div class="panel" id="p8">
<div class="sec">
<h3>⚙️ Sistem Ayarları</h3>
<div class="grid">
<button class="btn b" onclick="sendCmd('refreshwifi')">🔄 WiFi Yenile</button>
<button class="btn g" onclick="sendCmd('refreshgithub')">🔄 GitHub Yenile</button>
<button class="btn o" onclick="sendCmd('calibrate')">🎯 Kalibrasyon</button>
<button class="btn r" onclick="sendCmd('reset')">🔄 Reset</button>
</div>
</div>
<div class="sec">
<h3>📊 Sistem Durumu</h3>
<div id="systemStatus" style="padding:10px;background:rgba(255,255,255,0.1);border-radius:8px;margin-bottom:15px;">
Sistem durumu yükleniyor...
</div>
</div>
</div>

<div class="panel" id="p9">
<div class="sec">
<h3>🔐 GitHub Token Test</h3>
<div style="padding:15px;background:rgba(255,255,255,0.1);border-radius:8px;margin-bottom:15px;">
<p>⚠️ Bu token TEST içindir! Kullanımdan sonra mutlaka iptal edin.</p>
<p>İptal etmek için: <a href="https://github.com/settings/tokens" target="_blank" style="color:#00d2ff;">GitHub Tokens</a></p>
</div>
<button class="btn" onclick="checkToken()" style="background:linear-gradient(45deg,#FF5722,#FF9800);padding:15px;width:100%;margin-bottom:15px;">
🔍 Token'ı Test Et
</button>
<div id="tokenStatus" style="padding:15px;background:rgba(255,255,255,0.1);border-radius:8px;min-height:100px;">
Token durumu burada görünecek...
</div>
<button class="btn" onclick="sendCmd('refreshgithub')" style="background:linear-gradient(45deg,#2196F3,#21CBF3);padding:15px;width:100%;margin-top:15px;">
🔄 GitHub İçeriğini Yenile
</button>
</div>

<script>
function sendCmd(cmd) {
  fetch('/cmd?q='+cmd).then(r=>r.text()).then(t=>document.getElementById('st').innerText=t);
}

function showTab(id, el) {
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  el.classList.add('active');
  
  if (id === 'p7') {
    loadSystemStatus();
  }
  if (id === 'p8') {
    updateSystemStatus();
  }
  if (id === 'p9') {
    document.getElementById('tokenStatus').innerHTML = 'Token durumu burada görünecek...';
  }
}

function loadSystemStatus() {
  const statusEl = document.getElementById('githubAllStatus');
  statusEl.innerHTML = '📡 GitHub durumu kontrol ediliyor...';
  
  fetch('/getSystemStatus')
    .then(response => response.json())
    .then(data => {
      let statusHtml = '';
      if (data.hasInternet) {
        statusHtml = '✅ İnternet bağlantısı var<br>';
        statusHtml += '📁 Dans: ' + data.danceCount + ' | ';
        statusHtml += '🎵 Müzik: ' + data.musicCount + ' | ';
        statusHtml += '👀 Göz: ' + data.eyeCount + ' | ';
        statusHtml += '🦵 Hareket: ' + data.movementCount;
      } else {
        statusHtml = '❌ İnternet bağlantısı yok<br>';
        statusHtml += 'GitHub içeriği kullanılamaz';
      }
      statusEl.innerHTML = statusHtml;
    })
    .catch(error => {
      statusEl.innerHTML = '❌ Durum alınamadı';
    });
}

function loadGitHubFolder(folder, elementId) {
  const listEl = document.getElementById(elementId);
  listEl.innerHTML = '📡 Yükleniyor...';
  
  fetch('/getGitHubFiles?folder=' + encodeURIComponent(folder))
    .then(response => response.json())
    .then(files => {
      listEl.innerHTML = '';
      if (files.length === 0) {
        listEl.innerHTML = '❌ Dosya bulunamadı';
        return;
      }
      
      files.forEach(file => {
        const btn = document.createElement('button');
        btn.className = 'btn dance-btn';
        btn.innerHTML = `📄 ${file}`;
        btn.onclick = () => playGitHubFile(folder, file);
        listEl.appendChild(btn);
      });
    })
    .catch(error => {
      listEl.innerHTML = '❌ Hata: ' + error;
    });
}

function playGitHubFile(folder, filename) {
  document.getElementById('st').innerText = `⏳ "${filename}" yükleniyor...`;
  fetch('/playGitHubFile?folder=' + encodeURIComponent(folder) + '&file=' + encodeURIComponent(filename))
    .then(response => response.text())
    .then(result => {
      document.getElementById('st').innerText = result;
    });
}

function updateSystemStatus() {
  const statusEl = document.getElementById('systemStatus');
  statusEl.innerHTML = '📊 Sistem durumu yükleniyor...';
  
  fetch('/getSystemStatus')
    .then(response => response.json())
    .then(data => {
      let html = '';
      html += '<strong>WiFi Durumu:</strong> ' + data.wifiMode + '<br>';
      html += '<strong>İnternet:</strong> ' + (data.hasInternet ? '✅ Var' : '❌ Yok') + '<br>';
      html += '<strong>GitHub:</strong> ' + (data.githubInitialized ? '✅ Hazır' : '❌ Hazır Değil') + '<br>';
      if (data.lastGitHubError) html += '<strong>Son Hata:</strong> ' + data.lastGitHubError + '<br>';
      html += '<strong>SSID:</strong> ' + data.ssid + '<br>';
      if (data.password) html += '<strong>Şifre:</strong> ' + data.password + '<br>';
      html += '<strong>IP:</strong> ' + data.ip + '<br>';
      html += '<strong>Dans:</strong> ' + data.danceCount + ' adet<br>';
      html += '<strong>Müzik:</strong> ' + data.musicCount + ' adet<br>';
      html += '<strong>Göz:</strong> ' + data.eyeCount + ' adet<br>';
      html += '<strong>Hareket:</strong> ' + data.movementCount + ' adet';
      statusEl.innerHTML = html;
    })
    .catch(error => {
      statusEl.innerHTML = '❌ Durum alınamadı';
    });
}

function checkToken() {
  const statusEl = document.getElementById('tokenStatus');
  statusEl.innerHTML = '🔍 Token test ediliyor...';
  
  fetch('/checkToken')
    .then(response => response.text())
    .then(result => {
      statusEl.innerHTML = result.replace(/\n/g, '<br>');
      if (result.includes("geçerli")) {
        statusEl.style.color = "green";
      } else {
        statusEl.style.color = "red";
      }
    })
    .catch(error => {
      statusEl.innerHTML = '❌ Test başarısız: ' + error;
      statusEl.style.color = "red";
    });
}

window.onload = function() {
  updateSystemStatus();
};
</script>
</body>
</html>
)rawliteral";

  return html;
}

// ═══════════════════════════════════════════════════════════
//                    WEB HANDLER'LAR
// ═══════════════════════════════════════════════════════════

void handleRoot() {
  server.send(200, "text/html", generateMainPage());
}

void handleGetSystemStatus() {
  String json = "{";
  json += "\"wifiMode\":\"";
  
  if (currentWiFiState == WIFI_STATE_STA) json += "STA";
  else if (currentWiFiState == WIFI_STATE_AP) json += "AP";
  else json += "OFF";
  
  json += "\",\"hasInternet\":";
  json += hasInternet ? "true" : "false";
  
  json += ",\"githubInitialized\":";
  json += githubInitialized ? "true" : "false";
  
  json += ",\"lastGitHubError\":\"";
  json += lastGitHubError;
  json += "\"";
  
  json += ",\"danceCount\":";
  json += String(danceFileCount);
  
  json += ",\"musicCount\":";
  json += String(musicFileCount);
  
  json += ",\"eyeCount\":";
  json += String(eyeFileCount);
  
  json += ",\"movementCount\":";
  json += String(movementFileCount);
  
  json += ",\"gameCount\":";
  json += String(gameFileCount);
  
  json += ",\"ssid\":\"";
  if (currentWiFiState == WIFI_STATE_STA) json += STA_SSID;
  else json += AP_SSID;
  
  json += "\",\"password\":\"";
  if (currentWiFiState == WIFI_STATE_AP) json += AP_PASS;
  else json += "";
  
  json += "\",\"ip\":\"";
  if (currentWiFiState == WIFI_STATE_STA) {
    json += WiFi.localIP().toString();
  } else if (currentWiFiState == WIFI_STATE_AP) {
    json += WiFi.softAPIP().toString();
  } else {
    json += "0.0.0.0";
  }
  
  json += "\"}";
  
  server.send(200, "application/json", json);
}

void handleGetGitHubFiles() {
  String folder = server.arg("folder");
  
  String json = "[";
  
  if (folder == "dances") {
    for (int i = 0; i < danceFileCount; i++) {
      json += "\"" + danceFiles[i] + "\"";
      if (i < danceFileCount - 1) json += ",";
    }
  } else if (folder == "music") {
    for (int i = 0; i < musicFileCount; i++) {
      json += "\"" + musicFiles[i] + "\"";
      if (i < musicFileCount - 1) json += ",";
    }
  } else if (folder == "eyes") {
    for (int i = 0; i < eyeFileCount; i++) {
      json += "\"" + eyeFiles[i] + "\"";
      if (i < eyeFileCount - 1) json += ",";
    }
  } else if (folder == "movements") {
    for (int i = 0; i < movementFileCount; i++) {
      json += "\"" + movementFiles[i] + "\"";
      if (i < movementFileCount - 1) json += ",";
    }
  } else if (folder == "games") {
    for (int i = 0; i < gameFileCount; i++) {
      json += "\"" + gameFiles[i] + "\"";
      if (i < gameFileCount - 1) json += ",";
    }
  }
  
  json += "]";
  
  server.send(200, "application/json", json);
}

void handlePlayGitHubFile() {
  if (!hasInternet) {
    server.send(200, "text/plain", "İnternet bağlantısı yok!");
    return;
  }
  
  String folder = server.arg("folder");
  String file = server.arg("file");
  
  if (playGitHubFile(folder.c_str(), file.c_str())) {
    server.send(200, "text/plain", "Oynatıldı: " + folder + "/" + file);
  } else {
    server.send(500, "text/plain", "Hata!");
  }
}

void handleCheckToken() {
  if (!hasInternet) {
    server.send(200, "text/plain", "İnternet yok!");
    return;
  }
  
  HTTPClient http;
  http.begin("https://api.github.com/user");
  http.addHeader("User-Agent", "ESP32-Otto-Robot");
  http.addHeader("Authorization", "Bearer " GITHUB_TOKEN);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  String response = "";
  
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      response = "✅ Token geçerli!\n";
      response += "Kullanıcı: " + doc["login"].as<String>() + "\n";
      response += "Ad: " + doc["name"].as<String>() + "\n";
      response += "Repo: " + String(doc["public_repos"].as<int>()) + "\n";
      response += "GitHub: " + doc["html_url"].as<String>() + "\n";
      response += "⚠️ BU TOKEN TEST İÇİNDİR! Kullanımdan sonra GitHub'dan iptal edin!\n";
      response += "İptal: https://github.com/settings/tokens";
      
    } else {
      response = "❌ JSON parse hatası";
    }
  } else if (httpCode == 401) {
    response = "❌ Token geçersiz veya süresi dolmuş!";
  } else if (httpCode == 403) {
    response = "⚠️ Rate limit! Token çalışıyor ama çok fazla istek yapıldı.";
  } else {
    response = "⚠️ HTTP Hatası: " + String(httpCode);
  }
  
  http.end();
  server.send(200, "text/plain", response);
}

void handleCmd() {
  String q = server.arg("q");
  String r = "OK";
  
  // Modlar
  if (q == "idle") { currentMode = MODE_IDLE; ottoHome(); resetEyes(); r = "Mod: Bekleme"; }
  else if (q == "auto") { currentMode = MODE_AUTO; r = "Mod: Otonom"; }
  else if (q == "dance") { currentMode = MODE_DANCE; r = "Mod: Dans"; }
  else if (q == "party") { currentMode = MODE_PARTY; r = "Mod: Parti"; }
  else if (q == "mdance") { currentMode = MODE_MUSIC_DANCE; r = "Mod: Müzikli Dans"; }
  else if (q == "sleep") { currentMode = MODE_SLEEP; eyeSleepy(); ottoHome(); r = "Mod: Uyku"; }
  else if (q == "demo") { demoMode(); r = "Demo bitti!"; }
  
  // Hareketler
  else if (q == "fwd") { eyeHappy(); ottoWalk(2,1); resetEyes(); r = "İleri"; }
  else if (q == "back") { eyeBlink(); ottoWalk(2,-1); resetEyes(); r = "Geri"; }
  else if (q == "left") { eyeBlink(); ottoTurn(2,1); resetEyes(); r = "Sol"; }
  else if (q == "right") { eyeBlink(); ottoTurn(2,-1); resetEyes(); r = "Sağ"; }
  else if (q == "stop") { ottoHome(); resetEyes(); r = "Durdu"; }
  else if (q == "stopmotors") { servoEmergencyStop(); r = "Motorlar durduruldu"; }
  else if (q == "jump") { eyeSurprised(); ottoJump(2); resetEyes(); r = "Zıpladı"; }
  else if (q == "bend") { ottoBend(1); r = "Eğildi"; }
  else if (q == "kleft") { ottoKickLeft(); r = "Sol tekme"; }
  else if (q == "kright") { ottoKickRight(); r = "Sağ tekme"; }
  else if (q == "clap") { ottoClap(3); r = "Alkışladı"; }
  
  // Danslar
  else if (q == "moon") { eyeHappy(); ottoMoonwalk(3); resetEyes(); r = "Moonwalk"; }
  else if (q == "swing") { eyeHappy(); ottoSwing(3); resetEyes(); r = "Swing"; }
  else if (q == "shake") { ottoShake(6); r = "Shake"; }
  else if (q == "gang") { eyeHappy(); ottoGangnam(3); resetEyes(); r = "Gangnam!"; }
  else if (q == "dab") { ottoDab(1); r = "Dab!"; }
  else if (q == "twist") { ottoTwist(3); r = "Twist"; }
  else if (q == "wave") { ottoWave(4); r = "Wave"; }
  else if (q == "twerk") { ottoTwerk(5); r = "Twerk!"; }
  else if (q == "robot") { ottoRobot(2); r = "Robot"; }
  else if (q == "rdance") { randomDance(); r = "Rastgele dans!"; }
  
  // Çocuk Şarkıları
  else if (q == "danne") { danceAnneniSeviyorsan(); r = "Anneni Seviyorsan!"; }
  else if (q == "dhappy") { danceIfYoureHappy(); r = "If You're Happy!"; }
  else if (q == "dshark") { danceBabyShark(); r = "Baby Shark!"; }
  else if (q == "dhead") { danceHeadShoulders(); r = "Head Shoulders!"; }
  
  // Müzikli Danslar
  else if (q == "dmario") { danceMario(); r = "Mario Dance!"; }
  else if (q == "ddisco") { danceDisco(); r = "Disco!"; }
  else if (q == "rmdance") { randomMusicDance(); r = "Rastgele müzikli!"; }
  
  // Gözler
  else if (q == "enorm") { resetEyes(); r = "Normal"; }
  else if (q == "ehappy") { eyeHappy(); resetEyes(); r = "Mutlu"; }
  else if (q == "esad") { eyeSad(); resetEyes(); r = "Üzgün"; }
  else if (q == "eangry") { eyeAngry(); resetEyes(); r = "Kızgın"; }
  else if (q == "esurp") { eyeSurprised(); r = "Şaşkın"; }
  else if (q == "elove") { eyeLove(); resetEyes(); r = "Aşık"; }
  else if (q == "esleep") { eyeSleepy(); r = "Uykulu"; }
  else if (q == "ecool") { eyeCool(); resetEyes(); r = "Cool"; }
  else if (q == "eskull") { eyeSkull(); r = "İskelet"; }
  else if (q == "ewink") { eyeWink(); r = "Göz kırptı"; }
  else if (q == "ethink") { eyeThinking(); r = "Düşünüyor"; }
  else if (q == "edead") { eyeDead(); r = "Ölü gözler"; }
  else if (q == "blink") { eyeBlink(); r = "Kırptı"; }
  else if (q == "eleft") { eyeLookLeft(); r = "Sola baktı"; }
  else if (q == "eright") { eyeLookRight(); r = "Sağa baktı"; }
  
  // Sesler
  else if (q == "shappy") { sndHappy(); r = "Mutlu ses"; }
  else if (q == "ssad") { sndSad(); r = "Üzgün ses"; }
  else if (q == "sangry") { sndAngry(); r = "Kızgın ses"; }
  else if (q == "ssurp") { sndSurprise(); r = "Şaşkın ses"; }
  else if (q == "sfart") { sndFart(); r = "Prrrt!"; }
  else if (q == "scuddly") { sndCuddly(); r = "Tatlı ses"; }
  else if (q == "spolice") { sndPolice(); r = "Polis!"; }
  else if (q == "sr2d2") { sndR2D2(); r = "R2D2!"; }
  else if (q == "slaser") { sndLaser(); r = "Pew pew!"; }
  else if (q == "sgameover") { sndGameOver(); r = "Game Over!"; }
  else if (q == "sdeath") { sndDeath(); r = "Ölüm sesi"; }
  else if (q == "sclap") { sndClap(); r = "Alkış sesi"; }
  
  // Melodiler
  else if (q == "mmario") { playMario(); r = "Mario!"; }
  else if (q == "mstar") { playStarWars(); r = "Star Wars!"; }
  else if (q == "mtetris") { playTetris(); r = "Tetris!"; }
  else if (q == "mpirate") { playPirates(); r = "Pirates!"; }
  else if (q == "mhappy") { playHappy(); r = "Happy!"; }
  else if (q == "mvic") { playVictory(); r = "Victory!"; }
  
  // Oyun Efektleri
  else if (q == "death") { ottoDeath(); r = "Öldü!"; }
  else if (q == "gameover") { showGameOver(); sndGameOver(); delay(2000); resetEyes(); r = "Game Over!"; }
  else if (q == "victory") { playVictory(); ottoGangnam(2); r = "Zafer!"; }
  else if (q == "hit") { eyeSurprised(); sndSurprise(); delay(500); resetEyes(); r = "Vuruldu!"; }
  else if (q == "levelup") { eyeHappy(); playVictory(); r = "Level Up!"; }
  else if (q == "powerup") { eyeCool(); sndLaser(); r = "Power Up!"; }
  
  // Özel Hareketler
  else if (q == "hello") { eyeHappy(); sndHappy(); ottoWave(3); resetEyes(); r = "Merhaba!"; }
  else if (q == "bye") { eyeSad(); sndSad(); ottoWave(3); resetEyes(); r = "Güle güle!"; }
  else if (q == "yes") { eyeHappy(); sndHappy(); ottoBend(1); ottoBend(-1); resetEyes(); r = "Evet!"; }
  else if (q == "no") { eyeAngry(); sndAngry(); ottoTurn(1,1); ottoTurn(1,-1); resetEyes(); r = "Hayır!"; }
  else if (q == "confused") { eyeThinking(); beep(NOTE_E4,150); beep(NOTE_G4,100); beep(NOTE_E4,150); resetEyes(); r = "Anlamadım?"; }
  else if (q == "love") { eyeLove(); sndCuddly(); ottoClap(2); resetEyes(); r = "Seni seviyorum!"; }
  
  // Sistem komutları
  else if (q == "refreshwifi") { 
    if (connectToWiFi()) {
      r = "WiFi yenilendi";
      showWiFiStatus();
    } else {
      r = "WiFi bağlanamadı";
    }
  }
  else if (q == "refreshgithub") { 
    if (hasInternet) {
      fetchAllGitHubContent();
      r = "GitHub içeriği yenilendi";
      showWiFiStatus();
    } else {
      r = "Internet yok!";
    }
  }
  else if (q == "calibrate") { 
    ottoHome();
    r = "Kalibrasyon tamamlandı";
  }
  else if (q == "reset") { 
    ESP.restart();
    r = "Yeniden başlatılıyor...";
  }
  
  server.send(200, "text/plain", r);
}

// ═══════════════════════════════════════════════════════════
//                         SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== OTTO SUPER ROBOT v6.1 - ESP32-S3 ===\n");
  Serial.println("GitHub Token Desteği Aktif!");
  Serial.println("⚠️ UYARI: Bu token TEST içindir. Kullanımdan sonra iptal edin!");
  
  // BOOT BUTON PINI
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  // TFT Ekranı başlat
  SPI.begin(TFT_SCK, -1, TFT_MOSI);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  
  // Başlangıç ekranı
  showBootScreen();
  sndStart();
  
  delay(1000);
  
  // 1. ADIM: WIFI'YE BAĞLANMAYA ÇALIŞ
  Serial.println("1. WiFi'ye bağlanılıyor...");
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 20);
  tft.println("WiFi Bağlanıyor...");
  tft.setCursor(10, 40);
  tft.print("SSID: ");
  tft.println(STA_SSID);
  
  bool wifiConnected = connectToWiFi();
  
  if (wifiConnected && hasInternet) {
    sndWiFiConnected();
    
    Serial.println("2. GitHub Token test ediliyor...");
    tft.setCursor(10, 60);
    tft.println("GitHub Token test...");
    
    // GitHub token test
    checkGitHubToken();
    
    Serial.println("3. GitHub'dan tüm içerik çekiliyor...");
    tft.setCursor(10, 80);
    tft.println("GitHub bağlanıyor...");
    
    // GitHub bağlantısını test et
    if (fetchAllGitHubContent()) {
      tft.setCursor(10, 100);
      tft.println("GitHub: BAŞARILI!");
      githubInitialized = true;
    } else {
      tft.setCursor(10, 100);
      tft.println("GitHub: BAŞARISIZ!");
      tft.setCursor(10, 115);
      tft.println("Hata: " + lastGitHubError.substring(0, 15));
      githubInitialized = false;
    }
    
    delay(1500);
    showWiFiStatus();
    
  } else if (wifiConnected && !hasInternet) {
    sndWiFiFailed();
    
    Serial.println("2. WiFi var ama internet yok!");
    tft.setCursor(10, 60);
    tft.println("WiFi var, internet yok!");
    tft.setCursor(10, 80);
    tft.println("GitHub çalışmaz!");
    
    delay(1500);
    showWiFiStatus();
    
  } else {
    sndWiFiFailed();
    delay(500);
    sndAPMode();
    
    Serial.println("2. WiFi bağlanamadı, AP Modu başlatılıyor...");
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 20);
    tft.println("WiFi Bağlanamadı!");
    tft.setCursor(10, 40);
    tft.println("AP Modu başlatılıyor...");
    
    startAPMode();
    
    delay(1000);
    showWiFiStatus();
  }
  
  // BEKLEME MODUNDA BAŞLA
  currentMode = MODE_IDLE;
  resetEyes();
  
  // WEB SUNUCUSU ROTALARI
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/getSystemStatus", handleGetSystemStatus);
  server.on("/getGitHubFiles", handleGetGitHubFiles);
  server.on("/playGitHubFile", handlePlayGitHubFile);
  server.on("/checkToken", handleCheckToken);
  
  server.begin();
  Serial.println("\n=== SİSTEM HAZIR ===");
  Serial.print("WiFi Durumu: ");
  Serial.println(currentWiFiState == WIFI_STATE_STA ? "STA" : "AP");
  Serial.print("Internet: ");
  Serial.println(hasInternet ? "Var" : "Yok");
  Serial.print("GitHub: ");
  Serial.println(githubInitialized ? "Hazır" : "Hazır Değil");
  Serial.print("Dans Sayısı: ");
  Serial.println(danceFileCount);
  Serial.print("Web: http://");
  if (currentWiFiState == WIFI_STATE_STA) {
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(WiFi.softAPIP());
  }
}

// ═══════════════════════════════════════════════════════════
//                          LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  
  // Buton kontrolü
  handleButton();
  
  // Dans modu - her 5 saniyede rastgele dans
  if (currentMode == MODE_DANCE) {
    if (millis() - lastDance > 5000) {
      randomDance();
      lastDance = millis();
    }
  }
  
  // Parti modu - sürekli parti!
  if (currentMode == MODE_PARTY) {
    if (millis() - lastDance > 4000) {
      partyMode();
      lastDance = millis();
    }
  }
  
  // Müzikli Dans modu - her 8 saniyede müzikli dans
  if (currentMode == MODE_MUSIC_DANCE) {
    if (millis() - lastDance > 8000) {
      randomMusicDance();
      lastDance = millis();
    }
  }
  
  // Otomatik göz kırpma (uyku ve otonom harici)
  if (currentMode != MODE_SLEEP) {
    if (millis() - lastBlink > 5000) {
      eyeBlink();
      lastBlink = millis();
    }
  }
  
  // GitHub'dan otomatik rastgele dans (internet varsa ve otonom modda)
  if (currentMode == MODE_AUTO && hasInternet && githubInitialized && danceFileCount > 0) {
    if (millis() - lastDance > 15000) { // Her 15 saniyede
      int randomIndex = random(0, danceFileCount);
      String danceName = danceFiles[randomIndex];
      
      showText("GitHub Dans", danceName.c_str(), "");
      playGitHubFile("dances", danceName.c_str());
      
      lastDance = millis();
    }
  }
  
  delay(10);
}
