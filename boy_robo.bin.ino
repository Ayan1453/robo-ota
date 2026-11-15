// ==================== PART 1 OF 3: CORE, AUDIO, MIC, WEATHER & WEB ====================
#include <HTTPUpdate.h>

#if IS_MY_BOARD
  const char* GITHUB_OTA_URL = "https://raw.githubusercontent.com/Ayan1453/robo-ota/main/boy_robo.bin";
#else
  const char* GITHUB_OTA_URL = "https://raw.githubusercontent.com/Ayan1453/robo-ota/main/girl_robo.bin";
#endif
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s_std.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// =========================================================================
// DISPLAY ORIENTATION & BOARD IDENTITY SWITCH
// TFT_ROTATION: 2 = Upside-Down (180 deg for enclosure), 0 = Standard Upright
// IS_MY_BOARD:  true = Your board (Srinagar), false = Her board (Halle, DE)
// =========================================================================
#define TFT_ROTATION         2
#define IS_MY_BOARD          true

// ===== HIDDEN SURPRISE BIRTHDAY DATES =====
#define GIRL_BDAY_DAY        15
#define GIRL_BDAY_MONTH      9
#define BOY_BDAY_DAY         21
#define BOY_BDAY_MONTH       12

#if IS_MY_BOARD
  #define MY_BDAY_D          BOY_BDAY_DAY
  #define MY_BDAY_M          BOY_BDAY_MONTH
  #define PARTNER_BDAY_D     GIRL_BDAY_DAY
  #define PARTNER_BDAY_M     GIRL_BDAY_MONTH
#else
  #define MY_BDAY_D          GIRL_BDAY_DAY
  #define MY_BDAY_M          GIRL_BDAY_MONTH
  #define PARTNER_BDAY_D     BOY_BDAY_DAY
  #define PARTNER_BDAY_M     BOY_BDAY_MONTH
#endif

// ===== DEFAULT TIMEZONES =====
#define TZ_INDIA             "IN|IST-5:30"
#define TZ_GERMANY_AUTO      "DE|CET-1CEST,M3.5.0,M10.5.0/3"

#if IS_MY_BOARD
  #define DEFAULT_MY_TZ      TZ_INDIA
  #define DEFAULT_PARTNER_TZ TZ_GERMANY_AUTO
#else
  #define DEFAULT_MY_TZ      TZ_GERMANY_AUTO
  #define DEFAULT_PARTNER_TZ TZ_INDIA
#endif

// ===== LIVE OPEN-METEO WEATHER ENDPOINTS (SRINAGAR & HALLE, GERMANY) =====
const char* SXR_WEATHER_HTTP = "http://api.open-meteo.com/v1/forecast?latitude=34.0837&longitude=74.7973&current_weather=true";
const char* HAL_WEATHER_HTTP = "http://api.open-meteo.com/v1/forecast?latitude=51.4828&longitude=11.9697&current_weather=true";

#if IS_MY_BOARD
  #define MY_WEATHER_URL     SXR_WEATHER_HTTP
  #define HER_WEATHER_URL    HAL_WEATHER_HTTP
  #define MY_CITY_TAG        "SXR"
  #define HER_CITY_TAG       "HAL"
#else
  #define MY_WEATHER_URL     HAL_WEATHER_HTTP
  #define HER_WEATHER_URL    SXR_WEATHER_HTTP
  #define MY_CITY_TAG        "HAL"
  #define HER_CITY_TAG       "SXR"
#endif

// ===== HARDWARE PINS =====
#define BOOT_BTN_PIN         0    // Onboard GPIO 0 button
#define HER_ROBO_HOME_BTN    34   // Short press = Home | Hold 600ms = Voice Chat
#define BTN_UP_PIN           14   // Next Card / Cycle Season or Clock Layout / Jump
#define BTN_SEL_PIN          12   // Short click = Card Dock | Hold 600ms = Voice Chat
#define BTN_BACK_PIN         13   // Cycle Home -> Dual Clock -> Weather Station

#define PET_TOUCH_PIN        32
#define LDR_PIN              35
#define BAT_ADC_PIN          36
#define BUZZER_PIN           27
#define I2C_SDA              21
#define I2C_SCL              22

// CONFIRMED INMP441 I2S DIGITAL MICROPHONE PINS
#define MIC_WS_PIN           25
#define MIC_SCK_PIN          26
#define MIC_SD_PIN           33

// LDR Light Thresholds
#define LDR_DARK_THRESHOLD   100
#define LDR_LIGHT_THRESHOLD  500

// ===== CALIBRATED MICROPHONE THRESHOLDS (0..9999 UNLOCKED SCALE) =====
#define MIC_NOISE_GATE_RAW     550
#define MIC_FULL_SCALE_RAW     5500
#define MIC_QUIET_FLOOR        450
#define MIC_TALK_THRESHOLD     750
#define MIC_GAME_JUMP_THRESH   950
#define MIC_TOO_LOUD_THRESHOLD 3800
#define MIC_TOO_LOUD_HOLD_MS   2400
#define MIC_CLAP_THRESHOLD     5000
#define MIC_BLOW_THRESHOLD     8200

// Balanced MPU6050 Thresholds
#define MPU_TAP_ACCEL_G        0.22f
#define MPU_TAP_GYRO_DPS       55.0f
#define MPU_MAX_STEP_ENERGY    0.20f
#define MPU_FAINT_ENERGY       1.65f

#define DEBOUNCE_MS            55
#define DOUBLE_CLICK_MS        380
#define LONG_PRESS_MS          600

#define TFT_CS                 5
#define TFT_DC                 2
#define TFT_RST                4
#define SW                     128
#define SH                     128

// ===== TIMERS & ANIMATION SPEED =====
#define ANIM_FRAME_FAST_MS     35
#define ANIM_FRAME_SAVER_MS    50
#define DEFAULT_IDLE_SEC       45
#define PET_HOLD_MS            1800
#define TICKLE_HOLD_MS         5000
#define WAKE_UP_DURATION_MS    2600
#define HYDRATE_INTERVAL_MS    (45UL * 60UL * 1000UL)
#define TANTRUM_IDLE_MS        (20UL * 60UL * 1000UL)

// ===== NETWORK & MQTT CREDENTIALS =====
const char* DEFAULT_WIFI_SSID = "Airtel_Shayankhan";
const char* DEFAULT_WIFI_PASS = "Shayankayy@1@2@0";
const char* MQTT_HOST         = "b600c0e7.ala.eu-central-1.emqxsl.com";
const int   MQTT_PORT         = 8883;
const char* MQTT_USER         = "Ayann1453";
const char* MQTT_PASS         = "qwertytype";

const char* SHARED_NOTE_TOPIC = "us/note/all";

#if IS_MY_BOARD
  const char* MY_MOOD_TOPIC      = "us/mood/boy";
  const char* REMOTE_MOOD_TOPIC  = "us/mood/girl";
  const char* SEND_ACT_TOPIC     = "us/action/to_girl";
  const char* RECV_ACT_TOPIC     = "us/action/to_boy";
  const char* MY_DINO_TOPIC      = "us/dino/boy";
  const char* REMOTE_DINO_TOPIC  = "us/dino/girl";
  const char* MY_FLAP_TOPIC      = "us/flap/boy";
  const char* REMOTE_FLAP_TOPIC  = "us/flap/girl";
  const char* MY_DODGE_TOPIC     = "us/dodge/boy";
  const char* REMOTE_DODGE_TOPIC = "us/dodge/girl";
  const char* SEND_HB_TOPIC      = "us/hb/to_girl";
  const char* RECV_HB_TOPIC      = "us/hb/to_boy";
  const char* SEND_POMO_TOPIC    = "us/pomo/to_girl";
  const char* RECV_POMO_TOPIC    = "us/pomo/to_boy";
  const char* SEND_NOTE_TOPIC    = "us/note/to_girl";
  const char* RECV_NOTE_TOPIC    = "us/note/to_boy";
#else
  const char* MY_MOOD_TOPIC      = "us/mood/girl";
  const char* REMOTE_MOOD_TOPIC  = "us/mood/boy";
  const char* SEND_ACT_TOPIC     = "us/action/to_boy";
  const char* RECV_ACT_TOPIC     = "us/action/to_girl";
  const char* MY_DINO_TOPIC      = "us/dino/girl";
  const char* REMOTE_DINO_TOPIC  = "us/dino/boy";
  const char* MY_FLAP_TOPIC      = "us/flap/girl";
  const char* REMOTE_FLAP_TOPIC  = "us/flap/boy";
  const char* MY_DODGE_TOPIC     = "us/dodge/girl";
  const char* REMOTE_DODGE_TOPIC = "us/dodge/boy";
  const char* SEND_HB_TOPIC      = "us/hb/to_boy";
  const char* RECV_HB_TOPIC      = "us/hb/to_girl";
  const char* SEND_POMO_TOPIC    = "us/pomo/to_boy";
  const char* RECV_POMO_TOPIC    = "us/pomo/to_girl";
  const char* SEND_NOTE_TOPIC    = "us/note/to_boy";
  const char* RECV_NOTE_TOPIC    = "us/note/to_girl";
#endif

// ===== MODERN WARM OBSIDIAN PALETTE (RGB565) =====
#define BLACK         0x0000
#define WHITE         0xFFFF
#define GRAY          0x7BEF
#define DARK_GRAY     0x39E7
#define SLATE_DARK    0x18E3  // Floating card dark charcoal
#define SLATE_MID     0x3186  // Subtle track / divider
#define SLATE_LIGHT   0x7BEF  // Muted label
#define PINK          0xF81F
#define LIGHT_PINK    0xFD7F
#define SAKURA_PINK   0xF986  // Warm rose-coral accent
#define SAKURA_LIGHT  0xFD7F  // Soft sakura blush
#define SUN_GOLD      0xFEA0  // Golden hour warm sun
#define ICE_CYAN      0x6DFF  // Frost / rain accent
#define RED           0xF800
#define CYAN          0x07FF
#define BLUE          0x1C9F
#define YELLOW        0xFFE0
#define GREEN         0x07E0
#define ORANGE        0xFD20

// ===== 5 SEASONAL / LIVE WEATHER THEMES =====
enum SeasonType {
  SEASON_SPRING_SAKURA = 0, // Cloudy / Mild: Cherry Blossoms & soft cloud mist
  SEASON_GOLDEN_SUMMER,     // Clear / Sunny: Golden-Hour sun halo & rising sun-motes
  SEASON_AUTUMN_CHINAR,     // Windy / Breezy: Wind streamlines & falling Chinar leaves
  SEASON_WINTER_SNOW,       // Snowy / Freezing: Drifting snowflakes & eye squint
  SEASON_MONSOON_RAIN,      // Rainy / Showers: Diagonal rain streaks & bottom splashes
  SEASON_TOTAL
};

struct SeasonTheme {
  const char* name;
  const char* shortTag;
  uint16_t eyeCore;     // Main warm eye fill
  uint16_t eyeRim;      // Mid glow ring
  uint16_t eyeHalo;     // Outer ambient aura
  uint16_t pCol1;       // Primary particle color
  uint16_t pCol2;       // Secondary particle color
};

const SeasonTheme SEASONS[SEASON_TOTAL] = {
  { "CHERRY BLOSSOM", "SPR", 0xFF7B, 0xFACA, 0x48E2, SAKURA_PINK, SAKURA_LIGHT },
  { "SUNNY GOLDEN",   "SUN", 0xFFDF, 0xFD64, 0x51C0, SUN_GOLD,    0xFF9C       },
  { "WINDY CHINAR",   "WND", 0xFF9C, 0xFD64, 0x51C0, 0xD982,      0xFA80       },
  { "WINTER SNOW",    "SNW", 0xFFDF, 0xDF14, 0x4204, 0xFFFF,      0x9E7F       },
  { "COZY RAIN",      "RAN", 0xFF9C, 0xE6B2, 0x39E7, ICE_CYAN,    0x3186       }
};

// ===== MOODS & UI STATES =====
enum Mood {
  SIMPLE, HAPPY, LOVE, ANGRY, SAD, CRY, FROWN,
  SYNC_MOOD_COUNT,
  CLOCK = SYNC_MOOD_COUNT,
  PETTING,
  TICKLISH,
  IRRITATED,
  ANXIOUS,
  FAINTING,
  ACTION_HUG,
  ACTION_POKE,
  ACTION_KISS,
  ACTION_BDAY,
  TWIN_SYNC,
  TANTRUM_POUT,
  HYDRATE_REMIND,
  MIC_STARTLED,
  MIC_R2D2_REPLY,
  SLEEPING,
  WAKING_UP,
  SND_DINO_JUMP,
  SND_DINO_SCORE,
  SND_DINO_DIE,
  SND_NEW_RECORD,
  SND_MENU_TICK,
  SND_HEARTBEAT,
  SND_POMO_START,
  SND_POMO_DONE,
  SND_QUIRK_CHIRP,
  SND_QUIRK_SNEEZE,
  SND_CLAP_ONE,
  SND_NOTE_RECV
};

enum LifeState { LIFE_SLEEPING, LIFE_WAKING_UP, LIFE_AWAKE };

enum UIState {
  STATE_NORMAL_HER_ROBO,
  STATE_MENU_MAIN,        // Sleek Horizontal Sliding Card Dock
  STATE_MY_ROBO,
  STATE_CLOCK,            // Modern Vector Dual Clock (Split-Horizon & Twin-Pods)
  STATE_WEATHER_STATION,  // Live Dual Weather Station (Halle DE & Srinagar IN)
  STATE_ACTIONS_MENU,
  STATE_GAMES_MENU,
  STATE_DINO_GAME,
  STATE_FLAPPY_GAME,
  STATE_DODGE_GAME,
  STATE_HEARTBEAT,
  STATE_POMODORO,
  STATE_VOICE_CHAT,
  STATE_SOUND_PARTY,
  STATE_SETTINGS,
  STATE_CONNECTION_INFO
};

enum IdleQuirk {
  QUIRK_NONE = 0,
  QUIRK_LOOK_LEFT,
  QUIRK_LOOK_RIGHT,
  QUIRK_CURIOUS_TILT,
  QUIRK_WINK,
  QUIRK_WHISTLE,
  QUIRK_MORNING_CHAI,
  QUIRK_LUNCH_SNACK,
  QUIRK_NIGHT_YAWN,
  QUIRK_STARGAZE,
  QUIRK_BUTTERFLY,
  QUIRK_SNEEZE,
  QUIRK_DANCE_GROOVE,
  QUIRK_DETECTIVE_SCAN,
  QUIRK_DAYDREAM_LOVE,
  QUIRK_STRETCH
};

LifeState lifeState = LIFE_SLEEPING;
unsigned long wakeUpStartedAt = 0;

UIState currentUI = STATE_NORMAL_HER_ROBO;
int menuSelection = 0;
float menuScrollPos = 0.0f;
int gameMenuSelection = 0;
int actionSelection = 0;
int settingsSelection = 0;
const int TOTAL_ROOT_ITEMS = 10;
const int TOTAL_GAME_ITEMS = 5;
const int TOTAL_SETTINGS_ITEMS = 11;
const int TOTAL_ACTION_ITEMS = 4;

bool audioMuted     = false;
bool micEnabled     = true;
bool ldrEnabled     = false;
bool motionEnabled  = true;
bool hydrateEnabled = true;
bool battSaverMode  = true;
bool wifiEnabled    = true;
int  idleSecSetting = DEFAULT_IDLE_SEC;

static const uint32_t SPI_HZ = 4000000;

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16* frame = nullptr;
WiFiClientSecure net;
PubSubClient mqtt(net);
Preferences preferences;
WebServer server(80);

Mood myLocalMood   = SIMPLE;
Mood remoteHerMood = SIMPLE;
Mood localOverride = SIMPLE;
bool hasLocalOverride = false;
unsigned long overrideStartedAt = 0;
unsigned long overrideEndsAt = 0;
unsigned long lastButtonPressAt = 0;
unsigned long screenEnteredAt = 0;

// Smooth Physics-Interpolated Warm Eye Rig State
float curLx = 18.0f, curLy = 44.0f, curLw = 36.0f, curLh = 36.0f;
float curRx = 74.0f, curRy = 44.0f, curRw = 36.0f, curRh = 36.0f;
float curCorner = 12.0f;
float curPupilX = 0.0f, curPupilY = 0.0f;
float tgtPupilX = 0.0f, tgtPupilY = 0.0f;
unsigned long snowSquintUntil = 0;

// Seasonal & Live Dual Weather State (Halle, DE & Srinagar, IN)
SeasonType currentSeason    = SEASON_SPRING_SAKURA;
bool manualSeasonOverride   = false; // false = Auto-sync particles to live city weather!
bool clockTwinPodLayout     = false; // false = Split-Horizon, true = Twin-Pods
bool weatherSplitLayout     = false; // false = Side-by-Side Pods, true = Detailed Wind Deck

// wType: 0=Clear/Sunny, 1=Cloudy, 2=Rainy, 3=Snowy, 4=Windy/Breezy
volatile int  myTempC            = 12;
volatile int  herTempC           = 5;
volatile int  myWindKmh          = 4;
volatile int  herWindKmh         = 9;
volatile int  myWeatherType      = 0;
volatile int  herWeatherType     = 1;
volatile bool liveWeatherSynced  = false;
volatile bool requestPanelRecover= false;
volatile bool requestParticleInit= false;
char syncStatusText[16]          = "WIFI WAIT..";

#define MAX_PARTICLES 8
struct WeatherParticle {
  float x, y, vx, vy;
  uint8_t phase, size;
};
WeatherParticle particles[MAX_PARTICLES];
int trackedParticleIdx = 0;

volatile int currentR2ReplyId = 0;
int lastR2ReplyId = -1;

IdleQuirk activeQuirk = QUIRK_NONE;
unsigned long quirkStartedAt = 0;
unsigned long quirkEndsAt = 0;
unsigned long nextQuirkAt = 7000;
unsigned long lastHydrateAt = 0;
unsigned long lastTantrumCheckAt = 0;
int currentLocalHour  = 12;
int currentLocalDay   = 1;
int currentLocalMonth = 1;
bool isMyBdayToday      = false;
bool isPartnerBdayToday = false;

float smoothedBatMv = 0.0f;
int   batteryPct    = 100;
bool  batterySensed = false;

// ===== 3ms HARDWARE-TIMED MICROPHONE & CLAP STATE =====
i2s_chan_handle_t micRxHandle      = NULL;
TaskHandle_t micTaskHandle         = NULL;
volatile bool i2sMicReady          = false;
volatile int  micRawAvgL           = 0;
volatile int  micInstantNorm       = 0;
volatile int  micSmoothLevel       = 0;
volatile int  micPeakHold          = 0;
volatile unsigned long micPeakHoldUntil = 0;
volatile int  micZeroCrossings     = 0;

volatile int  validClapCount       = 0;
volatile unsigned long firstValidClapMs = 0;
volatile bool doubleClapEventPending    = false;
volatile bool clapOnePipPending         = false;

int   micBands[7]                  = {0, 0, 0, 0, 0, 0, 0};
bool  isBlowingMic                 = false;
bool  isVoiceListening             = false;
unsigned long blowStartMs          = 0;
unsigned long lastBlowHighMs       = 0;
unsigned long voiceStartMs         = 0;
unsigned long lastVoiceMs          = 0;
int   micPeakSession               = 0;

bool  isTooLoudTracking            = false;
unsigned long tooLoudStartMs       = 0;
unsigned long lastLoudSampleMs     = 0;

volatile bool isBuzzerPlaying      = false;
volatile unsigned long lastBuzzerEndMs = 0;

int totalLoveCount = 0;
String eventName = "Meetup";
int eventDaysLeft = 0;

bool stickyNoteActive = false;
char activeStickyNote[24] = "";
char outgoingStickyNote[24] = "";
bool publishNoteNeeded = false;
volatile bool hasIncomingNote = false;
char incomingNoteBuf[24] = "";

unsigned long lastAnimDraw = 0;
unsigned long lastActivityAt = 0;
unsigned long idleTimeoutMs = DEFAULT_IDLE_SEC * 1000UL;
bool inIdleClock = false;
bool manualClockActive = false;

int lastHerBtnReading  = LOW,  stableHerBtnState  = LOW;  unsigned long lastHerBtnDebounce  = 0;
int lastUpReading      = LOW,  stableUpState      = LOW;  unsigned long lastUpDebounce      = 0;
int lastSelReading     = LOW,  stableSelState     = LOW;  unsigned long lastSelDebounce     = 0;
int lastBackReading    = LOW,  stableBackState    = LOW;  unsigned long lastBackDebounce    = 0;
int lastBootBtnReading = HIGH, stableBootBtnState = HIGH; unsigned long lastBootBtnDebounce = 0;

unsigned long lastSelClickTime = 0;
unsigned long selPressStartMs  = 0;
bool selLongPressFired         = false;
bool selShortClickPending      = false;
unsigned long homePressStartMs = 0;
bool homeLongPressFired        = false;
unsigned long bootPressStartMs = 0;
bool bootLongPressFired        = false;
float holdProgressBar          = 0.0f;

bool petIsDigital = false;
int petBaseline = 0;
bool isPetting = false;
bool isTicklish = false;
bool prevPetTouched = false;
unsigned long petContinuousStart = 0;
unsigned long lastPetTouchAt = 0;
unsigned long lastPetBeepAt = 0;
int petFilterCount = 0;
unsigned long darkStartedAt = 0;
unsigned long lightStartedAt = 0;
int lastLdrValue = 2000;

uint8_t mpuAddr = 0x68;
bool mpuAvailable = false;
bool mpuBaselineReady = false;
float baseAx = 0.0f, baseAy = 0.0f, baseAz = 1.0f;
float baseGx = 0.0f, baseGy = 0.0f, baseGz = 0.0f;
float shakeEnergy = 0.0f;
unsigned long lastMpuPoll = 0;
unsigned long lastMotionTrigAt = 0;

char mqttClientId[20];
unsigned long lastMqttAttempt = 0;
unsigned long wifiBootAt = 0;
bool publishMoodNeeded  = false;
bool publishDinoNeeded  = false;
bool publishFlapNeeded  = false;
bool publishDodgeNeeded = false;
int pendingActionPublish = -1;
bool wifiReady = false;
bool apActive = false;
volatile int incomingMood = -1;
volatile int incomingAction = -1;
volatile int incomingRemoteDinoHigh  = -1;
volatile int incomingRemoteFlapHigh  = -1;
volatile int incomingRemoteDodgeHigh = -1;

#define MAX_HB_TAPS 12
uint16_t hbTapIntervals[MAX_HB_TAPS];
int hbTapCount = 0;
unsigned long lastHbTapMs = 0;
unsigned long hbPulseUntilMs = 0;
bool hbSentBanner = false;
unsigned long hbSentBannerUntil = 0;
bool publishHbNeeded = false;
char hbOutgoingPayload[80] = "";

uint16_t hbPlayIntervals[MAX_HB_TAPS];
int hbPlayTotal = 0;
int hbPlayIndex = 0;
bool hbPlaybackActive = false;
unsigned long hbNextBeatMs = 0;
unsigned long hbPlayEndMs = 0;
volatile bool hasIncomingHb = false;
char hbIncomingBuf[80] = "";

enum PomoPhase { POMO_IDLE, POMO_FOCUS, POMO_BREAK };
PomoPhase pomoPhase = POMO_IDLE;
bool pomoSelectedBreak = false;
unsigned long pomoPhaseEndsAt = 0;
unsigned long pomoTotalDurationMs = 25UL * 60UL * 1000UL;
bool publishPomoNeeded = false;
char pomoOutgoingCmd[16] = "";
volatile int incomingPomoCmd = 0;

String savedSsid      = "";
String savedPass      = "";
String savedTz        = DEFAULT_MY_TZ;
String savedPartnerTz = DEFAULT_PARTNER_TZ;

time_t lastClockEpoch = 0;
bool timeSynced = false;
char mainHmStr[8]       = "--:--";
char partnerHmStr[8]    = "--:--";
char mainSecStr[6]      = ":--";
char mainDateStr[22]    = "Syncing Time...";
char partnerMiniStr[12] = "DE --:--";
char localMiniStr[12]   = "IN --:--";
int  liveMyHr = 19, liveMyMin = 30, liveMySec = 0;
int  livePartnerHr = 15, livePartnerMin = 0;

// ===== COMPETITIVE GAMES STATE =====
bool  dinoVoiceMode   = false;
float dinoY           = 0.0f;
float dinoVel         = 0.0f;
float obsX            = 128.0f;
float obsSpeed        = 3.6f;
int   obsType         = 0;
int   dinoScore       = 0;
int   myDinoHigh      = 0;
int   herDinoHigh     = 0;
bool  dinoGameOver    = false;
bool  dinoNewRecord   = false;

float flapY           = 60.0f;
float flapVel         = 0.0f;
float pipeX           = 128.0f;
int   pipeGapY        = 60;
int   flapScore       = 0;
int   myFlapHigh      = 0;
int   herFlapHigh     = 0;
bool  flapGameOver    = false;
bool  flapNewRecord   = false;
unsigned long lastVoiceFlapMs = 0;

int   dodgeShipX      = 64;
int   dodgeScore      = 0;
int   myDodgeHigh     = 0;
int   herDodgeHigh    = 0;
float meteorX[3]      = { 24.0f, 64.0f, 104.0f };
float meteorY[3]      = { 28.0f, -10.0f, -48.0f };
float meteorSpeed     = 2.6f;
bool  dodgeGameOver   = false;
bool  dodgeNewRecord  = false;

// =========================================================================
// SAFE BATTERY OPTIMIZER & VOLTAGE MONITOR
// =========================================================================
void applyBatteryOptimization() {
  if (!wifiEnabled) return;
  WiFi.setSleep(false);
  if (battSaverMode) {
    WiFi.setTxPower(WIFI_POWER_11dBm);
  } else {
    WiFi.setTxPower(WIFI_POWER_15dBm);
  }
}

void updateBatteryMonitor(unsigned long now) {
  static unsigned long lastBatRead = 0;
  if (now - lastBatRead < 1500 && smoothedBatMv > 0.0f) return;
  lastBatRead = now;

  uint32_t pinMv = analogReadMilliVolts(BAT_ADC_PIN);
  float rawBatMv = (float)pinMv * 2.0f;

  if (rawBatMv < 1200.0f) {
    batterySensed = false;
    batteryPct = 100;
    return;
  }

  batterySensed = true;
  if (smoothedBatMv < 1200.0f) smoothedBatMv = rawBatMv;
  else smoothedBatMv = (smoothedBatMv * 0.88f) + (rawBatMv * 0.12f);

  int pct = (int)((smoothedBatMv - 3450.0f) * 100.0f / (4150.0f - 3450.0f));
  if (pct > 100) pct = 100;
  if (pct < 0)   pct = 0;
  batteryPct = pct;
}

void recoverPanel() {
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(TFT_ROTATION); // 2 = Upside-down 180-deg rotation for enclosure!
  tft.setSPISpeed(SPI_HZ);
}

// =========================================================================
// R2-D2 ASTRO-DROID SYNTHESIZER ENGINE (16 DISTINCT VOCAL REPLIES!)
// =========================================================================
TaskHandle_t buzzerTaskHandle = NULL;
volatile int pendingSoundMood = -1;

void playGrain(int onUs, int offUs, int durationMs) {
  unsigned long start = micros();
  unsigned long totalUs = (unsigned long)durationMs * 1000UL;
  while (micros() - start < totalUs) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(onUs);
    digitalWrite(BUZZER_PIN, LOW);
    if (offUs > 0) delayMicroseconds(offUs);
  }
}

void playChirpSlide(int startOnUs, int endOnUs, int startOffUs, int endOffUs, int durationMs) {
  const int steps = 28;
  int stepMs = durationMs / steps;
  if (stepMs < 2) stepMs = 2;
  for (int i = 0; i < steps; i++) {
    float t = (float)i / (float)(steps - 1);
    int onUs  = startOnUs  + (int)((endOnUs  - startOnUs)  * t);
    int offUs = startOffUs + (int)((endOffUs - startOffUs) * t);
    playGrain(onUs, offUs, stepMs);
  }
}

void playTrill(int on1, int off1, int on2, int off2, int stepMs, int count) {
  for (int i = 0; i < count; i++) {
    playGrain(on1, off1, stepMs);
    playGrain(on2, off2, stepMs);
  }
}

void playR2D2Variant(int variant) {
  switch (variant % 16) {
    case 0:
      playGrain(550, 220, 55); vTaskDelay(pdMS_TO_TICKS(20));
      playGrain(380, 380, 45); vTaskDelay(pdMS_TO_TICKS(20));
      playChirpSlide(280, 1350, 600, 90, 160);
      break;
    case 1:
      playTrill(320, 450, 520, 300, 22, 3);
      vTaskDelay(pdMS_TO_TICKS(18));
      playChirpSlide(250, 1500, 750, 75, 190);
      break;
    case 2:
      playChirpSlide(950, 220, 180, 950, 110);
      vTaskDelay(pdMS_TO_TICKS(25));
      playGrain(180, 1100, 70);
      vTaskDelay(pdMS_TO_TICKS(20));
      playChirpSlide(400, 1200, 400, 110, 80);
      break;
    case 3:
      for (int i = 0; i < 6; i++) {
        int p = 250 + ((i * 210 + (esp_random() % 180)) % 950);
        playGrain(p, 950 - (p / 2), 34);
        vTaskDelay(pdMS_TO_TICKS(14));
      }
      break;
    case 4:
      playTrill(480, 320, 920, 150, 24, 4);
      vTaskDelay(pdMS_TO_TICKS(20));
      playChirpSlide(500, 1450, 350, 80, 110);
      break;
    case 5:
      playChirpSlide(220, 1400, 800, 80, 120);
      playChirpSlide(1400, 320, 80, 600, 110);
      vTaskDelay(pdMS_TO_TICKS(20));
      playChirpSlide(350, 1300, 500, 90, 95);
      break;
    case 6:
      for (int i = 0; i < 4; i++) {
        playChirpSlide(300 + (i * 120), 850 + (i * 150), 450, 110, 45);
        vTaskDelay(pdMS_TO_TICKS(22));
      }
      break;
    case 7:
      playChirpSlide(180, 580, 950, 280, 180);
      vTaskDelay(pdMS_TO_TICKS(30));
      playChirpSlide(580, 1150, 280, 110, 140);
      break;
    case 8:
      playGrain(450, 380, 55); vTaskDelay(pdMS_TO_TICKS(22));
      playGrain(650, 260, 55); vTaskDelay(pdMS_TO_TICKS(22));
      playChirpSlide(850, 1400, 190, 85, 170);
      break;
    case 9:
      playGrain(200, 1150, 65); vTaskDelay(pdMS_TO_TICKS(30));
      playGrain(240, 950, 65);  vTaskDelay(pdMS_TO_TICKS(30));
      playChirpSlide(280, 1100, 750, 130, 130);
      break;
    case 10:
      playTrill(300, 500, 1300, 90, 20, 5);
      vTaskDelay(pdMS_TO_TICKS(15));
      playChirpSlide(1300, 400, 90, 500, 80);
      playChirpSlide(400, 1550, 500, 70, 95);
      break;
    case 11:
      playChirpSlide(1100, 380, 120, 550, 170);
      vTaskDelay(pdMS_TO_TICKS(35));
      playChirpSlide(520, 220, 380, 980, 160);
      break;
    case 12:
      playChirpSlide(420, 580, 450, 300, 75);
      vTaskDelay(pdMS_TO_TICKS(45));
      playChirpSlide(580, 1180, 300, 110, 135);
      break;
    case 13:
      playTrill(750, 200, 980, 140, 18, 4);
      playChirpSlide(1200, 280, 100, 680, 90);
      playTrill(420, 380, 1150, 110, 18, 3);
      break;
    case 14:
      playChirpSlide(260, 1250, 700, 95, 115);
      vTaskDelay(pdMS_TO_TICKS(55));
      playChirpSlide(220, 1500, 800, 75, 95);
      playChirpSlide(1500, 480, 75, 420, 135);
      break;
    case 15:
      playGrain(380, 420, 40); vTaskDelay(pdMS_TO_TICKS(15));
      playGrain(620, 260, 40); vTaskDelay(pdMS_TO_TICKS(15));
      playGrain(920, 160, 40); vTaskDelay(pdMS_TO_TICKS(15));
      playChirpSlide(450, 1500, 400, 75, 110);
      vTaskDelay(pdMS_TO_TICKS(20));
      playTrill(600, 220, 1350, 85, 22, 3);
      break;
  }
}

void buzzerTask(void *pvParameters) {
  while (true) {
    int m = pendingSoundMood;
    if (m >= 0 && !audioMuted) {
      pendingSoundMood = -1;
      isBuzzerPlaying = true;
      switch (m) {
        case SND_MENU_TICK:
          playChirpSlide(180, 450, 300, 100, 18);
          break;
        case SND_CLAP_ONE:
          playGrain(950, 110, 16);
          break;
        case SND_QUIRK_CHIRP:
          playChirpSlide(300, 850, 500, 180, 45);
          vTaskDelay(pdMS_TO_TICKS(25));
          playChirpSlide(700, 1250, 300, 120, 55);
          break;
        case SND_QUIRK_SNEEZE:
          playChirpSlide(220, 950, 800, 150, 90);
          vTaskDelay(pdMS_TO_TICKS(35));
          playGrain(180, 650, 65);
          playChirpSlide(1200, 250, 90, 850, 85);
          break;
        case MIC_R2D2_REPLY:
          playR2D2Variant(currentR2ReplyId);
          break;
        case MIC_STARTLED:
          playChirpSlide(200, 1450, 600, 70, 95);
          vTaskDelay(pdMS_TO_TICKS(20));
          playTrill(350, 450, 850, 180, 24, 4);
          break;
        case ACTION_BDAY:
          playChirpSlide(160, 260, 1200, 880, 230); vTaskDelay(pdMS_TO_TICKS(115));
          playChirpSlide(180, 280, 1150, 840, 210); vTaskDelay(pdMS_TO_TICKS(115));
          playChirpSlide(230, 380, 920,  640, 380); vTaskDelay(pdMS_TO_TICKS(140));
          playChirpSlide(180, 280, 1150, 840, 380); vTaskDelay(pdMS_TO_TICKS(140));
          playChirpSlide(300, 500, 740,  480, 400); vTaskDelay(pdMS_TO_TICKS(150));
          playChirpSlide(440, 210, 520, 1150, 660);
          vTaskDelay(pdMS_TO_TICKS(360));
          playChirpSlide(160, 260, 1200, 880, 230); vTaskDelay(pdMS_TO_TICKS(115));
          playChirpSlide(180, 280, 1150, 840, 210); vTaskDelay(pdMS_TO_TICKS(115));
          playChirpSlide(230, 380, 920,  640, 380); vTaskDelay(pdMS_TO_TICKS(140));
          playChirpSlide(180, 280, 1150, 840, 380); vTaskDelay(pdMS_TO_TICKS(140));
          playChirpSlide(360, 600, 620,  380, 400); vTaskDelay(pdMS_TO_TICKS(150));
          playChirpSlide(540, 220, 440, 1250, 740);
          break;
        case SND_NOTE_RECV:
          playChirpSlide(400, 900, 350, 150, 65);
          vTaskDelay(pdMS_TO_TICKS(25));
          playChirpSlide(700, 1400, 250, 80, 85);
          vTaskDelay(pdMS_TO_TICKS(25));
          playTrill(600, 200, 1300, 90, 28, 3);
          break;
        case SND_HEARTBEAT:
          playGrain(350, 850, 42);
          vTaskDelay(pdMS_TO_TICKS(48));
          playChirpSlide(550, 220, 450, 1100, 58);
          break;
        case SND_POMO_START:
          playGrain(400, 400, 55);
          vTaskDelay(pdMS_TO_TICKS(25));
          playGrain(700, 250, 55);
          vTaskDelay(pdMS_TO_TICKS(25));
          playChirpSlide(900, 1500, 180, 70, 110);
          break;
        case SND_POMO_DONE:
        case TWIN_SYNC:
          playTrill(450, 300, 1100, 100, 35, 3);
          vTaskDelay(pdMS_TO_TICKS(35));
          playChirpSlide(500, 1600, 300, 60, 150);
          break;
        case TICKLISH:
          playTrill(400, 350, 1200, 100, 24, 5);
          vTaskDelay(pdMS_TO_TICKS(20));
          playChirpSlide(600, 1550, 250, 70, 80);
          break;
        case TANTRUM_POUT:
          playChirpSlide(750, 220, 250, 1100, 110);
          vTaskDelay(pdMS_TO_TICKS(55));
          playChirpSlide(850, 200, 200, 1200, 130);
          break;
        case HYDRATE_REMIND:
          for (int i = 0; i < 3; i++) {
            playChirpSlide(250, 950, 600, 140, 55);
            vTaskDelay(pdMS_TO_TICKS(25));
          }
          break;
        case SIMPLE:
          playChirpSlide(250, 900, 700, 150, 85);
          break;
        case HAPPY:
          playChirpSlide(300, 1100, 600, 120, 90);
          vTaskDelay(pdMS_TO_TICKS(35));
          playTrill(350, 450, 950, 150, 28, 3);
          break;
        case LOVE:
          playGrain(180, 950, 110);
          vTaskDelay(pdMS_TO_TICKS(25));
          playChirpSlide(300, 1400, 500, 90, 140);
          vTaskDelay(pdMS_TO_TICKS(30));
          playChirpSlide(600, 1600, 300, 60, 110);
          break;
        case PETTING:
          playChirpSlide(120, 280, 1400, 850, 160);
          vTaskDelay(pdMS_TO_TICKS(25));
          playChirpSlide(450, 1200, 400, 100, 75);
          break;
        case SAD:
          playChirpSlide(900, 250, 150, 900, 160);
          vTaskDelay(pdMS_TO_TICKS(60));
          playChirpSlide(600, 160, 350, 1500, 210);
          break;
        case CRY:
          for (int i = 0; i < 2; i++) {
            playChirpSlide(250, 750, 600, 200, 55);
            vTaskDelay(pdMS_TO_TICKS(30));
            playChirpSlide(800, 200, 200, 1200, 130);
            vTaskDelay(pdMS_TO_TICKS(55));
          }
          break;
        case FROWN:
          playChirpSlide(500, 200, 500, 1300, 120);
          break;
        case ANGRY:
          playGrain(220, 1600, 140);
          vTaskDelay(pdMS_TO_TICKS(35));
          playChirpSlide(1100, 250, 120, 1100, 130);
          break;
        case IRRITATED:
          playTrill(220, 700, 650, 250, 24, 3);
          vTaskDelay(pdMS_TO_TICKS(20));
          playChirpSlide(350, 1200, 500, 100, 80);
          break;
        case ANXIOUS:
          playTrill(280, 550, 520, 320, 22, 4);
          break;
        case FAINTING:
          playTrill(900, 150, 400, 450, 28, 3);
          playChirpSlide(1100, 300, 120, 700, 150);
          playChirpSlide(700, 120, 350, 1800, 210);
          break;
        case ACTION_KISS:
          playChirpSlide(200, 650, 800, 250, 70);
          vTaskDelay(pdMS_TO_TICKS(35));
          playGrain(1400, 80, 22);
          vTaskDelay(pdMS_TO_TICKS(25));
          playChirpSlide(500, 1500, 350, 80, 95);
          break;
        case ACTION_HUG:
          playChirpSlide(180, 750, 1100, 220, 150);
          vTaskDelay(pdMS_TO_TICKS(40));
          playChirpSlide(350, 1150, 600, 120, 170);
          break;
        case ACTION_POKE:
          for (int i = 0; i < 3; i++) {
            playChirpSlide(200, 1350, 700, 80, 60);
            playChirpSlide(1350, 300, 80, 650, 55);
            if (i < 2) vTaskDelay(pdMS_TO_TICKS(850));
          }
          break;
        case WAKING_UP:
          playChirpSlide(150, 950, 1300, 180, 240);
          playChirpSlide(950, 200, 180, 1400, 260);
          vTaskDelay(pdMS_TO_TICKS(180));
          playChirpSlide(300, 700, 400, 150, 35);
          vTaskDelay(pdMS_TO_TICKS(90));
          playChirpSlide(400, 950, 350, 100, 45);
          break;
        case SLEEPING:
          playChirpSlide(650, 120, 300, 1800, 220);
          break;
        case CLOCK:
          playGrain(600, 200, 25);
          vTaskDelay(pdMS_TO_TICKS(45));
          playGrain(1100, 100, 35);
          break;
        case SND_DINO_JUMP:
          playChirpSlide(250, 1100, 500, 90, 45);
          break;
        case SND_DINO_SCORE:
          playGrain(700, 200, 35);
          vTaskDelay(pdMS_TO_TICKS(20));
          playGrain(1400, 80, 55);
          break;
        case SND_NEW_RECORD:
          playTrill(500, 250, 1300, 80, 35, 4);
          break;
        case SND_DINO_DIE:
          playChirpSlide(900, 180, 150, 1500, 240);
          break;
        default:
          playChirpSlide(300, 800, 500, 150, 60);
          break;
      }
      lastBuzzerEndMs = millis();
      isBuzzerPlaying = false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  xTaskCreatePinnedToCore(buzzerTask, "BuzzerTask", 2048, NULL, 1, &buzzerTaskHandle, 0);
}

void playMoodSound(Mood m) {
  if (!audioMuted) pendingSoundMood = (int)m;
}

void enterVoiceChatMode(unsigned long now) {
  manualClockActive = false;
  inIdleClock = false;
  hasLocalOverride = false;
  isBlowingMic = false;
  isVoiceListening = false;
  isTooLoudTracking = false;
  validClapCount = 0;
  currentUI = STATE_VOICE_CHAT;
  screenEnteredAt = now;
  lastActivityAt = now;
  playMoodSound(SND_QUIRK_CHIRP);
}

void triggerR2D2Reply(unsigned long now, bool wasLoudOrLong) {
  int nextId = esp_random() % 16;
  if (nextId == lastR2ReplyId) nextId = (nextId + 1 + (esp_random() % 7)) % 16;
  if (wasLoudOrLong && (esp_random() & 1)) {
    const int energeticIds[5] = { 5, 6, 10, 14, 15 };
    nextId = energeticIds[esp_random() % 5];
  }
  lastR2ReplyId = nextId;
  currentR2ReplyId = nextId;

  localOverride = MIC_R2D2_REPLY;
  hasLocalOverride = true;
  overrideStartedAt = now;
  overrideEndsAt = now + 1850;
  activeQuirk = QUIRK_NONE;
  isBlowingMic = false;
  isVoiceListening = false;
  validClapCount = 0;
  lastActivityAt = now;
  inIdleClock = false;
  playMoodSound(MIC_R2D2_REPLY);
}

void incrementLoveCounter() {
  totalLoveCount++;
  preferences.begin("robot", false);
  preferences.putInt("love_cnt", totalLoveCount);
  preferences.end();
}

void clearTouchState() {
  isPetting = false;
  isTicklish = false;
  petFilterCount = 0;
}

void triggerOverride(Mood m, unsigned long durationMs, unsigned long now) {
  localOverride = m;
  hasLocalOverride = true;
  overrideStartedAt = now;
  overrideEndsAt = now + durationMs;
  activeQuirk = QUIRK_NONE;
  isBlowingMic = false;
  isVoiceListening = false;
  isTooLoudTracking = false;
  validClapCount = 0;
  lastActivityAt = now;
  inIdleClock = false;
  playMoodSound(m);
}

// =========================================================================
// SLEEP & WAKE-UP CONTROLLER
// =========================================================================
void startWakeUp(unsigned long now) {
  if (lifeState == LIFE_AWAKE) return;
  lifeState = LIFE_WAKING_UP;
  wakeUpStartedAt = now;
  lastActivityAt = now;
  lastHydrateAt = now;
  inIdleClock = false;
  manualClockActive = false;
  clearTouchState();
  playMoodSound(WAKING_UP);
}

void goToSleep(unsigned long now) {
  if (lifeState == LIFE_SLEEPING) return;
  lifeState = LIFE_SLEEPING;
  inIdleClock = false;
  manualClockActive = false;
  hasLocalOverride = false;
  hbPlaybackActive = false;
  isBlowingMic = false;
  isVoiceListening = false;
  isTooLoudTracking = false;
  clearTouchState();
  currentUI = STATE_NORMAL_HER_ROBO;
  playMoodSound(SLEEPING);
}

// =========================================================================
// 3ms HARDWARE-TIMED I2S MICROPHONE & CLAP TASK
// =========================================================================
void micSamplerTask(void *pvParameters) {
  int32_t samples[128];
  bool taskInImpulse = false;
  unsigned long taskImpulseStart = 0;
  unsigned long taskLastQuietAt  = 0;
  unsigned long taskLastLoudAt   = 0;

  while (true) {
    if (!i2sMicReady || !micEnabled || micRxHandle == NULL) {
      micInstantNorm = 0;
      micSmoothLevel = 0;
      validClapCount = 0;
      taskInImpulse  = false;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    size_t bytesRead = 0;
    esp_err_t err = i2s_channel_read(micRxHandle, samples, sizeof(samples), &bytesRead, 15);
    int frames = (bytesRead / sizeof(int32_t)) / 2;
    if (err == ESP_OK && frames >= 16) {
      unsigned long now = millis();

      int64_t sumL = 0, sumR = 0;
      for (int i = 0; i < frames; i++) {
        sumL += (samples[i * 2]     >> 11);
        sumR += (samples[i * 2 + 1] >> 11);
      }
      int32_t meanL = (int32_t)(sumL / frames);
      int32_t meanR = (int32_t)(sumR / frames);

      int64_t ampL = 0, ampR = 0;
      int zcL = 0;
      int32_t prevL = 0;
      for (int i = 0; i < frames; i++) {
        int32_t sL = (samples[i * 2]     >> 11) - meanL;
        int32_t sR = (samples[i * 2 + 1] >> 11) - meanR;
        ampL += abs(sL);
        ampR += abs(sR);
        if (i > 0 && ((sL >= 0 && prevL < 0) || (sL < 0 && prevL >= 0))) {
          zcL++;
        }
        prevL = sL;
      }

      int avgL = (int)(ampL / frames);
      int avgR = (int)(ampR / frames);
      int activeRaw = (avgL >= avgR) ? avgL : avgR;
      micRawAvgL = activeRaw;

      if (now < 3000 && activeRaw > 45000) {
        vTaskDelay(pdMS_TO_TICKS(3));
        continue;
      }

      if (isBuzzerPlaying || (now - lastBuzzerEndMs < 180) || (now - lastButtonPressAt < 380)) {
        micInstantNorm = 0;
        micSmoothLevel = (micSmoothLevel * 5) / 10;
        taskInImpulse  = false;
        vTaskDelay(pdMS_TO_TICKS(4));
        continue;
      }

      int norm = 0;
      if (activeRaw > MIC_NOISE_GATE_RAW) {
        norm = (int)(((int64_t)(activeRaw - MIC_NOISE_GATE_RAW) * 1000LL) / MIC_FULL_SCALE_RAW);
        if (norm < 20) norm = 20;
        if (norm > 9999) norm = 9999;
      }

      micInstantNorm   = norm;
      micZeroCrossings = zcL;

      if (norm > micSmoothLevel) {
        micSmoothLevel = (micSmoothLevel * 2 + norm * 8) / 10;
      } else {
        micSmoothLevel = (micSmoothLevel * 6 + norm * 4) / 10;
      }

      if (norm >= micPeakHold || now >= micPeakHoldUntil) {
        micPeakHold = norm;
        if (norm > 0) micPeakHoldUntil = now + 1200;
      }

      if (validClapCount == 1 && (now - firstValidClapMs > 750)) {
        validClapCount = 0;
      }

      if (currentUI == STATE_VOICE_CHAT || currentUI == STATE_SOUND_PARTY ||
          currentUI == STATE_DINO_GAME  || currentUI == STATE_FLAPPY_GAME ||
          isBlowingMic) {
        taskInImpulse  = false;
        validClapCount = 0;
      } else {
        if (!taskInImpulse) {
          if (norm < MIC_QUIET_FLOOR) {
            taskLastQuietAt = now;
          } else if (norm >= MIC_CLAP_THRESHOLD) {
            if (now - taskLastQuietAt <= 55 && (now - taskLastLoudAt >= 90)) {
              taskInImpulse    = true;
              taskImpulseStart = now;
            }
            taskLastLoudAt = now;
          }
        } else {
          unsigned long width = now - taskImpulseStart;
          if (norm >= MIC_QUIET_FLOOR) {
            taskLastLoudAt = now;
          }
          if (norm < MIC_QUIET_FLOOR) {
            taskInImpulse   = false;
            taskLastQuietAt = now;
            if (width >= 6 && width <= 115) {
              if (validClapCount == 0) {
                validClapCount   = 1;
                firstValidClapMs = taskImpulseStart;
                micSmoothLevel   = 0;
                clapOnePipPending = true;
              } else if (validClapCount == 1) {
                unsigned long gap = taskImpulseStart - firstValidClapMs;
                if (gap >= 110 && gap <= 750) {
                  validClapCount         = 0;
                  micSmoothLevel         = 0;
                  doubleClapEventPending = true;
                } else {
                  validClapCount   = 1;
                  firstValidClapMs = taskImpulseStart;
                  clapOnePipPending = true;
                }
              }
            } else {
              validClapCount = 0;
            }
          } else if (width > 115) {
            taskInImpulse  = false;
            validClapCount = 0;
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void initMicrophone() {
  if (micRxHandle != NULL) {
    i2s_channel_disable(micRxHandle);
    i2s_del_channel(micRxHandle);
    micRxHandle = NULL;
  }

  i2s_chan_config_t chan_cfg = {
    .id = I2S_NUM_0,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 6,
    .dma_frame_num = 64,
    .auto_clear = false,
  };

  if (i2s_new_channel(&chan_cfg, NULL, &micRxHandle) != ESP_OK) return;

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(22050),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)MIC_SCK_PIN,
      .ws   = (gpio_num_t)MIC_WS_PIN,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)MIC_SD_PIN,
      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
    },
  };

  if (i2s_channel_init_std_mode(micRxHandle, &std_cfg) == ESP_OK) {
    if (i2s_channel_enable(micRxHandle) == ESP_OK) {
      int32_t dummy[128];
      size_t br = 0;
      for (int i = 0; i < 30; i++) {
        i2s_channel_read(micRxHandle, dummy, sizeof(dummy), &br, 20);
      }
      i2sMicReady = true;
      if (micTaskHandle == NULL) {
        xTaskCreatePinnedToCore(micSamplerTask, "MicTask", 3072, NULL, 2, &micTaskHandle, 0);
      }
    }
  }
}

void processMicrophoneEvents(unsigned long now) {
  if (!i2sMicReady || !micEnabled) {
    isBlowingMic = false;
    isVoiceListening = false;
    isTooLoudTracking = false;
    validClapCount = 0;
    doubleClapEventPending = false;
    clapOnePipPending = false;
    return;
  }

  int currentNorm = micInstantNorm;
  int smooth      = micSmoothLevel;
  int zcL         = micZeroCrossings;

  int partyLevel = (smooth > 4000) ? 4000 : smooth;
  for (int b = 0; b < 7; b++) {
    int target = (partyLevel * (50 + (esp_random() % 70))) / 100;
    if (b < 2 && zcL < 14) target = (target * 13) / 10;
    if (b > 4 && zcL > 18) target = (target * 13) / 10;
    int h = (target * 26) / 4000;
    if (h > 26) h = 26;
    micBands[b] = (micBands[b] + h) / 2;
  }

  if (currentUI == STATE_SOUND_PARTY) {
    doubleClapEventPending = false;
    clapOnePipPending = false;
    if (currentNorm >= MIC_TALK_THRESHOLD) lastActivityAt = now;
    return;
  }

  if (currentUI == STATE_VOICE_CHAT) {
    doubleClapEventPending = false;
    clapOnePipPending = false;
    validClapCount = 0;
    isBlowingMic = false;
    isTooLoudTracking = false;

    if (hasLocalOverride && localOverride == MIC_R2D2_REPLY) {
      isVoiceListening = false;
      return;
    }

    if (currentNorm >= MIC_TALK_THRESHOLD || smooth >= MIC_TALK_THRESHOLD) {
      if (!isVoiceListening) {
        isVoiceListening = true;
        voiceStartMs = now;
        micPeakSession = currentNorm;
      } else if (currentNorm > micPeakSession) {
        micPeakSession = currentNorm;
      }
      lastVoiceMs = now;
      lastActivityAt = now;
    } else if (isVoiceListening && (now - lastVoiceMs > 480)) {
      unsigned long talkedDuration = lastVoiceMs - voiceStartMs;
      isVoiceListening = false;
      if (talkedDuration >= 160) {
        bool loudOrLong = (talkedDuration >= 1300 || micPeakSession >= (MIC_TALK_THRESHOLD * 3));
        triggerR2D2Reply(now, loudOrLong);
      }
    }
    return;
  }

  isVoiceListening = false;
  bool onHomeOrClock = (currentUI == STATE_NORMAL_HER_ROBO || currentUI == STATE_CLOCK ||
                        currentUI == STATE_WEATHER_STATION);

  if (clapOnePipPending) {
    clapOnePipPending = false;
    if ((lifeState == LIFE_SLEEPING || (lifeState == LIFE_AWAKE && onHomeOrClock)) &&
        !hasLocalOverride && !stickyNoteActive && !hbPlaybackActive) {
      playMoodSound(SND_CLAP_ONE);
    }
  }

  if (doubleClapEventPending) {
    doubleClapEventPending = false;
    if ((lifeState == LIFE_SLEEPING || (lifeState == LIFE_AWAKE && onHomeOrClock)) &&
        !hasLocalOverride && !stickyNoteActive && !hbPlaybackActive) {
      isBlowingMic = false;
      isTooLoudTracking = false;
      if (lifeState == LIFE_SLEEPING) {
        startWakeUp(now);
      } else {
        if (currentUI == STATE_NORMAL_HER_ROBO && !manualClockActive && !inIdleClock) {
          manualClockActive = true;
          inIdleClock = false;
        } else if (manualClockActive || inIdleClock || currentUI == STATE_CLOCK) {
          manualClockActive = false;
          inIdleClock = false;
          currentUI = STATE_WEATHER_STATION;
        } else {
          currentUI = STATE_NORMAL_HER_ROBO;
          manualClockActive = false;
          inIdleClock = false;
        }
        lastActivityAt = now;
        playMoodSound(CLOCK);
      }
      return;
    }
  }

  bool onHome = (lifeState == LIFE_AWAKE && currentUI == STATE_NORMAL_HER_ROBO &&
                 !manualClockActive && !inIdleClock && !hasLocalOverride &&
                 !isPetting && !stickyNoteActive && !hbPlaybackActive);

  if (onHome) {
    if (currentNorm >= MIC_BLOW_THRESHOLD || smooth >= MIC_BLOW_THRESHOLD) {
      lastBlowHighMs = now;
      if (!isBlowingMic) {
        isBlowingMic = true;
        blowStartMs = now;
      }
      if (now - blowStartMs >= 80) {
        validClapCount = 0;
        isTooLoudTracking = false;
      }
      if (now - blowStartMs >= 420) {
        isBlowingMic = false;
        isTooLoudTracking = false;
        pendingActionPublish = (int)ACTION_KISS;
        incrementLoveCounter();
        triggerOverride(ACTION_KISS, 3200, now);
        return;
      }
    } else if (isBlowingMic && (now - lastBlowHighMs > 180)) {
      isBlowingMic = false;
    }
  } else {
    isBlowingMic = false;
  }

  // Sustained "Too Loud" Detector (2.4s -> MIC_STARTLED!)
  if (lifeState == LIFE_AWAKE && onHomeOrClock && !hasLocalOverride &&
      !isPetting && !stickyNoteActive && !hbPlaybackActive && !isBlowingMic) {
    if (smooth >= MIC_TOO_LOUD_THRESHOLD || currentNorm >= MIC_TOO_LOUD_THRESHOLD) {
      lastLoudSampleMs = now;
      if (!isTooLoudTracking) {
        isTooLoudTracking = true;
        tooLoudStartMs = now;
      } else if (now - tooLoudStartMs >= MIC_TOO_LOUD_HOLD_MS) {
        isTooLoudTracking = false;
        manualClockActive = false;
        inIdleClock = false;
        currentUI = STATE_NORMAL_HER_ROBO;
        triggerOverride(MIC_STARTLED, 2800, now);
        return;
      }
    } else if (isTooLoudTracking && (now - lastLoudSampleMs > 320)) {
      isTooLoudTracking = false;
    }
  } else {
    isTooLoudTracking = false;
  }
}

// =========================================================================
// LIVE OPEN-METEO WEATHER TASK (HALLE, DE & SRINAGAR, IN)
// =========================================================================
int mapWmoCodeToWeatherType(int wmo, int windKmh) {
  if (wmo >= 71 && wmo <= 86) return 3; // Snowy
  if (wmo >= 51 && wmo <= 99) return 2; // Rainy / Showers
  if (windKmh >= 20 || wmo == 45 || wmo == 48) return 4; // Windy / Breezy
  if (wmo == 0 || wmo == 1)   return 0; // Sunny / Clear
  return 1;                             // Cloudy / Fair
}

bool extractNumericJsonValue(const String& json, const char* key, float& outVal) {
  int pos = 0;
  int keyLen = strlen(key);
  while ((pos = json.indexOf(key, pos)) >= 0) {
    int valStart = pos + keyLen;
    while (valStart < (int)json.length() &&
           (json[valStart] == ' ' || json[valStart] == '\t' || json[valStart] == ':')) {
      valStart++;
    }
    if (valStart < (int)json.length()) {
      char c = json[valStart];
      if ((c >= '0' && c <= '9') || c == '-') {
        int valEnd = valStart + 1;
        while (valEnd < (int)json.length() &&
               ((json[valEnd] >= '0' && json[valEnd] <= '9') || json[valEnd] == '.')) {
          valEnd++;
        }
        outVal = json.substring(valStart, valEnd).toFloat();
        return true;
      }
    }
    pos += keyLen;
  }
  return false;
}

bool parseOpenMeteoResponse(const String& json, volatile int& outTemp, volatile int& outWind, volatile int& outType) {
  float tVal = 0.0f, wVal = 0.0f, cVal = 0.0f;
  if (!extractNumericJsonValue(json, "\"temperature\"", tVal)) return false;
  outTemp = (int)roundf(tVal);
  if (extractNumericJsonValue(json, "\"windspeed\"", wVal)) outWind = (int)roundf(wVal);
  if (extractNumericJsonValue(json, "\"weathercode\"", cVal)) {
    outType = mapWmoCodeToWeatherType((int)roundf(cVal), outWind);
  }
  return true;
}

bool fetchCityWeather(const char* httpUrl, volatile int& outTemp, volatile int& outWind, volatile int& outType) {
  {
    WiFiClient plainClient;
    HTTPClient http;
    http.setTimeout(4500);
    http.useHTTP10(true);
    if (http.begin(plainClient, httpUrl)) {
      int code = http.GET();
      if (code == 200) {
        String payload = http.getString();
        http.end();
        if (parseOpenMeteoResponse(payload, outTemp, outWind, outType)) return true;
      } else {
        http.end();
      }
    }
  }
  {
    String httpsUrl = String(httpUrl);
    httpsUrl.replace("http://", "https://");
    WiFiClientSecure secClient;
    secClient.setInsecure();
    secClient.setTimeout(5);
    HTTPClient https;
    https.setTimeout(4500);
    https.useHTTP10(true);
    if (https.begin(secClient, httpsUrl)) {
      int code = https.GET();
      if (code == 200) {
        String payload = https.getString();
        https.end();
        if (parseOpenMeteoResponse(payload, outTemp, outWind, outType)) return true;
      } else {
        https.end();
      }
    }
  }
  return false;
}

void weatherSyncTask(void* pvParameters) {
  vTaskDelay(pdMS_TO_TICKS(3500));
  while (true) {
    if (!wifiEnabled || WiFi.status() != WL_CONNECTED) {
      strncpy(syncStatusText, "WIFI WAIT..", sizeof(syncStatusText));
      vTaskDelay(pdMS_TO_TICKS(4000));
      continue;
    }

    if (!liveWeatherSynced) strncpy(syncStatusText, "GET SXR...", sizeof(syncStatusText));
    bool ok1 = fetchCityWeather(MY_WEATHER_URL, myTempC, myWindKmh, myWeatherType);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (!liveWeatherSynced) strncpy(syncStatusText, "GET HALLE.", sizeof(syncStatusText));
    bool ok2 = fetchCityWeather(HER_WEATHER_URL, herTempC, herWindKmh, herWeatherType);

    if (ok1 || ok2) {
      bool firstSync = !liveWeatherSynced;
      liveWeatherSynced = true;
      strncpy(syncStatusText, "LIVE WEATHER", sizeof(syncStatusText));
      requestParticleInit = true;
      if (firstSync) requestPanelRecover = true;
      vTaskDelay(pdMS_TO_TICKS(12UL * 60UL * 1000UL));
    } else {
      strncpy(syncStatusText, "API RETRY..", sizeof(syncStatusText));
      vTaskDelay(pdMS_TO_TICKS(8000));
    }
  }
}

// =========================================================================
// GLOBAL TIMEZONE PARSER, DUAL CLOCK CACHE & WEB PORTAL
// =========================================================================
String extractPosixTz(const String& raw) {
  int sep = raw.indexOf('|');
  if (sep > 0) return raw.substring(sep + 1);
  return raw;
}

String extractTzCode(const String& raw) {
  int sep = raw.indexOf('|');
  if (sep == 2) return raw.substring(0, 2);
  if (raw.startsWith("IST"))  return "IN";
  if (raw.startsWith("CET") || raw.startsWith("CEST")) return "DE";
  return "UT";
}

void setupNTP() {
  String myPosix = extractPosixTz(savedTz);
  configTzTime(myPosix.c_str(), "pool.ntp.org", "time.google.com", "time.nist.gov");
  lastClockEpoch = 0;
}

void setManualOfflineTime(const String& hhmm) {
  int colon = hhmm.indexOf(':');
  if (colon <= 0) return;
  int hh = hhmm.substring(0, colon).toInt();
  int mm = hhmm.substring(colon + 1).toInt();
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return;

  String myPosix = extractPosixTz(savedTz);
  setenv("TZ", myPosix.c_str(), 1);
  tzset();

  struct tm t = {0};
  t.tm_year = 2026 - 1900;
  t.tm_mon  = 10;
  t.tm_mday = 14;
  t.tm_hour = hh;
  t.tm_min  = mm;
  t.tm_sec  = 0;
  time_t epoch = mktime(&t);
  struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  lastClockEpoch = 0;
}

void updateDualClockCache() {
  time_t nowEpoch = time(nullptr);
  if (nowEpoch < 100000) {
    timeSynced = false;
    unsigned long secTotal = (19UL * 3600UL) + (30UL * 60UL) + (millis() / 1000UL);
    liveMySec      = secTotal % 60;
    liveMyMin      = (secTotal / 60UL) % 60;
    liveMyHr       = (secTotal / 3600UL) % 24;
    int deTot      = ((liveMyHr * 60 + liveMyMin) - 270 + 1440) % 1440;
    livePartnerHr  = deTot / 60;
    livePartnerMin = deTot % 60;
    snprintf(mainHmStr,    sizeof(mainHmStr),    "%02d:%02d", liveMyHr, liveMyMin);
    snprintf(partnerHmStr, sizeof(partnerHmStr), "%02d:%02d", livePartnerHr, livePartnerMin);
    snprintf(mainSecStr,   sizeof(mainSecStr),   ":%02d",     liveMySec);
    return;
  }
  if (nowEpoch == lastClockEpoch) return;
  lastClockEpoch = nowEpoch;
  timeSynced = true;

  String partnerPosix = extractPosixTz(savedPartnerTz);
  String partnerCode  = extractTzCode(savedPartnerTz);
  setenv("TZ", partnerPosix.c_str(), 1);
  tzset();
  struct tm partnerTm;
  localtime_r(&nowEpoch, &partnerTm);
  livePartnerHr  = partnerTm.tm_hour;
  livePartnerMin = partnerTm.tm_min;
  strftime(partnerHmStr, sizeof(partnerHmStr), "%H:%M", &partnerTm);
  snprintf(partnerMiniStr, sizeof(partnerMiniStr), "%s %s", partnerCode.c_str(), partnerHmStr);

  String myPosix = extractPosixTz(savedTz);
  String myCode  = extractTzCode(savedTz);
  setenv("TZ", myPosix.c_str(), 1);
  tzset();
  struct tm myTm;
  localtime_r(&nowEpoch, &myTm);
  currentLocalHour  = myTm.tm_hour;
  currentLocalDay   = myTm.tm_mday;
  currentLocalMonth = myTm.tm_mon + 1;
  liveMyHr          = myTm.tm_hour;
  liveMyMin         = myTm.tm_min;
  liveMySec         = myTm.tm_sec;

  isMyBdayToday      = (currentLocalDay == MY_BDAY_D && currentLocalMonth == MY_BDAY_M);
  isPartnerBdayToday = (currentLocalDay == PARTNER_BDAY_D && currentLocalMonth == PARTNER_BDAY_M);

  strftime(mainHmStr,   sizeof(mainHmStr),   "%H:%M", &myTm);
  strftime(mainSecStr,  sizeof(mainSecStr),  ":%S",   &myTm);
  strftime(mainDateStr, sizeof(mainDateStr), "%a, %d %b %Y", &myTm);
  snprintf(localMiniStr, sizeof(localMiniStr), "%s %s", myCode.c_str(), mainHmStr);
}

const char TZ_OPTIONS_HTML[] PROGMEM = R"rawliteral(
  <optgroup label="South Asia &amp; Europe">
    <option value="IN|IST-5:30">India / Kashmir (IST - UTC+5:30)</option>
    <option value="DE|CET-1CEST,M3.5.0,M10.5.0/3">Germany / Halle (CET/CEST Auto-DST)</option>
    <option value="UK|GMT0BST,M3.5.0/1,M10.5.0">UK / London (GMT/BST Auto-DST)</option>
    <option value="AE|<+04>-4">UAE / Dubai (GST - UTC+4:00)</option>
    <option value="US|EST5EDT,M3.2.0,M11.1.0">USA Eastern - NY (Auto-DST)</option>
    <option value="UT|GMT0">UTC / GMT (0:00)</option>
  </optgroup>
)rawliteral";

void handleRoot() {
  String page = F(
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Robo Companion Hub</title><style>"
    "body{font-family:Arial,sans-serif;background:#111;color:#eee;text-align:center;padding:16px;}"
    ".box{background:#1e1e1e;max-width:410px;margin:0 auto 16px;padding:20px;border-radius:12px;border:1px solid #333;}"
    "h2{margin-top:0;color:#ffa726;}"
    "h3{color:#ff4081;margin:14px 0 6px;font-size:15px;}"
    "label{display:block;text-align:left;width:94%;margin:10px auto 4px;font-size:12px;color:#aaa;}"
    "input,select{width:94%;padding:9px;border-radius:6px;border:1px solid #444;background:#2b2b2b;color:#fff;font-size:14px;box-sizing:border-box;}"
    "button{background:#ffa726;color:#111;border:none;padding:12px;font-size:15px;font-weight:bold;border-radius:6px;cursor:pointer;width:94%;margin-top:14px;}"
    ".btn-pink{background:#ff4081;color:#fff;}"
    "</style></head><body>"
    "<div class='box'><h2>1. BEAM STICKY NOTE</h2>"
    "<form action='/beam' method='POST' autocomplete='off'>"
    "<input type='text' name='note' maxlength='20' required placeholder='Type short note (e.g. Miss you!)'>"
    "<button type='submit' class='btn-pink'>Beam Memo Now</button>"
    "</form></div>"
    "<div class='box'><h2>2. TIMEZONES &amp; SETUP</h2>"
    "<form action='/save' method='POST' autocomplete='off'>"
    "<h3>Special Date Countdown</h3>"
    "<label>Event Title (e.g. Meetup / Trip):</label>"
    "<input type='text' name='ev_name' maxlength='12' value='");
  page += eventName;
  page += F("'><label>Days Left (0 to hide):</label>"
    "<input type='number' name='ev_days' min='0' max='999' value='");
  page += String(eventDaysLeft);
  page += F("'><h3>Global Timezones &amp; Clock</h3>"
    "<label>My Timezone:</label><select id='tz' name='tz'>");
  page += FPSTR(TZ_OPTIONS_HTML);
  page += F("</select><label>Partner's Timezone:</label><select id='ptz' name='ptz'>");
  page += FPSTR(TZ_OPTIONS_HTML);
  page += F("</select>"
    "<label>Offline Manual Time (Optional, HH:MM):</label>"
    "<input type='text' name='mtime' placeholder='e.g. 19:30'>"
    "<h3>WiFi Setup</h3>"
    "<label>WiFi SSID (Leave blank to keep current):</label>"
    "<input type='text' name='ssid' autocomplete='off' placeholder='Keep current WiFi'>"
    "<label>WiFi Password:</label>"
    "<input type='password' name='pass' autocomplete='new-password' placeholder='Keep current password'>"
    "<button type='submit'>Save Settings</button></form></div>"
    "<script>"
    "document.getElementById('tz').value='");
  page += savedTz;
  page += F("';document.getElementById('ptz').value='");
  page += savedPartnerTz;
  page += F("';</script></body></html>");

  server.send(200, "text/html", page);
}

void handleBeam() {
  String newNote = server.arg("note");
  newNote.trim();

  if (newNote.length() > 0) {
    unsigned long now = millis();
    if (lifeState != LIFE_AWAKE) lifeState = LIFE_AWAKE;
    inIdleClock = false;
    manualClockActive = false;

    if (newNote.equalsIgnoreCase("#bday")) {
      stickyNoteActive = false;
      currentUI = STATE_NORMAL_HER_ROBO;
      pendingActionPublish = (int)ACTION_BDAY;
      triggerOverride(ACTION_BDAY, 6000, now);
    } else if (newNote.equalsIgnoreCase("#update")) {
      // Beams #update to her board first, then updates your board
      mqtt.publish(SEND_NOTE_TOPIC, "#update", false);
      mqtt.publish(SHARED_NOTE_TOPIC, "#update", false);
      server.send(200, "text/html", "Updating both robots from GitHub...");
      delay(400);
      runCloudOtaUpdate();
      return;
    } else if (newNote.equalsIgnoreCase("#updateher")) {
      mqtt.publish(SEND_NOTE_TOPIC, "#updateher", false);
      server.send(200, "text/html", "Sent Cloud Update command to Her Robo!");
      return;
    } else if (newNote.equalsIgnoreCase("#updateboy")) {
      server.send(200, "text/html", "Updating My Robo from GitHub...");
      delay(400);
      runCloudOtaUpdate();
      return;
    } else {
      strncpy(activeStickyNote, newNote.c_str(), 21);
      activeStickyNote[21] = '\0';
      strncpy(outgoingStickyNote, activeStickyNote, 21);
      outgoingStickyNote[21] = '\0';
      stickyNoteActive = true;
      publishNoteNeeded = true;
      incrementLoveCounter();
      playMoodSound(SND_NOTE_RECV);
    }
  }

  server.send(200, "text/html",
    "<body style='background:#111;color:#fff;font-family:sans-serif;text-align:center;padding:40px;'>"
    "<h2 style='color:#ff4081;'>Memo Beamed!</h2>"
    "<a href='/' style='color:#ffa726;'>Back to Hub</a></body>");
}

void handleSave() {
  String newEvName = server.arg("ev_name");
  String newEvDays = server.arg("ev_days");
  String newMtime  = server.arg("mtime");
  String newSsid   = server.arg("ssid");
  String newPass   = server.arg("pass");
  String newTz     = server.arg("tz");
  String newPtz    = server.arg("ptz");

  newEvName.trim();
  newMtime.trim();
  newSsid.trim();
  newTz.trim();
  newPtz.trim();

  bool ssidChanged = (newSsid.length() > 0 && newSsid != savedSsid);

  preferences.begin("robot", false);
  if (newEvName.length() > 0) {
    eventName = newEvName.substring(0, 12);
    preferences.putString("ev_name", eventName);
  }
  if (newEvDays.length() > 0) {
    eventDaysLeft = newEvDays.toInt();
    preferences.putInt("ev_days", eventDaysLeft);
  }
  if (newTz.length() > 0) {
    preferences.putString("tz_str", newTz);
    savedTz = newTz;
  }
  if (newPtz.length() > 0) {
    preferences.putString("ptz_str", newPtz);
    savedPartnerTz = newPtz;
  }
  if (newTz.length() > 0 || newPtz.length() > 0) setupNTP();
  if (newMtime.length() >= 4) setManualOfflineTime(newMtime);
  if (ssidChanged) {
    preferences.putString("ssid", newSsid);
    savedSsid = newSsid;
    if (newPass.length() > 0) {
      preferences.putString("pass", newPass);
      savedPass = newPass;
    }
  }
  preferences.end();
  lastClockEpoch = 0;

  server.send(200, "text/html",
    "<body style='background:#111;color:#fff;font-family:sans-serif;text-align:center;padding:40px;'>"
    "<h2>Settings Saved!</h2><a href='/' style='color:#ffa726;'>Back to Hub</a></body>");

  if (ssidChanged && wifiEnabled) {
    delay(400);
    WiFi.disconnect();
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    applyBatteryOptimization();
  }
}
// ==================== END OF PART 1 (PASTE PART 2 DIRECTLY BELOW) ====================
// ==================== START OF PART 2 (PASTE BELOW PART 1) ====================

// =========================================================================
// SMOOTH HELPER FUNCTIONS, VECTOR DIGITS & STYLISH BATTERY PILL
// =========================================================================
float lerpF(float a, float b, float t) {
  return a + (b - a) * t;
}

void drawCenteredText(const char* text, int y, uint16_t col, uint8_t size, bool bold = false) {
  int len = strlen(text);
  int textW = len * 6 * size - size;
  int x = (SW - textW) / 2;
  if (x < 0) x = 0;
  frame->setTextSize(size);
  frame->setTextColor(col);
  frame->setCursor(x, y);
  frame->print(text);
  if (bold) {
    frame->setCursor(x + 1, y);
    frame->print(text);
  }
}

// Stylish Architectural Battery Capsule (Visible on both Dual Clock layouts!)
void drawStylishBatteryPill(int x, int y) {
  uint16_t fillCol = ICE_CYAN;
  int pct = batteryPct;
  if (batterySensed) {
    if (pct <= 20)      fillCol = RED;
    else if (pct <= 45) fillCol = SUN_GOLD;
    else                fillCol = GREEN;
  } else {
    pct = 100;
    fillCol = ICE_CYAN;
  }

  // Outer sleek dark-slate pill + warm metallic rim + positive nub
  frame->fillRoundRect(x, y, 22, 9, 3, SLATE_DARK);
  frame->drawRoundRect(x, y, 22, 9, 3, SLATE_LIGHT);
  frame->fillRect(x + 22, y + 2, 2, 5, SLATE_LIGHT);

  // Proportional inner glowing bar (max width = 18px)
  int barW = (pct * 18) / 100;
  if (barW < 2) barW = 2;
  if (barW > 18) barW = 18;
  frame->fillRoundRect(x + 2, y + 2, barW, 5, 1, fillCol);

  // Subtle Eco-Mode indicator dot on the left of the battery
  if (battSaverMode) {
    frame->fillCircle(x - 4, y + 4, 1, GREEN);
  }
}

// Subtle bottom-edge indicator dot for Clap #1 & Too-Loud build-up
void drawSubtleBottomOverlays(unsigned long now, bool isHerFace, const SeasonTheme& st) {
  uint16_t barCol = isHerFace ? SAKURA_PINK : st.eyeRim;
  frame->fillRoundRect(46, 124, 36, 2, 1, barCol);
  frame->fillCircle(isHerFace ? 42 : 86, 125, 1, st.pCol1);

  if (validClapCount == 1 && (now - firstValidClapMs <= 750)) {
    frame->fillCircle(64, 121, 2, CYAN);
    frame->drawPixel(64, 121, WHITE);
  }

  if (isTooLoudTracking && (now > tooLoudStartMs)) {
    unsigned long elapsed = now - tooLoudStartMs;
    if (elapsed > 300) {
      int barW = ((elapsed - 300) * 80) / (MIC_TOO_LOUD_HOLD_MS - 300);
      if (barW > 80) barW = 80;
      frame->drawFastHLine(64 - (barW / 2), 121, barW, (elapsed > 1500) ? RED : SUN_GOLD);
    }
  }
}

void drawMicBadgeIcon(unsigned long now, bool activeSpeech) {
  int mx = 106, my = 8;
  uint16_t micCol = activeSpeech ? SAKURA_PINK : SUN_GOLD;
  frame->fillRoundRect(mx + 2, my, 5, 8, 2, micCol);
  frame->drawFastHLine(mx, my + 6, 9, WHITE);
  frame->drawFastVLine(mx + 4, my + 8, 4, WHITE);
  frame->drawFastHLine(mx + 2, my + 11, 5, WHITE);
  if (((now / 250) & 1) || activeSpeech) {
    frame->drawPixel(mx - 2, my + 3, CYAN);
    frame->drawPixel(mx + 10, my + 3, CYAN);
  }
}

void drawThickLine(int x0, int y0, int x1, int y1, int r, uint16_t col) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;
  while (true) {
    if (r <= 1) frame->drawPixel(x0, y0, col);
    else frame->fillCircle(x0, y0, r, col);
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void drawHeart(int cx, int cy, int r, uint16_t col) {
  frame->fillCircle(cx - r + 1, cy - (r / 2), r, col);
  frame->fillCircle(cx + r - 1, cy - (r / 2), r, col);
  frame->fillTriangle(cx - (r * 2) + 1, cy - (r / 4),
                      cx + (r * 2) - 1, cy - (r / 4),
                      cx, cy + (r * 2) - 1, col);
}

void drawTeardrop(int cx, int cy, int r, uint16_t col) {
  frame->fillCircle(cx, cy + r, r, col);
  frame->fillTriangle(cx - r, cy + r - 1, cx + r, cy + r - 1, cx, cy - r, col);
}

void drawSpiralEye(int cx, int cy, int maxR, float phaseOffset, uint16_t coreCol, uint16_t rimCol) {
  const int steps = 44;
  const float maxAngle = 2.7f * 2.0f * 3.14159f;

  // Outer warm glow halo ring
  frame->drawCircle(cx, cy, maxR + 1, rimCol);

  float prevX = cx, prevY = cy;
  for (int i = 1; i <= steps; i++) {
    float t = (float)i / (float)steps;
    float a = t * maxAngle + phaseOffset;
    float r = t * maxR;
    float x = cx + cosf(a) * r;
    float y = cy + sinf(a) * r;
    // Draw warm amber rim behind bright ivory spiral core
    drawThickLine((int)prevX, (int)prevY, (int)x, (int)y, 2, rimCol);
    drawThickLine((int)prevX, (int)prevY, (int)x, (int)y, 1, coreCol);
    prevX = x;
    prevY = y;
  }
}

void drawHappyCurveBlock(int x, int y, int w, int h, uint16_t col) {
  frame->fillRoundRect(x, y, w, h, 9, col);
  frame->fillCircle(x + (w / 2), y + h + 16, 23, BLACK);
}

void drawAngryVein(int cx, int cy, uint16_t col) {
  drawThickLine(cx - 5, cy - 2, cx - 1, cy - 2, 1, col);
  drawThickLine(cx - 1, cy - 2, cx - 1, cy - 6, 1, col);
  drawThickLine(cx + 1, cy - 6, cx + 1, cy - 2, 1, col);
  drawThickLine(cx + 1, cy - 2, cx + 5, cy - 2, 1, col);
  drawThickLine(cx - 5, cy + 2, cx - 1, cy + 2, 1, col);
  drawThickLine(cx - 1, cy + 2, cx - 1, cy + 6, 1, col);
  drawThickLine(cx + 1, cy + 6, cx + 1, cy + 2, 1, col);
  drawThickLine(cx + 1, cy + 2, cx + 5, cy + 2, 1, col);
}

void drawR2VocalModulator(int cx, int cy, unsigned long now, uint16_t col1, uint16_t col2) {
  for (int i = -2; i <= 2; i++) {
    int wave = 2 + (((now / 45) + abs(i) * 3 + currentR2ReplyId) % 9);
    uint16_t c = (abs(i) == ((now / 90) % 3)) ? col1 : col2;
    frame->fillRoundRect(cx + (i * 9) - 3, cy - (wave / 2), 6, wave, 2, c);
  }
}

// =========================================================================
// SEASONAL SPRITES & LIVE WEATHER BACKGROUND EFFECTS
// =========================================================================
void drawCherryBlossom(int cx, int cy, uint8_t size, uint8_t phase, uint16_t petalCol, uint16_t coreCol) {
  if (size >= 2) {
    float rot = (phase * 0.1f);
    for (int p = 0; p < 5; p++) {
      float a = rot + (p * 1.2566f);
      int px = cx + (int)(cosf(a) * 3.2f);
      int py = cy + (int)(sinf(a) * 3.2f);
      frame->fillCircle(px, py, 1, petalCol);
    }
    frame->fillCircle(cx, cy, 1, coreCol);
    frame->drawPixel(cx, cy, 0xD982);
  } else {
    int sway = (phase & 1) ? 1 : -1;
    frame->fillCircle(cx, cy, 2, coreCol);
    frame->drawPixel(cx + sway, cy - 2, petalCol);
    frame->drawPixel(cx - sway, cy + 2, petalCol);
    frame->drawPixel(cx + sway, cy + 1, BLACK);
  }
}

void drawSummerSunCorner(unsigned long now) {
  float pulse = sinf(now / 600.0f) * 2.0f;
  int r = 15 + (int)pulse;
  frame->drawCircle(8, 8, r + 6, 0x3120);
  frame->drawCircle(8, 8, r + 2, 0x51C0);
  frame->fillCircle(8, 8, r - 2, 0xFD64);
  frame->fillCircle(6, 6, r - 6, 0xFFDF);
}

void drawDandelionMote(int cx, int cy, uint8_t size, uint16_t goldCol, uint16_t creamCol) {
  if (size >= 2) {
    frame->fillCircle(cx, cy, 1, creamCol);
    frame->drawPixel(cx - 2, cy - 2, goldCol);
    frame->drawPixel(cx + 2, cy - 2, goldCol);
    frame->drawPixel(cx,     cy - 3, goldCol);
    frame->drawPixel(cx,     cy + 2, 0x51C0);
  } else {
    frame->fillCircle(cx, cy, 1, goldCol);
    frame->drawPixel(cx, cy, creamCol);
  }
}

void drawChinarLeaf(int cx, int cy, uint16_t leafCol, uint16_t veinCol) {
  frame->fillCircle(cx, cy, 2, leafCol);
  frame->fillTriangle(cx - 2, cy, cx + 2, cy, cx, cy - 5, leafCol);
  frame->fillTriangle(cx - 1, cy - 1, cx - 1, cy + 1, cx - 5, cy - 3, leafCol);
  frame->fillTriangle(cx + 1, cy - 1, cx + 1, cy + 1, cx + 5, cy - 3, leafCol);
  frame->fillTriangle(cx - 1, cy, cx, cy + 2, cx - 4, cy + 2, leafCol);
  frame->fillTriangle(cx + 1, cy, cx, cy + 2, cx + 4, cy + 2, leafCol);
  frame->drawFastVLine(cx, cy - 2, 7, veinCol);
}

void drawSnowflake(int cx, int cy, int sz, uint16_t col) {
  if (sz <= 1) {
    frame->drawPixel(cx, cy, col);
    return;
  }
  frame->drawFastHLine(cx - 2, cy, 5, col);
  frame->drawFastVLine(cx, cy - 2, 5, col);
  frame->drawPixel(cx - 1, cy - 1, col);
  frame->drawPixel(cx + 1, cy - 1, col);
  frame->drawPixel(cx - 1, cy + 1, col);
  frame->drawPixel(cx + 1, cy + 1, col);
}

// Live Zone Weather Background Renderer for Clock & Weather Station Cards!
// Renders real-time Sunny, Cloudy, Rainy, Snowy, or Windy effects inside any card/zone
void drawLiveWeatherZoneBg(int x0, int y0, int w, int h, int wType, unsigned long now) {
  if (wType == 0) {
    // 0: SUNNY / CLEAR -> Warm pulsing sun rays in corner + rising golden sun-motes
    int sx = x0 + w - 10;
    int sy = y0 + 10;
    int r = 6 + (((now / 400) & 1) ? 1 : 0);
    frame->drawCircle(sx, sy, r + 3, 0x3120);
    frame->drawCircle(sx, sy, r,     0x51C0);
    for (int i = 0; i < 4; i++) {
      int mx = x0 + 8 + ((i * 23 + (now / 60)) % (w - 16));
      int my = y0 + h - 4 - (((now / 45) + i * 15) % (h - 8));
      frame->drawPixel(mx, my, SUN_GOLD);
    }
  } else if (wType == 1) {
    // 1: CLOUDY / FAIR -> Soft drifting ambient cloud puffs + floating sakura
    int drift = (now / 90) % (w - 20);
    int cx = x0 + 10 + drift;
    int cy = y0 + 10;
    frame->fillRoundRect(cx - 8, cy, 16, 4, 2, 0x2124);
    frame->fillCircle(cx - 3, cy - 1, 3, 0x2124);
    frame->fillCircle(cx + 2, cy - 2, 4, 0x2124);
    for (int i = 0; i < 3; i++) {
      int px = x0 + 6 + ((i * 31 + (now / 50)) % (w - 12));
      int py = y0 + 6 + ((i * 19 + (now / 45)) % (h - 12));
      frame->drawPixel(px, py, SAKURA_LIGHT);
    }
  } else if (wType == 2) {
    // 2: RAINY -> Diagonal falling rain streaks + splash ripples at bottom of zone
    for (int i = 0; i < 5; i++) {
      int rx = x0 + 8 + ((i * 21 + (now / 25)) % (w - 14));
      int ry = y0 + 4 + (((now / 14) + (i * 17)) % (h - 8));
      if (ry >= y0 + h - 7) {
        frame->drawFastHLine(rx - 2, y0 + h - 3, 5, ICE_CYAN);
      } else {
        frame->drawLine(rx, ry, rx - 2, ry + 5, 0x39E7);
        frame->drawPixel(rx - 2, ry + 5, ICE_CYAN);
      }
    }
  } else if (wType == 3) {
    // 3: SNOWY -> Drifting snowflakes across the zone
    for (int i = 0; i < 5; i++) {
      int sx = x0 + 6 + ((i * 23 + (int)(sinf((now / 300.0f) + i) * 4.0f)) % (w - 12));
      int sy = y0 + 4 + (((now / 35) + (i * 15)) % (h - 8));
      drawSnowflake(sx, sy, (i & 1) ? 2 : 1, (i & 1) ? WHITE : 0x9E7F);
    }
  } else {
    // 4: WINDY / BREEZY -> Fast horizontal wind streamlines + tumbling Chinar leaf
    for (int i = 0; i < 3; i++) {
      int wy = y0 + 10 + (i * (h / 3));
      int wx = x0 + 4 + (((now / 18) + (i * 29)) % (w - 22));
      frame->drawFastHLine(wx, wy, 14, 0x51C0);
      frame->drawFastHLine(wx + 10, wy - 1, 6, SUN_GOLD);
    }
    int lx = x0 + 8 + (((now / 28)) % (w - 16));
    int ly = y0 + 8 + (((now / 40)) % (h - 16));
    drawChinarLeaf(lx, ly, 0xD982, 0xFA80);
  }
}

void drawWeatherIcon(int cx, int cy, int wType, unsigned long now) {
  int bob = (int)(sinf(now / 350.0f) * 1.5f);
  if (wType == 0) {
    frame->fillCircle(cx, cy + bob, 7, SUN_GOLD);
    frame->drawCircle(cx, cy + bob, 9, 0xFD64);
    for (int i = 0; i < 8; i++) {
      float a = (now / 800.0f) + (i * 0.7854f);
      frame->drawLine(cx + cosf(a) * 10, cy + bob + sinf(a) * 10,
                      cx + cosf(a) * 13, cy + bob + sinf(a) * 13, SUN_GOLD);
    }
  } else if (wType == 1) {
    frame->fillCircle(cx - 5, cy - 3 + bob, 5, SUN_GOLD);
    frame->fillRoundRect(cx - 10, cy + bob, 20, 8, 4, 0xFFDF);
    frame->fillCircle(cx - 4, cy - 1 + bob, 5, 0xFFDF);
    frame->fillCircle(cx + 3, cy - 2 + bob, 6, 0xFFDF);
  } else if (wType == 2) {
    frame->fillRoundRect(cx - 11, cy - 4 + bob, 22, 8, 4, 0x9E7F);
    frame->fillCircle(cx - 4, cy - 5 + bob, 5, 0xFFDF);
    frame->fillCircle(cx + 3, cy - 6 + bob, 6, 0xFFDF);
    int dropShift = (now / 90) % 6;
    for (int d = -1; d <= 1; d++) {
      int dx = cx + (d * 6);
      int dy = cy + 6 + ((dropShift + (d + 1) * 2) % 6);
      frame->drawLine(dx, dy, dx - 1, dy + 3, ICE_CYAN);
    }
  } else if (wType == 3) {
    frame->fillRoundRect(cx - 11, cy - 4 + bob, 22, 8, 4, 0xFFDF);
    frame->fillCircle(cx - 4, cy - 5 + bob, 5, 0xFFDF);
    frame->fillCircle(cx + 3, cy - 6 + bob, 6, 0xFFDF);
    int flakeShift = (now / 140) % 6;
    drawSnowflake(cx - 5, cy + 7 + flakeShift, 2, 0xFFFF);
    drawSnowflake(cx + 5, cy + 7 + ((flakeShift + 3) % 6), 2, 0x9E7F);
  } else {
    drawChinarLeaf(cx - 4, cy + bob, 0xD982, 0xFA80);
    frame->drawFastHLine(cx + 2, cy - 3 + bob, 9, 0xFD64);
    frame->drawFastHLine(cx + 4, cy + 3 + bob, 8, 0xFA80);
  }
}

void drawVectorDigit(int x, int y, int w, int h, int t, int digit, uint16_t col) {
  static const uint8_t SEG_MAP[10] = {
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111
  };
  if (digit < 0 || digit > 9) return;
  uint8_t mask = SEG_MAP[digit];
  int halfH = h / 2;
  int r = t / 2;

  if (mask & 0x01) frame->fillRoundRect(x + 1, y, w - 2, t, r, col);
  if (mask & 0x02) frame->fillRoundRect(x + w - t, y + 1, t, halfH, r, col);
  if (mask & 0x04) frame->fillRoundRect(x + w - t, y + halfH - 1, t, halfH + 1, r, col);
  if (mask & 0x08) frame->fillRoundRect(x + 1, y + h - t, w - 2, t, r, col);
  if (mask & 0x10) frame->fillRoundRect(x, y + halfH - 1, t, halfH + 1, r, col);
  if (mask & 0x20) frame->fillRoundRect(x, y + 1, t, halfH, r, col);
  if (mask & 0x40) frame->fillRoundRect(x + 2, y + halfH - (t / 2), w - 4, t, r, col);
}

void drawVectorTemp(int centerX, int y, int w, int h, int t, int tempC, uint16_t digitCol, uint16_t degCol) {
  bool isNeg = (tempC < 0);
  int val = abs(tempC);
  if (val > 99) val = 99;

  if (val >= 10) {
    int startX = centerX - w - 2;
    if (isNeg) frame->fillRoundRect(startX - 7, y + (h / 2) - 1, 5, t, 1, digitCol);
    drawVectorDigit(startX,         y, w, h, t, val / 10, digitCol);
    drawVectorDigit(startX + w + 3, y, w, h, t, val % 10, digitCol);
    frame->drawCircle(startX + (w * 2) + 7, y + 2, 2, degCol);
  } else {
    int startX = centerX - (w / 2);
    if (isNeg) frame->fillRoundRect(startX - 8, y + (h / 2) - 1, 6, t, 1, digitCol);
    drawVectorDigit(startX, y, w, h, t, val, digitCol);
    frame->drawCircle(startX + w + 4, y + 2, 2, degCol);
  }
}

// =========================================================================
// SEASONAL & LIVE WEATHER PARTICLE ENGINE (FOR FACE SCREENS)
// =========================================================================
SeasonType getActiveSeasonForCurrentScreen() {
  if (manualSeasonOverride) return currentSeason;
  bool showingHer = (currentUI != STATE_MY_ROBO);
  int wType = showingHer ? herWeatherType : myWeatherType;
  int temp  = showingHer ? herTempC      : myTempC;
  int wind  = showingHer ? herWindKmh    : myWindKmh;
  if (wType == 3 || temp <= 2)  return SEASON_WINTER_SNOW;
  if (wType == 2)               return SEASON_MONSOON_RAIN;
  if (wType == 4 || wind >= 18) return SEASON_AUTUMN_CHINAR;
  if (wType == 0)               return SEASON_GOLDEN_SUMMER;
  return SEASON_SPRING_SAKURA;
}

void spawnParticle(int i, bool randomY, SeasonType sType) {
  particles[i].x = (float)(10 + (esp_random() % 108));
  particles[i].phase = esp_random() % 64;
  particles[i].size = (i % 3 == 0) ? 2 : 1;

  switch (sType) {
    case SEASON_SPRING_SAKURA:
      particles[i].y  = randomY ? (float)(esp_random() % 120) : -6.0f;
      particles[i].vx = 0.38f + (esp_random() % 30) * 0.01f;
      particles[i].vy = 0.48f + (esp_random() % 30) * 0.01f;
      break;
    case SEASON_GOLDEN_SUMMER:
      particles[i].y  = randomY ? (float)(esp_random() % 120) : 126.0f;
      particles[i].vx = 0.20f + (esp_random() % 25) * 0.01f;
      particles[i].vy = -0.38f - (esp_random() % 25) * 0.01f;
      break;
    case SEASON_AUTUMN_CHINAR:
      particles[i].y  = randomY ? (float)(esp_random() % 110) : -6.0f;
      particles[i].vx = 0.45f + (esp_random() % 35) * 0.01f;
      particles[i].vy = 0.65f + (esp_random() % 35) * 0.01f;
      break;
    case SEASON_WINTER_SNOW:
      particles[i].y  = randomY ? (float)(esp_random() % 120) : -4.0f;
      particles[i].vx = -0.15f + (esp_random() % 30) * 0.01f;
      particles[i].vy = 0.45f + (esp_random() % 40) * 0.01f;
      break;
    case SEASON_MONSOON_RAIN:
      particles[i].y  = randomY ? (float)(esp_random() % 120) : -8.0f;
      particles[i].vx = -0.8f;
      particles[i].vy = 3.8f + (esp_random() % 15) * 0.1f;
      break;
    default:
      break;
  }
}

void initWeatherParticles() {
  SeasonType sType = getActiveSeasonForCurrentScreen();
  for (int i = 0; i < MAX_PARTICLES; i++) {
    spawnParticle(i, true, sType);
  }
  trackedParticleIdx = 0;
}

void updateAndDrawWeather(unsigned long now, bool onFaceScreen) {
  SeasonType sType = getActiveSeasonForCurrentScreen();
  const SeasonTheme& st = SEASONS[sType];
  int activeCount = (sType == SEASON_AUTUMN_CHINAR) ? 4 : MAX_PARTICLES;

  if (sType == SEASON_GOLDEN_SUMMER && onFaceScreen) {
    drawSummerSunCorner(now);
  } else if (sType == SEASON_AUTUMN_CHINAR && onFaceScreen) {
    // Subtle breezy wind streamlines in Windy/Chinar mode
    for (int w = 0; w < 2; w++) {
      int wy = 24 + (w * 64);
      int wx = 10 + (((now / 20) + w * 45) % 90);
      frame->drawFastHLine(wx, wy, 18, 0x51C0);
    }
  }

  for (int i = 0; i < activeCount; i++) {
    WeatherParticle& p = particles[i];

    if (sType == SEASON_SPRING_SAKURA) {
      p.y += p.vy;
      p.x += p.vx + sinf((now / 300.0f) + p.phase) * 0.45f;
      if (p.x > 130.0f) p.x = -4.0f;
      if (p.y > 126.0f) {
        spawnParticle(i, false, sType);
        if (p.size >= 2) trackedParticleIdx = i;
      }
      drawCherryBlossom((int)p.x, (int)p.y, p.size, p.phase + (now / 90), st.pCol1, st.pCol2);

    } else if (sType == SEASON_GOLDEN_SUMMER) {
      p.y += p.vy;
      p.x += p.vx + sinf((now / 420.0f) + p.phase) * 0.35f;
      if (p.x > 130.0f) p.x = -2.0f;
      if (p.y < -6.0f) {
        spawnParticle(i, false, sType);
        trackedParticleIdx = i;
      }
      drawDandelionMote((int)p.x, (int)p.y, p.size, st.pCol1, st.pCol2);

    } else if (sType == SEASON_AUTUMN_CHINAR) {
      p.y += p.vy;
      p.x += p.vx + sinf((now / 320.0f) + p.phase) * 0.75f;
      if (p.x > 130.0f) p.x = -4.0f;
      if (p.y > 126.0f) {
        spawnParticle(i, false, sType);
        trackedParticleIdx = i;
      }
      uint16_t lCol = (i & 1) ? st.pCol1 : st.pCol2;
      drawChinarLeaf((int)p.x, (int)p.y, lCol, st.pCol2);

    } else if (sType == SEASON_WINTER_SNOW) {
      p.y += p.vy;
      p.x += sinf((now / 480.0f) + p.phase) * 0.35f;
      if (onFaceScreen && p.size > 1 && p.y >= curLy - 2 && p.y <= curLy + 4) {
        if ((p.x >= curLx && p.x <= curLx + curLw) || (p.x >= curRx && p.x <= curRx + curRw)) {
          snowSquintUntil = now + 420;
          spawnParticle(i, false, sType);
          continue;
        }
      }
      if (p.y > 126.0f) spawnParticle(i, false, sType);
      drawSnowflake((int)p.x, (int)p.y, p.size, (p.size > 1) ? st.pCol1 : st.pCol2);

    } else if (sType == SEASON_MONSOON_RAIN) {
      p.y += p.vy;
      p.x += p.vx;
      if (p.y >= 118.0f) {
        int rx = (int)p.x;
        frame->drawFastHLine(rx - 3, 121, 7, st.pCol1);
        frame->drawPixel(rx - 2, 119, st.pCol2);
        frame->drawPixel(rx + 2, 119, st.pCol2);
        spawnParticle(i, false, sType);
      } else {
        int ix = (int)p.x, iy = (int)p.y;
        frame->drawLine(ix, iy, ix - 2, iy + 7, st.pCol1);
      }
    }
  }
}

void cycleNextSeason(unsigned long now) {
  manualSeasonOverride = true;
  currentSeason = (SeasonType)(((int)currentSeason + 1) % SEASON_TOTAL);
  initWeatherParticles();
  screenEnteredAt = now;
  playMoodSound(SND_MENU_TICK);
}

// =========================================================================
// BORDERLESS WARM EYE PRIMITIVE & MINI GLYPH
// =========================================================================
void drawSoftWarmEye(int x, int y, int w, int h, int r, bool isLeft, bool isHerFace, const SeasonTheme& st) {
  if (h < 4) h = 4;
  if (r > h / 2) r = h / 2;

  uint16_t rimCol  = isHerFace ? 0xFACA : st.eyeRim;
  uint16_t haloCol = isHerFace ? 0x48E2 : st.eyeHalo;
  uint16_t coreCol = isHerFace ? 0xFF7B : st.eyeCore;

  frame->drawRoundRect(x - 2, y - 2, w + 4, h + 4, r + 2, haloCol);
  frame->drawRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, rimCol);
  frame->fillRoundRect(x, y, w, h, r, coreCol);

  if (isHerFace && h >= 14) {
    if (isLeft) {
      frame->drawLine(x - 4, y + 2, x, y + 5, rimCol);
      frame->drawLine(x - 3, y + 1, x, y + 4, coreCol);
    } else {
      frame->drawLine(x + w - 1, y + 5, x + w + 3, y + 2, rimCol);
      frame->drawLine(x + w - 1, y + 4, x + w + 2, y + 1, coreCol);
    }
  }

  if (h >= 18) {
    int sparkX = x + 6 + (int)(curPupilX * 0.4f);
    int sparkY = y + 6 + (int)(curPupilY * 0.4f);
    frame->fillCircle(sparkX, sparkY, 2, 0xFFFF);
  }
}

void drawMiniEyeGlyph(int cx, int cy, Mood m, bool isHer, const SeasonTheme& st) {
  int ew = 9, eh = 10, r = isHer ? 4 : 2;
  int lx = cx - 11, rx = cx + 2, y = cy - 5;
  uint16_t rimCol  = isHer ? 0xFACA : st.eyeRim;
  uint16_t haloCol = isHer ? 0x48E2 : st.eyeHalo;
  uint16_t coreCol = isHer ? 0xFF7B : st.eyeCore;

  if (m == LOVE) {
    drawHeart(cx - 6, cy, 4, SAKURA_PINK);
    drawHeart(cx + 6, cy, 4, SAKURA_PINK);
    return;
  }

  frame->drawRoundRect(lx - 1, y - 1, ew + 2, eh + 2, r + 1, haloCol);
  frame->drawRoundRect(rx - 1, y - 1, ew + 2, eh + 2, r + 1, haloCol);
  frame->fillRoundRect(lx, y, ew, eh, r, (m == ANGRY) ? RED : coreCol);
  frame->fillRoundRect(rx, y, ew, eh, r, (m == ANGRY) ? RED : coreCol);

  if (isHer) {
    frame->drawLine(lx - 2, y + 1, lx, y, rimCol);
    frame->drawLine(rx + ew - 1, y, rx + ew + 1, y + 1, rimCol);
  }

  if (m == HAPPY) {
    frame->fillCircle(lx + (ew / 2), y + eh + 2, 5, SLATE_DARK);
    frame->fillCircle(rx + (ew / 2), y + eh + 2, 5, SLATE_DARK);
  } else if (m == ANGRY || m == FROWN) {
    frame->fillTriangle(lx, y, lx + ew, y, lx + ew, y + 4, SLATE_DARK);
    frame->fillTriangle(rx, y, rx + ew, y, rx, y + 4, SLATE_DARK);
  }
}

// =========================================================================
// 16-VARIANT R2-D2 ASTRO-DROID VISUAL REPLY RENDERER (BORDERLESS WARM)
// =========================================================================
void renderR2D2ReplyFace(unsigned long now, bool isHerFace, const SeasonTheme& st) {
  int v = currentR2ReplyId % 16;
  int bob = (int)(fabsf(sinf(now / 85.0f)) * -6.0f);
  int wig = ((now / 60) & 1) ? 2 : -2;
  uint16_t cCore = isHerFace ? 0xFF7B : st.eyeCore;

  switch (v) {
    case 0:
      drawSoftWarmEye(22, 40 + bob, 32, 24, 10, true, isHerFace, st);
      drawSoftWarmEye(74, 40 + bob, 32, 24, 10, false, isHerFace, st);
      frame->fillCircle(38, 66 + bob, 18, BLACK);
      frame->fillCircle(90, 66 + bob, 18, BLACK);
      drawR2VocalModulator(64, 94, now, st.eyeRim, st.pCol1);
      break;
    case 1:
      drawSoftWarmEye(20, 50 + (bob / 2), 30, 24, 6, true, isHerFace, st);
      drawSoftWarmEye(74, 38 + (bob / 2), 34, 36, 10, false, isHerFace, st);
      drawR2VocalModulator(64, 94, now, SUN_GOLD, CYAN);
      break;
    case 2:
      drawSoftWarmEye(22 + wig, 44, 32, 32, 8, true, isHerFace, st);
      drawHappyCurveBlock(74 + wig, 48, 32, 22, cCore);
      drawR2VocalModulator(64, 94, now, SAKURA_PINK, st.eyeRim);
      break;
    case 3: {
      int scanX = (int)(sinf(now / 55.0f) * 8.0f);
      drawSoftWarmEye(22 + scanX, 46, 30, 26, 6, true, isHerFace, st);
      drawSoftWarmEye(76 + scanX, 46, 30, 26, 6, false, isHerFace, st);
      drawR2VocalModulator(64, 94, now, GREEN, CYAN);
      break;
    }
    case 4:
      drawHappyCurveBlock(22 + wig, 42 + bob, 32, 24, cCore);
      drawHappyCurveBlock(74 + wig, 42 + bob, 32, 24, cCore);
      frame->fillCircle(18 + wig, 74 + bob, 5, SAKURA_PINK);
      frame->fillCircle(110 + wig, 74 + bob, 5, SAKURA_PINK);
      drawR2VocalModulator(64, 94, now, SAKURA_PINK, SAKURA_LIGHT);
      break;
    case 5: {
      int zoomR = 14 + (((now / 70) & 1) ? 2 : -1);
      frame->drawCircle(38, 56, zoomR + 1, st.eyeRim);
      frame->drawCircle(90, 56, zoomR + 1, st.eyeRim);
      frame->fillCircle(38, 56, zoomR - 3, cCore);
      frame->fillCircle(90, 56, zoomR - 3, cCore);
      drawR2VocalModulator(64, 94, now, SUN_GOLD, ORANGE);
      break;
    }
    case 6:
      drawThickLine(24, 44 + bob, 48, 56 + bob, 2, cCore);
      drawThickLine(48, 56 + bob, 24, 68 + bob, 2, cCore);
      drawThickLine(104, 44 + bob, 80, 56 + bob, 2, cCore);
      drawThickLine(80, 56 + bob, 104, 68 + bob, 2, cCore);
      drawR2VocalModulator(64, 94, now, GREEN, SUN_GOLD);
      break;
    case 7:
      drawHappyCurveBlock(22, 46, 32, 22, cCore);
      drawHappyCurveBlock(74, 46, 32, 22, cCore);
      drawHeart(64, 30 + (bob / 2), 6, SAKURA_PINK);
      drawR2VocalModulator(64, 94, now, SAKURA_PINK, RED);
      break;
    case 8:
      drawSoftWarmEye(22, 34 + bob, 32, 40, 9, true, isHerFace, st);
      drawSoftWarmEye(74, 34 + bob, 32, 40, 9, false, isHerFace, st);
      drawR2VocalModulator(64, 94, now, SUN_GOLD, CYAN);
      break;
    case 9: {
      int slyX = (int)(sinf(now / 120.0f) * 6.0f);
      drawSoftWarmEye(22 + slyX, 54, 32, 16, 4, true, isHerFace, st);
      drawSoftWarmEye(74 + slyX, 54, 32, 16, 4, false, isHerFace, st);
      drawR2VocalModulator(64, 94, now, ORANGE, SUN_GOLD);
      break;
    }
    case 10: {
      int alt = (int)(sinf(now / 65.0f) * 8.0f);
      drawSoftWarmEye(22, 44 + alt, 32, 28, 7, true, isHerFace, st);
      drawSoftWarmEye(74, 44 - alt, 32, 28, 7, false, isHerFace, st);
      drawR2VocalModulator(64, 94, now, CYAN, SAKURA_PINK);
      break;
    }
    case 11:
      drawSoftWarmEye(22, 48, 32, 26, 6, true, isHerFace, st);
      drawSoftWarmEye(74, 48, 32, 26, 6, false, isHerFace, st);
      frame->fillTriangle(20, 46, 56, 46, 20, 58, BLACK);
      frame->fillTriangle(72, 46, 108, 46, 108, 58, BLACK);
      drawR2VocalModulator(64, 94, now, ICE_CYAN, BLUE);
      break;
    case 12: {
      int nodY = (int)(sinf(now / 70.0f) * 9.0f);
      drawSoftWarmEye(22, 44 + nodY, 32, 30, 7, true, isHerFace, st);
      drawSoftWarmEye(74, 44 + nodY, 32, 30, 7, false, isHerFace, st);
      drawR2VocalModulator(64, 94, now, GREEN, cCore);
      break;
    }
    case 13: {
      frame->drawRoundRect(22, 44, 32, 30, 6, st.eyeRim);
      frame->drawRoundRect(74, 44, 32, 30, 6, st.eyeRim);
      int scanLineY = 46 + ((now / 18) % 26);
      frame->drawFastHLine(24, scanLineY, 28, GREEN);
      frame->drawFastHLine(76, scanLineY, 28, GREEN);
      drawR2VocalModulator(64, 94, now, GREEN, CYAN);
      break;
    }
    case 14:
      drawHappyCurveBlock(22, 44 + bob, 32, 22, cCore);
      drawSoftWarmEye(74, 40 + bob, 32, 32, 8, false, isHerFace, st);
      drawR2VocalModulator(64, 94, now, SUN_GOLD, SAKURA_PINK);
      break;
    case 15:
      drawHappyCurveBlock(22 + wig, 40 + bob, 32, 24, cCore);
      drawHappyCurveBlock(74 - wig, 40 + bob, 32, 24, cCore);
      frame->fillCircle(20, 26, 2, SUN_GOLD);
      frame->fillCircle(108, 28, 2, CYAN);
      frame->fillCircle(64, 22, 2, SAKURA_PINK);
      drawR2VocalModulator(64, 94, now, SAKURA_PINK, SUN_GOLD);
      break;
  }
}

// =========================================================================
// VOICE CHAT, SOUND PARTY, STICKY NOTE, SLEEPING, HEARTBEAT & POMODORO
// =========================================================================
void renderVoiceChatScreen(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  if (hasLocalOverride && localOverride == MIC_R2D2_REPLY) {
    if (now < overrideEndsAt) {
      renderR2D2ReplyFace(now, true, st);
      return;
    }
    hasLocalOverride = false;
  }

  drawMicBadgeIcon(now, isVoiceListening);

  if (isVoiceListening) {
    drawSoftWarmEye(20, 38, 34, 34, 10, true, true, st);
    drawSoftWarmEye(74, 38, 34, 34, 10, false, true, st);

    int talkSpan = (micSmoothLevel > MIC_TALK_THRESHOLD) ? (micSmoothLevel - MIC_TALK_THRESHOLD) : 0;
    int mouthH = 4 + (talkSpan * 12) / 2500;
    if (mouthH > 16) mouthH = 16;
    int mouthW = 14 + (talkSpan * 12) / 2500;
    if (mouthW > 26) mouthW = 26;
    frame->fillRoundRect(64 - (mouthW / 2), 88 - (mouthH / 2), mouthW, mouthH, 4, SAKURA_PINK);
    if (mouthH > 6) {
      frame->fillRoundRect(66 - (mouthW / 2), 90 - (mouthH / 2), mouthW - 4, mouthH - 4, 2, BLACK);
    }

    int waveH = 2 + (talkSpan / 300);
    if (waveH > 10) waveH = 10;
    frame->fillRect(34, 88 - waveH, 3, waveH * 2, SUN_GOLD);
    frame->fillRect(91, 88 - waveH, 3, waveH * 2, SUN_GOLD);
  } else {
    int breathY = (int)(sinf(now / 320.0f) * 2.0f);
    unsigned long blinkPhase = now % 3600;
    if (blinkPhase >= 3460) {
      drawSoftWarmEye(20, 56, 34, 5, 2, true, true, st);
      drawSoftWarmEye(74, 56, 34, 5, 2, false, true, st);
    } else {
      drawSoftWarmEye(20, 44 + breathY, 34, 34, 10, true, true, st);
      drawSoftWarmEye(74, 44 + breathY, 34, 34, 10, false, true, st);
    }
    int activeDot = (now / 280) % 3;
    for (int d = 0; d < 3; d++) {
      frame->fillCircle(56 + (d * 8), 96, (d == activeDot) ? 2 : 1, (d == activeDot) ? SUN_GOLD : SLATE_LIGHT);
    }
  }
}

void renderSoundPartyScreen(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  int visLevel = (micSmoothLevel > 4000) ? 4000 : micSmoothLevel;
  int beatBounce = -(visLevel * 9) / 4000;
  if (beatBounce < -9) beatBounce = -9;

  frame->drawRoundRect(14, 18 + beatBounce, 100, 44, 18, SUN_GOLD);
  frame->fillRoundRect(8,  40 + beatBounce, 11,  26, 4,  SAKURA_PINK);
  frame->fillRoundRect(109,40 + beatBounce, 11,  26, 4,  SAKURA_PINK);

  if (micSmoothLevel >= MIC_TALK_THRESHOLD) {
    drawHappyCurveBlock(24, 38 + beatBounce, 30, 22, st.eyeCore);
    drawHappyCurveBlock(74, 38 + beatBounce, 30, 22, st.eyeCore);
  } else {
    int idleBob = (int)(sinf(now / 260.0f) * 2.0f);
    drawSoftWarmEye(24, 38 + idleBob, 30, 24, 6, true, false, st);
    drawSoftWarmEye(74, 38 + idleBob, 30, 24, 6, false, false, st);
  }

  int mouthH = 2 + (visLevel * 12) / 4000;
  if (mouthH > 12) mouthH = 12;
  if (micSmoothLevel >= (MIC_TALK_THRESHOLD / 2)) {
    frame->fillRoundRect(54, 70 + beatBounce - (mouthH / 2), 20, mouthH + 2, 4, SAKURA_PINK);
  }

  const uint16_t barCols[7] = { ICE_CYAN, GREEN, SUN_GOLD, ORANGE, SAKURA_PINK, SUN_GOLD, ICE_CYAN };
  for (int b = 0; b < 7; b++) {
    int bx = 19 + (b * 13);
    int bh = 3 + micBands[b];
    if (bh > 26) bh = 26;
    frame->fillRoundRect(bx, 118 - bh, 9, bh, 2, barCols[b]);
  }
}

void renderStickyNoteScreen(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];
  int bobY = (int)(sinf(now / 180.0f) * 2.0f);

  drawHappyCurveBlock(24, 14 + bobY, 28, 20, st.eyeCore);
  drawHappyCurveBlock(76, 14 + bobY, 28, 20, st.eyeCore);

  frame->fillRoundRect(8, 38 + bobY, 112, 56, 8, SLATE_DARK);
  frame->drawRoundRect(8, 38 + bobY, 112, 56, 8, SUN_GOLD);

  drawCenteredText("MEMO FOR YOU", 47 + bobY, SAKURA_PINK, 1, true);
  drawCenteredText(activeStickyNote, 66 + bobY, st.eyeCore, 1, true);
  drawCenteredText("Tap any button", 108, SLATE_LIGHT, 1, false);
}

void renderSleepingFace(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];
  int breathY = (int)(sinf(now / 650.0f) * 3.0f);

  drawSoftWarmEye(20, 60 + breathY, 34, 7, 3, true, true, st);
  drawSoftWarmEye(74, 60 + breathY, 34, 7, 3, false, true, st);

  for (int i = 0; i < 3; i++) {
    int my = 46 - (((now / 55) + i * 14) % 34);
    int mx = 44 + (i * 20) + (int)(sinf((now / 300.0f) + i) * 4.0f);
    frame->fillCircle(mx, my, (i == 1) ? 2 : 1, (i == 1) ? st.eyeRim : st.eyeHalo);
  }
  drawSubtleBottomOverlays(now, true, st);
}

void renderWakingUpFace(unsigned long elapsed, unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  if (elapsed < 950) {
    bool slitBlink = ((elapsed / 220) % 2 == 1);
    int h = slitBlink ? 5 : 12;
    drawSoftWarmEye(20, 58, 34, h, 3, true, true, st);
    drawSoftWarmEye(74, 58, 34, h, 3, false, true, st);
  } else if (elapsed < 1950) {
    int stretchY = (int)(sinf((elapsed - 950) / 318.0f * 3.14159f) * -6.0f);
    drawHappyCurveBlock(20, 48 + stretchY, 34, 22, st.eyeCore);
    drawHappyCurveBlock(74, 48 + stretchY, 34, 22, st.eyeCore);
    int yawnR = 4 + (int)(sinf((elapsed - 950) / 1000.0f * 3.14159f) * 7.0f);
    frame->drawCircle(64, 84, yawnR, SAKURA_PINK);
  } else {
    int popBounce = (int)(sinf((elapsed - 1950) / 100.0f) * 4.0f);
    drawSoftWarmEye(20, 44 + popBounce, 34, 36, 12, true, true, st);
    drawSoftWarmEye(74, 44 + popBounce, 34, 36, 12, false, true, st);
  }
}

void renderHeartbeatScreen(unsigned long now, bool isIncomingPlayback) {
  frame->fillScreen(BLACK);
  bool isPulsing = (now < hbPulseUntilMs);
  int heartRadius = isPulsing ? 22 : 14;
  uint16_t heartCol = isPulsing ? RED : SAKURA_PINK;

  if (isPulsing) {
    frame->drawCircle(64, 58, 34, SAKURA_LIGHT);
    frame->drawCircle(64, 58, 38, SLATE_MID);
  }
  drawHeart(64, 58, heartRadius, heartCol);

  if (isIncomingPlayback) {
    drawCenteredText("HER HEARTBEAT", 12, SAKURA_LIGHT, 1, true);
    char beatStr[20];
    snprintf(beatStr, sizeof(beatStr), "Beat %d / %d", hbPlayIndex, hbPlayTotal);
    drawCenteredText(beatStr, 106, WHITE, 1, false);
  } else {
    drawCenteredText("HEARTBEAT SYNC", 12, SUN_GOLD, 1, true);
    if (hbSentBanner && now < hbSentBannerUntil) {
      drawCenteredText("SENT TO HER <3", 98, GREEN, 1, true);
    } else if (hbTapCount > 0) {
      char tapStr[22];
      snprintf(tapStr, sizeof(tapStr), "Taps: %d (Wait=Send)", hbTapCount);
      drawCenteredText(tapStr, 98, ICE_CYAN, 1, false);
    } else {
      drawCenteredText("Tap Top or SEL", 96, WHITE, 1, false);
      drawCenteredText("BACK: Exit", 110, SLATE_LIGHT, 1, false);
    }
  }
}

void renderPomodoroScreen(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  if (pomoPhase == POMO_IDLE) {
    drawCenteredText("SHARED FOCUS", 12, SUN_GOLD, 1, true);
    frame->fillRoundRect(16, 28, 96, 7, 3, RED);
    frame->fillCircle(64, 31, 4, st.eyeCore);
    drawSoftWarmEye(24, 40, 30, 22, 6, true, false, st);
    drawSoftWarmEye(74, 40, 30, 22, 6, false, false, st);
    drawCenteredText(pomoSelectedBreak ? "Mode: 5m Break" : "Mode: 25m Focus", 74, ICE_CYAN, 1, true);
    drawCenteredText("SEL: Start Both", 92, GREEN, 1, false);
    drawCenteredText("UP: Switch 25/5m", 106, SLATE_LIGHT, 1, false);
    return;
  }

  unsigned long remMs = (pomoPhaseEndsAt > now) ? (pomoPhaseEndsAt - now) : 0;
  int totalSec = remMs / 1000UL;
  int mins = totalSec / 60;
  int secs = totalSec % 60;

  if (pomoPhase == POMO_FOCUS) {
    frame->fillRoundRect(14, 14, 100, 7, 3, RED);
    frame->fillCircle(64, 17, 3, st.eyeCore);
    drawSoftWarmEye(24, 28, 30, 22, 6, true, false, st);
    drawSoftWarmEye(74, 28, 30, 22, 6, false, false, st);
    frame->fillTriangle(22, 26, 56, 26, 56, 34, BLACK);
    frame->fillTriangle(72, 26, 106, 26, 72, 34, BLACK);

    drawVectorDigit(18, 62, 16, 28, 3, mins / 10, st.eyeCore);
    drawVectorDigit(38, 62, 16, 28, 3, mins % 10, st.eyeCore);
    frame->fillCircle(63, 70, 1, SUN_GOLD);
    frame->fillCircle(63, 80, 1, SUN_GOLD);
    drawVectorDigit(70, 62, 16, 28, 3, secs / 10, st.eyeRim);
    drawVectorDigit(90, 62, 16, 28, 3, secs % 10, st.eyeRim);

    int elapsedPct = 100 - (int)((remMs * 100ULL) / pomoTotalDurationMs);
    if (elapsedPct < 0) elapsedPct = 0;
    if (elapsedPct > 100) elapsedPct = 100;
    frame->fillRoundRect(14, 104, 100, 6, 3, SLATE_DARK);
    if (elapsedPct > 0) {
      frame->fillRoundRect(14, 104, elapsedPct, 6, 3, SUN_GOLD);
    }
  } else {
    int bounce = (int)(fabsf(sinf(now / 130.0f)) * -6.0f);
    drawHappyCurveBlock(24, 24 + bounce, 30, 22, st.eyeCore);
    drawHappyCurveBlock(74, 24 + bounce, 30, 22, st.eyeCore);
    drawHeart(64, 50 + bounce, 5, SAKURA_PINK);
    drawVectorDigit(18, 64, 16, 28, 3, mins / 10, GREEN);
    drawVectorDigit(38, 64, 16, 28, 3, mins % 10, GREEN);
    drawVectorDigit(70, 64, 16, 28, 3, secs / 10, ICE_CYAN);
    drawVectorDigit(90, 64, 16, 28, 3, secs % 10, ICE_CYAN);
    drawCenteredText("BREAK TIME", 104, GREEN, 1, true);
  }
}

// =========================================================================
// BORDERLESS WARM FACE RENDERER (GLITCH-FREE ASCII PILLS + LIVE WEATHER)
// =========================================================================
void renderFace(Mood m, unsigned long now, bool isHerFace) {
  if (m == SLEEPING) {
    renderSleepingFace(now);
    return;
  }
  SeasonType sType      = getActiveSeasonForCurrentScreen();
  const SeasonTheme& st = SEASONS[sType];
  uint16_t cCore        = isHerFace ? 0xFF7B : st.eyeCore;

  if (m == MIC_R2D2_REPLY) {
    frame->fillScreen(BLACK);
    renderR2D2ReplyFace(now, isHerFace, st);
    return;
  }

  frame->fillScreen(BLACK);

  if (currentUI == STATE_NORMAL_HER_ROBO && !hasLocalOverride && isBlowingMic) {
    int windShift = (now / 25) % 24;
    drawHappyCurveBlock(22, 44, 32, 20, cCore);
    drawHappyCurveBlock(74, 44, 32, 20, cCore);
    for (int w = 0; w < 3; w++) {
      int wy = 28 + (w * 28);
      int wx = 18 + ((windShift + w * 9) % 70);
      frame->drawFastHLine(wx, wy, 22, SUN_GOLD);
    }
    drawSubtleBottomOverlays(now, isHerFace, st);
    return;
  }

  if (m == LOVE) {
    float pulse = sinf(now / 150.0f);
    int r = 10 + (pulse > 0.3f ? 1 : 0);
    int lift = (int)(pulse * 3.0f);
    drawHeart(38, 60 + lift, r, SAKURA_PINK);
    drawHeart(90, 60 + lift, r, SAKURA_PINK);
    updateAndDrawWeather(now, false);
    drawSubtleBottomOverlays(now, isHerFace, st);
    return;
  } else if (m == ACTION_HUG) {
    drawHappyCurveBlock(20, 34, 34, 22, cCore);
    drawHappyCurveBlock(74, 34, 34, 22, cCore);
    drawHeart(64, 82, 13 + ((now / 150) & 1), SAKURA_PINK);
    updateAndDrawWeather(now, false);
    return;
  } else if (m == ACTION_KISS) {
    drawSoftWarmEye(20, 34, 34, 24, 9, true, isHerFace, st);
    drawHappyCurveBlock(74, 34, 34, 22, cCore);
    frame->fillCircle(64, 66, 4, SAKURA_PINK);
    int progress = (now / 22) % 34;
    int driftX = (int)(sinf(progress / 4.0f) * 6.0f);
    drawHeart(64 + driftX, 96 - progress, 4 + (progress / 7), SAKURA_PINK);
    return;
  } else if (m == ACTION_POKE) {
    unsigned long elapsed = (now - overrideStartedAt) % 950;
    float jab = (elapsed < 260) ? (elapsed / 260.0f) : ((elapsed < 560) ? (1.0f - (elapsed - 260) / 300.0f) : 0.0f);
    int recoilX = -(int)(jab * 11.0f);
    int fingerTipX = 114 - (int)(jab * 34.0f);
    drawSoftWarmEye(20 + recoilX, 46, 32, 30, 8, true, isHerFace, st);
    drawThickLine(70 + recoilX, 50, 88 + recoilX, 62, 2, cCore);
    drawThickLine(88 + recoilX, 62, 70 + recoilX, 74, 2, cCore);
    if (jab > 0.25f) drawAngryVein(32 + recoilX, 26, RED);
    frame->fillRoundRect(fingerTipX, 58, 28, 12, 5, SUN_GOLD);
    return;
  } else if (m == ACTION_BDAY) {
    int bounce = (int)(fabsf(sinf(now / 160.0f)) * -4.0f);
    drawHappyCurveBlock(20, 34 + bounce, 32, 20, cCore);
    drawHappyCurveBlock(76, 34 + bounce, 32, 20, cCore);
    frame->fillRoundRect(42, 78, 44, 14, 3, SAKURA_PINK);
    frame->fillRoundRect(48, 68, 32, 11, 2, SAKURA_LIGHT);
    frame->fillRect(62, 59, 4, 9, WHITE);
    frame->fillCircle(64 + (((now / 90) & 1) ? 1 : -1), 55, 3, SUN_GOLD);
    drawCenteredText("HAPPY BIRTHDAY", 102, SUN_GOLD, 1, true);
    return;
  } else if (m == TWIN_SYNC) {
    int bounce = (int)(fabsf(sinf(now / 110.0f)) * -7.0f);
    drawHappyCurveBlock(20, 36 + bounce, 34, 22, cCore);
    drawHappyCurveBlock(74, 36 + bounce, 34, 22, cCore);
    drawHeart(48, 78 + bounce, 8, SAKURA_PINK);
    drawHeart(80, 78 + bounce, 8, SUN_GOLD);
    drawCenteredText("TWIN MOOD SYNC", 14, SUN_GOLD, 1, true);
    return;
  } else if (m == HYDRATE_REMIND) {
    int bob = (int)(sinf(now / 160.0f) * 3.0f);
    drawHappyCurveBlock(20, 26 + bob, 34, 20, cCore);
    drawHappyCurveBlock(74, 26 + bob, 34, 20, cCore);
    frame->drawRoundRect(50, 56 + bob, 28, 34, 4, WHITE);
    frame->fillRoundRect(52, 64 + bob, 24, 24, 3, ICE_CYAN);
    drawCenteredText("DRINK WATER", 102, ICE_CYAN, 1, true);
    return;
  } else if (m == MIC_STARTLED) {
    int jitX = ((now / 28) & 1) ? 3 : -3;
    int jitY = ((now / 38) & 1) ? -3 : 2;
    frame->drawRoundRect(20 + jitX, 34 + jitY, 34, 34, 10, SUN_GOLD);
    frame->drawRoundRect(74 - jitX, 34 + jitY, 34, 34, 10, SUN_GOLD);
    frame->fillCircle(37 + jitX, 51 + jitY, 4, cCore);
    frame->fillCircle(91 - jitX, 51 + jitY, 4, cCore);
    drawTeardrop(108, 22 + ((now / 35) % 10), 4, ICE_CYAN);
    frame->drawCircle(64, 78 + jitY, 6, SAKURA_PINK);
    drawCenteredText("TOO LOUD!", 102, SUN_GOLD, 1, true);
    return;
 } else if (m == IRRITATED) {
    // Visibly shakes harder as shakeEnergy builds up toward fainting!
    int amp = 2 + (int)(shakeEnergy * 1.5f);
    if (amp > 6) amp = 6;
    int jx = ((now / 28) & 1) ? amp : -amp;
    int jy = ((now / 36) & 1) ? (amp / 2) : -(amp / 2);
    drawThickLine(22 + jx, 48 + jy, 50 + jx, 62 + jy, 2, cCore);
    drawThickLine(50 + jx, 62 + jy, 22 + jx, 76 + jy, 2, cCore);
    drawThickLine(106 + jx, 48 + jy, 78 + jx, 62 + jy, 2, cCore);
    drawThickLine(78 + jx, 62 + jy, 106 + jx, 76 + jy, 2, cCore);
    drawAngryVein(64 + jx, 24 + jy, RED);
    // Subtle dizzy-meter bar at bottom while shaking
    if (shakeEnergy > 0.4f) {
      int barW = (int)((shakeEnergy / 3.20f) * 80.0f);
      if (barW > 80) barW = 80;
      frame->drawFastHLine(64 - (barW / 2), 121, barW, (shakeEnergy > 2.2f) ? RED : SUN_GOLD);
    }
    return;
  } else if (m == FAINTING) {
    // Big (r=21), matching, synchronized warm swirly eyes + 3 orbiting stars!
    int wobbleX = (int)(sinf(now / 80.0f) * 3.0f);
    int wobbleY = (int)(cosf(now / 110.0f) * 2.0f);
    float phase = now / 70.0f;

    // Both eyes use +phase so their spiral arms match and spin in unison!
    drawSpiralEye(35 + wobbleX, 64 + wobbleY, 21, phase, cCore, st.eyeRim);
    drawSpiralEye(93 + wobbleX, 64 + wobbleY, 21, phase, cCore, st.eyeRim);

    // 3 Orbiting golden dizzy stars above the eyes
    for (int i = 0; i < 3; i++) {
      float starAng = (now / 160.0f) + (i * 2.094f);
      int sx = 64 + (int)(cosf(starAng) * 28.0f);
      int sy = 20 + (int)(sinf(starAng) * 6.0f);
      frame->fillCircle(sx, sy, 2, SUN_GOLD);
      frame->drawPixel(sx, sy, WHITE);
    }
    // Wavy dizzy mouth at bottom
    drawThickLine(52 + wobbleX, 102, 64 + wobbleX, 98, 1, SAKURA_PINK);
    drawThickLine(64 + wobbleX, 98, 76 + wobbleX, 102, 1, SAKURA_PINK);
    return;
  } else if (m == TICKLISH) {
    int wigX = ((now / 35) % 5) - 2;
    int wigY = (int)(fabsf(sinf(now / 75.0f)) * -8.0f);
    drawThickLine(24 + wigX, 36 + wigY, 48 + wigX, 48 + wigY, 2, cCore);
    drawThickLine(48 + wigX, 48 + wigY, 24 + wigX, 60 + wigY, 2, cCore);
    drawThickLine(104 + wigX, 36 + wigY, 80 + wigX, 48 + wigY, 2, cCore);
    drawThickLine(80 + wigX, 48 + wigY, 104 + wigX, 60 + wigY, 2, cCore);
    frame->fillCircle(64 + wigX, 80 + wigY, 11, SAKURA_PINK);
    frame->fillRect(50 + wigX, 68 + wigY, 28, 12, BLACK);
    return;
  }

  unsigned long blinkPhase = now % 3600;
  bool isBlinking = (blinkPhase >= 3460) && (m == SIMPLE || m == HAPPY || m == SAD || m == FROWN);

  float breathY = sinf(now / 420.0f) * 2.2f;
  float baseH   = isHerFace ? 38.0f : 34.0f;
  float tLx = 18.0f, tLy = 44.0f + breathY, tLw = 36.0f, tLh = baseH;
  float tRx = 74.0f, tRy = 44.0f + breathY, tRw = 36.0f, tRh = baseH;
  float tCorner = isHerFace ? 14.0f : 7.0f;

  if (m == SIMPLE && activeQuirk == QUIRK_NONE) {
    if (sType == SEASON_MONSOON_RAIN) {
      tgtPupilX = sinf(now / 900.0f) * 4.0f;
      tgtPupilY = -5.0f;
    } else {
      WeatherParticle& p = particles[trackedParticleIdx % 3];
      tgtPupilX = (p.x - 64.0f) / 4.8f;
      tgtPupilY = (p.y - 58.0f) / 7.0f;
    }
  } else if (activeQuirk == QUIRK_LOOK_LEFT) {
    tgtPupilX = -7.0f; tgtPupilY = -2.0f;
  } else if (activeQuirk == QUIRK_LOOK_RIGHT) {
    tgtPupilX = 7.0f;  tgtPupilY = -2.0f;
  } else if (activeQuirk == QUIRK_CURIOUS_TILT) {
    tLx = 16.0f; tLy = 50.0f; tLw = 34.0f; tLh = 25.0f;
    tRx = 72.0f; tRy = 38.0f; tRw = 38.0f; tRh = 43.0f;
    tgtPupilX = 2.0f; tgtPupilY = -3.0f;
  } else if (activeQuirk == QUIRK_STARGAZE) {
    tgtPupilX = 0.0f; tgtPupilY = -9.0f;
    bool tw = ((now / 180) & 1);
    frame->fillCircle(28, 14, tw ? 2 : 1, SUN_GOLD);
    frame->fillCircle(64, 10, tw ? 1 : 2, ICE_CYAN);
    frame->fillCircle(100, 16, tw ? 2 : 1, SUN_GOLD);
  } else if (activeQuirk == QUIRK_BUTTERFLY) {
    unsigned long qElapsed = now - quirkStartedAt;
    int bx = 14 + ((qElapsed * 100) / 3600);
    int by = 30 + (int)(sinf(now / 90.0f) * 6.0f);
    tgtPupilX = (bx - 64) / 5.0f;
    tgtPupilY = (by - 56) / 6.0f;
    int wing = ((now / 60) & 1) ? 4 : 2;
    frame->fillTriangle(bx, by, bx - wing, by - wing, bx - wing, by + wing, SAKURA_PINK);
    frame->fillTriangle(bx, by, bx + wing, by - wing, bx + wing, by + wing, SUN_GOLD);
  } else if (activeQuirk == QUIRK_DETECTIVE_SCAN) {
    tgtPupilX = sinf(now / 220.0f) * 6.0f;
    tLh = 14.0f;
  } else if (activeQuirk == QUIRK_STRETCH) {
    unsigned long qElapsed = now - quirkStartedAt;
    tLh = (qElapsed < 1000) ? 14.0f : 44.0f;
    tRh = tLh;
    tLy = 64.0f - (tLh * 0.5f);
    tRy = tLy;
  } else if (activeQuirk == QUIRK_DANCE_GROOVE) {
    tgtPupilX = sinf(now / 110.0f) * 6.0f;
    tLy = 42.0f + fabsf(cosf(now / 110.0f)) * -6.0f;
    tRy = tLy;
  } else if (activeQuirk == QUIRK_NIGHT_YAWN) {
    tLh = 12.0f; tRh = 12.0f; tLy = 56.0f; tRy = 56.0f;
    int yawnR = 3 + (int)(fabsf(sinf(now / 320.0f)) * 5.0f);
    frame->drawCircle(64, 86, yawnR, SAKURA_PINK);
  } else {
    tgtPupilX = 0.0f; tgtPupilY = 0.0f;
  }

  if (m == HAPPY || m == PETTING || m == CRY || activeQuirk == QUIRK_DAYDREAM_LOVE ||
      activeQuirk == QUIRK_WHISTLE || activeQuirk == QUIRK_MORNING_CHAI || activeQuirk == QUIRK_LUNCH_SNACK) {
    int bounce = (m == PETTING) ? (int)(fabsf(sinf(now / 115.0f)) * -6.0f) : (int)(fabsf(sinf(now / 150.0f)) * -5.0f);
    tLy = 44.0f + bounce;
    tRy = 44.0f + bounce;
    tLh = 26.0f;
    tRh = 26.0f;
    tCorner = 12.0f;
  }

  if (now < snowSquintUntil) {
    tLh = 12.0f; tRh = 12.0f; tLy += 10.0f; tRy += 10.0f;
  }
  if (isBlinking) {
    tLh = 4.0f; tRh = 4.0f; tLy += 16.0f; tRy += 16.0f;
  }
  if (activeQuirk == QUIRK_WINK) {
    tRh = 5.0f; tRy += 14.0f;
  }

  curPupilX = lerpF(curPupilX, tgtPupilX, 0.22f);
  curPupilY = lerpF(curPupilY, tgtPupilY, 0.22f);
  curLx = lerpF(curLx, tLx + curPupilX, 0.26f);
  curLy = lerpF(curLy, tLy + curPupilY, 0.26f);
  curLw = lerpF(curLw, tLw, 0.26f);
  curLh = lerpF(curLh, tLh, 0.26f);
  curRx = lerpF(curRx, tRx + curPupilX, 0.26f);
  curRy = lerpF(curRy, tRy + curPupilY, 0.26f);
  curRw = lerpF(curRw, tRw, 0.26f);
  curRh = lerpF(curRh, tRh, 0.26f);
  curCorner = lerpF(curCorner, tCorner, 0.26f);

  int lx = (int)curLx, ly = (int)curLy, lw = (int)curLw, lh = (int)curLh;
  int rx = (int)curRx, ry = (int)curRy, rw = (int)curRw, rh = (int)curRh;
  int rad = (int)curCorner;

  drawSoftWarmEye(lx, ly, lw, lh, rad, true,  isHerFace, st);
  drawSoftWarmEye(rx, ry, rw, rh, rad, false, isHerFace, st);

  if (!isBlinking) {
    if (m == HAPPY || m == PETTING || m == CRY || activeQuirk == QUIRK_DAYDREAM_LOVE ||
        activeQuirk == QUIRK_WHISTLE || activeQuirk == QUIRK_MORNING_CHAI || activeQuirk == QUIRK_LUNCH_SNACK) {
      frame->fillCircle(lx + (lw / 2), ly + lh + 10, 22, BLACK);
      frame->fillCircle(rx + (rw / 2), ry + rh + 10, 22, BLACK);
    } else if (m == ANGRY || m == FROWN || m == TANTRUM_POUT) {
      frame->fillTriangle(lx - 2, ly - 2, lx + lw + 2, ly - 2, lx + lw + 2, ly + 12, BLACK);
      frame->fillTriangle(rx - 2, ry - 2, rx + rw + 2, ry - 2, rx - 2, ry + 12, BLACK);
    } else if (m == SAD) {
      frame->fillTriangle(lx - 2, ly - 2, lx + lw + 2, ly - 2, lx - 2, ly + 12, BLACK);
      frame->fillTriangle(rx - 2, ry - 2, rx + rw + 2, ry - 2, rx + rw + 2, ry + 12, BLACK);
    }
  }

  if (m == CRY) {
    int drop = (now / 28) % 26;
    drawTeardrop(lx + 14, ly + lh + drop, 3, ICE_CYAN);
    drawTeardrop(rx + 18, ry + rh + drop, 3, ICE_CYAN);
  } else if (activeQuirk == QUIRK_DAYDREAM_LOVE) {
    int hRise = ((now - quirkStartedAt) / 40) % 26;
    drawHeart(36, 32 - hRise, 4, SAKURA_PINK);
    drawHeart(92, 36 - ((hRise + 13) % 26), 5, SAKURA_LIGHT);
  } else if (activeQuirk == QUIRK_WHISTLE) {
    int noteRise = ((now - quirkStartedAt) / 35) % 26;
    frame->fillCircle(92, 40 - noteRise, 2, SUN_GOLD);
    frame->drawFastVLine(94, 32 - noteRise, 8, SUN_GOLD);
  } else if (activeQuirk == QUIRK_MORNING_CHAI) {
    frame->fillRoundRect(52, 76, 22, 18, 4, ORANGE);
    frame->drawRoundRect(72, 79, 6, 10, 2, ORANGE);
    int steamY = (now / 60) % 10;
    frame->drawPixel(58, 70 - steamY, st.eyeCore);
    frame->drawPixel(65, 72 - steamY, st.eyeRim);
  } else if (activeQuirk == QUIRK_LUNCH_SNACK) {
    int chew = ((now / 140) & 1) ? 2 : -2;
    frame->fillCircle(64, 82 + chew, 9, SUN_GOLD);
    frame->fillCircle(61, 80 + chew, 1, SLATE_DARK);
    frame->fillCircle(66, 84 + chew, 1, SLATE_DARK);
  }

  if ((isHerFace || m == PETTING || m == HAPPY) && !isBlinking) {
    frame->fillRoundRect(lx - 4, ly + lh + 5, 11, 3, 1, SAKURA_PINK);
    frame->fillRoundRect(rx + rw - 7, ry + rh + 5, 11, 3, 1, SAKURA_PINK);
  }

  updateAndDrawWeather(now, true);
  drawSubtleBottomOverlays(now, isHerFace, st);

  // Glitch-free ASCII top identity pill (No UTF-8 middle dot!)
  if (now - screenEnteredAt < 2200) {
    frame->fillRoundRect(14, 4, 100, 13, 6, SLATE_DARK);
    frame->fillCircle(22, 10, 2, isHerFace ? SAKURA_PINK : st.eyeRim);
    frame->setTextSize(1);
    frame->setTextColor(st.eyeCore);
    frame->setCursor(28, 7);
    if (isHerFace) {
      frame->printf("%s %dC %s", HER_CITY_TAG, herTempC, st.shortTag);
    } else {
      frame->printf("%s %dC %s", MY_CITY_TAG, myTempC, st.shortTag);
    }
  }
}

// =========================================================================
// MODERN VECTOR DUAL CLOCK (STYLISH BATTERY PILL + LIVE SPLIT WEATHER BG)
// =========================================================================
void renderClock(unsigned long now) {
  updateDualClockCache();
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  if (!clockTwinPodLayout) {
    // -------------------------------------------------------------------
    // STYLE A: SPLIT-HORIZON DUAL CHRONOGRAPH + STYLISH BATTERY + WEATHER BG
    // -------------------------------------------------------------------
    // Top Zone Background: Live Weather in Your City (Srinagar)
    drawLiveWeatherZoneBg(2, 2, 124, 48, myWeatherType, now);

    // Top-Left: Giant Vector Numerals for YOUR Time
    drawVectorDigit(6,  10, 18, 34, 4, liveMyHr / 10,  st.eyeCore);
    drawVectorDigit(27, 10, 18, 34, 4, liveMyHr % 10,  st.eyeCore);
    if ((now / 500) % 2 == 0) {
      frame->fillCircle(49, 21, 1, st.pCol1);
      frame->fillCircle(49, 32, 1, st.pCol1);
    }
    drawVectorDigit(53, 10, 18, 34, 4, liveMyMin / 10, st.eyeRim);
    drawVectorDigit(74, 10, 18, 34, 4, liveMyMin % 10, st.eyeRim);

    // Top-Right: Your Mini-Face + Live Temp Pill ("SXR 12C")
    frame->fillRoundRect(95, 6, 29, 40, 6, SLATE_DARK);
    drawMiniEyeGlyph(109, 18, myLocalMood, false, st);
    frame->setTextSize(1);
    frame->setTextColor(st.eyeRim);
    frame->setCursor(98, 33);
    frame->printf("%dC", myTempC);

    // Center Horizon Track with Sweeping Seconds + Stylish Battery Pill!
    frame->drawFastHLine(6, 54, 88, SLATE_MID);
    int secDotX = 6 + ((liveMySec * 87) / 59);
    frame->drawFastHLine(6, 54, secDotX - 6, st.eyeRim);
    frame->fillCircle(secDotX, 54, 2, st.pCol1);

    // Stylish Battery Capsule at Right of Horizon Bar (x=100, y=50)
    drawStylishBatteryPill(100, 50);

    // Bottom Floating Card: HER Time (Halle, Germany) + Her Live Weather BG!
    frame->fillRoundRect(4, 64, 120, 56, 9, SLATE_DARK);
    drawLiveWeatherZoneBg(6, 66, 116, 52, herWeatherType, now);
    frame->drawRoundRect(4, 64, 120, 56, 9, st.eyeHalo);

    drawMiniEyeGlyph(24, 80, remoteHerMood, true, st);
    frame->setTextColor(SAKURA_PINK);
    frame->setCursor(8, 96);
    frame->printf("%s %dC", HER_CITY_TAG, herTempC);

    drawVectorDigit(46, 76, 15, 28, 3, livePartnerHr / 10,  st.eyeCore);
    drawVectorDigit(64, 76, 15, 28, 3, livePartnerHr % 10,  st.eyeCore);
    frame->fillCircle(82, 85, 1, SAKURA_PINK);
    frame->fillCircle(82, 95, 1, SAKURA_PINK);
    drawVectorDigit(86, 76, 15, 28, 3, livePartnerMin / 10, 0xFACA);
    drawVectorDigit(104,76, 15, 28, 3, livePartnerMin % 10, 0xFACA);

  } else {
    // -------------------------------------------------------------------
    // STYLE B: TWIN-POD SIDE-BY-SIDE WATCHFACE + TOP BATTERY BAR
    // -------------------------------------------------------------------
    // Top Header: City Tags + Center Stylish Battery Capsule
    frame->setTextSize(1);
    frame->setTextColor(SAKURA_PINK);
    frame->setCursor(6, 3);
    frame->printf("%s %dC", HER_CITY_TAG, herTempC);

    drawStylishBatteryPill(52, 2);

    frame->setTextColor(st.eyeRim);
    frame->setCursor(84, 3);
    frame->printf("%s %dC", MY_CITY_TAG, myTempC);

    // Left Pod: HER (Halle, Germany) with Her Live Weather Background!
    frame->fillRoundRect(4, 15, 57, 105, 9, SLATE_DARK);
    drawLiveWeatherZoneBg(6, 17, 53, 101, herWeatherType, now);
    frame->drawRoundRect(4, 15, 57, 105, 9, 0x48E2);
    drawMiniEyeGlyph(32, 30, remoteHerMood, true, st);

    drawVectorDigit(15, 48, 14, 24, 3, livePartnerHr / 10,  st.eyeCore);
    drawVectorDigit(33, 48, 14, 24, 3, livePartnerHr % 10,  st.eyeCore);
    drawVectorDigit(15, 80, 14, 24, 3, livePartnerMin / 10, 0xFACA);
    drawVectorDigit(33, 80, 14, 24, 3, livePartnerMin % 10, 0xFACA);

    // Right Pod: YOU (Srinagar, India) with Your Live Weather Background!
    frame->fillRoundRect(67, 15, 57, 105, 9, SLATE_DARK);
    drawLiveWeatherZoneBg(69, 17, 53, 101, myWeatherType, now);
    frame->drawRoundRect(67, 15, 57, 105, 9, st.eyeHalo);
    drawMiniEyeGlyph(95, 30, myLocalMood, false, st);

    drawVectorDigit(78, 48, 14, 24, 3, liveMyHr / 10,  st.eyeCore);
    drawVectorDigit(96, 48, 14, 24, 3, liveMyHr % 10,  st.eyeCore);
    drawVectorDigit(78, 80, 14, 24, 3, liveMyMin / 10, st.eyeRim);
    drawVectorDigit(96, 80, 14, 24, 3, liveMyMin % 10, st.eyeRim);

    // Center Vertical Seconds Bar
    int sFill = (liveMySec * 96) / 59;
    frame->drawFastVLine(63, 18, 96, SLATE_MID);
    if (sFill > 0) frame->drawFastVLine(63, 114 - sFill, sFill, st.pCol1);
  }

  drawSubtleBottomOverlays(now, true, st);
}

// =========================================================================
// LIVE DUAL WEATHER STATION (GLITCH-FREE ASCII + REAL-TIME WEATHER BG)
// =========================================================================
void renderWeatherStationScreen(unsigned long now) {
  updateDualClockCache();
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];
  const char* condLabels[5] = { "SUNNY", "CLOUDY", "RAINY", "SNOWY", "WINDY" };

  if (!weatherSplitLayout) {
    // Top Status Pill + Stylish Battery Capsule
    frame->fillRoundRect(4, 2, 92, 12, 5, SLATE_DARK);
    uint16_t dotCol = liveWeatherSynced ? GREEN : ((WiFi.status() == WL_CONNECTED) ? SUN_GOLD : SAKURA_PINK);
    frame->fillCircle(11, 8, 2, dotCol);
    frame->setTextSize(1);
    frame->setTextColor(st.eyeCore);
    frame->setCursor(17, 4);
    frame->print(syncStatusText);

    drawStylishBatteryPill(100, 3);

    // Left Pod: HER (HALLE DE) + Live Halle Weather Background!
    frame->fillRoundRect(3, 17, 59, 108, 9, SLATE_DARK);
    drawLiveWeatherZoneBg(5, 19, 55, 104, herWeatherType, now);
    frame->drawRoundRect(3, 17, 59, 108, 9, 0x48E2);

    frame->setTextColor(SAKURA_PINK);
    frame->setCursor(8, 22);
    frame->print("HALLE");
    frame->fillCircle(41, 25, 1, SAKURA_LIGHT);
    frame->setCursor(45, 22);
    frame->print("DE");

    drawWeatherIcon(32, 46, herWeatherType, now);
    drawVectorTemp(31, 66, 12, 22, 3, herTempC, st.eyeCore, SAKURA_PINK);

    frame->setTextColor(0xFACA);
    int hLen = strlen(condLabels[herWeatherType % 5]) * 6;
    frame->setCursor(32 - (hLen / 2), 94);
    frame->print(condLabels[herWeatherType % 5]);

    frame->setTextColor(SLATE_LIGHT);
    frame->setCursor(17, 108);
    frame->print(partnerHmStr);

    // Right Pod: YOU (SXR IN) + Live Srinagar Weather Background!
    frame->fillRoundRect(66, 17, 59, 108, 9, SLATE_DARK);
    drawLiveWeatherZoneBg(68, 19, 55, 104, myWeatherType, now);
    frame->drawRoundRect(66, 17, 59, 108, 9, st.eyeHalo);

    frame->setTextColor(st.eyeRim);
    frame->setCursor(74, 22);
    frame->print("SXR");
    frame->fillCircle(96, 25, 1, SUN_GOLD);
    frame->setCursor(101, 22);
    frame->print("IN");

    drawWeatherIcon(95, 46, myWeatherType, now);
    drawVectorTemp(94, 66, 12, 22, 3, myTempC, st.eyeCore, st.pCol1);

    frame->setTextColor(st.eyeRim);
    int mLen = strlen(condLabels[myWeatherType % 5]) * 6;
    frame->setCursor(95 - (mLen / 2), 94);
    frame->print(condLabels[myWeatherType % 5]);

    frame->setTextColor(SLATE_LIGHT);
    frame->setCursor(80, 108);
    frame->print(mainHmStr);
  } else {
    // Horizontal Split Weather Deck (With Live Weather BG + Wind Speed)
    frame->fillRoundRect(4, 4, 120, 57, 9, SLATE_DARK);
    drawLiveWeatherZoneBg(6, 6, 116, 53, herWeatherType, now);
    frame->drawRoundRect(4, 4, 120, 57, 9, 0x48E2);
    drawWeatherIcon(24, 32, herWeatherType, now);

    frame->setTextSize(1);
    frame->setTextColor(SAKURA_PINK);
    frame->setCursor(44, 10);
    frame->printf("HALLE %s", partnerHmStr);

    drawVectorTemp(64, 23, 12, 20, 3, herTempC, st.eyeCore, SAKURA_PINK);

    frame->setTextColor(0xFACA);
    frame->setCursor(88, 25);
    frame->print(condLabels[herWeatherType % 5]);
    frame->setTextColor(SLATE_LIGHT);
    frame->setCursor(88, 36);
    frame->printf("%dkmh", herWindKmh);

    // Bottom Card: YOU (SXR IN)
    frame->fillRoundRect(4, 66, 120, 57, 9, SLATE_DARK);
    drawLiveWeatherZoneBg(6, 68, 116, 53, myWeatherType, now);
    frame->drawRoundRect(4, 66, 120, 57, 9, st.eyeHalo);
    drawWeatherIcon(24, 94, myWeatherType, now);

    frame->setTextColor(st.eyeRim);
    frame->setCursor(44, 72);
    frame->printf("SXR   %s", mainHmStr);

    drawVectorTemp(64, 85, 12, 20, 3, myTempC, st.eyeCore, st.pCol1);

    frame->setTextColor(st.eyeRim);
    frame->setCursor(88, 87);
    frame->print(condLabels[myWeatherType % 5]);
    frame->setTextColor(SLATE_LIGHT);
    frame->setCursor(88, 98);
    frame->printf("%dkmh", myWindKmh);
  }
}

// =========================================================================
// SLEEK HORIZONTAL SLIDING CARD DOCK MAIN MENU + SUB-MENUS
// =========================================================================
// =========================================================================
// SMARTPHONE 3-COLUMN APP ICON GRID MENU (NAME AT TOP & STATUS AT BOTTOM)
// =========================================================================
void drawPhoneAppIcon(int idx, int cx, int cy, unsigned long now, const SeasonTheme& st) {
  switch (idx) {
    case 0: // 1. My Robo Mood (Warm Mini Eyes)
      drawMiniEyeGlyph(cx, cy, myLocalMood, false, st);
      break;
    case 1: // 2. Send Action (Warm Heart)
      drawHeart(cx, cy + 1, 6, SAKURA_PINK);
      break;
    case 2: // 3. Games Arcade (Gamepad Icon)
      frame->fillRoundRect(cx - 11, cy - 6, 22, 13, 4, GREEN);
      frame->drawFastHLine(cx - 7, cy, 5, BLACK);
      frame->drawFastVLine(cx - 5, cy - 2, 5, BLACK);
      frame->fillCircle(cx + 4, cy - 1, 1, SUN_GOLD);
      frame->fillCircle(cx + 7, cy + 1, 1, RED);
      break;
    case 3: // 4. R2-D2 Voice Chat (Microphone + Soundwaves)
      frame->fillRoundRect(cx - 2, cy - 7, 5, 9, 2, ICE_CYAN);
      frame->drawFastHLine(cx - 4, cy + 3, 9, WHITE);
      frame->drawFastVLine(cx, cy + 3, 4, WHITE);
      frame->drawPixel(cx - 6, cy - 3, SUN_GOLD);
      frame->drawPixel(cx + 6, cy - 3, SUN_GOLD);
      break;
    case 4: // 5. Dual Clock (Watch Dial)
      frame->drawCircle(cx, cy, 8, st.eyeRim);
      frame->drawLine(cx, cy, cx, cy - 5, st.eyeCore);
      frame->drawLine(cx, cy, cx + 4, cy + 1, SAKURA_PINK);
      break;
    case 5: // 6. Live Weather (Sun + Cloud)
      frame->fillCircle(cx - 3, cy - 2, 4, SUN_GOLD);
      frame->fillRoundRect(cx - 7, cy, 15, 6, 3, WHITE);
      frame->fillCircle(cx + 1, cy - 1, 4, WHITE);
      break;
    case 6: // 7. Heartbeat Sync (Pulse Line + Heart)
      drawHeart(cx, cy - 2, 5, RED);
      frame->drawLine(cx - 9, cy + 5, cx - 3, cy + 5, SAKURA_LIGHT);
      frame->drawLine(cx - 3, cy + 5, cx, cy + 1, WHITE);
      frame->drawLine(cx, cy + 1, cx + 3, cy + 6, WHITE);
      frame->drawLine(cx + 3, cy + 6, cx + 9, cy + 5, SAKURA_LIGHT);
      break;
    case 7: // 8. Shared Focus Timer (Pomodoro Flame/Timer)
      frame->drawCircle(cx, cy + 1, 7, SUN_GOLD);
      frame->drawFastHLine(cx - 2, cy - 8, 5, GREEN);
      frame->drawLine(cx, cy + 1, cx + 3, cy - 3, st.eyeCore);
      break;
    case 8: // 9. DJ Sound Party (Equalizer Bars)
      for (int b = -2; b <= 2; b++) {
        int bh = 4 + ((abs(b) * 3 + (now / 90)) % 9);
        uint16_t bc = (b & 1) ? SAKURA_PINK : ICE_CYAN;
        frame->fillRect(cx + (b * 4) - 1, cy + 6 - bh, 3, bh, bc);
      }
      break;
    case 9: // 10. Settings (Gear Icon)
      frame->fillCircle(cx, cy, 6, st.eyeRim);
      frame->fillCircle(cx, cy, 3, SLATE_DARK);
      for (int a = 0; a < 4; a++) {
        frame->drawPixel(cx + ((a == 0) ? 7 : ((a == 1) ? -7 : 0)),
                         cy + ((a == 2) ? 7 : ((a == 3) ? -7 : 0)), st.eyeCore);
      }
      break;
  }
}

void renderMenuMain(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  const char* titles[TOTAL_ROOT_ITEMS] = {
    "MY ROBO MOOD",
    "SEND ACTION",
    "GAMES ARCADE",
    "VOICE CHAT",
    "DUAL CLOCK",
    "LIVE WEATHER",
    "HEARTBEAT",
    "FOCUS TIMER",
    "SOUND PARTY",
    "SETTINGS"
  };

  // 1. TOP HEADER BAR: Shows Highlighted App Name in Bold!
  frame->fillRoundRect(4, 2, 120, 17, 5, SLATE_DARK);
  frame->drawRoundRect(4, 2, 120, 17, 5, st.eyeHalo);
  drawCenteredText(titles[menuSelection], 7, SUN_GOLD, 1, true);

  // 2. CENTER SMARTPHONE 3-COLUMN APP ICON GRID (Smoothly scrolls rows)
  int selectedRow = menuSelection / 3;
  int targetTopRow = (selectedRow == 0) ? 0 : (selectedRow - 1);
  if (targetTopRow > 2) targetTopRow = 2;
  menuScrollPos = lerpF(menuScrollPos, (float)targetTopRow, 0.30f);

  for (int i = 0; i < TOTAL_ROOT_ITEMS; i++) {
    int col = i % 3;
    int row = i / 3;
    float rowOffset = (float)row - menuScrollPos;

    // Only draw rows currently visible in the center viewport
    if (rowOffset < -0.6f || rowOffset > 1.7f) continue;

    int iconW = 34;
    int iconH = 30;
    int iconX = 8 + (col * 39);
    int iconY = 25 + (int)(rowOffset * 38.0f);

    bool isSel = (i == menuSelection);
    if (isSel) {
      // Selected App Icon pops up with a bright double warm glow border!
      frame->fillRoundRect(iconX - 2, iconY - 2, iconW + 4, iconH + 4, 8, st.eyeHalo);
      frame->fillRoundRect(iconX - 1, iconY - 1, iconW + 2, iconH + 2, 7, SLATE_MID);
      frame->drawRoundRect(iconX - 1, iconY - 1, iconW + 2, iconH + 2, 7, st.eyeCore);
      frame->drawRoundRect(iconX - 2, iconY - 2, iconW + 4, iconH + 4, 8, st.eyeRim);
    } else {
      frame->fillRoundRect(iconX, iconY, iconW, iconH, 7, SLATE_DARK);
      frame->drawRoundRect(iconX, iconY, iconW, iconH, 7, SLATE_MID);
    }

    drawPhoneAppIcon(i, iconX + (iconW / 2), iconY + (iconH / 2), now, st);
  }

  // Right-side vertical scrollbar dots (4 rows)
  for (int r = 0; r < 4; r++) {
    int dy = 42 + (r * 12);
    if (r == selectedRow) {
      frame->fillRoundRect(124, dy - 2, 3, 6, 1, SUN_GOLD);
    } else {
      frame->drawPixel(125, dy, SLATE_LIGHT);
    }
  }

  // 3. BOTTOM PILL: Shows Live App Status / Subtitle!
  char sub[22] = "";
  if (menuSelection == 0)      snprintf(sub, sizeof(sub), "Set Your Expression");
  else if (menuSelection == 1) snprintf(sub, sizeof(sub), "Hug / Poke / Kiss");
  else if (menuSelection == 2) snprintf(sub, sizeof(sub), "4 Competitive Games");
  else if (menuSelection == 3) snprintf(sub, sizeof(sub), "R2-D2 Droid Talk");
  else if (menuSelection == 4) snprintf(sub, sizeof(sub), "%s | %s", partnerHmStr, mainHmStr);
  else if (menuSelection == 5) snprintf(sub, sizeof(sub), "HAL %dC | SXR %dC", herTempC, myTempC);
  else if (menuSelection == 6) snprintf(sub, sizeof(sub), "Beam Live Pulse");
  else if (menuSelection == 7) snprintf(sub, sizeof(sub), "25m Shared Study");
  else if (menuSelection == 8) snprintf(sub, sizeof(sub), "DJ Spectrum Visualizer");
  else                         snprintf(sub, sizeof(sub), "System & Toggles");

  frame->fillRoundRect(4, 106, 120, 18, 5, SLATE_DARK);
  drawCenteredText(sub, 111, st.eyeCore, 1, false);
}

void renderGamesMenu(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  frame->fillRoundRect(6, 5, 116, 18, 5, SLATE_DARK);
  drawCenteredText("GAMES ARCADE", 10, SUN_GOLD, 1, true);

  const char* gameItems[TOTAL_GAME_ITEMS] = {
    "1. Dino (Button)",
    "2. Dino (Voice!)",
    "3. Flappy Droid",
    "4. Astro Dodge",
    "<- Back to Dock"
  };

  for (int i = 0; i < TOTAL_GAME_ITEMS; i++) {
    int y = 30 + (i * 18);
    if (i == gameMenuSelection) {
      frame->fillRoundRect(7, y - 3, 114, 15, 5, SLATE_DARK);
      frame->drawRoundRect(7, y - 3, 114, 15, 5, st.eyeRim);
      frame->setTextColor(SUN_GOLD);
      frame->setCursor(11, y);
      frame->print(">");
    } else {
      frame->setTextColor(SLATE_LIGHT);
    }
    frame->setCursor(21, y);
    frame->print(gameItems[i]);
  }
}

void renderActionsMenu(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  frame->fillRoundRect(6, 5, 116, 18, 5, SLATE_DARK);
  drawCenteredText("SEND ACTION", 10, SAKURA_PINK, 1, true);

  const char* actions[TOTAL_ACTION_ITEMS] = {
    "1. Send Warm Hug",
    "2. Playful Poke",
    "3. Blow a Kiss",
    "<- Back to Dock"
  };

  for (int i = 0; i < TOTAL_ACTION_ITEMS; i++) {
    int y = 34 + (i * 20);
    if (i == actionSelection) {
      frame->fillRoundRect(8, y - 3, 112, 16, 5, SLATE_DARK);
      frame->drawRoundRect(8, y - 3, 112, 16, 5, SAKURA_PINK);
      frame->setTextColor(st.eyeCore);
      frame->setCursor(12, y);
      frame->print(">");
    } else {
      frame->setTextColor(SLATE_LIGHT);
    }
    frame->setCursor(24, y);
    frame->print(actions[i]);
  }
}

void renderSettingsMenu(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  frame->fillRoundRect(6, 5, 116, 18, 5, SLATE_DARK);
  drawCenteredText("SETTINGS", 10, st.eyeRim, 1, true);

  char idleLabel[18];
  if (idleSecSetting == 0) snprintf(idleLabel, sizeof(idleLabel), "Idle Clk: OFF");
  else snprintf(idleLabel, sizeof(idleLabel), "Idle Clk: %ds", idleSecSetting);

  char seasonLabel[18];
  if (!manualSeasonOverride) snprintf(seasonLabel, sizeof(seasonLabel), "Theme: AUTO LIVE");
  else snprintf(seasonLabel, sizeof(seasonLabel), "Theme: %s", SEASONS[currentSeason].shortTag);

  const char* items[TOTAL_SETTINGS_ITEMS] = {
    audioMuted     ? "Sound: MUTED"   : "Sound: ON",
    micEnabled     ? "Mic Sense: ON"  : "Mic Sense: OFF",
    ldrEnabled     ? "LDR Auto: ON"   : "LDR Auto: OFF",
    motionEnabled  ? "Motion: ON"     : "Motion: OFF",
    hydrateEnabled ? "Hydrate: ON"    : "Hydrate: OFF",
    battSaverMode  ? "Batt Saver: ON" : "Batt Saver:OFF",
    wifiEnabled    ? "WiFi: ON"       : "WiFi: OFF",
    seasonLabel,
    idleLabel,
    "System & Batt",
    "<- Back"
  };

  int startIdx = 0;
  if (settingsSelection >= 4) startIdx = settingsSelection - 4;
  if (startIdx > TOTAL_SETTINGS_ITEMS - 5) startIdx = TOTAL_SETTINGS_ITEMS - 5;

  for (int i = 0; i < 5; i++) {
    int idx = startIdx + i;
    int y = 30 + (i * 18);
    if (idx == settingsSelection) {
      frame->fillRoundRect(7, y - 3, 114, 15, 5, SLATE_DARK);
      frame->drawRoundRect(7, y - 3, 114, 15, 5, st.eyeRim);
      frame->setTextColor(st.eyeCore);
      frame->setCursor(11, y);
      frame->print(">");
    } else {
      frame->setTextColor(SLATE_LIGHT);
    }
    frame->setCursor(21, y);
    frame->print(items[idx]);
  }
}

void renderConnectionInfo(unsigned long now) {
  frame->fillScreen(BLACK);
  const SeasonTheme& st = SEASONS[getActiveSeasonForCurrentScreen()];

  frame->fillRoundRect(5, 5, 118, 18, 5, SLATE_DARK);
  drawCenteredText("SYSTEM & BATT", 10, GREEN, 1, true);

  frame->setTextSize(1);
  frame->setTextColor(SLATE_LIGHT);
  frame->setCursor(10, 26); frame->print("IP:");
  frame->setTextColor(SUN_GOLD);
  frame->setCursor(30, 26);
  if (!wifiEnabled) frame->print("WiFi OFF");
  else if (WiFi.status() == WL_CONNECTED) frame->print(WiFi.localIP().toString());
  else frame->print("192.168.4.1");

  frame->setTextColor(SLATE_LIGHT);
  frame->setCursor(10, 40); frame->print("Batt:");
  if (batterySensed) {
    uint16_t bCol = (batteryPct > 45) ? GREEN : ((batteryPct > 20) ? SUN_GOLD : RED);
    frame->setTextColor(bCol);
    frame->setCursor(42, 40);
    frame->printf("%.2fV (%d%%)", smoothedBatMv / 1000.0f, batteryPct);
  } else {
    frame->setTextColor(ICE_CYAN);
    frame->setCursor(42, 40);
    frame->print(battSaverMode ? "Eco Mode" : "Full Pwr");
  }

  frame->drawRoundRect(10, 51, 104, 8, 2, st.eyeCore);
  frame->fillRect(114, 53, 3, 4, st.eyeCore);
  int fillW = (batteryPct * 100) / 100;
  uint16_t fillCol = batterySensed ? ((batteryPct > 20) ? GREEN : RED) : ICE_CYAN;
  if (fillW > 0) frame->fillRect(12, 53, fillW, 4, fillCol);

  frame->setTextColor(SLATE_LIGHT);
  frame->setCursor(10, 65); frame->print("Wthr:");
  frame->setTextColor(liveWeatherSynced ? GREEN : SUN_GOLD);
  frame->setCursor(42, 65);
  frame->printf("H:%dC S:%dC", herTempC, myTempC);

  frame->setTextColor(SLATE_LIGHT);
  frame->setCursor(10, 79); frame->print("MQTT:");
  frame->setTextColor(mqtt.connected() ? GREEN : RED);
  frame->setCursor(44, 79); frame->print(mqtt.connected() ? "OK" : "WAIT");

  frame->setTextColor(SLATE_LIGHT);
  frame->setCursor(72, 79); frame->print("MPU:");
  frame->setTextColor((mpuAvailable && motionEnabled) ? GREEN : RED);
  frame->setCursor(98, 79);
  frame->print(!motionEnabled ? "OFF" : (mpuAvailable ? "OK" : "ERR"));

  frame->setTextColor(SLATE_LIGHT);
  frame->setCursor(10, 93); frame->print("LDR:");
  frame->setTextColor(ICE_CYAN);
  frame->setCursor(34, 93); frame->print(lastLdrValue);
  frame->setTextColor(SLATE_LIGHT);
  frame->setCursor(62, 93); frame->print("Mic:");
  frame->setTextColor((micEnabled && i2sMicReady) ? GREEN : RED);
  frame->setCursor(86, 93);
  if (!micEnabled) frame->print("OFF");
  else frame->printf("%d", micPeakHold);

  drawCenteredText("BACK: Return", 113, SLATE_LIGHT, 1, false);
}

// =========================================================================
// 4 COMPETITIVE GAMES (DINO BTN, DINO VOICE, FLAPPY DROID, ASTRO DODGE)
// =========================================================================
void spawnNextObstacle() {
  obsX = 128.0f + (esp_random() % 28);
  if (dinoScore < 4) obsType = esp_random() % 2;
  else obsType = esp_random() % 4;
}

void resetDinoGame() {
  dinoY = 0.0f;
  dinoVel = 0.0f;
  dinoScore = 0;
  obsSpeed = 3.6f;
  dinoGameOver = false;
  dinoNewRecord = false;
  spawnNextObstacle();
}

void updateAndRenderDino(unsigned long now) {
  frame->fillScreen(BLACK);
  frame->fillRoundRect(5, 5, 118, 22, 4, SLATE_DARK);
  frame->setTextSize(1);

  frame->setTextColor(myDinoHigh >= herDinoHigh ? SUN_GOLD : SLATE_LIGHT);
  frame->setCursor(8, 8);
  frame->printf("YOU:%d", myDinoHigh);

  frame->setTextColor(herDinoHigh >= myDinoHigh ? SAKURA_PINK : SLATE_LIGHT);
  frame->setCursor(66, 8);
  frame->printf("HER:%d", herDinoHigh);

  frame->setTextColor(WHITE);
  frame->setCursor(18, 17);
  frame->printf("%s NOW:%d", dinoVoiceMode ? "MIC" : "BTN", dinoScore);

  if (!dinoGameOver) {
    if (dinoVoiceMode && dinoY <= 0.1f &&
        (micInstantNorm >= MIC_GAME_JUMP_THRESH || micSmoothLevel >= MIC_GAME_JUMP_THRESH)) {
      dinoVel = 6.3f;
      playMoodSound(SND_DINO_JUMP);
    }

    dinoY += dinoVel;
    dinoVel -= 0.88f;
    if (dinoY <= 0.0f) { dinoY = 0.0f; dinoVel = 0.0f; }

    obsSpeed = 3.6f + (dinoScore * 0.14f);
    if (obsSpeed > 7.6f) obsSpeed = 7.6f;
    obsX -= obsSpeed;

    int obsW = 8, obsH = 14, obsTopY = 88;
    if (obsType == 0)      { obsW = 7;  obsH = 13; obsTopY = 89; }
    else if (obsType == 1) { obsW = 8;  obsH = 19; obsTopY = 83; }
    else if (obsType == 2) { obsW = 15; obsH = 14; obsTopY = 88; }
    else if (obsType == 3) { obsW = 12; obsH = 8;  obsTopY = 74; }

    if (obsX < -obsW) {
      dinoScore++;
      if (dinoScore > myDinoHigh) {
        myDinoHigh = dinoScore;
        dinoNewRecord = true;
        preferences.begin("robot", false);
        preferences.putInt("my_hi", myDinoHigh);
        preferences.end();
        publishDinoNeeded = true;
      }
      if (dinoScore % 5 == 0) playMoodSound(SND_DINO_SCORE);
      spawnNextObstacle();
    }

    int dinoTop = 86 - (int)dinoY;
    int dinoBot = dinoTop + 16;
    int ix = (int)obsX;
    if ((ix + obsW >= 20) && (ix <= 30) && (dinoBot > obsTopY) && (dinoTop < obsTopY + obsH)) {
      dinoGameOver = true;
      playMoodSound(dinoNewRecord ? SND_NEW_RECORD : SND_DINO_DIE);
    }
  }

  frame->drawFastHLine(8, 102, 112, SLATE_MID);
  if (dinoVoiceMode) {
    int vBar = (micSmoothLevel * 100) / 2500;
    if (vBar > 100) vBar = 100;
    frame->drawRect(14, 112, 100, 6, SLATE_DARK);
    if (vBar > 0) frame->fillRect(14, 113, vBar, 4, (micSmoothLevel >= MIC_GAME_JUMP_THRESH) ? GREEN : ICE_CYAN);
  }

  int dy = 86 - (int)dinoY;
  frame->fillRoundRect(18, dy, 13, 14, 3, GREEN);
  frame->fillRoundRect(23, dy - 4, 10, 8, 2, GREEN);
  frame->drawPixel(29, dy - 2, BLACK);
  int legStep = ((now / 70) & 1);
  frame->fillRect(20, dy + 14, 3, (dinoY > 1.0f) ? 2 : (legStep ? 3 : 1), GREEN);
  frame->fillRect(26, dy + 14, 3, (dinoY > 1.0f) ? 2 : (legStep ? 1 : 3), GREEN);

  int ix = (int)obsX;
  if (obsType == 0)      frame->fillRoundRect(ix, 89, 7, 13, 2, RED);
  else if (obsType == 1) frame->fillRoundRect(ix, 83, 8, 19, 2, RED);
  else if (obsType == 2) {
    frame->fillRoundRect(ix, 88, 6, 14, 2, RED);
    frame->fillRoundRect(ix + 8, 90, 7, 12, 2, ORANGE);
  } else {
    int wingY = ((now / 80) & 1) ? -2 : 2;
    frame->fillRoundRect(ix, 74, 12, 6, 2, ICE_CYAN);
    frame->drawLine(ix + 2, 74, ix + 8, 74 + wingY, WHITE);
  }

  if (dinoGameOver) {
    if (dinoNewRecord) drawCenteredText("NEW RECORD!", 42, GREEN, 1, true);
    else drawCenteredText("GAME OVER", 42, RED, 1, true);
    drawCenteredText("SEL: Play Again", 58, WHITE, 1, false);
  }
}

void resetFlappyGame() {
  flapY = 64.0f;
  flapVel = 0.0f;
  pipeX = 128.0f;
  pipeGapY = 48 + (esp_random() % 38);
  flapScore = 0;
  flapGameOver = false;
  flapNewRecord = false;
}

void updateAndRenderFlappy(unsigned long now) {
  frame->fillScreen(BLACK);
  frame->fillRoundRect(5, 5, 118, 20, 4, SLATE_DARK);
  frame->setTextSize(1);
  frame->setTextColor(myFlapHigh >= herFlapHigh ? SUN_GOLD : SLATE_LIGHT);
  frame->setCursor(8, 7);  frame->printf("YOU:%d", myFlapHigh);
  frame->setTextColor(herFlapHigh >= myFlapHigh ? SAKURA_PINK : SLATE_LIGHT);
  frame->setCursor(52, 7); frame->printf("HER:%d", herFlapHigh);
  frame->setTextColor(WHITE);
  frame->setCursor(96, 7); frame->printf("S:%d", flapScore);

  const int gapHalf = 19;

  if (!flapGameOver) {
    if ((micInstantNorm >= MIC_GAME_JUMP_THRESH) && (now - lastVoiceFlapMs > 180)) {
      lastVoiceFlapMs = now;
      flapVel = -3.4f;
      playMoodSound(SND_DINO_JUMP);
    }

    flapVel += 0.36f;
    if (flapVel > 4.2f) flapVel = 4.2f;
    flapY += flapVel;

    float spd = 2.4f + (flapScore * 0.10f);
    if (spd > 5.0f) spd = 5.0f;
    pipeX -= spd;

    if (pipeX < -14.0f) {
      pipeX = 128.0f;
      pipeGapY = 46 + (esp_random() % 44);
      flapScore++;
      if (flapScore > myFlapHigh) {
        myFlapHigh = flapScore;
        flapNewRecord = true;
        preferences.begin("robot", false);
        preferences.putInt("flap_hi", myFlapHigh);
        preferences.end();
        publishFlapNeeded = true;
      }
      if (flapScore % 3 == 0) playMoodSound(SND_DINO_SCORE);
    }

    int py = (int)flapY;
    int px = (int)pipeX;
    bool hitWall = (py < 28 || py > 118);
    bool inPipeX = (28 + 5 >= px) && (28 - 5 <= px + 12);
    bool hitPipe = inPipeX && (py - 5 < (pipeGapY - gapHalf) || py + 5 > (pipeGapY + gapHalf));

    if (hitWall || hitPipe) {
      flapGameOver = true;
      playMoodSound(flapNewRecord ? SND_NEW_RECORD : SND_DINO_DIE);
    }
  }

  int px = (int)pipeX;
  int topH = (pipeGapY - gapHalf) - 26;
  if (topH > 0) frame->fillRoundRect(px, 26, 12, topH, 2, SAKURA_PINK);
  int botY = pipeGapY + gapHalf;
  int botH = 122 - botY;
  if (botH > 0) frame->fillRoundRect(px, botY, 12, botH, 2, SAKURA_PINK);

  int dy = (int)flapY;
  frame->fillRoundRect(22, dy - 5, 12, 10, 3, 0xFFDF);
  frame->fillRect(25, dy - 3, 3, 3, SUN_GOLD);
  frame->fillRect(30, dy - 3, 3, 3, SUN_GOLD);

  if (flapGameOver) {
    if (flapNewRecord) drawCenteredText("NEW RECORD!", 52, GREEN, 1, true);
    else drawCenteredText("CRASHED!", 52, RED, 1, true);
    drawCenteredText("SEL: Retry", 68, WHITE, 1, false);
  }
}

void resetDodgeGame() {
  dodgeShipX = 64;
  dodgeScore = 0;
  meteorSpeed = 2.6f;
  dodgeGameOver = false;
  dodgeNewRecord = false;
  for (int i = 0; i < 3; i++) {
    meteorX[i] = 16.0f + (esp_random() % 96);
    meteorY[i] = 26.0f - (i * 36.0f);
  }
}

void updateAndRenderDodge(unsigned long now) {
  frame->fillScreen(BLACK);
  frame->fillRoundRect(5, 5, 118, 20, 4, SLATE_DARK);
  frame->setTextSize(1);
  frame->setTextColor(myDodgeHigh >= herDodgeHigh ? SUN_GOLD : SLATE_LIGHT);
  frame->setCursor(8, 7);  frame->printf("YOU:%d", myDodgeHigh);
  frame->setTextColor(herDodgeHigh >= myDodgeHigh ? SAKURA_PINK : SLATE_LIGHT);
  frame->setCursor(52, 7); frame->printf("HER:%d", herDodgeHigh);
  frame->setTextColor(WHITE);
  frame->setCursor(96, 7); frame->printf("S:%d", dodgeScore);

  if (!dodgeGameOver) {
    if (digitalRead(BTN_UP_PIN) == HIGH)  dodgeShipX -= 4;
    if (digitalRead(BTN_SEL_PIN) == HIGH || digitalRead(BOOT_BTN_PIN) == LOW) dodgeShipX += 4;
    if (dodgeShipX < 14)  dodgeShipX = 14;
    if (dodgeShipX > 114) dodgeShipX = 114;

    meteorSpeed = 2.6f + (dodgeScore * 0.08f);
    if (meteorSpeed > 6.2f) meteorSpeed = 6.2f;

    for (int i = 0; i < 3; i++) {
      meteorY[i] += meteorSpeed;
      if (meteorY[i] > 122.0f) {
        meteorY[i] = 24.0f;
        meteorX[i] = 14.0f + (esp_random() % 100);
        dodgeScore++;
        if (dodgeScore > myDodgeHigh) {
          myDodgeHigh = dodgeScore;
          dodgeNewRecord = true;
          preferences.begin("robot", false);
          preferences.putInt("dodge_hi", myDodgeHigh);
          preferences.end();
          publishDodgeNeeded = true;
        }
        if (dodgeScore % 5 == 0) playMoodSound(SND_DINO_SCORE);
      }

      int mx = (int)meteorX[i];
      int my = (int)meteorY[i];
      if (my >= 94 && my <= 114 && abs(mx - dodgeShipX) <= 10) {
        dodgeGameOver = true;
        playMoodSound(dodgeNewRecord ? SND_NEW_RECORD : SND_DINO_DIE);
      }
    }
  }

  for (int i = 0; i < 3; i++) {
    int mx = (int)meteorX[i];
    int my = (int)meteorY[i];
    if (my >= 26) {
      frame->fillCircle(mx, my, 5, RED);
      frame->fillCircle(mx - 1, my - 1, 2, SUN_GOLD);
      frame->drawLine(mx, my - 5, mx, my - 10, ORANGE);
    }
  }

  frame->fillTriangle(dodgeShipX, 96, dodgeShipX - 8, 112, dodgeShipX + 8, 112, ICE_CYAN);
  frame->fillCircle(dodgeShipX, 106, 2, WHITE);

  if (dodgeGameOver) {
    if (dodgeNewRecord) drawCenteredText("NEW RECORD!", 52, GREEN, 1, true);
    else drawCenteredText("METEOR HIT!", 52, RED, 1, true);
    drawCenteredText("SEL: Play Again", 68, WHITE, 1, false);
  }
}

void drawHoldProgressBar() {
  if (holdProgressBar <= 0.02f) return;
  int w = (int)(holdProgressBar * 100.0f);
  if (w > 100) w = 100;
  frame->fillRoundRect(64 - (w / 2), 123, w, 3, 1, SUN_GOLD);
}

// =========================================================================
// MASTER RENDERER & 180-DEGREE UPSIDE-DOWN CHUNKED 4MHz BLIT
// =========================================================================
void renderMaster(unsigned long now) {
  if (!frame || !frame->getBuffer()) return;

  if (lifeState == LIFE_SLEEPING) {
    renderSleepingFace(now);
    return;
  }
  if (lifeState == LIFE_WAKING_UP) {
    renderWakingUpFace(now - wakeUpStartedAt, now);
    return;
  }

  if (stickyNoteActive)  { renderStickyNoteScreen(now); return; }
  if (hbPlaybackActive)  { renderHeartbeatScreen(now, true); return; }

  if (currentUI == STATE_MENU_MAIN)       { renderMenuMain(now); drawHoldProgressBar(); return; }
  if (currentUI == STATE_GAMES_MENU)      { renderGamesMenu(now); return; }
  if (currentUI == STATE_SETTINGS)        { renderSettingsMenu(now); return; }
  if (currentUI == STATE_ACTIONS_MENU)    { renderActionsMenu(now); return; }
  if (currentUI == STATE_CONNECTION_INFO) { renderConnectionInfo(now); return; }
  if (currentUI == STATE_DINO_GAME)       { updateAndRenderDino(now); return; }
  if (currentUI == STATE_FLAPPY_GAME)     { updateAndRenderFlappy(now); return; }
  if (currentUI == STATE_DODGE_GAME)      { updateAndRenderDodge(now); return; }
  if (currentUI == STATE_HEARTBEAT)       { renderHeartbeatScreen(now, false); return; }
  if (currentUI == STATE_POMODORO)        { renderPomodoroScreen(now); return; }
  if (currentUI == STATE_VOICE_CHAT)      { renderVoiceChatScreen(now); return; }
  if (currentUI == STATE_SOUND_PARTY)     { renderSoundPartyScreen(now); return; }
  if (currentUI == STATE_WEATHER_STATION) { renderWeatherStationScreen(now); drawHoldProgressBar(); return; }
  if (currentUI == STATE_MY_ROBO)         { renderFace(myLocalMood, now, false); drawHoldProgressBar(); return; }

  if (hasLocalOverride) {
    if (now < overrideEndsAt) {
      renderFace(localOverride, now, true);
      drawHoldProgressBar();
      return;
    }
    hasLocalOverride = false;
  }

  if (manualClockActive || inIdleClock || currentUI == STATE_CLOCK) {
    renderClock(now);
    drawHoldProgressBar();
    return;
  }

  if (isPetting) {
    renderFace(isTicklish ? TICKLISH : PETTING, now, true);
    drawHoldProgressBar();
    return;
  }

  renderFace(remoteHerMood, now, true);
  drawHoldProgressBar();
}

void blit() {
  if (!frame) return;
  uint16_t* buf = frame->getBuffer();
  if (!buf) return;
  tft.setSPISpeed(SPI_HZ);
  tft.setRotation(TFT_ROTATION); // 2 = 180-deg upside-down orientation for enclosure!
  for (int y = 0; y < SH; y += 8) {
    tft.drawRGBBitmap(0, y, buf + (y * SW), SW, 8);
  }
}

void show(unsigned long now) {
  renderMaster(now);
  blit();
}
// =========================================================================
// FREE GITHUB CLOUD OTA UPDATER WITH LIVE TFT PROGRESS BAR
// =========================================================================
void drawOtaProgressScreen(const char* statusMsg, int pct, uint16_t barCol) {
  frame->fillScreen(BLACK);
  frame->fillRoundRect(6, 12, 116, 20, 5, SLATE_DARK);
  drawCenteredText("CLOUD OTA UPDATE", 18, SUN_GOLD, 1, true);

  drawCenteredText(statusMsg, 48, WHITE, 1, false);

  // Progress Bar Track
  frame->drawRoundRect(12, 68, 104, 14, 4, SLATE_LIGHT);
  if (pct > 0) {
    int fillW = (pct * 98) / 100;
    if (fillW > 98) fillW = 98;
    frame->fillRoundRect(15, 71, fillW, 8, 2, barCol);
  }

  char pctBuf[12];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
  drawCenteredText(pctBuf, 92, barCol, 1, true);
  drawCenteredText("Do not power off", 112, SLATE_LIGHT, 1, false);
  blit();
}

void runCloudOtaUpdate() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) {
    drawOtaProgressScreen("WIFI NOT READY!", 0, RED);
    delay(2500);
    return;
  }

  playMoodSound(SND_POMO_START);
  drawOtaProgressScreen("CONNECTING...", 5, ICE_CYAN);

  // Disconnect MQTT first so Core 1 has 100% free heap RAM for TLS download
  if (mqtt.connected()) {
    mqtt.disconnect();
  }
  delay(200);

  WiFiClientSecure otaClient;
  otaClient.setInsecure();
  otaClient.setTimeout(15);

  httpUpdate.rebootOnUpdate(false);
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  // Update the TFT progress bar live as chunks arrive from GitHub
  Update.onProgress([](size_t written, size_t total) {
    static int lastDrawnPct = -1;
    if (total > 0) {
      int pct = (written * 100) / total;
      if (pct != lastDrawnPct && (pct % 4 == 0 || pct == 100)) {
        lastDrawnPct = pct;
        drawOtaProgressScreen("DOWNLOADING...", pct, GREEN);
      }
    }
  });

  t_httpUpdate_return ret = httpUpdate.update(otaClient, GITHUB_OTA_URL);

  if (ret == HTTP_UPDATE_OK) {
    playMoodSound(SND_NEW_RECORD);
    drawOtaProgressScreen("SUCCESS! REBOOT", 100, GREEN);
    delay(1500);
    ESP.restart();
  } else {
    playMoodSound(SND_DINO_DIE);
    char errBuf[22];
    snprintf(errBuf, sizeof(errBuf), "ERR: %d", httpUpdate.getLastError());
    drawOtaProgressScreen(errBuf, 0, RED);
    delay(3500);
  }
}
// ==================== END OF PART 2 (PASTE PART 3 DIRECTLY BELOW) ====================
// ==================== START OF PART 3 (PASTE BELOW PART 2) ====================

// =========================================================================
// EXPANDED PERSONALITY ENGINE (15 AUTONOMOUS IDLE QUIRKS)
// =========================================================================
void updatePersonalityEngine(unsigned long now) {
  if (lifeState != LIFE_AWAKE || currentUI != STATE_NORMAL_HER_ROBO ||
      manualClockActive || inIdleClock || hasLocalOverride || isPetting ||
      hbPlaybackActive || stickyNoteActive || isBlowingMic || validClapCount > 0) {
    activeQuirk = QUIRK_NONE;
    return;
  }

  if (hydrateEnabled && (now - lastHydrateAt >= HYDRATE_INTERVAL_MS)) {
    lastHydrateAt = now;
    triggerOverride(HYDRATE_REMIND, 4500, now);
    return;
  }

  if ((now - lastActivityAt >= TANTRUM_IDLE_MS) && (now - lastTantrumCheckAt >= 600000UL)) {
    lastTantrumCheckAt = now;
    triggerOverride(TANTRUM_POUT, 4000, now);
    return;
  }

  if (activeQuirk != QUIRK_NONE) {
    if (now >= quirkEndsAt) {
      activeQuirk = QUIRK_NONE;
      nextQuirkAt = now + 4800 + (esp_random() % 6200);
    }
    return;
  }

  if (now >= nextQuirkAt && (remoteHerMood == SIMPLE || remoteHerMood == HAPPY)) {
    if ((isMyBdayToday || isPartnerBdayToday) && (esp_random() % 3 == 0)) {
      nextQuirkAt = now + 20000;
      triggerOverride(ACTION_BDAY, 6000, now);
      return;
    }

    int pick = esp_random() % 13;
    quirkStartedAt = now;
    switch (pick) {
      case 0:  activeQuirk = QUIRK_LOOK_LEFT;      quirkEndsAt = now + 1800; break;
      case 1:  activeQuirk = QUIRK_LOOK_RIGHT;     quirkEndsAt = now + 1800; break;
      case 2:  activeQuirk = QUIRK_CURIOUS_TILT;   quirkEndsAt = now + 2200; break;
      case 3:  activeQuirk = QUIRK_WINK;           quirkEndsAt = now + 1200; break;
      case 4:
        activeQuirk = QUIRK_WHISTLE;
        quirkEndsAt = now + 2500;
        if ((esp_random() % 2) == 0) playMoodSound(SND_QUIRK_CHIRP);
        break;
      case 5:  activeQuirk = QUIRK_STARGAZE;       quirkEndsAt = now + 3200; break;
      case 6:  activeQuirk = QUIRK_BUTTERFLY;      quirkEndsAt = now + 3600; break;
      case 7:
        activeQuirk = QUIRK_SNEEZE;
        quirkEndsAt = now + 2400;
        playMoodSound(SND_QUIRK_SNEEZE);
        break;
      case 8:
        activeQuirk = QUIRK_DANCE_GROOVE;
        quirkEndsAt = now + 2800;
        if ((esp_random() % 2) == 0) playMoodSound(SND_QUIRK_CHIRP);
        break;
      case 9:  activeQuirk = QUIRK_DETECTIVE_SCAN; quirkEndsAt = now + 2600; break;
      case 10: activeQuirk = QUIRK_DAYDREAM_LOVE;  quirkEndsAt = now + 3000; break;
      case 11: activeQuirk = QUIRK_STRETCH;        quirkEndsAt = now + 2400; break;
      case 12:
        if (currentLocalHour >= 6 && currentLocalHour <= 10) {
          activeQuirk = QUIRK_MORNING_CHAI; quirkEndsAt = now + 3200;
        } else if ((currentLocalHour >= 12 && currentLocalHour <= 14) ||
                   (currentLocalHour >= 17 && currentLocalHour <= 19)) {
          activeQuirk = QUIRK_LUNCH_SNACK;  quirkEndsAt = now + 3200;
        } else if (currentLocalHour >= 22 || currentLocalHour <= 5) {
          activeQuirk = QUIRK_NIGHT_YAWN;   quirkEndsAt = now + 3000;
        } else {
          activeQuirk = ((esp_random() & 1) == 0) ? QUIRK_MORNING_CHAI : QUIRK_LUNCH_SNACK;
          quirkEndsAt = now + 3000;
        }
        break;
    }
  }
}

void updateLifeAndLdr(unsigned long now) {
  if (lifeState == LIFE_WAKING_UP && (now - wakeUpStartedAt >= WAKE_UP_DURATION_MS)) {
    lifeState = LIFE_AWAKE;
    lastActivityAt = now;
    nextQuirkAt = now + 5000;
    if (isMyBdayToday || isPartnerBdayToday) {
      triggerOverride(ACTION_BDAY, 6000, now);
    } else {
      playMoodSound(HAPPY);
    }
  }

  static unsigned long lastLdrSample = 0;
  if (now - lastLdrSample < 200) return;
  lastLdrSample = now;

  lastLdrValue = analogRead(LDR_PIN);
  if (!ldrEnabled || pomoPhase != POMO_IDLE || stickyNoteActive ||
      currentUI == STATE_SOUND_PARTY || currentUI == STATE_VOICE_CHAT ||
      currentUI == STATE_DINO_GAME   || currentUI == STATE_FLAPPY_GAME ||
      currentUI == STATE_DODGE_GAME) {
    darkStartedAt = 0;
    lightStartedAt = 0;
    return;
  }

  if (lifeState == LIFE_AWAKE) {
    if (lastLdrValue < LDR_DARK_THRESHOLD) {
      if (darkStartedAt == 0) darkStartedAt = now;
      else if (now - darkStartedAt >= 5000UL) {
        darkStartedAt = 0;
        goToSleep(now);
      }
    } else {
      darkStartedAt = 0;
    }
  } else if (lifeState == LIFE_SLEEPING) {
    if (lastLdrValue > LDR_LIGHT_THRESHOLD) {
      if (lightStartedAt == 0) lightStartedAt = now;
      else if (now - lightStartedAt >= 4000UL) {
        lightStartedAt = 0;
        startWakeUp(now);
      }
    } else {
      lightStartedAt = 0;
    }
  }
}

// =========================================================================
// HEARTBEAT TAP RECORDER & PLAYBACK ENGINE
// =========================================================================
void recordHeartbeatTap(unsigned long now) {
  if (hbTapCount == 0) {
    hbTapIntervals[0] = 0;
    hbTapCount = 1;
  } else if (hbTapCount < MAX_HB_TAPS) {
    unsigned long delta = now - lastHbTapMs;
    if (delta < 90) return;
    if (delta > 1600) delta = 1600;
    hbTapIntervals[hbTapCount] = (uint16_t)delta;
    hbTapCount++;
  }
  lastHbTapMs = now;
  hbPulseUntilMs = now + 220;
  hbSentBanner = false;
  lastActivityAt = now;
  playMoodSound(SND_HEARTBEAT);
}

void updateHeartbeatEngine(unsigned long now) {
  if (currentUI == STATE_HEARTBEAT && hbTapCount > 0 && (now - lastHbTapMs >= 1800)) {
    hbOutgoingPayload[0] = '\0';
    char tmp[10];
    snprintf(tmp, sizeof(tmp), "%d", hbTapCount);
    strcat(hbOutgoingPayload, tmp);
    for (int i = 1; i < hbTapCount; i++) {
      snprintf(tmp, sizeof(tmp), ",%u", hbTapIntervals[i]);
      strcat(hbOutgoingPayload, tmp);
    }
    publishHbNeeded = true;
    hbTapCount = 0;
    hbSentBanner = true;
    hbSentBannerUntil = now + 2200;
    incrementLoveCounter();
    playMoodSound(LOVE);
  }

  if (hbPlaybackActive) {
    if (hbPlayIndex < hbPlayTotal && now >= hbNextBeatMs) {
      hbPulseUntilMs = now + 240;
      playMoodSound(SND_HEARTBEAT);
      hbPlayIndex++;
      if (hbPlayIndex < hbPlayTotal) {
        hbNextBeatMs = now + hbPlayIntervals[hbPlayIndex];
      } else {
        hbPlayEndMs = now + 1400;
      }
    } else if (hbPlayIndex >= hbPlayTotal && now >= hbPlayEndMs) {
      hbPlaybackActive = false;
    }
  }
}

// =========================================================================
// SHARED POMODORO FOCUS / STUDY TIMER ENGINE
// =========================================================================
void startPomodoroPhase(PomoPhase phase, unsigned long now, bool broadcastToPartner) {
  pomoPhase = phase;
  if (phase == POMO_FOCUS) {
    pomoTotalDurationMs = 25UL * 60UL * 1000UL;
    pomoPhaseEndsAt = now + pomoTotalDurationMs;
    playMoodSound(SND_POMO_START);
    if (broadcastToPartner) {
      strcpy(pomoOutgoingCmd, "F25");
      publishPomoNeeded = true;
    }
  } else if (phase == POMO_BREAK) {
    pomoTotalDurationMs = 5UL * 60UL * 1000UL;
    pomoPhaseEndsAt = now + pomoTotalDurationMs;
    playMoodSound(SND_POMO_DONE);
    if (broadcastToPartner) {
      strcpy(pomoOutgoingCmd, "B05");
      publishPomoNeeded = true;
    }
  } else {
    pomoPhase = POMO_IDLE;
    playMoodSound(SIMPLE);
    if (broadcastToPartner) {
      strcpy(pomoOutgoingCmd, "STOP");
      publishPomoNeeded = true;
    }
  }
}

void updatePomodoroTimer(unsigned long now) {
  if (pomoPhase == POMO_FOCUS && now >= pomoPhaseEndsAt) {
    startPomodoroPhase(POMO_BREAK, now, false);
  } else if (pomoPhase == POMO_BREAK && now >= pomoPhaseEndsAt) {
    pomoPhase = POMO_IDLE;
    playMoodSound(HAPPY);
  }
}

// =========================================================================
// BALANCED MPU6050 ENGINE (TAPS = IRRITATED | SUSTAINED SHAKE = FAINT)
// =========================================================================
void wakeAndConfigMpu(uint8_t addr) {
  Wire.beginTransmission(addr); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission();
  Wire.beginTransmission(addr); Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission();
  Wire.beginTransmission(addr); Wire.write(0x1B); Wire.write(0x00); Wire.endTransmission();
  Wire.beginTransmission(addr); Wire.write(0x1A); Wire.write(0x02); Wire.endTransmission();
}

void initMotionSensor() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  mpuAvailable = false;
  for (uint8_t addr = 0x68; addr <= 0x69; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      mpuAddr = addr;
      mpuAvailable = true;
      wakeAndConfigMpu(mpuAddr);
      mpuBaselineReady = false;
      shakeEnergy = 0.0f;
      return;
    }
  }
}

void readMotionSensor(unsigned long now) {
  if (!motionEnabled) return;

  if (!mpuAvailable) {
    static unsigned long lastRetry = 0;
    if (now - lastRetry > 2000) {
      lastRetry = now;
      initMotionSensor();
    }
    return;
  }

  if (now - lastMpuPoll < 32) return;
  lastMpuPoll = now;

  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) {
    mpuAvailable = false;
    return;
  }
  if (Wire.requestFrom(mpuAddr, (uint8_t)14) != 14) {
    mpuAvailable = false;
    return;
  }

  uint8_t raw[14];
  for (int i = 0; i < 14; i++) raw[i] = Wire.read();

  int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
  int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
  int16_t az = (int16_t)((raw[4] << 8) | raw[5]);
  int16_t gx = (int16_t)((raw[8] << 8) | raw[9]);
  int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
  int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

  if (ax == 0 && ay == 0 && az == 0) {
    wakeAndConfigMpu(mpuAddr);
    mpuBaselineReady = false;
    return;
  }

  float fAx = ax / 16384.0f;
  float fAy = ay / 16384.0f;
  float fAz = az / 16384.0f;
  float fGx = gx / 131.0f;
  float fGy = gy / 131.0f;
  float fGz = gz / 131.0f;

  if (!mpuBaselineReady) {
    baseAx = fAx; baseAy = fAy; baseAz = fAz;
    baseGx = fGx; baseGy = fGy; baseGz = fGz;
    mpuBaselineReady = true;
    return;
  }

  float accelDiff = fabsf(fAx - baseAx) + fabsf(fAy - baseAy) + fabsf(fAz - baseAz);
  float gyroDiff  = fabsf(fGx - baseGx) + fabsf(fGy - baseGy) + fabsf(fGz - baseGz);

  baseAx = baseAx * 0.93f + fAx * 0.07f;
  baseAy = baseAy * 0.93f + fAy * 0.07f;
  baseAz = baseAz * 0.93f + fAz * 0.07f;
  baseGx = baseGx * 0.94f + fGx * 0.06f;
  baseGy = baseGy * 0.94f + fGy * 0.06f;
  baseGz = baseGz * 0.94f + fGz * 0.06f;

  if (lifeState == LIFE_SLEEPING && (accelDiff > 0.45f || gyroDiff > 110.0f)) {
    startWakeUp(now);
    return;
  }

  bool canReact = (lifeState == LIFE_AWAKE) &&
                  (currentUI == STATE_NORMAL_HER_ROBO || currentUI == STATE_CLOCK ||
                   currentUI == STATE_WEATHER_STATION) &&
                  !hbPlaybackActive && !isPetting && !stickyNoteActive &&
                  (!hasLocalOverride || localOverride == IRRITATED) &&
                  (now - lastButtonPressAt >= 550);

  if (!canReact) {
    shakeEnergy = 0.0f;
    return;
  }

  if (accelDiff > MPU_TAP_ACCEL_G || gyroDiff > MPU_TAP_GYRO_DPS) {
    float step = (accelDiff * 0.55f) + (gyroDiff / 320.0f);
    if (step > MPU_MAX_STEP_ENERGY) step = MPU_MAX_STEP_ENERGY;
    shakeEnergy += step;
  } else {
    shakeEnergy *= 0.80f;
  }

  if (shakeEnergy >= MPU_FAINT_ENERGY) {
    shakeEnergy = 0.0f;
    lastMotionTrigAt = now;
    triggerOverride(FAINTING, 3800, now);
    return;
  }

  if ((accelDiff > MPU_TAP_ACCEL_G || gyroDiff > MPU_TAP_GYRO_DPS) &&
      !hasLocalOverride && (now - lastMotionTrigAt > 1400)) {
    lastMotionTrigAt = now;
    triggerOverride(IRRITATED, 2200, now);
  }
}

// =========================================================================
// THROUGH-PLASTIC FOIL CAPACITIVE TOUCH HANDLER (REPLACE IN PART 3)
// =========================================================================
void initTouchSensors() {
  delay(80); // Let foil pad charge stabilize at boot
  long sumPet = 0;
  int validSamples = 0;
  for (int i = 0; i < 16; i++) {
    int r = touchRead(PET_TOUCH_PIN);
    if (r > 0) {
      sumPet += r;
      validSamples++;
    }
    delay(5);
  }
  petBaseline = (validSamples > 0) ? (sumPet / validSamples) : 40;
  petIsDigital = false; // Force ESP32 capacitive mode for aluminum foil pad
  Serial.printf("[TOUCH INIT] Foil Baseline on GPIO 32 = %d\n", petBaseline);
}

void readTouchSensors(unsigned long now) {
  static unsigned long lastSample = 0;
  if (now - lastSample < 30) return;
  lastSample = now;

  // Average 2 quick readings to filter out Wi-Fi noise
  int raw = (touchRead(PET_TOUCH_PIN) + touchRead(PET_TOUCH_PIN)) / 2;

  // Through-plastic threshold: triggers on a 10% drop OR a 4-unit drop from baseline
  int triggerThreshold = (petBaseline * 90) / 100;
  if (triggerThreshold >= petBaseline - 3) {
    triggerThreshold = petBaseline - 4;
  }

  bool petTouched = (raw > 2 && raw <= triggerThreshold);

  // Slowly adapt baseline if room humidity/temperature shifts while untouched
  static unsigned long lastBaselineAdapt = 0;
  if (!petTouched && raw > triggerThreshold && (now - lastBaselineAdapt > 2000)) {
    lastBaselineAdapt = now;
    if (raw > petBaseline) petBaseline++;
    else if (raw < petBaseline && (petBaseline - raw <= 2)) petBaseline--;
  }

  // Print live readings every 400ms so you can see your finger through the plastic
  static unsigned long lastTouchDebug = 0;
  if (now - lastTouchDebug > 400) {
    lastTouchDebug = now;
    Serial.printf("Touch Raw:%3d | Base:%3d | Trig<=%3d | Hit:%d\n",
                  raw, petBaseline, triggerThreshold, petTouched ? 1 : 0);
  }

  if (currentUI == STATE_HEARTBEAT && lifeState == LIFE_AWAKE && !stickyNoteActive) {
    if (petTouched && !prevPetTouched) {
      recordHeartbeatTap(now);
    }
    prevPetTouched = petTouched;
    return;
  }
  prevPetTouched = petTouched;

  if (lifeState == LIFE_AWAKE && !stickyNoteActive &&
      currentUI != STATE_NORMAL_HER_ROBO && currentUI != STATE_MY_ROBO) {
    clearTouchState();
    return;
  }

  if (petTouched) { if (petFilterCount < 4) petFilterCount++; }
  else            { if (petFilterCount > 0) petFilterCount--; }

  if (petFilterCount >= 2) {
    if (lifeState == LIFE_SLEEPING) {
      clearTouchState();
      startWakeUp(now);
      return;
    }
    if (lifeState == LIFE_WAKING_UP) return;
    if (stickyNoteActive) {
      stickyNoteActive = false;
      clearTouchState();
      playMoodSound(SIMPLE);
      return;
    }

    if (!isPetting) {
      isPetting = true;
      isTicklish = false;
      petContinuousStart = now;
    } else if (!isTicklish && (now - petContinuousStart >= TICKLE_HOLD_MS)) {
      isTicklish = true;
    }

    if (hasLocalOverride && localOverride == TANTRUM_POUT) {
      hasLocalOverride = false;
    }

    lastPetTouchAt = now;
    lastActivityAt = now;
    inIdleClock = false;
    activeQuirk = QUIRK_NONE;
    if (now - lastPetBeepAt > 1200) {
      playMoodSound(isTicklish ? TICKLISH : PETTING);
      lastPetBeepAt = now;
    }
  } else if (isPetting && (now - lastPetTouchAt > PET_HOLD_MS)) {
    clearTouchState();
    lastActivityAt = now;
  }
}

// =========================================================================
// MQTT & WIFI CONNECTION MANAGER
// =========================================================================
void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  if (len == 0) return;

  if (strcmp(topic, REMOTE_DINO_TOPIC) == 0 ||
      strcmp(topic, REMOTE_FLAP_TOPIC) == 0 ||
      strcmp(topic, REMOTE_DODGE_TOPIC) == 0) {
    char numBuf[12];
    unsigned int copyLen = (len < 11) ? len : 11;
    memcpy(numBuf, payload, copyLen);
    numBuf[copyLen] = '\0';
    int score = atoi(numBuf);
    if (score >= 0) {
      if (strcmp(topic, REMOTE_DINO_TOPIC) == 0)       incomingRemoteDinoHigh  = score;
      else if (strcmp(topic, REMOTE_FLAP_TOPIC) == 0)  incomingRemoteFlapHigh  = score;
      else if (strcmp(topic, REMOTE_DODGE_TOPIC) == 0) incomingRemoteDodgeHigh = score;
    }
    return;
  }

  if (strcmp(topic, RECV_HB_TOPIC) == 0) {
    unsigned int copyLen = (len < sizeof(hbIncomingBuf) - 1) ? len : (sizeof(hbIncomingBuf) - 1);
    memcpy(hbIncomingBuf, payload, copyLen);
    hbIncomingBuf[copyLen] = '\0';
    hasIncomingHb = true;
    return;
  }

  if (strcmp(topic, RECV_NOTE_TOPIC) == 0 || strcmp(topic, SHARED_NOTE_TOPIC) == 0) {
    unsigned int copyLen = (len < sizeof(incomingNoteBuf) - 1) ? len : (sizeof(incomingNoteBuf) - 1);
    memcpy(incomingNoteBuf, payload, copyLen);
    incomingNoteBuf[copyLen] = '\0';
    hasIncomingNote = true;
    return;
  }

  if (strcmp(topic, RECV_POMO_TOPIC) == 0) {
    if (len >= 3 && strncmp((const char*)payload, "F25", 3) == 0) incomingPomoCmd = 1;
    else if (len >= 3 && strncmp((const char*)payload, "B05", 3) == 0) incomingPomoCmd = 2;
    else if (len >= 4 && strncmp((const char*)payload, "STOP", 4) == 0) incomingPomoCmd = 3;
    return;
  }

  int val = payload[0] - '0';
  if (strcmp(topic, REMOTE_MOOD_TOPIC) == 0) {
    if (val >= 0 && val < SYNC_MOOD_COUNT) {
      incomingMood = val;
    }
  } else if (strcmp(topic, RECV_ACT_TOPIC) == 0) {
    if (val >= (int)ACTION_HUG && val <= (int)ACTION_BDAY) {
      incomingAction = val;
    }
  }
}

void parseAndStartHeartbeatPlayback(unsigned long now) {
  char* tok = strtok(hbIncomingBuf, ",");
  if (!tok) return;
  int count = atoi(tok);
  if (count <= 0) return;
  if (count > MAX_HB_TAPS) count = MAX_HB_TAPS;

  hbPlayIntervals[0] = 0;
  int idx = 1;
  while (idx < count && (tok = strtok(NULL, ",")) != NULL) {
    int iv = atoi(tok);
    if (iv < 80) iv = 80;
    if (iv > 2000) iv = 2000;
    hbPlayIntervals[idx++] = (uint16_t)iv;
  }
  hbPlayTotal = idx;
  hbPlayIndex = 0;
  hbPlaybackActive = true;
  hbNextBeatMs = now + 100;
  lastActivityAt = now;
  inIdleClock = false;
  if (lifeState != LIFE_AWAKE) {
    lifeState = LIFE_AWAKE;
  }
}

void applyIncoming(unsigned long now) {
  if (incomingRemoteDinoHigh >= 0) {
    int newHerHi = incomingRemoteDinoHigh;
    incomingRemoteDinoHigh = -1;
    if (newHerHi > herDinoHigh) {
      herDinoHigh = newHerHi;
      preferences.begin("robot", false);
      preferences.putInt("her_hi", herDinoHigh);
      preferences.end();
    }
  }
  if (incomingRemoteFlapHigh >= 0) {
    int newHerFlap = incomingRemoteFlapHigh;
    incomingRemoteFlapHigh = -1;
    if (newHerFlap > herFlapHigh) {
      herFlapHigh = newHerFlap;
      preferences.begin("robot", false);
      preferences.putInt("her_flap", herFlapHigh);
      preferences.end();
    }
  }
  if (incomingRemoteDodgeHigh >= 0) {
    int newHerDodge = incomingRemoteDodgeHigh;
    incomingRemoteDodgeHigh = -1;
    if (newHerDodge > herDodgeHigh) {
      herDodgeHigh = newHerDodge;
      preferences.begin("robot", false);
      preferences.putInt("her_dodge", herDodgeHigh);
      preferences.end();
    }
  }
  if (hasIncomingNote) {
    hasIncomingNote = false;
    // Secret Cloud OTA Triggers via Sticky Note!
    if (strcasecmp(incomingNoteBuf, "#update") == 0 ||
        (IS_MY_BOARD && strcasecmp(incomingNoteBuf, "#updateboy") == 0) ||
        (!IS_MY_BOARD && strcasecmp(incomingNoteBuf, "#updateher") == 0)) {
      runCloudOtaUpdate();
      return;
    }
    strncpy(activeStickyNote, incomingNoteBuf, 21);
    activeStickyNote[21] = '\0';
    stickyNoteActive = true;
    inIdleClock = false;
    manualClockActive = false;
    if (lifeState != LIFE_AWAKE) lifeState = LIFE_AWAKE;
    playMoodSound(SND_NOTE_RECV);
  }
  if (hasIncomingHb) {
    hasIncomingHb = false;
    incrementLoveCounter();
    parseAndStartHeartbeatPlayback(now);
  }
  if (incomingPomoCmd > 0) {
    int cmd = incomingPomoCmd;
    incomingPomoCmd = 0;
    if (lifeState != LIFE_AWAKE) lifeState = LIFE_AWAKE;
    inIdleClock = false;
    manualClockActive = false;
    if (cmd == 1) {
      currentUI = STATE_POMODORO;
      startPomodoroPhase(POMO_FOCUS, now, false);
    } else if (cmd == 2) {
      currentUI = STATE_POMODORO;
      startPomodoroPhase(POMO_BREAK, now, false);
    } else if (cmd == 3) {
      startPomodoroPhase(POMO_IDLE, now, false);
    }
  }
  if (incomingMood >= 0) {
    Mood prevHer = remoteHerMood;
    remoteHerMood = (Mood)incomingMood;
    incomingMood = -1;
    if (lifeState == LIFE_AWAKE) {
      if (remoteHerMood == myLocalMood && remoteHerMood != SIMPLE && remoteHerMood != prevHer) {
        triggerOverride(TWIN_SYNC, 3200, now);
      } else {
        playMoodSound(remoteHerMood);
      }
    }
  }
  if (incomingAction >= 0) {
    Mood act = (Mood)incomingAction;
    incomingAction = -1;
    incrementLoveCounter();
    if (lifeState != LIFE_AWAKE) lifeState = LIFE_AWAKE;
    unsigned long dur = (act == ACTION_BDAY) ? 6000 : ((act == ACTION_POKE) ? 2850 : 3200);
    triggerOverride(act, dur, now);
  }
}

void flushPublish() {
  if (!wifiEnabled || !mqtt.connected()) return;
  if (publishMoodNeeded) {
    char buf[4] = { (char)('0' + (int)myLocalMood), 0 };
    if (mqtt.publish(MY_MOOD_TOPIC, buf, true)) publishMoodNeeded = false;
  }
  if (publishDinoNeeded) {
    char scoreStr[12];
    snprintf(scoreStr, sizeof(scoreStr), "%d", myDinoHigh);
    if (mqtt.publish(MY_DINO_TOPIC, scoreStr, true)) publishDinoNeeded = false;
  }
  if (publishFlapNeeded) {
    char scoreStr[12];
    snprintf(scoreStr, sizeof(scoreStr), "%d", myFlapHigh);
    if (mqtt.publish(MY_FLAP_TOPIC, scoreStr, true)) publishFlapNeeded = false;
  }
  if (publishDodgeNeeded) {
    char scoreStr[12];
    snprintf(scoreStr, sizeof(scoreStr), "%d", myDodgeHigh);
    if (mqtt.publish(MY_DODGE_TOPIC, scoreStr, true)) publishDodgeNeeded = false;
  }
  if (publishHbNeeded) {
    if (mqtt.publish(SEND_HB_TOPIC, hbOutgoingPayload, false)) publishHbNeeded = false;
  }
  if (publishPomoNeeded) {
    if (mqtt.publish(SEND_POMO_TOPIC, pomoOutgoingCmd, false)) publishPomoNeeded = false;
  }
  if (publishNoteNeeded) {
    mqtt.publish(SEND_NOTE_TOPIC, outgoingStickyNote, false);
    mqtt.publish(SHARED_NOTE_TOPIC, outgoingStickyNote, false);
    publishNoteNeeded = false;
  }
  if (pendingActionPublish >= 0) {
    char buf[4] = { (char)('0' + pendingActionPublish), 0 };
    if (mqtt.publish(SEND_ACT_TOPIC, buf, false)) pendingActionPublish = -1;
  }
}

void maintainConnection(unsigned long now) {
  if (!wifiEnabled) return;
  if (validClapCount > 0 || isVoiceListening || isBlowingMic ||
      currentUI == STATE_DINO_GAME || currentUI == STATE_FLAPPY_GAME ||
      currentUI == STATE_DODGE_GAME) return;

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiReady) {
      wifiReady = true;
      setupNTP();
    }
    if (!mqtt.connected() && (now - lastMqttAttempt > 6000)) {
      lastMqttAttempt = now;
      if (mqtt.connect(mqttClientId, MQTT_USER, MQTT_PASS)) {
        mqtt.subscribe(REMOTE_MOOD_TOPIC);
        mqtt.subscribe(RECV_ACT_TOPIC);
        mqtt.subscribe(REMOTE_DINO_TOPIC);
        mqtt.subscribe(REMOTE_FLAP_TOPIC);
        mqtt.subscribe(REMOTE_DODGE_TOPIC);
        mqtt.subscribe(RECV_HB_TOPIC);
        mqtt.subscribe(RECV_POMO_TOPIC);
        mqtt.subscribe(RECV_NOTE_TOPIC);
        mqtt.subscribe(SHARED_NOTE_TOPIC);
        publishMoodNeeded  = true;
        publishDinoNeeded  = true;
        publishFlapNeeded  = true;
        publishDodgeNeeded = true;
      }
    }
  } else {
    wifiReady = false;
    if (!apActive && (now - wifiBootAt > 12000)) {
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP("Robo-Setup");
      applyBatteryOptimization();
      apActive = true;
    }
  }
}

// =========================================================================
// UNIFIED BUTTON HANDLER (4 MAIN BUTTONS + ONBOARD GPIO 0 SHORTCUT)
// =========================================================================
void handleUpAction(unsigned long now) {
  lastActivityAt = now;
  lastButtonPressAt = now;
  clearTouchState();
  if (lifeState == LIFE_SLEEPING) { startWakeUp(now); return; }
  if (lifeState == LIFE_WAKING_UP) return;
  if (stickyNoteActive) { stickyNoteActive = false; playMoodSound(SND_MENU_TICK); return; }

  if (currentUI == STATE_DINO_GAME) {
    if (!dinoGameOver && dinoY <= 0.1f) { dinoVel = 6.2f; playMoodSound(SND_DINO_JUMP); }
  } else if (currentUI == STATE_FLAPPY_GAME) {
    if (!flapGameOver) { flapVel = -3.4f; playMoodSound(SND_DINO_JUMP); }
  } else if (currentUI == STATE_DODGE_GAME) {
    // Continuous steering handled in updateAndRenderDodge()
  } else if (currentUI == STATE_HEARTBEAT) {
    recordHeartbeatTap(now);
  } else if (currentUI == STATE_VOICE_CHAT) {
    triggerR2D2Reply(now, false);
  } else if (currentUI == STATE_SOUND_PARTY) {
    currentR2ReplyId = (currentR2ReplyId + 1) % 16;
    playMoodSound(MIC_R2D2_REPLY);
  } else if (currentUI == STATE_POMODORO) {
    if (pomoPhase == POMO_IDLE) {
      pomoSelectedBreak = !pomoSelectedBreak;
      playMoodSound(SND_MENU_TICK);
    }
  } else if (currentUI == STATE_WEATHER_STATION) {
    weatherSplitLayout = !weatherSplitLayout;
    playMoodSound(SND_MENU_TICK);
  } else if (currentUI == STATE_CLOCK || manualClockActive || inIdleClock) {
    clockTwinPodLayout = !clockTwinPodLayout;
    playMoodSound(SND_MENU_TICK);
  } else if (currentUI == STATE_NORMAL_HER_ROBO) {
    cycleNextSeason(now);
  } else if (currentUI == STATE_MENU_MAIN) {
    menuSelection = (menuSelection + 1) % TOTAL_ROOT_ITEMS;
    playMoodSound(SND_MENU_TICK);
  } else if (currentUI == STATE_GAMES_MENU) {
    gameMenuSelection = (gameMenuSelection + 1) % TOTAL_GAME_ITEMS;
    playMoodSound(SND_MENU_TICK);
  } else if (currentUI == STATE_SETTINGS) {
    settingsSelection = (settingsSelection + 1) % TOTAL_SETTINGS_ITEMS;
    playMoodSound(SND_MENU_TICK);
  } else if (currentUI == STATE_ACTIONS_MENU) {
    actionSelection = (actionSelection + 1) % TOTAL_ACTION_ITEMS;
    playMoodSound(SND_MENU_TICK);
  } else if (currentUI == STATE_MY_ROBO) {
    myLocalMood = (Mood)((myLocalMood + 1) % SYNC_MOOD_COUNT);
    publishMoodNeeded = true;
    screenEnteredAt = now;
    if (myLocalMood == remoteHerMood && myLocalMood != SIMPLE) {
      playMoodSound(TWIN_SYNC);
    } else {
      playMoodSound(myLocalMood);
    }
  }
}

void executeMenuOrScreenSelect(unsigned long now) {
  if (currentUI == STATE_DINO_GAME) {
    if (dinoGameOver) { resetDinoGame(); playMoodSound(SIMPLE); }
    else if (dinoY <= 0.1f) { dinoVel = 6.2f; playMoodSound(SND_DINO_JUMP); }
  } else if (currentUI == STATE_FLAPPY_GAME) {
    if (flapGameOver) { resetFlappyGame(); playMoodSound(SIMPLE); }
    else { flapVel = -3.4f; playMoodSound(SND_DINO_JUMP); }
  } else if (currentUI == STATE_DODGE_GAME) {
    if (dodgeGameOver) { resetDodgeGame(); playMoodSound(SIMPLE); }
  } else if (currentUI == STATE_HEARTBEAT) {
    recordHeartbeatTap(now);
  } else if (currentUI == STATE_VOICE_CHAT) {
    triggerR2D2Reply(now, true);
  } else if (currentUI == STATE_SOUND_PARTY) {
    currentR2ReplyId = esp_random() % 16;
    playMoodSound(MIC_R2D2_REPLY);
  } else if (currentUI == STATE_POMODORO) {
    if (pomoPhase == POMO_IDLE) {
      startPomodoroPhase(pomoSelectedBreak ? POMO_BREAK : POMO_FOCUS, now, true);
    } else {
      startPomodoroPhase(POMO_IDLE, now, true);
    }
  } else if (currentUI == STATE_MENU_MAIN) {
    if (menuSelection == 0)      { currentUI = STATE_MY_ROBO; screenEnteredAt = now; playMoodSound(HAPPY); }
    else if (menuSelection == 1) { actionSelection = 0; currentUI = STATE_ACTIONS_MENU; playMoodSound(SND_MENU_TICK); }
    else if (menuSelection == 2) { gameMenuSelection = 0; currentUI = STATE_GAMES_MENU; playMoodSound(SND_MENU_TICK); }
    else if (menuSelection == 3) { enterVoiceChatMode(now); }
    else if (menuSelection == 4) { currentUI = STATE_CLOCK; manualClockActive = true; playMoodSound(CLOCK); }
    else if (menuSelection == 5) { currentUI = STATE_WEATHER_STATION; manualClockActive = false; playMoodSound(CLOCK); }
    else if (menuSelection == 6) { hbTapCount = 0; hbSentBanner = false; currentUI = STATE_HEARTBEAT; playMoodSound(SND_HEARTBEAT); }
    else if (menuSelection == 7) { currentUI = STATE_POMODORO; playMoodSound(SND_MENU_TICK); }
    else if (menuSelection == 8) { currentUI = STATE_SOUND_PARTY; currentR2ReplyId = 0; playMoodSound(MIC_R2D2_REPLY); }
    else if (menuSelection == 9) { settingsSelection = 0; currentUI = STATE_SETTINGS; playMoodSound(SND_MENU_TICK); }
  } else if (currentUI == STATE_GAMES_MENU) {
    if (gameMenuSelection == 0) {
      dinoVoiceMode = false; resetDinoGame(); currentUI = STATE_DINO_GAME; playMoodSound(HAPPY);
    } else if (gameMenuSelection == 1) {
      dinoVoiceMode = true;  resetDinoGame(); currentUI = STATE_DINO_GAME; playMoodSound(HAPPY);
    } else if (gameMenuSelection == 2) {
      resetFlappyGame(); currentUI = STATE_FLAPPY_GAME; playMoodSound(HAPPY);
    } else if (gameMenuSelection == 3) {
      resetDodgeGame();  currentUI = STATE_DODGE_GAME;  playMoodSound(HAPPY);
    } else {
      currentUI = STATE_MENU_MAIN; playMoodSound(SND_MENU_TICK);
    }
  } else if (currentUI == STATE_SETTINGS) {
    if (settingsSelection == 0) {
      audioMuted = !audioMuted;
      preferences.begin("robot", false); preferences.putBool("muted", audioMuted); preferences.end();
      if (!audioMuted) playMoodSound(HAPPY);
    } else if (settingsSelection == 1) {
      micEnabled = !micEnabled;
      preferences.begin("robot", false); preferences.putBool("mic_en", micEnabled); preferences.end();
      playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 2) {
      ldrEnabled = !ldrEnabled;
      preferences.begin("robot", false); preferences.putBool("ldr_en", ldrEnabled); preferences.end();
      playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 3) {
      motionEnabled = !motionEnabled; mpuBaselineReady = false; shakeEnergy = 0.0f;
      preferences.begin("robot", false); preferences.putBool("mpu_en", motionEnabled); preferences.end();
      playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 4) {
      hydrateEnabled = !hydrateEnabled; lastHydrateAt = now;
      preferences.begin("robot", false); preferences.putBool("hyd_en", hydrateEnabled); preferences.end();
      playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 5) {
      battSaverMode = !battSaverMode;
      preferences.begin("robot", false); preferences.putBool("bat_sav", battSaverMode); preferences.end();
      applyBatteryOptimization();
      playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 6) {
      wifiEnabled = !wifiEnabled;
      preferences.begin("robot", false); preferences.putBool("wifi_en", wifiEnabled); preferences.end();
      if (wifiEnabled) {
        WiFi.mode(WIFI_STA); applyBatteryOptimization();
        WiFi.begin(savedSsid.c_str(), savedPass.c_str()); wifiBootAt = now;
      } else {
        mqtt.disconnect(); WiFi.disconnect(true); WiFi.mode(WIFI_OFF); wifiReady = false; apActive = false;
      }
      playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 7) {
      if (!manualSeasonOverride) {
        manualSeasonOverride = true;
        currentSeason = SEASON_SPRING_SAKURA;
      } else {
        int nextS = (int)currentSeason + 1;
        if (nextS >= SEASON_TOTAL) manualSeasonOverride = false;
        else currentSeason = (SeasonType)nextS;
      }
      initWeatherParticles();
      playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 8) {
      if (idleSecSetting == 45) idleSecSetting = 90;
      else if (idleSecSetting == 90) idleSecSetting = 0;
      else idleSecSetting = 45;
      idleTimeoutMs = (unsigned long)idleSecSetting * 1000UL;
      preferences.begin("robot", false); preferences.putInt("idle", idleSecSetting); preferences.end();
      playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 9) {
      currentUI = STATE_CONNECTION_INFO; playMoodSound(SND_MENU_TICK);
    } else if (settingsSelection == 10) {
      currentUI = STATE_MENU_MAIN; playMoodSound(SND_MENU_TICK);
    }
  } else if (currentUI == STATE_ACTIONS_MENU) {
    if (actionSelection == 0) {
      pendingActionPublish = (int)ACTION_HUG; incrementLoveCounter();
      currentUI = STATE_NORMAL_HER_ROBO; triggerOverride(ACTION_HUG, 3200, now);
    } else if (actionSelection == 1) {
      pendingActionPublish = (int)ACTION_POKE; incrementLoveCounter();
      currentUI = STATE_NORMAL_HER_ROBO; triggerOverride(ACTION_POKE, 2850, now);
    } else if (actionSelection == 2) {
      pendingActionPublish = (int)ACTION_KISS; incrementLoveCounter();
      currentUI = STATE_NORMAL_HER_ROBO; triggerOverride(ACTION_KISS, 3200, now);
    } else {
      currentUI = STATE_MENU_MAIN; playMoodSound(SND_MENU_TICK);
    }
  } else if (currentUI == STATE_MY_ROBO) {
    myLocalMood = (Mood)((myLocalMood + 1) % SYNC_MOOD_COUNT);
    publishMoodNeeded = true;
    screenEnteredAt = now;
    if (myLocalMood == remoteHerMood && myLocalMood != SIMPLE) playMoodSound(TWIN_SYNC);
    else playMoodSound(myLocalMood);
  }
}

void readButtons(unsigned long now) {
  bool upPressed  = (digitalRead(BTN_UP_PIN) == HIGH);
  bool selPressed = (digitalRead(BTN_SEL_PIN) == HIGH);

  if (upPressed && selPressed && currentUI != STATE_DODGE_GAME) {
    static unsigned long lastComboAt = 0;
    if (now - lastComboAt > 450) {
      lastComboAt = now;
      lastActivityAt = now;
      lastButtonPressAt = now;
      selShortClickPending = false;
      selLongPressFired = true;
      holdProgressBar = 0.0f;
      clearTouchState();
      if (lifeState != LIFE_AWAKE) { startWakeUp(now); return; }
      if (stickyNoteActive) { stickyNoteActive = false; playMoodSound(SND_MENU_TICK); return; }
      inIdleClock = false;
      manualClockActive = false;
      currentUI = STATE_MY_ROBO;
      screenEnteredAt = now;
      playMoodSound(HAPPY);
      return;
    }
  }

  // 1. DEDICATED HOME / HER ROBO BUTTON (GPIO 34)
  int herReading = digitalRead(HER_ROBO_HOME_BTN);
  if (herReading != lastHerBtnReading) { lastHerBtnReading = herReading; lastHerBtnDebounce = now; }
  if (now - lastHerBtnDebounce >= DEBOUNCE_MS && stableHerBtnState != lastHerBtnReading) {
    stableHerBtnState = lastHerBtnReading;
    if (stableHerBtnState == HIGH) {
      homePressStartMs = now;
      homeLongPressFired = false;
      lastActivityAt = now;
      lastButtonPressAt = now;
      clearTouchState();
      if (lifeState == LIFE_SLEEPING) { homeLongPressFired = true; startWakeUp(now); return; }
      if (stickyNoteActive) { homeLongPressFired = true; stickyNoteActive = false; playMoodSound(SND_MENU_TICK); return; }
    } else if (stableHerBtnState == LOW) {
      holdProgressBar = 0.0f;
      if (!homeLongPressFired) {
        inIdleClock = false;
        manualClockActive = false;
        hasLocalOverride = false;
        hbPlaybackActive = false;
        isBlowingMic = false;
        isVoiceListening = false;
        isTooLoudTracking = false;
        validClapCount = 0;
        shakeEnergy = 0.0f;
        currentUI = STATE_NORMAL_HER_ROBO;
        screenEnteredAt = now;
        initWeatherParticles();
        playMoodSound(SIMPLE);
      }
    }
  }
  if (stableHerBtnState == HIGH && !homeLongPressFired && lifeState == LIFE_AWAKE) {
    unsigned long held = now - homePressStartMs;
    if (held > 100) holdProgressBar = (float)(held - 100) / (float)(LONG_PRESS_MS - 100);
    if (held >= LONG_PRESS_MS) {
      holdProgressBar = 0.0f;
      homeLongPressFired = true;
      lastButtonPressAt = now;
      enterVoiceChatMode(now);
      return;
    }
  }

  // 2. UP / NEXT BUTTON (GPIO 14)
  int upReading = digitalRead(BTN_UP_PIN);
  if (upReading != lastUpReading) { lastUpReading = upReading; lastUpDebounce = now; }
  if (now - lastUpDebounce >= DEBOUNCE_MS && stableUpState != lastUpReading) {
    stableUpState = lastUpReading;
    if (stableUpState == HIGH && (!selPressed || currentUI == STATE_DODGE_GAME)) {
      handleUpAction(now);
    }
  }

  // 3. SELECT / ENTER BUTTON (GPIO 12)
  int selReading = digitalRead(BTN_SEL_PIN);
  if (selReading != lastSelReading) { lastSelReading = selReading; lastSelDebounce = now; }
  if (now - lastSelDebounce >= DEBOUNCE_MS && stableSelState != lastSelReading) {
    stableSelState = lastSelReading;
    if (stableSelState == HIGH && (!upPressed || currentUI == STATE_DODGE_GAME)) {
      lastActivityAt = now;
      lastButtonPressAt = now;
      selPressStartMs = now;
      selLongPressFired = false;
      clearTouchState();
      if (lifeState == LIFE_SLEEPING) { selLongPressFired = true; startWakeUp(now); return; }
      if (lifeState == LIFE_WAKING_UP) { selLongPressFired = true; return; }
      if (stickyNoteActive) { selLongPressFired = true; stickyNoteActive = false; playMoodSound(SND_MENU_TICK); return; }

      if (currentUI == STATE_NORMAL_HER_ROBO || currentUI == STATE_CLOCK ||
          currentUI == STATE_WEATHER_STATION || manualClockActive || inIdleClock) {
        if (now - lastSelClickTime < DOUBLE_CLICK_MS) {
          lastSelClickTime = 0;
          selShortClickPending = false;
          selLongPressFired = true;
          holdProgressBar = 0.0f;
          manualClockActive = false;
          inIdleClock = false;
          hasLocalOverride = false;
          currentUI = STATE_MY_ROBO;
          screenEnteredAt = now;
          initWeatherParticles();
          playMoodSound(HAPPY);
          return;
        }
        lastSelClickTime = now;
        selShortClickPending = true;
        return;
      }

      executeMenuOrScreenSelect(now);
    } else if (stableSelState == LOW) {
      holdProgressBar = 0.0f;
      if (selShortClickPending && !selLongPressFired) {
        selShortClickPending = false;
        manualClockActive = false;
        inIdleClock = false;
        hasLocalOverride = false;
        currentUI = STATE_MENU_MAIN;
        playMoodSound(SND_MENU_TICK);
      }
    }
  }

  if (stableSelState == HIGH && selShortClickPending && !selLongPressFired) {
    unsigned long held = now - selPressStartMs;
    if (held > 100) holdProgressBar = (float)(held - 100) / (float)(LONG_PRESS_MS - 100);
    if (held >= LONG_PRESS_MS) {
      holdProgressBar = 0.0f;
      selLongPressFired = true;
      selShortClickPending = false;
      lastSelClickTime = 0;
      lastButtonPressAt = now;
      enterVoiceChatMode(now);
      return;
    }
  }

  // 4. BACK / VIEW CYCLE BUTTON (GPIO 13)
  int backReading = digitalRead(BTN_BACK_PIN);
  if (backReading != lastBackReading) { lastBackReading = backReading; lastBackDebounce = now; }
  if (now - lastBackDebounce >= DEBOUNCE_MS && stableBackState != lastBackReading) {
    stableBackState = lastBackReading;
    if (stableBackState == HIGH) {
      lastActivityAt = now;
      lastButtonPressAt = now;
      clearTouchState();
      if (lifeState == LIFE_SLEEPING) { startWakeUp(now); return; }
      if (lifeState == LIFE_WAKING_UP) return;
      if (stickyNoteActive) { stickyNoteActive = false; playMoodSound(SND_MENU_TICK); return; }

      if (hbPlaybackActive) {
        hbPlaybackActive = false;
        playMoodSound(SIMPLE);
        return;
      }

      if (currentUI == STATE_NORMAL_HER_ROBO && !manualClockActive && !inIdleClock) {
        manualClockActive = true;
        inIdleClock = false;
        playMoodSound(CLOCK);
      } else if (manualClockActive || inIdleClock || currentUI == STATE_CLOCK) {
        manualClockActive = false;
        inIdleClock = false;
        currentUI = STATE_WEATHER_STATION;
        playMoodSound(CLOCK);
      } else if (currentUI == STATE_WEATHER_STATION || currentUI == STATE_VOICE_CHAT) {
        manualClockActive = false;
        inIdleClock = false;
        hasLocalOverride = false;
        isVoiceListening = false;
        currentUI = STATE_NORMAL_HER_ROBO;
        screenEnteredAt = now;
        playMoodSound(SIMPLE);
      } else if (currentUI == STATE_DINO_GAME || currentUI == STATE_FLAPPY_GAME || currentUI == STATE_DODGE_GAME) {
        currentUI = STATE_GAMES_MENU;
        playMoodSound(SND_MENU_TICK);
      } else if (currentUI == STATE_CONNECTION_INFO) {
        currentUI = STATE_SETTINGS;
        playMoodSound(SND_MENU_TICK);
      } else if (currentUI == STATE_MENU_MAIN) {
        currentUI = STATE_NORMAL_HER_ROBO;
        screenEnteredAt = now;
        playMoodSound(SIMPLE);
      } else {
        currentUI = STATE_MENU_MAIN;
        playMoodSound(SND_MENU_TICK);
      }
    }
  }

  // 5. ONBOARD GPIO 0 (BOOT) BUTTON SUPPORT
  int bootReading = digitalRead(BOOT_BTN_PIN);
  if (bootReading != lastBootBtnReading) { lastBootBtnReading = bootReading; lastBootBtnDebounce = now; }
  if (now - lastBootBtnDebounce >= 35 && stableBootBtnState != lastBootBtnReading) {
    stableBootBtnState = lastBootBtnReading;
    if (stableBootBtnState == LOW) {
      bootPressStartMs = now;
      bootLongPressFired = false;
      lastActivityAt = now;
      lastButtonPressAt = now;
      if (lifeState == LIFE_SLEEPING) { bootLongPressFired = true; startWakeUp(now); return; }
      if (stickyNoteActive) { bootLongPressFired = true; stickyNoteActive = false; playMoodSound(SND_MENU_TICK); return; }
    } else {
      holdProgressBar = 0.0f;
      if (!bootLongPressFired) {
        handleUpAction(now);
      }
    }
  }
  if (stableBootBtnState == LOW && !bootLongPressFired && lifeState == LIFE_AWAKE) {
    unsigned long held = now - bootPressStartMs;
    if (held > 90) holdProgressBar = (float)(held - 90) / 400.0f;
    if (held >= 490) {
      holdProgressBar = 0.0f;
      bootLongPressFired = true;
      if (currentUI == STATE_NORMAL_HER_ROBO || currentUI == STATE_CLOCK ||
          currentUI == STATE_WEATHER_STATION || manualClockActive || inIdleClock) {
        manualClockActive = false;
        inIdleClock = false;
        currentUI = STATE_MENU_MAIN;
        playMoodSound(SND_MENU_TICK);
      } else {
        executeMenuOrScreenSelect(now);
      }
    }
  }
}

void checkIdleClock(unsigned long now) {
  if (idleSecSetting > 0 && lifeState == LIFE_AWAKE && currentUI == STATE_NORMAL_HER_ROBO &&
      !inIdleClock && !manualClockActive && !isPetting && !hasLocalOverride &&
      !hbPlaybackActive && !stickyNoteActive && !isBlowingMic && !isTooLoudTracking &&
      (now - lastActivityAt >= idleTimeoutMs)) {
    inIdleClock = true;
  }
}

// =========================================================================
// SETUP & MAIN LOOP
// =========================================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(150);

  pinMode(BOOT_BTN_PIN,      INPUT_PULLUP);
  pinMode(HER_ROBO_HOME_BTN, INPUT);
  pinMode(LDR_PIN,           INPUT);
  pinMode(BAT_ADC_PIN,       INPUT);
  pinMode(BTN_UP_PIN,        INPUT_PULLDOWN);
  pinMode(BTN_SEL_PIN,       INPUT_PULLDOWN);
  pinMode(BTN_BACK_PIN,      INPUT_PULLDOWN);

  initBuzzer();
  initTouchSensors();
  initMotionSensor();

  frame = new GFXcanvas16(SW, SH);
  if (!frame || !frame->getBuffer()) while (1) delay(1000);
  frame->setTextWrap(false);

  preferences.begin("robot", false);
  savedSsid      = preferences.getString("ssid",      DEFAULT_WIFI_SSID);
  savedPass      = preferences.getString("pass",      DEFAULT_WIFI_PASS);
  savedTz        = preferences.getString("tz_str",    DEFAULT_MY_TZ);
  savedPartnerTz = preferences.getString("ptz_str",   DEFAULT_PARTNER_TZ);
  idleSecSetting = preferences.getInt("idle",         DEFAULT_IDLE_SEC);
  myDinoHigh     = preferences.getInt("my_hi",        0);
  herDinoHigh    = preferences.getInt("her_hi",       0);
  myFlapHigh     = preferences.getInt("flap_hi",      0);
  herFlapHigh    = preferences.getInt("her_flap",     0);
  myDodgeHigh    = preferences.getInt("dodge_hi",     0);
  herDodgeHigh   = preferences.getInt("her_dodge",    0);
  totalLoveCount = preferences.getInt("love_cnt",     0);
  eventName      = preferences.getString("ev_name",   "Meetup");
  eventDaysLeft  = preferences.getInt("ev_days",      0);
  micEnabled     = preferences.getBool("mic_en",      true);
  ldrEnabled     = preferences.getBool("ldr_en",      false);
  audioMuted     = preferences.getBool("muted",       false);
  motionEnabled  = preferences.getBool("mpu_en",      true);
  hydrateEnabled = preferences.getBool("hyd_en",      true);
  battSaverMode  = preferences.getBool("bat_sav",     true);
  wifiEnabled    = preferences.getBool("wifi_en",     true);
  preferences.end();

  if (savedSsid.length() == 0)      savedSsid      = DEFAULT_WIFI_SSID;
  if (savedTz.length() == 0)        savedTz        = DEFAULT_MY_TZ;
  if (savedPartnerTz.length() == 0) savedPartnerTz = DEFAULT_PARTNER_TZ;
  idleTimeoutMs = (unsigned long)idleSecSetting * 1000UL;

  setupNTP();

  // 1. Power up Wi-Fi radio BEFORE initializing ST7735 panel to prevent RF spike freeze
  WiFi.persistent(false);
  if (wifiEnabled) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    applyBatteryOptimization();
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    wifiBootAt = millis();
  } else {
    WiFi.mode(WIFI_OFF);
  }
  delay(450);

  server.on("/", handleRoot);
  server.on("/beam", handleBeam);
  server.on("/save", handleSave);
  server.begin();

  // 2. Initialize ST7735 panel (with 180-deg TFT_ROTATION = 2) & I2S Microphone
  recoverPanel();
  tft.fillScreen(BLACK);
  initMicrophone();
  initWeatherParticles();

  lifeState = LIFE_SLEEPING;
  lastActivityAt = millis();
  lastHydrateAt = lastActivityAt;
  lastTantrumCheckAt = lastActivityAt;
  screenEnteredAt = lastActivityAt;
  updateBatteryMonitor(lastActivityAt);
  show(lastActivityAt);

  snprintf(mqttClientId, sizeof(mqttClientId), "robo_%08X", (uint32_t)ESP.getEfuseMac());
  net.setInsecure();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(2);

  // 3. Start background Live Open-Meteo Weather task (Halle, DE & Srinagar, IN) on Core 0
  xTaskCreatePinnedToCore(weatherSyncTask, "WeatherSync", 8192, NULL, 1, NULL, 0);
}

void loop() {
  unsigned long now = millis();

  if (requestPanelRecover) {
    requestPanelRecover = false;
    recoverPanel();
  }
  if (requestParticleInit) {
    requestParticleInit = false;
    initWeatherParticles();
  }

  if (wifiEnabled) {
    server.handleClient();
    maintainConnection(now);
    if (mqtt.connected()) mqtt.loop();
  }
  applyIncoming(now);
  updateBatteryMonitor(now);
  updateLifeAndLdr(now);
  processMicrophoneEvents(now);
  updatePersonalityEngine(now);
  updateHeartbeatEngine(now);
  updatePomodoroTimer(now);
  readButtons(now);
  readTouchSensors(now);
  readMotionSensor(now);
  checkIdleClock(now);
  flushPublish();

  unsigned long targetFrameMs = ANIM_FRAME_FAST_MS;
  if (battSaverMode && currentUI != STATE_DINO_GAME && currentUI != STATE_FLAPPY_GAME &&
      currentUI != STATE_DODGE_GAME && currentUI != STATE_SOUND_PARTY &&
      currentUI != STATE_VOICE_CHAT && !hasLocalOverride && !isPetting && !isBlowingMic &&
      validClapCount == 0 &&
      (lifeState == LIFE_SLEEPING || manualClockActive || inIdleClock || currentUI == STATE_CLOCK)) {
    targetFrameMs = ANIM_FRAME_SAVER_MS;
  }

  if (now - lastAnimDraw >= targetFrameMs) {
    lastAnimDraw = now;
    show(now);
  }
}
// ==================== END OF PART 3 (COMPLETE FINAL FIRMWARE) ====================
