/* ****************
 * project: carbon-footprint, an art installation
 *
 * Written by Gabriel Cain, with heavy borrowing from blogs and forums
 *
 * This code is designed for the ESP8266 platform, and makes assumptions based on that
 * **************** */

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

// Configuration
const PROGMEM char geoip_server[] = "freegeoip.net";    //  (using DNS)
const PROGMEM char ssid[] = "ssid";       //  your network SSID (name)
const PROGMEM char pass[] = "password";   // your network password
const PROGMEM char owm_url[] = "http://api.openweathermap.org/data/2.5/weather?APPID="; 
const PROGMEM char owm_api_key[] = "";    // API KEY for OpenWeatherMap
const pinRed = 0;
const pinBlue = 2;



// IP Address
uint32_t ip = 0L;

// json
DynamicJsonBuffer jsonBuffer;

// Location services
char country[20],
     region[20],
     city[20],
     name[13],
     value[64];

float longitude, latitude;

// Initialize the Ethernet client library
WiFiClient client;

// ******************************************************************** //
// Functions
//
// UTILITY
// void blinkPin(int pin, int ms, int duty_cycle) - duty_cycle = 0..100 !
// void blinkRed(int ms)
// void blinkBlue(int ms)
//
// GEOIP WRANGLING
// boolean jsonParse(int depth, byte endChar)
// int unidecode(byte len)
// boolean readString(char *dest, int maxLen, char quote)
// int timedRead()
// void getLocation()
//
// TEMPURATURE WRANGLING
// void getTemperatureAPI()
// void getTemperatureI2C() // todo: order i2c sensor
//
// WIFI
// voidConnectToWifi()
//
// CORE FUNCTIONS at BOTTOM
// void loop()
// void setup()
// ******************************************************************** //

/* blinkSetup - do pinmode stuff */
void blinkSetup() {
  pinMode( pinRed, OUTPUT );
  pinMode( pinBlue, OUTPUT );
}

/* just blinks a pin, spending dutyCycle amount on (0..100 %) */
void blinkPin(int pin, int ms, int dutyCycle) {
  int ds, dst_on, dst_off;
  if( dutyCycle < 0 ) { ds = 0; }
  if( dutyCycle > 100 ) { ds = 100; }

  dst_on = ( ds * ms ) / 100;
  dst_off = ms - dst_on;

  digitalWrite( pin, HIGH );
  delay(dst_on);
  digitalWrite( pin, LOW );
  delay(dst_off);
}

void blinkRed(int ms) {
  blinkPin( pinRed, ms, 50 );
}

void blinkBlue(int ms) {
  blinkPin( pinBlue, ms, 50 );
}

void ConnectToWifi() {
    // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    blinkRed(500); // delay 500ms
  }

  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.println("IP address: ");
  ip=WiFi.localIP();
  Serial.println(ip);    
  
}

void GetLocation() {
  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    client.println("GET /json/ HTTP/1.1\r\nHost: freegeoip.net\r\n\r\n");
    Serial.print("\r\nReading response...");
    country[0] = region[0] = city[0] = 0; // Clear data
    jsonParseLocation(0, 0);
  }

  Serial.print(F("\r\nRESULTS:\r\n  Country: "));
  Serial.println(country);
  Serial.print(F("  Region: "));
  Serial.println(region);
  Serial.print(F("  City: "));
  Serial.println(city);
  Serial.print(F("  Longitude: "));
  Serial.println(longitude);
  Serial.print(F("  Latitude: "));
  Serial.println(latitude);

}

boolean jsonParseLocation(int depth, byte endChar) {
  int     c, i;
  boolean readName = true;
  for(;;) {
    while(isspace(c = timedRead())); // Scan past whitespace
    if(c < 0)        return false;   // Timeout
    if(c == endChar) return true;    // EOD

    if(c == '{') { // Object follows
      if(!jsonParse(depth + 1, '}')) return false;
      if(!depth)                     return true; // End of file
    } else if(c == '[') { // Array follows
      if(!jsonParse(depth + 1,']')) return false;
    } else if((c == '"') || (c == '\'')) { // String follows
      if(readName) { // Name-reading mode
        if(!readString(name, sizeof(name)-1, c)) return false;
      } else { // Value-reading mode
        if(!readString(value, sizeof(value)-1, c)) return false;
        // Process name and value strings:
        if       (!strcasecmp(name, "country_name")) {
          strncpy(country, value, sizeof(country)-1);
        } else if(!strcasecmp(name, "region_name")) {
          strncpy(region, value, sizeof(region)-1);
        } else if(!strcasecmp(name, "city")) {
          strncpy(city, value, sizeof(city)-1);
        }
      }
    } else if(c == ':') { // Separator between name:value
      readName = false; // Now in value-reading mode
      value[0] = 0;     // Clear existing value data
    } else if(c == ',') { // Separator between name/value pairs
      readName = true; // Now in name-reading mode
      name[0]  = 0;    // Clear existing name data
    } else {
      // Else true/false/null or a number follows.
      value[0] = c;
      if(!strcasecmp(name, "longitude")) {
        if(!readString(value+1, sizeof(value)-1, ',')) return false;
        longitude = atof(value);
      } else if(!strcasecmp(name, "latitude")) {
        if(!readString(value+1, sizeof(value)-1, ',')) return false;
        latitude = atof(value);
      }
      readName = true; // Now in name-reading mode
      name[0]  = 0;    // Clear existing name data
    }
  }
}

// Read from client stream with a 5 second timeout.  Although an
// essentially identical method already exists in the Stream() class,
// it's declared private there...so this is a local copy.
int timedRead(void) {
  unsigned long start = millis();
  while((!client.available()) && ((millis() - start) < 5000L));
  return client.read();  // -1 on timeout
}

// Read string from client stream into destination buffer, up to a maximum
// requested length.  Buffer should be at least 1 byte larger than this to
// accommodate NUL terminator.  Opening quote is assumed already read,
// closing quote will be discarded, and stream will be positioned
// immediately following the closing quote (regardless whether max length
// is reached -- excess chars are discarded).  Returns true on success
// (including zero-length string), false on timeout/read error.
boolean readString(char *dest, int maxLen, char quote) {
  int c, len = 0;

  while((c = timedRead()) != quote) { // Read until closing quote
    if(c == '\\') {    // Escaped char follows
      c = timedRead(); // Read it
      // Certain escaped values are for cursor control --
      // there might be more suitable printer codes for each.
      if     (c == 'b') c = '\b'; // Backspace
      else if(c == 'f') c = '\f'; // Form feed
      else if(c == 'n') c = '\n'; // Newline
      else if(c == 'r') c = '\r'; // Carriage return
      else if(c == 't') c = '\t'; // Tab
      else if(c == 'u') c = unidecode(4);
      else if(c == 'U') c = unidecode(8);
      // else c is unaltered -- an escaped char such as \ or "
    } // else c is a normal unescaped char

    if(c < 0) return false; // Timeout

    // In order to properly position the client stream at the end of
    // the string, characters are read to the end quote, even if the max
    // string length is reached...the extra chars are simply discarded.
    if(len < maxLen) dest[len++] = c;
  }

  dest[len] = 0;
  return true; // Success (even if empty string)
}

// Read a given number of hexadecimal characters from client stream,
// representing a Unicode symbol.  Return -1 on error, else return nearest
// equivalent glyph in printer's charset.  (See notes below -- for now,
// always returns '-' or -1.)
int unidecode(byte len) {
  int c, v, result = 0;
  while(len--) {
    if((c = timedRead()) < 0) return -1; // Stream timeout
    if     ((c >= '0') && (c <= '9')) v =      c - '0';
    else if((c >= 'A') && (c <= 'F')) v = 10 + c - 'A';
    else if((c >= 'a') && (c <= 'f')) v = 10 + c - 'a';
    else return '-'; // garbage
    result = (result << 4) | v;
  }

  return '?';
}

// ******************************************************************** //


void setup() {
  Serial.begin(115200);
  Serial.println("Start");
  blinkSetup();
  ConnectToWifi();
  GetLocation();
  
}

void loop() {
  
}
