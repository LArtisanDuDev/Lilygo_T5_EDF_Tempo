// Basé sur le travail du projet : https://github.com/kaloskagatos/EDF-Tempo-E-Ink-Display

// Customize with your settings
#include "TOCUSTOMIZE.h"

#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <WiFi.h>
#include <MyDumbWifi.h>
#include <TempoLikeSupplyContractAPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "time.h"
#include <math.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/Org_01.h>

// décommenter pour deguggage de l'affichage
// #define DEBUG_GRID

// #define DEBUG_WIFI

// pour logger les flux
#define DEBUG_API

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/17, /*RST=*/16);
GxEPD_Class display(io, /*RST=*/16, /*BUSY=*/4);

const char *ntpServer = "pool.ntp.org";

// Global variables to store TEMPO information
String todayColor = DAY_NOT_AVAILABLE;
String tomorrowColor = DAY_NOT_AVAILABLE;
int countBlue = 0;
int countRed = 0;
int countWhite = 0;

bool wifiSucceeded = true;
int currentLinePos = 0;

const int PIN_BAT = 35; // adc for bat voltage
// Seuils de tension pour la batterie (pour une batterie Li-ion)
const float VOLTAGE_100 = 4.2; // Tension de batterie pleine
const float VOLTAGE_0 = 3.5;   // Tension de batterie vide

int batteryPercentage = 0;
float batteryVoltage = 0.0;

// Structure pour stocker les heures de réveil
struct WakeupTime
{
  int hour;
  int minute;
};

// Tableau des heures de réveil
const WakeupTime wakeupTimes[] = {
    {2, 0},  // Réveil à 02:00
    {6, 30}, // Réveil à 06:30 pour préview RTE
    {11, 5}  // Réveil à 11:05
};

// Definitions
void setup();
void loop();
void drawBatteryLevel(int batteryTopLeftX, int batteryTopLeftY, int percentage);
void updateBatteryPercentage(int &percentage, float &voltage);
bool initializeTime();
void displayLine(String text);
tm getTimeWithDelta(int delta);
String getDayOfWeekInFrench(int dayOfWeek);
String getMonthInFrench(int month);
String getDateStringForRTE(int delta);
String getFullDateStringAddDelta(bool withTime, int delta);
void displayInfo();
bool getCurrentTime(struct tm *timeinfo);
time_t getNextWakeupTime();
void goToDeepSleepUntilNextWakeup();
void drawDebugGrid();

void setup()
{
  setlocale(LC_TIME, "fr_FR.UTF-8");

  Serial.begin(115200);
  Serial.println("Démarrage...\n");

  // récupérer le voltage de la carte
  updateBatteryPercentage(batteryPercentage, batteryVoltage);

  Serial.println("Adresse MAC:");
  Serial.println(WiFi.macAddress().c_str());
  Serial.println("Batterie:");
  char line[24];
  sprintf(line, "%5.3fv (%d%%)", batteryVoltage, batteryPercentage);
  Serial.println(line);

  display.init();
  display.setTextColor(GxEPD_BLACK);

  MyDumbWifi mdw;
#ifdef DEBUG_WIFI
  mdw.setDebug(true);
#endif
  // Connecter au WiFi

  if (!mdw.connectToWiFi(wifi_ssid, wifi_key))
  {
    displayLine("Erreur de connexion");
    display.update();
    Serial.println("Erreur de connexion WiFi.");
  }
  else
  {
    // Initialiser l'heure
    if (!initializeTime())
    {
      Serial.println("Erreur de synchronisation NTP: passage en deep sleep pendant 1 heures.");
      displayLine("Err de conn ou de synchro: deep sleep.");
      display.update();
      // Deep sleep for 1 hours
      esp_sleep_enable_timer_wakeup(1 * 60 * 60 * 1000000LL);
      esp_deep_sleep_start();
    }
    else
    {
      TempoLikeSupplyContractAPI *myAPI = new TempoLikeSupplyContractAPI(client_secret, client_id);
#ifdef DEBUG_API
      myAPI->setDebug(true);
#endif

      int retour = 0;
      if (tempoSansCompteTRE) {
        retour = myAPI->fecthColorsFreeApi(
            getDateStringForRTE(0).substring(0,10),
            getDateStringForRTE(1).substring(0,10),
            saisonTempo);
      } else {
        String currentTZ = getDateStringForRTE(0);
        currentTZ.remove(0,10);
        
        retour = myAPI->fetchColors(
            getDateStringForRTE(0),
            getDateStringForRTE(1),
            getDateStringForRTE(2),
            debutSaisonTempo + currentTZ);
      }

      if (retour == TEMPOAPI_OK)
      {
        todayColor = myAPI->todayColor;
        tomorrowColor = myAPI->tomorrowColor;
        countBlue = myAPI->countBlue;
        countWhite = myAPI->countWhite;
        countRed = myAPI->countRed;

        // clear screen
        display.fillScreen(GxEPD_WHITE);

#ifdef DEBUG_GRID
        drawDebugGrid();
#endif
        // Display info
        displayInfo();
        display.update();
      }
    }

    // Sommeil profond jusqu'à la prochaine heure de réveil
    goToDeepSleepUntilNextWakeup();
  }
}

void loop()
{
  // Nothing to do here, device will go to deep sleep
}

void drawBatteryLevel(int batteryTopLeftX, int batteryTopLeftY, int percentage)
{
  // Draw battery Level
  const int nbBars = 4;
  const int barWidth = 3;
  const int batteryWidth = (barWidth + 1) * nbBars + 2;
  const int barHeight = 4;
  const int batteryHeight = barHeight + 4;

  // Horizontal
  display.drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY, GxEPD_BLACK);
  display.drawLine(batteryTopLeftX, batteryTopLeftY + batteryHeight, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
  // Vertical
  display.drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
  display.drawLine(batteryTopLeftX + batteryWidth, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
  // + Pole
  display.drawLine(batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + (batteryHeight - 1), GxEPD_BLACK);
  display.drawLine(batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + (batteryHeight - 1), GxEPD_BLACK);

  int i, j;
  int nbBarsToDraw = round(percentage / 25.0);
  for (j = 0; j < nbBarsToDraw; j++)
  {
    for (i = 0; i < barWidth; i++)
    {
      display.drawLine(batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2, batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2 + barHeight, GxEPD_BLACK);
    }
  }

  if (percentage < 25)
  {
    // Quand il reste moins de 25% de batterie on affiche le pourcentage
    char line[6];
    sprintf(line, "%d%%", percentage);
    display.setFont(&FreeSans9pt7b);
    display.setCursor(batteryTopLeftX + batteryWidth + 5, batteryTopLeftY + 10);
    display.print(line);
  }
}

void updateBatteryPercentage(int &percentage, float &voltage)
{
  // Lire la tension de la batterie
  voltage = analogRead(PIN_BAT) / 4096.0 * 7.05;
  percentage = 0;
  if (voltage > 1)
  { // Afficher uniquement si la lecture est valide
    percentage = static_cast<int>(2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303);
    // Ajuster le pourcentage en fonction des seuils de tension
    if (voltage >= VOLTAGE_100)
    {
      percentage = 100;
    }
    else if (voltage <= VOLTAGE_0)
    {
      percentage = 0;
    }
  }
}

bool initializeTime()
{
  // If connected to WiFi, attempt to synchronize time with NTP
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Tentative de synchronisation NTP...");
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", ntpServer); // Configure time zone to adjust for daylight savings

    const int maxNTPAttempts = 5;
    int ntpAttempts = 0;
    time_t now;
    struct tm timeinfo;
    while (ntpAttempts < maxNTPAttempts)
    {
      time(&now);
      localtime_r(&now, &timeinfo);

      if (timeinfo.tm_year > (2016 - 1900))
      { // Check if the year is plausible
        Serial.println("NTP time synchronized!");
        return true;
      }

      ntpAttempts++;
      Serial.println("Attente de la synchronisation NTP...");
      delay(2000); // Delay between attempts to prevent overloading the server
    }

    Serial.println("Échec de synchronisation NTP, utilisation de l'heure RTC.");
  }

  // Regardless of WiFi or NTP sync, try to use RTC time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0))
  { // Immediately return the RTC time without waiting
    if (timeinfo.tm_year < (2016 - 1900))
    { // If year is not plausible, RTC time is not set
      Serial.println("Échec de récupération de l'heure RTC, veuillez vérifier si l'heure a été définie.");
      return false;
    }
  }

  Serial.println("Heure RTC utilisée.");
  return true;
}

void displayLine(String text)
{
  if (currentLinePos > 150)
  {
    currentLinePos = 0;
    display.fillScreen(GxEPD_WHITE);
  }
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10, currentLinePos);
  display.print(text);
  currentLinePos += 10;
}

// Helper functions to get French abbreviations
String getDayOfWeekInFrench(int dayOfWeek)
{
  const char *daysFrench[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
  return daysFrench[dayOfWeek % 7]; // Use modulo just in case
}

String getMonthInFrench(int month)
{
  const char *monthsFrench[] = {"Jan", "Fev", "Mar", "Avr", "Mai", "Juin", "Juil", "Aou", "Sep", "Oct", "Nov", "Dec"};
  return monthsFrench[(month - 1) % 12]; // Use modulo and adjust since tm_mon is [0,11]
}

tm getTimeWithDelta(int delta)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Echec de récupération de la date !");
    timeinfo = {0};
  }

  timeinfo.tm_mday += delta;
  mktime(&timeinfo);
  return timeinfo;
}

String getDateStringForRTE(int delta)
{
  struct tm timeinfo = getTimeWithDelta(delta);
  char tmpDate[11];
  strftime(tmpDate, sizeof(tmpDate), "%Y-%m-%d", &timeinfo);
  String result = String(tmpDate) + "T00:00:00+0" + String(timeinfo.tm_isdst+1)+":00";
  Serial.println(result);
  return result;
}

String getFullDateStringAddDelta(bool withTime, int delta)
{
  struct tm timeinfo = getTimeWithDelta(delta);
  String dayOfWeek = getDayOfWeekInFrench(timeinfo.tm_wday);
  String month = getMonthInFrench(timeinfo.tm_mon + 1); // tm_mon is months since January - [0,11]
  char dayBuffer[3];
  snprintf(dayBuffer, sizeof(dayBuffer), "%02d", timeinfo.tm_mday);

  String result = dayOfWeek + " " + String(dayBuffer) + " " + month;
  if (withTime)
  {
    char timeBuffer[9];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    result = result + " " + String(timeBuffer);
  }
  return result;
}

void displayInfo()
{
  // Define layout parameters
  const int rotation = 1;
  const int leftMargin = 2;
  const int topMargin = 6;
  const int rectWidth = 120;
  const int rectHeight = 100;
  const int borderRadius = 8;
  const int topLineY = 30;
  const int separatorY = 50;
  const int colorTextY = 80;
  const int rectSpacing = 5; // Space between rectangles
  const int bottomIndicatorY = 120;
  const int circleRadius = 6;
  const int redRectWidth = 12;
  const int redRectHeight = 13;
  const int redRectRadius = 3;
  const int textOffsetX = 10;
  const int adjustTitleX = -3;
  const int textRemainOffsetX = 10;
  const int textRemainExclamationOffsetX = 15;
  const int textRemainOffsetY = 6;
  const int circleOffsetX = 90;
  const int exclamantionOffsetX = 45;

  // Set the display rotation
  display.setRotation(rotation);

  const int batteryTopMargin = 10;
  const int batteryTopLeftX = leftMargin + textOffsetX;
  const int batteryTopLeftY = colorTextY + batteryTopMargin;
  drawBatteryLevel(batteryTopLeftX, batteryTopLeftY, batteryPercentage);

  // Calculate positions based on layout parameters
  int secondRectX = leftMargin + rectWidth + rectSpacing;

  // Draw the first rectangle (for today)
  display.drawRoundRect(leftMargin, topMargin, rectWidth, rectHeight, borderRadius, GxEPD_BLACK);
  // Draw date for today
  display.setFont(&FreeSans9pt7b);
  display.setCursor(leftMargin + textOffsetX + adjustTitleX, topLineY);
  display.print(getFullDateStringAddDelta(false, 0));
  // Draw separator
  display.drawLine(leftMargin + textOffsetX, separatorY, rectWidth - textOffsetX, separatorY, GxEPD_BLACK);
  // Draw color for today
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(leftMargin + textOffsetX, colorTextY);
  display.print(todayColor);

  // Draw the second rectangle (for tomorrow)
  display.drawRoundRect(secondRectX, topMargin, rectWidth, rectHeight, borderRadius, GxEPD_BLACK);
  // Draw date for tomorrow
  display.setFont(&FreeSans9pt7b);
  display.setCursor(secondRectX + textOffsetX + adjustTitleX, topLineY);
  display.print(getFullDateStringAddDelta(false, 1));
  // Draw separator
  display.drawLine(secondRectX + textOffsetX, separatorY, secondRectX + rectWidth - textOffsetX, separatorY, GxEPD_BLACK);
  // Draw color for tomorrow
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(secondRectX + textOffsetX, colorTextY);
  display.print(tomorrowColor);

  // remise en place des compteurs
  // Positioning for the bottom indicators
  // int x_bleu = 15;
  int x_blanc = 15;
  int x_rouge = x_blanc + exclamantionOffsetX;

  // Draw bottom indicators
  // Blue circle
  /*
  Ma MOA se moque des jours bleus :)
  display.fillCircle(x_bleu, bottomIndicatorY, circleRadius, GxEPD_BLACK);
  display.setFont(&FreeSans9pt7b);
  display.setCursor(x_bleu + textRemainOffsetX, bottomIndicatorY + textRemainOffsetY);
  display.print(remainingBlueDays + "/300");*/

  // White circle
  display.drawCircle(x_blanc, bottomIndicatorY, circleRadius, GxEPD_BLACK);
  display.setCursor(x_blanc + textRemainOffsetX, bottomIndicatorY + textRemainOffsetY);
  display.print(43 - countWhite);

  // Red rounded rectangle
  display.drawRoundRect(x_rouge, bottomIndicatorY - redRectHeight / 2, redRectWidth, redRectHeight, redRectRadius, GxEPD_BLACK);
  // Exclamation mark: upper bar (double line for better visibility)
  int exclamationCenterX = x_rouge + redRectWidth / 2;
  display.drawLine(exclamationCenterX - 1, bottomIndicatorY - 4, exclamationCenterX - 1, bottomIndicatorY + 2, GxEPD_BLACK);
  display.drawLine(exclamationCenterX, bottomIndicatorY - 4, exclamationCenterX, bottomIndicatorY + 2, GxEPD_BLACK); // Adjacent line to thicken
  // Exclamation mark: lower dot (double line for better visibility)
  display.drawLine(exclamationCenterX - 1, bottomIndicatorY + 4, exclamationCenterX - 1, bottomIndicatorY + 4, GxEPD_BLACK);
  display.drawLine(exclamationCenterX, bottomIndicatorY + 4, exclamationCenterX, bottomIndicatorY + 4, GxEPD_BLACK); // Adjacent line to thicken

  // ROUGE
  display.setCursor(x_rouge + textRemainExclamationOffsetX, bottomIndicatorY + textRemainOffsetY);
  display.print(22 - countRed);

  // draw refresh date time
  display.setFont(&FreeSans9pt7b);
  display.setCursor(leftMargin + textOffsetX + 120 + adjustTitleX, bottomIndicatorY + textRemainOffsetY);
  String tmpDate = getFullDateStringAddDelta(true, 0);
  tmpDate = tmpDate.substring(4,16);
  display.print(tmpDate);
}

// Fonction pour obtenir le temps actuel sous forme de structure tm
bool getCurrentTime(struct tm *timeinfo)
{
  if (!getLocalTime(timeinfo))
  {
    Serial.println("Échec de l'obtention de l'heure");
    return false;
  }
  return true;
}

// Fonction pour calculer la prochaine heure de réveil
time_t getNextWakeupTime()
{
  struct tm timeinfo;
  if (!getCurrentTime(&timeinfo))
  {
    return 0; // Retourner 0 si l'heure n'a pas pu être obtenue
  }

  time_t now = mktime(&timeinfo);
  time_t nextWakeup = 0;
  bool found = false;

  for (WakeupTime wakeup : wakeupTimes)
  {

    struct tm futureTime = timeinfo;
    futureTime.tm_hour = wakeup.hour;
    futureTime.tm_min = wakeup.minute;
    futureTime.tm_sec = 0;
    time_t futureTimestamp = mktime(&futureTime);

    if (futureTimestamp > now)
    {
      // Si l'heure de réveil est dans le futur, c'est le prochain réveil
      nextWakeup = futureTimestamp;
      found = true;
      break;
    }
  }

  // Si aucun réveil futur n'a été trouvé, prendre le premier réveil du lendemain
  if (!found)
  {
    struct tm nextDayTime = timeinfo;
    nextDayTime.tm_mday += 1; // Ajouter un jour
    nextDayTime.tm_hour = wakeupTimes[0].hour;
    nextDayTime.tm_min = wakeupTimes[0].minute;
    nextDayTime.tm_sec = 0;
    mktime(&nextDayTime);              // Normaliser la structure tm après modification manuelle
    nextWakeup = mktime(&nextDayTime); // mktime will handle the end of month/year
  }

  return nextWakeup;
}

void goToDeepSleepUntilNextWakeup()
{
  time_t nextWakeupTime = getNextWakeupTime();
  if (nextWakeupTime == 0)
  {
    Serial.println("Aucune heure de réveil valide trouvée. Passage en mode veille indéfiniment.");
    esp_deep_sleep_start();
    return;
  }

  // Convertir le temps de réveil en structure tm pour l'affichage
  struct tm *nextWakeupTm = localtime(&nextWakeupTime);
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%A, %B %d %Y %H:%M:%S", nextWakeupTm);

  // Afficher la prochaine date de réveil
  Serial.print("L'heure du prochain réveil est : ");
  Serial.println(buffer);

  // Calculer la durée de sommeil en secondes
  time_t now;
  time(&now);
  time_t sleepDuration = nextWakeupTime - now;

  // Convertir l'heure courante en structure tm pour l'affichage
  struct tm *currentTimeTm = localtime(&now);
  char currentTimeBuffer[64];
  strftime(currentTimeBuffer, sizeof(currentTimeBuffer), "%A, %B %d %Y %H:%M:%S", currentTimeTm);

  // Afficher l'heure courante
  Serial.print("La date courante est: ");
  Serial.println(currentTimeBuffer);

  // Afficher la durée de sommeil en secondes
  Serial.print("Durée de sommeil en secondes: ");
  Serial.println(sleepDuration);

  // Configurer l'alarme pour se réveiller à la prochaine heure configurée
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  Serial.println("Passage en mode sommeil profond jusqu'au prochain réveil.");
  esp_deep_sleep_start();
}

#ifdef DEBUG_GRID
void drawDebugGrid()
{
  int gridSpacing = 10; // Espacement entre les lignes de la grille
  int screenWidth = 122;
  int screenHeight = 250;

  // Dessiner des lignes verticales
  for (int x = 0; x <= screenWidth; x += gridSpacing)
  {
    display.drawLine(x, 0, x, screenHeight, GxEPD_BLACK);
  }

  // Dessiner des lignes horizontales
  for (int y = 0; y <= screenHeight; y += gridSpacing)
  {
    display.drawLine(0, y, screenWidth, y, GxEPD_BLACK);
  }
}
#endif
