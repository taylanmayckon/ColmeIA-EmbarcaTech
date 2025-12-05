// lib/MqttClient.cpp
#include "MqttClient.h"
#include <string.h>
#include <stdio.h>

// Construtor
MqttClient::MqttClient() {
    client = NULL;
    connected = false;
    wifiConnected = false;
    msgQueue = NULL;
    
    // Configura infos do cliente
    memset(&clientInfo, 0, sizeof(clientInfo));
    clientInfo.client_id = "pico_apissense";
    clientInfo.client_user = "user"; 
    clientInfo.client_pass = "pass"; 
    clientInfo.keep_alive = 60;
}

bool MqttClient::begin() {
    // Inicializa fila
    msgQueue = xQueueCreate(MQTT_MSG_QUEUE_SIZE, sizeof(MqttMessage));
    if (msgQueue == NULL) {
        printf("[MQTT] Erro ao criar fila\n");
        return false;
    }

    // Inicializa Wi-Fi (CYW43)
    if (cyw43_arch_init()) {
        printf("[MQTT] Falha ao iniciar hardware Wi-Fi\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();

    return true;
}

// === Callbacks Estáticos (Ponte entre C e C++) ===
void MqttClient::mqttConnectionCb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MqttClient* self = (MqttClient*)arg; // Recupera a instância da classe
    
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("[MQTT] Conectado ao Broker!\n");
        self->connected = true;
    } else {
        printf("[MQTT] Erro na conexão: %d\n", status);
        self->connected = false;
    }
}

void MqttClient::mqttPubRequestCb(void *arg, err_t result) {
    if (result != ERR_OK) {
        printf("[MQTT] Falha ao publicar: %d\n", result);
    }
}

// === Lógica Principal ===
void MqttClient::connectWifi() {
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        wifiConnected = true;
        return;
    }
    
    printf("[WIFI] Conectando a %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("[WIFI] Falha na conexão. Tentando novamente...\n");
        wifiConnected = false;
    } else {
        printf("[WIFI] Conectado! IP obtido.\n");
        wifiConnected = true;
    }
}

void MqttClient::connectBroker() {
    if (client == NULL) {
        client = mqtt_client_new();
    }

    if (!connected && wifiConnected) {
        ip_addr_t brokerIp;
        if (!ip4addr_aton(MQTT_BROKER_IP, &brokerIp)) {
            printf("[MQTT] IP do broker invalido\n");
            return;
        }

        printf("[MQTT] Conectando ao Broker %s...\n", MQTT_BROKER_IP);
        // Passamos 'this' como último argumento para recuperá-lo no callback
        mqtt_client_connect(client, &brokerIp, BROKER_PORT, mqttConnectionCb, this, &clientInfo);
    }
}

bool MqttClient::publish(const char* topic, const char* payload) {
    MqttMessage msg;
    strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
    
    // Envia para a fila (thread-safe). Não bloqueia se a fila estiver cheia (0 delay)
    // para não travar a task de envio.
    if (xQueueSend(msgQueue, &msg, 0) == pdTRUE) {
        return true;
    } else {
        printf("[MQTT] Fila cheia! Mensagem descartada.\n");
        return false;
    }
}

void MqttClient::processQueue() {
    if (!connected) return;

    MqttMessage msg;
    // Verifica se há algo na fila
    while (xQueueReceive(msgQueue, &msg, 0) == pdTRUE) {
        err_t err = mqtt_publish(client, msg.topic, msg.payload, strlen(msg.payload), 0, 0, mqttPubRequestCb, this);
        if (err != ERR_OK) {
            printf("[MQTT] Erro ao enviar para lwIP: %d\n", err);
            // Opcional: Colocar de volta na fila se for crítico
        } else {
            printf("[MQTT] Publicado em %s: %s\n", msg.topic, msg.payload);
        }
    }
}

// Task para manter o MQTT Client
void MqttClient::taskImpl(void* _this) {
    MqttClient* self = (MqttClient*)_this; // Cast para a instância
    
    // Loop principal da Task
    while (true) {
        self->connectWifi();
        
        if (self->wifiConnected) {
            if (!self->connected) {
                self->connectBroker();
            } else {
                self->processQueue();
            }
        }
        
        // Mantém a task rodando
        vTaskDelay(pdMS_TO_TICKS(100)); // Verifica a cada 100ms
    }
}