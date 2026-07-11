# SmartTank HMI

Panel HMI do monitorowania poziomu szamba i studni, pogody oraz stanu instalacji pomiarowej. Projekt jest rozwijany dla płytki Waveshare ESP32-S3 Touch LCD 5" z ekranem 800×480.

## Status projektu

**Etap:** kandydat do wersji 1.0.0  
**Stan interfejsu:** zamrożony do czasu uruchomienia rzeczywistego toru pomiarowego  
**Aktualne źródło danych poziomu:** symulator  
**Najbliższy etap:** Waveshare Modbus RTU Analog Input 8CH + czujniki 4–20 mA

Historia zmian znajduje się w pliku [CHANGELOG.md](CHANGELOG.md).

## Główne funkcje

- pulpit z kaflami: szambo, studnia i pogoda,
- szczegóły oraz kalibracja szamba,
- szczegóły oraz kalibracja studni,
- zapis konfiguracji w NVS,
- Wi-Fi z wyszukiwaniem sieci i zapisem hasła,
- RTC PCF85063, NTP i czas lokalny,
- pogoda Open-Meteo z wyborem miejscowości,
- czterodniowa prognoza z ikonami,
- centrum alarmów w formie tabeli zdarzeń,
- historia pomiarów w czasie działania urządzenia,
- diagnostyka RS485/Modbus i modułu analogowego,
- przygotowane partycje `factory`, `ota_0`, `ota_1` i `otadata`.

## Sprzęt

### Panel główny

- Waveshare ESP32-S3 Touch LCD 5",
- rozdzielczość 800×480,
- ESP32-S3,
- dotyk po I2C,
- RTC PCF85063,
- RS485 przez pokładowy SP3485,
- UART2: RX GPIO43, TX GPIO44.

### Tor pomiarowy

Planowany moduł:

- Waveshare Modbus RTU Analog Input 8CH,
- Modbus RTU 9600, 8N1,
- domyślny adres slave: 1,
- AI1: szambo, 4–20 mA,
- AI2: studnia, 4–20 mA.

Planowany czujnik:

- microsonic mic+130/IU/TC lub zgodny czujnik przemysłowy 4–20 mA.

## Oprogramowanie

- ESP-IDF 5.5.1,
- LVGL 8.4,
- FreeRTOS,
- NVS,
- ESP HTTP Client,
- Modbus RTU,
- Open-Meteo.

## Kompilacja i wgrywanie

W terminalu ESP-IDF PowerShell:

```powershell
git pull origin main

idf.py build
if ($LASTEXITCODE -ne 0) { throw "Kompilacja nieudana — nie flashuję" }

idf.py flash monitor
```

`fullclean` nie jest potrzebny przy zwykłych zmianach kodu. Należy go używać wyłącznie po zmianach konfiguracji projektu, tablicy partycji lub `sdkconfig`.

## Stabilna konfiguracja wyświetlacza

Projekt korzysta z:

- bufora klatki panelu w PSRAM,
- dwóch wewnętrznych buforów bounce RGB po 20 linii,
- częściowego renderowania LVGL,
- bufora LVGL o wysokości 40 linii w pamięci wewnętrznej,
- PCLK 12 MHz,
- DMA burst 64.

Nie należy zmieniać sterownika wyświetlacza bez wyraźnej potrzeby, ponieważ obecna konfiguracja została potwierdzona jako stabilna.

## Zakres wersji 1.0.0

Przed wydaniem 1.0.0 należy zakończyć:

1. uruchomienie komunikacji z modułem Waveshare 8CH,
2. konfigurację AI1 i AI2 w trybie 4–20 mA,
3. podłączenie rzeczywistych czujników,
4. kalibrację szamba i studni,
5. zastąpienie symulatora prawdziwymi pomiarami,
6. filtrowanie, histerezę i wykrywanie awarii przewodu,
7. test zaniku zasilania, braku Wi-Fi i braku RS485,
8. test pracy ciągłej przez minimum 24 godziny.

## Po wersji 1.0.0

Planowane rozszerzenia:

- aktualizacja OTA przez Wi-Fi,
- lokalny webserver,
- MQTT,
- Home Assistant,
- zdalny dostęp,
- trwała historia pomiarów i alarmów.

## Autor

Marcin Hoinca  
Kontakt: marcin.hoinca@gmail.com
