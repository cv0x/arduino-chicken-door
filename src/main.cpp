#include <Wire.h>
#include <RTClib.h>
#include <Arduino.h>
#include <Dusk2Dawn.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>
#include <EEPROM.h>

RTC_DS3231 rtc;

// Definice pro krokový motor
#define STEPS_PER_REVOLUTION 2048 // Počet kroků na otáčku pro 28BYJ-48
#define MOTOR_SPEED 10            // Rychlost motoru (RPM)
#define DOOR_STEPS 6144           // Počet kroků pro otevření/zavření (1 otáčka - 2048)

// Piny pro připojení driveru ULN2003
#define IN1 2
#define IN2 3
#define IN3 4
#define IN4 5

// Adresa v EEPROM pro uložení stavu dveří
#define DOOR_STATE_ADDR 0

// Konstanty pro podsvícení a tlačítko
const int BACKLIGHT_PIN = 9;         // PWM pin pro ovládání podsvícení
const int BACKLIGHT_LEVEL = 20;      // Hodnota 0-255 pro jas (0 = vypnuto, 255 = plný jas)
const int BUTTON_PIN = 8;            // Pin pro tlačítko podsvícení
const int BACKLIGHT_TIMEOUT = 30000; // 30 sekund v milisekundách

// Zadejte vaši zeměpisnou polohu (šířka, délka, časové pásmo)
const float LATITUDE = 50.0755;  // zeměpisná šířka
const float LONGITUDE = 14.4378; // zeměpisná délka
const int TIMEZONE = 1;          // časové pásmo (UTC+1 pro ČR, v létě +2)

// Inicializace krokového motoru
Stepper motor(STEPS_PER_REVOLUTION, IN1, IN3, IN2, IN4);

// Inicializace LCD displeje (0x27 je typická I2C adresa, 16x2 je velikost displeje)
LiquidCrystal_I2C lcd(0x27, 16, 2);

Dusk2Dawn location(LATITUDE, LONGITUDE, TIMEZONE);

// Globální proměnné
bool doorOpen = false;
bool isDaytime = false;
bool backlightOn = true;
unsigned long backlightStartTime = 0;
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 1800000; // 30 minut v milisekundách

// Funkce pro zapnutí podsvícení
void turnOnBacklight()
{
  lcd.setBacklight(true);
  backlightOn = true;
  backlightStartTime = millis();
  Serial.println("Podsvícení zapnuto");
}

// Funkce pro vypnutí podsvícení
void turnOffBacklight()
{
  lcd.setBacklight(false);
  backlightOn = false;
  Serial.println("Podsvícení vypnuto");
}

// Funkce pro uložení stavu dveří do EEPROM
void saveDoorState()
{
  EEPROM.update(DOOR_STATE_ADDR, doorOpen ? 1 : 0);
  Serial.print("Stav dveří uložen: ");
  Serial.println(doorOpen ? "OTEVŘENO" : "ZAVŘENO");
}

// Funkce pro načtení stavu dveří z EEPROM
void loadDoorState()
{
  byte savedState = EEPROM.read(DOOR_STATE_ADDR);
  doorOpen = (savedState == 1);
  Serial.print("Načten uložený stav dveří: ");
  Serial.println(doorOpen ? "OTEVŘENO" : "ZAVŘENO");
}

// Funkce pro otevření dveří
void openDoor()
{
  if (!doorOpen)
  {
    Serial.println("Otevírám dveře...");
    motor.step(DOOR_STEPS); // Otočení ve směru hodinových ručiček
    doorOpen = true;

    // Vypnutí cívek motoru pro úsporu energie
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);

    // Uložení nového stavu
    saveDoorState();
  }
}

// Funkce pro zavření dveří
void closeDoor()
{
  if (doorOpen)
  {
    Serial.println("Zavírám dveře...");
    motor.step(-DOOR_STEPS); // Otočení proti směru hodinových ručiček
    doorOpen = false;

    // Vypnutí cívek motoru pro úsporu energie
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);

    // Uložení nového stavu
    saveDoorState();
  }
}

// Funkce pro formátování času (HH:MM)
String formatTime(int hours, int minutes)
{
  String result = "";
  if (hours < 10)
    result += "0";
  result += String(hours) + ":";
  if (minutes < 10)
    result += "0";
  result += String(minutes);
  return result;
}

void setup()
{
  Serial.begin(9600);

  // Nastavení pinu pro podsvícení
  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, BACKLIGHT_LEVEL);

  // Nastavení pinu pro tlačítko
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Inicializace LCD
  lcd.init();
  lcd.setBacklight(true); // Zapnutí základního podsvícení
  backlightOn = true;
  backlightStartTime = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inicializace...");

  // Inicializace RTC
  if (!rtc.begin())
  {
    Serial.println("Nelze najít RTC modul!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC nenalezeno!");
    while (1)
      ;
  }

  // Nastavení času RTC modulu na čas kompilace - odkomentujte při prvním nahrání
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // Serial.println("RTC čas nastaven na čas kompilace!");

  // Nastavení rychlosti motoru
  motor.setSpeed(MOTOR_SPEED);

  // Nastavení pinů motoru jako výstupní
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Vypnutí cívek motoru
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  // Načtení uloženého stavu dveří
  loadDoorState();
}

void loop()
{
  unsigned long currentMillis = millis();

  // Kontrola tlačítka (aktivní v LOW stavu kvůli INPUT_PULLUP)
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW)
    {
      while (digitalRead(BUTTON_PIN) == LOW)
      {
        delay(10);
      }
      turnOnBacklight();

      // Přidáno: Vynutit kontrolu stavu dveří při stisku tlačítka
      lastCheckTime = 0; // Vynutí okamžitou kontrolu
    }
  }

  // Kontrola časovače podsvícení
  if (backlightOn && (millis() - backlightStartTime > BACKLIGHT_TIMEOUT))
  {
    turnOffBacklight();
  }
  // Kontrola, zda uplynulo 30 minut od poslední aktualizace nebo jde o první spuštění
  if (currentMillis - lastCheckTime >= checkInterval || lastCheckTime == 0)
  {
    lastCheckTime = currentMillis;

    DateTime now = rtc.now();
    Serial.print("Aktuální datum a čas: ");
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.println(now.second(), DEC);

    // Výpočet času východu a západu slunce pro aktuální den
    int sunriseMinutes = location.sunrise(now.year(), now.month(), now.day(), false);
    int sunsetMinutes = location.sunset(now.year(), now.month(), now.day(), false);

    // Převod minut na hodiny a minuty
    int sunriseHour = sunriseMinutes / 60;
    int sunriseMinute = sunriseMinutes % 60;

    int sunsetHour = sunsetMinutes / 60;
    int sunsetMinute = sunsetMinutes % 60;

    // Aktuální čas v minutách od půlnoci
    int currentMinutes = now.hour() * 60 + now.minute();

    // Formátování časů pro zobrazení
    String currentTimeStr = formatTime(now.hour(), now.minute());
    String sunriseTimeStr = formatTime(sunriseHour, sunriseMinute);
    String sunsetTimeStr = formatTime(sunsetHour, sunsetMinute);

    // Kontrola, zda je den nebo noc
    bool newIsDaytime = (currentMinutes > sunriseMinutes && currentMinutes < sunsetMinutes);

    // Výpočet zbývajícího času do změny
    int minutesUntilChange;
    if (newIsDaytime)
    {
      // Je den, počítáme čas do západu
      minutesUntilChange = sunsetMinutes - currentMinutes;
    }
    else
    {
      // Je noc, počítáme čas do východu
      if (currentMinutes < sunriseMinutes)
      {
        // Východ bude dnes
        minutesUntilChange = sunriseMinutes - currentMinutes;
      }
      else
      {
        // Východ bude zítra (24 hodin - aktuální čas + zítřejší východ)
        minutesUntilChange = (24 * 60 - currentMinutes) + sunriseMinutes;
      }
    }

    Serial.print("Aktuální čas (minuty od půlnoci): ");
    Serial.println(currentMinutes);
    Serial.print("Východ slunce (minuty od půlnoci): ");
    Serial.println(sunriseMinutes);
    Serial.print("Západ slunce (minuty od půlnoci): ");
    Serial.println(sunsetMinutes);
    Serial.print("Je den? ");
    Serial.println(newIsDaytime ? "ANO" : "NE");
    Serial.print("Předchozí stav - Je den? ");
    Serial.println(isDaytime ? "ANO" : "NE");

    // Kontrola, zda se změnil stav dne/noci
    if (newIsDaytime != isDaytime)
    {
      Serial.println("ZMĚNA STAVU DEN/NOC DETEKOVÁNA!");
      isDaytime = newIsDaytime;

      if (isDaytime)
      {
        // Přechod z noci na den - otevřít dveře
        openDoor();
      }
      else
      {
        // Přechod ze dne na noc - zavřít dveře
        closeDoor();
      }
    }
    else
    {
      // Přidáno: Kontrola, zda stav dveří odpovídá denní době
      if (isDaytime && !doorOpen)
      {
        Serial.println("Korekce: Je den, ale dveře jsou zavřené. Otevírám...");
        openDoor();
      }
      else if (!isDaytime && doorOpen)
      {
        Serial.println("Korekce: Je noc, ale dveře jsou otevřené. Zavírám...");
        closeDoor();
      }
    }

    // Aktualizace LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    if (isDaytime)
    {
      lcd.print(currentTimeStr + " OTEVRENO");
      lcd.setCursor(0, 1);
      lcd.print("Zapad v " + sunsetTimeStr);
    }
    else
    {
      lcd.print(currentTimeStr + " ZAVRENO");
      lcd.setCursor(0, 1);
      lcd.print("Vychod v " + sunriseTimeStr);
    }

    // Výpis do sériového monitoru pro ladění
    Serial.print("Aktuální čas: ");
    Serial.println(currentTimeStr);
    Serial.print("Východ slunce: ");
    Serial.println(sunriseTimeStr);
    Serial.print("Západ slunce: ");
    Serial.println(sunsetTimeStr);
    Serial.print("Zbývající čas do změny: ");
    Serial.print(minutesUntilChange / 60);
    Serial.print(" hodin a ");
    Serial.print(minutesUntilChange % 60);
    Serial.println(" minut");
    Serial.println("---------------------");
  }

  // Aktualizace času na displeji každou minutu
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate >= 60000)
  { // každou minutu
    lastTimeUpdate = millis();

    DateTime now = rtc.now();
    String currentTimeStr = formatTime(now.hour(), now.minute());

    // Aktualizace pouze času na displeji, zbytek zůstává
    lcd.setCursor(0, 0);
    lcd.print(currentTimeStr);
  }

  // Krátká pauza pro snížení zatížení procesoru
  delay(1000);
}