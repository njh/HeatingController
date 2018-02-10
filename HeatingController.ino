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
 * There is a UDP control interface on port 25910.
 * It accepts the following two-byte ASCII messages:
 *  R1 - Turn the radiators On
 *  R0 - Turn the radiators Off
 *  U1 - Turn the Underfloor Heating On
 *  U0 - Turn the Underfloor Heating Off
 */

#include <Arduino.h>
#include <avr/wdt.h>

#include <EtherSia.h>
#include <TimerOne.h>
#include <avdweb_Switch.h>


#define BOILER_BUTTON_PIN     (A0)
#define RADIATOR_BUTTON_PIN   (A1)
#define UNDERFLOOR_BUTTON_PIN (A2)

#define BOILER_RELAY_PIN      (5)
#define RADIATOR_RELAY_PIN    (6)
#define UNDERFLOOR_RELAY_PIN  (7)


/** ENC28J60 Ethernet Interface */
EtherSia_ENC28J60 ether(10);

/** Define HTTP server */
HTTPServer http(ether);

/** Define UDP server */
UDPSocket udp(ether, 25910);



// Button to VCC, 10k pull-down resistor, no internal pull-up resistor, HIGH polarity
Switch radiatorButton = Switch(RADIATOR_BUTTON_PIN, INPUT, HIGH);
Switch underfloorButton = Switch(UNDERFLOOR_BUTTON_PIN, INPUT, HIGH);


void setup()
{
    MACAddress macAddress("aa:d3:5a:f7:51:c5");

    Serial.begin(115200);
    Serial.println(F("[HeatingController]"));
    macAddress.println();

    // Set relay pins to outputs
    pinMode(BOILER_RELAY_PIN, OUTPUT);
    pinMode(RADIATOR_RELAY_PIN, OUTPUT);
    pinMode(UNDERFLOOR_RELAY_PIN, OUTPUT);

    Timer1.initialize(50000);   // 50ms
    Timer1.attachInterrupt(checkButtons);

    // Enable the Watchdog timer
    wdt_enable(WDTO_8S);

    // Start Ethernet
    if (ether.begin(macAddress) == false) {
        Serial.println(F("Failed to configure Ethernet"));
    }

    Serial.print(F("Our address is: "));
    ether.globalAddress().println();

    Serial.println(F("Ready."));
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

void checkButtons(void)
{
  radiatorButton.poll();  
  underfloorButton.poll();  

  if (radiatorButton.pushed()) {
      digitalToggle(RADIATOR_RELAY_PIN);
  }
  
  if (underfloorButton.pushed()) {
      digitalToggle(UNDERFLOOR_RELAY_PIN);
  }

  setBoilerRelay();
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
    http.print(F("<html><head><title>Heating Controller</title>"));
    http.print(F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"));
    http.print(F("<link href=\"https://star.aelius.co.uk/heating-controller.css\" rel=\"stylesheet\" />"));
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


void handleUdp()
{
    char *payload = (char*)udp.payload();
    bool state;

    if (udp.payloadLength() != 2) {
        // Invalid payload
        Serial.println(F("Received invalid UDP payload"));
        return;
    }

    if (payload[1] == '1') {
        state = HIGH;
    } else if (payload[1] == '0') {
        state = LOW;
    } else {
        // Invalid Payload
        Serial.println(F("Received invalid UDP device state"));
        return;
    }

    if (payload[0] == 'R') {
        // Change Radiator State
        digitalWrite(RADIATOR_RELAY_PIN, state);
    } else if (payload[0] == 'U') {
        // Change Underfloor State
        digitalWrite(UNDERFLOOR_RELAY_PIN, state);
    } else {
        // Invalid Payload
        Serial.println(F("Received invalid UDP device id"));
        return;
    }

    setBoilerRelay();
}

// the loop function runs over and over again forever
void loop()
{
    // Check for an available packet
    ether.receivePacket();

    if (http.havePacket()) {
        handleHttp();
    } else if (udp.havePacket()) {
        handleUdp();
    } else {
        // Send back a ICMPv6 Destination Unreachable response
        // to any other connection attempts
        ether.rejectPacket();
    }

    // Reset the watchdog
    wdt_reset();
}
