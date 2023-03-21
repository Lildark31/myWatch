#include <NTPClient.h>
#include <WiFi.h>
#include <M5Stack.h>
#include <Stepper.h>

const char *ssid     = "OnePlus 8T";
const char *password = "Lilian12";
int timeBefore = 0;

const long utcOffsetInSeconds = 2 * 60 * 60;

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
    M5.Lcd.setTextSize(6);
    
    M5.Lcd.setCursor(20, 90);
    M5.Lcd.setTextColor(TFT_BLACK, TFT_BLACK);
    M5.Lcd.print(timeBefore);
    
    M5.Lcd.setCursor(20, 90);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Lcd.println(timeClient.getFormattedTime());

    timeBefore = timeClient.getSeconds();
    
    Serial.println("MPAP action " + String(timeClient.getSeconds()));
    mpap.step(stepperPasTour/60);
  }
}
