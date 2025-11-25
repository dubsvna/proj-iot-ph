#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>

// ================= CONFIGURAÇÕES DE PINOS (IMPORTANTE) =================
// Mudamos para pinos digitais para não bloquear o USB do computador
// Ligue o TX do ESP no Pino 6 do Arduino
// Ligue o RX do ESP no Pino 7 do Arduino
SoftwareSerial SerialEsp(6, 7); // RX, TX

// ================= CONFIGURAÇÕES DE REDE =================
char ssid[] = "nothing_phone_1";            
char pass[] = "cmiyglost";           

// Endereço IP do seu computador (onde roda o Python/Mosquitto)
// Use vírgulas, não pontos, para o IPAddress
// 3.234.230.104
IPAddress server(3, 234, 230, 104); // <--- ALTERE AQUI PARA SEU IP
const int port = 1883;

// ================= CONFIGURAÇÕES DO SENSOR PH =================
#define PH_PIN A0
const float V_MIN = 0.0;
const float V_MAX = 5.0;
const float PH_MIN = 0.0;
const float PH_MAX = 14.0;
const int NUM_SAMPLES = 10;
int buf[NUM_SAMPLES];

// ================= OBJETOS MQTT E WIFI =================
WiFiEspClient espClient;
PubSubClient client(espClient);
long lastSend = 0;
int status = WL_IDLE_STATUS;

void setup() {
  // Serial do Computador (Debug)
  Serial.begin(9600);
  
  // Serial do ESP-01
  // Tente 9600 primeiro. Se der timeout, mude para 115200 aqui.
  SerialEsp.begin(9600); 

  Serial.println("Inicializando WiFiEsp...");
  
  // Inicializa o módulo WiFi usando a porta SoftwareSerial
  WiFi.init(&SerialEsp);

  // Verifica se o hardware respondeu
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("ERRO: Modulo WiFi nao detectado via SoftwareSerial.");
    Serial.println("Verifique: 1. Conexoes TX/RX cruzadas? 2. CH_PD no 3.3V? 3. Baud rate correto?");
    // Não trava o loop, mas avisa
  }

  // Tenta conectar ao WiFi
  while (status != WL_CONNECTED) {
    Serial.print("Tentando conectar na rede: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(2000); // Aguarda conexão
  }

  Serial.println("WiFi Conectado!");
  Serial.print("IP do Arduino: ");
  Serial.println(WiFi.localIP());

  // Configura o servidor MQTT
  client.setServer(server, port);
}

// ================= FUNÇÕES DO SENSOR =================
float lerTensaoPH() {
  for (int i = 0; i < NUM_SAMPLES; i++) {
    buf[i] = analogRead(PH_PIN);
    delay(10);
  }
  // Ordenar para pegar mediana
  for(int i=0; i<NUM_SAMPLES-1; i++) {
    for(int j=i+1; j<NUM_SAMPLES; j++) {
      if(buf[i] > buf[j]) {
        int temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  // Média descartando extremos
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

// ================= RECONEXÃO MQTT =================
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    // Tenta conectar com ID único
    if (client.connect("ArduinoPH_Client")) {
      Serial.println("Conectado!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

void loop() {
  // Mantém a conexão MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Envia a cada 5 segundos
  long now = millis();
  if (now - lastSend > 5000) {
    lastSend = now;

    // Leitura dos dados
    float tensao = lerTensaoPH();
    float phValue = calcularPH(tensao);
    int tempMock = 123; 

    // Debug no Monitor Serial
    Serial.print("Lendo pH: ");
    Serial.print(phValue);
    Serial.println(" | Temp: 123");

    // Prepara JSON Temperatura
    String jsonTemp = "{\"sensor_id\":\"arduino_temp\",\"temperatura\":";
    jsonTemp += tempMock;
    jsonTemp += "}";

    // Prepara JSON pH
    String jsonPH = "{\"sensor_id\":\"arduino_ph\",\"ph\":";
    jsonPH += String(phValue, 2);
    jsonPH += "}";

    // Envia MQTT
    char msgBuffer[100];
    
    jsonTemp.toCharArray(msgBuffer, 100);
    client.publish("sensor/temperatura", msgBuffer);

    jsonPH.toCharArray(msgBuffer, 100);
    client.publish("sensor/ph", msgBuffer);
    
    Serial.println("Dados enviados!");
  }
}