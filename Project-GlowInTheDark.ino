// --- Includes ---
#include <Arduino.h>

#include <ESP8266WiFi.h> // for setting up the softAP
#include <WiFiClient.h>
#include <ESP8266WebServer.h> // for setting up server
// #include <ESPAsyncTCP.h>
// #include <ESPAsyncWebServer.h>
#include <FS.h>
#include <uri/UriRegex.h>
#include <Adafruit_DotStar.h> // for the leds
#include <SPI.h>

#include "Config.h"

// --- Variables ---
ESP8266WebServer server(80);

String output0State = "off";

Adafruit_DotStar strip =
    Adafruit_DotStar(NUMPIXELS, DOTSTAR_DATA_PIN, DOTSTAR_CLOCK_PIN, DOTSTAR_BRG);
int16_t head = 0;   // Index of first 'on' pixel
int16_t tail = -10; // Index of first 'off' pixel
// Set some default values

uint32_t color = 0xFF0000;                                                              // 'On' color (starts red)
uint8_t leds_to_skip = 1;                                                               // Number of LEDs to skip between each LED
uint32_t pixelValues[6] = {0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000}; // if custom pattern, pixel values

uint8_t brightnessValue = 50;   // Brightness
uint8_t delayValue = 20;        // Delay between frames
String patternValue = "strand"; // Pattern to display : rainbow, strand, custom, allsame
long firstPixelHue = 0;
// --- Public function definitions ---

// Initial setups
void setup()
{
  // Setup LED strip
  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off ASAP

  // Initialize the output variables as outputs
  pinMode(LED_PIN, OUTPUT);
  // Set outputs to LOW
  digitalWrite(LED_PIN, LOW);

  // Connect serial port
  Serial.begin(115200);
  delay(10);
  Serial.println('\n');

  //  Create a wifi AP
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(SSID) ? "Ready" : "Failed!");

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());

  // Start the SPI Flash Files System
  SPIFFS.begin();

  server.onNotFound(handleNotFound);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_GET, parseQueryString);
  server.on("/reset", HTTP_POST, []()
            {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
      ESP.restart(); });
  /*
  We need to handle get requests to the /set path. here is a sample uri
  /set?brightness=50&delay=33&spacing=1&pattern=strand&pixel1=%23000000&pixel2=%23000000&pixel3=%23000000&pixel4=%23000000&pixel5=%23000000&pixel6=%23000000
  */
  // Actually start the server
  server.begin();
  Serial.println("HTTP server started");
}

// Main loop
void loop()
{
  server.handleClient();
  handleLedStrip();
}

// --- Local function definitions ---
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\narg: ";
  message += server.arg("plain");
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  Serial.println(message);
}

void handleRoot()
{
  // If the file exists, open it, send it to the client, close the file
  String path = "/index.html";
  if (SPIFFS.exists(path))
  {
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, "text/html");
    file.close();
  }
  else
  {
    // If the file doesn't exist, return false
    Serial.println("\tFile Not Found");
  }
}

static void handleLedStrip()
{
  // set brightness
  //  Serial.println(" Handle LED Strip");
  //  Serial.println("brightnessValue: " + String(brightnessValue));
  //  Serial.println("delayValue: " + String(delayValue));
  //  Serial.println("leds_to_skip: " + String(leds_to_skip));
  //  Serial.println("patternValue: " + patternValue);
  strip.setBrightness(brightnessValue);

  if (patternValue == "rainbow")
  {
    rainbowPattern(delayValue);
  }
  else if (patternValue == "strand")
  {
    strandedPattern(delayValue);
  }
  else if (patternValue == "custom")
  {
    customPattern(delayValue);
  }
  else
  {
    // set all pixels to the same color
    for (int i = 0; i < NUMPIXELS; i++)
    {
      strip.setPixelColor(i, color);
    }
    strip.show();
    delay(delayValue);
  }
}

static void strandedPattern(int wait)
{
  strip.setPixelColor(head, color);
  strip.setPixelColor(tail, 0);
  strip.show();
  delay(wait*(leds_to_skip + 1)); // Pause ; 20 milliseconds ~50 FPS
  head = head + (leds_to_skip + 1);
  // set up state for next cycle
  if(head >= NUMPIXELS) {         // Increment head index.  Off end of strip?
    head = 0;                       //  Yes, reset head index to start
    if((color >>= 8) == 0)          //  Next color (R->G->B) ... past blue now?
      color = 0xFF0000;             //   Yes, reset to red
  }
  tail = tail + (leds_to_skip + 1);
  if(tail >= NUMPIXELS) tail = 0; // Increment, reset tail index
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
// From https://github.com/adafruit/Adafruit_DotStar/blob/master/examples/onboard/onboard.ino
void rainbowPattern(int wait)
{
  Serial.println("Rainbow Pattern");
  // Color wheel has a range of 65536
  // just count from 0 to 65536. Adding 256 to firstPixelHue each time
  firstPixelHue += 256;
  if (firstPixelHue >= 65536){
    firstPixelHue = 0;
  }
  for (int i = 0; i < strip.numPixels(); i = i + (leds_to_skip + 1))
    { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / (strip.numPixels()*(leds_to_skip+1)));
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    };
  strip.show(); // Update strip with new contents
  delay(wait);  // Pause for a moment
}

static void customPattern(int wait)
{
  for (int i = 0; i < NUMPIXELS; i += (leds_to_skip + 1))
  {
    strip.setPixelColor(i, pixelValues[i]);
  }
  strip.show();
  delay(wait);
}

void parseQueryString()
{
  /*
  We need to handle get requests to the /set path. here is a sample uri
  /set?brightness=50&delay=33&spacing=1&pattern=strand&pixel1=%23000000&pixel2=%23000000&pixel3=%23000000&pixel4=%23000000&pixel5=%23000000&pixel6=%23000000
  This we expect to be parsed into 10 arguments:
    arg1: brightness: 50
    arg2: delay: 20
    arg3: spacing: 0
    arg4: pattern: rainbow
    arg5: pixel1: #000000
    arg6: pixel2: #000000
    arg7: pixel3: #000000
    arg8: pixel4: #000000
    arg9: pixel5: #000000
    arg10: pixel6: #000000

   */

  Serial.println("Parsing . . . ");
  brightnessValue = server.arg(0).toInt();
  delayValue = server.arg(1).toInt();
  leds_to_skip = server.arg(2).toInt();
  patternValue = server.arg(3);
  if (patternValue == "custom")
  {
    pixelValues[0] = hex_string_to_int(server.arg(4));
    pixelValues[1] = hex_string_to_int(server.arg(5));
    pixelValues[2] = hex_string_to_int(server.arg(6));
    pixelValues[3] = hex_string_to_int(server.arg(7));
    pixelValues[4] = hex_string_to_int(server.arg(8));
    pixelValues[5] = hex_string_to_int(server.arg(9));
  }
  Serial.println("Got ::: ");
  Serial.println("brightnessValue: " + String(brightnessValue));
  Serial.println("delayValue: " + String(delayValue));
  Serial.println("leds_to_skip: " + String(leds_to_skip));
  Serial.println("patternValue: " + patternValue);
  Serial.println("pixelValues[0]: " + pixelValues[0]);
  Serial.println("pixelValues[1]: " + pixelValues[1]);
  Serial.println("pixelValues[2]: " + pixelValues[2]);
  Serial.println("pixelValues[3]: " + pixelValues[3]);
  Serial.println("pixelValues[4]: " + pixelValues[4]);
  Serial.println("pixelValues[5]: " + pixelValues[5]);

  // send 200 and redirect to root
  strip.clear();
  server.send(200, "text/plain", "OK");

  handleRoot();
}

uint32_t hex_string_to_int(String hex_string)
{
  /*
  We need to convert #000000 ( string ) to 0x000000 ( int )
  we can use strtol() to do this
  strtol(&hex_string[1], NULL, 16):
  This is a function call that converts a string to a long integer.
  The arguments are as follows:

  &hex_string[1]: This takes the memory address of
    the second character in the string (skipping the initial #).
  NULL: This argument is for the endptr parameter,
    which is not being used in this case.
  16: This indicates that the input string is in base-16 (hexadecimal)

  */
  uint32_t hex_int = (uint32_t)strtol(&hex_string[1], NULL, 16);
  return hex_int;
}
