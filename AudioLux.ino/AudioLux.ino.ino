#include <FastLED.h>
#include "WebServer.h"
#include "arduinoFFT.h"
#include "LITTLEFS.h"


/**********************************************************************************************/
/* Code imported from Nanolux                                                                 */

/*
        FastLED
*/
#define MAX_LEDS            60              // Number of leds in strip.
#define DATA_PIN            15              // No hardware SPI pins defined for the ESP32 yet.
#define CLK_PIN             14              // Use bitbanged output.
#define LED_TYPE            SK9822          // Define LED protocol.
#define COLOR_ORDER         BGR             // Define color color order.
#define MAX_BRIGHTNESS      255
#define FRAMES_PER_SECOND   120             // Not used

// LED strip configuration. To be consumed by FastLED.
// serial definitions for changing parameters, sent as 8 bit chars
#define SER_LED_OFF     '2'
#define SER_NEXT_PTRN   '1'
#define SER_PREV_PTRN   '0'
#define SER_60_LEDS     'A'   // strip has 60 leds
#define SER_144_LEDS    'B'   // strip has 144 leds
#define SER_DELTA       'C'   // use max delta frequency analysis
#define SER_NORMAL      'D'   // stop using max delta

CRGB leds[MAX_LEDS];        // Buffer (front)



void initialize_serial();
void initialize_led_strip();
void show_pattern();
void freq_hue_trail();
void blank();

/**********************************************************************************************/


/*
 * FFT related values
 */
#define SAMPLES             512     // Must be a power of 2
#define SAMPLING_FREQUENCY  40000   // May need to be less than 10000 due to ADC

unsigned int sampling_period_us = round(1000000 / SAMPLING_FREQUENCY);
unsigned long capture_start;

double lReal[SAMPLES];      // left channel
double lImag[SAMPLES];

double rReal[SAMPLES];      // right channel
double rImag[SAMPLES];

double lpeak = 0.;          //  left channel peak frequency
double rpeak = 0.;          //  right channel peak frequency

arduinoFFT FFT = arduinoFFT();


/*
 * LED Display parameters
 */
// HSV values. We only manipulate hue.
volatile uint8_t fHue = HUE_RED;
const uint8_t LED_BRIGHTNESS = 190;
const uint8_t LED_SATURATION = 255;


/*
 * Timer Interrupt to update LEDs
 */
// Parameters. See Espressif Docs.
const uint8_t PRESCALER = 80;
const uint8_t TIMER0 = 0;
const bool COUNT_UP = true;
const bool TRIGGER_ON_HIGH = true;

// Interval: 60ms (in us).
const uint64_t TIMER_INTERVAL_US = 200000;
const bool REARM_ON_TRIGGER = true;

hw_timer_t* timer = NULL;

void IRAM_ATTR onTimer(){
    show_pattern();
}

/*
 * Web Server
 */
WiFiWebServer webServer(80);

void initialize_web_server();

// These are the "route" handlers of the API.
void handle_root();
void handle_red_led();
void handle_blue_led();
void handle_green_led();

/*
 * File system
 */
void initialize_file_system();
void listDir(fs::FS &fs, const char * dirname, uint8_t levels);

/*
 * Main Arduino Section
 */
void setup() {
    initialize_serial();
    initialize_file_system();
    initialize_led_strip();
    initialize_web_server();
    initialize_timer();
}

void loop() {
    audio_analysis();
    webServer.handleClient();
}


/*
 * Audio Processing
 */
void initialize_timer() {
    timer = timerBegin(TIMER0, PRESCALER, COUNT_UP);
    timerAttachInterrupt(timer, &onTimer, TRIGGER_ON_HIGH);

    timerAlarmWrite(timer, TIMER_INTERVAL_US, REARM_ON_TRIGGER);
    timerAlarmEnable(timer);
}

void audio_analysis() {
  /* Sampling */
  for(int i=0; i<SAMPLES; i++) {
    capture_start = micros();

    lReal[i] = analogRead(A0);
    lImag[i] = 0;

    rReal[i] = analogRead(A1);
    rImag[i] = 0;

    while(micros() < (capture_start + sampling_period_us)) {}
  }

  /* FFT */
  FFT.Windowing(lReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(lReal, lImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(lReal, lImag, SAMPLES);
  lpeak = FFT.MajorPeak(lReal, SAMPLES, SAMPLING_FREQUENCY);

  FFT.Windowing(rReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(rReal, rImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(rReal, rImag, SAMPLES);
  rpeak = FFT.MajorPeak(rReal, SAMPLES, SAMPLING_FREQUENCY);

//  Serial.println("Audio Processed.");
}


/*
 * File system
 */
void initialize_file_system() {
    Serial.println(F("Initializing FS..."));
    if (LITTLEFS.begin()) {
        Serial.println(F("done."));
    } else {
        Serial.println(F("fail."));
    }

    listDir(LITTLEFS, "/", 2);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void register_web_paths(fs::FS &fs, const char * dirname, uint8_t levels) {
    File root = fs.open(dirname);
    if (!root) {
        Serial.println("- failed to open root directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println(" - / does not seem to be a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("Scanning directory: ");
            Serial.println(file.name());
            if (levels) {
                register_web_paths(fs, file.name(), levels-1);
            }
        } else {
            Serial.print("  Registering path: ");
            Serial.println(file.name());
            webServer.serveStatic(file.name(), fs, file.name());
        }
        file = root.openNextFile();
    }
}

/*
 * Web Server Processing
 */
void initialize_web_server() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, pass);
    delay(1000);

    IPAddress ip = WiFi.softAPIP();

//    webServer.on("/", handle_root);
    webServer.on("/red", handle_red_led);
    webServer.on("/green", handle_green_led);
    webServer.on("/blue", handle_blue_led);

    register_web_paths(LITTLEFS, "/", 2);
    webServer.serveStatic("/", LITTLEFS, "/index.html");
    webServer.begin();

    Serial.print(F("To interact with the AudioLux open a browser to http://"));
    Serial.println(ip);
}

void handle_root() {
    webServer.send(200, "text/html", web_page());
}

void handle_red_led() {
    fHue = HUE_RED;
    webServer.send(200, "text/html", web_page());
}

void handle_blue_led() {
    fHue = HUE_BLUE;
    webServer.send(200, "text/html", web_page());
}

void handle_green_led() {
    fHue = HUE_GREEN;
    webServer.send(200, "text/html", web_page());
}

String web_page() {
    String page = "<!DOCTYPE html> <html>\n";
    page +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
    page +="<title>LED Strip Color Control</title>\n";
    page +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
    page +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
    page +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
    page +=".button-red {background-color: #cc0000;}\n";
    page +=".button-red:active {background-color: #ff0000;}\n";
    page +=".button-red-disabled {background-color: #683636;}\n";
    page +=".button-green {background-color: #00cc00;}\n";
    page +=".button-green:active {background-color: #00ff00;}\n";
    page +=".button-green-disabled {background-color: #324932;}\n";
    page +=".button-blue {background-color: #0000cc;}\n";
    page +=".button-blue:active {background-color: #0000ff;}\n";
    page +=".button-blue-disabled {background-color: #36365e;}\n";
    page +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
    page +="</style>\n";
    page +="</head>\n";
    page +="<body>\n";
    page +="<h1>AudioLux Web Server</h1>\n";
    page +="<h3>Using Access Point(AP) Mode</h3>\n";

    if (fHue == HUE_RED){
        page +="<a class=\"button button-red-disabled\" href=\"#\">RED ON</a>\n";
    } else {
        page +="<a class=\"button button-red\" href=\"/red\">SET RED</a>\n";
    }

    if (fHue == HUE_GREEN){
        page +="<a class=\"button button-green-disabled\" href=\"#\">GREEN ON</a>\n";
    } else {
        page +="<a class=\"button button-green\" href=\"/green\">SET GREEN</a>\n";
    }

    if (fHue == HUE_BLUE){
        page +="<a class=\"button button-blue-disabled\" href=\"#\">BLUE ON</a>\n";
    } else {
        page +="<a class=\"button button-blue\" href=\"/blue\">SET BLUE</a>\n";
    }

    page +="</body>\n";
    page +="</html>\n";
    return page;
}


/**********************************************************************************************/
/* Code imported from Nanolux                                                                 */

void show_pattern() {
    freq_hue_trail();
    FastLED.show();
}

void freq_hue_trail() {
    static bool blank = false;
    leds[0] = CHSV(fHue, LED_SATURATION, blank ? 0 : LED_BRIGHTNESS);

    for(int i = 20; i >= 0; i--) {
        leds[i] = leds[i-1];
    }
    blank = !blank;
}

void blank() {
    FastLED.clear();
}

void initialize_serial() {
    Serial.begin(115200);
    while(!Serial){ ; }
    Serial.println("Serial Ready!\n\n");
}

void initialize_led_strip() {
    FastLED.addLeds<LED_TYPE, DATA_PIN, CLK_PIN, COLOR_ORDER>(leds, MAX_LEDS).setCorrection(TypicalLEDStrip);
    blank();
}

/**********************************************************************************************/
