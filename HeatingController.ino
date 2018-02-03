/*
 * Heating Controller
 *
 * Button Input 1 - A0
 * Button Input 2 - A1
 * Button Input 3 - A2
 *
 * Relay Output 1 - D5 - Boiler 'Call for Heat'
 * Relay Output 2 - D6 - Radiators Zone Valve
 * Relay Output 3 - D7 - Underfloor heating pump
 *
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

#define BOILER_RELAY_PIN     (5)
#define RADIATOR_RELAY_PIN   (6)
#define UNDERFLOOR_RELAY_PIN (7)


void setup()
{
    MACAddress macAddress("aa:d3:5a:f7:51:c5");

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

void setBoilerRelay()
{
    // Turn on the boiler if either the Radiator or Underfloor are On
    digitalWrite(
        BOILER_RELAY_PIN,
        digitalRead(RADIATOR_RELAY_PIN) || digitalRead(UNDERFLOOR_RELAY_PIN)
    );
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

                    setBoilerRelay();

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

void printOnOffHtml(uint8_t pin)
{
    bool state = digitalRead(pin);
    if (state) {
        http.print(F("<td class=\"on\">On</td>"));
    } else {
        http.print(F("<td class=\"off\">Off</td>"));
    }
}

void printIndex()
{
    http.print(F("<!DOCTYPE html>"));
    http.print(F("<html><head><title>Boiler Controller</title>"));
    http.print(F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"));
    http.print(F("<link href=\"http://star.aelius.co.uk/boiler.css\" rel=\"stylesheet\" />"));
    http.print(F("</head><body><h1>Heating Controller</h1>"));

    http.print(F("<form method=\"POST\"><table>"));

    http.print(F("<tr><th>Radiators</th>"));
    printOnOffHtml(RADIATOR_RELAY_PIN);
    http.print(F("<td><button type=\"submit\" formaction=\"/radiators\">Toggle</button></td></tr>"));

    http.print(F("<tr><th>Underfloor Heating</th>"));
    printOnOffHtml(UNDERFLOOR_RELAY_PIN);
    http.print(F("<td><button type=\"submit\" formaction=\"/underfloor\">Toggle</button></td></tr>"));

    http.print(F("</table></form></body></html>"));
}

void sendPinStateReply(uint8_t pin)
{
    bool state = digitalRead(pin);
    http.printHeaders(http.typePlain);
    if (state) {
        http.print(F("on"));
    } else {
        http.print(F("off"));
    }
    http.sendReply();
}

void handleHttpPost(uint8_t pin)
{
    if (http.body() == NULL) {
        digitalToggle(pin);
    } else if (http.bodyEquals("on")) {
        digitalWrite(pin, HIGH);
    } else if (http.bodyEquals("off")) {
        digitalWrite(pin, LOW);
    }
    http.redirect(F("/"));

    setBoilerRelay();
}

void handleHttp()
{
    // GET the index page
    if (http.isGet(F("/"))) {
        http.printHeaders(http.typeHtml);
        printIndex();
        http.sendReply();

    } else if (http.isGet(F("/radiators"))) {
        sendPinStateReply(RADIATOR_RELAY_PIN);

    } else if (http.isGet(F("/underfloor"))) {
        sendPinStateReply(UNDERFLOOR_RELAY_PIN);

    } else if (http.isPost(F("/radiators"))) {
        handleHttpPost(RADIATOR_RELAY_PIN);

    } else if (http.isPost(F("/underfloor"))) {
        handleHttpPost(UNDERFLOOR_RELAY_PIN);

    } else {
        // No matches - return 404 Not Found page
        http.notFound();
    }
}


// the loop function runs over and over again forever
void loop()
{
    // Check for an available packet
    ether.receivePacket();

    if (http.havePacket()) {
        handleHttp();
    }

    // Reset the watchdog
    wdt_reset();
}
