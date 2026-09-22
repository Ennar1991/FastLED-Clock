#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
WiFiManager wm;

#include <Timezone.h>  // https://github.com/JChristensen/Timezone
#include <TimeLib.h>   // https://github.com/PaulStoffregen/Time

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // Central European Summer Time
TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    // Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr;  // pointer to the time change rule, use to get TZ abbrev
#include <WiFi.h>
#include <WiFiUdp.h>
#include <mDNS.h>
#include <ArduinoOTA.h>

static const char ntpServerName[] = "1.pool.ntp.org";

uint8_t brightness = 127;  //Global LED brightness



//Needed for NTP
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

#include <FastLED.h>  //3.9.10, newer versions (3.10.4+) break RGBW among other bugs
#define FASTLED_RGBW
//FASTLED_USING_NAMESPACE
#define FRAMES_PER_SECOND 30
// How many leds_right in your strip?
#define NUM_LEDS_RIGHT ((2 * 4 * 7) + 1)
#define NUM_LEDS_LEFT ((2 * 4 * 7) + 1)

uint8_t gHue = 0;  // rotating "base color" used by many of the patterns

bool isRgbMode = false;
bool isFastRgbMode = false;

// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define DATA_PIN_RIGHT 3
#define DATA_PIN_LEFT 4

uint32_t content_clock[4] = {};  //The buffer that determines the displayed content

//Define timer values and control pins

#define DISPLAY_MODE_TOGGLE_PIN 7
#define START_TIMER1_PIN 8
#define START_TIMER2_PIN 9
#define STARTVALUE_TIMER1_SECONDS (15 * 60)
#define STARTVALUE_TIMER2_SECONDS (20 * 60)
#define BLINKING_REMAININGSECONDS 60
#define COLOR_CLOCK CRGB::Teal
#define COLOR_TIMER CRGB(255, 16, 16)
#define COLOR_TIMER_DIM CRGB(64, 4, 4)

#define NTP_INTERVAL (3600 * 12) /*Seconds until re-sync*/
uint8_t btn1_hold = 0;
uint8_t btn2_hold = 0;


int32_t timerRemainingSeconds = 0;
bool isTimerRunning = false;
enum displaymodes { displayClock,
                    displayTimer };
displaymodes displayMode = displayClock;
int32_t displayBlankTimer = 0;


//Define glyphs
const uint8_t glyphs_left[][7] = {
//  g  f  a  b  c  d  e          
  { 0, 1, 1, 1, 1, 1, 1 }, //0
  { 0, 0, 0, 1, 1, 0, 0 }, //1
  { 1, 0, 1, 1, 0, 1, 1 }, //2
  { 1, 0, 1, 1, 1, 1, 0 }, //3
  { 1, 1, 0, 1, 1, 0, 0 }, //4
  { 1, 1, 1, 0, 1, 1, 0 }, //5
  { 1, 1, 1, 0, 1, 1, 1 }, //6
  { 0, 0, 1, 1, 1, 0, 0 }, //7
  { 1, 1, 1, 1, 1, 1, 1 }, //8
  { 1, 1, 1, 1, 1, 1, 0 }, //9
  { 0, 0, 0, 0, 0, 0, 0 }, //Space
  { 1, 1, 1, 1, 1, 0, 1 }, //A
  { 1, 1, 1, 1, 0, 0, 1 }, //P
  { 1, 1, 0, 0, 0, 1, 1 }  //t
};

const uint8_t glyphs_right[][7] = {
//  g  b  a  f  e  d  c  
  { 0, 1, 1, 1, 1, 1, 1 }, //0
  { 0, 1, 0, 0, 0, 0, 1 }, //1
  { 1, 0, 1, 1, 0, 1, 1 }, //2
  { 1, 1, 1, 0, 0, 1, 1 }, //3
  { 1, 1, 0, 0, 1, 0, 1 }, //4
  { 1, 1, 1, 0, 1, 1, 0 }, //5
  { 1, 1, 1, 1, 1, 1, 0 }, //6
  { 0, 1, 0, 0, 0, 1, 1 }, //7
  { 1, 1, 1, 1, 1, 1, 1 }, //8
  { 1, 1, 1, 0, 1, 1, 1 }, //9
  { 0, 0, 0, 0, 0, 0, 0 }, //Space
  { 1, 1, 1, 1, 1, 0, 1 }, //A
  
};

// Define the array of leds_right
CRGB leds_right[NUM_LEDS_RIGHT];
CRGB leds_left[NUM_LEDS_LEFT];
uint32_t progress_anim = 0;

bool isOnline = false;
bool isSynced = false;
void setup() {

  //Serial.begin(115200);
  pinMode(DISPLAY_MODE_TOGGLE_PIN, INPUT_PULLUP);
  pinMode(START_TIMER1_PIN, INPUT_PULLUP);
  pinMode(START_TIMER2_PIN, INPUT_PULLUP);
  //if (!digitalRead(START_TIMER1_PIN)) isRgbMode = true;
  //if (!digitalRead(START_TIMER2_PIN)) isFastRgbMode = true;

  FastLED.addLeds<SK6812, DATA_PIN_RIGHT, GRB>(leds_right, NUM_LEDS_RIGHT).setRgbw(RgbwDefault());
  FastLED.addLeds<SK6812, DATA_PIN_LEFT, GRB>(leds_left, NUM_LEDS_LEFT).setRgbw(RgbwDefault());  // GRB ordering is typical
  FastLED.setBrightness(brightness);
  leds_left[0] = CRGB::White;
  leds_right[0] = CRGB::White;

  FastLED.show();

  WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(5 * 60);  //5 minutes
                                      //automatically connect using saved credentials if they exist
                                      //If connection fails it starts an access point with the specified name
  introAnim();
  if (wm.autoConnect("FastLED-Clock-AP")) {
    //Serial.println("connected...yeey :)");
    isOnline = true;
    isSynced = false;
  } else {
    //Serial.println("Configportal running");
    isOnline = false;
    isSynced = false;
  }

  //If WiFi is available, set syncing parameters
  if (isOnline) {
    Udp.begin(localPort);
    ////Serial.print("Local port: ");
    ////Serial.println(Udp.localPort());
    //Serial.println("waiting for sync");
    setSyncProvider(getNtpTime);
    setSyncInterval(NTP_INTERVAL);
    //Serial.print(String(hour()) + ":" + String(minute()) + ":" + String(second()));
  }
  // Port defaults to 8266
  ArduinoOTA.setPort(3232);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("FastLED-Clock");

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    //Serial.println("Start updating " + type);
    fill_solid(leds_left, NUM_LEDS_LEFT, CRGB::Teal);
    fill_solid(leds_right, NUM_LEDS_RIGHT, CRGB::Teal);
    FastLED.show();
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    uint32_t percent = progress / (total / 100);
    fill_solid(leds_left, NUM_LEDS_LEFT, CRGB::Teal);
    fill_solid(leds_right, NUM_LEDS_RIGHT, CRGB::Teal);
    content_clock[0] = 10;
    content_clock[1] = { (percent / 100) % 10 };
    content_clock[2] = { (percent / 10) % 10 };
    content_clock[3] = { (percent % 10) };
    UpdateLedBuffers();
    leds_left[0] = percent % 2 ? CRGB::Black : CRGB::White;
    leds_right[0] = !(percent % 2) ? CRGB::Black : CRGB::White;

    FastLED.show();
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void loop() {
  wm.process();
  ArduinoOTA.handle();

  if (isTimerRunning) {
    if (displayBlankTimer < 3) {
      fill_solid(leds_left, NUM_LEDS_LEFT, COLOR_TIMER);
      fill_solid(leds_right, NUM_LEDS_RIGHT, COLOR_TIMER);
    } else {
      fill_solid(leds_left, NUM_LEDS_LEFT, COLOR_TIMER_DIM);
      fill_solid(leds_right, NUM_LEDS_RIGHT, COLOR_TIMER_DIM);
    }       //displayBlankTimer < 5
  } else {  //isTimerRunning
    if (isRgbMode) {
      fill_rainbow(leds_left, NUM_LEDS_LEFT, gHue, 10);
      fill_rainbow(leds_right, NUM_LEDS_RIGHT, gHue, 10);
    } else {
      fill_solid(leds_left, NUM_LEDS_LEFT, COLOR_CLOCK);
      fill_solid(leds_right, NUM_LEDS_RIGHT, COLOR_CLOCK);
    }
  }
  // eight colored dots, weaving in and out of sync with each other
  /*
    fadeToBlackBy( leds_left, NUM_LEDS_LEFT, 20);
    fadeToBlackBy( leds_right, NUM_LEDS_LEFT, 20);
    uint8_t dothue = 0;
    for( int i = 0; i < 8; i++) {
    leds_left[beatsin16( i+7, 0, NUM_LEDS_LEFT-1 )] |= CHSV(dothue, 200, 255);
    leds_right[beatsin16( i+7, 0, NUM_LEDS_RIGHT-1 )] |= CHSV(dothue, 200, 255);

    dothue += 32;
    }
  */
  //Blank out unlit segments for numbers
  UpdateLedBuffers();
  if (millis() % 1000 > 500) {
    leds_left[0] = CRGB::Black;
    leds_right[0] = CRGB::Black;
  }
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // do some periodic updates
  EVERY_N_MILLISECONDS(100) {

    if (digitalRead(DISPLAY_MODE_TOGGLE_PIN) == HIGH) {
      displayMode = displayClock;
    } else {
      displayMode = displayTimer;
    }

    if (digitalRead(START_TIMER1_PIN) == LOW) {
      btn1_hold++;
      if (btn1_hold == 5) timerRemainingSeconds = STARTVALUE_TIMER1_SECONDS;
      if ((btn1_hold == 12)) {
        if (brightness > 20) brightness -= 5;
        btn1_hold = 11;
      }
    } else {
      btn1_hold = 0;
    }

    if (digitalRead(START_TIMER2_PIN) == LOW) {
      btn2_hold++;

      if (btn2_hold == 5) timerRemainingSeconds = STARTVALUE_TIMER2_SECONDS;
      if ((btn2_hold == 12)) {
        if (brightness < 250) brightness += 5;
        btn2_hold = 11;
      }
    } else {
      btn2_hold = 0;
    }

    if (wm.getConfigPortalActive()) {
      content_clock[0] = { 11 };  //A
      content_clock[1] = { 12 };  //P
      content_clock[2] = { 10 };  //Space
      content_clock[3] = { 10 };  //Space
    } else if (displayMode == displayClock) {
      time_t utc = now();
      time_t local = CE.toLocal(utc, &tcr);
      uint8_t myHour = hour(local);
      uint8_t myMinute = minute(local);
      uint8_t mySecond = second(local);
      content_clock[0] = { (myHour > 9 ? myHour / 10 : 10) };  //Use first digit only when hour is >10
      content_clock[1] = { (myHour % 10) };
      content_clock[2] = { (myMinute / 10) };
      content_clock[3] = { (myMinute % 10) };
    } else if (displayMode == displayTimer) {
      uint8_t myMinute = timerRemainingSeconds / 60;
      uint8_t mySecond = timerRemainingSeconds % 60;
      content_clock[0] = { (myMinute > 9 ? myMinute / 10 : 10) };  //Use first digit only when minute is >10
      content_clock[1] = { (myMinute % 10) };
      content_clock[2] = { (mySecond / 10) };
      content_clock[3] = { (mySecond % 10) };
    }

    //Helper timing variable to blink display
    if (isTimerRunning && timerRemainingSeconds <= BLINKING_REMAININGSECONDS) {
      displayBlankTimer++;
      if (displayBlankTimer >= 5) displayBlankTimer = 0;
    } else {
      displayBlankTimer = 0;  //Do not blank when no timer is active
    }
    FastLED.setBrightness(brightness);
  }

  if (isFastRgbMode) {
    gHue += 3;
  } else {
    EVERY_N_MILLISECONDS(30) {
      gHue++;
    }
  }

  EVERY_N_SECONDS(1) {
    //Decrease timer value
    if (timerRemainingSeconds > 0) {
      isTimerRunning = true;
      timerRemainingSeconds--;
    } else {
      isTimerRunning = false;
    }

    //Check WiFi Logic
    if (isOnline) {
      if (timeStatus() == timeSet) {
        WiFi.mode(WIFI_OFF);
      } else if (timeStatus() == timeNeedsSync) {
        WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
        (wm.autoConnect("FastLED-Clock-AP"));
        setSyncProvider(getNtpTime);
        setSyncInterval(NTP_INTERVAL);
      }
    }
  }
}

void UpdateLedBuffers() {
  for (int i = 0; i < 7; i++) {  //Glyph bits
    for (int k = 0; k < 4; k++) {
      uint32_t j = (i * 4) + k;
      if (!(glyphs_left[content_clock[0]][i])) leds_left[j + 29] = CRGB::Black;    //left digit of left group
      if (!(glyphs_left[content_clock[1]][i])) leds_left[j + 1] = CRGB::Black;     //right digit of left group
      if (!(glyphs_right[content_clock[2]][i])) leds_right[j + 1] = CRGB::Black;   //left digit of right group
      if (!(glyphs_right[content_clock[3]][i])) leds_right[j + 29] = CRGB::Black;  //right digit of right group
    }
  }
}



/*-------- NTP code  taken from TimeNTP_ESP8266WiFi ----------*/

const int NTP_PACKET_SIZE = 48;      // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets

time_t getNtpTime() {

  IPAddress ntpServerIP;  // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
  //Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  //Serial.print(ntpServerName);
  //Serial.print(": ");
  //Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      //Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      isSynced = true;

      return secsSince1900 - 2208988800UL;
    }
  }
  //Serial.println("No NTP Response :-(");
  isSynced = false;

  return 0;  // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void introAnim() {
  for (int i = 0; i < NUM_LEDS_RIGHT * 5; i++) {
    leds_left[i / 5] = CHSV(i, 255, 255);
    leds_right[i / 5] = CHSV(i, 255, 255);
    fadeToBlackBy(leds_left, NUM_LEDS_LEFT, 1);
    fadeToBlackBy(leds_right, NUM_LEDS_LEFT, 1);
    FastLED.show();
    FastLED.delay(1);
  }
  for (int i = 0; i < (int)(NUM_LEDS_RIGHT * 2.5); i++) {
    fadeToBlackBy(leds_left, NUM_LEDS_LEFT, 1);
    fadeToBlackBy(leds_right, NUM_LEDS_LEFT, 1);
    FastLED.show();
    FastLED.delay(1);
  }
  for (int i = (NUM_LEDS_RIGHT * 5) - 1; i >= 0; i--) {
    leds_left[i / 5] = CHSV(i, 255, 255);
    leds_right[i / 5] = CHSV(i, 255, 255);
    //fadeToBlackBy(leds_left, NUM_LEDS_LEFT, 1);
    //fadeToBlackBy(leds_right, NUM_LEDS_LEFT, 1);
    FastLED.show();
    FastLED.delay(1);
  }
}

//WiFi Manager callback notifying us of the need to save config
void saveConfigCallback() {
  isOnline = true;
  setSyncProvider(getNtpTime);
  setSyncInterval(NTP_INTERVAL);
}