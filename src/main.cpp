// Basé sur le travail du projet : https://github.com/kaloskagatos/EDF-Tempo-E-Ink-Display

// Customize with your settings
#include "TOCUSTOMIZE.h"

#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <WiFi.h>
#include <MyDumbWifi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "time.h"
#include <math.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/Org_01.h>

// décommenter pour deguggage de l'affichage
//#define DEBUG_GRID

//#define DEBUG_WIFI

// pour logger les flux
//#define DEBUG_API

// board wake up interval in seconds
const int WAKEUP_INTERVAL = 3600;

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/17, /*RST=*/16);
GxEPD_Class display(io, /*RST=*/16, /*BUSY=*/4);

const char *ntpServer = "pool.ntp.org";

// Global variables to store TEMPO information
#define DAY_NOT_AVAILABLE "N/A" 
String todayColor = DAY_NOT_AVAILABLE;
String tomorrowColor = DAY_NOT_AVAILABLE;

bool wifiSucceeded = true;
int currentLinePos = 0;


const int   PIN_BAT           = 35; //adc for bat voltage
// Seuils de tension pour la batterie (pour une batterie Li-ion)
const float VOLTAGE_100       = 4.2;     // Tension de batterie pleine
const float VOLTAGE_0         = 3.5;     // Tension de batterie vide

int batteryPercentage = 0;
float batteryVoltage = 0.0;

// Flag  indiquant qu'on a récupéré les infos du jour et de lendemain
bool isTodayColorFound = false ;
bool isTomorrowColorFound = false ;
bool isPreviewRTENeedRetry = false ;

// Definitions
void setup();
void loop();
void updateBatteryPercentage( int &percentage, float &voltage );
bool initializeTime();
void displayLine(String text);
String getDayOfWeekInFrench(int dayOfWeek);
String getMonthInFrench(int month);
String getCurrentDateString();
String getNextDayDateString();
void displayInfo();
String mapRteTempoColor(const String tempoColor);
void fetchTempoInformationOnlyRtePreview();
bool getCurrentTime(struct tm *timeinfo);
void goToDeepSleepUntilNextWakeup();
void drawDebugGrid();

void setup()
{
    setlocale(LC_TIME, "fr_FR.UTF-8");

    Serial.begin(115200);
    Serial.println("Démarrage...\n");

    // récupérer le voltage de la carte 
    updateBatteryPercentage(batteryPercentage, batteryVoltage) ;
  
    Serial.println("Adresse MAC:");
    Serial.println(WiFi.macAddress().c_str());
    Serial.println("Batterie:");
    char line[24];
    sprintf(line, "%5.3fv (%d%%)",batteryVoltage,batteryPercentage);
    Serial.println(line);
    
    display.init();
    display.setTextColor(GxEPD_BLACK);
  
    MyDumbWifi mdw;
#ifdef DEBUG_WIFI
    mdw.setDebug(true);
#endif
    // Connecter au WiFi
    
    if (!mdw.connectToWiFi(wifi_ssid, wifi_key)) {
      displayLine("Erreur de connexion");  
      display.update();
      Serial.println("Erreur de connexion WiFi, utilisation de l'heure RTC si disponible.");
      // Pas de gestion d'erreur, on tente d'utiliser la date RTC
      wifiSucceeded = false ;
    }

    // Initialiser l'heure
    if( !initializeTime() ) 
    {
      Serial.println("Erreur de synchronisation NTP: passage en deep sleep pendant 6 heures.");
      displayLine("Err de conn ou de synchro: deep sleep.");
      display.update();
      // Deep sleep for 6 hours
      esp_sleep_enable_timer_wakeup(6 * 60 * 60 * 1000000LL);
      esp_deep_sleep_start();
      return ;
    }

    // si affichage précédent
    display.fillScreen(GxEPD_WHITE);

#ifdef DEBUG_GRID
    drawDebugGrid();
#endif
    // Récupération des infos
    fetchTempoInformationOnlyRtePreview();

    // Display info
    displayInfo();
    display.update();

    // Sommeil profond jusqu'à la prochaine heure de réveil
    goToDeepSleepUntilNextWakeup();
}

void loop()
{
    // Nothing to do here, device will go to deep sleep
}

void updateBatteryPercentage( int &percentage, float &voltage ) {
  // Lire la tension de la batterie
  voltage = analogRead(PIN_BAT) / 4096.0 * 7.05;
  percentage = 0;
  if (voltage > 1) { // Afficher uniquement si la lecture est valide
      percentage = static_cast<int>(2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303);
      // Ajuster le pourcentage en fonction des seuils de tension
      if (voltage >= VOLTAGE_100) {
          percentage = 100;
      } else if (voltage <= VOLTAGE_0) {
          percentage = 0;
      }
  }
}

bool initializeTime() {
  // If connected to WiFi, attempt to synchronize time with NTP
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Tentative de synchronisation NTP...");
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", ntpServer); // Configure time zone to adjust for daylight savings

    const int maxNTPAttempts = 5;
    int ntpAttempts = 0;
    time_t now;
    struct tm timeinfo;
    while (ntpAttempts < maxNTPAttempts) {
      time(&now);
      localtime_r(&now, &timeinfo);

      if (timeinfo.tm_year > (2016 - 1900)) { // Check if the year is plausible
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
  if (!getLocalTime(&timeinfo, 0)) { // Immediately return the RTC time without waiting
    if (timeinfo.tm_year < (2016 - 1900)) { // If year is not plausible, RTC time is not set
      Serial.println("Échec de récupération de l'heure RTC, veuillez vérifier si l'heure a été définie.");
      return false;
    }
  }

  Serial.println("Heure RTC utilisée.");
  return true;
}

void displayLine(String text)
{
  if (currentLinePos > 150) {
      currentLinePos = 0;
      display.fillScreen(GxEPD_WHITE);
  }
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10,currentLinePos);
  display.print(text);
  currentLinePos += 10; 
}

// Helper functions to get French abbreviations
String getDayOfWeekInFrench(int dayOfWeek) {
    const char* daysFrench[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
    return daysFrench[dayOfWeek % 7];  // Use modulo just in case
}

String getMonthInFrench(int month) {
    const char* monthsFrench[] = {"Jan", "Fev", "Mar", "Avr", "Mai", "Juin", "Juil", "Aou", "Sep", "Oct", "Nov", "Dec"};
    return monthsFrench[(month - 1) % 12];  // Use modulo and adjust since tm_mon is [0,11]
}

// Function to get current date in French abbreviated format
String getCurrentDateString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Echec de récupération de la date !");
        return "";
    }

    String dayOfWeek = getDayOfWeekInFrench(timeinfo.tm_wday);
    String month = getMonthInFrench(timeinfo.tm_mon + 1); // tm_mon is months since January - [0,11]
    char dayMonthBuffer[10];
    snprintf(dayMonthBuffer, sizeof(dayMonthBuffer), "%02d %s", timeinfo.tm_mday, month.c_str());        
        
    return dayOfWeek + " " + String(dayMonthBuffer);
}

// Function to get next day's date in French abbreviated format
String getNextDayDateString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Echec de récupération de la date !");
        return "";
    }

    // Add one day to the current time
    timeinfo.tm_mday++;
    mktime(&timeinfo); // Normalize the tm structure after manual increment

    String dayOfWeek = getDayOfWeekInFrench(timeinfo.tm_wday);
    String month = getMonthInFrench(timeinfo.tm_mon + 1);
    char dayMonthBuffer[10];
    snprintf(dayMonthBuffer, sizeof(dayMonthBuffer), "%02d %s", timeinfo.tm_mday, month.c_str());
    
    return dayOfWeek + " " + String(dayMonthBuffer);
}

void displayInfo() {
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
    const int rectSpacing = 5;  // Space between rectangles
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
    const int exclamantionOffsetX = 65;
    // Set the display rotation
    display.setRotation(rotation);

    String todayString = getCurrentDateString();

    String tomorrowString = getNextDayDateString();

    // Calculate positions based on layout parameters
    int secondRectX = leftMargin + rectWidth + rectSpacing;

    // Draw the first rectangle (for today)
    display.drawRoundRect(leftMargin, topMargin, rectWidth, rectHeight, borderRadius, GxEPD_BLACK);
    // Draw date for today
    display.setFont(&FreeSans9pt7b);
    display.setCursor(leftMargin + textOffsetX + adjustTitleX, topLineY);
    display.print(todayString);
    // Draw separator
    display.drawLine(leftMargin + textOffsetX, separatorY, rectWidth - textOffsetX, separatorY, GxEPD_BLACK);
    // Draw color for today
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(leftMargin + textOffsetX, colorTextY);
    display.print(todayColor);

    // Draw battery Level
    const int batteryTopMargin = 10;
    const int nbBars = 4;
    const int barWidth = 3;
    const int batteryWidth = (barWidth + 1) * nbBars + 2;
    const int barHeight = 4;
    const int batteryHeight = barHeight + 4; 
    const int batteryTopLeftX = leftMargin + textOffsetX;
    const int batteryTopLeftY = colorTextY + batteryTopMargin;

    // Horizontal
    display.drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY , GxEPD_BLACK);
    display.drawLine(batteryTopLeftX, batteryTopLeftY + batteryHeight, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
    // Vertical
    display.drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
    display.drawLine(batteryTopLeftX + batteryWidth, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight, GxEPD_BLACK);
    // + Pole
    display.drawLine(batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + (batteryHeight - 1), GxEPD_BLACK);
    display.drawLine(batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + (batteryHeight - 1), GxEPD_BLACK);
    
    int i,j;
    int nbBarsToDraw = round(batteryPercentage / 25.0);
    for (j = 0; j < nbBarsToDraw; j++) {
      for(i = 0; i < barWidth; i++) {
        display.drawLine(batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2, batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2 + barHeight, GxEPD_BLACK);
      }
    }
    if (batteryPercentage < 25) {
      // Quand il reste moins de 25% de batterie on affiche le pourcentage
      char line[6];
      sprintf(line, "%d%%",batteryPercentage);
      display.setFont(&FreeSans9pt7b);
      display.setCursor(batteryTopLeftX + batteryWidth + 5, batteryTopLeftY + 10);
      display.print(line);
    }
    
    // Draw the second rectangle (for tomorrow)
    display.drawRoundRect(secondRectX, topMargin, rectWidth, rectHeight, borderRadius, GxEPD_BLACK);
    // Draw date for tomorrow
    display.setFont(&FreeSans9pt7b);
    display.setCursor(secondRectX + textOffsetX + adjustTitleX, topLineY);
    display.print(tomorrowString);
    // Draw separator
    display.drawLine(secondRectX + textOffsetX, separatorY, secondRectX + rectWidth - textOffsetX, separatorY, GxEPD_BLACK);
    // Draw color for tomorrow
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(secondRectX + textOffsetX, colorTextY);
    display.print(tomorrowColor);

    // Ajout d'un symbole indiquant que le wifi ne s'est pas connecté
    if( !wifiSucceeded ){
      display.print( " w");
    }  
}

String mapRteTempoColor(String tempoColor) {
    if (tempoColor == String("BLUE")) {
      return String("BLEU");
    }
    if (tempoColor == String("WHITE")){
      return String("BLANC");
    }
    if (tempoColor == String("RED")) {
      return String("ROUGE");
    }
    return tempoColor; // If it's an unrecognized value, return as is.
}

String getColorFromDocAndDate(DynamicJsonDocument doc, String dayDateString) {
  String dayColor = DAY_NOT_AVAILABLE;
  if (doc.containsKey("values") && doc["values"].containsKey(dayDateString)) {
    // WHITE RED BLUE
    String rteColor = doc["values"][dayDateString].as<String>();
    dayColor = mapRteTempoColor(rteColor);
    if (dayColor == "null") {
      dayColor = DAY_NOT_AVAILABLE;
    }
  }
  return dayColor;
}

// Quick and dirty fix : api edf ko, on fait tout avec la preview RTE
void fetchTempoInformationOnlyRtePreview() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            Serial.println("Echec de récupération de la date !");
            return;
        }
        char todayDate[11];
        strftime(todayDate, sizeof(todayDate), "%Y-%m-%d", &timeinfo);
        String todayDateString = String(todayDate);

        Serial.println("Preview RTE");
        String tempoStoreUrl = "https://www.services-rte.com/cms/open_data/v1/tempoLight";
        http.begin(tempoStoreUrl);
        int httpCode = http.GET();
        Serial.print("httpCode : ");
        Serial.println(httpCode);
        if (httpCode > 0) {
            DynamicJsonDocument doc(1024);
            String payload = http.getString();
#ifdef DEBUG_API
      Serial.println("body tempoLight:");
      Serial.println(payload);
#endif
              deserializeJson(doc, payload);
              // il faut récupérer l'éventuelle valeur de demain
              // formatage de la date de demain
              todayColor = getColorFromDocAndDate(doc,todayDateString);

              timeinfo.tm_mday++;
              mktime(&timeinfo); // Normalize the tm structure after manual increment

              char tomorrowDate[11];
              strftime(tomorrowDate, sizeof(tomorrowDate), "%Y-%m-%d", &timeinfo);
              String tomorrowDateString = String(tomorrowDate);
              tomorrowColor = getColorFromDocAndDate(doc,tomorrowDateString);              
        } else {
            Serial.println("Échec de la récupération des couleurs preview RTE, code HTTP : " + String(httpCode));
        }
        http.end();
        

        // Print the retrieved information
        Serial.println("Couleur d'aujourd'hui: " + todayColor);
        Serial.println("Couleur de demain: " + tomorrowColor);
    } else {
        Serial.println("Wi-Fi non connecté !");
    }


    // Fetch potentiellement nécessaire
    isTodayColorFound = strcmp( todayColor.c_str(), DAY_NOT_AVAILABLE) != 0;
    isTomorrowColorFound = strcmp( tomorrowColor.c_str(), DAY_NOT_AVAILABLE) != 0;
}

// Fonction pour obtenir le temps actuel sous forme de structure tm
bool getCurrentTime(struct tm *timeinfo) {
  if (!getLocalTime(timeinfo)) {
    Serial.println("Échec de l'obtention de l'heure");
    return false;
  }
  return true;
}

void goToDeepSleepUntilNextWakeup()
{
  time_t sleepDuration = WAKEUP_INTERVAL;
  Serial.print("Sleeping duration (seconds): ");
  Serial.println(sleepDuration);

  // Configure wake up
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
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

