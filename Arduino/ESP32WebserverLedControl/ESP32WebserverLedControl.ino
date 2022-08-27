// ---------------------------------------------------------------------------------------
//
// Code for a webserver on the ESP32 to control LEDs (device used for tests: ESP32-WROOM-32D).
// The code allows user to switch between three LEDs and set the intensity of the LED selected
//
// For installation, the following libraries need to be installed:
// * Websockets by Markus Sattler (can be tricky to find -> search for "Arduino Websockets"
// * ArduinoJson by Benoit Blanchon
//
// Written by mo thunderz (last update: 19.11.2021)
//
// ---------------------------------------------------------------------------------------

#include <WiFi.h>                                     // needed to connect to WiFi
#include <WebServer.h>                                // needed to create a simple webserver (make sure tools -> board is set to ESP32, otherwise you will get a "WebServer.h: No such file or directory" error)
#include <WebSocketsServer.h>                         // needed for instant communication between client and server through Websockets
#include <ArduinoJson.h>                              // needed for JSON encapsulation (send multiple variables with one string)

// SSID and password of Wifi connection:
//const char* ssid = "TYPE_YOUR_SSID_HERE";
//const char* password = "TYPE_YOUR_PASSWORD_HERE";
#include "WifiPassword.h"

// The String below "webpage" contains the complete HTML code that is sent to the client whenever someone connects to the webserver
// NOTE 27.08.2022: I updated in the webpage "slider.addEventListener('click', slider_changed);" to "slider.addEventListener('change', slider_changed);" -> the "change" did not work on my phone.
String webpage = "<!DOCTYPE html><html><head><title>Page Title</title></head><body style='background-color: #EEEEEE;'><span style='color: #003366;'><h1>LED Controller</h1><form> <p>Select LED:</p> <div> <input type='radio' id='ID_LED_0' name='operation_mode'> <label for='ID_LED_0'>LED 0</label> <input type='radio' id='ID_LED_1' name='operation_mode'> <label for='ID_LED_1'>LED 1</label> <input type='radio' id='ID_LED_2' name='operation_mode'> <label for='ID_LED_2'>LED 2</label> </div></form><br>Set intensity level: <br><input type='range' min='1' max='100' value='50' class='slider' id='LED_INTENSITY'>Value: <span id='LED_VALUE'>-</span><br></span></body><script> document.getElementById('ID_LED_0').addEventListener('click', led_changed); document.getElementById('ID_LED_1').addEventListener('click', led_changed); document.getElementById('ID_LED_2').addEventListener('click', led_changed); var slider = document.getElementById('LED_INTENSITY'); var output = document.getElementById('LED_VALUE'); slider.addEventListener('change', slider_changed); var Socket; function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processCommand(event); }; } function led_changed() {var l_LED_selected = 0;if(document.getElementById('ID_LED_1').checked == true) { l_LED_selected = 1;} else if(document.getElementById('ID_LED_2').checked == true) { l_LED_selected = 2;}console.log(l_LED_selected); var msg = { type: 'LED_selected', value: l_LED_selected};Socket.send(JSON.stringify(msg)); } function slider_changed () { var l_LED_intensity = slider.value;console.log(l_LED_intensity);var msg = { type: 'LED_intensity', value: l_LED_intensity};Socket.send(JSON.stringify(msg)); } function processCommand(event) {var obj = JSON.parse(event.data); var type = obj.type;if (type.localeCompare(\"LED_intensity\") == 0) { var l_LED_intensity = parseInt(obj.value); console.log(l_LED_intensity); slider.value = l_LED_intensity; output.innerHTML = l_LED_intensity;}else if(type.localeCompare(\"LED_selected\") == 0) { var l_LED_selected = parseInt(obj.value); console.log(l_LED_selected); if(l_LED_selected == 0) { document.getElementById('ID_LED_0').checked = true; } else if (l_LED_selected == 1) { document.getElementById('ID_LED_1').checked = true; } else if (l_LED_selected == 2) { document.getElementById('ID_LED_2').checked = true; }} } window.onload = function(event) { init(); }</script></html>";

// global variables of the LED selected and the intensity of that LED
int LED_selected = 0;
int LED_intensity = 50;

// init PINs: assign any pin on ESP32
int led_pin_0 = 4;
int led_pin_1 = 0;
int led_pin_2 = 2;     // On some ESP32 pin 2 is an internal LED, mine did not have one

// some standard stuff needed to do "analogwrite" on the ESP32 -> an ESP32 does not have "analogwrite" and uses ledcWrite instead
const int freq = 5000;
const int led_channels[] = {0, 1, 2};
const int resolution = 8;

// The JSON library uses static memory, so this will need to be allocated:
// -> in the video I used global variables for "doc_tx" and "doc_rx", however, I now changed this in the code to local variables instead "doc" -> Arduino documentation recomends to use local containers instead of global to prevent data corruption

// Initialization of webserver and websocket
WebServer server(80);                                 // the server uses port 80 (standard port for websites
WebSocketsServer webSocket = WebSocketsServer(81);    // the websocket uses port 81 (standard port for websockets

void setup() {
  Serial.begin(115200);                               // init serial port for debugging

  // setup LED channels
  ledcSetup(led_channels[0], freq, resolution);       
  ledcSetup(led_channels[1], freq, resolution);
  ledcSetup(led_channels[2], freq, resolution);

  // attach the channels to the led_pins to be controlled
  ledcAttachPin(led_pin_0, led_channels[0]);
  ledcAttachPin(led_pin_1, led_channels[1]);
  ledcAttachPin(led_pin_2, led_channels[2]);  

  WiFi.begin(ssid, password);                         // start WiFi interface
  Serial.println("Establishing connection to WiFi with SSID: " + String(ssid));     // print SSID to the serial interface for debugging
 
  while (WiFi.status() != WL_CONNECTED) {             // wait until WiFi is connected
    delay(1000);
    Serial.print(".");
  }
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());                     // show IP address that the ESP32 has received from router
  
  server.on("/", []() {                               // define here wat the webserver needs to do
    server.send(200, "text/html", webpage);           //    -> it needs to send out the HTML string "webpage" to the client
  });
  server.begin();                                     // start server
  
  webSocket.begin();                                  // start websocket
  webSocket.onEvent(webSocketEvent);                  // define a callback function -> what does the ESP32 need to do when an event from the websocket is received? -> run function "webSocketEvent()"

  ledcWrite(led_channels[LED_selected], map(LED_intensity, 0, 100, 0, 255));    // write initial state to LED selected
}

void loop() {
  server.handleClient();                              // Needed for the webserver to handle all clients
  webSocket.loop();                                   // Update function for the webSockets 
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {      // the parameters of this callback function are always the same -> num: id of the client who send the event, type: type of message, payload: actual data sent and length: length of payload
  switch (type) {                                     // switch on the type of information sent
    case WStype_DISCONNECTED:                         // if a client is disconnected, then type == WStype_DISCONNECTED
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:                            // if a client is connected, then type == WStype_CONNECTED
      Serial.println("Client " + String(num) + " connected");
      
      // send LED_intensity and LED_select to clients -> as optimization step one could send it just to the new client "num", but for simplicity I left that out here
      sendJson("LED_intensity", String(LED_intensity));
      sendJson("LED_selected", String(LED_selected));
      break;
    case WStype_TEXT:                                 // if a client has sent data, then type == WStype_TEXT
      // try to decipher the JSON string received
      StaticJsonDocument<200> doc;                    // create JSON container 
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      else {
        // JSON string was received correctly, so information can be retrieved:
        const char* l_type = doc["type"];
        const int l_value = doc["value"];
        Serial.println("Type: " + String(l_type));
        Serial.println("Value: " + String(l_value));

        // if LED_intensity value is received -> update and write to LED
        if(String(l_type) == "LED_intensity") {
          LED_intensity = int(l_value);
          sendJson("LED_intensity", String(l_value));
          ledcWrite(led_channels[LED_selected], map(LED_intensity, 0, 100, 0, 255));
        }
        // else if LED_select is changed -> switch on LED and switch off the rest
        if(String(l_type) == "LED_selected") {
          LED_selected = int(l_value);
          sendJson("LED_selected", String(l_value));
          for (int i = 0; i < 3; i++) {
            if (i == LED_selected)
              ledcWrite(led_channels[i], map(LED_intensity, 0, 100, 0, 255));       // switch on LED
            else
              ledcWrite(led_channels[i], 0);                                        // switch off not-selected LEDs 
          }
        }
      }
      Serial.println("");
      break;
  }
}

// Simple function to send information to the web clients
void sendJson(String l_type, String l_value) {
    String jsonString = "";                           // create a JSON string for sending data to the client
    StaticJsonDocument<200> doc;                      // create JSON container
    JsonObject object = doc.to<JsonObject>();         // create a JSON Object
    object["type"] = l_type;                          // write data into the JSON object -> I used "type" to identify if LED_selected or LED_intensity is sent and "value" for the actual value
    object["value"] = l_value;
    serializeJson(doc, jsonString);                // convert JSON object to string
    webSocket.broadcastTXT(jsonString);               // send JSON string to all clients
}
