#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <Servo.h> // <--- 1. Incluimos a biblioteca do Servo

// ================= CONFIGURAÇÕES DE PINOS =================
// TX do ESP -> Pino 6 do Arduino
// RX do ESP -> Pino 7 do Arduino
SoftwareSerial SerialEsp(6, 7); 

#define PH_PIN A0
#define SERVO_PIN 9 // <--- 2. Pino onde o Servo está ligado (Fio Laranja/Amarelo)

// ================= LIMITES DE pH PARA ATIVAR O SERVO =================
// Se o pH for MENOR que 6.0 ou MAIOR que 8.0, o servo move
const float PH_LIMITE_BAIXO = 6.0;
const float PH_LIMITE_ALTO = 8.0;

// ================= CONFIGURAÇÕES DE REDE =================
char ssid[] = "nothing_phone_1";            
char pass[] = "cmiyglost";           

// Seu servidor MQTT (IP Público)
// 34.235.228.232
IPAddress server(34, 235, 228, 232); 
const int port = 1883;

// ================= VARIÁVEIS DO SENSOR =================
const float V_MIN = 0.0;
const float V_MAX = 5.0;
const float PH_MIN = 0.0;
const float PH_MAX = 14.0;
const int NUM_SAMPLES = 10;
int buf[NUM_SAMPLES];

// ================= OBJETOS =================
WiFiEspClient espClient;
PubSubClient client(espClient);
Servo meuServo; // <--- 3. Objeto do Servo
long lastSend = 0;
int status = WL_IDLE_STATUS;

void setup() {
  Serial.begin(9600);     // Debug PC
  SerialEsp.begin(9600);  // ESP-01

  // Inicializa o Servo
  meuServo.attach(SERVO_PIN);

  Serial.println("Inicializando WiFiEsp...");
  WiFi.init(&SerialEsp);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("ERRO: Modulo WiFi nao detectado.");
    while (true);
  }

  while (status != WL_CONNECTED) {
    Serial.print("Tentando conectar na rede: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    if(status != WL_CONNECTED) {
       Serial.println("Falha... tentando em 5s");
       delay(5000); 
    }
  }

  Serial.println("WiFi Conectado!");
  client.setServer(server, port);
}

// ================= FUNÇÕES DO SENSOR =================
float lerTensaoPH() {
  for (int i = 0; i < NUM_SAMPLES; i++) {
    buf[i] = analogRead(PH_PIN);
    delay(10);
  }
  // Ordenação (Bubble sort simples)
  for(int i=0; i<NUM_SAMPLES-1; i++) {
    for(int j=i+1; j<NUM_SAMPLES; j++) {
      if(buf[i] > buf[j]) {
        int temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  // Média central
  unsigned long valorMedio = 0;
  for (int i = 2; i < 8; i++) {
    valorMedio += buf[i];
  }
  float tensao = (valorMedio * 5.0) / 1024.0 / 6;
  return tensao;
}

float calcularPH(float tensao) {
  return PH_MAX - ((tensao - V_MIN) * (PH_MAX - PH_MIN)) / (V_MAX - V_MIN);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    if (client.connect("ArduinoPH_Client")) {
      Serial.println("Conectado!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

// ================= LÓGICA DO SERVO =================
void controlarServo(float ph) {
  // Se o pH estiver fora da faixa segura (Muito ácido ou Muito básico)
  if (ph < PH_LIMITE_BAIXO || ph > PH_LIMITE_ALTO) {
    meuServo.write(90); // Move para 90 graus (Ação/Alerta)
    // Serial.println("ALERTA: pH fora do normal! Servo ativado (90 graus).");
  } else {
    meuServo.write(0);  // Volta para 0 graus (Normal)
    // Serial.println("Normal: pH estavel. Servo em repouso (0 graus).");
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastSend > 5000) {
    lastSend = now;

    // 1. Ler Sensor
    float tensao = lerTensaoPH();
    float phValue = calcularPH(tensao);
    int tempMock = 123; 

    // 2. Controlar Servo baseado no pH
    controlarServo(phValue);

    // 3. Debug
    Serial.print("pH: ");
    Serial.print(phValue);
    Serial.print(" | Servo: ");
    if(phValue < PH_LIMITE_BAIXO || phValue > PH_LIMITE_ALTO) Serial.println("ATIVO (90)");
    else Serial.println("INATIVO (0)");

    // 4. Enviar MQTT
    String jsonTemp = "{\"sensor_id\":\"arduino_temp\",\"temperatura\":123}";
    
    String jsonPH = "{\"sensor_id\":\"arduino_ph\",\"ph\":";
    jsonPH += String(phValue, 2);
    jsonPH += "}";

    char msgBuffer[100];
    
    jsonTemp.toCharArray(msgBuffer, 100);
    client.publish("sensor/temperatura", msgBuffer);

    jsonPH.toCharArray(msgBuffer, 100);
    client.publish("sensor/ph", msgBuffer);
    
    Serial.println("Dados enviados!");
  }
}