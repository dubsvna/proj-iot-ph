#include <WiFi.h>              // WiFi nativo do ESP32
#include <PubSubClient.h>
#include <ESP32Servo.h>        // Biblioteca Servo específica para ESP32
#include <OneWire.h>
#include <DallasTemperature.h>

// ================= CONFIGURAÇÕES DE PINOS (ESP32) =================
// No ESP32 usamos os números dos GPIOs
#define PH_PIN 34      // Pino Analógico (ADC1). Use pinos como 32, 33, 34, 35, 36
#define SERVO_PIN 13   // Pino digital PWM
#define TEMP_PIN 4     // Pino digital para o DS18B20

// ================= OBJETOS DOS SENSORES =================
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);
Servo meuServo;

// ================= LIMITES E CONSTANTES =================
const float PH_LIMITE_BAIXO = 6.0;
const float PH_LIMITE_ALTO = 8.0;

// Ajuste para ESP32 (ADC é 12 bits: 0-4095, e voltagem é 3.3V)
// IMPORTANTE: Se usar divisor de tensão (ex: 2 resistores iguais), 
// a tensão lida será metade. Ajuste a lógica conforme seu circuito.
// Abaixo assumindo que você calibrou para entrar no máx 3.3V.
const float ADC_REF_V = 5.3; 
const int ADC_RESOLUTION = 4095;

// Calibração do pH (Isso varia MUITO de sensor para sensor)
const float PH_MAX_VOLTAGE = 2.5; // Exemplo: Tensão do sensor em pH 7 (ajustar offset)
// No PH4502C: PH 7 geralmente é ajustado para 2.5V no pino PO.

// ================= CONFIGURAÇÕES DE REDE =================
const char* ssid = "nothing_phone_1";            
const char* pass = "cmiyglost";           

// Servidor MQTT
// http://34.235.228.232/
IPAddress server(34, 235, 228, 232); 
const int port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

long lastSend = 0;
const int NUM_SAMPLES = 20; // Mais amostras pois o ADC do ESP32 oscila mais

void setup() {
  Serial.begin(115200); // ESP32 usa serial mais rápido por padrão

  // Inicializa Servo
  // ESP32Servo precisa de configuração de timers, mas a lib cuida disso no attach
  meuServo.setPeriodHertz(50); // Servo padrão 50Hz
  meuServo.attach(SERVO_PIN, 500, 2400); 
  meuServo.write(0); 
  
  // Inicializa Sensor Temperatura
  sensors.begin();

  // Conexão WiFi (Muito mais simples que no Arduino)
  setup_wifi();

  client.setServer(server, port);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando em ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    // ID do Cliente
    if (client.connect("ESP32_PH_Client")) {
      Serial.println("Conectado!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

// Leitura do pH com suavização (Média Móvel)
float lerPhVoltage() {
  unsigned long soma = 0;
  // O ADC do ESP32 não é perfeitamente linear e tem ruído
  // Fazemos várias leituras
  for (int i = 0; i < NUM_SAMPLES; i++) {
    soma += analogRead(PH_PIN);
    delay(5); // Pequeno delay entre leituras
  }
  
  float mediaADC = soma / (float)NUM_SAMPLES;
  
  // Converte para Tensão
  // ATENÇÃO: Se usar divisor de tensão, multiplique pelo fator aqui
  float tensao = (mediaADC * ADC_REF_V) / ADC_RESOLUTION;
  
  return tensao;
}

float calcularPH(float tensao) {
  // A fórmula do módulo PH4502C geralmente é linear
  // pH = 7 + ((2.5 - tensao) / m)
  // Onde 'm' é a inclinação (slope). Sem calibração, usamos uma aproximação.
  // Você precisará calibrar isso com soluções buffer 4.0 e 7.0
  
  // Exemplo genérico (Assumindo que em pH 7 a tensão é ~2.5V e a sensibilidade é alta)
  // Ajuste os valores '3.5' abaixo até bater com seu medidor de referência
  float ph = 7.0 + ((2.5 - tensao) * 3.5); 
  
  return ph;
}

float lerTemperatura() {
  sensors.requestTemperatures(); 
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C) return -999.0;
  return tempC;
}

void controllingServo(float ph) {
    if (ph < PH_LIMITE_BAIXO || ph > PH_LIMITE_ALTO) {
        meuServo.write(90); 
    } else {
        meuServo.write(0);  
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

    // 1. Leituras
    float tensao = lerPhVoltage();
    float phValue = calcularPH(tensao);
    float tempReal = lerTemperatura();

    // 2. Controle
    controllingServo(phValue);

    // 3. Debug
    Serial.print("ADC Volts: ");
    Serial.print(tensao);
    Serial.print("V | pH Calc: ");
    Serial.print(phValue);
    Serial.print(" | Temp: ");
    Serial.println(tempReal);

    // 4. Envio MQTT (O ESP32 aguenta enviar rápido, não precisa daquele delay gigante)
    char msgBuffer[100];

    // Temp
    String jsonTemp = "{\"sensor_id\":\"esp32_temp\",\"temperatura\":";
    jsonTemp += String(tempReal, 2);
    jsonTemp += "}";
    jsonTemp.toCharArray(msgBuffer, 100);
    client.publish("sensor/temperatura", msgBuffer);

    // pH
    String jsonPH = "{\"sensor_id\":\"esp32_ph\",\"ph\":";
    jsonPH += String(phValue, 2);
    jsonPH += "}";
    jsonPH.toCharArray(msgBuffer, 100);
    client.publish("sensor/ph", msgBuffer);
    
    Serial.println("Dados enviados!");
  }
}