/**
 * @file przedpokoj.ino
 * @brief Sterowanie oświetleniem Salonu, Ubikacji i Przedpokoju z poziomu serwera.
 * 
 * Po połączeniu z WiFi urządzenie ESP32 cyklicznie pobiera status oświetlenia z serwera
 * i steruje odpowiednimi pinami GPIO w zależności od odebranych danych.
 */

#include <WiFi.h>
#include <HTTPClient.h>

/// @brief Nazwa sieci WiFi
const char* ssid = "ESP32-Network";

/// @brief Hasło do sieci WiFi
const char* password = "Esp32-Password";

/// @brief Adres IP serwera, z którego pobierane są statusy urządzeń
const char* serverIP = "192.168.10.1";

/// @brief Pin odpowiadający za światło nr 1 w salonie
const int pinSalonSwiatlo1 = 25;

/// @brief Pin odpowiadający za światło nr 2 w salonie
const int pinSalonSwiatlo2 = 26;

/// @brief Pin odpowiadający za światło nr 1 w ubikacji
const int pinUbikacjaSwiatlo1 = 27;

/// @brief Pin odpowiadający za światło nr 2 w ubikacji
const int pinUbikacjaSwiatlo2 = 32;

/// @brief Pin odpowiadający za światło w przedpokoju
const int pinPrzedpokojSwiatlo = 33;

/**
 * @brief Inicjalizacja WiFi oraz konfiguracja pinów GPIO jako wyjścia.
 */
void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nPołączono z WiFi");

  pinMode(pinSalonSwiatlo1, OUTPUT);
  pinMode(pinSalonSwiatlo2, OUTPUT);
  pinMode(pinUbikacjaSwiatlo1, OUTPUT);
  pinMode(pinUbikacjaSwiatlo2, OUTPUT);
  pinMode(pinPrzedpokojSwiatlo, OUTPUT);
}

/**
 * @brief Główna pętla programu.
 * 
 * Co 1 sekundę pobiera dane z serwera i steruje oświetleniem w różnych pomieszczeniach.
 */
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + serverIP + "/status";
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Odpowiedź serwera: " + payload);

      // Rozdzielenie danych po przecinkach
      int states[11];
      int fromIndex = 0;
      for (int i = 0; i < 11; i++) {
        int commaIndex = payload.indexOf(',', fromIndex);
        if (commaIndex == -1) commaIndex = payload.length();
        String val = payload.substring(fromIndex, commaIndex);
        states[i] = val.toInt();
        fromIndex = commaIndex + 1;
      }

      // Sterowanie światłem na podstawie odebranych stanów
      digitalWrite(pinSalonSwiatlo1,     states[4]  ? HIGH : LOW);
      digitalWrite(pinSalonSwiatlo2,     states[5]  ? HIGH : LOW);
      digitalWrite(pinUbikacjaSwiatlo1,  states[8]  ? HIGH : LOW);
      digitalWrite(pinUbikacjaSwiatlo2,  states[9]  ? HIGH : LOW);
      digitalWrite(pinPrzedpokojSwiatlo, states[10] ? HIGH : LOW);

    } else {
      Serial.printf("Błąd HTTP: %d\n", httpCode);
    }

    http.end();
  } else {
    Serial.println("Brak połączenia WiFi");
  }

  delay(1000);
}
