#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include <ArduinoJson.h>

// drivere custom
#include "PIR_Sensor.h"
#include "GPIO_Registers.h" 
#include "Radar_Driver.h" 
#include "LCD_Driver.h"

// --- CONFIGURARE PINI ---
#define PIN_PIR     47
#define PIN_BUZZER  42
#define PIN_LED     21
#define RADAR_RX    41 
#define RADAR_TX    40
#define I2C_SDA     1
#define I2C_SCL     2

// Pinii camerei
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM   4
#define SIOC_GPIO_NUM   5
#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM     8
#define Y3_GPIO_NUM     9
#define Y2_GPIO_NUM    11
#define VSYNC_GPIO_NUM  6
#define HREF_GPIO_NUM   7
#define PCLK_GPIO_NUM  13

// Setari Server // Schimba cu IP-ul local al PC-ului tau (unde ruleaza serverul Python)
const char* WIFI_SSID = "YOUR_WIFI_SSID_HERE";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD_HERE";
const char* SERVER_URL    = "http://your-local-ipv4:8000"; 

// --- OBIECTE ---
PIRSensor pirSensor(PIN_PIR);

// LCD pe Registri (Bit-Banging)
LCD_I2C_Raw lcd(I2C_SDA, I2C_SCL); 

// Radar pe Registri
RadarLD2410 radar(RADAR_RX, RADAR_TX);

bool isArmed = false;
unsigned long lastPoll = 0;

void updateLCD(String l1, String l2) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(l1);
    lcd.setCursor(0, 1); lcd.print(l2);
}

bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    return esp_camera_init(&config) == ESP_OK;
}

void sendAlert() {
    reg_write_high(PIN_LED);
    updateLCD("!! ALERTA !!", "Trimitere foto...");
    
    camera_fb_t * fb = esp_camera_fb_get();
    if(fb) {
        HTTPClient http;
        http.begin(String(SERVER_URL) + "/upload");
        http.addHeader("Content-Type", "image/jpeg");
        int res = http.POST(fb->buf, fb->len);
        if(res > 0) {
            reg_write_high(PIN_BUZZER); delay(500); reg_write_low(PIN_BUZZER);
        }
        esp_camera_fb_return(fb);
        http.end();
    }
    reg_write_low(PIN_LED);
}

void setup() {
    Serial.begin(115200);
    
    lcd.init(); 
    updateLCD("Conectare WiFi", "...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        retry++;
        if (retry > 20) {
            Serial.println("\nEroare WiFi!");
            updateLCD("Eroare WiFi", "Verifica Parola");
            break; 
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Conectat!");
        updateLCD("WiFi OK", WiFi.localIP().toString());
    }
    
    radar.init();
    Serial.println("Sistem Radar (Registri) Pregatit!");

    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED, OUTPUT);
    
    pirSensor.init();
    
    if (initCamera()) {
        Serial.println("Camera OK");
    } else {
        Serial.println("Eroare Camera!");
        updateLCD("Eroare", "Camera");
    }

    delay(2000);
    updateLCD("Sistem Ready", "Astept Comenzi");
}

void loop() {
    radar.actualizeaza();

    // Polling Server
    if (millis() - lastPoll > 2000) { 
        HTTPClient http;
        http.begin(String(SERVER_URL) + "/status");
        
        int httpCode = http.GET();
        if (httpCode == 200) {
            DynamicJsonDocument doc(256);
            deserializeJson(doc, http.getString());
            bool serverArmed = doc["armed"];

            if (serverArmed != isArmed) { 
                isArmed = serverArmed;
                
                if (isArmed) {
                    updateLCD("STATUS: ARMAT", "Sistem veghe");
                    reg_write_high(PIN_BUZZER); delay(200); reg_write_low(PIN_BUZZER);
                    Serial.println("ARMAT");
                } else {
                    updateLCD("STATUS: DEZARMAT", "Sistem inactiv");
                    reg_write_high(PIN_BUZZER); delay(100); reg_write_low(PIN_BUZZER);
                    delay(50);
                    reg_write_high(PIN_BUZZER); delay(100); reg_write_low(PIN_BUZZER);
                    Serial.println("DEZARMAT");
                }
            }
        }
        http.end();
        lastPoll = millis();
    }

    // Logica Alarma
    if (isArmed) {
        int pirState = reg_read(PIN_PIR);
        bool radarState = (radar.stare != 0);
        
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 1000) {
            Serial.printf("PIR: %d | RADAR: %d\n", pirState, radar.stare);
            lastDebug = millis();
        }

        if (pirState == HIGH && radarState == true) {
            Serial.println("!!! DETECTIE DUBLA !!!");
            sendAlert();
            delay(5000);
            updateLCD("STATUS: ARMAT", "Supraveghere...");
        }
    }
}