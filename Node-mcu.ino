#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Wi-Fi and MQTT credentials
const char* ssid = "Home";
const char* password = "Faraz1024@";
const char* mqttServer = "94.101.189.161";
const int mqttPort = 1883;
const char* mqttUser = "admin";
const char* mqttPassword = "admin";
const char* clientName = "NodeMCUClient";
const char* mqttTopic = "zones-topic";

#define SOUND_VELOCITY 0.034
long duration;
float distanceCm;
bool enteringProcess = false;
bool exitingProcess = false;
int zoneID = 2;

const int trigPin1 = D1; // Replace with your sensor's trigger pin
const int echoPin1 = D5; // Replace with your sensor's echo pin
const int trigPin2 = D2; // Replace with your sensor's trigger pin
const int echoPin2 = D6; // Replace with your sensor's echo pin
const int trigPin3 = D3; // Replace with your sensor's trigger pin
const int echoPin3 = D7; // Replace with your sensor's echo pin
const int trigPin4 = D4; // Replace with your sensor's trigger pin
const int echoPin4 = D8; // Replace with your sensor's echo pin
const int distanceThreshold = 5; // Set your distance threshold in centimeters

enum CarState {
  IDLE,
  SENSORS_1_2_SENSE,
  SENSORS_1_2_3_4_SENSE,
  SENSORS_3_4_SENSE,
  CAR_ENTERING,
  CAR_EXITING
};

CarState currentState = IDLE;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);
  pinMode(trigPin3, OUTPUT);
  pinMode(echoPin3, INPUT);
  pinMode(trigPin4, OUTPUT);
  pinMode(echoPin4, INPUT);

  // Connect to Wi-Fi
  Serial.print("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Connect to MQTT broker
  Serial.println("\nConnecting to RabbitMQ...");
  client.setServer(mqttServer, mqttPort);
  while (!client.connected()) {
    if (client.connect(clientName, mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.println("\nFailed to connect to MQTT broker");
      delay(1000);
    }
  }
}

void loop() {

  delay(1500);
  Serial.println("------------");
  Serial.println(currentState);
  Serial.println("------------");

  bool s1And2 = sensors1And2Sense();
  bool s3And4 = sensors3And4Sense();


  switch (currentState) {
    case IDLE:
      enteringProcess = false;
      exitingProcess = false;
      if (s1And2 && !s3And4) {
        currentState = SENSORS_1_2_SENSE;
        enteringProcess = true;
        break;
      }
      if (s3And4 && !s1And2) {
        currentState = SENSORS_3_4_SENSE;
        exitingProcess = true;
        break;
      }
      break;

    case SENSORS_1_2_SENSE:
      if (!s1And2 && exitingProcess) {
        currentState = CAR_EXITING;
        break;
      } 
      if (!s1And2) {
        currentState = IDLE;
        break;
      }
      if (s1And2 && s3And4) {
        currentState = SENSORS_1_2_3_4_SENSE;
        break;
      }
      break;

    case SENSORS_1_2_3_4_SENSE:
      if (!s1And2 && !s3And4) {
        currentState = IDLE;
        break;
      } 
      if (!s1And2 && s3And4) {
        currentState = SENSORS_3_4_SENSE;
        break;
      }
      if (s1And2 && !s3And4) {
        currentState = SENSORS_1_2_SENSE;
        break;
      }
      break;

    case SENSORS_3_4_SENSE:
      if (!s3And4 && enteringProcess) {
        currentState = CAR_ENTERING;
        break;
      } 
      if (!s3And4){
        currentState = IDLE;
        break;
      }
      if (s3And4 && s1And2){
        currentState = SENSORS_1_2_3_4_SENSE;
        break;
      }
      break;
      
    case CAR_ENTERING:
      publishToMQTT("enter");
      Serial.println("enetering");
      currentState = IDLE;
      break;

    case CAR_EXITING:
      publishToMQTT("exit");
      Serial.println("exiting");
      currentState = IDLE;
      break;
  }

}

bool sensors1And2Sense() {
  // Check if both sensors 1 and 2 sense an object (distance below 5 cm)
  bool sensor1Sensed = isDistanceBelowThreshold(trigPin1, echoPin1);
  bool sensor2Sensed = isDistanceBelowThreshold(trigPin2, echoPin2);
  
  return (sensor1Sensed && sensor2Sensed);
}

bool sensors3And4Sense() {
  // Check if both sensors 3 and 4 sense an object (distance below 50 cm)
  bool sensor3Sensed = isDistanceBelowThreshold(trigPin3, echoPin3);
  bool sensor4Sensed = isDistanceBelowThreshold(trigPin4, echoPin4);
  
  return (sensor3Sensed && sensor4Sensed);
}

bool isDistanceBelowThreshold(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(15);
  digitalWrite(trigPin, LOW);
  
    delayMicroseconds(100); // Adjust the delay time as needed

  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = duration * SOUND_VELOCITY/2;

  Serial.print("Sensor ");
  if (trigPin == D1) {
    Serial.print("1");
  }
  if (trigPin == D2) {
    Serial.print("2");
  }
  if (trigPin == D3) {
    Serial.print("3");
  }
  if (trigPin == D4) {
    Serial.print("4");
  }
  Serial.print(":");
  Serial.println(distanceCm);

  // Check if the distance is below the threshold
  return (distanceCm < distanceThreshold && distanceCm != 0.0);
}

void publishToMQTT(const char* messageType) {
  // Check MQTT connection
  if (!client.connected()) {
    // Reconnect to MQTT broker
    while (!client.connected()) {
      Serial.print("Attempting MQTT reconnection...");
      if (client.connect(clientName, mqttUser, mqttPassword)) {
        Serial.println("Connected to MQTT broker");
      } else {
        Serial.print("Failed to connect to MQTT broker. Retry in 5 seconds...");
        delay(5000);
      }
    }
  }

  // Create a JSON object
  StaticJsonDocument<200> jsonDocument;
  jsonDocument["type"] = messageType;
  jsonDocument["zone_id"] = zoneID;

  // Serialize the JSON object to a string
  String jsonString;
  serializeJson(jsonDocument, jsonString);

  // Publish the JSON message to the MQTT topic
  if (client.publish(mqttTopic, jsonString.c_str())) {
    Serial.println("Message sent to MQTT topic successfully");
  } else {
    Serial.println("Failed to send message to MQTT topic");
  }
}



