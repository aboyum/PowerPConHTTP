/*
 *  The ESP8266 Internet Connection Alarm
 *  by @3zuli
 *  based off the /ESP8266WiFi/WiFiClient example
 *  
 *  This program continually tries to connect to a website and when there's an error
 *  it turns on a 12V warning light / strobe / siren / whatever else it's connected to.
 *
 *  Circuit:
 *  ESP8266 wired up according to <ADD ESP WIRING GUIDE>
 *    -> GPIO02 connected to Gate of IRF540N mosfet through a 560R resistor
 *  Mosfet Source connected to GND, GND of 12V supply also connected to GND
 *  12V input connected to + lead of the strobe, - lead of the strobe connected to Drain of the mosfet
 *  todo: schematic
 *  https://www.instructables.com/ESP8266-Internet-Alarm/
 * 
 * https://github.com/SilverFire/esp8266-pc-power-control/blob/master/src/firmware.ino
 * https://gitlab.com/snippets/1985496
 * https://lerks.blog/turning-a-pc-on-and-off-using-an-esp/ 
 * 
 * https://github.com/aboyum/PowerPConHTTP
 *
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

// Select mode of operation by uncommenting one of the following lines
// Default: DETECT_NO_INTERNET
//#define DETECT_INTERNET    // Turn on the light when there IS internet connnection
#define DETECT_NO_INTERNET // Turn on the light when there is NO internet connection


// How long the PowerOFF button should be pressed to power off PC forcefully
#define PWR_OFF_TIME 5000
// How long the button should be pressed to REBOOT, POWER ON or RESET
#define PUSH_TIME 400

// Pin for the PWR signal line
#define PWR_PIN 13
// Pin for the RST signal line
#define RST_PIN 12
// Pin for the status LED signal line
#define STATUS_PIN 16



// Your WiFi network credentials
const char* ssid     = "INSERT_YOUR_SSID;
const char* password = "INSERT_YOUR_PASSWORD";

// The URL we will use to test our connection
// We'll be connecting to http://httpbin.org/get , the "get" part is specified later in the code
// httpbin returns a very small amount of data, therefore should work
// even on the slowest connection (EDGE/2G)
const char* host = "httpbin.org";

// Global variable to track our connection status
bool hasInternet = false; 

// The ESP pin on which the strobe light is connected
const int strobePin = 02; //GPIO2 is D4 on D1mini

// Set to true to enable debug printouts
const bool debug = false;

ADC_MODE(ADC_VCC);
ESP8266WebServer server(WiFi.softAPIP(), 80);

int counter = 0;
int counter2 = 0;

void failLoop() {
  while(true) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);                       
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
  }
}

void togglePin(int pin, int ms) {
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
  delay(ms);
  pinMode(pin, INPUT);
}

bool isPoweredOn() {
  return digitalRead(STATUS_PIN) == HIGH;
}

String getStatusString() {
  return String(isPoweredOn() ? "ON" : "OFF") + ", " + 
         WiFi.softAPIP().toString() + ", " + 
         ESP.getVcc()/1024.00f + "V";
}

void do_powerOffForce() {
  digitalWrite(LED_BUILTIN, LOW);
  togglePin(PWR_PIN, PWR_OFF_TIME);
  digitalWrite(LED_BUILTIN, HIGH);
}

void do_powerOn() {
  if(!isPoweredOn()) {
    digitalWrite(LED_BUILTIN, LOW);
    togglePin(PWR_PIN, PUSH_TIME);
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    server.send(409, "text/plain", "ERR, " + getStatusString());
  }
}

void do_powerOff() {
  if(isPoweredOn()) {
    digitalWrite(LED_BUILTIN, LOW);
    togglePin(PWR_PIN, PUSH_TIME);
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    server.send(409, "text/plain", "ERR, " + getStatusString());
  }
}

void do_reset() {
  if(isPoweredOn()) {
    digitalWrite(LED_BUILTIN, LOW);
    togglePin(RST_PIN, PUSH_TIME);
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    server.send(409, "text/plain", "ERR, " + getStatusString());
  }
}

void configureEndpoints() {
  server.on("/pcswitch/pwr_off_force/", [] () {
    do_powerOffForce();
    server.send(200, "text/plain", "PWR_OFF_FORCE, " + getStatusString());
  });

  server.on("/pcswitch/pwr_off/", [] () {
    do_powerOff();
    server.send(200, "text/plain", "PWR_OFF, " + getStatusString());
  });

  server.on("/pcswitch/pwr_on/", [] () {
    do_powerOn();
    server.send(200, "text/plain", "PWR_ON, " + getStatusString());
  });

  server.on("/pcswitch/reset/", [] () {
    if(isPoweredOn()) {
      do_reset();
      server.send(200, "text/plain", "RESET, " + getStatusString());
    } else {
      server.send(409, "text/plain", "RESET, " + getStatusString());
    }
  });

  server.on("/pcswitch/status/", [] () {
    server.send(200, "text/plain", "STATUS, " + getStatusString());
  });

  server.onNotFound([] () {
    server.send(404, "text/plain", "Not Found");
  });
}

void led(int pin){
#if defined(DETECT_INTERNET)
    digitalWrite(pin,hasInternet);
    if(debug){Serial.print("Setting LED to "); Serial.println(hasInternet);}
#elif defined(DETECT_NO_INTERNET)
    digitalWrite(pin,!hasInternet);
    if(debug){Serial.print("Setting LED to "); Serial.println(!hasInternet);}
#else
    digitalWrite(pin,!hasInternet);
    if(debug){Serial.print("Setting LED to "); Serial.println(!hasInternet);}
#endif
}
//// led(strobePin);


int connectWifi(int retryDelay=500){
  // Retry connection to the specified network until success.
  // @param int retryDelay: Time in milliseconds to wait between status checking (default 500ms)
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  //Assume we aren't connected and turn on the strobe
  hasInternet=false; 
  led(strobePin);
  
  //Start connecting, the connection process is done by the ESP in the background
  WiFi.begin(ssid, password); 
  
  // values of WiFi.status() (wl_status_t) defined in wl_definitions.h located in (Windows):
  // %AppData%\Roaming\Arduino15\packages\esp8266\hardware\esp8266\1.6.5-947-g39819f0\libraries\ESP8266WiFi\src\include
  // Mac & Linux will vary
  
  wl_status_t wifiStatus = WL_IDLE_STATUS; //Assume nothing
  while (wifiStatus != WL_CONNECTED) {
    // While the ESP is connecting, we can periodicaly check the connection status using WiFi.status()
    // We keep checking until ESP has successfuly connected
    wifiStatus = WiFi.status();
    switch(wifiStatus){
      // Print the error status we are getting
      case WL_NO_SSID_AVAIL:
          Serial.println("SSID not available");
          hasInternet=false;
          counter++;
          delay(500);
          if (counter > 15)
          {
              counter2++;
              Serial.println("counter " + String(counter) + " counter2 " + String(counter2));
              if (counter2 == 20){
                  do_powerOn();
              }
          }
          break;
      case WL_CONNECT_FAILED:
          Serial.println("Connection failed");
          hasInternet=false;
          counter++;
          delay(500);
          if (counter > 15)
          {
              counter2++;
              Serial.println("counter " + String(counter) + " counter2 " + String(counter2));
              if (counter2 == 20){
                  do_powerOn();
              }
          }
          break;
      case WL_CONNECTION_LOST:
          Serial.println("Connection lost");
          hasInternet=false;
          counter++;
          delay(500);
          if (counter > 15)
          {
              counter2++;
              Serial.println("counter " + String(counter) + " counter2 " + String(counter2));
              if (counter2 == 20){
                  do_powerOn();
              }
          }
          break;
      case WL_DISCONNECTED:
          Serial.println("WiFi disconnected");
          hasInternet=false;
          counter++;
          delay(500);
          if (counter > 15)
          {
              counter2++;
              Serial.println("counter " + String(counter) + " counter2 " + String(counter2));
              if (counter2 == 20){
                  do_powerOn();
              }
          }
          break;
    }
    delay(retryDelay);
  }
  
  // Here we are connected, we can turn off the strobe & print our IP address
  hasInternet = true;
  led(strobePin);
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  pinMode(strobePin,OUTPUT); // Set strobe pin as OUTPUT
  led(strobePin);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(STATUS_PIN, INPUT_PULLDOWN_16);
  delay(10);
  
  // Print out current configuration
  Serial.println();
#if defined(DETECT_INTERNET)
  Serial.println("Operation mode: DETECT_INTERNET");
#elif defined(DETECT_NO_INTERNET)
  Serial.println("Operation mode: DETECT_NO_INTERNET");
#else
  Serial.println("Operation mode: DETECT_NO_INTERNET");
#endif

  // We start by connecting to a WiFi network
  connectWifi(500);
  configureEndpoints();
  server.begin();
}



void loop() 
{
  // Loop/check connection every 5 seconds
  delay(5000);
  if(debug){Serial.print("hasInternet "); Serial.println(hasInternet);}
  // Turn on/off the strobe according to current connection state
  led(strobePin);
  
  wl_status_t wifiStatus = WiFi.status();
  if(wifiStatus != WL_CONNECTED)
  {
      // If we lost connection to our WiFi, reconnect
      Serial.println("Lost WiFi connection, reconnecting...");
      connectWifi(500); // (strobe is activated inside this function)
  }
  else Serial.println("WiFi OK"); // Otherwise we are connected

  Serial.print("Connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connection
  WiFiClient client;
  const int httpPort = 80;
  // Attempt to connect to httpbin.org:80 (http)
  if (!client.connect(host, httpPort)) 
  {
    counter++;
    if (counter > 15)
    {
        // If we can't connect, we obviously don't have internet
        Serial.println("Connection failed!!!");
        // Set hasInternet to false, turn on the strobe, 
        // exit the loop() function and wait for the next round
        hasInternet=false;
        led(strobePin);
        counter2++;
        Serial.println("counter " + String(counter) + " counter2 " + String(counter2));
        if (counter2 == 20)
        {
            do_powerOn();
        }
        return;
    }
  }

  HTTPClient http;  //Declare an object of class HTTPClient
  
  http.begin("http://httpbin.org/get");  //Specify request destination
  int httpCode = http.GET();     //Send the request
   
  if (httpCode > 0) 
  { //Check the returning code
   
    String payload = http.getString();   //Get the request response payload
    Serial.println(payload);                     //Print the response payload
    if (payload != "")
    {
      counter2 = 0;
      counter = 0;
      Serial.println("counter " + String(counter) + " counter2 " + String(counter2));
    }
    else
    {
      counter++;
      if (counter > 15)
      {
          // If we can't connect, we obviously don't have internet
          Serial.println("Connection failed!!!");
          // Set hasInternet to false, turn on the strobe, 
          // exit the loop() function and wait for the next round
          hasInternet=false;
          led(strobePin);
          counter2++;
          Serial.println("counter " + String(counter) + " counter2 " + String(counter2));
          if (counter2 == 20)
          {
              do_powerOn();
          }
          return;
      }
    }
  }
   
  http.end();   //Close connection
 



}
