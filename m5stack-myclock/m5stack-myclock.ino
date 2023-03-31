// Pour afficher l'heure 
#include <WiFi.h>
#include <M5Stack.h>
#include <time.h>                         // time() ctime()
#include <sntp.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "neocampus_debug.h"

// Time related definitions
#define MYTZ                    "CET-1CEST,M3.5.0/2,M10.5.0/3"
//#define NTP_DEFAULT_SERVER1       "time.nist.gov"     // DNS location aware
//#define NTP_DEFAULT_SERVER2       "0.fr.pool.ntp.org" // DNS location aware
#define NTP_DEFAULT_SERVER1     "ntp.irit.fr"       // UT3-IRIT specific
#define NTP_DEFAULT_SERVER2     "ntp.univ-tlse3.fr" // UT3 specific
#define NTP_DEFAULT_SERVER3     "pool.ntp.org"      // DNS location aware

// time server related
bool cbtime_set = false;
bool _cbtime_call = false;          // used to display things about time sync
time_t cbtime_cur, cbtime_prev;     // time set in callback

// JSON related definitons
#define HTTP_URL_MAXSIZE              256   // maximum size of a URL
#define HTTP_MAX_RESPONSE_SIZE        1024  // maximum size of a HTTP response
char _answer[HTTP_MAX_RESPONSE_SIZE];

#define WEATHER_JSON_SIZE             (JSON_OBJECT_SIZE(256))   // max number of objects in any WEATHER JSON response
StaticJsonDocument<WEATHER_JSON_SIZE> root;


// Your Domain name with URL path or IP address with path
String openWeatherMapApiKey = "80fc8067f072dbc70402b3e0a4ad4f49";
// Example:
//String openWeatherMapApiKey = "bd939aa3d23ff33d3c8f5dd1dd435";

// Replace with your country code and city
String city = "Toulouse";
String countryCode = "FR";

// THE DEFAULT TIMER IS SET TO 10 SECONDS FOR TESTING PURPOSES
// For a final application, check the API call limits per hour/minute to avoid getting blocked/banned
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 10 seconds (10000)
unsigned long timerDelay = 20000;


//Connection au réseau avec mot de passe et le nom
const char ssid[] = "amilab";
const char password[] = "neocampus";

// type for mac address in raw format
typedef uint8_t mac_addr_t[6];
// type for mac address in string format
typedef char mac_str_t[18];



// ---
// retrieve mac address
const char *getMacAddress( void ) {
  static bool _initialized = false;
  // mac address array format
  static mac_addr_t _mac;
  // mac address string format
  static mac_str_t _mac_sta;

  if( _initialized == false ) {
    //WiFi.softAPmacAddress( _mac );  // beware that AP mac addr is different from STA mac addr!
    WiFi.macAddress( _mac );
    snprintf(_mac_sta, sizeof(_mac_sta), "%02x:%02x:%02x:%02x:%02x:%02x", _mac[0], _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]);
    _initialized = true;
  }
  return (const char *)_mac_sta;
}

// ---
/*
 * Get current time
 * WARNING: static variable inside (i.e single thread)
 */
const char *getCurTime( const char *fmt=nullptr ) {

  // STATIC variable !
  static char _tmpbuf[64];
  
  // default format
  const char defl_fmt[] = "%Y-%m-%d %H:%M:%S %z";
  
  if( !fmt ) fmt=defl_fmt;

  struct tm *_tm;
  time_t _curTime;
  
  time( &_curTime );
  _tm = localtime( &_curTime );   // Weird part ... localtime function *corrects* time to match timezone ... :s
  strftime( _tmpbuf, sizeof(_tmpbuf), fmt, _tm);

  return (const char *)_tmpbuf;
}

// ---
/* sync ntp callback:
 *  This function gets called whenever a NTP sync occurs
 */

void _syncNTP_cb(struct timeval *tv) {
  // tv arg is time from NTP server
  syncNTP_cb();
}

void syncNTP_cb( void ) {
  char _tmpbuf[64];
  struct tm *_tm;

  /* WARNING
   *  - gettimeofday --> TZ settings does not apply
   *  - time() while incorrect will get masked by localtime behaviour :s
   */
   
  // retrieve when time sync occured
  // gettimeofday( &cbtime_cur, NULL ); // WARNING: timezone does not apply ... :(
  // log_debug(F("\n[NTP] time sync occured ")); log_debug( ctime(&cbtime_cur.tv_sec) );
  time( &cbtime_cur );
  _tm = localtime( &cbtime_cur );   // Weird part ... localtime function *corrects* time to match timezone ... :s

  
  log_info(F("\n[NTP] sync CALLBACK ... ")); log_flush();

  /* Note: while in this callback, the ntp server
   *  is not yet written in the list of NTP servers!
  */

  // display time synchro message
  strftime( _tmpbuf, sizeof(_tmpbuf), "%Y-%m-%d %H:%M:%S %z", _tm);
  log_debug(F("\n[NTP] time sync occured ")); log_debug(_tmpbuf);

  // first call ?
  if( not cbtime_set ) {
    cbtime_set = true;
  }
  else {
    int16_t _sync_diff = cbtime_cur - ( cbtime_prev + (SNTP_UPDATE_DELAY/1000) ); // seconds
    if( abs(_sync_diff) <= 2 ) {
      log_info(F(" local clock well syncronized :)"));
    }
    else if( abs(_sync_diff) <= 60 ) {
      log_warning(F(" local clock shift(seconds)= "));log_info(_sync_diff,DEC);
    }
    else {
      log_debug(F(" local clock shift > 60s ... probably a WiFi resync ..."));
    }
  }
  log_flush();

  cbtime_prev = cbtime_cur;

  // callback called
  _cbtime_call = true;
}


// ---
// NTP setup
bool setupNTP( void ) {
 
  log_debug(F("\n[NTP] start setup of (S)NTP ..."));log_flush();
  // register ntp sync callback
  sntp_set_time_sync_notification_cb( _syncNTP_cb );
  // [nov.21] now using timezone definition :)
  configTzTime( MYTZ, NTP_DEFAULT_SERVER1, NTP_DEFAULT_SERVER2, NTP_DEFAULT_SERVER3 );
  // [may.20] as default, NTP server is provided by the DHCP server
  // configTime(gmtOffset_sec, daylightOffset_sec, NTP_DEFAULT_SERVER1, NTP_DEFAULT_SERVER2, NTP_DEFAULT_SERVER3 );
  log_flush();
  // the end ...
  return true;
}


// ---
// HTTP GET
bool httpGETRequest( const char* url, char *buf, size_t bufsize) {

  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  log_debug(F("\n[HTTP] GET url : ")); log_debug(url); log_flush();
  http.begin(client, url);
  
  // perform GET
  int httpCode = http.GET();

  // connexion failed to server ?
  if( httpCode < 0 ) {
    log_error(F("\n[HTTP] connexion error code : ")); log_debug(httpCode,DEC); log_flush();
    return false;
  }
  
  // check for code 200
  if( httpCode == HTTP_CODE_OK ) {
    String payload = http.getString();
    snprintf( buf, bufsize, "%s", payload.c_str() );
  }
  else {
    log_error(F("\n[HTTP] GET retcode : ")); log_debug(httpCode,DEC); log_flush();
  }
  
  // close connexion established with server
  http.end();

  yield();
  
  return ( httpCode == HTTP_CODE_OK );
}



// ======================HeureEtDate=====================
void setup() {

  Serial.begin(115200);
  delay(3000);
  log_info(F("\nmyWatch on M5stack is starting ..."));log_flush();
  log_info(F("\n\tMAC ADDR : "));log_info(getMacAddress());
  log_info(F("\n")); log_flush();
  
  M5.begin();
  WiFi.begin(ssid, password);
  M5.Power.begin();

  while ( WiFi.status() != WL_CONNECTED ) {
    delay (500);
    log_debug(F("."));
  }
  
  log_debug(F("\nWe're CONNECTED !!! :S"));log_flush();

  log_debug(F("\nstart to We're CONNECTED !!! :S"));log_flush();
  setupNTP();
  delay(1000);  
}

//date et heure
void loop() {

  M5.Lcd.setTextSize(4);
  
  M5.Lcd.setCursor(75, 90);
  M5.Lcd.setTextColor(TFT_BLUE, TFT_BLACK);
  M5.Lcd.println(getCurTime("%H:%M:%S"));

  // Affiche la date sur l'écran
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(75, 140);
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.printf(getCurTime("%Y-%m-%d"));

  //meteo
  // Send an HTTP GET request
  if ( ((millis() - lastTime) > timerDelay) and (WiFi.status()== WL_CONNECTED)) {

    // create request
    char _url[HTTP_URL_MAXSIZE];
    snprintf(_url,sizeof(_url),"http://api.openweathermap.org/data/2.5/weather?q=%s,%s&APPID=%s",
              city.c_str(),countryCode.c_str(),openWeatherMapApiKey.c_str());
    //String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openWeatherMapApiKey;

    if( httpGETRequest(_url,_answer,sizeof(_answer)) ) {
      log_debug(F("\n[myWatch] sucess HTTP GET :)"));log_flush();
  
      auto err = deserializeJson( root, _answer );
      if( err ) {
        log_error(F("\n[myWatch] ERROR parsing HTTP answer: "));log_error(err.c_str()); log_flush();
      }
      else {
        serializeJsonPretty( root, Serial );
/*  
        Serial.print("JSON object = ");
        Serial.println(myObject);
        Serial.print("Temperature: ");
        Serial.println(myObject["main"]["temp"]);
        Serial.print("Pressure: ");
        Serial.println(myObject["main"]["pressure"]);
        Serial.print("Humidity: ");
        Serial.println(myObject["main"]["humidity"]);
        Serial.print("Wind Speed: ");
        Serial.println(myObject["wind"]["speed"]);
*/    
      }
    }
    else {
      log_error(F("\n[myWatch] ERROR getting answer from url:"));log_error(_url);log_flush();
    }

    lastTime = millis();
  }
}
