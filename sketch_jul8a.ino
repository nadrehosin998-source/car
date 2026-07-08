#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include "PCF8574.h"
#include <Preferences.h>
#include <RCSwitch.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <WiFiUdp.h>

// ==========================================
// ===== تنظیمات UDP Server (دریافت از ESP8266) =====
// ==========================================
const int udpPort = 8888;
WiFiUDP udp;

// ==========================================
// ===== HardwareSerial برای ارتباط با ESP8266 (RFID) - اختیاری =====
// ==========================================
#define ESP8266_RX 33
#define ESP8266_TX 32
HardwareSerial SerialESP8266(2);

// ==========================================
// ===== Objects =====
// ==========================================
Servo myServo;
Preferences preferences;
RCSwitch mySwitch = RCSwitch();
PCF8574 PCF(0x20);

// ==========================================
// ===== PCF8574 PINS =====
// ==========================================
#define START_LED 0
#define SEAT_LEFT_UP 2
#define SEAT_LEFT_DOWN 3
#define SEAT_RIGHT_UP 4
#define SEAT_RIGHT_DOWN 5
#define ELEMENT_LEFT 6

bool startLed = false, seat_left_up = false, seat_left_down = false;
bool seat_right_up = false, seat_right_down = false, element_left = false;

// ==========================================
// ===== RELAY PINS =====
// ==========================================
#define RELAY1 16
#define RELAY2 17
#define RELAY3 18
#define RELAY4 19
#define RELAY5 26
#define RELAY6 22
#define RELAY7 23
#define RELAY8 25

bool relay1State = false, relay2State = false, relay3State = false, relay4State = false;
bool relay5Active = false, relay6State = false, relay7State = false, relay8State = false;

// ==========================================
// ===== متغیرهای کنترل بوق رله 8 =====
// ==========================================
bool relay8BeepEnabled = true;  // فعال/غیرفعال بودن بوق
int relay8BeepDuration = 200;   // مدت زمان بوق به میلی‌ثانیه (پیش‌فرض 200ms)

// ==========================================
// ===== دکمه‌ها =====
// ==========================================
#define PIN_TOGGLE_BUTTON 12
#define PIN_HOLD_BUTTON 15
#define RF_RX_PIN 13
#define BUTTON_START_PIN 34
#define BUTTON_FLASHER_PIN 5

bool lastToggleState = LOW;
unsigned long lastToggleDebounce = 0;
const unsigned long TOGGLE_DEBOUNCE = 300;
int toggleState = 0;
bool toggleButtonEnabled = true;

bool lastHoldState = LOW;
bool holdButtonActive = false;
bool holdButtonEnabled = true;

// ==========================================
// ===== RF Receiver =====
// ==========================================
String RF_CODE_LOCK = "16327688";
String RF_CODE_UNLOCK = "16327681";
String RF_CODE_START = "16327682";
String RF_CODE_TRUNK = "16327684";

// ==========================================
// ===== نور بالا =====
// ==========================================
bool highBeamState = false;
bool highBeamTimerActive = false;
unsigned long highBeamTimerStart = 0;
const unsigned long HIGH_BEAM_TIMEOUT = 30000;

// ==========================================
// ===== Flasher Control =====
// ==========================================
unsigned long relay6LastToggle = 0;
int relay6Interval = 200;
bool flasherEnabled = true;

// ==========================================
// ===== Photocell =====
// ==========================================
#define LDR_PIN 35
bool ldrEnabled = true;
int thresholdOn = 800, thresholdOff = 600;
int delayOn = 5, delayOff = 3;
unsigned long ldrOnTime = 0, ldrOffTime = 0;

// ==========================================
// ===== Web Server =====
// ==========================================
WebServer server(80);
bool loggedIn = false;
String currentUser = "";
bool isAdmin = false;

// ==========================================
// ===== Button START =====
// ==========================================
bool carRun = false;
bool accOn = false;
bool buttonStart;
bool lastbuttonStart = HIGH;
static int startState = 0;
static unsigned long pressTime = 0;
static bool pressHandled = false;
static bool starterActive = false;

// ==========================================
// ===== Button Flasher =====
// ==========================================
bool buttonFlasher;
bool lastbuttonFlasher = HIGH;
unsigned long pressFlashButton = 0;
bool flasherActionDone = false;

bool DoorLock = true;

#define SERVO_PIN 4

// ==========================================
// ===== Keyless LED Variables =====
// ==========================================
bool keylessLedBlinkMode = false;
unsigned long lastKeylessLedToggle = 0;
bool keylessLedState = false;

// ==========================================
// ===== Seat Variables =====
// ==========================================
bool seatAutoAdjustEnabled = true;
bool seatMovingForward = false;
bool seatMovingBackward = false;
unsigned long seatMoveStartTime = 0;

int seatForwardTime = 6;
int seatBackwardTime = 5;
int sleepSeatTime = 5;
int wakeSeatTime = 5;

bool seatBackPerformedThisLockCycle = false;
bool seatForwardPerformedThisAccCycle = false;

bool driverSleepMode = false;
bool passengerSleepMode = false;
bool driverWakeMode = false;
bool passengerWakeMode = false;
unsigned long driverSleepStartTime = 0;
unsigned long passengerSleepStartTime = 0;
unsigned long driverWakeStartTime = 0;
unsigned long passengerWakeStartTime = 0;

bool fullLockMode = false;
bool isCarInGear = false;

TaskHandle_t Task1;
TaskHandle_t Task2;

// ==========================================
// ===== HTML Pages =====
// ==========================================

const char login_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ورود - N Pixel</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:radial-gradient(ellipse at 30% 40%,#0a0f1a,#050608);font-family:'Segoe UI',sans-serif;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.login-card{background:rgba(18,25,35,0.85);backdrop-filter:blur(20px);border-radius:28px;padding:35px 25px;border:1px solid rgba(0,230,255,0.2);width:100%;max-width:380px;box-shadow:0 20px 60px rgba(0,0,0,0.5)}
.logo{text-align:center;margin-bottom:30px}
.car-icon{font-size:3.5rem;animation:float 3s ease-in-out infinite;display:inline-block;filter:drop-shadow(0 0 20px rgba(0,230,255,0.3))}
@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-8px)}}
h1{text-align:center;font-size:1.5rem;background:linear-gradient(135deg,#fff,#00e6ff);-webkit-background-clip:text;background-clip:text;color:transparent;font-weight:700}
.sub-title{text-align:center;color:#4a6a8a;font-size:0.75rem;margin-top:4px;letter-spacing:2px}
.input-icon{position:relative;margin-bottom:18px}
.input-icon span{position:absolute;right:15px;top:50%;transform:translateY(-50%);font-size:1.2rem;color:#00e6ff}
.input-icon input{width:100%;padding:14px 45px 14px 15px;background:rgba(0,0,0,0.4);border:1px solid rgba(0,230,255,0.2);border-radius:16px;color:#fff;font-size:1rem;transition:all 0.3s}
.input-icon input:focus{outline:none;border-color:#00e6ff;box-shadow:0 0 30px rgba(0,230,255,0.1)}
.login-btn{width:100%;padding:14px;background:linear-gradient(135deg,#00e6ff,#0099cc);border:none;border-radius:16px;color:#000;font-size:1rem;font-weight:bold;cursor:pointer;transition:all 0.3s;position:relative;overflow:hidden}
.login-btn:hover{transform:scale(1.02);box-shadow:0 0 40px rgba(0,230,255,0.3)}
</style>
</head>
<body>
<div class="login-card"><div class="logo"><div class="car-icon">🏎️</div><h1>N PIXEL</h1><div class="sub-title">سیستم کنترل هوشمند خودرو</div></div>
<form method="post" action="/check"><div class="input-icon"><span>👤</span><input name="username" placeholder="نام کاربری" required></div>
<div class="input-icon"><span>🔒</span><input type="password" name="password" placeholder="رمز عبور" required></div>
<button type="submit" class="login-btn">🚀 ورود به سیستم</button></form></div>
</body></html>
)rawliteral";

const char control_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>کنترل هوشمند - N Pixel</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:radial-gradient(ellipse at 50% 0%,#0f1923,#050608);color:#fff;font-family:'Segoe UI',system-ui,sans-serif;min-height:100vh;padding:20px;overflow-x:hidden}
::-webkit-scrollbar{width:6px}
::-webkit-scrollbar-track{background:rgba(255,255,255,0.05);border-radius:10px}
::-webkit-scrollbar-thumb{background:linear-gradient(180deg,#00e6ff,#0099cc);border-radius:10px}
@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-10px)}}
@keyframes glow{0%,100%{box-shadow:0 0 20px rgba(0,230,255,0.1)}50%{box-shadow:0 0 40px rgba(0,230,255,0.3)}}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
@keyframes slideDown{from{opacity:0;transform:translateX(-50%) translateY(-80px) scale(0.8)}to{opacity:1;transform:translateX(-50%) translateY(0) scale(1)}}
@keyframes slideIn{from{opacity:0;transform:translateY(30px)}to{opacity:1;transform:translateY(0)}}
@keyframes pulseLock{0%,100%{transform:scale(1)}50%{transform:scale(1.2);color:#ff0000}}
@keyframes pulseUnlock{0%,100%{transform:scale(1)}50%{transform:scale(1.2);color:#00ff88}}
@keyframes notificationSlide{from{transform:translateX(100%);opacity:0}to{transform:translateX(0);opacity:1}}
@keyframes notificationSlideOut{from{transform:translateX(0);opacity:1}to{transform:translateX(100%);opacity:0}}
@keyframes btnPress{0%{transform:scale(1)}50%{transform:scale(0.95)}100%{transform:scale(1)}}
.animate-in{animation:slideIn 0.5s ease forwards}
.container{max-width:1400px;margin:0 auto}

/* ===== Notification Bar ===== */
.notification-container{position:fixed;top:20px;right:20px;z-index:9999;display:flex;flex-direction:column;gap:10px;max-width:450px;width:100%;pointer-events:none}
.notification{pointer-events:auto;padding:16px 20px;border-radius:16px;backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);border:1px solid rgba(255,255,255,0.1);box-shadow:0 10px 40px rgba(0,0,0,0.5);animation:notificationSlide 0.5s cubic-bezier(0.34,1.56,0.64,1) forwards;display:flex;align-items:center;gap:14px;min-width:300px}
.notification.slide-out{animation:notificationSlideOut 0.4s ease forwards}
.notification .icon{font-size:1.8rem;flex-shrink:0}
.notification .content{flex:1}
.notification .title{font-size:0.75rem;font-weight:700;margin-bottom:2px;letter-spacing:0.5px}
.notification .message{font-size:0.9rem;font-weight:500}
.notification .time{font-size:0.6rem;opacity:0.5;margin-top:2px}
.notification .close-btn{background:none;border:none;color:rgba(255,255,255,0.3);font-size:1.2rem;cursor:pointer;transition:all 0.3s;padding:0 5px}
.notification .close-btn:hover{color:#fff;transform:scale(1.2)}
.notification-success{background:rgba(0,214,143,0.12);border-color:rgba(0,214,143,0.3)}
.notification-success .title{color:#00d68f}
.notification-success .message{color:#b0e8d0}
.notification-error{background:rgba(255,61,113,0.12);border-color:rgba(255,61,113,0.3)}
.notification-error .title{color:#ff3d71}
.notification-error .message{color:#f0b0c0}
.notification-warning{background:rgba(255,170,0,0.12);border-color:rgba(255,170,0,0.3)}
.notification-warning .title{color:#ffaa00}
.notification-warning .message{color:#f0d0a0}
.notification-info{background:rgba(0,230,255,0.12);border-color:rgba(0,230,255,0.3)}
.notification-info .title{color:#00e6ff}
.notification-info .message{color:#b0e8f0}

/* ===== Toast ===== */
.toast-message{position:fixed;top:30px;left:50%;transform:translateX(-50%);padding:18px 35px;border-radius:20px;font-size:1.1rem;font-weight:700;z-index:9998;box-shadow:0 15px 60px rgba(0,0,0,0.6);animation:slideDown 0.5s cubic-bezier(0.34,1.56,0.64,1) forwards;backdrop-filter:blur(25px);-webkit-backdrop-filter:blur(25px);max-width:90%;text-align:center;border:1px solid rgba(255,255,255,0.1);letter-spacing:0.5px}
.toast-success{background:rgba(0,214,143,0.15);color:#00d68f;border-color:rgba(0,214,143,0.3);text-shadow:0 0 20px rgba(0,214,143,0.2)}
.toast-error{background:rgba(255,61,113,0.15);color:#ff3d71;border-color:rgba(255,61,113,0.3);text-shadow:0 0 20px rgba(255,61,113,0.2)}
.toast-info{background:rgba(0,230,255,0.15);color:#00e6ff;border-color:rgba(0,230,255,0.3);text-shadow:0 0 20px rgba(0,230,255,0.2)}
.toast-warning{background:rgba(255,170,0,0.15);color:#ffaa00;border-color:rgba(255,170,0,0.3);text-shadow:0 0 20px rgba(255,170,0,0.2)}

/* ===== Header ===== */
.header{display:flex;align-items:center;justify-content:space-between;background:rgba(18,30,45,0.85);backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);border-radius:25px;padding:15px 30px;margin-bottom:25px;border:1px solid rgba(0,230,255,0.15);animation:glow 3s ease-in-out infinite}
.header-left{display:flex;align-items:center;gap:20px}
.car-icon{font-size:3rem;animation:float 3s ease-in-out infinite}
.brand{display:flex;flex-direction:column}
.brand-title{font-size:1.8rem;font-weight:800;background:linear-gradient(135deg,#fff,#00e6ff);-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;letter-spacing:-1px}
.brand-sub{font-size:0.7rem;color:#4a8a9a;letter-spacing:3px;text-transform:uppercase}
.header-right{display:flex;align-items:center;gap:15px;flex-wrap:wrap}
.user-badge{background:rgba(0,230,255,0.1);padding:8px 20px;border-radius:30px;display:flex;align-items:center;gap:12px;font-size:0.85rem;border:1px solid rgba(0,230,255,0.1)}
.user-badge .avatar{width:32px;height:32px;border-radius:50%;background:linear-gradient(135deg,#00e6ff,#0099cc);display:flex;align-items:center;justify-content:center;font-size:1rem}
.admin-tag{background:linear-gradient(135deg,#FFD700,#FFA500);color:#000;font-size:0.6rem;padding:2px 12px;border-radius:20px;font-weight:700}
.online-dot{width:10px;height:10px;background:#00d68f;border-radius:50%;display:inline-block;animation:pulse 2s ease-in-out infinite;margin-right:5px}
.logout-btn{background:rgba(255,61,113,0.15);padding:8px 18px;border-radius:30px;cursor:pointer;transition:all 0.3s;border:1px solid rgba(255,61,113,0.2);font-size:0.85rem;color:#ff3d71}
.logout-btn:hover{background:rgba(255,61,113,0.3);transform:scale(1.05)}

/* ===== Status Cards ===== */
.status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin-bottom:25px}
.status-card{background:linear-gradient(145deg,rgba(14,20,28,0.8),rgba(8,12,18,0.8));border-radius:18px;padding:18px 15px;text-align:center;border:1px solid rgba(0,230,255,0.05);transition:all 0.3s;cursor:default}
.status-card:hover{transform:translateY(-3px);border-color:rgba(0,230,255,0.2)}
.status-card .icon{font-size:1.8rem;display:block;margin-bottom:6px}
.status-label{font-size:0.65rem;color:#6a8a9a;text-transform:uppercase;letter-spacing:1px}
.status-value{font-size:1.1rem;font-weight:700;margin-top:4px}
.status-value.on{color:#00d68f}
.status-value.off{color:#ff3d71}
.status-value.blink{color:#ffaa00;animation:pulse 1s ease-in-out infinite}
.status-value.locked{color:#ff3d71;animation:pulseLock 1s ease}
.status-value.unlocked{color:#00d68f;animation:pulseUnlock 1s ease}

/* ===== Main Grid ===== */
.main-grid{display:grid;grid-template-columns:1fr 1fr;gap:20px}
@media(max-width:1024px){.main-grid{grid-template-columns:1fr}}

/* ===== Glass Cards ===== */
.glass-card{background:rgba(18,30,45,0.6);backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);border-radius:22px;padding:22px;border:1px solid rgba(0,230,255,0.08);transition:all 0.3s;animation:slideIn 0.5s ease forwards}
.glass-card:hover{border-color:rgba(0,230,255,0.2)}
.card-header{display:flex;align-items:center;gap:12px;margin-bottom:18px;padding-bottom:12px;border-bottom:1px solid rgba(0,230,255,0.08)}
.card-header .icon{font-size:1.4rem;background:rgba(0,230,255,0.08);padding:8px 12px;border-radius:12px}
.card-header h3{font-size:1rem;color:#00e6ff;font-weight:600}
.card-header .badge{margin-right:auto;font-size:0.6rem;padding:3px 12px;border-radius:20px;background:rgba(255,215,0,0.15);color:#FFD700}

/* ===== Buttons ===== */
.btn{background:linear-gradient(145deg,#1a2533,#10161f);border:none;color:#fff;padding:12px 20px;border-radius:14px;font-size:0.8rem;font-weight:600;cursor:pointer;display:inline-flex;align-items:center;justify-content:center;gap:8px;box-shadow:0 4px 0 #0a0e12;transform:translateY(-2px);width:100%;transition:all 0.15s ease;user-select:none;position:relative;overflow:hidden;text-decoration:none}
.btn::after{content:'';position:absolute;inset:0;background:linear-gradient(90deg,transparent,rgba(255,255,255,0.05),transparent);transform:translateX(-100%);transition:transform 0.5s}
.btn:hover::after{transform:translateX(100%)}
.btn:active{transform:translateY(2px);box-shadow:0 2px 0 #0a0e12;animation:btnPress 0.2s ease}
.btn:disabled{opacity:0.5;cursor:not-allowed;transform:none!important}
.btn-primary{background:linear-gradient(135deg,#00ccee,#0099bb);color:#000;box-shadow:0 4px 0 #007799}
.btn-success{background:linear-gradient(135deg,#00d68f,#00aa66);color:#000;box-shadow:0 4px 0 #008855}
.btn-danger{background:linear-gradient(135deg,#ff3d71,#cc1144);color:#fff;box-shadow:0 4px 0 #990033}
.btn-warning{background:linear-gradient(135deg,#ff8800,#cc6600);color:#000;box-shadow:0 4px 0 #995500}
.btn-purple{background:linear-gradient(135deg,#aa00ff,#7700cc);color:#fff;box-shadow:0 4px 0 #550099}
.btn-teal{background:linear-gradient(135deg,#00cc99,#009977);color:#000;box-shadow:0 4px 0 #007755}
.btn-pulse{background:linear-gradient(135deg,#ff6622,#cc4411);color:#fff;box-shadow:0 4px 0 #993300}
.btn-hold{background:linear-gradient(135deg,#ffaa22,#cc8822);color:#000;box-shadow:0 4px 0 #996600}
.btn-servo-lock{background:linear-gradient(135deg,#cc0000,#990000);color:#fff;box-shadow:0 4px 0 #660000}
.btn-servo-unlock{background:linear-gradient(135deg,#00aa66,#008844);color:#000;box-shadow:0 4px 0 #006633}
.btn.active{background:linear-gradient(135deg,#00d68f,#00aa66);color:#fff;box-shadow:0 0 30px rgba(0,214,143,0.3)}
.btn.inactive{background:linear-gradient(135deg,#ff3d71,#cc1144);color:#fff;box-shadow:0 0 30px rgba(255,61,113,0.3)}
.grid-2{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px}
.grid-4{display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:12px}
@media(max-width:600px){.grid-2,.grid-3,.grid-4{grid-template-columns:1fr 1fr}}

/* ===== Status Bar ===== */
.status-bar{display:flex;gap:15px;margin-bottom:20px;flex-wrap:wrap}
.status-item-card{flex:1;min-width:200px;background:rgba(18,30,45,0.5);border-radius:16px;padding:14px 20px;display:flex;justify-content:space-between;align-items:center;border:1px solid rgba(0,230,255,0.05)}
.lock-toggle{background:rgba(255,61,113,0.15);padding:4px 16px;border-radius:20px;cursor:pointer;transition:all 0.3s;font-size:0.75rem;border:1px solid rgba(255,61,113,0.2)}
.lock-toggle:hover{background:rgba(255,61,113,0.3);transform:scale(1.05)}
.progress-container{margin-top:15px}
.progress-bar{width:100%;height:6px;background:rgba(255,255,255,0.05);border-radius:10px;overflow:hidden}
.progress-fill{width:0%;height:100%;background:linear-gradient(90deg,#00e6ff,#00d68f);transition:width 0.3s ease;border-radius:10px}
.timer-text{font-size:0.7rem;color:#00e6ff;text-align:center;margin-top:6px}
.alert-box{background:rgba(255,170,0,0.08);border:1px solid rgba(255,170,0,0.2);border-radius:12px;padding:10px 15px;margin-top:12px;font-size:0.7rem;color:#ccaa66}
.alert-box.info{background:rgba(0,230,255,0.08);border-color:rgba(0,230,255,0.2);color:#66ccdd}
.alert-box.success{background:rgba(0,214,143,0.08);border-color:rgba(0,214,143,0.2);color:#66ddaa}
.settings-btn{background:rgba(0,230,255,0.05);border:1px solid rgba(0,230,255,0.15);padding:14px;border-radius:16px;text-align:center;cursor:pointer;margin-top:20px;transition:all 0.3s;color:#00e6ff;font-size:0.85rem}
.settings-btn:hover{background:rgba(0,230,255,0.15);transform:scale(1.01)}
.footer{text-align:center;margin-top:30px;padding:20px;font-size:0.65rem;color:#3a5a6a;border-top:1px solid rgba(0,230,255,0.05);letter-spacing:1px}
.footer span{color:#00e6ff}
.status-badge{display:inline-block;padding:3px 14px;border-radius:20px;font-size:0.65rem;font-weight:700;transition:0.3s}
.status-badge.on{background:rgba(0,214,143,0.2);color:#00d68f}
.status-badge.off{background:rgba(255,61,113,0.2);color:#ff3d71}
.btn-group{display:flex;gap:12px;flex-wrap:wrap}
.btn-group .btn{flex:1;min-width:100px}
.full-lock-active{color:#ff3d71!important}
.full-lock-inactive{color:#00d68f!important}
.disabled{opacity:0.4;pointer-events:none}
.car-status-card{display:flex;align-items:center;justify-content:center;gap:30px;padding:25px;background:radial-gradient(ellipse at center,rgba(0,230,255,0.05),transparent);border-radius:20px;border:1px solid rgba(0,230,255,0.05);margin-bottom:20px}
.car-status-card .car-icon-big{font-size:5rem;animation:float 3s ease-in-out infinite}
.car-status-card .status-text{font-size:1.2rem;font-weight:700}
.car-status-card .status-text.on{color:#00d68f}
.car-status-card .status-text.off{color:#ff3d71}
.physical-buttons-card{margin-top:15px}
.physical-buttons-card .btn-group{display:flex;gap:12px}
.physical-buttons-card .btn-group .btn{flex:1}
.special-section{display:flex;flex-direction:column;gap:12px}
.scroll-top{position:fixed;bottom:30px;right:30px;width:45px;height:45px;border-radius:50%;background:rgba(0,230,255,0.15);border:1px solid rgba(0,230,255,0.2);color:#00e6ff;font-size:1.5rem;cursor:pointer;display:none;align-items:center;justify-content:center;backdrop-filter:blur(10px);transition:all 0.3s;z-index:100}
.scroll-top:hover{background:rgba(0,230,255,0.3);transform:scale(1.1)}
.scroll-top.show{display:flex}

/* ===== Responsive ===== */
@media(max-width:768px){.header{flex-direction:column;gap:15px;padding:15px 20px}.header-left{width:100%;justify-content:center}.header-right{width:100%;justify-content:center;flex-wrap:wrap}.status-grid{grid-template-columns:repeat(4,1fr);gap:8px}.status-card{padding:12px 8px}.status-card .icon{font-size:1.2rem}.status-value{font-size:0.85rem}.glass-card{padding:16px}.car-status-card{flex-direction:column;gap:15px;padding:20px}.car-status-card .car-icon-big{font-size:3.5rem}.status-bar{flex-direction:column}.status-item-card{min-width:unset}.notification-container{top:10px;right:10px;max-width:calc(100% - 20px)}.notification{min-width:unset;padding:12px 16px}.physical-buttons-card .btn-group{flex-direction:column}}
@media(max-width:480px){.status-grid{grid-template-columns:repeat(2,1fr)}.grid-2,.grid-3,.grid-4{grid-template-columns:1fr 1fr}.btn{font-size:0.65rem;padding:10px 12px}.brand-title{font-size:1.2rem}.user-badge{font-size:0.7rem;padding:5px 12px}.toast-message{font-size:0.85rem;padding:14px 20px;top:80px}}
</style>
</head>
<body>
<div class="notification-container" id="notificationContainer"></div>
<div class="container">
<header class="header animate-in">
<div class="header-left"><div class="car-icon">🏎️</div><div class="brand"><span class="brand-title">N PIXEL</span><span class="brand-sub">سیستم کنترل هوشمند</span></div></div>
<div class="header-right"><div class="user-badge"><span class="avatar">👤</span><span id="userNameDisplay">---</span><span id="adminBadge" class="admin-tag" style="display:none">ادمین</span></div><div class="user-badge"><span class="online-dot"></span><span>آنلاین</span></div><div class="logout-btn" onclick="logout()">🚪 خروج</div></div>
</header>
<div class="car-status-card animate-in"><div class="car-icon-big">🚗</div><div><div class="status-text" id="carMainStatus">⏹️ خاموش</div><div style="font-size:0.8rem;color:#6a8a9a;margin-top:4px;"><span id="doorMainStatus">🔒 قفل</span> • <span id="accMainStatus">⚡ خاموش</span></div></div></div>
<div class="status-grid animate-in">
<div class="status-card"><span class="icon">🚪</span><div class="status-label">درب</div><div class="status-value" id="doorStatus">---</div></div>
<div class="status-card"><span class="icon">⚡</span><div class="status-label">ACC</div><div class="status-value" id="accStatus">---</div></div>
<div class="status-card"><span class="icon">🔧</span><div class="status-label">موتور</div><div class="status-value" id="carStatus">---</div></div>
<div class="status-card"><span class="icon">🪑</span><div class="status-label">صندلی</div><div class="status-value" id="seatStatus">متوقف</div></div>
<div class="status-card"><span class="icon">💡</span><div class="status-label">نور بالا</div><div class="status-value" id="flasherStatus">---</div></div>
<div class="status-card"><span class="icon">📸</span><div class="status-label">فوتوسل</div><div class="status-value" id="photocellStatus">---</div></div>
<div class="status-card"><span class="icon">✨</span><div class="status-label">چراغ کیلس</div><div class="status-value" id="keylessStatus">---</div></div>
<div class="status-card"><span class="icon">🏁</span><div class="status-label">حالت</div><div class="status-value" id="sportStatus">معمولی</div></div>
</div>
<div class="status-bar animate-in">
<div class="status-item-card"><span>🎛️ فیلتر هوای اسپرت</span><div class="lock-toggle" onclick="sendCommand('/filterchange')">🔄 تغییر</div></div>
<div class="status-item-card"><span>🛡️ قفل کامل</span><span id="fullLockStatus" style="font-weight:700;color:#00d68f">غیرفعال</span><div class="lock-toggle" onclick="toggleFullLock()">🔒 تغییر</div></div>
</div>
<div class="main-grid">
<div class="left-col">
<div class="glass-card animate-in"><div class="card-header"><span class="icon">💡</span><h3>چراغ بدرقه</h3></div><button class="btn btn-primary" onclick="toggleHighBeam()">🚗 چراغ بدرقه (۳۰ ثانیه)</button><div id="highbeamProgress" style="display:none" class="progress-container"><div class="progress-bar"><div class="progress-fill" id="progressFill"></div></div><div class="timer-text" id="timerText"></div></div></div>
<div class="glass-card animate-in"><div class="card-header"><span class="icon">🪑</span><h3>کنترل صندلی</h3></div><div class="grid-2"><button class="btn btn-hold" onmousedown="holdStart('SEAT_LEFT_UP')" onmouseup="holdStop('SEAT_LEFT_UP')" ontouchstart="holdStart('SEAT_LEFT_UP')" ontouchend="holdStop('SEAT_LEFT_UP')">👆 راننده بالا</button><button class="btn btn-hold" onmousedown="holdStart('SEAT_LEFT_DOWN')" onmouseup="holdStop('SEAT_LEFT_DOWN')" ontouchstart="holdStart('SEAT_LEFT_DOWN')" ontouchend="holdStop('SEAT_LEFT_DOWN')">👇 راننده پایین</button><button class="btn btn-hold" onmousedown="holdStart('SEAT_RIGHT_UP')" onmouseup="holdStop('SEAT_RIGHT_UP')" ontouchstart="holdStart('SEAT_RIGHT_UP')" ontouchend="holdStop('SEAT_RIGHT_UP')">👆 سرنشین بالا</button><button class="btn btn-hold" onmousedown="holdStart('SEAT_RIGHT_DOWN')" onmouseup="holdStop('SEAT_RIGHT_DOWN')" ontouchstart="holdStart('SEAT_RIGHT_DOWN')" ontouchend="holdStop('SEAT_RIGHT_DOWN')">👇 سرنشین پایین</button></div></div>
<div class="glass-card animate-in"><div class="card-header"><span class="icon">🎯</span><h3>حالت‌های ویژه</h3></div><div class="special-section"><div class="grid-3"><button class="btn btn-purple" onclick="sendCommand('/sleepboth')">😴 هر دو</button><button class="btn btn-purple" onclick="sendCommand('/sleepdriver')">😴 راننده</button><button class="btn btn-purple" onclick="sendCommand('/sleeppassenger')">😴 سرنشین</button><button class="btn btn-teal" onclick="sendCommand('/wakeboth')">🔆 هر دو</button><button class="btn btn-teal" onclick="sendCommand('/wakedriver')">🔆 راننده</button><button class="btn btn-teal" onclick="sendCommand('/wakepassenger')">🔆 سرنشین</button></div>
<div class="physical-buttons-card"><div class="card-header" style="margin-bottom:12px;padding-bottom:8px;"><span class="icon">🎛️</span><h3 style="font-size:0.9rem;">کنترل دکمه‌های فیزیکی</h3></div><div class="btn-group"><button class="btn active" id="toggleBtn" onclick="toggleButton('toggle')">🔄 دکمه آینه</button><button class="btn active" id="holdBtn" onclick="toggleButton('hold')">🔘 دکمه رله 5</button></div><div style="text-align:center;margin-top:12px;display:flex;justify-content:space-around;flex-wrap:wrap;gap:8px"><div><span style="color:#6a8a9a;font-size:0.75rem;">🪞 آینه:</span><span id="toggleStatusText" class="status-badge on">فعال</span></div><div><span style="color:#6a8a9a;font-size:0.75rem;">⚡ رله 5:</span><span id="holdStatusText" class="status-badge on">فعال</span></div></div></div>
<div style="margin-top:5px;"><button class="btn btn-danger" onclick="resetSystem()">🔁 ریست سیستم</button></div></div></div>
</div>
<div class="right-col">
<div class="glass-card animate-in">
<div class="card-header">
<span class="icon">🔐</span>
<h3>کنترل درب</h3>
<span class="badge" id="beepStatusBadge" style="background:rgba(0,214,143,0.15);color:#00d68f;">🔊 فعال</span>
</div>
<div class="grid-2">
<button class="btn btn-success" onclick="unlockDoor()">🔓 باز کردن قفل</button>
<button class="btn btn-danger" onclick="lockDoor()">🔒 قفل کردن</button>
</div>
<div class="grid-2" style="margin-top:12px">
<button class="btn btn-servo-lock" onclick="sendCommand('/servolock')">🔒 قفل سروو</button>
<button class="btn btn-servo-unlock" onclick="sendCommand('/servounlock')">🔓 باز سروو</button>
</div>
<div style="margin-top:12px; display:flex; gap:12px;">
<button class="btn btn-success" id="beepToggleBtn" onclick="toggleBeep()" style="flex:1;">🔊 بوق رله 8</button>
</div>
<div style="text-align:center;margin-top:8px;font-size:0.7rem;color:#6a8a9a;">مدت زمان: <span id="beepDurationDisplay">200</span>ms</div>
<div class="alert-box info">🔧 فقط سروو حرکت میکند - درب تغییری نمیکند</div>
<div class="alert-box success">💡 هنگام قفل، نور بالا خاموش میشود</div>
</div>
<div class="glass-card animate-in"><div class="card-header"><span class="icon">⚡</span><h3>رله‌های کنترلی</h3></div><div class="grid-2"><button class="btn btn-pulse" onclick="sendCommand('/pulse1')">🪞 باز شدن اینه</button><button class="btn btn-pulse" onclick="sendCommand('/pulse2')">🪞 بسته شدن اینه</button><button class="btn btn-warning" onclick="openTrunkWithUnlock()">📦 صندوق عقب</button></div></div>
<div class="glass-card animate-in"><div class="card-header"><span class="icon">🔑</span><h3>استارت</h3></div><div class="grid-2"><button class="btn btn-success" onclick="remoteStart()">▶️ استارت از راه دور</button><button class="btn btn-danger" onclick="remoteStop()">⏹️ خاموش کردن</button></div></div>
<div class="glass-card animate-in" id="photocellCard"><div class="card-header"><span class="icon">☀️</span><h3>کنترل فوتوسل</h3><span class="badge">🔒 فقط ادمین</span></div><div class="grid-3"><button class="btn btn-success" onclick="sendCommand('/pcon')">💡 روشن</button><button class="btn btn-danger" onclick="sendCommand('/pcoff')">💡 خاموش</button><button class="btn btn-primary" onclick="sendCommand('/pcauto')">🔄 خودکار</button></div></div>
</div>
</div>
<div class="settings-btn animate-in" id="settingsBtn" onclick="window.location.href='/ldr'">⚙️ تنظیمات پیشرفته</div>
<div class="footer">⚡ <span>N PIXEL</span> • سیستم کنترل هوشمند خودرو</div>
</div>
<button class="scroll-top" id="scrollTop" onclick="window.scrollTo({top:0,behavior:'smooth'})">⬆</button>

<script>
// ===== Notification System (فقط یک اعلان) =====
function showNotification(icon,title,message,type='success',duration=4000){
    const container=document.getElementById('notificationContainer');
    container.innerHTML='';
    const notif=document.createElement('div');
    notif.className='notification notification-'+type;
    notif.innerHTML='<div class="icon">'+icon+'</div><div class="content"><div class="title">'+title+'</div><div class="message">'+message+'</div><div class="time">'+new Date().toLocaleTimeString('fa-IR')+'</div></div><button class="close-btn" onclick="closeNotification(this)">✕</button>';
    container.appendChild(notif);
    setTimeout(()=>{closeNotification(notif.querySelector('.close-btn'))},duration);
}
function closeNotification(btn){const notif=btn.closest('.notification');notif.classList.add('slide-out');setTimeout(()=>{notif.remove()},400)}

// ===== Toast =====
function showToast(message,type='success'){
    if(navigator.vibrate){
        if(type==='success'){navigator.vibrate([100,50,100,50,200])}
        else if(type==='error'){navigator.vibrate([200,100,200,100,300])}
        else{navigator.vibrate(100)}
    }
    const oldMsg=document.querySelector('.toast-message');
    if(oldMsg)oldMsg.remove();
    const toast=document.createElement('div');
    toast.className='toast-message toast-'+type;
    toast.innerHTML=message;
    document.body.appendChild(toast);
    setTimeout(()=>{toast.style.opacity='0';setTimeout(()=>toast.remove(),300)},3000);
}

// ===== Send Command (فقط یک اعلان) =====
function sendCommandWithNotification(url,icon,title,successMsg,errorMsg='❌ خطا در اجرا!'){
    fetch(url).then(response=>{
        if(response.ok){
            showNotification(icon,title,successMsg,'success');
            updateStatus();
        }else{
            showNotification('❌','خطا',errorMsg,'error');
        }
    }).catch(()=>{
        showNotification('❌','خطا','ارتباط با سرور قطع شد!','error');
    });
}
function sendCommand(url){fetch(url).catch(e=>console.log(e))}

// ===== Functions =====
function lockDoor(){if(navigator.vibrate)navigator.vibrate(50);sendCommandWithNotification('/relay2','🔒','قفل درب','🔒 درب ماشین قفل شد ✅','❌ قفل کردن درب انجام نشد!')}
function unlockDoor(){if(navigator.vibrate)navigator.vibrate(50);sendCommandWithNotification('/relay1','🔓','باز کردن درب','🔓 درب ماشین باز شد ✅','❌ باز کردن درب انجام نشد!')}
function remoteStart(){if(navigator.vibrate)navigator.vibrate(50);sendCommandWithNotification('/remotestart','🚗','استارت','🚗 ماشین با موفقیت روشن شد ✅','❌ روشن کردن ماشین انجام نشد!')}
function remoteStop(){if(navigator.vibrate)navigator.vibrate(50);sendCommandWithNotification('/remotestop','⏹️','خاموش کردن','⏹️ ماشین با موفقیت خاموش شد ✅','❌ خاموش کردن ماشین انجام نشد!')}
function toggleHighBeam(){if(navigator.vibrate)navigator.vibrate(50);sendCommandWithNotification('/highbeam','💡','چراغ بدرقه','💡 چراغ بدرقه فعال شد (۳۰ ثانیه) ✅','❌ فعال کردن چراغ بدرقه انجام نشد!')}
function openTrunkWithUnlock(){if(navigator.vibrate)navigator.vibrate(50);sendCommandWithNotification('/opentrunk','📦','صندوق عقب','📦 صندوق عقب با موفقیت باز شد ✅','❌ باز کردن صندوق عقب انجام نشد!')}
function toggleElement(){if(navigator.vibrate)navigator.vibrate(50);sendCommandWithNotification('/elementleft','❄️','المنت چپ','❄️ المنت چپ تغییر وضعیت داد ✅','❌ تغییر وضعیت المنت انجام نشد!')}

// ===== Full Lock =====
function toggleFullLock(){
    if(!isAdminUser){
        showNotification('⚠️','دسترسی محدود','فقط ادمین میتواند قفل کامل را تغییر دهد!','warning');
        return;
    }
    fetch('/toggleFullLock').then(response=>response.text()).then(data=>{
        if(data==='true'){
            showNotification('🔒','قفل کامل','قفل کامل فعال شد - همه دکمه‌ها غیرفعال ✅','warning');
            if(navigator.vibrate)navigator.vibrate([100,50,100,50,200,50,300]);
        }else{
            showNotification('🔓','قفل کامل','قفل کامل غیرفعال شد - دکمه‌ها فعال ✅','success');
        }
        updateStatus();
    }).catch(()=>{showNotification('❌','خطا','خطا در تغییر وضعیت قفل کامل!','error')});
}

// ===== Toggle Buttons =====
function toggleButton(type){
    let btn=document.getElementById(type=='toggle'?'toggleBtn':'holdBtn');
    btn.innerHTML='⏳...';
    btn.disabled=true;
    fetch('/togglebutton?type='+type).then(()=>{
        updateButtonStatus();
        btn.innerHTML=type=='toggle'?'🔄 دکمه آینه':'🔘 دکمه رله 5';
        btn.disabled=false;
        let msg=type=='toggle'?'دکمه آینه':'دکمه رله 5';
        showNotification('🔄','تغییر وضعیت',msg+' تغییر کرد ✅','info');
    });
}
function updateButtonStatus(){
    fetch('/getbuttonstatus').then(r=>r.json()).then(data=>{
        document.getElementById('toggleStatusText').innerText=data.toggleEnabled?'فعال ✅':'غیرفعال ❌';
        document.getElementById('toggleStatusText').className=data.toggleEnabled?'status-badge on':'status-badge off';
        document.getElementById('toggleBtn').className=data.toggleEnabled?'btn active':'btn inactive';
        document.getElementById('holdStatusText').innerText=data.holdEnabled?'فعال ✅':'غیرفعال ❌';
        document.getElementById('holdStatusText').className=data.holdEnabled?'status-badge on':'status-badge off';
        document.getElementById('holdBtn').className=data.holdEnabled?'btn active':'btn inactive';
    });
}

// ===== Hold Start/Stop =====
function holdStart(pin){fetch('/holdStart?pin='+pin);if(navigator.vibrate)navigator.vibrate(20)}
function holdStop(pin){fetch('/holdStop?pin='+pin)}

// ===== کنترل بوق رله 8 =====

function toggleBeep(){
    if(!isAdminUser){
        showNotification('⚠️','دسترسی محدود','فقط ادمین میتواند این گزینه را تغییر دهد!','warning');
        return;
    }
    const btn=document.getElementById('beepToggleBtn');
    btn.innerHTML='⏳...';
    btn.disabled=true;
    fetch('/togglebeep').then(response=>response.text()).then(data=>{
        const isEnabled=data==='ON';
        updateBeepStatus(isEnabled);
        showNotification('🔊','بوق رله 8',isEnabled?'بوق فعال شد ✅':'بوق غیرفعال شد ❌',isEnabled?'success':'warning');
        btn.innerHTML='🔊 بوق رله 8';
        btn.disabled=false;
    }).catch(()=>{
        showNotification('❌','خطا','تغییر وضعیت بوق انجام نشد!','error');
        btn.innerHTML='🔊 بوق رله 8';
        btn.disabled=false;
    });
}

function updateBeepStatus(isEnabled){
    const badge=document.getElementById('beepStatusBadge');
    const btn=document.getElementById('beepToggleBtn');
    if(isEnabled){
        badge.innerHTML='🔊 فعال';
        badge.style.background='rgba(0,214,143,0.15)';
        badge.style.color='#00d68f';
        btn.className='btn btn-success';
        btn.innerHTML='🔊 بوق فعال - غیرفعال کن';
    }else{
        badge.innerHTML='🔇 غیرفعال';
        badge.style.background='rgba(255,61,113,0.15)';
        badge.style.color='#ff3d71';
        btn.className='btn btn-danger';
        btn.innerHTML='🔇 بوق غیرفعال - فعال کن';
    }
}

// ===== Logout & Reset =====
function logout(){window.location.href='/logout'}
function resetSystem(){if(confirm('⚠️ آیا مطمئن هستید؟ سیستم ریست خواهد شد!')){showNotification('🔄','ریست سیستم','سیستم در حال ریست شدن...','warning');fetch('/reset')}}

// ===== Update Status =====
function updateStatus(){
    fetch('/status').then(r=>r.text()).then(html=>{
        let parser=new DOMParser();
        let doc=parser.parseFromString(html,'text/html');
        let items=doc.querySelectorAll('.status-item');
        let ids=['doorStatus','accStatus','carStatus','sportStatus','seatStatus','flasherStatus','photocellStatus','keylessStatus'];
        items.forEach((item,i)=>{if(i<ids.length){let val=item.querySelector('.status-value');if(val)document.getElementById(ids[i]).innerText=val.innerText}});
    });
    fetch('/getFullLock').then(r=>r.json()).then(data=>{
        let el=document.getElementById('fullLockStatus');
        if(el){el.innerText=data.fullLock?'🔒 فعال':'🔓 غیرفعال';el.style.color=data.fullLock?'#ff3d71':'#00d68f';el.className=data.fullLock?'full-lock-active':'full-lock-inactive'}
        let doorEl=document.getElementById('doorStatus');
        if(doorEl){doorEl.innerText=data.doorLock?'🔒 قفل':'🔓 باز';doorEl.className=data.doorLock?'status-value locked':'status-value unlocked'}
        let doorMain=document.getElementById('doorMainStatus');
        if(doorMain){doorMain.innerText=data.doorLock?'🔒 قفل':'🔓 باز'}
    });
    fetch('/getUserInfo').then(r=>r.json()).then(data=>{
        document.getElementById('userNameDisplay').innerText=data.name;
        if(data.isAdmin){
            document.getElementById('adminBadge').style.display='inline-block';
            document.getElementById('photocellCard').classList.remove('disabled');
            document.getElementById('settingsBtn').style.display='block';
            isAdminUser=true;
        }else{
            document.getElementById('adminBadge').style.display='none';
            document.getElementById('photocellCard').classList.add('disabled');
            document.getElementById('settingsBtn').style.display='none';
            isAdminUser=false;
        }
    });
    fetch('/getbeepstatus').then(r=>r.json()).then(data=>{
        updateBeepStatus(data.enabled);
        document.getElementById('beepDurationDisplay').innerText=data.duration+'ms';
    });
    updateButtonStatus();
}

// ===== Scroll to Top =====
let isAdminUser=false;
window.addEventListener('scroll',function(){const scrollBtn=document.getElementById('scrollTop');if(window.scrollY>300){scrollBtn.classList.add('show')}else{scrollBtn.classList.remove('show')}});
setInterval(updateStatus,1000);
updateStatus();
setTimeout(()=>{showNotification('👋','خوش آمدید','به سیستم کنترل هوشمند N Pixel خوش آمدید!','info',3000)},1000);
</script>
</body></html>
)rawliteral";

const char settings_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>تنظیمات - N Pixel</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:radial-gradient(ellipse at 30% 40%,#0a0f1a,#050608);color:#fff;font-family:'Segoe UI',sans-serif;padding:20px}
.container{max-width:600px;margin:0 auto}
.header{display:flex;align-items:center;justify-content:space-between;background:rgba(20,30,45,0.8);backdrop-filter:blur(20px);border-radius:20px;padding:12px 18px;margin-bottom:15px;border:1px solid rgba(0,230,255,0.1)}
.settings-icon{font-size:2rem;animation:spin 4s linear infinite}@keyframes spin{from{transform:rotate(0)}to{transform:rotate(360deg)}}
h1{font-size:1.2rem;background:linear-gradient(135deg,#fff,#00e6ff);-webkit-background-clip:text;background-clip:text;color:transparent;font-weight:700}
.back-btn{background:rgba(0,230,255,0.15);padding:5px 15px;border-radius:20px;cursor:pointer;transition:0.3s;border:1px solid rgba(0,230,255,0.1)}
.back-btn:hover{background:rgba(0,230,255,0.3)}
.status-card{background:linear-gradient(145deg,#0e141c,#080c12);border-radius:16px;padding:12px;margin-bottom:15px;border:1px solid rgba(0,230,255,0.05)}
.status-row{display:flex;justify-content:space-between;padding:8px 0}
.glass-card{background:rgba(18,25,35,0.6);backdrop-filter:blur(12px);border-radius:18px;padding:14px;margin-bottom:12px;border:1px solid rgba(0,230,255,0.05)}
.card-header{display:flex;align-items:center;gap:10px;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid rgba(0,230,255,0.15)}
.card-header .icon{font-size:1.2rem;background:rgba(0,230,255,0.1);padding:6px 10px;border-radius:10px}
.card-header h3{font-size:0.85rem;color:#00e6ff}
.setting-row{margin-bottom:15px}
.setting-row label{display:block;margin-bottom:6px;font-size:0.75rem;color:#8a9aa8}
.setting-row input{width:100%;padding:12px;background:rgba(0,0,0,0.4);border:1px solid rgba(0,230,255,0.2);border-radius:12px;color:#fff;transition:0.3s}
.setting-row input:focus{outline:none;border-color:#00e6ff;box-shadow:0 0 20px rgba(0,230,255,0.1)}
.toggle-switch{display:flex;justify-content:space-between;align-items:center;padding:10px 0;margin-bottom:10px}
.toggle-checkbox{position:relative;width:50px;height:24px;appearance:none;background:rgba(255,255,255,0.1);border-radius:30px;cursor:pointer;transition:0.3s}
.toggle-checkbox:checked{background:#00d68f}
.toggle-checkbox::before{content:'';position:absolute;width:20px;height:20px;background:#fff;border-radius:50%;top:2px;left:4px;transition:0.3s}
.toggle-checkbox:checked::before{left:26px}
.btn{background:linear-gradient(145deg,#1a2533,#10161f);border:none;color:#fff;padding:14px;border-radius:14px;cursor:pointer;width:100%;transition:0.3s}
.btn:hover{opacity:0.8}
.btn-primary{background:linear-gradient(145deg,#00ccee,#0099bb);color:#000}
.grid-2{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
.footer{text-align:center;margin-top:20px;padding:15px;font-size:0.6rem;color:#4a5a6a;border-top:1px solid rgba(0,230,255,0.1)}
</style>
</head>
<body>
<div class="container">
<div class="header"><div style="display:flex;align-items:center;gap:10px"><div class="settings-icon">⚙️</div><h1>تنظیمات پیشرفته</h1></div><div class="back-btn" onclick="window.location.href='/control'">🔙 بازگشت</div></div>
<div class="status-card"><div class="status-row"><span>📸 نور فعلی فوتوسل:</span><span id="lightLevel">---</span></div><div class="status-row"><span>🔌 وضعیت رله:</span><span id="relayState">---</span></div></div>
<div class="glass-card"><div class="card-header"><span class="icon">🪑</span><h3>تنظیمات صندلی</h3></div><div class="toggle-switch"><span>🚗 حرکت خودکار صندلی به عقب هنگام قفل</span><input type="checkbox" id="seatAutoEnable" class="toggle-checkbox"></div><div class="setting-row"><label>⏱️ زمان حرکت به عقب (ثانیه)</label><input type="number" id="backwardTime" min="1" max="30"></div><div class="setting-row"><label>⏱️ زمان حرکت به جلو (ثانیه)</label><input type="number" id="forwardTime" min="1" max="30"></div><div class="setting-row"><label>😴 زمان حالت خواب (ثانیه)</label><input type="number" id="sleepTime" min="1" max="30"></div><div class="setting-row"><label>🔆 زمان حالت بیدار (ثانیه)</label><input type="number" id="wakeTime" min="1" max="30"></div><button class="btn btn-primary" onclick="saveSeatSettings()">💾 ذخیره تنظیمات صندلی</button></div>
<div class="glass-card"><div class="card-header"><span class="icon">📸</span><h3>تنظیمات فوتوسل</h3></div><div class="toggle-switch"><span>🔄 فعال بودن حالت خودکار</span><input type="checkbox" id="ldrEnabled" class="toggle-checkbox"></div><div class="setting-row"><label>☀️ آستانه روشن شدن</label><input type="number" id="thOn" min="0" max="4095"></div><div class="setting-row"><label>🌙 آستانه خاموش شدن</label><input type="number" id="thOff" min="0" max="4095"></div><div class="setting-row"><label>⏱️ تأخیر روشن شدن (ثانیه)</label><input type="number" id="dOn" min="0" max="60"></div><div class="setting-row"><label>⏱️ تأخیر خاموش شدن (ثانیه)</label><input type="number" id="dOff" min="0" max="60"></div><div class="grid-2"><button class="btn btn-primary" onclick="sendCommand('/pcon')">💡 روشن اجباری</button><button class="btn btn-primary" onclick="sendCommand('/pcoff')">💡 خاموش اجباری</button><button class="btn btn-primary" onclick="sendCommand('/pcauto')">🔄 حالت خودکار</button></div></div>
<div class="glass-card"><div class="card-header"><span class="icon">🔊</span><h3>تنظیمات بوق رله 8</h3></div><div class="toggle-switch"><span>🔊 فعال بودن بوق</span><input type="checkbox" id="beepEnabled" class="toggle-checkbox"></div><div class="setting-row"><label>⏱️ مدت زمان هر بوق (میلی‌ثانیه) - محدوده ۵۰ تا ۱۰۰۰</label><input type="number" id="beepDuration" min="50" max="1000" value="200"><div style="text-align:center;margin-top:5px;font-size:0.7rem;color:#6a8a9a;">زمان فعلی: <span id="beepDurationDisplay">200</span>ms</div></div><button class="btn btn-primary" onclick="saveBeepSettings()">💾 ذخیره تنظیمات بوق</button></div>
<div class="footer">⚡ N PIXEL • تنظیمات پیشرفته</div>
</div>
<script>
function sendCommand(url){fetch(url).catch(e=>console.log(e));setTimeout(()=>{loadSettings();updatePCStatus();},500);}
function loadSettings(){fetch('/getallsettings').then(r=>r.json()).then(data=>{document.getElementById('seatAutoEnable').checked=data.seatAuto;document.getElementById('backwardTime').value=data.seatBackward;document.getElementById('forwardTime').value=data.seatForward;document.getElementById('sleepTime').value=data.sleepTime;document.getElementById('wakeTime').value=data.wakeTime;document.getElementById('ldrEnabled').checked=data.ldrEnabled;document.getElementById('thOn').value=data.thOn;document.getElementById('thOff').value=data.thOff;document.getElementById('dOn').value=data.dOn;document.getElementById('dOff').value=data.dOff;}).catch(e=>console.log(e));
fetch('/getbeepstatus').then(r=>r.json()).then(data=>{document.getElementById('beepEnabled').checked=data.enabled;document.getElementById('beepDuration').value=data.duration;document.getElementById('beepDurationDisplay').innerText=data.duration+'ms';});}
function saveSeatSettings(){let auto=document.getElementById('seatAutoEnable').checked;let fwd=document.getElementById('forwardTime').value;let bwd=document.getElementById('backwardTime').value;let sleep=document.getElementById('sleepTime').value;let wake=document.getElementById('wakeTime').value;fetch('/setseatsettings?auto='+auto+'&fwd='+fwd+'&bwd='+bwd+'&sleep='+sleep+'&wake='+wake).then(()=>loadSettings());}
function saveLDRSettings(){let en=document.getElementById('ldrEnabled').checked;let thOn=document.getElementById('thOn').value;let thOff=document.getElementById('thOff').value;let dOn=document.getElementById('dOn').value;let dOff=document.getElementById('dOff').value;fetch('/setldr?en='+en+'&thOn='+thOn+'&thOff='+thOff+'&dOn='+dOn+'&dOff='+dOff).then(()=>loadSettings());}
function saveBeepSettings(){const enabled=document.getElementById('beepEnabled').checked;const duration=document.getElementById('beepDuration').value;if(duration<50||duration>1000){alert('⚠️ مدت زمان باید بین ۵۰ تا ۱۰۰۰ میلی‌ثانیه باشد!');return;}
fetch('/togglebeep').then(()=>{fetch('/setbeepduration?duration='+duration).then(()=>{loadSettings();showToast('✅ تنظیمات بوق ذخیره شد!','success');});});}
function updatePCStatus(){fetch('/pcstatus').then(r=>r.json()).then(data=>{document.getElementById('lightLevel').innerHTML=data.light+' ('+data.percent+'%)';document.getElementById('relayState').innerHTML=data.state?'💡 روشن':'🌑 خاموش';}).catch(e=>console.log(e));}
function showToast(message,type='success'){const oldMsg=document.querySelector('.toast-message');if(oldMsg)oldMsg.remove();const toast=document.createElement('div');toast.className='toast-message toast-'+type;toast.innerHTML=message;document.body.appendChild(toast);setTimeout(()=>{toast.style.opacity='0';setTimeout(()=>toast.remove(),300)},3000);}
document.getElementById('ldrEnabled').addEventListener('change',saveLDRSettings);
document.getElementById('thOn').addEventListener('change',saveLDRSettings);
document.getElementById('thOff').addEventListener('change',saveLDRSettings);
document.getElementById('dOn').addEventListener('change',saveLDRSettings);
document.getElementById('dOff').addEventListener('change',saveLDRSettings);
document.getElementById('beepEnabled').addEventListener('change',saveBeepSettings);
document.getElementById('beepDuration').addEventListener('change',saveBeepSettings);
setInterval(updatePCStatus,1000);loadSettings();updatePCStatus();
</script>
</body></html>
)rawliteral";

// ==========================================
// ===== تابع بوق زدن رله 8 (کاپوت) =====
// ==========================================

void beepRelay8(int count) {
    // اگر بوق غیرفعال باشه، کاری نکن
    if(!relay8BeepEnabled) {
        Serial.println("🔇 RELAY8 Beep disabled");
        return;
    }
    
    for(int i = 0; i < count; i++) {
        digitalWrite(RELAY8, LOW);
        delay(relay8BeepDuration);  // استفاده از متغیر تنظیمات
        digitalWrite(RELAY8, HIGH);
        if(i < count - 1) delay(relay8BeepDuration);
    }
    
    // اگر بوق برای باز کردن بود (2 بار)، دیگه تا قفل شدن بوق نزنه
    if(count == 2) {
        relay8BeepEnabled = false;
        Serial.println("🔇 RELAY8 Beep disabled until lock");
    }
}

// ==========================================
// ===== هندلرهای کنترل بوق رله 8 =====
// ==========================================

void handleToggleBeep() {
    if(!loggedIn || !isAdmin) {
        server.send(403, "text/plain", "Forbidden");
        return;
    }
    
    relay8BeepEnabled = !relay8BeepEnabled;
    saveSettings();
    server.send(200, "text/plain", relay8BeepEnabled ? "ON" : "OFF");
    Serial.println("🔊 RELAY8 Beep: " + String(relay8BeepEnabled ? "ENABLED" : "DISABLED"));
}

void handleGetBeepStatus() {
    if(!loggedIn) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    String json = "{";
    json += "\"enabled\":" + String(relay8BeepEnabled ? "true" : "false") + ",";
    json += "\"duration\":" + String(relay8BeepDuration);
    json += "}";
    server.send(200, "application/json", json);
}

void handleSetBeepDuration() {
    if(!loggedIn || !isAdmin) {
        server.send(403, "text/plain", "Forbidden");
        return;
    }
    
    if(server.hasArg("duration")) {
        int duration = server.arg("duration").toInt();
        if(duration >= 50 && duration <= 1000) {  // محدوده 50 تا 1000 میلی‌ثانیه
            relay8BeepDuration = duration;
            saveSettings();
            server.send(200, "text/plain", "OK");
            Serial.println("🔊 RELAY8 Beep duration set to: " + String(duration) + "ms");
        } else {
            server.send(400, "text/plain", "Invalid duration (50-1000ms)");
        }
    } else {
        server.send(400, "text/plain", "Missing duration");
    }
}

// ==========================================
// ===== توابع =====
// ==========================================

void start_led(int num, int duration_ms) { 
    for(int i=0;i<num;i++){ 
        PCF.write(START_LED, LOW); 
        delay(duration_ms); 
        PCF.write(START_LED, HIGH); 
        delay(duration_ms); 
    } 
}

void turnOffHighBeam() {
    if(relay6State){
        digitalWrite(RELAY6, HIGH);
        relay6State = false;
        highBeamState = false;
        highBeamTimerActive = false;
        Serial.println("💡 High Beam turned OFF");
        delay(100);
    }
}

void moveDriverSeat(bool forward, int seconds) {
    if(seat_left_up || seat_left_down){ 
        PCF.write(SEAT_LEFT_UP, HIGH); 
        PCF.write(SEAT_LEFT_DOWN, HIGH); 
        seat_left_up=false; 
        seat_left_down=false; 
        delay(100); 
    }
    if(forward){ 
        PCF.write(SEAT_LEFT_UP, LOW); 
        seat_left_up=true; 
        seatMovingForward=true; 
        seatMoveStartTime=millis(); 
    }
    else{ 
        PCF.write(SEAT_LEFT_DOWN, LOW); 
        seat_left_down=true; 
        seatMovingBackward=true; 
        seatMoveStartTime=millis(); 
    }
}

void stopDriverSeat() { 
    PCF.write(SEAT_LEFT_UP, HIGH); 
    PCF.write(SEAT_LEFT_DOWN, HIGH); 
    seat_left_up=false; 
    seat_left_down=false; 
    seatMovingForward=false; 
    seatMovingBackward=false; 
}

void stopAllSeats() {
    if(seat_left_up || seat_left_down){ 
        PCF.write(SEAT_LEFT_UP, HIGH); 
        PCF.write(SEAT_LEFT_DOWN, HIGH); 
        seat_left_up=false; 
        seat_left_down=false; 
    }
    if(seat_right_up || seat_right_down){ 
        PCF.write(SEAT_RIGHT_UP, HIGH); 
        PCF.write(SEAT_RIGHT_DOWN, HIGH); 
        seat_right_up=false; 
        seat_right_down=false; 
    }
    driverSleepMode=false; 
    passengerSleepMode=false; 
    driverWakeMode=false; 
    passengerWakeMode=false; 
    seatMovingForward=false; 
    seatMovingBackward=false;
}

void checkSeatMovements() {
    unsigned long currentTime = millis();
    if(seatMovingForward && (currentTime - seatMoveStartTime >= seatForwardTime * 1000)) stopDriverSeat();
    if(seatMovingBackward && (currentTime - seatMoveStartTime >= seatBackwardTime * 1000)) stopDriverSeat();
    if(driverSleepMode && (currentTime - driverSleepStartTime >= sleepSeatTime * 1000)){ 
        PCF.write(SEAT_LEFT_DOWN, HIGH); 
        seat_left_down=false; 
        driverSleepMode=false; 
    }
    if(passengerSleepMode && (currentTime - passengerSleepStartTime >= sleepSeatTime * 1000)){ 
        PCF.write(SEAT_RIGHT_DOWN, HIGH); 
        seat_right_down=false; 
        passengerSleepMode=false; 
    }
    if(driverWakeMode && (currentTime - driverWakeStartTime >= wakeSeatTime * 1000)){ 
        PCF.write(SEAT_LEFT_UP, HIGH); 
        seat_left_up=false; 
        driverWakeMode=false; 
    }
    if(passengerWakeMode && (currentTime - passengerWakeStartTime >= wakeSeatTime * 1000)){ 
        PCF.write(SEAT_RIGHT_UP, HIGH); 
        seat_right_up=false; 
        passengerWakeMode=false; 
    }
}

void controlKeylessLed() {
    unsigned long currentTime = millis();
    if(carRun){ 
        if(keylessLedState){ 
            PCF.write(START_LED, HIGH); 
            keylessLedState=false; 
        } 
        keylessLedBlinkMode=false; 
        return; 
    }
    if(!DoorLock && !carRun){ 
        if(!keylessLedState){ 
            PCF.write(START_LED, LOW); 
            keylessLedState=true; 
        } 
        keylessLedBlinkMode=false; 
        return; 
    }
    if(DoorLock && !carRun){ 
        keylessLedBlinkMode=true; 
        if(currentTime - lastKeylessLedToggle >= 3000){ 
            lastKeylessLedToggle=currentTime; 
            keylessLedState=!keylessLedState; 
            PCF.write(START_LED, keylessLedState?LOW:HIGH); 
        } 
        return; 
    }
}

void checkHighBeamTimer() { 
    if(highBeamTimerActive && (millis()-highBeamTimerStart>=HIGH_BEAM_TIMEOUT)){ 
        digitalWrite(RELAY6, HIGH); 
        relay6State=false; 
        highBeamState=false; 
        highBeamTimerActive=false; 
    } 
}

void checkAccStatus() { 
    static bool lastAccOn=false; 
    if(!accOn && lastAccOn){ 
        carRun=false; 
        startState=0; 
    } 
    lastAccOn=accOn; 
}

void checkHoldButton() {
    if (fullLockMode) {
        if (relay5Active) {
            digitalWrite(RELAY5, HIGH);
            relay5Active = false;
        }
        return;
    }
    
    if (!holdButtonEnabled || !accOn) {
        if (relay5Active) {
            digitalWrite(RELAY5, HIGH);
            relay5Active = false;
        }
        return;
    }
    
    bool currentState = digitalRead(PIN_HOLD_BUTTON);
    
    if (currentState == HIGH && lastHoldState == LOW) {
        Serial.println("🔘 PIN 15: Connection BROKEN - RELAY5 ON");
        digitalWrite(RELAY5, LOW);
        relay5Active = true;
    }
    
    if (currentState == LOW && lastHoldState == HIGH) {
        Serial.println("🔘 PIN 15: Connection RESTORED - RELAY5 OFF");
        digitalWrite(RELAY5, HIGH);
        relay5Active = false;
    }
    
    lastHoldState = currentState;
}

void checkToggleButton() {
    if (fullLockMode) {
        return;
    }
    
    if (!toggleButtonEnabled || !accOn) {
        return;
    }
    
    bool currentState = digitalRead(PIN_TOGGLE_BUTTON);
    
    if (currentState == HIGH && lastToggleState == LOW) {
        
        if (toggleState == 0) {
            Serial.println("🔘 آینه‌ها باز میشن (RELAY 1)");
            digitalWrite(RELAY1, LOW);
            delay(500);
            digitalWrite(RELAY1, HIGH);
            Serial.println("✅ آینه‌ها باز شدند");
            toggleState = 1;
        } else {
            Serial.println("🔘 آینه‌ها بسته میشن (RELAY 2)");
            digitalWrite(RELAY2, LOW);
            delay(500);
            digitalWrite(RELAY2, HIGH);
            Serial.println("✅ آینه‌ها بسته شدند");
            toggleState = 0;
        }
    }
    
    lastToggleState = currentState;
}

// ==========================================
// ===== تابع پردازش UDP از ESP8266 (RFID) =====
// ==========================================

void processUDPCommand(String command) {
    command.trim();
    
    Serial.print("📡 UDP Command received: ");
    Serial.println(command);
    
    if (command == "REMOTE_START") {
        Serial.println("🚗 Remote START requested from ESP8266!");
        if (fullLockMode) {
            Serial.println("🔒 FULL LOCK - Command DENIED!");
            start_led(3, 100);
            return;
        }
        if (isCarInGear) {
            Serial.println("🚫 SAFETY BLOCK! Car is in gear!");
            start_led(5, 100);
            return;
        }
        if(!carRun && !accOn){
            Serial.println("▶️ Starting car from ESP8266 button");
            if(DoorLock){ 
                digitalWrite(RELAY1, LOW); 
                delay(500); 
                digitalWrite(RELAY1, HIGH); 
                DoorLock = false; 
                myServo.write(180); 
                delay(1000); 
            }
            digitalWrite(RELAY4, LOW); 
            accOn = true; 
            startState = 1;
            if(seatAutoAdjustEnabled && !seatForwardPerformedThisAccCycle){ 
                moveDriverSeat(true, seatForwardTime); 
                seatForwardPerformedThisAccCycle = true; 
            }
            seatBackPerformedThisLockCycle = false; 
            delay(500);
            digitalWrite(RELAY5, LOW); 
            PCF.write(START_LED, LOW); 
            delay(1000); 
            digitalWrite(RELAY5, HIGH); 
            PCF.write(START_LED, HIGH);
            carRun = true; 
            startState = 2;
            start_led(3, 200);
            Serial.println("✅ Car STARTED by ESP8266 button");
        } else {
            Serial.println("⚠️ Car is already running!");
        }
        return;
    }
    else if (command == "REMOTE_STOP") {
        Serial.println("⏹️ Remote STOP requested from ESP8266!");
        if(carRun || accOn){ 
            digitalWrite(RELAY4, HIGH); 
            digitalWrite(RELAY5, HIGH); 
            PCF.write(START_LED, HIGH); 
            accOn = false; 
            carRun = false; 
            startState = 0; 
            starterActive = false; 
            stopDriverSeat();
            start_led(2, 200);
            Serial.println("⛔ Car STOPPED by ESP8266 button");
        } else {
            Serial.println("⚠️ Car is already OFF!");
        }
        return;
    }
    else if (command == "RELAY1") {
        Serial.println("🔑 RFID: Unlock Door (UDP)");
        if(fullLockMode){ 
            Serial.println("🔒 FULL LOCK - Command DENIED!");
            start_led(3, 100);
            return; 
        }
        if(carRun || accOn){
            digitalWrite(RELAY4, HIGH);
            digitalWrite(RELAY5, HIGH);
            PCF.write(START_LED, HIGH);
            accOn = false;
            carRun = false;
            startState = 0;
            starterActive = false;
            delay(100);
        }
        digitalWrite(RELAY1, LOW);
        delay(500);
        digitalWrite(RELAY1, HIGH);
        DoorLock = false;
        myServo.write(180);
        
        // ===== بوق دو بار (باز کردن) =====
        beepRelay8(2);
        
        start_led(2, 200);
        Serial.println("🔓 Door UNLOCKED by RFID (UDP)");
    }
    else if (command == "RELAY2") {
        Serial.println("🔑 RFID: Lock Door (UDP)");
        if(fullLockMode){ 
            Serial.println("🔒 FULL LOCK - Command DENIED!");
            start_led(3, 100);
            return; 
        }
        turnOffHighBeam();
        if(carRun || accOn){
            digitalWrite(RELAY4, HIGH);
            digitalWrite(RELAY5, HIGH);
            PCF.write(START_LED, HIGH);
            accOn = false;
            carRun = false;
            startState = 0;
            starterActive = false;
            delay(100);
        }
        digitalWrite(RELAY2, LOW);
        delay(500);
        digitalWrite(RELAY2, HIGH);
        delay(200);
        digitalWrite(RELAY2, LOW);
        delay(500);
        digitalWrite(RELAY2, HIGH);
        DoorLock = true;
        myServo.write(0);
        if(seatAutoAdjustEnabled && !seatBackPerformedThisLockCycle){
            moveDriverSeat(false, seatBackwardTime);
            seatBackPerformedThisLockCycle = true;
        }
        seatForwardPerformedThisAccCycle = false;
        
        // ===== ریست کردن حالت بوق =====
        relay8BeepEnabled = true;
        Serial.println("🔊 RELAY8 Beep re-enabled (locked)");
        
        // ===== بوق یک بار (قفل) =====
        beepRelay8(1);
        
        start_led(2, 200);
        Serial.println("🔒 Door LOCKED by RFID (UDP)");
    }
    else if (command == "UNAUTHORIZED") {
        Serial.println("❌ RFID: Unauthorized card!");
        start_led(3, 100);
    }
    else if (command.startsWith("UID:")) {
        String uid = command.substring(4);
        Serial.println("🆔 RFID UID: " + uid);
    }
    else if (command.startsWith("TURN:")) {
        int turn = command.substring(5).toInt();
        Serial.println("🔢 RFID Turn Number: " + String(turn));
    }
}

// ==========================================
// ===== تابع دریافت از ESP8266 (سریال - اختیاری) =====
// ==========================================

void checkESP8266Serial() {
    if(SerialESP8266.available()) {
        String data = SerialESP8266.readStringUntil('\n');
        data.trim();
        if(data.length() > 0) {
            Serial.print("📡 From Serial (ESP8266): ");
            Serial.println(data);
            processUDPCommand(data);
        }
    }
}

// ==========================================
// ===== RF Receiver Functions =====
// ==========================================

void processRFCommand(String code) {
    if (fullLockMode) {
        Serial.println("🔒 FULL LOCK ACTIVE - RF Command DENIED!");
        start_led(3, 100);
        return;
    }
    
    Serial.print("📡 RF Command received: ");
    Serial.println(code);
    
    if (code == RF_CODE_LOCK) {
        Serial.println("🔒 LOCK command");
        turnOffHighBeam();
        if(carRun || accOn){
            digitalWrite(RELAY4, HIGH);
            digitalWrite(RELAY5, HIGH);
            PCF.write(START_LED, HIGH);
            accOn = false;
            carRun = false;
            startState = 0;
            starterActive = false;
            delay(100);
        }
        digitalWrite(RELAY2, LOW);
        delay(500);
        digitalWrite(RELAY2, HIGH);
        delay(200);
        digitalWrite(RELAY2, LOW);
        delay(500);
        digitalWrite(RELAY2, HIGH);
        DoorLock = true;
        myServo.write(0);
        if(seatAutoAdjustEnabled && !seatBackPerformedThisLockCycle){ 
            moveDriverSeat(false, seatBackwardTime); 
            seatBackPerformedThisLockCycle = true; 
        }
        seatForwardPerformedThisAccCycle = false;
        
        // ===== ریست کردن حالت بوق =====
        relay8BeepEnabled = true;
        Serial.println("🔊 RELAY8 Beep re-enabled (locked)");
        
        // ===== بوق یک بار (قفل) =====
        beepRelay8(1);
        
        start_led(2, 200);
        Serial.println("✅ Door LOCKED by RF");
    }
    else if (code == RF_CODE_UNLOCK) {
        Serial.println("🔓 UNLOCK command");
        if(carRun || accOn){
            digitalWrite(RELAY4, HIGH);
            digitalWrite(RELAY5, HIGH);
            PCF.write(START_LED, HIGH);
            accOn = false;
            carRun = false;
            startState = 0;
            starterActive = false;
            delay(100);
        }
        digitalWrite(RELAY1, LOW);
        delay(500);
        digitalWrite(RELAY1, HIGH);
        DoorLock = false;
        myServo.write(180);
        
        // ===== بوق دو بار (باز کردن) =====
        beepRelay8(2);
        
        start_led(2, 200);
        Serial.println("✅ Door UNLOCKED by RF");
    }
    else if (code == RF_CODE_START) {
        Serial.println("🚗 START command");
        if (isCarInGear) {
            Serial.println("🚫 SAFETY BLOCK! Car is in gear!");
            start_led(5, 100);
            return;
        }
        if(!carRun && !accOn){
            Serial.println("▶️ Starting car by RF");
            if(DoorLock){ 
                digitalWrite(RELAY1, LOW); 
                delay(500); 
                digitalWrite(RELAY1, HIGH); 
                DoorLock = false; 
                myServo.write(180); 
                delay(1000); 
            }
            digitalWrite(RELAY4, LOW);
            accOn = true;
            Serial.println("⚡ ACC = ON (RELAY4 active)");
            startState = 1;
            if(seatAutoAdjustEnabled && !seatForwardPerformedThisAccCycle){ 
                moveDriverSeat(true, seatForwardTime); 
                seatForwardPerformedThisAccCycle = true; 
            }
            seatBackPerformedThisLockCycle = false; 
            delay(500);
            digitalWrite(RELAY5, LOW); 
            PCF.write(START_LED, LOW); 
            delay(1000); 
            digitalWrite(RELAY5, HIGH); 
            PCF.write(START_LED, HIGH);
            carRun = true; 
            startState = 2;
            start_led(3, 200);
            Serial.println("✅ Car STARTED by RF");
        } else {
            Serial.println("⏹️ Stopping car by RF");
            digitalWrite(RELAY4, HIGH);
            digitalWrite(RELAY5, HIGH);
            PCF.write(START_LED, HIGH);
            accOn = false;
            carRun = false;
            startState = 0;
            starterActive = false;
            stopDriverSeat();
            start_led(2, 200);
            Serial.println("⛔ ACC = OFF");
        }
    }
    else if (code == RF_CODE_TRUNK) {
        Serial.println("📦 TRUNK command");
        if(DoorLock) {
            if(carRun || accOn){ 
                digitalWrite(RELAY4, HIGH); 
                digitalWrite(RELAY5, HIGH); 
                PCF.write(START_LED, HIGH); 
                accOn = false; 
                carRun = false; 
                startState = 0; 
                starterActive = false; 
                stopDriverSeat(); 
                delay(500); 
            }
            digitalWrite(RELAY1, LOW); 
            delay(500); 
            digitalWrite(RELAY1, HIGH); 
            DoorLock = false; 
            myServo.write(180); 
            delay(800);
        }
        digitalWrite(RELAY7, LOW); 
        delay(500); 
        digitalWrite(RELAY7, HIGH);
        start_led(2, 200);
        Serial.println("✅ Trunk OPENED by RF");
    }
    else {
        Serial.println("❌ Unknown RF code: " + code);
    }
}

// ==========================================
// ===== Load/Save Settings =====
// ==========================================

void loadSettings() {
    preferences.begin("car-settings", false);
    seatAutoAdjustEnabled = preferences.getBool("seatAuto", true);
    seatForwardTime = preferences.getInt("seatFwd", 6);
    seatBackwardTime = preferences.getInt("seatBwd", 5);
    sleepSeatTime = preferences.getInt("sleepTime", 5);
    wakeSeatTime = preferences.getInt("wakeTime", 5);
    flasherEnabled = preferences.getBool("flasherEn", true);
    relay6Interval = preferences.getInt("flasherInt", 200);
    ldrEnabled = preferences.getBool("ldrEn", true);
    thresholdOn = preferences.getInt("thOn", 800);
    thresholdOff = preferences.getInt("thOff", 600);
    delayOn = preferences.getInt("dOn", 5);
    delayOff = preferences.getInt("dOff", 3);
    fullLockMode = preferences.getBool("fullLock", false);
    toggleButtonEnabled = preferences.getBool("toggleBtnEn", true);
    holdButtonEnabled = preferences.getBool("holdBtnEn", true);
    relay8BeepEnabled = preferences.getBool("beep8En", true);
    relay8BeepDuration = preferences.getInt("beep8Dur", 200);
    preferences.end();
}

void saveSettings() {
    preferences.begin("car-settings", false);
    preferences.putBool("seatAuto", seatAutoAdjustEnabled);
    preferences.putInt("seatFwd", seatForwardTime);
    preferences.putInt("seatBwd", seatBackwardTime);
    preferences.putInt("sleepTime", sleepSeatTime);
    preferences.putInt("wakeTime", wakeSeatTime);
    preferences.putBool("flasherEn", flasherEnabled);
    preferences.putInt("flasherInt", relay6Interval);
    preferences.putBool("ldrEn", ldrEnabled);
    preferences.putInt("thOn", thresholdOn);
    preferences.putInt("thOff", thresholdOff);
    preferences.putInt("dOn", delayOn);
    preferences.putInt("dOff", delayOff);
    preferences.putBool("fullLock", fullLockMode);
    preferences.putBool("toggleBtnEn", toggleButtonEnabled);
    preferences.putBool("holdBtnEn", holdButtonEnabled);
    preferences.putBool("beep8En", relay8BeepEnabled);
    preferences.putInt("beep8Dur", relay8BeepDuration);
    preferences.end();
}

// ==========================================
// ===== HTTP Handlers =====
// ==========================================

void handleRoot() { 
    server.send_P(200, "text/html", login_html); 
}

void handleCheck() {
    String username = server.arg("username");
    String password = server.arg("password");
    if(username == "hosein_win" && password == "1234") { 
        loggedIn = true; 
        currentUser = "hosein_win"; 
        isAdmin = true; 
        server.send_P(200, "text/html", control_html); 
    }
    else if(username == "hosein_mobile" && password == "1234") { 
        loggedIn = true; 
        currentUser = "hosein_mobile"; 
        isAdmin = true; 
        server.send_P(200, "text/html", control_html); 
    }
    else if((username == "fayze" || username == "siawash" || username == "mohammad") && password == "1234") { 
        loggedIn = true; 
        currentUser = username; 
        isAdmin = false; 
        server.send_P(200, "text/html", control_html); 
    }
    else { 
        server.send_P(200, "text/html", login_html); 
    }
}

void handleControl() { 
    if(loggedIn) { 
        server.send_P(200, "text/html", control_html); 
    } else { 
        server.send_P(200, "text/html", login_html); 
    } 
}

void handleSettings() { 
    if(loggedIn && isAdmin) { 
        server.send_P(200, "text/html", settings_html); 
    } else if(loggedIn && !isAdmin) { 
        server.send(403, "text/plain", "Forbidden - Admin only"); 
    } else { 
        server.send_P(200, "text/html", login_html); 
    } 
}

void handleLogout() { 
    loggedIn = false; 
    currentUser = ""; 
    isAdmin = false; 
    server.sendHeader("Location", "/"); 
    server.send(303); 
}

void handleGetUserInfo() { 
    if(loggedIn) { 
        String name; 
        if(currentUser == "hosein_win" || currentUser == "hosein_mobile") name = "Hosein Naderi"; 
        else if(currentUser == "fayze") name = "Fayze Naderi"; 
        else if(currentUser == "siawash") name = "Siawash Naderi"; 
        else name = "Mohammad Ghanbari"; 
        String json = "{\"name\":\"" + name + "\",\"isAdmin\":" + String(isAdmin ? "true" : "false") + "}"; 
        server.send(200, "application/json", json); 
    } else { 
        server.send(401, "text/plain", "Unauthorized"); 
    } 
}

String getStatus() {
    String s = "<div class='status-item'><span class='status-label'>وضعیت درب:</span> <span class='status-value ";
    s += DoorLock ? "off locked'>🔒 قفل" : "on unlocked'>🔓 باز";
    s += "</span></div><div class='status-item'><span class='status-label'>ACC:</span> <span class='status-value ";
    s += accOn ? "on'>⚡ روشن" : "off'>⚡ خاموش";
    s += "</span></div><div class='status-item'><span class='status-label'>موتور:</span> <span class='status-value ";
    s += carRun ? "on'>🔧 روشن" : "off'>🔧 خاموش";
    s += "</span></div><div class='status-item'><span class='status-label'>حالت:</span> <span class='status-value off'>🚗 معمولی</span></div><div class='status-item'><span class='status-label'>صندلی:</span> <span class='status-value'>";
    if(seatMovingForward) s += "⬆️ جلو";
    else if(seatMovingBackward) s += "⬇️ عقب";
    else s += "⏹️ متوقف";
    s += "</span></div><div class='status-item'><span class='status-label'>نور بالا:</span> <span class='status-value ";
    s += relay6State ? "on'>💡 روشن" : "off'>💡 خاموش";
    if(highBeamTimerActive) s += " ⏱️";
    s += "</span></div><div class='status-item'><span class='status-label'>فوتوسل:</span> <span class='status-value ";
    s += relay3State ? "on'>📸 روشن" : "off'>📸 خاموش";
    s += " (";
    s += ldrEnabled ? "🔄 خودکار" : "✋ دستی";
    s += ")</span></div><div class='status-item'><span class='status-label'>کیلس:</span> <span class='status-value ";
    if(DoorLock && keylessLedBlinkMode) s += "blink'>✨ چشمک زن";
    else if(!DoorLock && !carRun) s += "on'>✨ روشن";
    else if(carRun) s += "off'>✨ خاموش";
    else s += "off'>✨ خاموش";
    s += "</span></div>";
    return s;
}

void handleStatus() { 
    if(loggedIn) { 
        server.send(200, "text/html", getStatus()); 
    } else { 
        server.send(401, "text/plain", "Unauthorized"); 
    } 
}

// ==========================================
// ===== Sleep/Wake Handlers =====
// ==========================================

void handleSleepBoth() { 
    if(loggedIn){ 
        stopAllSeats(); 
        PCF.write(SEAT_LEFT_DOWN, LOW); 
        PCF.write(SEAT_RIGHT_DOWN, LOW); 
        driverSleepMode=true; 
        passengerSleepMode=true; 
        driverSleepStartTime=millis(); 
        passengerSleepStartTime=millis(); 
        seat_left_down=true; 
        seat_right_down=true; 
        server.send(200,"text/plain","SLEEP BOTH");
    } 
}

void handleSleepDriver() { 
    if(loggedIn){ 
        if(seat_left_up){ 
            PCF.write(SEAT_LEFT_UP, HIGH); 
            seat_left_up=false; 
        } 
        if(seat_left_down){ 
            PCF.write(SEAT_LEFT_DOWN, HIGH); 
            seat_left_down=false; 
        } 
        PCF.write(SEAT_LEFT_DOWN, LOW); 
        driverSleepMode=true; 
        driverSleepStartTime=millis(); 
        seat_left_down=true; 
        server.send(200,"text/plain","SLEEP DRIVER");
    } 
}

void handleSleepPassenger() { 
    if(loggedIn){ 
        if(seat_right_up){ 
            PCF.write(SEAT_RIGHT_UP, HIGH); 
            seat_right_up=false; 
        } 
        if(seat_right_down){ 
            PCF.write(SEAT_RIGHT_DOWN, HIGH); 
            seat_right_down=false; 
        } 
        PCF.write(SEAT_RIGHT_DOWN, LOW); 
        passengerSleepMode=true; 
        passengerSleepStartTime=millis(); 
        seat_right_down=true; 
        server.send(200,"text/plain","SLEEP PASSENGER");
    } 
}

void handleWakeBoth() { 
    if(loggedIn){ 
        stopAllSeats(); 
        PCF.write(SEAT_LEFT_UP, LOW); 
        PCF.write(SEAT_RIGHT_UP, LOW); 
        driverWakeMode=true; 
        passengerWakeMode=true; 
        driverWakeStartTime=millis(); 
        passengerWakeStartTime=millis(); 
        seat_left_up=true; 
        seat_right_up=true; 
        server.send(200,"text/plain","WAKE BOTH");
    } 
}

void handleWakeDriver() { 
    if(loggedIn){ 
        if(seat_left_up){ 
            PCF.write(SEAT_LEFT_UP, HIGH); 
            seat_left_up=false; 
        } 
        if(seat_left_down){ 
            PCF.write(SEAT_LEFT_DOWN, HIGH); 
            seat_left_down=false; 
        } 
        PCF.write(SEAT_LEFT_UP, LOW); 
        driverWakeMode=true; 
        driverWakeStartTime=millis(); 
        seat_left_up=true; 
        server.send(200,"text/plain","WAKE DRIVER");
    } 
}

void handleWakePassenger() { 
    if(loggedIn){ 
        if(seat_right_up){ 
            PCF.write(SEAT_RIGHT_UP, HIGH); 
            seat_right_up=false; 
        } 
        if(seat_right_down){ 
            PCF.write(SEAT_RIGHT_DOWN, HIGH); 
            seat_right_down=false; 
        } 
        PCF.write(SEAT_RIGHT_UP, LOW); 
        passengerWakeMode=true; 
        passengerWakeStartTime=millis(); 
        seat_right_up=true; 
        server.send(200,"text/plain","WAKE PASSENGER");
    } 
}

// ==========================================
// ===== Relay Handlers =====
// ==========================================

void handleRelay1Web() {
    if(loggedIn){
        if(fullLockMode){ server.send(403,"text/plain","سیستم قفل است"); return; }
        if(carRun || accOn){ digitalWrite(RELAY4, HIGH); digitalWrite(RELAY5, HIGH); PCF.write(START_LED, HIGH); accOn=false; carRun=false; startState=0; starterActive=false; delay(100); }
        digitalWrite(RELAY1, LOW); delay(500); digitalWrite(RELAY1, HIGH); DoorLock=false; myServo.write(180);
        
        // ===== بوق دو بار (باز کردن) =====
        beepRelay8(2);
        
        server.send(200,"text/plain","UNLOCKED");
    }
}

void handleRelay2Web() {
    if(loggedIn){
        if(fullLockMode){ server.send(403,"text/plain","سیستم قفل است"); return; }
        turnOffHighBeam();
        if(carRun || accOn){ digitalWrite(RELAY4, HIGH); digitalWrite(RELAY5, HIGH); PCF.write(START_LED, HIGH); accOn=false; carRun=false; startState=0; starterActive=false; delay(100); }
        digitalWrite(RELAY2, LOW); delay(500); digitalWrite(RELAY2, HIGH); delay(200); digitalWrite(RELAY2, LOW); delay(500); digitalWrite(RELAY2, HIGH); DoorLock=true; myServo.write(0);
        if(seatAutoAdjustEnabled && !seatBackPerformedThisLockCycle){ moveDriverSeat(false, seatBackwardTime); seatBackPerformedThisLockCycle=true; }
        seatForwardPerformedThisAccCycle=false;
        
        // ===== ریست کردن حالت بوق =====
        relay8BeepEnabled = true;
        Serial.println("🔊 RELAY8 Beep re-enabled (locked)");
        
        // ===== بوق یک بار (قفل) =====
        beepRelay8(1);
        
        server.send(200,"text/plain","LOCKED");
    }
}

void handleElement() { 
    if(loggedIn){ 
        if(fullLockMode){ server.send(403,"text/plain","سیستم قفل است"); return; } 
        element_left=!element_left; 
        PCF.write(ELEMENT_LEFT, element_left?LOW:HIGH); 
        server.send(200,"text/plain","OK"); 
    } 
}

// ==========================================
// ===== Pulse Handlers =====
// ==========================================

void handlePulse1() { 
    if(loggedIn){ 
        if(accOn && !fullLockMode){ 
            digitalWrite(RELAY1, LOW); 
            delay(500); 
            digitalWrite(RELAY1, HIGH); 
            server.send(200,"text/plain","OK"); 
        } else server.send(403,"text/plain","ACC OFF or FULL LOCK"); 
    } 
}

void handlePulse2() { 
    if(loggedIn){ 
        if(accOn && !fullLockMode){ 
            digitalWrite(RELAY2, LOW); 
            delay(500); 
            digitalWrite(RELAY2, HIGH); 
            server.send(200,"text/plain","OK"); 
        } else server.send(403,"text/plain","ACC OFF or FULL LOCK"); 
    } 
}

// ==========================================
// ===== Remote Start Web =====
// ==========================================

void handleRemoteStartWeb() {
    if(loggedIn){
        if (fullLockMode) {
            server.send(403, "text/plain", "FULL LOCK ACTIVE");
            return;
        }
        if (isCarInGear) {
            server.send(403, "text/plain", "CAR IN GEAR - START DENIED");
            start_led(5, 100);
            return;
        }
        if(!carRun && !accOn){
            if(DoorLock){ 
                digitalWrite(RELAY1, LOW); 
                delay(500); 
                digitalWrite(RELAY1, HIGH); 
                DoorLock=false; 
                myServo.write(180); 
                delay(1000); 
            }
            digitalWrite(RELAY4, LOW); 
            accOn=true; 
            startState=1;
            if(seatAutoAdjustEnabled && !seatForwardPerformedThisAccCycle){ 
                moveDriverSeat(true, seatForwardTime); 
                seatForwardPerformedThisAccCycle=true; 
            }
            seatBackPerformedThisLockCycle=false; 
            delay(500);
            digitalWrite(RELAY5, LOW); 
            PCF.write(START_LED, LOW); 
            delay(1000); 
            digitalWrite(RELAY5, HIGH); 
            PCF.write(START_LED, HIGH);
            carRun=true; 
            startState=2;
            server.send(200,"text/plain","Car started");
        } else server.send(200,"text/plain","Already on");
    }
}

void handleRemoteStopWeb() {
    if(loggedIn){
        if(carRun || accOn){ 
            digitalWrite(RELAY4, HIGH); 
            digitalWrite(RELAY5, HIGH); 
            PCF.write(START_LED, HIGH); 
            accOn=false; 
            carRun=false; 
            startState=0; 
            starterActive=false; 
            server.send(200,"text/plain","Car stopped"); 
        }
    }
}

// ==========================================
// ===== Trunk =====
// ==========================================

void handleOpenTrunkWithUnlock() {
    if(loggedIn){
        if(fullLockMode){ server.send(403,"text/plain","سیستم قفل است"); return; }
        if(DoorLock) {
            if(carRun || accOn){ 
                digitalWrite(RELAY4, HIGH); 
                digitalWrite(RELAY5, HIGH); 
                PCF.write(START_LED, HIGH); 
                accOn=false; 
                carRun=false; 
                startState=0; 
                starterActive=false; 
                stopDriverSeat(); 
                delay(500); 
            }
            digitalWrite(RELAY1, LOW); 
            delay(500); 
            digitalWrite(RELAY1, HIGH); 
            DoorLock=false; 
            myServo.write(180); 
            delay(800);
        }
        digitalWrite(RELAY7, LOW); 
        delay(500); 
        digitalWrite(RELAY7, HIGH);
        start_led(2,200);
        server.send(200,"text/plain","Trunk opened");
    }
}

// ==========================================
// ===== Full Lock =====
// ==========================================

void handleToggleFullLock() { 
    if(loggedIn && isAdmin){ 
        fullLockMode = !fullLockMode; 
        
        if(fullLockMode){
            if(carRun || accOn){
                digitalWrite(RELAY4, HIGH);
                digitalWrite(RELAY5, HIGH);
                PCF.write(START_LED, HIGH);
                accOn = false;
                carRun = false;
                startState = 0;
                starterActive = false;
                delay(100);
            }
            turnOffHighBeam();
            digitalWrite(RELAY2, LOW);
            delay(500);
            digitalWrite(RELAY2, HIGH);
            delay(200);
            digitalWrite(RELAY2, LOW);
            delay(500);
            digitalWrite(RELAY2, HIGH);
            DoorLock = true;
            myServo.write(0);
            
            // ===== ریست کردن حالت بوق =====
            relay8BeepEnabled = true;
            Serial.println("🔊 RELAY8 Beep re-enabled (locked)");
            
            // ===== بوق یک بار (قفل) =====
            beepRelay8(1);
            
            start_led(2, 200);
            Serial.println("🔒 FULL LOCK ACTIVATED - Door LOCKED");
        } else {
            digitalWrite(RELAY1, LOW);
            delay(500);
            digitalWrite(RELAY1, HIGH);
            DoorLock = false;
            myServo.write(180);
            
            // ===== بوق دو بار (باز کردن) =====
            beepRelay8(2);
            
            start_led(2, 200);
            Serial.println("🔓 FULL LOCK DEACTIVATED - Door UNLOCKED");
        }
        
        saveSettings(); 
        server.send(200, "text/plain", fullLockMode ? "true" : "false"); 
    } else { 
        server.send(403, "text/plain", "Forbidden - Admin only"); 
    } 
}

void handleGetFullLock() { 
    if(loggedIn) { 
        String json = "{\"fullLock\":" + String(fullLockMode ? "true" : "false") + ",\"doorLock\":" + String(DoorLock ? "true" : "false") + "}";
        server.send(200, "application/json", json); 
    } else {
        server.send(401, "text/plain", "Unauthorized");
    }
}

// ==========================================
// ===== High Beam =====
// ==========================================

void handleHighBeam() {
    if(!loggedIn){ server.send(401,"text/plain","Unauthorized"); return; }
    if(fullLockMode){ server.send(403,"text/plain","FULL LOCK ACTIVE"); return; }
    highBeamState = !highBeamState;
    if(highBeamState){ 
        digitalWrite(RELAY6, LOW); 
        relay6State=true; 
        highBeamTimerStart=millis(); 
        highBeamTimerActive=true; 
        server.send(200,"text/plain","ON"); 
    }
    else{ 
        digitalWrite(RELAY6, HIGH); 
        relay6State=false; 
        highBeamTimerActive=false; 
        server.send(200,"text/plain","OFF"); 
    }
}

// ==========================================
// ===== Reset =====
// ==========================================

void handleReset() {
    if(loggedIn && isAdmin) {
        server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='3;url=/'><style>body{background:#0a0f1a;color:#fff;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;text-align:center}</style></head><body><div><h1>🔄 سیستم در حال ریست...</h1><p>لطفا چند ثانیه صبر کنید</p></div></body></html>");
        delay(100);
        ESP.restart();
    } else {
        server.send(403, "text/plain", "Forbidden - Admin only");
    }
}

// ==========================================
// ===== Hold Start/Stop =====
// ==========================================

int pinFromName(const String &pinName) {
    if (pinName == "SEAT_LEFT_UP") return SEAT_LEFT_UP;
    if (pinName == "SEAT_LEFT_DOWN") return SEAT_LEFT_DOWN;
    if (pinName == "SEAT_RIGHT_UP") return SEAT_RIGHT_UP;
    if (pinName == "SEAT_RIGHT_DOWN") return SEAT_RIGHT_DOWN;
    return -1;
}

void handleHoldStart() {
    if (!loggedIn) { server.send(401, "text/plain", "Unauthorized"); return; }
    if (!server.hasArg("pin")) { server.send(400, "text/plain", "Missing pin"); return; }
    String pinName = server.arg("pin");
    int pin = pinFromName(pinName);
    if (pin < 0) { server.send(400, "text/plain", "Unknown pin"); return; }
    PCF.write(pin, LOW);
    if (pinName == "SEAT_LEFT_UP") seat_left_up = true;
    if (pinName == "SEAT_LEFT_DOWN") seat_left_down = true;
    if (pinName == "SEAT_RIGHT_UP") seat_right_up = true;
    if (pinName == "SEAT_RIGHT_DOWN") seat_right_down = true;
    server.send(200, "text/plain", "ON");
}

void handleHoldStop() {
    if (!loggedIn) { server.send(401, "text/plain", "Unauthorized"); return; }
    if (!server.hasArg("pin")) { server.send(400, "text/plain", "Missing pin"); return; }
    String pinName = server.arg("pin");
    int pin = pinFromName(pinName);
    if (pin < 0) { server.send(400, "text/plain", "Unknown pin"); return; }
    PCF.write(pin, HIGH);
    if (pinName == "SEAT_LEFT_UP") seat_left_up = false;
    if (pinName == "SEAT_LEFT_DOWN") seat_left_down = false;
    if (pinName == "SEAT_RIGHT_UP") seat_right_up = false;
    if (pinName == "SEAT_RIGHT_DOWN") seat_right_down = false;
    server.send(200, "text/plain", "OFF");
}

// ==========================================
// ===== Settings Handlers =====
// ==========================================

void handleSetSeatSettings() { 
    if(loggedIn && isAdmin && server.hasArg("auto") && server.hasArg("fwd") && server.hasArg("bwd") && server.hasArg("sleep") && server.hasArg("wake")){ 
        seatAutoAdjustEnabled = server.arg("auto") == "true"; 
        seatForwardTime = server.arg("fwd").toInt(); 
        seatBackwardTime = server.arg("bwd").toInt(); 
        sleepSeatTime = server.arg("sleep").toInt(); 
        wakeSeatTime = server.arg("wake").toInt(); 
        saveSettings(); 
        server.send(200,"text/plain","OK"); 
    } else { 
        server.send(403,"text/plain","Forbidden - Admin only"); 
    } 
}

void handleSetLDR() { 
    if(loggedIn && isAdmin && server.hasArg("en") && server.hasArg("thOn") && server.hasArg("thOff") && server.hasArg("dOn") && server.hasArg("dOff")){ 
        ldrEnabled = server.arg("en") == "true"; 
        thresholdOn = server.arg("thOn").toInt(); 
        thresholdOff = server.arg("thOff").toInt(); 
        delayOn = server.arg("dOn").toInt(); 
        delayOff = server.arg("dOff").toInt(); 
        saveSettings(); 
        server.send(200,"text/plain","OK"); 
    } else { 
        server.send(403,"text/plain","Forbidden - Admin only"); 
    } 
}

void handlePcOn() { 
    if(loggedIn && isAdmin){ 
        ldrEnabled=false; 
        digitalWrite(RELAY3, LOW); 
        relay3State=true; 
        saveSettings(); 
        server.send(200,"text/plain","OK"); 
    } else { 
        server.send(403,"text/plain","Forbidden - Admin only"); 
    } 
}

void handlePcOff() { 
    if(loggedIn && isAdmin){ 
        ldrEnabled=false; 
        digitalWrite(RELAY3, HIGH); 
        relay3State=false; 
        saveSettings(); 
        server.send(200,"text/plain","OK"); 
    } else { 
        server.send(403,"text/plain","Forbidden - Admin only"); 
    } 
}

void handlePcAuto() { 
    if(loggedIn && isAdmin){ 
        ldrEnabled=true; 
        saveSettings(); 
        server.send(200,"text/plain","OK"); 
    } else { 
        server.send(403,"text/plain","Forbidden - Admin only"); 
    } 
}

void handlePcStatus() { 
    if(loggedIn){ 
        int light = analogRead(LDR_PIN); 
        int percent = map(light,0,4095,0,100); 
        String json = "{\"state\":" + String(relay3State?"true":"false") + ",\"light\":" + String(light) + ",\"percent\":" + String(percent) + "}"; 
        server.send(200,"application/json",json); 
    } 
}

void handleGetAllSettings() { 
    if(loggedIn && isAdmin){ 
        String json = "{"; 
        json += "\"seatAuto\":" + String(seatAutoAdjustEnabled?"true":"false") + ","; 
        json += "\"seatForward\":" + String(seatForwardTime) + ","; 
        json += "\"seatBackward\":" + String(seatBackwardTime) + ","; 
        json += "\"sleepTime\":" + String(sleepSeatTime) + ","; 
        json += "\"wakeTime\":" + String(wakeSeatTime) + ","; 
        json += "\"flasherEnabled\":" + String(flasherEnabled?"true":"false") + ","; 
        json += "\"flasherInterval\":" + String(relay6Interval) + ","; 
        json += "\"ldrEnabled\":" + String(ldrEnabled?"true":"false") + ","; 
        json += "\"thOn\":" + String(thresholdOn) + ","; 
        json += "\"thOff\":" + String(thresholdOff) + ","; 
        json += "\"dOn\":" + String(delayOn) + ","; 
        json += "\"dOff\":" + String(delayOff) + ","; 
        json += "\"fullLock\":" + String(fullLockMode?"true":"false"); 
        json += "}"; 
        server.send(200,"application/json",json); 
    } else { 
        server.send(403,"text/plain","Forbidden - Admin only"); 
    } 
}

// ==========================================
// ===== Door Status & Gear =====
// ==========================================

void handleDoorStatus() { 
    String json = "{\"doorLock\":" + String(DoorLock ? "true" : "false") + "}"; 
    server.send(200, "application/json", json); 
}

void handleGearStatus() { 
    if (server.hasArg("inGear")) { 
        isCarInGear = server.arg("inGear") == "true"; 
        server.send(200, "text/plain", "OK"); 
    } else { 
        server.send(400, "text/plain", "Missing parameter"); 
    } 
}

void handleRemoteCommand() {
    if (server.hasArg("cmd")) {
        String cmd = server.arg("cmd");
        if (fullLockMode) {
            server.send(403, "text/plain", "FULL LOCK ACTIVE - DENIED");
            return;
        }
        if (cmd == "lock") {
            turnOffHighBeam();
            if(carRun || accOn){
                digitalWrite(RELAY4, HIGH);
                digitalWrite(RELAY5, HIGH);
                PCF.write(START_LED, HIGH);
                accOn = false;
                carRun = false;
                startState = 0;
                starterActive = false;
                delay(100);
            }
            digitalWrite(RELAY2, LOW);
            delay(500);
            digitalWrite(RELAY2, HIGH);
            delay(200);
            digitalWrite(RELAY2, LOW);
            delay(500);
            digitalWrite(RELAY2, HIGH);
            DoorLock = true;
            myServo.write(0);
            if(seatAutoAdjustEnabled && !seatBackPerformedThisLockCycle){ 
                moveDriverSeat(false, seatBackwardTime); 
                seatBackPerformedThisLockCycle = true; 
            }
            seatForwardPerformedThisAccCycle = false;
            
            // ===== ریست کردن حالت بوق =====
            relay8BeepEnabled = true;
            Serial.println("🔊 RELAY8 Beep re-enabled (locked)");
            
            // ===== بوق یک بار (قفل) =====
            beepRelay8(1);
            
            start_led(2, 200);
            server.send(200, "text/plain", "LOCKED");
        }
        else if (cmd == "unlock") {
            if(carRun || accOn){
                digitalWrite(RELAY4, HIGH);
                digitalWrite(RELAY5, HIGH);
                PCF.write(START_LED, HIGH);
                accOn = false;
                carRun = false;
                startState = 0;
                starterActive = false;
                delay(100);
            }
            digitalWrite(RELAY1, LOW);
            delay(500);
            digitalWrite(RELAY1, HIGH);
            DoorLock = false;
            myServo.write(180);
            
            // ===== بوق دو بار (باز کردن) =====
            beepRelay8(2);
            
            start_led(2, 200);
            server.send(200, "text/plain", "UNLOCKED");
        }
        else if (cmd == "start") {
            if (isCarInGear) {
                server.send(403, "text/plain", "CAR IN GEAR - START DENIED");
                start_led(5, 100);
                return;
            }
            if(!carRun && !accOn){
                if(DoorLock){ 
                    digitalWrite(RELAY1, LOW); 
                    delay(500); 
                    digitalWrite(RELAY1, HIGH); 
                    DoorLock = false; 
                    myServo.write(180); 
                    delay(1000); 
                }
                digitalWrite(RELAY4, LOW); 
                accOn = true; 
                startState = 1;
                if(seatAutoAdjustEnabled && !seatForwardPerformedThisAccCycle){ 
                    moveDriverSeat(true, seatForwardTime); 
                    seatForwardPerformedThisAccCycle = true; 
                }
                seatBackPerformedThisLockCycle = false; 
                delay(500);
                digitalWrite(RELAY5, LOW); 
                PCF.write(START_LED, LOW); 
                delay(1000); 
                digitalWrite(RELAY5, HIGH); 
                PCF.write(START_LED, HIGH);
                carRun = true; 
                startState = 2;
                start_led(3, 200);
                server.send(200, "text/plain", "STARTING");
            } else {
                server.send(200, "text/plain", "ALREADY RUNNING");
            }
        }
        else if (cmd == "trunk") {
            if(DoorLock) {
                if(carRun || accOn){ 
                    digitalWrite(RELAY4, HIGH); 
                    digitalWrite(RELAY5, HIGH); 
                    PCF.write(START_LED, HIGH); 
                    accOn = false; 
                    carRun = false; 
                    startState = 0; 
                    starterActive = false; 
                    stopDriverSeat(); 
                    delay(500); 
                }
                digitalWrite(RELAY1, LOW); 
                delay(500); 
                digitalWrite(RELAY1, HIGH); 
                DoorLock = false; 
                myServo.write(180); 
                delay(800);
            }
            digitalWrite(RELAY7, LOW); 
            delay(500); 
            digitalWrite(RELAY7, HIGH);
            start_led(2, 200);
            server.send(200, "text/plain", "TRUNK OPENED");
        }
        else {
            server.send(400, "text/plain", "Unknown command");
        }
    } else {
        server.send(400, "text/plain", "Missing cmd parameter");
    }
}

void handleServoLock() {
    if(loggedIn && isAdmin){
        myServo.write(0);
        start_led(2, 200);
        server.send(200, "text/plain", "Servo LOCKED");
        Serial.println("🔒 Servo locked to 0 degrees (Door UNCHANGED)");
    } else {
        server.send(403, "text/plain", "Forbidden");
    }
}

void handleServoUnlock() {
    if(loggedIn && isAdmin){
        myServo.write(180);
        start_led(2, 200);
        server.send(200, "text/plain", "Servo UNLOCKED");
        Serial.println("🔓 Servo unlocked to 180 degrees (Door UNCHANGED)");
    } else {
        server.send(403, "text/plain", "Forbidden");
    }
}

void handleToggleButtonWeb() {
    if (!loggedIn || !isAdmin) {
        server.send(403, "text/plain", "Forbidden");
        return;
    }
    if (server.hasArg("type")) {
        String type = server.arg("type");
        if (type == "toggle") {
            toggleButtonEnabled = !toggleButtonEnabled;
            saveSettings();
            server.send(200, "text/plain", toggleButtonEnabled ? "ON" : "OFF");
        } else if (type == "hold") {
            holdButtonEnabled = !holdButtonEnabled;
            saveSettings();
            server.send(200, "text/plain", holdButtonEnabled ? "ON" : "OFF");
        } else {
            server.send(400, "text/plain", "Invalid type");
        }
    } else {
        server.send(400, "text/plain", "Missing type");
    }
}

void handleGetButtonStatus() {
    if (!loggedIn) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    String json = "{";
    json += "\"toggleEnabled\":" + String(toggleButtonEnabled ? "true" : "false") + ",";
    json += "\"holdEnabled\":" + String(holdButtonEnabled ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

// ==========================================
// ===== Core Tasks =====
// ==========================================

void Core1Task(void * parameter) {
    while(true){
        checkSeatMovements(); 
        controlKeylessLed(); 
        checkAccStatus(); 
        checkHighBeamTimer();
        checkToggleButton();
        checkHoldButton();
        
        buttonFlasher = digitalRead(BUTTON_FLASHER_PIN);
        if(buttonFlasher == LOW && lastbuttonFlasher == HIGH) {
            pressFlashButton = millis();
            flasherActionDone = false;
        }
        if(buttonFlasher == LOW) {
            unsigned long held = millis() - pressFlashButton;
            if(relay6State && !flasherActionDone && held >= 200 && held <= 400) {
                digitalWrite(RELAY6, HIGH);
                relay6State = false;
                flasherActionDone = true;
            }
            if(!relay6State && !flasherActionDone && held >= 600 && held <= 700) {
                digitalWrite(RELAY6, LOW);
                relay6State = true;
                flasherActionDone = true;
            }
        }
        lastbuttonFlasher = buttonFlasher;
        
        buttonStart = digitalRead(BUTTON_START_PIN);
        if(!DoorLock && buttonStart == LOW && lastbuttonStart == HIGH) {
            pressTime = millis(); 
            pressHandled = false;
        }
        if(!DoorLock && buttonStart == LOW) {
            unsigned long holdTime = millis() - pressTime;
            if(startState == 0 && holdTime > 50 && holdTime < 500 && !pressHandled) {
                if (isCarInGear) {
                    Serial.println("🚫 Physical start BLOCKED - Car is in gear!");
                    start_led(5, 100);
                    pressHandled = true;
                } else {
                    digitalWrite(RELAY4, LOW); 
                    accOn = true; 
                    startState = 1;
                    if(seatAutoAdjustEnabled && !seatForwardPerformedThisAccCycle) {
                        moveDriverSeat(true, seatForwardTime); 
                        seatForwardPerformedThisAccCycle = true;
                    }
                    seatBackPerformedThisLockCycle = false; 
                    start_led(2,200); 
                    pressHandled = true;
                }
            }
            if(startState == 1 && !carRun && !starterActive) {
                digitalWrite(RELAY5, LOW); 
                starterActive = true;
            }
            if((startState == 1 || startState == 2) && holdTime > 500 && !pressHandled) {
                digitalWrite(RELAY4, HIGH); 
                digitalWrite(RELAY5, HIGH); 
                accOn = false; 
                carRun = false; 
                startState = 0; 
                starterActive = false; 
                stopDriverSeat(); 
                start_led(3,200); 
                pressHandled = true;
            }
        }
        if(!DoorLock && buttonStart == HIGH && lastbuttonStart == LOW) {
            unsigned long holdTime = millis() - pressTime;
            if(starterActive) { 
                digitalWrite(RELAY5, HIGH); 
                starterActive = false;
                if(startState == 1 && holdTime > 300) { 
                    carRun = true; 
                    startState = 2;
                    for(int i = 0; i < 3; i++) { 
                        PCF.write(START_LED, LOW); 
                        delay(200); 
                        PCF.write(START_LED, HIGH); 
                        delay(200); 
                    }
                }
            }
            pressHandled = false;
        }
        if(DoorLock && buttonStart == LOW && lastbuttonStart == HIGH) {
            turnOffHighBeam();
            start_led(3,200);
        }
        lastbuttonStart = buttonStart;
        
        int light = analogRead(LDR_PIN);
        if(ldrEnabled){
            if(light > thresholdOn) { 
                if(ldrOnTime == 0) ldrOnTime = millis(); 
                if(millis() - ldrOnTime >= delayOn * 1000) { 
                    digitalWrite(RELAY3, LOW); 
                    relay3State = true; 
                } 
                ldrOffTime = 0; 
            }
            else if(light < thresholdOff) { 
                if(ldrOffTime == 0) ldrOffTime = millis(); 
                if(millis() - ldrOffTime >= delayOff * 1000) { 
                    digitalWrite(RELAY3, HIGH); 
                    relay3State = false; 
                } 
                ldrOnTime = 0; 
            }
        }
        vTaskDelay(10);
    }
}

void Core2Task(void * parameter) {
    while(true){
        server.handleClient();
        
        // ===== دریافت UDP از ESP8266 (RFID) =====
        int packetSize = udp.parsePacket();
        if (packetSize) {
            char incomingPacket[255];
            int len = udp.read(incomingPacket, 255);
            if (len > 0) {
                incomingPacket[len] = 0;
            }
            String data = String(incomingPacket);
            data.trim();
            if (data.length() > 0) {
                processUDPCommand(data);
            }
        }
        
        // ===== دریافت از سریال (اختیاری) =====
        checkESP8266Serial();
        
        // ===== RF Receiver =====
        if (mySwitch.available()) {
            unsigned long value = mySwitch.getReceivedValue();
            unsigned int bitlen = mySwitch.getReceivedBitlength();
            unsigned int protocol = mySwitch.getReceivedProtocol();
            if (value != 0) {
                Serial.print("📡 RF Received: ");
                Serial.print(value);
                Serial.print(" | Bitlen: ");
                Serial.print(bitlen);
                Serial.print(" | Protocol: ");
                Serial.println(protocol);
                String received = String(value);
                processRFCommand(received);
            }
            mySwitch.resetAvailable();
        }
        
        vTaskDelay(10);
    }
}

// ==========================================
// ===== Setup =====
// ==========================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // ==========================================
    // ===== حداکثر قدرت WiFi =====
    // ==========================================
    esp_bt_controller_disable();
    esp_wifi_set_max_tx_power(84);  // حداکثر قدرت (0-84)
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40);  // پهنای باند 40MHz
    esp_wifi_set_ps(WIFI_PS_NONE);  // غیرفعال کردن Power Save
    
    // تنظیمات کشور برای حداکثر کانال‌ها
    wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    esp_wifi_set_country(&country);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    
    // ==========================================
    // ===== شروع WiFi AP =====
    // ==========================================
    WiFi.softAP("N Pixel", "13hFyab@#18haj8", 1, 0);  // channel 1, hidden=0
    esp_wifi_set_inactive_time(WIFI_IF_AP, 0);
    
    Serial.println("✅ WiFi AP Started with MAX Power");
    Serial.println("📡 SSID: N Pixel");
    Serial.println("🔑 Password: 13hFyab@#18haj8");
    Serial.print("🌐 IP: ");
    Serial.println(WiFi.softAPIP());
    
    // ==========================================
    // ===== شروع UDP Server =====
    // ==========================================
    udp.begin(udpPort);
    Serial.println("📡 UDP Server started on port: " + String(udpPort));
    Serial.println("📡 Waiting for RFID (ESP8266) on WiFi...");
    
    // ==========================================
    // ===== Serial2 (اختیاری) =====
    // ==========================================
    SerialESP8266.begin(115200, SERIAL_8N1, ESP8266_RX, ESP8266_TX);
    Serial.println("📡 Serial2 started on pins 33(RX) and 32(TX) for RFID (ESP8266) - Optional");
    
    // ==========================================
    // ===== Relay Pins =====
    // ==========================================
    pinMode(RELAY1, OUTPUT); digitalWrite(RELAY1, HIGH);
    pinMode(RELAY2, OUTPUT); digitalWrite(RELAY2, HIGH);
    pinMode(RELAY3, OUTPUT); digitalWrite(RELAY3, HIGH);
    pinMode(RELAY4, OUTPUT); digitalWrite(RELAY4, HIGH);
    pinMode(RELAY5, OUTPUT); digitalWrite(RELAY5, HIGH);
    pinMode(RELAY6, OUTPUT); digitalWrite(RELAY6, HIGH);
    pinMode(RELAY7, OUTPUT); digitalWrite(RELAY7, HIGH);
    pinMode(RELAY8, OUTPUT); digitalWrite(RELAY8, HIGH);

    pinMode(BUTTON_START_PIN, INPUT_PULLUP);
    pinMode(BUTTON_FLASHER_PIN, INPUT_PULLUP);

    pinMode(PIN_TOGGLE_BUTTON, INPUT_PULLUP);
    lastToggleState = digitalRead(PIN_TOGGLE_BUTTON);

    pinMode(PIN_HOLD_BUTTON, INPUT_PULLUP);
    lastHoldState = digitalRead(PIN_HOLD_BUTTON);

    pinMode(RF_RX_PIN, INPUT);
    mySwitch.enableReceive(RF_RX_PIN);

    myServo.attach(SERVO_PIN);
    myServo.write(90);

    Wire.begin(14, 27);
    delay(100);
    
    Wire.beginTransmission(0x20);
    if (Wire.endTransmission() == 0) {
        Serial.println("✅ PCF8574 found");
        PCF.begin();
        PCF.write(SEAT_LEFT_UP, HIGH);
        PCF.write(SEAT_LEFT_DOWN, HIGH);
        PCF.write(SEAT_RIGHT_UP, HIGH);
        PCF.write(SEAT_RIGHT_DOWN, HIGH);
        PCF.write(START_LED, HIGH);
        PCF.write(ELEMENT_LEFT, HIGH);
    } else {
        Serial.println("❌ PCF8574 NOT found!");
    }

    loadSettings();

    // ==========================================
    // ===== Web Server Handlers =====
    // ==========================================
    server.on("/", handleRoot);
    server.on("/check", HTTP_POST, handleCheck);
    server.on("/control", handleControl);
    server.on("/status", handleStatus);
    server.on("/ldr", handleSettings);
    server.on("/logout", handleLogout);
    server.on("/getUserInfo", handleGetUserInfo);
    server.on("/opentrunk", handleOpenTrunkWithUnlock);
    server.on("/reset", handleReset);
    server.on("/doorstatus", handleDoorStatus);
    server.on("/remotecommand", handleRemoteCommand);
    server.on("/cargear", handleGearStatus);
    server.on("/sleepboth", handleSleepBoth);
    server.on("/sleepdriver", handleSleepDriver);
    server.on("/sleeppassenger", handleSleepPassenger);
    server.on("/wakeboth", handleWakeBoth);
    server.on("/wakedriver", handleWakeDriver);
    server.on("/wakepassenger", handleWakePassenger);
    server.on("/relay1", handleRelay1Web);
    server.on("/relay2", handleRelay2Web);
    server.on("/elementleft", handleElement);
    server.on("/holdStart", handleHoldStart);
    server.on("/holdStop", handleHoldStop);
    server.on("/remotestart", handleRemoteStartWeb);
    server.on("/remotestop", handleRemoteStopWeb);
    server.on("/pulse1", handlePulse1);
    server.on("/pulse2", handlePulse2);
    server.on("/setseatsettings", handleSetSeatSettings);
    server.on("/setldr", handleSetLDR);
    server.on("/getallsettings", handleGetAllSettings);
    server.on("/pcstatus", handlePcStatus);
    server.on("/pcon", handlePcOn);
    server.on("/pcoff", handlePcOff);
    server.on("/pcauto", handlePcAuto);
    server.on("/toggleFullLock", handleToggleFullLock);
    server.on("/getFullLock", handleGetFullLock);
    server.on("/highbeam", handleHighBeam);
    server.on("/servolock", handleServoLock);
    server.on("/servounlock", handleServoUnlock);
    server.on("/togglebutton", handleToggleButtonWeb);
    server.on("/getbuttonstatus", handleGetButtonStatus);
    server.on("/togglebeep", handleToggleBeep);
    server.on("/getbeepstatus", handleGetBeepStatus);
    server.on("/setbeepduration", handleSetBeepDuration);
    server.on("/filterchange", []() { server.send(200, "text/plain", "OK"); });

    server.begin();
    Serial.println("✅ Web server started");
    
    keylessLedState = false;
    lastKeylessLedToggle = millis();

    xTaskCreatePinnedToCore(Core1Task, "Core1Task", 10000, NULL, 1, &Task1, 0);
    xTaskCreatePinnedToCore(Core2Task, "Core2Task", 10000, NULL, 1, &Task2, 1);
    
    Serial.println("\n✅ System ready with MAX Power!");
    Serial.println("📡 WiFi: MAX Power (84)");
    Serial.println("📡 RF Remote: MAX Sensitivity");
    Serial.println("📡 RFID (ESP8266) via UDP on port: " + String(udpPort));
    Serial.println("🔘 PIN 12: Toggle Mirror");
    Serial.println("🔘 PIN 15: Hold for RELAY5");
    Serial.println("🔒 FULL LOCK: Locks door and disables ALL buttons");
    Serial.println("💡 Lock will automatically turn OFF High Beam");
    Serial.println("🔊 RELAY8: 1 beep for LOCK, 2 beeps for UNLOCK (only once per lock cycle)");
    Serial.println("🔊 RELAY8 Beep duration: " + String(relay8BeepDuration) + "ms");
    Serial.println("🌐 Web: http://" + WiFi.softAPIP().toString());
}

void loop() {
    vTaskDelay(1000);
}