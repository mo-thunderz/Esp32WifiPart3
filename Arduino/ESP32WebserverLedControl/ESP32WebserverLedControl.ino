// ---------------------------------------------------------------------------------------
//
// Code for a webserver on the ESP32 to control LEDs (device used for tests: ESP32-WROOM-32D).
// The code allows user to switch between three LEDs and set the intensity of the LED selected
//
// For installation, the following libraries need to be installed:
// * Websockets by Markus Sattler (can be tricky to find -> search for "Arduino Websockets"
// * ArduinoJson by Benoit Blanchon
//
// Written by mo thunderz (last update: 25.10.2021)
//
// ---------------------------------------------------------------------------------------

#include <WiFi.h>                                     // needed to connect to WiFi
#include <WebServer.h>                                // needed to create a simple webserver
#include <WebSocketsServer.h>                         // needed for instant communication between client and server through Websockets
#include <ArduinoJson.h>                              // needed for JSON encapsulation (send multiple variables with one string)

// SSID and password of Wifi connection:
const char* ssid = "TYPE_YOUR_SSID_HERE";
const char* password = "TYPE_YOUR_PASSWORD_HERE";

// The String below "webpage" contains the complete HTML code that is sent to the client whenever someone connects to the webserver
String webpage = "<!DOCTYPE html><html><head><title>Page Title</title></head><body style='background-color: #EEEEEE;'><span style='color: #003366;'><h1>LED controller</h1><form> <p>Select LED:</p> <div> <input type='radio' id='RADIO_MODE1' name='operation_mode' value='Mode 1'> <label for='RADIO_MODE1'>LED 1</label> <input type='radio' id='RADIO_MODE2' name='operation_mode' value='Mode 2'> <label for='RADIO_MODE2'>LED 2</label> <input type='radio' id='RADIO_MODE3' name='operation_mode' value='Mode 3'> <label for='RADIO_MODE3'>LED 3</label> </div> <div></form><br>Set intensity level: <br><input type='range' min='1' max='100' value='50' class='slider' id='LED_intensity'>Value: <span id='value'>-</span><br></span></body><script> var slider = document.getElementById('LED_intensity'); var output = document.getElementById('value'); document.getElementById('RADIO_MODE1').addEventListener('click', cb_operation_mode); document.getElementById('RADIO_MODE2').addEventListener('click', cb_operation_mode); document.getElementById('RADIO_MODE3').addEventListener('click', cb_operation_mode); slider.addEventListener('click', slider_change); var Socket; function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processCommand(event); }; } function button_send_back() {console.log(slider.value);var x = slider.value; var msg = {type: 'LED_intensity',value: x};Socket.send(JSON.stringify(msg)); } function slider_change() { console.log(slider.value);var x = slider.value;var msg = { type: 'LED_intensity', value: x};Socket.send(JSON.stringify(msg)); } function cb_operation_mode() {var LED_select = 0;if(document.getElementById('RADIO_MODE1').checked == true) { console.log('oen'); } else if(document.getElementById('RADIO_MODE2').checked == true) { LED_select = 1; } else { LED_select = 2;}var msg = { type: 'LED_select', value: LED_select};Socket.send(JSON.stringify(msg)); } function processCommand(event) { var obj = JSON.parse(event.data);var type = obj.type;if (type.localeCompare(\"LED_intensity\") == 0) { var slider_value = obj.value; console.log(\"successful\"); console.log(slider_value); output.innerHTML = slider_value; slider.value = parseInt(slider_value);}if (type.localeCompare(\"LED_select\") == 0) { var LED_select = parseInt(obj.value); console.log(\"LED_select\"); console.log(LED_select); if(LED_select == 0) {console.log(\"mode1\");document.getElementById('RADIO_MODE1').checked = true; } else if(LED_select == 1) { console.log(\"mode2\");document.getElementById('RADIO_MODE2').checked = true; } else if(LED_select == 2) { console.log(\"mode3\");document.getElementById('RADIO_MODE3').checked = true; }} } window.onload = function(event) { init(); }</script></html>";

int LED_select = 0;                                   // LED selected
int LED_intensity = 50;                               // LED intensity

// some standard stuff needed to do "analogwrite" on the ESP32 -> an ESP32 does not have "analogwrite" and uses ledcWrite instead
const int freq = 5000;
const int led_channels[] = {0, 1, 2};
const int resolution = 8;

// init PINs: assign any pin on ESP32
int led_pin_1 = 4;
int led_pin_2 = 0;
int led_pin_3 = 2;     // On some ESP32 pin 2 is an internal LED, mine did not have one

// The JSON library uses static memory, so this will need to be allocated:
StaticJsonDocument<200> doc_tx;                       // provision memory for about 200 characters
StaticJsonDocument<200> doc_rx;

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
  ledcAttachPin(led_pin_1, led_channels[0]);
  ledcAttachPin(led_pin_2, led_channels[1]);
  ledcAttachPin(led_pin_3, led_channels[2]);
  
  WiFi.begin(ssid, password);                         // start WiFi interface
  Serial.println("Establishing connection to WiFi with SSID: " + String(ssid));     // print SSID to the serial interface for debugging
 
  while (WiFi.status() != WL_CONNECTED) {             // wait until WiFi is connected
    delay(1000);
    Serial.print(".");
  }
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());                     // show IP address that the ESP32 has received from router
  
  server.on("/", []() {                               // define here wat the webserver needs to do
    server.send(200, "text\html", webpage);           //    -> it needs to send out the HTML string "webpage" to the client
  });
  server.begin();                                     // start server
  
  webSocket.begin();                                  // start websocket
  webSocket.onEvent(webSocketEvent);                  // define a callback function -> what does the ESP32 need to do when an event from the websocket is received? -> run function "webSocketEvent()"

  ledcWrite(led_channels[LED_select], map(LED_intensity, 0, 100, 0, 255));    // write initial state to LED selected
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
      sendJson("LED_select", String(LED_select));
  
      break;
    case WStype_TEXT:                                 // if a client has sent data, then type == WStype_TEXT
      // try to decipher the JSON string received
      DeserializationError error = deserializeJson(doc_rx, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      else {
        // JSON string was received correctly, so information can be retrieved:
        const char* l_type = doc_rx["type"];
        const int l_value = doc_rx["value"];
        Serial.println(l_type);

        // if LED_intensity value is received -> update and write to LED
        if(String(l_type) == "LED_intensity") {
          LED_intensity = int(l_value);
          Serial.println("Value = " + String(l_value));
          sendJson("LED_intensity", String(l_value));
          ledcWrite(led_channels[LED_select], map(LED_intensity, 0, 100, 0, 255));
        }
        // else if LED_select is changed -> switch on LED and switch off the rest
        if(String(l_type) == "LED_select") {
          LED_select = int(l_value);
          Serial.println("Value = " + String(l_value));
          sendJson("LED_select", String(l_value));
          for (int i = 0; i < 3; i++) {
            if (i == LED_select)
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

void sendJson(String l_type, String l_value) {
    String jsonString = "";                           // create a JSON string for sending data to the client
    JsonObject object = doc_tx.to<JsonObject>();      // create a JSON Object
    object["type"] = l_type;                          // write data into the JSON object -> I used "rand1" and "rand2" here, but you can use anything else
    object["value"] = l_value;  
    serializeJson(doc_tx, jsonString);                // convert JSON object to string
    webSocket.broadcastTXT(jsonString);               // send JSON string to clients  
}
