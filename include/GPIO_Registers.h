#ifndef GPIO_REGISTERS_H
#define GPIO_REGISTERS_H

#include <Arduino.h>
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"

// Macro pentru a verifica daca un pin este in Bank 0 sau Bank 1
#define IS_BANK_1(pin) (pin >= 32)
#define GET_BIT_POS(pin) (IS_BANK_1(pin) ? (pin - 32) : pin)

// Functii inline pentru viteza maxima in RAM // IRAM = instruction RAM
inline void IRAM_ATTR reg_write_high(int pin) { 
    if (IS_BANK_1(pin)) REG_WRITE(GPIO_OUT1_W1TS_REG, (1UL << GET_BIT_POS(pin)));
    else REG_WRITE(GPIO_OUT_W1TS_REG, (1UL << GET_BIT_POS(pin)));
}

inline void IRAM_ATTR reg_write_low(int pin) {
    if (IS_BANK_1(pin)) REG_WRITE(GPIO_OUT1_W1TC_REG, (1UL << GET_BIT_POS(pin)));
    else REG_WRITE(GPIO_OUT_W1TC_REG, (1UL << GET_BIT_POS(pin)));
}

inline int IRAM_ATTR reg_read(int pin) {
    if (IS_BANK_1(pin)) return (REG_READ(GPIO_IN1_REG) >> GET_BIT_POS(pin)) & 1;
    else return (REG_READ(GPIO_IN_REG) >> GET_BIT_POS(pin)) & 1;
}

#endif