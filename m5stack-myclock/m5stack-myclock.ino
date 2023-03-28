// Pour afficher l'heure 
#include <NTPClient.h>
#include <WiFi.h>
#include <M5Stack.h>
#include <Stepper.h>

const char *ssid = "Nom_du_réseaux";
const char *password = "mots_de_passe";
const long utcOffsetInSeconds = 3600; // Décalage horaire en secondes (pour la France)


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

//------------------------------------- Moteur pas a pas (mpap)
const int stepperPasTour = 2048;     // Nombre de pas par tour
#define mpapFilBleu 16     // Pour cablage
#define mpapFilRose 17
#define mpapFilJaune 2
#define mpapFilOrange 5
// Creation de l'objet mpap
Stepper mpap = Stepper(stepperPasTour, mpapFilBleu, mpapFilJaune, mpapFilRose, mpapFilOrange);

void setup() {
  Serial.begin(115200);
  delay(3000);
  M5.begin();
  WiFi.begin(ssid, password);
  M5.Power.begin();

  while ( WiFi.status() != WL_CONNECTED ) {
    delay (500);
    Serial.print ( "." );
  }

  mpap.setSpeed(15);
  timeClient.begin();
}

void loop() {
  timeClient.update();

  if (timeClient.getSeconds() != timeBefore)
  {
    M5.Lcd.setTextSize(4);
    
    M5.Lcd.setCursor(75, 90);
    M5.Lcd.setTextColor(TFT_BLUE, TFT_BLACK);
    M5.Lcd.println(timeClient.getFormattedTime());

    // Récupère la date actuelle à partir de timeClient.getEpochTime()
    time_t now = timeClient.getEpochTime();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Affiche la date sur l'écran
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(75, 140);
    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.printf("%02d/%02d/%d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

    timeBefore = timeClient.getSeconds();
    
    Serial.println("MPAP action " + String(timeClient.getSeconds()));
    mpap.step(stepperPasTour/60);

   }
}
