# Projeto IoT - Monitoramento de Qualidade da Água

## Descrição do Projeto
Sistema de monitoramento de qualidade da água utilizando tecnologia IoT para acompanhamento em tempo real de parâmetros como temperatura e pH em reservatórios. O projeto utiliza Node-RED para processamento de dados, InfluxDB para armazenamento, Grafana para visualização e Mosquitto como broker MQTT.

## Arquitetura do Sistema
- **Node-RED**: Processamento de fluxo de dados
- **InfluxDB**: Banco de dados temporal
- **Grafana**: Dashboard e visualização
- **Mosquitto**: Broker MQTT para comunicação IoT
- **OpenWeather API**: Dados meteorológicos externos
- **Telegram**: Sistema de alertas

## Pré-requisitos
- AWS EC2 instance
- Docker e Docker Compose
- Portas habilitadas: 80, 443, 1883, 3000, 8086

## Configuração do Ambiente

### 1. Arquivo Docker Compose
```yaml
version: '3.8'

services:
  node-red:
    image: nodered/node-red:latest
    container_name: node-red
    ports:
      - "1880:1880"
    volumes:
      - node_red_data:/data
    networks:
      - iot-network
    restart: unless-stopped

  grafana:
    image: grafana/grafana:latest
    container_name: grafana
    ports:
      - "3000:3000"
    volumes:
      - grafana_data:/var/lib/grafana
    networks:
      - iot-network
    restart: unless-stopped

  mosquitto:
    image: eclipse-mosquitto:latest
    container_name: mosquitto
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - mosquitto_data:/mosquitto/data
      - mosquitto_log:/mosquitto/log
      - ./mosquitto.conf:/mosquitto/config/mosquitto.conf  # Adicione esta linha
    networks:
      - iot-network
    restart: unless-stopped

volumes:
  node_red_data:
  grafana_data:
  mosquitto_data:
  mosquitto_log:

networks:
  iot-network:
    driver: bridge
```

### 2. Configuração do Mosquitto
Crie o arquivo `mosquitto/config/mosquitto.conf`:
```
listener 1883
allow_anonymous true

log_dest stdout
log_type all

persistence true
persistence_location /mosquitto/data

connection_messages true
```

### 3. Implantação
```bash
# Criar diretórios
mkdir -p mosquitto/{config,data,log} influxdb2 grafana/{data,provisioning} node-red/data

# Iniciar serviços
docker-compose up -d
```

## Configuração do Node-RED

### 1. Acessar Node-RED
- URL: `http://localhost:1880`

### 2. Instalar Paletas Necessárias
No menu Node-RED → Manage Palette → Install:
- node-red-dashboard
- node-red-node-influxdb
- node-red-contrib-telegrambot

### 3. Configuração dos Nós

#### MQTT In (Equipamentos)
- Servidor: `mosquitto:1883`
- Tópicos: `sensor/equipamento_01`
- QoS: 2

#### InfluxDB Out
- Servidor: `[v2.0] Student`
- URL: `http://influxdb:8086`
- Token: `student_token`
- Organization: `Student`
- Bucket: `bucket`
- Measurement: `equipamentos` e `meteorologia`

#### Function - OpenWeather
```javascript
// Configurações da API
const cidade = "São Paulo";
const pais = "BR";
const token = "TOKEN_OPENWEATHER";
const unidades = "metric";

// Construir a URL
const url = `https://api.openweathermap.org/data/2.5/weather?q=${cidade},${pais}&appid=${token}&units=${unidades}`;

// Atribuir a URL ao msg.payload
msg.url = url;
msg.method = "GET";
msg.headers = [];

return msg;
```

#### Function - Montagem de Payload
```javascript

if (msg.payload && msg.payload.main) {
    const weatherData = msg.payload;
    
    msg.payload = {
        measurement: "weather_monitoring",
        location: weatherData.name,
        country: weatherData.sys.country,
        source: "openweather",
        temperature: weatherData.main.temp,
        location_name: weatherData.name,
        timestamp: new Date()
    };
    
    return msg;
}

return null;
```

#### Function - Alerta Telegram
```javascript
// Configurações
const PH_MIN = 6.0;
const PH_MAX = 9.5;
let alertaAtivo = false;
let normalizadorAtivo = false;

// Valor do ph e Nome do Equipamento vindo do payload
const ph = parseFloat(msg.payload.ph);
const equipamento = msg.payload.sensor_id;
const temp = msg.payload.temp;

if (isNaN(ph)) {
    node.error("Valor de ph invalido");
    return null;
}

msg.payload.chatId = '@chatId'
msg.payload.content = ''
msg.payload.type = 'message'

// Verifica limites - ph DESREGULADO
if (ph < PH_MIN || ph > PH_MAX) {
    if (!alertaAtivo) {
        // Cria mensagem de alerta
        let tipo = ph < PH_MIN ? "Ácido" : "Alcalino";
        let limite = ph < PH_MIN ? PH_MIN : PH_MAX;
        msg.payload.content = `ALERTA - ph DESREGULADO!\nNome: ${equipamento.toUpperCase()}\nValor atual: ${ph.toFixed(2)}\nLimite ideal: ${limite}\nTemperatura do Recipiente: ${temp}\nTipo: ${tipo}\nNormalizador sendo inserido na água`;
        alertaAtivo = true; 
        normalizadorAtivo = true; 
        return msg;
    }
} else {
    // ph REGULADO (dentro dos limites normais)
    if (alertaAtivo || normalizadorAtivo) {
        // Código para normalização
    }
}
```

## Configuração do Grafana

### 1. Acessar Grafana
- URL: `http://localhost:3000`
- Login: `admin`
- Senha: `admin123`

### 2. Configurar Data Source
- **Name**: InfluxDB
- **Type**: InfluxDB
- **URL**: `http://influxdb:8086`
- **Access**: Server (Default)
- **Auth**: Basic auth
- **User**: `student`
- **Password**: `student123`
- **Database**: `bucket`
- **HTTP Method**: GET

### 3. Dashboards

#### Dashboard de Monitoramento de Qualidade da Água

**Visualizações Incluídas:**

1. **Gauges - pH em Tempo Real**
   - Tipo: Gauge
   - Query: 
   ```sql
   SELECT last("ph") FROM "equipamentos" 
   WHERE time >= now() - 5m 
   GROUP BY "equipamento"
   ```

2. **BarChart - Temperatura dos Equipamentos**
   - Tipo: Bar Chart
   - Query:
   ```sql
   SELECT last("temperature") FROM "equipamentos" 
   WHERE time >= now() - 5m 
   GROUP BY "equipamento"
   ```

3. **Série Temporal do pH**
   - Tipo: Time Series
   - Query:
   ```sql
   SELECT "ph" FROM "equipamentos" 
   WHERE time >= now() - 1h 
   GROUP BY "equipamento"
   ```

4. **Temperatura Meteorológica**
   - Tipo: Stat
   - Query:
   ```sql
   SELECT last("temperature") FROM "meteorologia" 
   WHERE time >= now() - 1h
   ```

## Configuração de Segurança EC2

### Grupos de Segurança
```bash
# Portas necessárias
Port 22 (SSH) - Sua IP
Port 80 (HTTP) - 0.0.0.0/0
Port 443 (HTTPS) - 0.0.0.0/0
Port 1883 (MQTT) - 0.0.0.0/0
Port 3000 (Grafana) - 0.0.0.0/0
Port 8086 (InfluxDB) - 0.0.0.0/0
```
## Solução de Problemas

### Problemas Comuns:
1. **Node-RED não conecta ao MQTT**: Verifique se o serviço Mosquitto está rodando
2. **Dados não aparecem no Grafana**: Confirme a configuração do data source InfluxDB
3. **Alertas não funcionam**: Verifique o token do bot do Telegram e o chatId

Este ambiente fornece uma base completa para monitoramento de qualidade da água com alertas em tempo real e visualização de dados históricos.
