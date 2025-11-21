// Pino analógico onde o sensor de pH está conectado
#define PH_PIN A0

// Configuração do sensor PH4502C
const float V_MIN = 0.0;  // tensão mínima do sensor
const float V_MAX = 5.0;  // tensão máxima do sensor
const float PH_MIN = 0.0;
const float PH_MAX = 14.0;

// Número de amostras para média
const int NUM_SAMPLES = 10;

int buf[NUM_SAMPLES];

void setup() {
  Serial.begin(9600);
  Serial.println("Leitura de pH - PH4502C");
}

// Função para ler a tensão do sensor
float lerTensaoPH() {
  for (int i = 0; i < NUM_SAMPLES; i++) {
    buf[i] = analogRead(PH_PIN);
    delay(10);
  }

  // Média descartando extremos (usar 6 amostras do meio)
  int valorMedio = 0;
  for (int i = 2; i < 8; i++) {
    valorMedio += buf[i];
  }

  float tensao = (valorMedio * 5.0) / 1024.0 / 6;
  return tensao;
}

// Converte a tensão para valor de pH
float calcularPH(float tensao) {
  // Corrige inversão do sensor: tensão alta = pH baixo
  float ph = PH_MAX - ((tensao - V_MIN) * (PH_MAX - PH_MIN)) / (V_MAX - V_MIN);
  return ph;
}

void loop() {
  float tensao = lerTensaoPH();
  float ph = calcularPH(tensao);

  Serial.print("Tensão lida: ");
  Serial.print(tensao, 2);
  Serial.print(" V  |  pH: ");
  Serial.println(ph, 2);

  delay(1000);
}
