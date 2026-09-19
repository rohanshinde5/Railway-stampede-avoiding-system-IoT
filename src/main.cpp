#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include "wifi.hpp"
#include "mqtt.hpp"

// ============================================================
// RAILWAY CROWD MANAGEMENT & STAMPEDE PREVENTION SYSTEM
// ESP32 + Wokwi
//
// VERSION 2:
// 4 PIR sensors for directional entry/exit detection
// Emergency + Reset buttons retained
// ============================================================


// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);


// ============================================================
// PIR MOTION SENSORS
// ============================================================

// ENTRY GATE
#define ENTRY_PIR_A 16
#define ENTRY_PIR_B 17

// EXIT GATE
#define EXIT_PIR_A 18
#define EXIT_PIR_B 19


// ============================================================
// BUTTONS
// ============================================================

#define EMERGENCY_BUTTON 33
#define RESET_BUTTON 32


// ============================================================
// STATUS LEDs
// ============================================================

#define GREEN_LED 25
#define YELLOW_LED 26
#define RED_LED 27
#define SECURITY_LED 14


// ============================================================
// BUZZER
// ============================================================

#define BUZZER_PIN 23


// ============================================================
// SERVO
// ============================================================

#define SERVO_PIN 13

Servo gateServo;


// ============================================================
// CROWD CONFIGURATION
// ============================================================

const int MAX_CAPACITY = 20;

const int WARNING_THRESHOLD = 12;
const int HIGH_THRESHOLD = 16;
const int CRITICAL_THRESHOLD = 19;


// Current people inside station
int crowdCount = 0;


// ============================================================
// PIR DIRECTION DETECTION
// ============================================================

// Sensor sequence states
enum SequenceState
{
    IDLE,
    FIRST_SENSOR_TRIGGERED
};


// ENTRY sequence
SequenceState entryState = IDLE;

unsigned long entryFirstTriggerTime = 0;


// EXIT sequence
SequenceState exitState = IDLE;

unsigned long exitFirstTriggerTime = 0;


// Maximum time allowed between sensor A and B
const unsigned long SEQUENCE_TIMEOUT = 2500;


// Previous PIR states
bool previousEntryA = LOW;
bool previousEntryB = LOW;

bool previousExitA = LOW;
bool previousExitB = LOW;


// ============================================================
// EMERGENCY
// ============================================================

bool emergencyMode = false;


// ============================================================
// CROWD STATUS
// ============================================================

enum CrowdStatus
{
    NORMAL,
    WARNING,
    HIGH_ALERT,
    CRITICAL
};


// ============================================================
// GET CROWD STATUS
// ============================================================

CrowdStatus getCrowdStatus()
{
    if (emergencyMode)
    {
        return CRITICAL;
    }

    if (crowdCount >= CRITICAL_THRESHOLD)
    {
        return CRITICAL;
    }

    if (crowdCount >= HIGH_THRESHOLD)
    {
        return HIGH_ALERT;
    }

    if (crowdCount >= WARNING_THRESHOLD)
    {
        return WARNING;
    }

    return NORMAL;
}


// ============================================================
// STATUS NAME
// ============================================================

const char* getStatusName(CrowdStatus status)
{
    switch (status)
    {
        case NORMAL:
            return "NORMAL";

        case WARNING:
            return "WARNING";

        case HIGH_ALERT:
            return "HIGH ALERT";

        case CRITICAL:
            return "CRITICAL";

        default:
            return "UNKNOWN";
    }
}


// ============================================================
// OLED DISPLAY
// ============================================================

void updateDisplay()
{
    CrowdStatus status = getCrowdStatus();

    int percentage =
        (crowdCount * 100) / MAX_CAPACITY;

    display.clearDisplay();

    display.setTextColor(SSD1306_WHITE);

    // TITLE
    display.setTextSize(1);

    display.setCursor(0, 0);

    display.println("RAILWAY CROWD SYSTEM");

    display.drawLine(
        0,
        10,
        127,
        10,
        SSD1306_WHITE
    );


    // CROWD COUNT
    display.setCursor(0, 15);

    display.print("Crowd: ");

    display.setTextSize(2);

    display.print(crowdCount);

    display.setTextSize(1);

    display.print("/");

    display.println(MAX_CAPACITY);


    // DENSITY
    display.setCursor(0, 37);

    display.print("Density: ");

    display.print(percentage);

    display.println("%");


    // STATUS
    display.setCursor(0, 50);

    display.print("Status: ");

    display.println(getStatusName(status));


    display.display();
}


// ============================================================
// HARDWARE RESPONSE
// ============================================================

void updateSystemResponse()
{
    CrowdStatus status = getCrowdStatus();


    // Turn OFF everything first

#if USE_LEDS
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(SECURITY_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
#endif


    switch (status)
    {

        // ====================================================
        // NORMAL
        // ====================================================

        case NORMAL:

#if USE_LEDS
            digitalWrite(
                GREEN_LED,
                HIGH
            );
#endif

            gateServo.write(0);

            break;


        // ====================================================
        // WARNING
        // ====================================================

        case WARNING:

#if USE_LEDS
            digitalWrite(
                YELLOW_LED,
                HIGH
            );
#endif

            // Additional passage partially opened
            gateServo.write(30);

            break;


        // ====================================================
        // HIGH ALERT
        // ====================================================

        case HIGH_ALERT:

#if USE_LEDS
            digitalWrite(
                RED_LED,
                HIGH
            );

            digitalWrite(
                SECURITY_LED,
                HIGH
            );
#endif

            // Open additional passage
            gateServo.write(90);

#if USE_LEDS
            // Short warning beep
            digitalWrite(
                BUZZER_PIN,
                HIGH
            );

            delay(100);

            digitalWrite(
                BUZZER_PIN,
                LOW
            );
#endif

            break;


        // ====================================================
        // CRITICAL
        // ====================================================

        case CRITICAL:

#if USE_LEDS
            digitalWrite(
                RED_LED,
                HIGH
            );

            digitalWrite(
                SECURITY_LED,
                HIGH
            );
#endif

            // Emergency passage fully open
            gateServo.write(150);

#if USE_LEDS
            // Continuous alarm
            digitalWrite(
                BUZZER_PIN,
                HIGH
            );
#endif

            break;
    }
}


// ============================================================
// SERIAL STATUS
// ============================================================

void printStatus()
{
    CrowdStatus status =
        getCrowdStatus();

    int percentage =
        (crowdCount * 100) /
        MAX_CAPACITY;


    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        "       RAILWAY CROWD MANAGEMENT"
    );

    Serial.println(
        "========================================"
    );


    Serial.print(
        "People inside : "
    );

    Serial.println(
        crowdCount
    );


    Serial.print(
        "Capacity      : "
    );

    Serial.println(
        MAX_CAPACITY
    );


    Serial.print(
        "Density       : "
    );

    Serial.print(
        percentage
    );

    Serial.println("%");


    Serial.print(
        "Status        : "
    );

    Serial.println(
        getStatusName(status)
    );


    Serial.print(
        "Emergency     : "
    );

    Serial.println(
        emergencyMode
            ? "ACTIVE"
            : "OFF"
    );


    Serial.println(
        "========================================"
    );
}


// ============================================================
// PERSON ENTERED
// ============================================================

void personEntered()
{
    if (crowdCount < MAX_CAPACITY)
    {
        crowdCount++;

        Serial.println();

        Serial.println(
            ">>> PERSON ENTERED"
        );

        Serial.println(
            "Direction: ENTRY A -> ENTRY B"
        );

        printStatus();

        updateDisplay();

        updateSystemResponse();
        publishState(crowdCount, MAX_CAPACITY, getStatusName(getCrowdStatus()));
        publishEntryEvent(crowdCount);
    }
    else
    {
        Serial.println();

        Serial.println(
            "!!! CAPACITY REACHED !!!"
        );

        Serial.println(
            "ENTRY RESTRICTED"
        );

#if USE_LEDS
        digitalWrite(
            RED_LED,
            HIGH
        );

        digitalWrite(
            SECURITY_LED,
            HIGH
        );

        digitalWrite(
            BUZZER_PIN,
            HIGH
        );
#endif

        gateServo.write(150);

        updateDisplay();
    }
}


// ============================================================
// PERSON EXITED
// ============================================================

void personExited()
{
    if (crowdCount > 0)
    {
        crowdCount--;

        Serial.println();

        Serial.println(
            "<<< PERSON EXITED"
        );

        Serial.println(
            "Direction: EXIT A -> EXIT B"
        );

        printStatus();

        updateDisplay();

        updateSystemResponse();
        publishState(crowdCount, MAX_CAPACITY, getStatusName(getCrowdStatus()));
        publishExitEvent(crowdCount);
    }
    else
    {
        Serial.println(
            "Station already empty."
        );
    }
}


// ============================================================
// ENTRY SENSOR SEQUENCE
// ============================================================

void processEntrySensors()
{
    bool sensorA =
        digitalRead(ENTRY_PIR_A);

    bool sensorB =
        digitalRead(ENTRY_PIR_B);


    // --------------------------------------------------------
    // SENSOR A TRIGGERED FIRST
    // --------------------------------------------------------

    if (
        entryState == IDLE &&
        sensorA == HIGH &&
        previousEntryA == LOW
    )
    {
        entryState =
            FIRST_SENSOR_TRIGGERED;

        entryFirstTriggerTime =
            millis();

        Serial.println();

        Serial.println(
            "[ENTRY] Sensor A detected movement"
        );
    }


    // --------------------------------------------------------
    // SENSOR B TRIGGERED AFTER A
    // --------------------------------------------------------

    if (
        entryState == FIRST_SENSOR_TRIGGERED &&
        sensorB == HIGH &&
        previousEntryB == LOW
    )
    {
        unsigned long elapsed =
            millis() -
            entryFirstTriggerTime;


        if (
            elapsed <=
            SEQUENCE_TIMEOUT
        )
        {
            Serial.println(
                "[ENTRY] A -> B sequence confirmed"
            );

            personEntered();
        }


        entryState =
            IDLE;
    }


    // --------------------------------------------------------
    // TIMEOUT
    // --------------------------------------------------------

    if (
        entryState ==
        FIRST_SENSOR_TRIGGERED
    )
    {
        if (
            millis() -
            entryFirstTriggerTime >
            SEQUENCE_TIMEOUT
        )
        {
            Serial.println(
                "[ENTRY] Sequence timeout"
            );

            entryState =
                IDLE;
        }
    }


    previousEntryA = sensorA;
    previousEntryB = sensorB;
}


// ============================================================
// EXIT SENSOR SEQUENCE
// ============================================================

void processExitSensors()
{
    bool sensorA =
        digitalRead(EXIT_PIR_A);

    bool sensorB =
        digitalRead(EXIT_PIR_B);


    // --------------------------------------------------------
    // SENSOR A FIRST
    // --------------------------------------------------------

    if (
        exitState == IDLE &&
        sensorA == HIGH &&
        previousExitA == LOW
    )
    {
        exitState =
            FIRST_SENSOR_TRIGGERED;

        exitFirstTriggerTime =
            millis();

        Serial.println();

        Serial.println(
            "[EXIT] Sensor A detected movement"
        );
    }


    // --------------------------------------------------------
    // SENSOR B SECOND
    // --------------------------------------------------------

    if (
        exitState ==
            FIRST_SENSOR_TRIGGERED &&
        sensorB == HIGH &&
        previousExitB == LOW
    )
    {
        unsigned long elapsed =
            millis() -
            exitFirstTriggerTime;


        if (
            elapsed <=
            SEQUENCE_TIMEOUT
        )
        {
            Serial.println(
                "[EXIT] A -> B sequence confirmed"
            );

            personExited();
        }


        exitState =
            IDLE;
    }


    // --------------------------------------------------------
    // TIMEOUT
    // --------------------------------------------------------

    if (
        exitState ==
        FIRST_SENSOR_TRIGGERED
    )
    {
        if (
            millis() -
            exitFirstTriggerTime >
            SEQUENCE_TIMEOUT
        )
        {
            Serial.println(
                "[EXIT] Sequence timeout"
            );

            exitState =
                IDLE;
        }
    }


    previousExitA = sensorA;
    previousExitB = sensorB;
}


// ============================================================
// EMERGENCY
// ============================================================

void activateEmergency()
{
    emergencyMode =
        true;


    Serial.println();

    Serial.println(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    );

    Serial.println(
        "       EMERGENCY MODE ACTIVATED"
    );

    Serial.println(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    );


    Serial.println(
        "Emergency passage OPEN"
    );

    Serial.println(
        "Security deployment required"
    );

    Serial.println(
        "Incoming crowd restricted"
    );


    updateDisplay();

    updateSystemResponse();
}


// ============================================================
// RESET
// ============================================================

void resetSystem()
{
    crowdCount =
        0;

    emergencyMode =
        false;


    entryState =
        IDLE;

    exitState =
        IDLE;


    gateServo.write(0);


#if USE_LEDS
    digitalWrite(
        GREEN_LED,
        HIGH
    );

    digitalWrite(
        YELLOW_LED,
        LOW
    );

    digitalWrite(
        RED_LED,
        LOW
    );

    digitalWrite(
        SECURITY_LED,
        LOW
    );

    digitalWrite(
        BUZZER_PIN,
        LOW
    );
#endif


    Serial.println();

    Serial.println(
        "########################################"
    );

    Serial.println(
        "             SYSTEM RESET"
    );

    Serial.println(
        "########################################"
    );


    printStatus();

    updateDisplay();
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);


    // --------------------------------------------------------
    // PIR SENSORS
    // --------------------------------------------------------

    pinMode(
        ENTRY_PIR_A,
        INPUT
    );

    pinMode(
        ENTRY_PIR_B,
        INPUT
    );

    pinMode(
        EXIT_PIR_A,
        INPUT
    );

    pinMode(
        EXIT_PIR_B,
        INPUT
    );


    // --------------------------------------------------------
    // BUTTONS
    // --------------------------------------------------------

    pinMode(
        EMERGENCY_BUTTON,
        INPUT_PULLUP
    );

    pinMode(
        RESET_BUTTON,
        INPUT_PULLUP
    );


    // --------------------------------------------------------
    // LEDs
    // --------------------------------------------------------

    pinMode(
        GREEN_LED,
        OUTPUT
    );

    pinMode(
        YELLOW_LED,
        OUTPUT
    );

    pinMode(
        RED_LED,
        OUTPUT
    );

    pinMode(
        SECURITY_LED,
        OUTPUT
    );


    // --------------------------------------------------------
    // BUZZER
    // --------------------------------------------------------

    pinMode(
        BUZZER_PIN,
        OUTPUT
    );


    // --------------------------------------------------------
    // SERVO
    // --------------------------------------------------------

    gateServo.attach(
        SERVO_PIN
    );

    gateServo.write(0);


    // --------------------------------------------------------
    // OLED
    // --------------------------------------------------------

    Wire.begin(
        21,
        22
    );


    if (
        !display.begin(
            SSD1306_SWITCHCAPVCC,
            OLED_ADDRESS
        )
    )
    {
        Serial.println(
            "OLED initialization failed!"
        );

        while (true)
        {
            delay(1000);
        }
    }


    // --------------------------------------------------------
    // INITIAL HARDWARE STATE
    // --------------------------------------------------------

#if USE_LEDS
    digitalWrite(
        GREEN_LED,
        HIGH
    );

    digitalWrite(
        YELLOW_LED,
        LOW
    );

    digitalWrite(
        RED_LED,
        LOW
    );

    digitalWrite(
        SECURITY_LED,
        LOW
    );

    digitalWrite(
        BUZZER_PIN,
        LOW
    );
#endif


    // --------------------------------------------------------
    // STARTUP SCREEN
    // --------------------------------------------------------

    display.clearDisplay();

    display.setTextColor(
        SSD1306_WHITE
    );


    display.setTextSize(1);

    display.setCursor(
        10,
        5
    );

    display.println(
        "RAILWAY STATION"
    );


    display.setTextSize(2);

    display.setCursor(
        12,
        22
    );

    display.println(
        "CROWD"
    );


    display.setCursor(
        12,
        43
    );

    display.println(
        "CONTROL"
    );


    display.display();

    delay(2000);


    updateDisplay();


    // --------------------------------------------------------
    // SERIAL STARTUP
    // --------------------------------------------------------

    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        " RAILWAY CROWD MANAGEMENT SYSTEM"
    );

    Serial.println(
        " ESP32 + 4 PIR DIRECTION DETECTION"
    );

    Serial.println(
        "========================================"
    );


    Serial.println();

    Serial.println(
        "ENTRY:"
    );

    Serial.println(
        "PIR-A -> PIR-B = PERSON ENTERED"
    );


    Serial.println();

    Serial.println(
        "EXIT:"
    );

    Serial.println(
        "PIR-A -> PIR-B = PERSON EXITED"
    );


    Serial.println();

    Serial.println(
        "Emergency Button = GPIO 33"
    );

    Serial.println(
        "Reset Button     = GPIO 32"
    );


    printStatus();

    // --------------------------------------------------------
    // WI-FI + MQTT
    // --------------------------------------------------------
    setupWifi();
    setupMqtt();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // WI-FI + MQTT MAINTENANCE
    // --------------------------------------------------------
    maintainWifi();
    maintainMqtt();

    // --------------------------------------------------------
    // PROCESS PIR SENSORS
    // --------------------------------------------------------

    processEntrySensors();

    processExitSensors();


    // --------------------------------------------------------
    // EMERGENCY BUTTON
    // --------------------------------------------------------

    static bool lastEmergencyState =
        HIGH;

    bool emergencyState =
        digitalRead(
            EMERGENCY_BUTTON
        );


    if (
        lastEmergencyState == HIGH &&
        emergencyState == LOW
    )
    {
        activateEmergency();

        delay(200);
    }


    lastEmergencyState =
        emergencyState;


    // --------------------------------------------------------
    // RESET BUTTON
    // --------------------------------------------------------

    static bool lastResetState =
        HIGH;

    bool resetState =
        digitalRead(
            RESET_BUTTON
        );


    if (
        lastResetState == HIGH &&
        resetState == LOW
    )
    {
        resetSystem();

        delay(200);
    }


    lastResetState =
        resetState;


    delay(10);
}