/*
   Code for Pressure Plate Nr. 01 (the e-paper ESP8266)

   Send UDP packages via terminal:
   echo -n "hello" >/dev/udp/192.168.179.23/4210 (or any other IP that the ESP has)
   --> echo -n "hello" >/dev/udp/IP/PORT
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>

#define SERIAL_BAUD_NUM   115200

#define   BUTTON_PIN    4
#define   LED_STRIP     5
#define   NUMPIXELS     49   
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, LED_STRIP, NEO_GRB + NEO_KHZ800);

int buttonState = 0;         // variable for reading the pushbutton status

uint32_t black = strip.Color(0, 0, 0);

uint32_t pink = strip.Color(255, 0, 234);           // 0
uint32_t orange = strip.Color(255, 162, 0);         // 1
uint32_t turquoise = strip.Color(0, 255, 145);      // 2
uint32_t red = strip.Color(255, 0, 0);              // 3
uint32_t green = strip.Color(0, 255, 0);            // 4
uint32_t blue = strip.Color(0, 0, 255);             // 5
uint32_t yellow = strip.Color(255, 255, 0);         // 6
uint32_t colors [4] = {pink, green, blue, yellow};
const int amountOfColors = 4;
int currentColor = 0; //can be: 0 - 4


WiFiUDP Udp;
unsigned int localUdpPort = 4210;  // local port to listen on
char incomingPacket[255];  // buffer for incoming packets

time_t lastTimeStepped;

#include "wifiAccessData.h"

//const char* usedIP = laptopIP; 
const char* usedIP = otherESPIP; 

void setup() {
  Serial.begin(SERIAL_BAUD_NUM);

  strip.begin();
  strip.show();
  strip.setBrightness(50); //150 Set BRIGHTNESS to about 1/5 (max = 255)
  setToRandomColor();
  pinMode(BUTTON_PIN, INPUT_PULLUP); 

  Serial.println();

  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  Udp.begin(localUdpPort);
  Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

  // Initialize Time
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

int lastButtonState = 0;
int lastDebounceTime = 0;
int debounceDelay = 50;

void loop() {
    // read the state of the switch into a local variable:
    int reading = digitalRead(BUTTON_PIN);

    // check to see if you just pressed the button
    // (i.e. the input went from LOW to HIGH), and you've waited long enough
    // since the last press to ignore any noise:

    // If the switch changed, due to noise or pressing:
    if (reading != lastButtonState) {
        // reset the debouncing timer
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        // whatever the reading is at, it's been there for longer than the debounce
        // delay, so take it as the actual current state:

        // if the button state has changed:
        if (reading != buttonState) {
            buttonState = reading;

            // only toggle the LED if the new button state is HIGH
            if (buttonState == HIGH) {
                setToRandomColor();
            }
        }
    }

    // set the LED:
    //setToRandomColor();

    // save the reading. Next time through the loop, it'll be the lastButtonState:
    lastButtonState = reading;
}

void loop1() {
  // read the state of the pushbutton value:
  buttonState = digitalRead(BUTTON_PIN);
  
  if (buttonState == LOW) {
  setToRandomColor();
  lastTimeStepped = time(nullptr);
  Serial.println(lastTimeStepped);
  Udp.beginPacket("10.5.10.36", 4210);
  Udp.write("41588765187");
  Udp.endPacket();
  }
}

void loop2() {
  rotateColors();

  // read the state of the pushbutton value:
  buttonState = digitalRead(BUTTON_PIN);
  //-----------------------------GOT STEPPED ON: SEND PACKAGE
  if (buttonState == LOW) {
    Serial.println("Got stepped on");
    //------Save Timestamp
    lastTimeStepped = time(nullptr);
    Serial.println(lastTimeStepped);
    //------ Tell other plate
    sendTimestampAndColorToOtherPlate(lastTimeStepped); //give timestamp + color! check if the got the same color!!!
  }
  //delay(1000);

  //-----------------------------RECEIVE PACKAGES
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    // receive incoming UDP packets
    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    int len = Udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0;
    }
    //------ Read content of package
    Serial.printf("received %s\n", incomingPacket);
    //A: we got a boolean char telling us if we won or not (we got pressed, and send color+timestamp to the other plate to check )
     winnerLights();
    if ((strcmp(incomingPacket, "1") == 0)) {
      winnerLights();
    }
    else if ((strcmp(incomingPacket, "0") == 0)) {
      looserLights();
    }
    //B: we got a color + timestamp (the other plate got pressed, and send a timestamp for us to check)
    else {
      //------ Split into color and timestamp
      //[0]        color
      //[1]-[10]   timestamp
      char receivedColorString = incomingPacket[0];
      int receivedColor = receivedColorString - 48;
      Serial.printf("received color %d\n", receivedColor);

      char receivedTimestampString [10];
      for (int j = 1; j <= 10; j++) {
        receivedTimestampString[j - 1] = incomingPacket[j];
      }
      char *bufferString;
      time_t receivedTimestamp = strtoul(receivedTimestampString, &bufferString, 10);
      Serial.printf("received timestamp %ld\n", receivedTimestamp);
      //------ Check color
      Serial.printf("comparing colors %d and %d\n", receivedColor, currentColor);
      if (receivedColor == currentColor) {
        //------ Check timestamp
        //todo check timestamp
        Serial.printf("comparing timestamps %ld and %ld\n", lastTimeStepped, receivedTimestamp);
        if (difftime(lastTimeStepped, receivedTimestamp) > 0) {
          sendOtherPlateItLost();
          winnerLights();
        } else {
          sendOtherPlateItWon();
          looserLights();
        }
      }
      else {
        sendOtherPlateItLost();
        winnerLights();
      }
    }
  }
}

void sendOtherPlateItWon() {
  Udp.beginPacket(usedIP, localUdpPort);
  Udp.write("1");
  Udp.endPacket();
}

void sendOtherPlateItLost() {
  Udp.beginPacket(usedIP, localUdpPort);
  Udp.write("0");
  Udp.endPacket();
}

void sendTimestampToOtherPlate(time_t timestamp) {
  //time_t value    long int    1583315675
  char timestampBuffer [15];
  snprintf (timestampBuffer, 15, "%ld", timestamp);
  Serial.printf("send %s\n", timestampBuffer);

  Udp.beginPacket(usedIP, localUdpPort);
  Udp.write(timestampBuffer);
  Udp.endPacket();
}

void sendTimestampAndColorToOtherPlate(time_t timestamp) {
  //time_t value    long int    1583315675
  char timestampBuffer [15];
  snprintf (timestampBuffer, 15, "%ld", timestamp);
  //create char of timestamp + current color
  char packageToSend[15];
  sprintf(packageToSend, "%d%s", currentColor, timestampBuffer);

  Serial.println("send package:");
  Serial.println(packageToSend);
  Serial.println("to IP:");
  Serial.println(usedIP);

  Udp.beginPacket(usedIP, localUdpPort);
  Udp.write(packageToSend);
  Udp.endPacket();
}

/**
     Rotate all the colors every x seconds.
     Using millis(), this won' stop the program
*/
int waitTime = 30000; //10 seconds
unsigned long timeNow = 0;
void rotateColors() {
  if ((unsigned long)(millis() - timeNow) > waitTime) {
    timeNow = millis();
    setToRandomColor();
  }
}

void setToRandomColor() {
  int randNumber = random(0, amountOfColors);
  strip.fill( colors[randNumber], 0, strip.numPixels() - 1);
  currentColor = randNumber;
  Serial.printf("set current color %d\n", currentColor);
  strip.show();
}


void winnerLights() {
  for (int rounds = 5; rounds >= 0; rounds--) {
    for (int i = 0; i <= amountOfColors; i++) {
      strip.fill( colors[i], 0, strip.numPixels() - 1);
      strip.show();
      delay(50);
    }
  }
  strip.fill( colors[currentColor], 0, strip.numPixels() - 1);
  strip.show();
}

void looserLights() {
  strip.fill(red, 0, strip.numPixels() - 1);
  strip.show();
  //delay(10);
  for (int i = 255; i >= 0; i--) {
    strip.fill( strip.Color(i, 0, 0), 0, strip.numPixels() - 1);
    strip.show();
    delay(5);
  }
  //delay(500);
  strip.fill( colors[currentColor], 0, strip.numPixels() - 1);
  strip.show();
}
