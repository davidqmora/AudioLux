#include <FastLED.h>
#include "WebServer.h"

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
 * LED Display parameters
 */
// HSV values. We only manipulate hue.
volatile uint8_t fHue = 0;
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
WiFiServer webServer(80);

void initialize_web_server();


void setup() {
    initialize_serial();
    initialize_led_strip();
    initialize_timer();
    initialize_web_server();
}

void loop() {
    handle_web();
}

void initialize_timer() {
    timer = timerBegin(TIMER0, PRESCALER, COUNT_UP);
    timerAttachInterrupt(timer, &onTimer, TRIGGER_ON_HIGH);

    timerAlarmWrite(timer, TIMER_INTERVAL_US, REARM_ON_TRIGGER);
    timerAlarmEnable(timer);
}

void initialize_web_server() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, pass);

    delay(1000);
    webServer.begin();

    IPAddress ip = WiFi.softAPIP();
    Serial.print(F("To interact with the AudioLux open a browser to http://"));
    Serial.println(ip);
}

void handle_web()
{
    WiFiClient client = webServer.available();   // listen for incoming clients

    if (client)
    {
        // if you get a client,
        Serial.println(F("New client"));           // print a message out the serial port
        String currentLine = "";                // make a String to hold incoming data from the client

        while (client.connected())
        {
            // loop while the client's connected
            if (client.available())
            {
                // if there's bytes to read from the client,
                char c = client.read();             // read a byte, then
                Serial.write(c);                    // print it out the serial monitor

                if (c == '\n')
                {
                    // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0)
                    {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println(F("HTTP/1.1 200 OK"));
                        client.println(F("Content-type:text/html"));
                        client.println();

                        // the content of the HTTP response follows the header:
                        client.print(F("Click <a href=\"/H\">here</a> turn the LED on<br>"));
                        client.print(F("Click <a href=\"/L\">here</a> turn the LED off<br>"));

                        // The HTTP response ends with another blank line:
                        client.println();
                        // break out of the while loop:
                        break;
                    }
                    else
                    {
                        // if you got a newline, then clear currentLine:
                        currentLine = "";
                    }
                }
                else if (c != '\r')
                {
                    // if you got anything else but a carriage return character,
                    currentLine += c;      // add it to the end of the currentLine
                }

                show_pattern();

//                // Check to see if the client request was "GET /H" or "GET /L":
//                if (currentLine.endsWith(F("GET /H")))
//                {
//                    digitalWrite(led, HIGH);               // GET /H turns the LED on
//                }
//
//                if (currentLine.endsWith(F("GET /L")))
//                {
//                    digitalWrite(led, LOW);                // GET /L turns the LED off
//                }
            }
        }

        // close the connection:
        client.stop();
        Serial.println(F("Client disconnected"));
    }
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
