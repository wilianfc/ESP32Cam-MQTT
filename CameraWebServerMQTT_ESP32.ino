//Versão modificada do exemplo ESP32 CameraWebServer com a inserção 
// de exemplos de integração com o protocolo MQTT
//Prof. Wilian França Costa
//wilian.costa@mackenzie.br
//Para maiores detalhes consulte:
// https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/
// https://randomnerdtutorials.com/esp32-cam-video-streaming-web-server-camera-home-assistant/
// https://www.youtube.com/watch?v=JYchUapoqzc
//

#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>

//#include <ESP32Servo.h>  //Se for utilizar servos, esta é o bibliteca para o ESP32
//Servo myservo; 

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

#define RED_LED  33  //RED indicator LED.
#define FLASH_LED 4  //FLASH LED

const char* ssid = "******";
const char* password = "******";
const char* mqtt_server = "broker.mqtt-dashboard.com"; //Utilize o broker de sua preferência


WiFiClient espClient;
PubSubClient client(espClient);

void startCameraServer();
void setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(RED_LED, LOW);  //sinaliza que está conectado ao wifi
}


void callback(char* topic, byte* payload, unsigned int length) {
  String inString="";
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  for (int i = 0; i < length; i++) {
    //Serial.print((char)payload[i]);
    inString+=(char)payload[i];
  }
  if(String((char*) topic) == "prof_wilian_oic/hello/led"){
    Serial.print(inString);

    if(inString=="on") digitalWrite(FLASH_LED, HIGH);
    else digitalWrite(FLASH_LED, LOW);
    
  }
  Serial.println();

   //pos=inString.toInt();
   //myservo.write(pos);

//  // Switch on the LED if an 1 was received as first character
//  //if (pos > 100) {
//  if ((char)payload[0] == '1') {
//    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
//    // but actually the LED is on; this is because
//    // it is acive low on the ESP-01)
//  } else {
//    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
//  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32CamClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("prof_wilian_oic/hello", "ESP32 Conectado");
      client.publish("prof_wilian_oic/msg",   
         (String("Camera Ready! Use 'http://")+
         String(WiFi.localIP())+
         String(":81/stream' to view \n")).c_str());
      
      // ... and resubscribe
      client.subscribe("prof_wilian_oic/hello/led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(FLASH_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(FLASH_LED, LOW);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

//  WiFi.begin(ssid, password);
//
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    Serial.print(".");
//  }
//  Serial.println("");
//  Serial.println("WiFi connected");
//
//  digitalWrite(RED_LED, LOW);
  setup_wifi();
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");


  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  if(Serial.available()){
    if(Serial.read()=='1') digitalWrite(FLASH_LED, HIGH);
    else   digitalWrite(FLASH_LED, LOW);
  }
  delay(1000);
}
