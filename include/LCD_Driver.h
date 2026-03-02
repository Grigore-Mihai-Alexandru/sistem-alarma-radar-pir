#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <Arduino.h>
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"

// Adresa standard pentru LCD-uri (sau 0x3F)
#define LCD_ADDR 0x27 

// Bitii pentru PCF8574 (cipul de pe spatele LCD-ului)
// P0=RS, P1=RW, P2=EN, P3=Backlight, P4-P7=Data
#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_EN 0x04
#define LCD_BL 0x08

class LCD_I2C_Raw {
private:
    int _sda, _scl;
    uint8_t _backlightVal = LCD_BL; // Lumina pornita implicit

    // --- SECTIUNEA I2C RAW (Bit-Banging pe Registri) ---

    // Seteaza pinul ca OUTPUT OPEN-DRAIN (specific I2C)
    void configPin(int pin) {
        // 1. Setam functia GPIO (IO_MUX)
        // Adresa base 0x60009000 + offset pin
        volatile uint32_t* mux = (volatile uint32_t*)(0x60009000 + (pin * 4));
        *mux = (1 << 12) | (1 << 9) | (1 << 8); // GPIO | Input | PullUp

        // 2. Activam Iesirea (GPIO_ENABLE_W1TS)
        if (pin < 32) REG_WRITE(GPIO_ENABLE_W1TS_REG, (1 << pin));
        else          REG_WRITE(GPIO_ENABLE1_W1TS_REG, (1 << (pin - 32)));

        // 3. Setam Open-Drain (GPIO_PIN_PAD_DRIVER)
        // Asta lasa pinul sa fie tras la masa, dar nu il forteaza la 3.3V (trage doar rezistenta)
        volatile uint32_t* pin_reg = (volatile uint32_t*)(0x60004088 + (pin * 4));
        *pin_reg |= (1 << 2); // Bit 2 = Pad Driver Open Drain
    }

    // Trage linia la LOW (0V)
    void sda_low() { 
        if (_sda < 32) REG_WRITE(GPIO_OUT_W1TC_REG, (1 << _sda));
        else           REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (_sda - 32)));
    }
    void scl_low() { 
        if (_scl < 32) REG_WRITE(GPIO_OUT_W1TC_REG, (1 << _scl));
        else           REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (_scl - 32)));
    }

    // Elibereaza linia la HIGH (3.3V prin rezistenta)
    void sda_high() { 
        if (_sda < 32) REG_WRITE(GPIO_OUT_W1TS_REG, (1 << _sda));
        else           REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (_sda - 32)));
    }
    void scl_high() { 
        if (_scl < 32) REG_WRITE(GPIO_OUT_W1TS_REG, (1 << _scl));
        else           REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (_scl - 32)));
    }

    // Semnal START I2C: SDA cade in timp ce SCL e sus
    void i2c_start() {
        sda_high(); scl_high(); delayMicroseconds(2);
        sda_low();  delayMicroseconds(2);
        scl_low();
    }

    // Semnal STOP I2C: SDA urca in timp ce SCL e sus
    void i2c_stop() {
        sda_low(); scl_high(); delayMicroseconds(2);
        sda_high(); delayMicroseconds(2);
    }

    // Scrie 1 Byte pe I2C (bit cu bit)
    bool i2c_writeByte(uint8_t data) {
        for (int i = 0; i < 8; i++) {
            if (data & 0x80) sda_high(); else sda_low();
            data <<= 1;
            delayMicroseconds(1);
            scl_high(); delayMicroseconds(2); // Clock Sus (Slave citeste)
            scl_low();  delayMicroseconds(1); // Clock Jos
        }
        
        // ACK (Ignoram citirea ACK pentru simplitate la LCD, doar dam un puls de ceas)
        sda_high(); // Eliberam SDA
        scl_high(); delayMicroseconds(2);
        scl_low();
        return true;
    }

    // Functie care trimite pachetul I2C complet: Start -> Adresa -> Date -> Stop
    void writePCF8574(uint8_t data) {
        i2c_start();
        i2c_writeByte(LCD_ADDR << 1); // Adresa + Write(0)
        i2c_writeByte(data | _backlightVal); // Datele + Lumina
        i2c_stop();
    }

    // Trimite 4 biti catre LCD
    void sendNibble(uint8_t nibble, uint8_t mode) {
        uint8_t data = (nibble & 0xF0) | mode | _backlightVal;
        
        // Pulsam pinul ENABLE (P2)
        writePCF8574(data);          // Pregatim datele
        writePCF8574(data | LCD_EN); // EN = 1
        delayMicroseconds(1);
        writePCF8574(data & ~LCD_EN);// EN = 0
        delayMicroseconds(50);
    }

    // Trimite un byte intreg (spart in 2 bucati de 4 biti)
    void send(uint8_t value, uint8_t mode) {
        sendNibble(value & 0xF0, mode);      // Cei 4 biti de sus
        sendNibble((value << 4) & 0xF0, mode); // Cei 4 biti de jos
    }

public:
    LCD_I2C_Raw(int sda, int scl) {
        _sda = sda;
        _scl = scl;
    }

    // Initializare LCD (Secventa magica din datasheet HD44780)
    void init() {
        configPin(_sda);
        configPin(_scl);
        
        // Stare initiala I2C
        sda_high(); scl_high();
        delay(50);

        // Secventa de resetare 4-bit
        sendNibble(0x30, 0); delay(5);
        sendNibble(0x30, 0); delay(5);
        sendNibble(0x30, 0); delay(1);
        sendNibble(0x20, 0); // Setare 4-bit mode

        // Configurare functionala
        command(0x28); // Function set: 2 linii, 5x8 font
        command(0x0C); // Display on, cursor off
        command(0x06); // Entry mode: increment
        clear();
    }

    // Trimite comanda (ex: Clear, Home)
    void command(uint8_t cmd) {
        send(cmd, 0);
    }

    // Sterge ecranul
    void clear() {
        command(0x01);
        delay(2);
    }

    // Muta cursorul (col, row)
    void setCursor(uint8_t col, uint8_t row) {
        int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
        command(0x80 | (col + row_offsets[row]));
    }

    // Scrie un text
    void print(String str) {
        for (int i = 0; i < str.length(); i++) {
            send(str[i], LCD_RS); // RS=1 inseamna Caracter
        }
    }
    
    // Scrie un numar
    void print(int num) {
        print(String(num));
    }
    
    // Aprinde/Stinge lumina
    void backlight(bool state) {
        _backlightVal = state ? LCD_BL : 0;
        writePCF8574(0); // Actualizam doar lumina
    }
};

#endif