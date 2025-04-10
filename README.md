# Automatické dveře kurníku

Tento projekt automaticky otevírá a zavírá dveře kurníku na základě času východu a západu slunce. Systém používá Arduino Nano, RTC modul pro sledování času a krokový motor pro ovládání dveří.

## Funkce

- Automatické otevírání dveří při východu slunce
- Automatické zavírání dveří při západu slunce
- LCD displej zobrazující aktuální čas a stav dveří
- Podsvícení displeje aktivované tlačítkem (vypíná se po 30 sekundách)
- Ukládání stavu dveří do EEPROM pro případ výpadku napájení
- Pravidelná kontrola správného stavu dveří

## Potřebný hardware

- Arduino Nano (ATmega328P)
- RTC modul DS3231
- LCD displej 16x2 s I2C adaptérem
- Krokový motor 28BYJ-48 s ULN2003 driverem
- Tlačítko pro aktivaci podsvícení
- Rezistory a propojovací vodiče

## Zapojení

| Komponenta | Pin Arduino | Poznámka |
|------------|-------------|----------|
| **Krokový motor (ULN2003)** | | |
| IN1 | D2 | |
| IN2 | D3 | |
| IN3 | D4 | |
| IN4 | D5 | |
| **LCD displej (I2C)** | | |
| SDA | A4 | |
| SCL | A5 | |
| **RTC modul (I2C)** | | |
| SDA | A4 | Sdíleno s LCD |
| SCL | A5 | Sdíleno s LCD |
| **Tlačítko podsvícení** | | |
| Tlačítko | D8 | Připojeno k GND při stisku (INPUT_PULLUP) |

## Nastavení

V kódu je potřeba nastavit:

1. Zeměpisnou polohu pro správný výpočet času východu a západu slunce:
   ```cpp
   const float LATITUDE = 50.0755;  // zeměpisná šířka
   const float LONGITUDE = 14.4378; // zeměpisná délka
   const int TIMEZONE = 1;          // časové pásmo (UTC+1 pro ČR, v létě +2)
   ```

2. Parametry krokového motoru podle potřeby:
   ```cpp
   #define STEPS_PER_REVOLUTION 2048 // Počet kroků na otáčku pro 28BYJ-48
   #define MOTOR_SPEED 10            // Rychlost motoru (RPM)
   #define DOOR_STEPS 6144           // Počet kroků pro otevření/zavření (1 otáčka - 2048)
   ```

## Instalace

1. Nainstalujte [PlatformIO](https://platformio.org/) nebo Arduino IDE
2. Nainstalujte potřebné knihovny:
   - RTClib
   - Dusk2Dawn
   - LiquidCrystal_I2C
   - Stepper
3. Nahrajte kód do Arduino Nano
4. Při prvním nahrání odkomentujte řádek pro nastavení času RTC modulu:
   ```cpp
   // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
   ```

## Použití

Po zapnutí systém automaticky:
- Načte uložený stav dveří z EEPROM
- Zkontroluje aktuální čas a porovná ho s časem východu/západu slunce
- Nastaví dveře do správné pozice (otevřeno ve dne, zavřeno v noci)
- Každých 30 minut kontroluje, zda je potřeba změnit stav dveří

Tlačítko na pinu D8 slouží k zapnutí podsvícení displeje, které se automaticky vypne po 30 sekundách.