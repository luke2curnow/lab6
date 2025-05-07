#include "mbed.h"

//=====[Declaration of private defines]========================================

//=====[Declaration and initialization of public global objects]===============

AnalogIn mq2(A2); // MQ-2 sensor on analog pin A2

//=====[Declaration and initialization of public global variables]=============

//=====[Implementations of public functions]===================================

void gasSensorInit()
{
    // Optional: Add initialization code if needed
}

void gasSensorUpdate()
{
    // Optional: Add periodic updates if needed
}

float gasSensorReadVoltage()
{
    float voltage = mq2.read() * 3.3; // Convert to volts (3.3V reference for NUCLEO-F439ZI)
    return voltage;
}