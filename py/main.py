import paho.mqtt.client as mqtt
import random
import time
import json
from datetime import datetime

# Configurações do MQTT para Docker Compose
MQTT_BROKER = "34.235.228.232"  # Ou o IP do seu host
MQTT_PORT = 1883
KEEP_ALIVE = 60

# Tópicos para os 2 equipamentos
TOPIC_EQUIPAMENTO_02 = "sensor/equipamento_02"
TOPIC_EQUIPAMENTO_03 = "sensor/equipamento_03"


# Função para gerar temperatura mock (entre 20°C e 30°C)
def gerar_temperatura():
    return round(random.uniform(20.0, 30.0), 2)


# Função para gerar pH mock (entre 6.0 e 8.0)
def gerar_ph():
    return round(random.uniform(6.0, 8.0), 2)


# Callback quando conectado ao broker
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Conectado ao broker MQTT em {MQTT_BROKER}:{MQTT_PORT}")
        print("✅ Conexão anônima permitida (allow_anonymous true)")
    else:
        print(f"❌ Falha na conexão. Código: {rc}")
        codigos_erro = {
            1: "Connection refused - incorrect protocol version",
            2: "Connection refused - invalid client identifier",
            3: "Connection refused - server unavailable",
            4: "Connection refused - bad username or password",
            5: "Connection refused - not authorised"
        }
        if rc in codigos_erro:
            print(f"Detalhes: {codigos_erro[rc]}")


# Callback quando publica com sucesso
def on_publish(client, userdata, mid):
    print(f"📤 Mensagem publicada com ID: {mid}")


# Callback para desconexão
def on_disconnect(client, userdata, rc):
    if rc != 0:
        print("⚠️  Conexão perdida. Tentando reconectar...")


# Callback para subscribe (opcional)
def on_subscribe(client, userdata, mid, granted_qos):
    print(f"✅ Inscrito nos tópicos com QoS: {granted_qos}")


# Configurar cliente MQTT
client = mqtt.Client()
client.on_connect = on_connect
client.on_publish = on_publish
client.on_disconnect = on_disconnect
client.on_subscribe = on_subscribe

# Configurações para melhor estabilidade
client.reconnect_delay_set(min_delay=1, max_delay=120)


def testar_conexao():
    """Testa se a porta MQTT está acessível"""
    import socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        result = sock.connect_ex((MQTT_BROKER, MQTT_PORT))
        sock.close()
        if result == 0:
            print(f"✅ Porta {MQTT_PORT} em {MQTT_BROKER} está acessível")
            return True
        else:
            print(f"❌ Porta {MQTT_PORT} em {MQTT_BROKER} não está acessível")
            return False
    except Exception as e:
        print(f"❌ Erro no teste de conexão: {e}")
        return False


try:
    print("🚀 Iniciando publisher MQTT...")
    print(f"📡 Tentando conectar em {MQTT_BROKER}:{MQTT_PORT}")

    # Testar conexão primeiro
    if not testar_conexao():
        print("💡 Dica: Verifique se o container Mosquitto está rodando:")
        print("   docker-compose ps mosquitto")
        print("   docker-compose logs mosquitto")
        exit(1)

    # Conectar ao broker
    client.connect(MQTT_BROKER, MQTT_PORT, KEEP_ALIVE)
    client.loop_start()

    # Aguardar conexão ser estabelecida
    time.sleep(2)

    print("🎯 Iniciando publicação de dados mock para 2 equipamentos...")
    print("⏹️  Pressione Ctrl+C para parar")
    print("-" * 50)

    # Loop principal de publicação
    contador = 0
    while True:
        contador += 1
        timestamp = datetime.now().isoformat()

        # Gerar dados para equipamento 02
        dados_equipamento_02 = {
            "equipamento": "equipamento_02",
            "timestamp": timestamp,
            "ph": gerar_ph(),
            "temperatura": gerar_temperatura()
        }

        # Gerar dados para equipamento 03
        dados_equipamento_03 = {
            "equipamento": "equipamento_03",
            "timestamp": timestamp,
            "ph": gerar_ph(),
            "temperatura": gerar_temperatura()
        }

        # Publicar dados do equipamento 02
        payload_02 = json.dumps(dados_equipamento_02)
        result_02 = client.publish(TOPIC_EQUIPAMENTO_02, payload_02)

        if result_02.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"📤 Equipamento 02: pH {dados_equipamento_02['ph']}, Temp {dados_equipamento_02['temperatura']}°C")
        else:
            print(f"❌ Erro ao publicar para equipamento 02")

        # Publicar dados do equipamento 03
        payload_03 = json.dumps(dados_equipamento_03)
        result_03 = client.publish(TOPIC_EQUIPAMENTO_03, payload_03)

        if result_03.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"📤 Equipamento 03: pH {dados_equipamento_03['ph']}, Temp {dados_equipamento_03['temperatura']}°C")
        else:
            print(f"❌ Erro ao publicar para equipamento 03")

        print(f"🔄 Ciclo {contador} concluído. Aguardando próximo ciclo...")
        print("-" * 50)

        # Aguardar 5 segundos antes do próximo ciclo
        time.sleep(5)

except KeyboardInterrupt:
    print("\n🛑 Publicação interrompida pelo usuário")
except Exception as e:
    print(f"❌ Erro inesperado: {e}")
finally:
    client.loop_stop()
    client.disconnect()
    print("🔌 Conexão MQTT finalizada")