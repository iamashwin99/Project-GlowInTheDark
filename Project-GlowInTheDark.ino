// --- Includes ---
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>

#include <Adafruit_DotStar.h>
#include <SPI.h>


#include "Config.h"

// --- Variables ---
ESP8266WebServer server(80);

String output0State = "off";

Adafruit_DotStar strip =
    Adafruit_DotStar(NUMPIXELS, DOTSTAR_DATA_PIN, DOTSTAR_CLOCK_PIN, DOTSTAR_BRG);
int16_t head = 0;    // Index of first 'on' pixel
int16_t tail = -10;  // Index of first 'off' pixel
uint32_t color = 0xFF0000;  // 'On' color (starts red)
uint8_t leds_to_skip = 0;   // Number of LEDs to skip between each LED

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
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASSWORD);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);


  // Start the SPI Flash Files System
  SPIFFS.begin();

  // If the client requests any URI, send it if it exists, otherwise, respond with a 404 (Not Found) error
  server.onNotFound([]()
  {
    if (!handleFileRead(server.uri()))
    server.send(404, "text/plain", "404: Not Found");
  });

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

static void handleLedStrip()
{
  rainbow(20) 
}

static void strandedPattern(int wait)
{
  strip.setPixelColor(head, color);
  strip.setPixelColor(tail, 0);
  strip.show();
  delay(wait);
  if (leds_to_skip > 0)
  {
    for (uint8_t i = 0; i < leds_to_skip; i++)
    {
      if (++head >= NUMPIXELS)
        head = 0;
      if (++tail >= NUMPIXELS)
        tail = 0;
    }
  }
  if ((color >>= 8) == 0)
  color = 0xFF0000;
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
// From https://github.com/adafruit/Adafruit_DotStar/blob/master/examples/onboard/onboard.ino
void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i=i+leds_to_skip) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}


// convert the file extension to the MIME type
static String getContentType(String filename)
{
  if (filename.endsWith(".html"))
  {
    return "text/html";
  }
  else if (filename.endsWith(".css"))
  {
    return "text/css";
  }
  else if (filename.endsWith(".js"))
  {
    return "application/javascript";
  }
  else if (filename.endsWith(".ico"))
  {
    return "image/x-icon";
  }
  return "text/plain";
}


/* Send the right file to the client (if it exists)
 * Return true if file exists else false
 */
static bool handleFileRead(String path)
{
  bool ret;

  Serial.println("handleFileRead: " + path);

  // Turns the GPIOs on and off
  if (path.indexOf("GET /0/on") >= 0)
  {
    Serial.println("GPIO 0 on");
    output0State = "on";
    digitalWrite(LED_PIN, HIGH);
  }
  else if (path.indexOf("GET /0/off") >= 0)
  {
    Serial.println("GPIO 0 off");
    output0State = "off";
    digitalWrite(LED_PIN, LOW);
  }

  // If a folder is requested, send the index file
  if (path.endsWith("/"))
  {
    path += "index.html";
  }

  // Get the MIME type
  String contentType = getContentType(path);

  // If the file exists, open it, send it to the client, close the file
  if (SPIFFS.exists(path))
  {
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    ret = true;
  }
  else
  {
    // If the file doesn't exist, return false
    Serial.println("\tFile Not Found");
    ret = false;
  }

  return ret;
}
