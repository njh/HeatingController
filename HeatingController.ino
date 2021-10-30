/*
 * Heating Controller
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
//#include <avr/wdt.h>

#include <EtherSia.h>


#define UDP_PORT    (25910)

#define BOILER_RELAY_PIN      (5)
#define RADIATOR_RELAY_PIN    (6)
#define UNDERFLOOR_RELAY_PIN  (7)

#if ETHERSIA_MAX_PACKET_SIZE < 900
#error EtherSia packet buffer is less than 900 bytes
#endif

/** ENC28J60 Ethernet Interface */
EtherSia_ENC28J60 ether(10);

/** Define HTTP server */
HTTPServer http(ether);

/** Define UDP socket - for receiving commands */
UDPSocket udpReciever(ether, UDP_PORT);

/** Define UDP socket - for transmitting status */
UDPSocket udpSender(ether);

/** Publish current status in next loop */
bool doPublish = true;


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


    // Enable the Watchdog timer
    //wdt_enable(WDTO_8S);

    // Start Ethernet
    if (ether.begin(macAddress) == false) {
        Serial.println(F("Failed to configure Ethernet"));
    }

    Serial.print(F("Our address is: "));
    ether.globalAddress().println();

    if (udpSender.setRemoteAddress("house.aelius.co.uk", UDP_PORT)) {
        Serial.print(F("UDP status destination: "));
        udpSender.remoteAddress().println();
    } else {
        Serial.println(F("Error: failed to set UDP remote address"));
    }

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


void sendUdp(byte pin, char label)
{
    char payload[3];

    payload[0] = label;
    if (digitalRead(pin)) {
        payload[1] = '1';
    } else {
        payload[1] = '0';
    }
    payload[2] = '\0';

    Serial.print(payload);
    udpSender.send(payload);
}

void publishStatusUdp()
{
    Serial.print(F("Publishing status: "));
    sendUdp(RADIATOR_RELAY_PIN, 'R');
    Serial.print(' ');
    sendUdp(UNDERFLOOR_RELAY_PIN, 'U');
    Serial.println();
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

    doPublish = true;
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


void handleUdpPacket()
{
    char *payload = (char*)udpReciever.payload();
    bool state;

    if (udpReciever.payloadLength() != 2) {
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

    doPublish = true;
    setBoilerRelay();
}

// the loop function runs over and over again forever
void loop()
{
    // Check for an available packet
    ether.receivePacket();

    if (http.havePacket()) {
        handleHttp();
    } else if (udpReciever.havePacket()) {
        handleUdpPacket();
    } else {
        // Send back a ICMPv6 Destination Unreachable response
        // to any other connection attempts
        ether.rejectPacket();
    }

    if (doPublish) {
        publishStatusUdp();
        doPublish = false;
    }

    // Reset the watchdog
    //wdt_reset();
}
