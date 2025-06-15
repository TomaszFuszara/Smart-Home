/**
 * @file Sy[pialnia.ino
 * @brief Sterowanie oświetleniem Sypialni z poziomu serwera i pobieranie danych pomiarowych z czujników.
 * 
 * Po połączeniu z WiFi urządzenie ESP32 cyklicznie pobiera status oświetlenia z serwera
 * i steruje odpowiednimi pinami GPIO w zależności od odebranych danych oraz odbiera dane pomiarowe z czujników zapisane w pliku JSON.
 */

#include <ESP32Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h> 
#include <DHT.h>
#include <LightDependentResistor.h>
#include <Stepper.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Serwo i czujniki
Servo serwomechanizm;
OneWire oneWire(5); // A5
DallasTemperature sensors(&oneWire);
DeviceAddress T = {0x28, 0x35, 0x5B, 0x7B, 0x0F, 0x00, 0x00, 0x9E};
DHT dht(4, DHT11);

#define OTHER_RESISTOR 10000
#define USED_PIN 35
#define USED_PHOTOCELL LightDependentResistor::GL5537_2
LightDependentResistor photocell(USED_PIN, OTHER_RESISTOR, USED_PHOTOCELL, 10, 10);

// Silnik krokowy
#define Kroki 2048
Stepper SilnikTermostat(Kroki, 13, 26, 27, 25);

// WiFi i serwer
const char* ssid = "ESP32-Network";
const char* password = "Esp32-Password";
const char* serverIP = "192.168.10.1";

// Wyjścia
const int pinSypialnia1 = 15;
const int pinSypialnia2 = 32;
const int pinRolety1 = 14;
const int pinRolety2 = 33;

unsigned long lastCheck = 0;
const unsigned long interval = 1000; // 1s

int sypialniaTermostatStan;
int poprzedniStanTermostatu = 2; //2 to nieużywana wartość różna od 0 i różna od 1

int sypialniaRoletyStan;
int poprzedniStanRolet = 2; //2 to nieużywana wartość różna od 0 i różna od 1

/**
 * @brief Inicjalizuje komponenty systemu: GPIO, czujniki, WiFi.
 */
void setup() {
  Serial.begin(115200);

  // GPIO
  pinMode(pinSypialnia1, OUTPUT);
  pinMode(pinSypialnia2, OUTPUT);

  pinMode(pinRolety1, OUTPUT);
  pinMode(pinRolety2, OUTPUT);
  digitalWrite(pinRolety1, LOW);
  digitalWrite(pinRolety2, LOW);

  // Czujniki
  sensors.begin();
  dht.begin();

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nPołączono z WiFi");
}

/**
 * @brief Główna pętla programu. Czyta dane z czujników i synchronizuje stan z serwerem.
 */
void loop() {
  if (millis() - lastCheck >= interval) {
    lastCheck = millis();
    checkServerStatus();
  }

  sensors.requestTemperatures();
  float tempSypialnia = sensors.getTempC(T);
  Serial.print("Temperatura: ");
  Serial.println(tempSypialnia);

  float wilgSypialnia = dht.readHumidity();
  Serial.print("Wilgotnosc: ");
  Serial.println(wilgSypialnia);

  float jasnSypialnia = photocell.getCurrentLux();
  Serial.print("Jasnosc: ");
  Serial.println(jasnSypialnia);

  sendSensorData(tempSypialnia, wilgSypialnia, jasnSypialnia);
}

/**
 * @brief Pobiera status z serwera i steruje urządzeniami (termoregulator, rolety, światła).
 *
 * Odczytuje dane z serwera HTTP i w zależności od ich zawartości steruje podłączonymi urządzeniami.
 */
void checkServerStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + serverIP + "/status";
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Status z serwera: " + payload);

      int states[11];
      int fromIndex = 0;
      for (int i = 0; i < 11; i++) {
        int commaIndex = payload.indexOf(',', fromIndex);
        if (commaIndex == -1) commaIndex = payload.length();
        String val = payload.substring(fromIndex, commaIndex);
        states[i] = val.toInt();
        fromIndex = commaIndex + 1;
      }

      digitalWrite(pinSypialnia1, states[0] ? HIGH : LOW);
      digitalWrite(pinSypialnia2, states[1] ? HIGH : LOW);

      sypialniaTermostatStan = states[2];
      if(sypialniaTermostatStan == 0 && poprzedniStanTermostatu != 0) {
        SilnikTermostat.setSpeed(10);
        SilnikTermostat.step(-1024);
        digitalWrite(13, LOW);
        digitalWrite(26, LOW);
        digitalWrite(27, LOW);
        digitalWrite(25, LOW);
      }

      if(sypialniaTermostatStan == 1 && poprzedniStanTermostatu != 1) {
        SilnikTermostat.setSpeed(10);
        SilnikTermostat.step(1024);
        digitalWrite(13, LOW);
        digitalWrite(26, LOW);
        digitalWrite(27, LOW);
        digitalWrite(25, LOW);
      }

      poprzedniStanTermostatu = sypialniaTermostatStan;

      sypialniaRoletyStan = states[3];
      if(sypialniaRoletyStan == 0 && poprzedniStanRolet != 0) {
        digitalWrite(pinRolety1, HIGH);
        digitalWrite(pinRolety2, LOW);
        delay(6000);
        digitalWrite(pinRolety1, LOW);
        digitalWrite(pinRolety2, LOW);
      }

      if(sypialniaRoletyStan == 1 && poprzedniStanRolet != 1) {
        digitalWrite(pinRolety1, LOW);
        digitalWrite(pinRolety2, HIGH);
        delay(6000);
        digitalWrite(pinRolety1, LOW);
        digitalWrite(pinRolety2, LOW);
      }

      poprzedniStanRolet = sypialniaRoletyStan;

    } else {
      Serial.printf("Błąd HTTP: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("Brak połączenia WiFi");
  }
}

/**
 * @brief Wysyła dane z czujników do serwera w formacie JSON.
 *
 * Wysyła temperaturę, wilgotność i jasność z sypialni do endpointu HTTP `/status`.
 *
 * @param tempSypialnia Temperatura odczytana z czujnika DS18B20
 * @param wilgSypialnia Wilgotność z czujnika DHT11 
 * @param jasnSypialnia Jasność w luksach z czujnika LDR
 */
void sendSensorData(float tempSypialnia, float wilgSypialnia, float jasnSypialnia) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + serverIP + "/status";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"status\":{";
    json += "\"sypialnia\":{";
    json += "\"temperatura\":" + String(tempSypialnia, 1) + ",";
    json += "\"wilgotnosc\":" + String(wilgSypialnia, 1) + ",";
    json += "\"jasnosc\":" + String(jasnSypialnia, 1);
    json += "}}}";

    int httpCode = http.POST(json);
    if (httpCode > 0) {
      Serial.println("Dane JSON wysłane: " + json);
    } else {
      Serial.println("Błąd wysyłania JSON-a");
    }
    http.end();
  }
}
