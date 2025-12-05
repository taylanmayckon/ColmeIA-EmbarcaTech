#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

// Configurações do MQTT
#define WIFI_SSID "SEU_WIFI_SSID"
#define WIFI_PASS "SUA_WIFI_SENHA"
#define MQTT_BROKER_IP "192.168.1.107" // IP do seu Broker
#define BROKER_PORT 1883
#define MQTT_MSG_QUEUE_SIZE 120 // Quantas mensagens guardar se estiver offline

// Estrutura para mensagens na fila
struct MqttMessage {
    char topic[100];
    char payload[128];
};

class MqttClient {
public:
    // Construtor
    MqttClient();

    // Inicializa hardware Wi-Fi e estruturas
    bool begin();

    // Método para publicar mensagens (Thread-safe, usa fila)
    bool publish(const char* topic, const char* payload);

    // Função estática que será a Task do FreeRTOS
    static void taskImpl(void* _this);

private:
    mqtt_client_t* client;
    struct mqtt_connect_client_info_t clientInfo;
    QueueHandle_t msgQueue;
    bool connected;
    bool wifiConnected;

    // Conecta ao Wi-Fi
    void connectWifi();
    
    // Conecta ao Broker MQTT
    void connectBroker();

    // Processa a fila de mensagens pendentes
    void processQueue();

    // --- Callbacks estáticos necessários para o lwIP (C API) ---
    static void mqttConnectionCb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
    static void mqttPubRequestCb(void *arg, err_t result);
    static void mqttIncomingPublishCb(void *arg, const char *topic, u32_t tot_len);
    static void mqttIncomingDataCb(void *arg, const u8_t *data, u16_t len, u8_t flags);
};

#endif