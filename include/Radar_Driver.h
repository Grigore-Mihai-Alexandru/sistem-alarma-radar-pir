#ifndef RADAR_DRIVER_H
#define RADAR_DRIVER_H

#include <Arduino.h>
#include "soc/uart_reg.h" // Librarie Espressif pentru adrese registri

// Clasa care gestioneaza tot radarul
class RadarLD2410 {
private:
    // Pinii fizici
    int _rxPin, _txPin;
    
    // Variabile interne pentru procesarea datelor
    uint8_t buffer[50];
    int bufIndex = 0;
    bool headerFound = false;

    // --- FUNCTII PRIVATE (Motorul pe registri) ---

    // Citeste un byte direct din memoria UART1
    uint8_t readReg() {
        return (uint8_t)REG_READ(UART_FIFO_REG(1));
    }

    // Verifica cati bytes asteapta in memoria UART1
    int availableReg() {
        return (REG_READ(UART_STATUS_REG(1)) & 0x3FF);
    }

    // Scrie bytes direct in memoria de iesire UART1
    void writeReg(const uint8_t* data, int len) {
        for (int i = 0; i < len; i++) {
            // Asteapta sa fie loc in coada de iesire
            while (((REG_READ(UART_STATUS_REG(1)) >> 16) & 0xFF) >= 126);
            // Scrie byte-ul
            REG_WRITE(UART_FIFO_REG(1), data[i]);
        }
    }

public:
    
    uint8_t stare = 0;      // 0=Nimic, 1=Miscare, 2=Static, 3=Ambele
    uint16_t distMisc = 0;  // Distanta tinta in miscare
    uint16_t distStat = 0;  // Distanta tinta statica

    RadarLD2410(int rx, int tx) {
        _rxPin = rx;
        _txPin = tx;
    }

    void init() {
        Serial1.begin(256000, SERIAL_8N1, _rxPin, _txPin);
        delay(200); 

        // De acum folosim doar registri. Facem o calibrare de test.
        calibreaza();
    }

    // Functia de calibrare (scade sensibilitatea sa nu prinda "fantome")
    void calibreaza() {
        // 1. Activare configurare
        const uint8_t cmd_enable[] = {0xFD,0xFC,0xFB,0xFA,0x04,0x00,0xFF,0x00,0x01,0x00,0x04,0x03,0x02,0x01};
        writeReg(cmd_enable, sizeof(cmd_enable));
        delay(50);

        // 2. Setare Sensibilitate (Poarta 2 -> Valoare 45)
        uint8_t cmd_sens[] = {
            0xFD,0xFC,0xFB,0xFA, 0x14,0x00, 0x64,0x00, 0x00,0x00, 
            0x02,0x00,0x00,0x00, 
            0x01,0x00, 0x2D,0x00,0x00,0x00, // 45 Miscare
            0x02,0x00, 0x2D,0x00,0x00,0x00, // 45 Static
            0x04,0x03,0x02,0x01
        };
        writeReg(cmd_sens, sizeof(cmd_sens));
        delay(50);

        // 3. Iesire configurare
        const uint8_t cmd_end[] = {0xFD,0xFC,0xFB,0xFA,0x02,0x00,0xFE,0x00,0x04,0x03,0x02,0x01};
        writeReg(cmd_end, sizeof(cmd_end));
    }

    // Functia principala: Citeste si descifreaza datele (pusa in loop)
    void actualizeaza() {
        // Citim cat timp avem date in registru
        while (availableReg() > 0) {
            uint8_t b = readReg();

            // Cautam secventa de start F4 F3 F2 F1
            if (!headerFound) {
                buffer[0] = buffer[1]; buffer[1] = buffer[2]; buffer[2] = buffer[3]; buffer[3] = b;
                if (buffer[0] == 0xF4 && buffer[1] == 0xF3 && buffer[2] == 0xF2 && buffer[3] == 0xF1) {
                    headerFound = true;
                    bufIndex = 0;
                }
            } 
            else {
                // Adunam datele in buffer
                buffer[bufIndex++] = b;
                
                // Cand avem tot pachetul (18 bytes), extragem numerele
                if (bufIndex >= 18) {
                    stare = buffer[4];
                    distMisc = buffer[5] | (buffer[6] << 8);
                    distStat = buffer[8] | (buffer[9] << 8);
                    
                    // Reset pentru urmatorul pachet
                    headerFound = false;
                    bufIndex = 0;
                }
            }
        }
    }
};

#endif