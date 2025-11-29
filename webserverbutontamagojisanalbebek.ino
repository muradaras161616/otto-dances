/*
 * ═══════════════════════════════════════════════════════════
 *              OTTO SUPER ROBOT - TAMAGOTCHI v1.1
 *      (Hata Duzeltilmis Tam Surum)
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
#define PIN_BUTTON 16  // Fiziksel Buton (D0)

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
enum Mode { MODE_IDLE, MODE_AUTO, MODE_DANCE, MODE_TAMA, MODE_SLEEP };
Mode currentMode = MODE_IDLE;

int moveSpeed = 1000;
unsigned long lastBlink = 0;
unsigned long lastObstacle = 0;
unsigned long lastDance = 0;
bool isMoving = false;

// Buton
unsigned long lastButtonPress = 0;
bool lastButtonState = false;

// Tamagotchi Degiskenleri
int tamaHunger = 0;   // 0 = Tok, 100 = Cok Ac
int tamaEnergy = 100; // 100 = Enerjik, 0 = Yorgun
int tamaHappy = 100;  // 100 = Mutlu, 0 = Uzgun
unsigned long lastTamaUpdate = 0;

struct Eye { int h, w, x, y; };
Eye eyeL, eyeR;
int eyeRadius = 10;

// ═══════════════════════════════════════════════════════════
//                         SES FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

void beep(int freq, int dur) {
  if (freq > 0) {
    tone(PIN_BUZZER, freq, dur);
    delay(dur);
    noTone(PIN_BUZZER);
  } else {
    delay(dur);
  }
}

void sndHappy() { beep(NOTE_C5,100); beep(NOTE_E5,100); beep(NOTE_G5,150); }
void sndSad() { beep(NOTE_G4,200); beep(NOTE_E4,300); }
void sndAngry() { beep(200,100); beep(200,100); beep(200,100); }
void sndSurprise() { for(int i=300; i<1200; i+=100) beep(i,20); }
void sndCuddly() { beep(NOTE_C5,100); beep(NOTE_E5,100); beep(NOTE_G5,100); beep(NOTE_C6,200); }
void sndFart() { for(int i=50; i<300; i+=10) { tone(PIN_BUZZER,i,10); delay(10); } noTone(PIN_BUZZER); }
void sndStart() { beep(NOTE_C5,100); beep(NOTE_G5,150); }
void sndEat() { 
  for(int i=0; i<3; i++) { beep(NOTE_C5+(i*50), 100); delay(50); } 
}
void sndSleep() { 
  for(int i=800; i>400; i-=100) { beep(i, 200); delay(100); }
}

void playMario() {
  int mel[] = {NOTE_E5,NOTE_E5,0,NOTE_E5,0,NOTE_C5,NOTE_E5,NOTE_G5};
  int dur[] = {150,150,150,150,150,150,150,400};
  for(int i=0; i<8; i++) { beep(mel[i],dur[i]); delay(30); yield(); }
}

// ═══════════════════════════════════════════════════════════
//                       GOZ FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

int safeRadius(int r, int w, int h) {
  if (w < 2*(r+1)) r = w/2 - 1;
  if (h < 2*(r+1)) r = h/2 - 1;
  return (r < 0) ? 0 : r;
}

void drawEyes() {
  int rL = safeRadius(eyeRadius, eyeL.w, eyeL.h);
  int rR = safeRadius(eyeRadius, eyeR.w, eyeR.h);
  oled.fillRoundRect(eyeL.x - eyeL.w/2, eyeL.y - eyeL.h/2, eyeL.w, eyeL.h, rL, WHITE);
  oled.fillRoundRect(eyeR.x - eyeR.w/2, eyeR.y - eyeR.h/2, eyeR.w, eyeR.h, rR, WHITE);
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

void showText(const char* l1, const char* l2, const char* l3) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println(l1);
  if (strlen(l2) > 0) oled.println(l2);
  if (strlen(l3) > 0) oled. println(l3);
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

void oscillate(int steps, int T, int A[], int O[], double P[]) {
  for (int i = 0; i < 4; i++) {
    servo[i].SetO(O[i]);
    servo[i].SetA(A[i]);
    servo[i].SetPh(P[i]);
    servo[i].SetT(T);
  }
  unsigned long ref = millis();
  for (int s = 0; s < steps; s++) {
    while (millis() - ref < (unsigned long)T) {
      for (int i = 0; i < 4; i++) servo[i].refresh();
      yield();
    }
    ref = millis();
  }
}

void walk(int steps, int dir) {
  isMoving = true;
  int A[] = {15, 15, 30, 30};
  int O[] = {0, 0, 0, 0};
  double P[] = {0, 0, DEG2RAD(dir*-90), DEG2RAD(dir*-90)};
  oscillate(steps, moveSpeed, A, O, P);
  isMoving = false;
}

void turn(int steps, int dir) {
  isMoving = true;
  int A[] = {20, 20, 15, 15};
  int O[] = {0, 0, 0, 0};
  double P[] = {0, 0, DEG2RAD(dir*-90), DEG2RAD(dir*-90)};
  oscillate(steps, moveSpeed, A, O, P);
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

void moonwalk(int steps) {
  int A[] = {25, 25, 25, 25};
  int O[] = {-15, 15, 0, 0};
  double P[] = {0, DEG2RAD(300), DEG2RAD(-90), DEG2RAD(90)};
  oscillate(steps, moveSpeed, A, O, P);
}

// ═══════════════════════════════════════════════════════════
//                   TAMAGOTCHI FONKSIYONLARI
// ═══════════════════════════════════════════════════════════

void updateTamaStats() {
  if (millis() - lastTamaUpdate > 30000) { // Her 30 saniyede bir guncelle
    lastTamaUpdate = millis();
    
    // Yasam dongusu
    tamaHunger += 2; // Acikma
    tamaEnergy -= 1; // Yorulma
    tamaHappy -= 1;  // Mutsuzlasma
    
    // Sinirlar
    tamaHunger = constrain(tamaHunger, 0, 100);
    tamaEnergy = constrain(tamaEnergy, 0, 100);
    tamaHappy = constrain(tamaHappy, 0, 100);
    
    // Eger cok ac veya yorgunsa uyar
    if (currentMode == MODE_TAMA) {
        if (tamaHunger > 80) { eyeSad(); sndSad(); showText("Cok", "Aciktim!", ""); delay(1000); resetEyes(); }
        else if (tamaEnergy < 20) { eyeSleepy(); sndSleep(); showText("Uykum", "Var...", ""); delay(1000); }
    }
  }
}

void tamaFeed() {
  if (tamaHunger > 10) {
    tamaHunger -= 20;
    tamaHappy += 5;
    tamaEnergy += 5;
    eyeHappy();
    sndEat();
    showText("Nam Nam!", "Lezzetli!", "");
    // Agiz sapurdatma hareketi
    servo[2].SetPosition(70); servo[3].SetPosition(110); delay(200);
    servo[2].SetPosition(90); servo[3].SetPosition(90); delay(200);
    home();
    resetEyes();
  } else {
    showText("Tokum!", "", "");
    sndAngry();
    delay(500);
  }
}

void tamaSleep() {
  tamaEnergy = 100;
  tamaHunger += 10; // Uyuyunca acikir
  currentMode = MODE_SLEEP;
  eyeSleepy();
  sndSleep();
  showText("Iyi", "Geceler...", "zzz");
  home();
}

void tamaPlay() {
  if (tamaEnergy > 10 && tamaHunger < 90) {
    tamaHappy += 20;
    tamaEnergy -= 15;
    tamaHunger += 10;
    eyeHappy();
    sndHappy();
    jump(2);
    showText("Yasasin!", "Oyun!", "");
    home();
    resetEyes();
  } else {
    showText("Hayir!", "Yorgun/Acim", "");
    eyeSad();
    sndSad();
    delay(1000);
    resetEyes();
  }
}

void tamaPet() {
  tamaHappy += 10;
  eyeLove();
  sndCuddly();
  showText("Seni", "Seviyorum", "<3");
  // Kalp atisi
  servo[2].SetPosition(80); servo[3].SetPosition(100); delay(200);
  servo[2].SetPosition(100); servo[3].SetPosition(80); delay(200);
  home();
  resetEyes();
}

bool checkStatus() {
  if (tamaEnergy < 10) {
    showText("Cok", "Yorgunum!", "Enerji Yok");
    eyeSleepy(); sndSad(); delay(1000); resetEyes();
    return false;
  }
  if (tamaHunger > 90) {
    showText("Cok", "Acim!", "Yemek Ver");
    eyeSad(); sndSad(); delay(1000); resetEyes();
    return false;
  }
  return true;
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
<title>Otto Tamagotchi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial;background:linear-gradient(135deg,#1a1a2e,#16213e);min-height:100vh;color:#fff;padding:10px}
.hdr{text-align:center;padding:15px;background:rgba(255,255,255,.1);border-radius:15px;margin-bottom:15px}
h1{font-size:20px}
.status{font-size:12px;opacity:.7;margin-top:8px}
.bars{margin:15px 0;background:rgba(0,0,0,.2);padding:10px;border-radius:10px}
.bar-con{margin-bottom:8px}
.lbl{font-size:11px;margin-bottom:3px;display:flex;justify-content:space-between}
.bar-bg{width:100%;height:10px;background:#333;border-radius:5px;overflow:hidden}
.bar{height:100%;transition:width .5s}
.b-h{background:#ff4757} .b-e{background:#2ed573} .b-hp{background:#ffa502}
.modes{display:flex;flex-wrap:wrap;gap:5px;justify-content:center;margin-bottom:15px}
.mode{padding:10px;border-radius:20px;border:none;font-size:12px;font-weight:700;cursor:pointer;color:#fff;background:#555}
.mode.active{background:#00d2ff}
.tabs{display:flex;gap:5px;margin-bottom:15px;justify-content:center}
.tab{padding:10px;background:rgba(255,255,255,.1);border:none;border-radius:8px;color:#fff;cursor:pointer}
.tab.active{background:#00d2ff}
.panel{display:none} .panel.active{display:block}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.btn{padding:15px 5px;border:none;border-radius:10px;font-size:11px;font-weight:700;cursor:pointer;color:#fff;background:#444}
.btn:active{transform:scale(.95)}
.g{background:#2ed573} .r{background:#ff4757} .b{background:#1e90ff} .o{background:#ffa502}
</style>
</head>
<body>
<div class="hdr">
<h1>🐶 Otto Sanal Bebek</h1>
<div class="status" id="st">Durum: Bekleniyor...</div>
<div class="bars">
 <div class="bar-con"><div class="lbl"><span>🍎 Aclik</span><span id="v-h">0%</span></div><div class="bar-bg"><div id="b-h" class="bar b-h" style="width:0%"></div></div></div>
 <div class="bar-con"><div class="lbl"><span>⚡ Enerji</span><span id="v-e">100%</span></div><div class="bar-bg"><div id="b-e" class="bar b-e" style="width:100%"></div></div></div>
 <div class="bar-con"><div class="lbl"><span>❤️ Mutluluk</span><span id="v-hp">100%</span></div><div class="bar-bg"><div id="b-hp" class="bar b-hp" style="width:100%"></div></div></div>
</div>
</div>

<div class="modes">
<button class="mode" onclick="c('idle')">Bekleme</button>
<button class="mode" onclick="c('tama')">Tamagotchi</button>
<button class="mode" onclick="c('dance')">Dans</button>
<button class="mode" onclick="c('auto')">Otonom</button>
</div>

<div class="tabs">
<button class="tab active" onclick="sw('p1',this)">🐾 Bakim</button>
<button class="tab" onclick="sw('p2',this)">🎮 Kontrol</button>
<button class="tab" onclick="sw('p3',this)">💃 Dans</button>
</div>

<div class="panel active" id="p1">
<div class="grid">
<button class="btn g" onclick="c('feed')">🍖 Besle</button>
<button class="btn b" onclick="c('sleep_action')">🛌 Uyut</button>
<button class="btn o" onclick="c('play')">⚽ Oyna</button>
<button class="btn r" onclick="c('pet')">🥰 Sev</button>
<button class="btn" onclick="c('check')">❓ Durum</button>
</div>
</div>

<div class="panel" id="p2">
<div class="grid">
<button class="btn" onclick="c('fwd')">⬆️ Ileri</button>
<button class="btn" onclick="c('back')">⬇️ Geri</button>
<button class="btn" onclick="c('stop')">⏹️ Dur</button>
<button class="btn" onclick="c('left')">⬅️ Sol</button>
<button class="btn" onclick="c('right')">➡️ Sag</button>
<button class="btn" onclick="c('jump')">🦘 Zipla</button>
</div>
</div>

<div class="panel" id="p3">
<div class="grid">
<button class="btn" onclick="c('moon')">Moonwalk</button>
<button class="btn" onclick="c('gang')">Gangnam</button>
<button class="btn" onclick="c('music')">Muzikli</button>
</div>
</div>

<script>
function c(x){fetch('/c?q='+x).then(r=>r.json()).then(d=>{
 document.getElementById('st').innerText = d.msg;
 update(d);
})}
function update(d){
 document.getElementById('b-h').style.width = d.h + '%'; document.getElementById('v-h').innerText = d.h + '%';
 document.getElementById('b-e').style.width = d.e + '%'; document.getElementById('v-e').innerText = d.e + '%';
 document.getElementById('b-hp').style.width = d.hp + '%'; document.getElementById('v-hp').innerText = d.hp + '%';
}
function sw(id,el){
 document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
 document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
 document.getElementById(id).classList.add('active');
 el.classList.add('active');
}
setInterval(()=>{fetch('/stats').then(r=>r.json()).then(update)}, 2000);
</script>
</body>
</html>
)rawliteral";

// ═══════════════════════════════════════════════════════════
//                    WEB HANDLER'LAR
// ═══════════════════════════════════════════════════════════

String getJson(String msg) {
  String json = "{";
  json += "\"msg\":\"" + msg + "\",";
  json += "\"h\":" + String(tamaHunger) + ",";
  json += "\"e\":" + String(tamaEnergy) + ",";
  json += "\"hp\":" + String(tamaHappy);
  json += "}";
  return json;
}

void handleRoot() {
  server.send(200, "text/html", WEBPAGE);
}

void handleStats() {
  server.send(200, "application/json", getJson("Guncellendi"));
}

void handleCmd() {
  String q = server.arg("q");
  String msg = "OK";
  
  if (q == "idle") { currentMode = MODE_IDLE; home(); msg = "Mod: Bekleme"; }
  else if (q == "tama") { currentMode = MODE_TAMA; eyeHappy(); msg = "Mod: Tamagotchi"; }
  else if (q == "dance") { currentMode = MODE_DANCE; msg = "Mod: Dans"; }
  else if (q == "auto") { currentMode = MODE_AUTO; msg = "Mod: Otonom"; }
  
  // Tamagotchi Komutlari
  else if (q == "feed") { tamaFeed(); msg = "Yemek yedi!"; }
  else if (q == "sleep_action") { tamaSleep(); msg = "Uyuyor..."; }
  else if (q == "play") { tamaPlay(); msg = "Oynuyor!"; }
  else if (q == "pet") { tamaPet(); msg = "Sevildi!"; }
  else if (q == "check") { 
    String s = "A:" + String(tamaHunger) + " E:" + String(tamaEnergy); 
    showText("Durum:", s.c_str(), ""); 
    msg = s; 
  }
  
  // Hareketler (Enerji kontrolu ile)
  else if (checkStatus()) {
    if (q == "fwd") { walk(2,1); msg="Ileri"; }
    else if (q == "back") { walk(2,-1); msg="Geri"; }
    else if (q == "left") { turn(2,1); msg="Sol"; }
    else if (q == "right") { turn(2,-1); msg="Sag"; }
    else if (q == "stop") { home(); msg="Dur"; }
    else if (q == "jump") { jump(2); tamaEnergy-=5; msg="Zipladi"; }
    else if (q == "moon") { moonwalk(3); tamaEnergy-=10; tamaHunger+=5; msg="Moonwalk"; }
    else if (q == "gang") { gangnam(2); tamaEnergy-=15; tamaHunger+=5; msg="Gangnam"; }
    else if (q == "music") { playMario(); msg="Muzik"; }
  }
  
  server.send(200, "application/json", getJson(msg));
}

// ═══════════════════════════════════════════════════════════
//                      BUTON YONETIMI
// ═══════════════════════════════════════════════════════════

void handleButton() {
  bool btnState = digitalRead(PIN_BUTTON);
  
  if (btnState && !lastButtonState) {
    if (millis() - lastButtonPress > 500) {
      lastButtonPress = millis();
      
      // Tamagotchi modundaysan butona basmak SEVMEK demektir
      if (currentMode == MODE_TAMA) {
        tamaPet();
      } else {
        // Degilse mod degistir
        switch(currentMode) {
          case MODE_IDLE: currentMode = MODE_TAMA; eyeHappy(); showText("Mod:", "Sanal Bebek", ""); break;
          case MODE_TAMA: currentMode = MODE_AUTO; eyeCool(); showText("Mod:", "Otonom", ""); break;
          case MODE_AUTO: currentMode = MODE_DANCE; eyeHappy(); showText("Mod:", "Dans", ""); break;
          default: currentMode = MODE_IDLE; home(); resetEyes(); showText("Mod:", "Bekleme", ""); break;
        }
      }
    }
  }
  lastButtonState = btnState;
}

// ═══════════════════════════════════════════════════════════
//                    ULTRASONIK & OTONOM
// ═══════════════════════════════════════════════════════════

long getDistance() {
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);
  long dur = pulseIn(PIN_ECHO, HIGH, 25000);
  long dist = dur * 0.034 / 2;
  if (dist == 0 || dist > 300) dist = 300;
  return dist;
}

void autonomousMode() {
  if (millis() - lastObstacle < 200) return;
  lastObstacle = millis();
  
  long dist = getDistance();
  
  if (dist < OBSTACLE_DIST) {
    // Engel var
    home();
    eyeSurprised();
    sndSurprise();
    delay(200);
    
    // Geri git
    walk(2, -1);
    
    // Sağa veya sola dön
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
    // Engel yok, ileri git
    eyeHappy();
    walk(1, 1);
  }
}

// ═══════════════════════════════════════════════════════════
//                         SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  
  initServos();
  home();
  
  pinMode(PIN_BUTTON, INPUT_PULLDOWN_16);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  
  Wire.begin(PIN_SDA, PIN_SCL);
  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  oled.clearDisplay();
  
  showText("OTTO", "TAMAGOTCHI", "v1.1");
  sndStart();
  delay(2000);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  showText("IP:", WiFi.localIP().toString().c_str(), "");
  
  server.on("/", handleRoot);
  server.on("/c", handleCmd);
  server.on("/stats", handleStats);
  server.begin();
  
  resetEyes();
}

// ═══════════════════════════════════════════════════════════
//                          LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  
  // Buton kontrolu
  handleButton();
  
  // Tamagotchi yasam dongusu (Aclik, Enerji, Mutluluk guncelleme)
  updateTamaStats();
  
  // Otonom mod aktifse
  if (currentMode == MODE_AUTO) {
    // Enerji kontrolu: Yorgunsa otonom gezmez
    if (tamaEnergy > 10) {
      autonomousMode();
    } else {
      // Enerji bittiyse uykuya gec
      currentMode = MODE_SLEEP;
      eyeSleepy();
      showText("Enerji", "Bitti...", "Uyuyor");
      sndSleep();
    }
  }
  
  // Dans modu (Rastgele dans)
  if (currentMode == MODE_DANCE) {
    if (millis() - lastDance > 5000) {
      // Dans etmek enerji harcar
      if (tamaEnergy > 15 && tamaHunger < 90) {
        // Rastgele bir dans sec (Basitlestirilmis)
        int r = random(0, 3);
        if (r == 0) moonwalk(2);
        else if (r == 1) gangnam(2);
        else jump(2);
        
        tamaEnergy -= 5;
        tamaHunger += 2;
        lastDance = millis();
      } else {
        // Cok yorgunsa dans etmeyi birak
        eyeSad();
        showText("Cok", "Yorgunum", "");
        currentMode = MODE_IDLE;
      }
    }
  }
  
  // Uyku modu disinda otomatik goz kirpma
  if (currentMode != MODE_SLEEP) {
    if (millis() - lastBlink > 4000) {
      eyeBlink();
      lastBlink = millis();
    }
  }
  
  yield();
}