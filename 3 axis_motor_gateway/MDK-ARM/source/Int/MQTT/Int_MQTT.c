#include "Int_MQTT.h"

uint8_t sendbuff[1024];
uint8_t recvbuff[1024];

MQTTClient mqtt_client;
Network mqtt_network;
uint8_t mqtt_connected = 0;

uint8_t MQTT_SERVER_IP[4] = {192, 168, 48, 25};
uint16_t MQTT_SERVER_PORT = 1883;

static SemaphoreHandle_t mqtt_mutex = NULL;

//!============================================================================================================================================

__weak void Int_MQTT_parse_handle(char *json)
{
}

//!============================================================================================================================================

static char mqtt_json_buf[256];
void mqtt_messageHandler(MessageData *data)
{

    uint16_t n = data->message->payloadlen;
    if (n >= sizeof(mqtt_json_buf))
    {
        n = sizeof(mqtt_json_buf) - 1;
        /* code */
    }
    memcpy(mqtt_json_buf, data->message->payload, n);

    mqtt_json_buf[n] = '\0'; // 结束符

    // printf("mqtt_messageHandler data: %s\r\n", (uint8_t *)data->message->payload);
    Int_MQTT_parse_handle(mqtt_json_buf);
    // 清零数据
    // memset(data->message->payload, 0, data->message->payloadlen);
}

/**
 * @brief 初始化MQTT
 *
 */
void Int_MQTT_Init(void)
{

    if (mqtt_mutex == NULL)
    {
        mqtt_mutex = xSemaphoreCreateMutex();
    }
    mqtt_connected = 0;

    // 连接服务器
    Int_W5500_Init();

    NewNetwork(&mqtt_network, SOCKET_NUM); // SN:socket number

    if (ConnectNetwork(&mqtt_network, MQTT_SERVER_IP, MQTT_SERVER_PORT) != SOCK_OK)
    {
        printf("ConnectNetwork failed\n");
        return;
        /* code */
    }
    printf("ConnectNetwork success\n");

    // 1. 创建网络客户端
    MQTTClientInit(&mqtt_client, &mqtt_network, 1000, sendbuff, 1024, recvbuff, 1024);

    // 2. 连接服务器
    MQTTPacket_connectData mqtt_options = MQTTPacket_connectData_initializer;
    mqtt_options.clientID.cstring = "daiwei9527/console_to_gateway";
    mqtt_options.keepAliveInterval = 60;
    mqtt_options.willFlag = 0; // 退出前的最后一条消息
    if (MQTTConnect(&mqtt_client, &mqtt_options) != SUCCESS)
    {
        printf("MQTTConnect failed\n");
        return;
        /* code */
    }
    mqtt_connected = 1;
    printf("MQTTConnect success\n");

    // 3.订阅主题
    MQTTSubscribe(&mqtt_client, "daiwei9527/console_to_gateway", QOS0, mqtt_messageHandler);
    printf("mqtt subscribe success\n");
}

//!============================================================================================================================================

/**
 * @brief 发送MQTT数据
 *
 * @param data 数据指针
 * @param len 数据长度
 */
void Int_MQTT_SendData(uint8_t *data, uint16_t len)
{

    if (!mqtt_connected || mqtt_mutex == NULL)
    {
        return;
        /* code */
    }

    if (xSemaphoreTake(mqtt_mutex, pdMS_TO_TICKS(500)) != pdTRUE)
    {
        return;
        /* code */
    }

    // 发布mqtt消息
    MQTTMessage message;
    message.payload = data;
    message.payloadlen = len;
    message.qos = QOS0; // 数据传输安全等级
    if (MQTTPublish(&mqtt_client, "daiwei9527/gateway_to_console", &message) != SUCCESS)
    {
        printf("MQTT publish failed\n");
        /* code */
    }

    // MQTTPublish(&mqtt_client, "daiwei9527/gateway_to_console", &message);
    printf("MQTT publish success\n");
    xSemaphoreGive(mqtt_mutex);
}

//!============================================================================================================================================

/**
 * @brief 刷新MQTT连接
 *
 */
void Int_MQTT_Refresh(void)
{

    if (!mqtt_connected)
    {
        return;
        /* code */
    }
    if (xSemaphoreTake(mqtt_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return;
    }

    // 周期性获取MQTT数据
    MQTTYield(&mqtt_client, 10);
    xSemaphoreGive(mqtt_mutex);
}
