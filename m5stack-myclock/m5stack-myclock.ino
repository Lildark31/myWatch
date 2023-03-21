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
// NTP setup
bool setupNTP( void ) {
  
  if ( _need2reboot ) return false;

  log_debug(F("\n[NTP] start setup of (S)NTP ..."));

#ifdef ESP8266
  // register ntp sync callback
  settimeofday_cb( syncNTP_cb );
  // [may.20] as default, NTP server is provided by the DHCP server
  configTime( MYTZ, NTP_DEFAULT_SERVER1, NTP_DEFAULT_SERVER2, NTP_DEFAULT_SERVER3 );
#elif ESP32
  // register ntp sync callback
  sntp_set_time_sync_notification_cb( _syncNTP_cb );
  // [nov.21] now using timezone definition :)
  configTzTime( MYTZ, NTP_DEFAULT_SERVER1, NTP_DEFAULT_SERVER2, NTP_DEFAULT_SERVER3 );
  // [may.20] as default, NTP server is provided by the DHCP server
  // configTime(gmtOffset_sec, daylightOffset_sec, NTP_DEFAULT_SERVER1, NTP_DEFAULT_SERVER2, NTP_DEFAULT_SERVER3 );
#endif    

  log_flush();
  // the end ...
  return true;
}

// time server related
bool cbtime_set = false;
bool _cbtime_call = false;          // used to display things about time sync
time_t cbtime_cur, cbtime_prev;     // time set in callback


/*
 * [aug.21] shared JSON document to allow modules to exchange data.
 * ... mainly used by display module ;)
 */
#define MODULES_SHARED_JSON_SIZE  512
//StaticJsonDocument<MODULES_SHARED_JSON_SIZE> sharedRoot;    // stack allocation
DynamicJsonDocument sharedRoot(MODULES_SHARED_JSON_SIZE);   // heap allocation


// --- Functions ---------------------------------------------------------------
void setupSerial( void ) {
#ifdef SERIAL_BAUDRATE
  delay(3000);  // time for USB serial link to come up anew
  Serial.begin(SERIAL_BAUDRATE); // Start serial for output

  // Arduino libs v2.4.1, to enable printf and debug messages output
  Serial.setDebugOutput( true );
  
  char tmp[96];
  snprintf(tmp,sizeof(tmp),"\n\n\n\n# %s firmware rev %d for neOCampus is starting ... ",getBoardName(),getFirmwareRev());
  log_info(tmp);
  log_info(F("\n#\tMac address is ")); log_info(getMacAddress());
  log_info(F("\n#\tlog level is ")); log_info(LOG_LEVEL,DEC);
  log_info(F("\n"));
  log_flush();
#endif
#ifndef DEBUG_WIFI_MANAGER
  wifiManager.setDebugOutput(false);
#endif
}
