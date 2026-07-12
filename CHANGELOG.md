# Historia zmian

W tym pliku zapisywane są istotne zmiany projektu SmartTank HMI. Do czasu wydania wersji 1.0.0 wszystkie wpisy trafiają do sekcji **Nieopublikowane**.

## [Nieopublikowane]

### Dodano

- pełny pulpit 800×480 z kaflami szamba, studni i pogody,
- ekran szczegółów i kalibracji szamba,
- ekran szczegółów i kalibracji studni,
- zapisywanie konfiguracji szamba i studni w NVS,
- Wi-Fi z wyszukiwaniem sieci, logowaniem, ponownym łączeniem i zapisem danych,
- RTC PCF85063 oraz synchronizację czasu przez NTP,
- wybór miejscowości dla pogody przez Open-Meteo Geocoding,
- rzeczywiste dane pogodowe i czterodniową prognozę,
- dynamiczne ikony pogody dla stanu bieżącego i prognozy,
- ekran szczegółów pogody otwierany z kafla,
- historię poziomu szamba i studni w czasie działania urządzenia,
- centrum alarmów,
- dziennik zdarzeń alarmowych w formie tabeli,
- rejestrację zdarzeń: aktywacja, potwierdzenie i ustąpienie,
- ekran serwisowy z diagnostyką systemu, RS485, Modbus i modułu 8CH,
- ekran informacji o firmware, pamięci, partycjach i sprzęcie,
- dolną nawigację z ikonami i licznikiem aktywnych alarmów,
- przygotowanie sterownika Waveshare Modbus RTU Analog Input 8CH,
- obsługę funkcji Modbus 03, 04 i 06,
- sprzętowe odpytywanie modułu Waveshare 8CH po RS485,
- odczyt adresu urządzenia komendą rozgłoszeniową producenta bez zmiany konfiguracji,
- automatyczne wykrywanie wszystkich udokumentowanych prędkości 4800–256000 oraz formatów 8N1/8E1/8O1,
- przygotowanie partycji `factory`, `ota_0`, `ota_1` i `otadata`.

### Zmieniono

- ustabilizowano wyświetlacz RGB przez częściowe renderowanie LVGL,
- przeniesiono TLS i duże alokacje sieciowe do PSRAM,
- zastąpiono litery prognozy małymi ikonami pogodowymi,
- zastąpiono osobne kafelki alarmowe jedną tabelą zdarzeń,
- przywrócono przycisk wyboru miejscowości w ustawieniach,
- rozszerzono pasek statusu o Wi-Fi, MQTT i RS485,
- zamrożono interfejs użytkownika przed integracją rzeczywistych czujników,
- stan ONLINE modułu 8CH oparto na potwierdzonym przez producenta odczycie wejść funkcją 04,
- pomocnicze odczyty rejestrów identyfikacji i trybów nie blokują już podstawowej komunikacji,
- po nieudanym odczycie częstotliwość kolejnych prób ograniczono, aby nie zalewać magistrali i logów.

### Naprawiono

- przesuwanie i smużenie obrazu podczas obsługi dotyku,
- awarię `LoadProhibited` podczas skanowania niestandardowych ikon nawigacji,
- błędy alokacji TLS/X509 przy geokodowaniu i pobieraniu pogody,
- pozycjonowanie ikony pogody,
- brak reakcji kafla studni,
- brak reakcji kafla pogody,
- znikający przycisk wyboru miejscowości.

### Ograniczenia obecnego wydania

- poziom szamba i studni nadal pochodzi z symulatora,
- moduł Waveshare 8CH jest włączony do testu komunikacji, ale jego dane nie sterują jeszcze kaflami,
- historia pomiarów i alarmów nie jest jeszcze trwała po restarcie,
- OTA przez Wi-Fi nie jest jeszcze zaimplementowane,
- webserver, MQTT i Home Assistant są zaplanowane po wersji 1.0.0.

## Plan wersji 1.0.0

- uruchomienie rzeczywistej komunikacji RS485,
- konfiguracja AI1 i AI2 jako 4–20 mA,
- integracja czujników poziomu,
- kalibracja rzeczywistego szamba i studni,
- wyłączenie symulatora,
- filtrowanie i walidacja pomiarów,
- histereza i opóźnienia alarmów,
- test zaniku zasilania,
- test bez Wi-Fi,
- test bez RS485,
- test pracy ciągłej przez minimum 24 godziny,
- oznaczenie stabilnego commita tagiem `v1.0.0`.

## Zasady wersjonowania

Projekt będzie używał numeracji:

```text
MAJOR.MINOR.PATCH
```

- `MAJOR` — zmiany niezgodne z poprzednią wersją,
- `MINOR` — nowe funkcje zachowujące zgodność,
- `PATCH` — poprawki błędów bez zmiany głównych funkcji.

Przykłady:

```text
1.0.0 — pierwsze stabilne wydanie
1.1.0 — webserver lub MQTT
1.1.1 — poprawka błędu bez nowej funkcji
2.0.0 — duża przebudowa architektury lub konfiguracji
```
