/* 
 * Author: Sam Faull
 * Details: WiFi enabled lamp
 *          ESP8266 WiFi module & WS281B RGB LEDs 
 * 
 * Pin allocations: 
 * NA
 */
 
/*
 Basic ESP8266 MQTT example

 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.

 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" whenever the button is pressed
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off

 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <config.h> // this stores the private variables such as wifi ssid and password etc.
#include <NeoPixelBrightnessBus.h> // instead of NeoPixelBus.h

#define BRIGHTNESS 100

#define BUTTON D3               //button on pin D3
#define BUTTON_CHECK_TIMEOUT 50 //check for button pressed every 50ms

WiFiClient espClient;
PubSubClient client(espClient);

long buttonTime = 0; // stores the time that the button was depressed for
long lastPushed = 0; // stores the time when button was last depressed
long lastCheck = 0;  // stores the time when last checked for a button press
char msg[50];        // message buffer
uint8_t rgbval[3] = {0,0,0};
char rgb[9] = {};

// Flags
bool button_pressed = false; // true if a button press has been registered
bool button_released = false; // true if a button release has been registered


const uint16_t PixelCount = 60; // this example assumes 3 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 12;  // make sure to set this to the correct pin, ignored for Esp8266

#define colorSaturation 255 // saturation of color constants
RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor off(0,0,0);
RgbColor white(colorSaturation, colorSaturation, colorSaturation);

NeoPixelBrightnessBus<NeoRgbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

void setup_wifi() 
{
  delay(10);

  // this resets all the neopixels to an off state
  strip.Begin();
  strip.Show();

  delay(10);
  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) 
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == 'c') 
  {
    for (int i=1; i<length; i++)
    {
      rgb[i-1] = (int)payload[i] - 48;
    }
    
    rgbval[0] = rgb[0]*100 + rgb[1]*10 + rgb[2];
    rgbval[1] = rgb[3]*100 + rgb[4]*10 + rgb[5];
    rgbval[2] = rgb[6]*100 + rgb[7]*10 + rgb[8];

    RgbColor colour(rgbval[0],rgbval[1],rgbval[2]);
    setColour();
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    /*
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    */
    // Attempt to connect
    if (client.connect("LampNode01", MQTTuser, MQTTpassword)) 
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("/test/outTopic", "ESP8266Client connected");  // potentially not necessary
      // ... and resubscribe
      client.subscribe("/test/inTopic");
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void readInputs(void)
{
  static bool button_state, last_button_state = false; // Remembers the current and previous button states
  
  button_state = !digitalRead(BUTTON); // read button state (active low)
  
  if (button_state && !last_button_state) // on a rising edge we register a button press
    button_pressed = true;
    
  if (!button_state && last_button_state) // on a falling edge we register a button press
    button_released = true;

  last_button_state = button_state;
}

void setColour(void)
{
    for (uint8_t i=0; i<PixelCount; i++)
    {
      strip.SetPixelColor(i, colour);
    }
    strip.Show();
}

void setup() 
{
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(BUTTON, INPUT_PULLUP);  // Enables the internal pull-up resistor
  Serial.begin(115200);
  setup_wifi();
  client.setServer(MQTTserver, MQTTport);
  client.setCallback(callback);
}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();

  strip.SetBrightness(BRIGHTNESS);

  long now = millis();  // get elapsed time
  
  if (now - lastCheck > BUTTON_CHECK_TIMEOUT) // check for button press periodically
  {
    lastCheck = now;
    readInputs();
    
    if (button_pressed)
    {
      //start conting
      lastPushed = now; // start the timer 
      Serial.print("Button pushed... ");
      button_pressed = false;
    }
    
    if (button_released)
    {
      Serial.println("Button released.");

      //get the time that button was held in
      buttonTime = now - lastPushed;

      snprintf (msg, 75, "hello world #%ld", buttonTime);
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish("/test/outTopic", msg);
      button_released = false;
    }
  }
}
