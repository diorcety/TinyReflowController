/*******************************************************************************
  Title: Tiny Reflow Controller
  Version: 2.10
  Date: 03-03-2019
  Company: Rocket Scream Electronics
  Author: Lim Phang Moh
  Website: www.rocketscream.com
  Contributor: Layne Berge https://www.heavidesign.com/

  Brief
  =====
  This is an example firmware for our Arduino compatible Tiny Reflow Controller.
  A big portion of the code is copied over from our Reflow Oven Controller
  Shield. We added both lead-free and leaded reflow profile support in this
  firmware which can be selected by pressing switch #2 (labelled as LF|PB on PCB)
  during system idle. The unit will remember the last selected reflow profile.
  You'll need to use the MAX31856 library for Arduino.

  Lead-Free Reflow Curve
  ======================

  Temperature (Degree Celcius)                 Magic Happens Here!
  245-|                                               x  x
      |                                            x        x
      |                                         x              x
      |                                      x                    x
  200-|                                   x                          x
      |                              x    |                          |   x
      |                         x         |                          |       x
      |                    x              |                          |
  150-|               x                   |                          |
      |             x |                   |                          |
      |           x   |                   |                          |
      |         x     |                   |                          |
      |       x       |                   |                          |
      |     x         |                   |                          |
      |   x           |                   |                          |
  30 -| x             |                   |                          |
      |<  60 - 90 s  >|<    90 - 120 s   >|<       90 - 120 s       >|
      | Preheat Stage |   Soaking Stage   |       Reflow Stage       | Cool
   0  |_ _ _ _ _ _ _ _|_ _ _ _ _ _ _ _ _ _|_ _ _ _ _ _ _ _ _ _ _ _ _ |_ _ _ _ _
                                                                 Time (Seconds)

  Leaded Reflow Curve (Kester EP256)
  ==================================

  Temperature (Degree Celcius)         Magic Happens Here!
  219-|                                       x  x
      |                                    x        x
      |                                 x              x
  180-|                              x                    x
      |                         x    |                    |   x
      |                    x         |                    |       x
  150-|               x              |                    |           x
      |             x |              |                    |
      |           x   |              |                    |
      |         x     |              |                    |
      |       x       |              |                    |
      |     x         |              |                    |
      |   x           |              |                    |
  30 -| x             |              |                    |
      |<  60 - 90 s  >|<  60 - 90 s >|<   60 - 90 s      >|
      | Preheat Stage | Soaking Stage|   Reflow Stage     | Cool
   0  |_ _ _ _ _ _ _ _|_ _ _ _ _ _ _ |_ _ _ _ _ _ _ _ _ _ |_ _ _ _ _ _ _ _ _ _ _
                                                                 Time (Seconds)

  This firmware owed very much on the works of other talented individuals as
  follows:
  ==========================================
  Brett Beauregard (www.brettbeauregard.com)
  ==========================================
  Author of Arduino PID library. On top of providing industry standard PID
  implementation, he gave a lot of help in making this reflow oven controller
  possible using his awesome library.

  ==========================================
  Limor Fried of Adafruit (www.adafruit.com)
  ==========================================
  Author of Arduino MAX31856 and SSD1306 libraries. Adafruit has been the source 
  of tonnes of tutorials, examples, and libraries for everyone to learn.

  ==========================================
  Spence Konde (www.drazzy.com/e/)
  ==========================================
  Maintainer of the ATtiny core for Arduino:
  https://github.com/SpenceKonde/ATTinyCore

  Disclaimer
  ==========
  Dealing with high voltage is a very dangerous act! Please make sure you know
  what you are dealing with and have proper knowledge before hand. Your use of
  any information or materials on this Tiny Reflow Controller is entirely at
  your own risk, for which we shall not be liable.

  Licences
  ========
  This Tiny Reflow Controller hardware and firmware are released under the
  Creative Commons Share Alike v3.0 license
  http://creativecommons.org/licenses/by-sa/3.0/
  You are free to take this piece of code, use it and modify it.
  All we ask is attribution including the supporting libraries used in this
  firmware.

  Required Libraries
  ==================
  - Arduino PID Library:
    >> https://github.com/br3ttb/Arduino-PID-Library
  - Adafruit MAX31856 Library:
    >> https://github.com/adafruit/Adafruit_MAX31856
  - Adafruit SSD1306 Library:
    >> https://github.com/adafruit/Adafruit_SSD1306
  - Adafruit GFX Library:
    >> https://github.com/adafruit/Adafruit-GFX-Library

  Revision  Description
  ========  ===========
  2.10      Added Bake option (Layne Berge)
            - Sets oven to bake temp and holds indefinitely
  2.00      Support V2 of the Tiny Reflow Controller:
            - Based on ATMega328P 3.3V @ 8MHz
            - Uses SSD1306 128x64 OLED
  1.00      Initial public release:
            - Based on ATtiny1634R 3.3V @ 8MHz
            - Uses 8x2 alphanumeric LCD

*******************************************************************************/

// ***** CONSTANTS *****
// ***** GENERAL *****
#define VERSION 2 // Replace with 1 or 2

// ***** INCLUDES *****
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#if VERSION == 1
#include <LiquidCrystal.h>
#elif VERSION == 2
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif
#include <Adafruit_MAX31856.h> 
#include <PID_v1.h>

// ***** TYPE DEFINITIONS *****
typedef enum REFLOW_STATE : uint8_t
{
  REFLOW_STATE_IDLE,
  REFLOW_STATE_PREHEAT,
  REFLOW_STATE_SOAK,
  REFLOW_STATE_REFLOW,
  REFLOW_STATE_COOL,
  REFLOW_STATE_COMPLETE,
  REFLOW_STATE_TOO_HOT,
  REFLOW_STATE_ERROR,
  REFLOW_STATE_BAKE
} reflowState_t;

typedef enum REFLOW_STATUS : uint8_t
{
  REFLOW_STATUS_OFF,
  REFLOW_STATUS_ON
} reflowStatus_t;

typedef	enum SWITCH : uint8_t
{
  SWITCH_NONE,
  SWITCH_1,
  SWITCH_2
} switch_t;

typedef enum DEBOUNCE_STATE : uint8_t
{
  DEBOUNCE_STATE_IDLE,
  DEBOUNCE_STATE_CHECK,
  DEBOUNCE_STATE_RELEASE
} debounceState_t;

typedef enum REFLOW_PROFILE : uint8_t
{
  REFLOW_PROFILE_LEADFREE,
  REFLOW_PROFILE_LEADED,
  REFLOW_PROFILE_BAKE
} reflowProfile_t;

// ***** GENERAL PROFILE CONSTANTS *****
#define PROFILE_TYPE_ADDRESS 0
#define TEMPERATURE_ROOM 50
#define TEMPERATURE_SOAK_MIN 150
#define TEMPERATURE_COOL_MIN 100
#define SENSOR_SAMPLING_TIME 1000
#define SOAK_TEMPERATURE_STEP 5

// ***** LEAD FREE PROFILE CONSTANTS *****
#define TEMPERATURE_SOAK_MAX_LF 200
#define TEMPERATURE_REFLOW_MAX_LF 250
#define SOAK_MICRO_PERIOD_LF 9000

// ***** LEADED PROFILE CONSTANTS *****
#define TEMPERATURE_SOAK_MAX_PB 180
#define TEMPERATURE_REFLOW_MAX_PB 224
#define SOAK_MICRO_PERIOD_PB 10000

// ***** BAKE PROFILE CONSTANTS *****
#define TEMPERATURE_BAKE 120

// ***** SWITCH SPECIFIC CONSTANTS *****
#define DEBOUNCE_PERIOD_MIN 100

// ***** DISPLAY SPECIFIC CONSTANTS *****
#define UPDATE_RATE 100

// ***** PID PARAMETERS *****
// ***** PRE-HEAT STAGE *****
#define PID_KP_PREHEAT 100
#define PID_KI_PREHEAT 0.025
#define PID_KD_PREHEAT 20
// ***** SOAKING STAGE *****
#define PID_KP_SOAK 300
#define PID_KI_SOAK 0.05
#define PID_KD_SOAK 250
// ***** REFLOW STAGE *****
#define PID_KP_REFLOW 300
#define PID_KI_REFLOW 0.05
#define PID_KD_REFLOW 350
#define PID_SAMPLE_TIME 1000
// ***** BAKE STAGE *****
#define PID_KP_BAKE 100
#define PID_KI_BAKE 0.07
#define PID_KD_BAKE 20

#if VERSION == 2
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define X_AXIS_START 18 // X-axis starting position
#endif

// ***** LCD MESSAGES *****
const char lcdMessagesReflowStatus_1[] PROGMEM = "Ready";
const char lcdMessagesReflowStatus_2[] PROGMEM = "Pre";
const char lcdMessagesReflowStatus_3[] PROGMEM = "Soak";
const char lcdMessagesReflowStatus_4[] PROGMEM = "Reflow";
const char lcdMessagesReflowStatus_5[] PROGMEM = "Cool";
const char lcdMessagesReflowStatus_6[] PROGMEM = "Done!";
const char lcdMessagesReflowStatus_7[] PROGMEM = "Hot!";
const char lcdMessagesReflowStatus_8[] PROGMEM = "Error";
const char lcdMessagesReflowStatus_9[] PROGMEM = "Bake";

const char* const lcdMessagesReflowStatus[] PROGMEM = {
  lcdMessagesReflowStatus_1,
  lcdMessagesReflowStatus_2,
  lcdMessagesReflowStatus_3,
  lcdMessagesReflowStatus_4,
  lcdMessagesReflowStatus_5,
  lcdMessagesReflowStatus_6,
  lcdMessagesReflowStatus_7,
  lcdMessagesReflowStatus_8,
  lcdMessagesReflowStatus_9
};

#if VERSION == 1
// ***** DEGREE SYMBOL FOR LCD *****
static const unsigned char degree[8] = {
  140, 146, 146, 140, 128, 128, 128, 128
};
#endif

// ***** PIN ASSIGNMENT *****
#if VERSION == 1
static const unsigned char ssrPin = 3;
static const unsigned char thermocoupleCSPin = 2;
static const unsigned char lcdRsPin = 10;
static const unsigned char lcdEPin = 9;
static const unsigned char lcdD4Pin = 8;
static const unsigned char lcdD5Pin = 7;
static const unsigned char lcdD6Pin = 6;
static const unsigned char lcdD7Pin = 5;
static const unsigned char buzzerPin = 14;
static const unsigned char switchPin = A1;
static const unsigned char ledPin = LED_BUILTIN;
#elif VERSION == 2
static const unsigned char ssrPin = A0;
static const unsigned char fanPin = A1;
static const unsigned char thermocoupleCSPin = 10;
static const unsigned char ledPin = 4;
static const unsigned char buzzerPin = 5;
static const unsigned char switchStartStopPin = 3;
static const unsigned char switchLfPbPin = 2;
#endif

// ***** PID CONTROL VARIABLES *****
double setpoint;
double input;
double output;
double kp = PID_KP_PREHEAT;
double ki = PID_KI_PREHEAT;
double kd = PID_KD_PREHEAT;
unsigned int windowSize;
unsigned long windowStartTime;
unsigned long nextCheck;
unsigned long nextRead;
unsigned long updateLcd;
unsigned long timerSoak;
unsigned long buzzerPeriod;
unsigned char soakTemperatureMax;
unsigned char reflowTemperatureMax;
unsigned long soakMicroPeriod;
// Reflow oven controller state machine state variable
reflowState_t reflowState;
// Reflow oven controller status
reflowStatus_t reflowStatus;
// Reflow profile type
reflowProfile_t reflowProfile;
// Switch debounce state machine state variable
debounceState_t debounceState;
// Switch debounce timer
long lastDebounceTime;
// Switch press status
switch_t switchStatus;
switch_t switchValue;
switch_t switchMask;
// Seconds timer
unsigned int timerSeconds;
// Thermocouple fault status
unsigned char fault;

#if VERSION == 2
unsigned int timerUpdate;
unsigned char temperature[SCREEN_WIDTH - X_AXIS_START];
unsigned char xHead=0;
unsigned char xCnt=0;
unsigned char xScrollOffset=0;
#endif

// PID control interface
PID reflowOvenPID(&input, &output, &setpoint, kp, ki, kd, DIRECT);
#if VERSION == 1
// LCD interface
LiquidCrystal lcd(lcdRsPin, lcdEPin, lcdD4Pin, lcdD5Pin, lcdD6Pin, lcdD7Pin);
#elif VERSION == 2
uint8_t SSD1306_SFB[SCREEN_WIDTH * ((SCREEN_HEIGHT + 7) / 8)];
class Adafruit_SSD1306_SB : public Adafruit_SSD1306
{
public:
  Adafruit_SSD1306_SB(uint8_t w, uint8_t h, TwoWire* twi = &Wire,
    int8_t rst_pin = -1, uint32_t clkDuring = 400000UL,
    uint32_t clkAfter = 100000UL) : Adafruit_SSD1306(w, h, twi, rst_pin, clkDuring, clkAfter)
  {
    buffer = SSD1306_SFB;

  }
  ~Adafruit_SSD1306_SB(void)
  {
    buffer = NULL;
  }
};

Adafruit_SSD1306_SB oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
#endif
// MAX31856 thermocouple interface
Adafruit_MAX31856 thermocouple = Adafruit_MAX31856(thermocoupleCSPin);

switch_t readSwitch(void);

void setup()
{
  // Check current selected reflow profile
  unsigned char value = EEPROM.read(PROFILE_TYPE_ADDRESS);
  if ((value == 0) || (value == 1) || (value == 2))
  {
    // Valid reflow profile value
    reflowProfile = static_cast<reflowProfile_t>(value);
  }
  else
  {
    // Default to lead-free profile
    EEPROM.write(PROFILE_TYPE_ADDRESS, 0);
    reflowProfile = REFLOW_PROFILE_LEADFREE;
  }

  // SSR pin initialization to ensure reflow oven is off
  digitalWrite(ssrPin, LOW);
  pinMode(ssrPin, OUTPUT);

  // Buzzer pin initialization to ensure annoying buzzer is off
  digitalWrite(buzzerPin, LOW);
  pinMode(buzzerPin, OUTPUT);

  // LED pins initialization and turn on upon start-up (active high)
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  // Initialize thermocouple interface
  thermocouple.begin();
  thermocouple.setThermocoupleType(MAX31856_TCTYPE_K);

  // Start-up splash
  digitalWrite(buzzerPin, HIGH);
#if VERSION == 1
  lcd.begin(8, 2);
  lcd.createChar(0, degree);
  lcd.clear();
  lcd.print(F(" Tiny  "));
  lcd.setCursor(0, 1);
  lcd.print(F(" Reflow "));
#elif VERSION == 2
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.display();
#endif
  digitalWrite(buzzerPin, LOW);
  delay(1000);
#if VERSION == 1
  lcd.clear();
  lcd.print(F(" v1.00  "));
  lcd.setCursor(0, 1);
  lcd.print(F("26-07-17"));
  delay(2000);
  lcd.clear();
#elif VERSION == 2
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println(F("     Tiny Reflow"));
  oled.println(F("     Controller"));
  oled.println();
  oled.println(F("       v2.10"));
  oled.println();
  oled.println(F("      01-05-20"));
  oled.display();
  delay(2000);
  oled.clearDisplay();
#endif

  // Serial communication at 115200 bps
  Serial.begin(115200);

  // Turn off LED (active high)
  digitalWrite(ledPin, LOW);
  // Set window size
  windowSize = 2000;
  // Initialize time keeping variable
  nextCheck = millis();
  // Initialize thermocouple reading variable
  nextRead = millis();
  // Initialize LCD update timer
  updateLcd = millis();
}

void loop()
{
  // Current time
  unsigned long now;

  // Time to read thermocouple?
  if (millis() > nextRead)
  {
    // Read thermocouple next sampling period
    nextRead += SENSOR_SAMPLING_TIME;
    // Read current temperature
    input = thermocouple.readThermocoupleTemperature();
    // Check for thermocouple fault
    fault = thermocouple.readFault();

    // If any thermocouple fault is detected
    if ((fault & MAX31856_FAULT_CJRANGE) ||
        (fault & MAX31856_FAULT_TCRANGE) ||
        (fault & MAX31856_FAULT_CJHIGH) ||
        (fault & MAX31856_FAULT_CJLOW) ||
        (fault & MAX31856_FAULT_TCHIGH) ||
        (fault & MAX31856_FAULT_TCLOW) ||
        (fault & MAX31856_FAULT_OVUV) ||
        (fault & MAX31856_FAULT_OPEN))
    {
      // Illegal operation
      reflowState = REFLOW_STATE_ERROR;
      reflowStatus = REFLOW_STATUS_OFF;
      Serial.println(F("Error"));
    }
  }

  if (millis() > nextCheck)
  {
    // Check input in the next seconds
    nextCheck += SENSOR_SAMPLING_TIME;
    // If reflow process is on going
    if (reflowStatus == REFLOW_STATUS_ON)
    {
      // Toggle red LED as system heart beat
      digitalWrite(ledPin, !(digitalRead(ledPin)));
      // Increase seconds timer for reflow curve plot
      timerSeconds++;
      // Send temperature and time stamp to serial
      Serial.print(timerSeconds);
      Serial.print(F(","));
      Serial.print(setpoint);
      Serial.print(F(","));
      Serial.print(input);
      Serial.print(F(","));
      Serial.println(output);
    }
    else
    {
      // Turn off red LED
      digitalWrite(ledPin, LOW);
    }
  }

  if (millis() > updateLcd)
  {
    char txtBuffer[8];
    strcpy_P(txtBuffer, (char *)pgm_read_word(&(lcdMessagesReflowStatus[reflowState])));
    // Update LCD in the next 100 ms
    updateLcd += UPDATE_RATE;
#if VERSION == 1
    // Clear LCD
    lcd.clear();
    // Print current system state
    lcd.print(txtBuffer);
    lcd.setCursor(6, 0);
    if (reflowProfile == REFLOW_PROFILE_LEADFREE)
    {
      lcd.print(F("LF"));
    }
    else if (reflowProfile == REFLOW_PROFILE_LEADED)
    {
      lcd.print(F("PB"));
    }
    else
    {
      lcd.print(F("BK"));
    }
    lcd.setCursor(0, 1);
    
    // If currently in error state
    if (reflowState == REFLOW_STATE_ERROR)
    {
      // Thermocouple error (open, shorted)
      lcd.print(F("TC Error"));
    }
    else
    {
      // Display current temperature
      lcd.print(input);
#if ARDUINO >= 100
      // Display degree Celsius symbol
      lcd.write((uint8_t)0);
#else
      // Display degree Celsius symbol
      lcd.print(0, BYTE);
#endif
      lcd.print("C ");
    }
#elif VERSION == 2
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(0, 0);
    oled.print(txtBuffer);
    oled.setTextSize(1);
    oled.setCursor(115, 0);

    if (reflowProfile == REFLOW_PROFILE_LEADFREE)
    {
      oled.print(F("LF"));
    }
    else if (reflowProfile == REFLOW_PROFILE_LEADED)
    {
      oled.print(F("PB"));
    }
    else
    {
      oled.print(F("BK"));
    }
    
    // Temperature markers
    oled.setCursor(0, 19);
    oled.print(F("250"));
    oled.setCursor(0, 36);
    oled.print(F("150"));
    oled.setCursor(0, 54);
    oled.print(F("50"));
    // Draw temperature and time axis
    oled.drawLine(18, 18, 18, 63, WHITE); //left vertical line
    oled.drawLine(18, 19, 20, 19, WHITE); //250 tick
    oled.drawLine(18, 36, 20, 36, WHITE); //150 tick
    oled.drawLine(18, 54, 20, 54, WHITE); //50 tick
    oled.drawLine(18, 63, 127, 63, WHITE); //bottom horizontal line
    //time markers, scroll with plot
    oled.drawLine(38-xScrollOffset, 63, 38-xScrollOffset, 61, WHITE);
    oled.drawLine(58-xScrollOffset, 63, 58-xScrollOffset, 61, WHITE);
    oled.drawLine(78-xScrollOffset, 63, 78-xScrollOffset, 61, WHITE);
    oled.drawLine(98-xScrollOffset, 63, 98-xScrollOffset, 61, WHITE);
    oled.drawLine(118-xScrollOffset, 63, 118-xScrollOffset, 61, WHITE);
    if (xScrollOffset>10)
      oled.drawLine(138-xScrollOffset, 63, 138-xScrollOffset, 61, WHITE);

    // If currently in error state
    if (reflowState == REFLOW_STATE_ERROR)
    {
      oled.setCursor(80, 9);
      oled.print(F("TC Error"));
    }
    else
    {
      // Right align temperature reading
      if (input < 10) oled.setCursor(91, 9);
      else if (input < 100) oled.setCursor(85,9);
      else oled.setCursor(80, 9);
      // Display current temperature
      oled.print(input);
      oled.print((char)247);
      oled.print(F("C"));
    }
    
    if (reflowStatus == REFLOW_STATUS_ON)
    {
      // We are updating the display faster than sensor reading
      if (timerSeconds > timerUpdate)
      {
        // Store temperature reading every 3 s
        if ((timerSeconds % 3) == 0)
        {
          timerUpdate = timerSeconds;
          unsigned char averageReading = map(input, 0, 250, 63, 19);
          if (xCnt < (SCREEN_WIDTH - X_AXIS_START)) //haven't filled entire screen yet
          {
            temperature[xCnt++] = averageReading;
          }
          else //screen full, scroll graph
          {
            temperature[xHead++] = averageReading;
            if (xHead == (SCREEN_WIDTH - X_AXIS_START))
              xHead=0;
            xScrollOffset++;
            if (xScrollOffset>19)
              xScrollOffset=0;
          }
        }
      }
    }
    
    unsigned char timeAxis, tElem;
    tElem=xHead;
    for (timeAxis = 0; timeAxis < xCnt; timeAxis++)
    {
      oled.drawPixel(timeAxis + X_AXIS_START, temperature[tElem++], WHITE);
      if (tElem==(SCREEN_WIDTH - X_AXIS_START))
        tElem=0;
    }
    
    // Update screen
    oled.display();
#endif
  }

  // Reflow oven controller state machine
  switch (reflowState)
  {
    case REFLOW_STATE_IDLE:
      // If oven temperature is still above room temperature
      if (input >= TEMPERATURE_ROOM)
      {
        reflowState = REFLOW_STATE_TOO_HOT;
      }
      else
      {
        // If switch is pressed to start reflow process
        if (switchStatus == SWITCH_1)
        {
          // Send header for CSV file
          Serial.println(F("Time,Setpoint,Input,Output"));
          // Intialize seconds timer for serial debug information
          timerSeconds = 0;
          
          #if VERSION == 2
          // Initialize reflow plot update timer
          timerUpdate = 0;
          xHead = 0;
          xCnt = 0;
          xScrollOffset=0;
          #endif
          
          // Initialize PID control window starting time
          windowStartTime = millis();

          // Either enter Bake or continue with chosen reflow profile
          if (reflowProfile == REFLOW_PROFILE_BAKE)
          {
            setpoint = TEMPERATURE_BAKE;
            // Tell the PID to range between 0 and the full window size
            reflowOvenPID.SetOutputLimits(0, windowSize);
            reflowOvenPID.SetSampleTime(PID_SAMPLE_TIME);
            // Turn the PID on
            reflowOvenPID.SetMode(AUTOMATIC);
            // Proceed to bake stage
            reflowState = REFLOW_STATE_BAKE;
          }
          else
          {
            // Ramp up to minimum soaking temperature
            setpoint = TEMPERATURE_SOAK_MIN;
            // Load profile specific constant
            if (reflowProfile == REFLOW_PROFILE_LEADFREE)
            {
              soakTemperatureMax = TEMPERATURE_SOAK_MAX_LF;
              reflowTemperatureMax = TEMPERATURE_REFLOW_MAX_LF;
              soakMicroPeriod = SOAK_MICRO_PERIOD_LF;
            }
            else
            {
              soakTemperatureMax = TEMPERATURE_SOAK_MAX_PB;
              reflowTemperatureMax = TEMPERATURE_REFLOW_MAX_PB;
              soakMicroPeriod = SOAK_MICRO_PERIOD_PB;
            }
            // Tell the PID to range between 0 and the full window size
            reflowOvenPID.SetOutputLimits(0, windowSize);
            reflowOvenPID.SetSampleTime(PID_SAMPLE_TIME);
            // Turn the PID on
            reflowOvenPID.SetMode(AUTOMATIC);
            // Proceed to preheat stage
            reflowState = REFLOW_STATE_PREHEAT;
          }
        }
      }
      break;

    case REFLOW_STATE_PREHEAT:
      reflowStatus = REFLOW_STATUS_ON;
      // If minimum soak temperature is achieve
      if (input >= TEMPERATURE_SOAK_MIN)
      {
        // Chop soaking period into smaller sub-period
        timerSoak = millis() + soakMicroPeriod;
        // Set less agressive PID parameters for soaking ramp
        reflowOvenPID.SetTunings(PID_KP_SOAK, PID_KI_SOAK, PID_KD_SOAK);
        // Ramp up to first section of soaking temperature
        setpoint = TEMPERATURE_SOAK_MIN + SOAK_TEMPERATURE_STEP;
        // Proceed to soaking state
        reflowState = REFLOW_STATE_SOAK;
      }
      break;

    case REFLOW_STATE_SOAK:
      // If micro soak temperature is achieved
      if (millis() > timerSoak)
      {
        timerSoak = millis() + soakMicroPeriod;
        // Increment micro setpoint
        setpoint += SOAK_TEMPERATURE_STEP;
        if (setpoint > soakTemperatureMax)
        {
          // Set agressive PID parameters for reflow ramp
          reflowOvenPID.SetTunings(PID_KP_REFLOW, PID_KI_REFLOW, PID_KD_REFLOW);
          // Ramp up to first section of soaking temperature
          setpoint = reflowTemperatureMax;
          // Proceed to reflowing state
          reflowState = REFLOW_STATE_REFLOW;
        }
      }
      break;

    case REFLOW_STATE_REFLOW:
      // We need to avoid hovering at peak temperature for too long
      // Crude method that works like a charm and safe for the components
      if (input >= (reflowTemperatureMax - 5))
      {
        // Set PID parameters for cooling ramp
        reflowOvenPID.SetTunings(PID_KP_REFLOW, PID_KI_REFLOW, PID_KD_REFLOW);
        // Ramp down to minimum cooling temperature
        setpoint = TEMPERATURE_COOL_MIN;
        // Proceed to cooling state
        reflowState = REFLOW_STATE_COOL;
      }
      break;

    case REFLOW_STATE_COOL:
      // If minimum cool temperature is achieve
      if (input <= TEMPERATURE_COOL_MIN)
      {
        // Retrieve current time for buzzer usage
        buzzerPeriod = millis() + 1000;
        // Turn on buzzer to indicate completion
        digitalWrite(buzzerPin, HIGH);
        // Turn off reflow process
        reflowStatus = REFLOW_STATUS_OFF;
        // Proceed to reflow Completion state
        reflowState = REFLOW_STATE_COMPLETE;
      }
      break;

    case REFLOW_STATE_COMPLETE:
      if (millis() > buzzerPeriod)
      {
        // Turn off buzzer
        digitalWrite(buzzerPin, LOW);
        // Reflow process ended
        reflowState = REFLOW_STATE_IDLE;
      }
      break;

    case REFLOW_STATE_TOO_HOT:
      // If oven temperature drops below room temperature
      if (input < TEMPERATURE_ROOM)
      {
        // Ready to reflow
        reflowState = REFLOW_STATE_IDLE;
      }
      break;

    case REFLOW_STATE_ERROR:
      // Check for thermocouple fault
      fault = thermocouple.readFault();

      // If thermocouple problem is still present
      if ((fault & MAX31856_FAULT_CJRANGE) ||
          (fault & MAX31856_FAULT_TCRANGE) ||
          (fault & MAX31856_FAULT_CJHIGH) ||
          (fault & MAX31856_FAULT_CJLOW) ||
          (fault & MAX31856_FAULT_TCHIGH) ||
          (fault & MAX31856_FAULT_TCLOW) ||
          (fault & MAX31856_FAULT_OVUV) ||
          (fault & MAX31856_FAULT_OPEN))
      {
        // Wait until thermocouple wire is connected
        reflowState = REFLOW_STATE_ERROR;
      }
      else
      {
        // Clear to perform reflow process
        reflowState = REFLOW_STATE_IDLE;
      }
      break;

   case REFLOW_STATE_BAKE:
    reflowStatus = REFLOW_STATUS_ON;
    reflowOvenPID.SetTunings(PID_KP_BAKE, PID_KI_BAKE, PID_KD_BAKE);
    break;
  }

  // If switch 1 is pressed
  if (switchStatus == SWITCH_1)
  {
    // If currently reflow process is on going
    if (reflowStatus == REFLOW_STATUS_ON)
    {
      // Button press is for cancelling
      // Turn off reflow process
      reflowStatus = REFLOW_STATUS_OFF;
      // Reinitialize state machine
      reflowState = REFLOW_STATE_IDLE;
    }
  }
  // Switch 2 is pressed
  else if (switchStatus == SWITCH_2)
  {
    // Only can switch reflow profile during idle
    if (reflowState == REFLOW_STATE_IDLE)
    {
      // Currently using lead-free reflow profile
      if (reflowProfile == REFLOW_PROFILE_LEADFREE)
      {
        // Switch to leaded reflow profile
        reflowProfile = REFLOW_PROFILE_LEADED;
        EEPROM.write(PROFILE_TYPE_ADDRESS, 1);
      }
      // Currently using leaded reflow profile
      else if (reflowProfile == REFLOW_PROFILE_LEADED)
      {
        // Switch to bake profile
        reflowProfile = REFLOW_PROFILE_BAKE;
        EEPROM.write(PROFILE_TYPE_ADDRESS, 2);
      }
      else
      {
        // Switch to lead-free profile
        reflowProfile = REFLOW_PROFILE_LEADFREE;
        EEPROM.write(PROFILE_TYPE_ADDRESS, 0);
      }
    }
  }
  // Switch status has been read
  switchStatus = SWITCH_NONE;

  // Simple switch debounce state machine (analog switch)
  switch (debounceState)
  {
    case DEBOUNCE_STATE_IDLE:
      // No valid switch press
      switchStatus = SWITCH_NONE;

      switchValue = readSwitch();

      // If either switch is pressed
      if (switchValue != SWITCH_NONE)
      {
        // Keep track of the pressed switch
        switchMask = switchValue;
        // Intialize debounce counter
        lastDebounceTime = millis();
        // Proceed to check validity of button press
        debounceState = DEBOUNCE_STATE_CHECK;
      }
      break;

    case DEBOUNCE_STATE_CHECK:
      switchValue = readSwitch();
      if (switchValue == switchMask)
      {
        // If minimum debounce period is completed
        if ((millis() - lastDebounceTime) > DEBOUNCE_PERIOD_MIN)
        {
          // Valid switch press
          switchStatus = switchMask;
          // Proceed to wait for button release
          debounceState = DEBOUNCE_STATE_RELEASE;
        }
      }
      // False trigger
      else
      {
        // Reinitialize button debounce state machine
        debounceState = DEBOUNCE_STATE_IDLE;
      }
      break;

    case DEBOUNCE_STATE_RELEASE:
      switchValue = readSwitch();
      if (switchValue == SWITCH_NONE)
      {
        // Reinitialize button debounce state machine
        debounceState = DEBOUNCE_STATE_IDLE;
      }
      break;
  }

  // PID computation and SSR control
  if (reflowStatus == REFLOW_STATUS_ON)
  {
    now = millis();

    reflowOvenPID.Compute();

    if ((now - windowStartTime) > windowSize)
    {
      // Time to shift the Relay Window
      windowStartTime += windowSize;
    }
    if (output > (now - windowStartTime)) digitalWrite(ssrPin, HIGH);
    else digitalWrite(ssrPin, LOW);
  }
  // Reflow oven process is off, ensure oven is off
  else
  {
    digitalWrite(ssrPin, LOW);
  }
}

switch_t readSwitch(void)
{
#if VERSION == 1
  int switchAdcValue = 0;
  // Analog multiplexing switch
  switchAdcValue = analogRead(switchPin);

  // Add some allowance (+10 ADC step) as ADC reading might be off a little
  // due to 3V3 deviation and also resistor value tolerance
  if (switchAdcValue >= 1000) return SWITCH_NONE;
  if (switchAdcValue <= 10) return SWITCH_1;
  if (switchAdcValue <= 522) return SWITCH_2;

#elif VERSION == 2
  // Switch connected directly to individual separate pins
  if (digitalRead(switchStartStopPin) == LOW) return SWITCH_1;
  if (digitalRead(switchLfPbPin) == LOW) return SWITCH_2;

#endif

  return SWITCH_NONE;
}
