# SmartTank HMI

SmartTank HMI to panel do monitorowania poziomu szamba i studni oraz diagnostyki instalacji. Projekt działa na Waveshare ESP32-S3 Touch LCD 5 cali (800x480) i komunikuje się z modułem Waveshare Modbus RTU Analog Input 8CH.

## Aktualny stan

Wersja: **0.9.0-unstable**

To wydanie rozwojowe przeznaczone do testów na docelowej instalacji. Rzeczywiste pomiary są pobierane z wejść analogowych; symulator jest wyłączony.

- AI1: poziom szamba, czujnik 4-20 mA,
- AI2: poziom studni, czujnik 4-20 mA,
- AI3: planowany zbiornik deszczówki,
- AI4: planowany pomiar wiatru,
- AI5-AI8: wolne wejścia.

## Najważniejsze funkcje

- rzeczywisty odczyt AI1 i AI2 przez RS485/Modbus RTU,
- osobna kalibracja szamba i studni zapisywana w NVS,
- diagnostyka ośmiu portów analogowych,
- wykrywanie odłączonego lub nieprawidłowego czujnika,
- historia poziomu z próbką co 10 minut i zakresem do 24 godzin,
- Wi-Fi, RTC, NTP, pogoda Open-Meteo i prognoza,
- alarmy i dziennik zdarzeń,
- aktualizacja firmware OTA przez przeglądarkę,
- partycje `factory`, `ota_0` i `ota_1` z obsługą rollbacku.

## Sprzęt i komunikacja

- Waveshare ESP32-S3 Touch LCD 5 cali,
- Waveshare Modbus RTU Analog Input 8CH,
- UART2: RX GPIO43, TX GPIO44,
- Modbus RTU: 9600 bit/s, 8N1, adres urządzenia 1,
- czujnik microsonic mic+130/IU/TC lub zgodny 4-20 mA,
- ESP-IDF 5.5.2 i LVGL 8.4.

## Pierwsze wgranie przez USB

W terminalu ESP-IDF 5.5.2 PowerShell:

```powershell
idf.py build
idf.py -p COM6 flash monitor
```

Numer portu COM należy dopasować do komputera. Pierwsze wgranie zapisuje bootloader, tablicę partycji i obraz fabryczny.

## Aktualizacja OTA

1. Zbuduj firmware poleceniem `idf.py build`.
2. Otwórz w przeglądarce `http://ADRES_IP_URZADZENIA:8080/update`.
3. Wybierz plik `build/SmartTank.bin` i rozpocznij aktualizację.
4. Nie odłączaj zasilania podczas przesyłania. Po poprawnym zapisie urządzenie uruchomi się ponownie.

OTA działa wyłącznie w sieci lokalnej i obecnie nie wymaga hasła. Nie należy wystawiać portu 8080 do Internetu.

## Znane ograniczenia

- historia pomiarów jest przechowywana w RAM i zeruje się po restarcie,
- AI3 i AI4 nie są jeszcze obsługiwane funkcjonalnie,
- brak MQTT, Home Assistant i dostępu zdalnego,
- jest to wydanie niestabilne, bez testu ciągłej pracy przez 24 godziny.

## Autor

Marcin Hoinca  
marcin.hoinca@gmail.com
