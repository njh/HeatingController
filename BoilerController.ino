/**
 * Boiler Controller
 */

#include <Arduino.h>
#include <avr/wdt.h>

#include <EtherSia.h>
#include <TimerOne.h>

/** ENC28J60 Ethernet Interface */
EtherSia_ENC28J60 ether(10);

/** Define HTTP server */
HTTPServer http(ether);


#define FIRST_INPUT_PIN   (A0)
#define FIRST_OUTPUT_PIN  (5)
#define CHANNEL_COUNT     (3)


const char label1[] PROGMEM = "Boiler";
const char label2[] PROGMEM = "Radiators";
const char label3[] PROGMEM = "Underfloor";
const char* const labels[CHANNEL_COUNT] = {label1, label2, label3};

void setup()
{
    MACAddress macAddress("aa:1c:5d:fe:e7:86");

    Serial.begin(115200);
    Serial.println(F("[BoilerController]"));
    macAddress.println();

    for(byte i=0; i<CHANNEL_COUNT; i++) {
        pinMode(FIRST_OUTPUT_PIN+i, OUTPUT);
        pinMode(FIRST_INPUT_PIN+i, INPUT);
    }

    Timer1.initialize(50000);   // 50ms
    Timer1.attachInterrupt(checkButtons);

    // Enable the Watchdog timer
    wdt_enable(WDTO_8S);

    // Start Ethernet
    if (ether.begin(macAddress) == false) {
        Serial.println("Failed to configure Ethernet");
    }

    Serial.print(F("Our address is: "));
    ether.globalAddress().println();

    Serial.println("Ready.");
}

void digitalToggle(byte pin)
{
    digitalWrite(pin, !digitalRead(pin));
}

void checkButtons(void) {
    static byte ignore_count[CHANNEL_COUNT] = {0,0,0};
    static byte trigger_count[CHANNEL_COUNT] = {0,0,0};

    for(byte i=0; i<CHANNEL_COUNT; i++) {
        byte state = digitalRead(FIRST_INPUT_PIN + i);

        // Check if we are in an ignore period
        if (ignore_count[i] == 0) {

            if (state == HIGH) {
                // Increment the trigger_count every 50 ms while it is less than 3 and the BTN is HIGH
                if (trigger_count[i] < 3) {
                    trigger_count[i]++;

                } else {
                    // If the button is HIGH and if the 200 milliseconds of debounce has been met
                    digitalToggle(FIRST_OUTPUT_PIN + i);

                    trigger_count[i] = 0;  // Reset the button counter
                    ignore_count[i] = 8;   // Set a 400 millisecond Ignore counter
                }

            } else {
                // Reset counters if the button press was a bounce (state LOW)
                if (trigger_count[i] > 0)
                    trigger_count[i] = 0;
            }

        } else if (state == LOW) {
            // Count Down the Button Ignore variable once it has been released
            --ignore_count[i];
        }
    }
}

void printIndex()
{
    http.print(F("<!DOCTYPE html>"));
    http.print(F("<html><head><title>Boiler Controller</title>"));
    http.print(F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"));
    http.print(F("<link href=\"http://star.aelius.co.uk/boiler.css\" rel=\"stylesheet\" />"));
    http.print(F("</head><body><h1>Boiler Controller</h1>"));
    http.print(F("<form method=\"POST\"><table>"));
    for (uint8_t i=0; i<CHANNEL_COUNT; i++) {
      char buffer[16];
      strcpy_P(buffer, labels[i]);
      http.print(F("<tr><th>"));
      http.print(buffer);
      http.print(F(": </th>"));
      if (digitalRead(FIRST_OUTPUT_PIN + i)) {
          http.print(F("<td class=\"on\">On</td>"));
      } else {
          http.print(F("<td class=\"off\">Off</td>"));
      }
      http.print(F("<td><button type=\"submit\" formaction=\"/outputs/"));
      http.print(i+1, DEC);
      http.print(F("\">Toggle</button></td></tr>"));
    }
    http.print(F("</table></form></body></html>"));
}

// Get the output number from the path of the request
int8_t pathToNum()
{
    // /outputs/X
    // 0123456789
    int8_t num = http.path()[9] - '1';
    if (0 <= num && num < CHANNEL_COUNT) {
        return num;
    } else {
        http.notFound();
        return -1;
    }
}

// the loop function runs over and over again forever
void loop()
{
    // Check for an available packet
    ether.receivePacket();

    // GET the index page
    if (http.isGet(F("/"))) {
        http.printHeaders(http.typeHtml);
        printIndex();
        http.sendReply();

    // GET the state of a single output
    } else if (http.isGet(F("/outputs/?"))) {
        int8_t num = pathToNum();
        if (num != -1) {
            http.printHeaders(http.typePlain);
            if (digitalRead(FIRST_OUTPUT_PIN + num)) {
                http.println(F("on"));
            } else {
                http.println(F("off"));
            }
            http.sendReply();
        }

    // POST the state of a single output
    } else if (http.isPost(F("/outputs/?"))) {
        int8_t num = pathToNum();
        if (num != -1) {
            if (http.body() == NULL) {
                digitalToggle(FIRST_OUTPUT_PIN + num);
            } else if (http.bodyEquals("on")) {
                digitalWrite(FIRST_OUTPUT_PIN + num, HIGH);
            } else if (http.bodyEquals("off")) {
                digitalWrite(FIRST_OUTPUT_PIN + num, LOW);
            }
            http.redirect(F("/"));
        }

    // No matches - return 404 Not Found page
    } else {
        http.notFound();
    }

    // Reset the watchdog
    wdt_reset();
}
