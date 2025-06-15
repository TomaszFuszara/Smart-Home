/**
 * @file server.ino
 * @brief ESP32 jako serwer HTTP do obsługi przycisków i odbierania danych z czujników (JSON lub GET).
 * 
 * Serwer działa w trybie Access Point, udostępniając interfejs webowy do sterowania urządzeniami
 * (światła, rolety, termostaty) oraz odbiera dane z czujników temperatury, wilgotności i jasności.
 * Obsługuje żądania HTTP POST z JSON-em oraz GET z parametrami.
 */

#include <WiFi.h>
#include <ArduinoJson.h>  // >>> MODYFIKACJA

/// SSID sieci WiFi Access Point
const char* ssid     = "ESP32-Network";
/// Hasło do sieci WiFi Access Point
const char* password = "Esp32-Password";

/// Obiekt serwera nasłuchujący na porcie 80
WiFiServer server(80);

/// Zmienna do przechowywania nagłówka HTTP żądania
String header;

// Stany logiczne przycisków (Buttonów)
String stateSypialniaSwiatlo1 =  "off";   ///< Stan światła 1 w sypialni
String stateSypialniaSwiatlo2 =  "off";   ///< Stan światła 2 w sypialni
String stateSypialniaTermostat = "off";   ///< Stan termostatu w sypialni
String stateSypialniaRolety =    "off";   ///< Stan rolet w sypialni

String stateSalonSwiatlo1 =      "off";   ///< Stan światła 1 w salonie
String stateSalonSwiatlo2 =      "off";   ///< Stan światła 2 w salonie
String stateSalonTermostat =     "off";   ///< Stan termostatu w salonie
String stateSalonRolety =        "off";   ///< Stan rolet w salonie

String stateUbikacjaSwiatlo1  =  "off";   ///< Stan światła 1 w ubikacji
String stateUbikacjaSwiatlo2  =  "off";   ///< Stan światła 2 w ubikacji
String statePrzedpokojSwiatlo =  "off";   ///< Stan światła w przedpokoju

// Dane z czujników
String tempSypialnia = "Brak";   ///< Temperatura w sypialni
String wilgSypialnia = "Brak";   ///< Wilgotność w sypialni
String jasnSypialnia = "Brak";   ///< Jasność w sypialni

String tempSalon = "Brak";       ///< Temperatura w salonie
String wilgSalon = "Brak";       ///< Wilgotność w salonie
String jasnSalon = "Brak";       ///< Jasność w salonie

/**
 * @brief Inicjalizacja ESP32 jako Access Point oraz uruchomienie serwera.
 */
void setup() {
  Serial.begin(115200);

  // Konfiguracja statycznego IP dla AP
  IPAddress local_IP(192, 168, 10, 1);
  IPAddress gateway(192, 168, 10, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);

  // Uruchomienie Access Point
  WiFi.softAP(ssid, password);

  // Start serwera HTTP
  server.begin();

  Serial.println("Serwer uruchomiony. IP: ");
  Serial.println(WiFi.softAPIP());
}

/**
 * @brief Główna pętla obsługująca połączenia klientów HTTP.
 */
void loop() {
  WiFiClient client = server.available();
  if (!client) return;  // jeśli brak klienta, wróć

  String currentLine = "";
  header = "";

  unsigned long timeout = millis() + 2000;  // timeout 2 sekundy

  // Odbiór nagłówka i przetwarzanie żądań klienta
  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      char c = client.read();
      header += c;

      if (c == '\n') {
        // Koniec nagłówka (pusta linia)
        if (currentLine.length() == 0) {
          // Obsługa POST /status do aktualizacji danych z czujników
          if (header.indexOf("POST /status") >= 0) {
            Serial.println("Odebrano POST /status");

            while (client.available() == 0) delay(1);

            String jsonBody = client.readString();
            Serial.println("JSON: " + jsonBody);

            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, jsonBody);

            if (!error) {
              JsonObject sypialnia = doc["status"]["sypialnia"];
              JsonObject salon = doc["status"]["salon"];

              // Odczyt wartości z JSON i aktualizacja zmiennych dla sypialni
              float t = sypialnia["temperatura"] | NAN;
              float h = sypialnia["wilgotnosc"] | NAN;
              float l = sypialnia["jasnosc"] | NAN;

              if (!isnan(t)) tempSypialnia = String(t, 1);
              if (!isnan(h)) wilgSypialnia = String(h, 1);
              if (!isnan(l)) jasnSypialnia = String(l, 1);

              Serial.println("Zaktualizowano dane z sypialni:");
              Serial.println("Temp: " + tempSypialnia);
              Serial.println("Wilg: " + wilgSypialnia);
              Serial.println("Jasn: " + jasnSypialnia);

              // Odczyt wartości z JSON i aktualizacja zmiennych dla salonu
              float t2 = salon["temperatura"] | NAN;
              float h2 = salon["wilgotnosc"] | NAN;
              float l2 = salon["jasnosc"] | NAN;
              
              if (!isnan(t2)) tempSalon = String(t2, 1);
              if (!isnan(h2)) wilgSalon = String(h2, 1);
              if (!isnan(l2)) jasnSalon = String(l2, 1);
            } else {
              Serial.print("Błąd parsowania JSON: ");
              Serial.println(error.c_str());
            }

            // Odpowiedź HTTP 200 OK
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println("Connection: close");
            client.println();
            client.println("OK");
            return;
          }

          // Obsługa GET /status - zwracanie statusów i aktualizacja danych
          if (header.indexOf("GET /status") >= 0) {
            int contentLength = 0;
            int clIndex = header.indexOf("Content-Length: ");
            if (clIndex != -1) {
              int endCl = header.indexOf("\r\n", clIndex);
              contentLength = header.substring(clIndex + 16, endCl).toInt();
            }

            String body = "";
            while (client.available() < contentLength) delay(1);
            for (int i = 0; i < contentLength; i++) body += (char)client.read();

            // Parsowanie parametrów z ciała zapytania
            int i1 = body.indexOf("temp=");
            int i2 = body.indexOf("&humidity=");
            int i3 = body.indexOf("&light=");
            int idIndex = body.indexOf("&id=");

            if (i1 >= 0 && i2 > i1 && i3 > i2 && idIndex > i3) {
              String temp = body.substring(i1 + 5, i2);
              String hum = body.substring(i2 + 10, i3);
              String light = body.substring(i3 + 7, idIndex);
              String id = body.substring(idIndex + 4);

              // Aktualizacja danych w zależności od lokalizacji
              if (id == "sypialnia") {
                tempSypialnia = temp;
                wilgSypialnia = hum;
                jasnSypialnia = light;
              } else if (id == "salon") {
                tempSalon = temp;
                wilgSalon = hum;
                jasnSalon = light;
              }
            }

            // Odpowiedź HTTP z aktualnymi stanami
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/plain");
            client.println("Connection: close");
            client.println();

            String status = "";
            status += stateSypialniaSwiatlo1 == "on" ? "1" : "0"; status += ",";
            status += stateSypialniaSwiatlo2 == "on" ? "1" : "0"; status += ",";
            status += stateSypialniaTermostat == "open" ? "1" : "0"; status += ",";
            status += stateSypialniaRolety == "open" ? "1" : "0"; status += ",";

            status += stateSalonSwiatlo1 == "on" ? "1" : "0"; status += ",";
            status += stateSalonSwiatlo2 == "on" ? "1" : "0"; status += ",";
            status += stateSalonTermostat == "open" ? "1" : "0"; status += ",";
            status += stateSalonRolety == "open" ? "1" : "0"; status += ",";

            status += stateUbikacjaSwiatlo1 == "on" ? "1" : "0"; status += ",";
            status += stateUbikacjaSwiatlo2 == "on" ? "1" : "0"; status += ",";
            status += statePrzedpokojSwiatlo == "on" ? "1" : "0"; status += ",";

            client.println(status);
            client.println();
            break;
          }

          // Obsługa przycisków i generowanie strony HTML sterowania
          handleButton(header, "/SypialniaSwiatlo1",   stateSypialniaSwiatlo1);
          handleButton(header, "/SypialniaSwiatlo2",   stateSypialniaSwiatlo2);
          handleDualButton(header, "/SypialniaRolety", stateSypialniaRolety);
          handleDualButton(header, "/SypialniaTermostat", stateSypialniaTermostat);
          handleButton(header, "/SalonSwiatlo1",       stateSalonSwiatlo1);
          handleButton(header, "/SalonSwiatlo2",       stateSalonSwiatlo2);
          handleDualButton(header, "/SalonRolety",     stateSalonRolety);
          handleDualButton(header, "/SalonTermostat",  stateSalonTermostat);
          handleButton(header, "/UbikacjaSwiatlo1",    stateUbikacjaSwiatlo1);
          handleButton(header, "/UbikacjaSwiatlo2",    stateUbikacjaSwiatlo2);
          handleButton(header, "/PrzedpokojSwiatlo",   statePrzedpokojSwiatlo);

          // Wysłanie nagłówków HTTP oraz zawartości HTML
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();

          client.println("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
          client.println("<style>body{font-family:monospace;text-align:center;}button{padding:12px;font-size:20px;margin:4px;}</style></head><body>");
          client.println("<h3>Dane z czujnikow</h3>");
          client.println("<p><strong>Sypialnia</strong>: Tempatura: " + tempSypialnia + " &deg;C, Wilgotnosc: " + wilgSypialnia + " %, Jasnosc: " + jasnSypialnia + " lux </p>");
          client.println("<p><strong>Salon</strong>: Tempatura: " + tempSalon + " &deg;C, Wilgotnosc: " + wilgSalon + " %, Jasnosc: " + jasnSalon + " lux </p>");
          client.println("<h2>Panel sterowania</h2>");

          // Przyciski do sterowania poszczególnymi elementami
          printButton(client, "SypialniaSwiatlo1",  stateSypialniaSwiatlo1, "/SypialniaSwiatlo1");
          printButton(client, "SypialniaSwiatlo2",  stateSypialniaSwiatlo2, "/SypialniaSwiatlo2");
          printDualButton(client, "SypialniaTermostat", stateSypialniaTermostat, "/SypialniaTermostat", "Otworz", "Zamknij");
          printDualButton(client, "SypialniaRolety", stateSypialniaRolety, "/SypialniaRolety", "Otworz", "Zamknij");
          printButton(client, "SalonSwiatlo1",      stateSalonSwiatlo1,     "/SalonSwiatlo1");
          printButton(client, "SalonSwiatlo2",      stateSalonSwiatlo2,     "/SalonSwiatlo2");
          printDualButton(client, "SalonTermostat", stateSalonTermostat, "/SalonTermostat", "Otworz", "Zamknij");
          printDualButton(client, "SalonRolety", stateSalonRolety, "/SalonRolety", "Otworz", "Zamknij");
          printButton(client, "UbikacjaSwiatlo1",   stateUbikacjaSwiatlo1,  "/UbikacjaSwiatlo1");
          printButton(client, "UbikacjaSwiatlo2",   stateUbikacjaSwiatlo2,  "/UbikacjaSwiatlo2");
          printButton(client, "PrzedpokojSwiatlo",  statePrzedpokojSwiatlo, "/PrzedpokojSwiatlo");

          client.println("</body></html>");
          break;
        } else {
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }

  // Zakończenie połączenia i czyszczenie nagłówka
  client.stop();
  header = "";
}

/**
 * @brief Obsługa prostych przycisków ON/OFF.
 * 
 * @param header Nagłówek HTTP żądania.
 * @param path Ścieżka do przycisku.
 * @param state Zmienna przechowująca stan przycisku (modyfikowana).
 */
void handleButton(const String& header, const String& path, String& state) {
  if (header.indexOf("GET " + path + "/on") >= 0) state = "on";
  else if (header.indexOf("GET " + path + "/off") >= 0) state = "off";
}

/**
 * @brief Obsługa przycisków o dwóch stanach (np. otwórz/zamknij).
 * 
 * @param header Nagłówek HTTP żądania.
 * @param path Ścieżka do przycisku.
 * @param state Zmienna przechowująca stan przycisku (modyfikowana).
 */
void handleDualButton(const String& header, const String& path, String& state) {
  if (header.indexOf("GET " + path + "/open") >= 0) state = "open";
  else if (header.indexOf("GET " + path + "/close") >= 0) state = "close";
}

/**
 * @brief Generuje HTML dla przycisku ON/OFF i wysyła do klienta.
 * 
 * @param client Obiekt klienta WiFi.
 * @param name Nazwa przycisku.
 * @param state Stan przycisku ("on" lub "off").
 * @param path Ścieżka akcji przycisku.
 */
void printButton(WiFiClient& client, const String& name, const String& state, const String& path) {
  client.print("<p>" + name + ": " + state + "</p>");
  if (state == "off") client.print("<a href='" + path + "/on'><button style='background:green;color:white'>WL</button></a>");
  else client.print("<a href='" + path + "/off'><button style='background:red;color:white'>WYL</button></a>");
}

/**
 * @brief Generuje HTML dla przycisków z dwoma stanami i wysyła do klienta.
 * 
 * @param client Obiekt klienta WiFi.
 * @param name Nazwa przycisku.
 * @param state Stan przycisku ("open" lub "close").
 * @param path Ścieżka akcji przycisku.
 * @param labelOn Etykieta przycisku "otwórz".
 * @param labelOff Etykieta przycisku "zamknij".
 */
void printDualButton(WiFiClient& client, const String& name, const String& state, const String& path, const String& labelOn, const String& labelOff) {
  client.print("<p>" + name + ": " + state + "</p>");
  client.print("<a href='" + path + "/open'><button style='background:green;color:white'>" + labelOn + "</button></a>");
  client.print("<a href='" + path + "/close'><button style='background:red;color:white'>" + labelOff + "</button></a>");
}
