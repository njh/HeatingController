/*
 * Boiler Controller
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


#define FIRST_INPUT_PIN   (A0)
#define FIRST_OUTPUT_PIN  (5)
#define CHANNEL_COUNT     (3)


void setup()
{
    Serial.begin(115200);
    Serial.println(F("[BoilerController]"));

    for(byte i=0; i<CHANNEL_COUNT; i++) {
        pinMode(FIRST_OUTPUT_PIN+i, OUTPUT);
        pinMode(FIRST_INPUT_PIN+i, INPUT);
    }

    Timer1.initialize(40000);   // 40ms
    Timer1.attachInterrupt(checkButtons);

    // Enable the Watchdog timer
    wdt_enable(WDTO_8S);


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


// the loop function runs over and over again forever
void loop()
{

    // Reset the watchdog
    wdt_reset();
}
