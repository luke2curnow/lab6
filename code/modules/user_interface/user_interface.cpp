#include "mbed.h"
#include "arm_book_lib.h"
#include "ThisThread.h"

#include "user_interface.h"

#include "code.h"
#include "siren.h"
#include "smart_home_system.h"
#include "fire_alarm.h"
#include "date_and_time.h"
#include "temperature_sensor.h"
#include "gas_sensor.h"
#include "matrix_keypad.h"
#include "display.h"

//=====[Declaration of private defines]========================================

#define DISPLAY_REFRESH_TIME_MS 1000
#define TEMPERATURE_THRESHOLD_C 30
#define CODE_PROMPT_TIME_MS 5000
#define BUTTON_DISPLAY_TIME_MS 2000

//=====[Declaration and initialization of public global objects]===============

DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

//=====[Declaration and initialization of public global variables]=============

char codeSequenceFromUserInterface[CODE_NUMBER_OF_KEYS];

//=====[Declaration and initialization of private global variables]============

static bool incorrectCodeState = OFF;
static bool systemBlockedState = OFF;

static bool codeComplete = false;
static int numberOfCodeChars = 0;
static bool codePromptDisplayed = false;
static int codePromptTime = 0;

static bool buttonDisplayActive = false;
static int buttonDisplayTime = 0;

//=====[Declarations (prototypes) of private functions]========================

static void userInterfaceMatrixKeypadUpdate();
static void incorrectCodeIndicatorUpdate();
static void systemBlockedIndicatorUpdate();

static void userInterfaceDisplayInit();
static void userInterfaceDisplayUpdate();

//=====[Implementations of public functions]===================================

void userInterfaceInit()
{
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
    matrixKeypadInit(SYSTEM_TIME_INCREMENT_MS);
    userInterfaceDisplayInit();
    printf("System initialized, Blocked: %d, Incorrect: %d\n",
           systemBlockedState, incorrectCodeState);
}

void userInterfaceUpdate()
{
    userInterfaceMatrixKeypadUpdate();
    incorrectCodeIndicatorUpdate();
    systemBlockedIndicatorUpdate();
    userInterfaceDisplayUpdate();
}

bool incorrectCodeStateRead()
{
    return incorrectCodeState;
}

void incorrectCodeStateWrite(bool state)
{
    incorrectCodeState = state;
}

bool systemBlockedStateRead()
{
    return systemBlockedState;
}

void systemBlockedStateWrite(bool state)
{
    systemBlockedState = state;
}

bool userInterfaceCodeCompleteRead()
{
    return codeComplete;
}

void userInterfaceCodeCompleteWrite(bool state)
{
    codeComplete = state;
    if (!state) {
        numberOfCodeChars = 0;
        memset(codeSequenceFromUserInterface, 0, CODE_NUMBER_OF_KEYS);
        printf("Code complete reset, CodeChars: %d\n", numberOfCodeChars);
    }
}

//=====[Implementations of private functions]==================================

static void userInterfaceMatrixKeypadUpdate()
{
    static int numberOfHashKeyReleased = 0;
    char keyReleased = matrixKeypadUpdate();

    if (keyReleased != '\0') {
        printf("Key: %c, Siren: %d, Blocked: %d, Incorrect: %d, CodeChars: %d, ButtonActive: %d\n",
               keyReleased, sirenStateRead(), systemBlockedStateRead(),
               incorrectCodeStateRead(), numberOfCodeChars, buttonDisplayActive);

        // Handle incorrect code reset
        if (incorrectCodeStateRead() && keyReleased == '#') {
            numberOfHashKeyReleased++;
            if (numberOfHashKeyReleased >= 2) {
                numberOfHashKeyReleased = 0;
                numberOfCodeChars = 0;
                codeComplete = false;
                incorrectCodeState = OFF;
                memset(codeSequenceFromUserInterface, 0, CODE_NUMBER_OF_KEYS);
                displayCharPositionWrite(0, 3);
                displayStringWrite(sirenStateRead() ? "Enter Code:     " : "                ");
                printf("Incorrect code reset\n");
            }
            return;
        }

        // Process code entry
        if (keyReleased >= '0' && keyReleased <= '9' && !incorrectCodeStateRead()) {
            codeSequenceFromUserInterface[numberOfCodeChars] = keyReleased;
            displayCharPositionWrite(numberOfCodeChars, 3);
            displayStringWrite("*");
            numberOfCodeChars++;
            printf("Digit %d: %c, Code: %s\n", numberOfCodeChars, keyReleased, codeSequenceFromUserInterface);
            if (numberOfCodeChars >= CODE_NUMBER_OF_KEYS) {
                codeComplete = true;
                numberOfCodeChars = 0;
                displayCharPositionWrite(0, 3);
                displayStringWrite("Code Submitted  ");
                printf("Code submitted: %s, Siren before: %d\n", codeSequenceFromUserInterface, sirenStateRead());
                ThisThread::sleep_for(500);
                printf("Siren after: %d\n", sirenStateRead());
                displayCharPositionWrite(0, 3);
                displayStringWrite(sirenStateRead() ? "Enter Code:     " : "                ");
                buttonDisplayActive = false;
            }
            return;
        }

        // Handle detector state keys (only when alarm off and no code entry)
        if (!incorrectCodeStateRead() && numberOfCodeChars == 0 && !sirenStateRead()) {
            if (keyReleased == '2') {
                displayCharPositionWrite(0, 3);
                displayStringWrite(gasDetectorStateRead() ? "WARNING: Gas    " : "No Gas Detected ");
                printf("Gas state: %d, Displayed: %s\n", gasDetectorStateRead(),
                       gasDetectorStateRead() ? "WARNING: Gas" : "No Gas Detected");
                buttonDisplayActive = true;
                buttonDisplayTime = 0;
            } else if (keyReleased == '3') {
                displayCharPositionWrite(0, 3);
                displayStringWrite(temperatureSensorReadCelsius() > TEMPERATURE_THRESHOLD_C ?
                                   "Temp: Over Limit" : "Temp: Normal    ");
                printf("Temp state displayed, Temp: %.1f\n", temperatureSensorReadCelsius());
                buttonDisplayActive = true;
                buttonDisplayTime = 0;
            }
        }
    }
}

static void userInterfaceDisplayInit()
{
    displayInit(DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER);
    displayCharPositionWrite(0, 0);
    displayStringWrite("Enter Code to   ");
    displayCharPositionWrite(0, 1);
    displayStringWrite("Deactivate Alarm");
    codePromptDisplayed = true;
    printf("Display initialized\n");
}

static void userInterfaceDisplayUpdate()
{
    static int accumulatedDisplayTime = 0;
    char temperatureString[3] = "";

    if (accumulatedDisplayTime >= DISPLAY_REFRESH_TIME_MS) {
        accumulatedDisplayTime = 0;

        if (codePromptDisplayed && codePromptTime >= CODE_PROMPT_TIME_MS) {
            codePromptDisplayed = false;
            displayCharPositionWrite(0, 0);
            displayStringWrite("Temperature:    ");
            displayCharPositionWrite(0, 1);
            displayStringWrite("Gas:            ");
            displayCharPositionWrite(0, 2);
            displayStringWrite("Alarm:          ");
            printf("Switched to continuous display\n");
        }
        codePromptTime += DISPLAY_REFRESH_TIME_MS;

        if (!codePromptDisplayed) {
            sprintf(temperatureString, "%.0f", temperatureSensorReadCelsius());
            displayCharPositionWrite(12, 0);
            displayStringWrite(temperatureString);
            displayCharPositionWrite(14, 0);
            displayStringWrite("'C");

            displayCharPositionWrite(4, 1);
            displayStringWrite(gasDetectorStateRead() ? "WARNING: Gas    " : "No Gas Detected ");
            printf("Gas display: %s, GasState: %d\n",
                   gasDetectorStateRead() ? "WARNING: Gas" : "No Gas Detected",
                   gasDetectorStateRead());

            displayCharPositionWrite(6, 2);
            displayStringWrite(sirenStateRead() ? "ON " : "OFF");

            // Fourth line: code entry or "Enter Code:", no warnings
            if (numberOfCodeChars == 0 && !incorrectCodeStateRead() && !buttonDisplayActive) {
                if (sirenStateRead()) {
                    displayCharPositionWrite(0, 3);
                    displayStringWrite("Enter Code:     ");
                    printf("Displaying Enter Code, Siren: %d\n", sirenStateRead());
                } else {
                    displayCharPositionWrite(0, 3);
                    displayStringWrite("                ");
                    printf("Displaying blank line\n");
                }
            } else {
                printf("Fourth line blocked: CodeChars=%d, Incorrect=%d, ButtonActive=%d\n",
                       numberOfCodeChars, incorrectCodeStateRead(), buttonDisplayActive);
            }
        }

        if (buttonDisplayActive) {
            buttonDisplayTime += DISPLAY_REFRESH_TIME_MS;
            if (buttonDisplayTime >= BUTTON_DISPLAY_TIME_MS) {
                buttonDisplayActive = false;
                buttonDisplayTime = 0;
                printf("Button display reset\n");
            }
        }
    } else {
        accumulatedDisplayTime += SYSTEM_TIME_INCREMENT_MS;
    }
}

static void incorrectCodeIndicatorUpdate()
{
    incorrectCodeLed = incorrectCodeStateRead();
}

static void systemBlockedIndicatorUpdate()
{
    systemBlockedLed = systemBlockedState;
}