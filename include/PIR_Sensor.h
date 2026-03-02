#ifndef PIR_SENSOR_H
#define PIR_SENSOR_H

#include "GPIO_Registers.h"

class PIRSensor {
private:
    int _pin;
public:
    PIRSensor(int pin) : _pin(pin) {}

    void init() {
        uint32_t pin_reg = GPIO_PIN_MUX_REG[_pin];
        PIN_FUNC_SELECT(pin_reg, PIN_FUNC_GPIO);
        
        // Activare Input Enable, Pull-Down si dezactivare Pull-Up
        REG_SET_BIT(pin_reg, FUN_IE);
        REG_SET_BIT(pin_reg, FUN_PD);
        REG_CLR_BIT(pin_reg, FUN_PU);

        // Setare directie INPUT in registrul ENABLE
        if (IS_BANK_1(_pin)) REG_WRITE(GPIO_ENABLE1_W1TC_REG, (1UL << GET_BIT_POS(_pin)));
        else REG_WRITE(GPIO_ENABLE_W1TC_REG, (1UL << GET_BIT_POS(_pin)));
    }

    bool isTriggered() {
        return reg_read(_pin) == 1; // Citire directa din registru
    }
};

#endif