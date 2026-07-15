# Historia zmian

Format jest oparty na Keep a Changelog, a numery wersji na Semantic Versioning.

## [0.9.0-unstable] - 2026-07-15

Pierwsze publiczne wydanie testowe przeznaczone do pracy na docelowej instalacji.

### Dodano

- rzeczywistą komunikację RS485/Modbus RTU z Waveshare Analog Input 8CH,
- odczyt czujnika szamba na AI1 i studni na AI2,
- przeliczenie prądu 4-20 mA na odległość i poziom,
- ustawienia oraz osobną kalibrację szamba i studni zapisywaną w NVS,
- ekran serwisowy ze stanem wszystkich ośmiu wejść analogowych,
- wykrywanie braku lub awarii czujnika,
- historię rzeczywistych pomiarów z próbką co 10 minut i zakresem 24 godzin,
- statystyki historii: minimum, maksimum i zmiana poziomu,
- aktualizację OTA z przeglądarki przez sieć lokalną,
- partycje A/B, walidację uruchomionego obrazu i mechanizm rollback,
- Wi-Fi, RTC, NTP, pogodę Open-Meteo i czterodniową prognozę,
- centrum alarmów i dziennik zdarzeń,
- ekrany informacji i diagnostyki urządzenia.

### Zmieniono

- wyłączono symulator danych poziomu,
- kafle szamba i studni korzystają z rzeczywistych pomiarów,
- poprawiono nawigację ekranów serwisowych i odświeżanie nagłówka,
- status portu jest zielony tylko przy poprawnym czujniku i czerwony przy jego braku,
- ograniczono zapis historii z jednej próbki na sekundę do jednej na 10 minut.

### Sprawdzono

- odłączenie czujników AI1 i AI2,
- utratę komunikacji RS485,
- pracę bez Wi-Fi,
- restart urządzenia i ponowne uruchomienie usług,
- pełną aktualizację OTA i start z partycji OTA.

### Znane ograniczenia

- historia pomiarów nie jest zachowywana po restarcie,
- wydanie nie przeszło testu ciągłej pracy przez 24 godziny,
- AI3, AI4, MQTT, Home Assistant i dostęp zdalny są zaplanowane na później,
- strona OTA nie ma uwierzytelniania i powinna być dostępna tylko w zaufanej sieci LAN.
