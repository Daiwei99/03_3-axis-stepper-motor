#include "App_Main.h"

//! @brief MQTT消息传递任务
void mqtt_task(void *pvParameters);
#define TASK_MQTT_NAME "mqtt_task"
#define TASK_MQTT_PRIORITY 4
#define TASK_MQTT_STACK_SIZE 1024
TaskHandle_t mqtt_task_handle = NULL;

//! @brief 网关到电机的消息传递任务
void gateway_to_motor_task(void *pvParameters);
#define TASK_GATEWAY_TO_MOTOR_NAME "gateway_to_motor_task"
#define TASK_GATEWAY_TO_MOTOR_PRIORITY 4
#define TASK_GATEWAY_TO_MOTOR_STACK_SIZE 512
TaskHandle_t gateway_to_motor_task_handle = NULL;

//! @brief 电机到网关的消息传递任务
void motor_to_gateway_task(void *pvParameters);
#define TASK_MOTOR_TO_GATEWAY_NAME "motor_to_gateway_task"
#define TASK_MOTOR_TO_GATEWAY_PRIORITY 4
#define TASK_MOTOR_TO_GATEWAY_STACK_SIZE 512
TaskHandle_t motor_to_gateway_task_handle = NULL;

//!============================================================================================================================================
// uint16_t revolution = 1;
// uint16_t speed = 1600;
typedef enum
{
    GW_CMD_NONE,
    GW_CMD_CONTINUOUS,
    GW_CMD_STOP,
    GW_CMD_HOMING,
    GW_CMD_STATUS, // CAN byte0=4 请求状态（协议保留，暂未用）
    GW_CMD_POSITION,
} GW_CMD;

#define SPEED_MAX_STEP_S 10000u // 与网页滑块上限一致
#define MAX_TRAVEL_MM 235.0

volatile GW_CMD g_cmd = GW_CMD_NONE;
volatile uint8_t g_dir = 0;           // 0=hand 1=motor
volatile uint16_t g_dist_01mm = 0;    // 0.1mm
volatile uint32_t g_speed = 1600;     // step/s
volatile uint32_t g_last_hb_tick = 0; // 连续运动看门狗

#define STEPS_PER_MM 200
                                      //! @brief 电机状态缓存（只被 motor_to_gateway_task 用，不用 volatile）
typedef struct
{
    uint8_t running;    // 0:停止，1：运行
    uint8_t home_phase; // 0—7
    uint16_t speed;     // step/s
    int32_t pos_step;   // 当前位置（步）
    uint8_t valid;      // 收到过至少一帧
} Motor_Status;

static Motor_Status s_st;

//! @brief 解析电机上报帧（8字节新格式，兼容旧4字节）
static void parse_status_frame(const CAN_Message *m)
{
    const uint8_t *data = m->data;
    if (m->data_len >= 8)
    {
        s_st.running = data[0];
        s_st.home_phase = data[1];
        s_st.speed = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
        s_st.pos_step = (int32_t)((uint32_t)data[4] | ((uint32_t)data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24));
        /* code */
    }
    else if (m->data_len >= 4)
    {
        // 兼容旧格式：data[0..1]=距离mm(大端) data[2..3]=状态
        uint16_t dis_mm = ((uint16_t)data[0] << 8) | data[1];
        uint16_t st = ((uint16_t)data[2] << 8) | data[3];
        s_st.running = (st == 1) ? 1 : 0;
        s_st.pos_step = (int32_t)dis_mm * STEPS_PER_MM;
        s_st.speed = 0;
        s_st.home_phase = 0;
        /* code */
    }
    else
    {
        return;
    }
    s_st.valid = 1;
}

//! @brief 组 JSON 发到网页
static void publish_status(void)
{

    cJSON *root = cJSON_CreateObject();
    // malloc失败判空 ， F103堆很紧
    if (root == NULL)
    {
        return;
        /* code */
    }
    cJSON_AddNumberToObject(root, "device_id", 1);
    cJSON_AddStringToObject(root, "motor_status", s_st.running ? "on" : "off");
    cJSON_AddStringToObject(root, "direction", s_st.running ? (g_dir ? "motor" : "hand") : "stop");
    cJSON_AddNumberToObject(root, "cur_speed_step_s", s_st.speed);
    cJSON_AddNumberToObject(root, "cur_position_mm", (double)s_st.pos_step / STEPS_PER_MM);
    cJSON_AddNumberToObject(root, "cur_angle", (double)s_st.pos_step / 1600.0 * 360.0);
    cJSON_AddNumberToObject(root, "home_phase", s_st.home_phase);
    cJSON_AddBoolToObject(root, "homed", s_st.home_phase == 6);

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root); // root用完立刻删，先把堆还回去
    if (str == NULL)
    {
        return;
        /* code */
    }
    Int_MQTT_SendData((uint8_t *)str, strlen(str));
    cJSON_free(str); // 释放str内存
}

//!============================================================================================================================================

//? ================== 两个小工具：把判空判类型收在一处 ==================
static int json_str_eq(const cJSON *root, const char *key, const char *val)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(root, key);
    return (cJSON_IsString(it) && it->valuestring && strcmp(it->valuestring, val) == 0);
}

static double json_num(const cJSON *root, const char *key, double def)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsNumber(it) ? it->valuedouble : def;
}

//!============================================================================================================================================

//! @brief MQTT信号量
SemaphoreHandle_t mqtt_semphore_handle = NULL;

//! @brief 启动信号量
SemaphoreHandle_t start_semphore_handle = NULL;

//!============================================================================================================================================
//! @brief 主函数
void App_Main(void)
{

    // 初始化CAN
    Int_CAN_Init();

    // 创建MQTT信号量
    mqtt_semphore_handle = xSemaphoreCreateBinary();

    // 创建启动信号量
    start_semphore_handle = xSemaphoreCreateBinary();

    // 创建MQTT消息传递任务
    xTaskCreate(
        mqtt_task,
        TASK_MQTT_NAME,
        TASK_MQTT_STACK_SIZE,
        NULL,
        TASK_MQTT_PRIORITY,
        &mqtt_task_handle);

    // 创建网关到电机的消息传递任务
    BaseType_t res = xTaskCreate(
        gateway_to_motor_task,
        TASK_GATEWAY_TO_MOTOR_NAME,
        TASK_GATEWAY_TO_MOTOR_STACK_SIZE,
        NULL,
        TASK_GATEWAY_TO_MOTOR_PRIORITY,
        &gateway_to_motor_task_handle);

    // printf("gateway_to_motor_task res = %d, handle = %p, free heap = %d\n", res, gateway_to_motor_task_handle, (unsigned int)xPortGetFreeHeapSize());

    // 创建电机到网关的消息传递任务
    xTaskCreate(
        motor_to_gateway_task,
        TASK_MOTOR_TO_GATEWAY_NAME,
        TASK_MOTOR_TO_GATEWAY_STACK_SIZE,
        NULL, TASK_MOTOR_TO_GATEWAY_PRIORITY,
        &motor_to_gateway_task_handle);
    // 启动调度器
    vTaskStartScheduler();
}

//!============================================================================================================================================

/**
 * @brief 解析MQTT消息
 *
 * @param json
 */
// void Int_MQTT_parse_handle(char *json)
// {
//     // 解析JSON字符串，获取revolution和speed
//     cJSON *json_obj = cJSON_Parse(json);
//     uint16_t target_distance = cJSON_GetObjectItem(json_obj, "target_distance")->valueint;
//     speed = cJSON_GetObjectItem(json_obj, "max_speed")->valueint;
//     revolution = target_distance / 8;

//     // 释放JSON对象
//     cJSON_Delete(json_obj);

//     xSemaphoreGive(mqtt_semphore_handle);
// }

//?================== 解析主体：替换原来的 Int_MQTT_parse_handle ==================
void Int_MQTT_parse_handle(char *json)
{

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        printf("Int_MQTT_parse_handle: json_Parse failed\r\n");
        return;
        /* code */
    }

    // ---- 1. heartbeat 只喂狗，绝不重发启动指令 ----
    //    不加这条，按住方向键就是每秒重启一次电机
    if (json_str_eq(root, "action", "heartbeat"))
    {
        g_last_hb_tick = xTaskGetTickCount();
        cJSON_Delete(root); // 每条return路径都要delete
        return;
        /* code */
    }

    // ---- 2. 速度：优先原生 speed_step_s，回落兼容字段 max_speed ----
    double spd = (json_num(root, "speed_step_s", 0.0));
    if (spd <= 0.0)
    {
        spd = json_num(root, "max_speed", 0.0);
        /* code */
    }
    if (spd < 0.0)
    {
        spd = 0.0;
        /* code */
    }
    if (spd > SPEED_MAX_STEP_S)
    {
        spd = SPEED_MAX_STEP_S; // 别信来自网络的数值
        /* code */
    }
    uint32_t sp = (uint32_t)spd;

    // ---- 3. 方向 ----
    uint8_t dir = json_str_eq(root, "direction", "motor") ? 1 : 0;

    // ---- 4. 按 action / mode 分流 ----
    GW_CMD cmd = GW_CMD_NONE;
    uint16_t dist_01mm = 0;

    if (json_str_eq(root, "action", "stop"))
    {
        cmd = GW_CMD_STOP; // 停止不需要速度和距离
        /* code */
    }
    else if (json_str_eq(root, "action", "homing") || json_str_eq(root, "mode", "homing"))
    {
        cmd = GW_CMD_HOMING;
        /* code */
    }
    else if (json_str_eq(root, "mode", "position"))
    {
        cmd = GW_CMD_POSITION;
        double mm = json_num(root, "target_distance", 0.0);
        if (mm < 0.0)
        {
            mm = 0.0;
            /* code */
        }
        if (mm > MAX_TRAVEL_MM)
        {
            mm = MAX_TRAVEL_MM; // 行程上限钳位
            /* code */
        }
        dist_01mm = (uint16_t)(mm * 10.0 + 0.5); // mm → 0.1mm，四舍五入

        // 无效指令直接丢，不下发
        if (dist_01mm == 0 || sp == 0)
        {
            printf(" position ignored : dist = % u sp = % lu\r\n ", dist_01mm, (unsigned long)sp);
            cJSON_Delete(root);
            return;
            /* code */
        }

        /* code */
    }
    else
    {
        cmd = GW_CMD_CONTINUOUS;              // action=start && mode=continuous
        g_last_hb_tick = xTaskGetTickCount(); // 启动即算一次喂狗
        if (sp == 0)
        {
            cJSON_Delete(root);
            return;
            /* code */
        }
    }
    cJSON_Delete(root); // 后面不再用root,先释放

    g_dir = dir;
    g_speed = sp;
    g_dist_01mm = dist_01mm;
    g_cmd = cmd;
    xSemaphoreGive(mqtt_semphore_handle);
}

//!============================================================================================================================================

//! @brief MQTT消息传递任务
void mqtt_task(void *pvParameters)
{
    // 开始任务
    printf("mqtt_task start\n");
    // 周期性获取MQTT中的消息

    // revolution = 10;
    // speed = 1600 * 4;

    // // 释放MQTT信号量，通知网关到电机的消息传递任务
    // xSemaphoreGive(mqtt_semphore_handle);
    /* code */

    // 初始化MQTT连接
    Int_MQTT_Init();

    Int_MQTT_SendData((uint8_t *)"daiwei666", 9);
    while (1)
    {
        // 周期性的获取数据
        Int_MQTT_Refresh();
        vTaskDelay(pdMS_TO_TICKS(20));
        // vTaskDelay(1000);
    }
}
//!============================================================================================================================================

//! @brief 网关到电机的消息传递任务
void gateway_to_motor_task(void *pvParameters)
{
    // 开始任务
    printf("gateway_to_motor_task start\n");
    // 任务循环
    /* code */
    while (1)
    {
        // 循环等待MQTT信号量
        // printf("gateway wait semaphore\r\n");

        if (xSemaphoreTake(mqtt_semphore_handle, portMAX_DELAY) == pdTRUE)
        {
            // 向电机发送启动数据:高位优先
            uint8_t data[8] = {0};
            uint16_t d = g_dist_01mm;
            uint32_t s = g_speed;

            data[0] = (uint8_t)g_cmd; // 1：连续，2：停，3：回零，4：定位
            data[1] = g_dir;
            data[2] = (uint8_t)(d);
            data[3] = (uint8_t)(d >> 8);
            data[4] = (uint8_t)(s);
            data[5] = (uint8_t)(s >> 8);
            data[6] = (uint8_t)(s >> 16);
            data[7] = (uint8_t)(s >> 24);

            // printf("gateway got semaphore\r\n");

            Int_CAN_SendData(data, 8);
            printf("CAN-> cmd=%u dir=%u dist=%u sp=%lu\r\n",
                   data[0], data[1], d, (unsigned long)s);

            // printf("gateway CAN sent\r\n");

            // vTaskDelay(pdMS_TO_TICKS(200));

            // 通知其他任务获取电机状态
            if (g_cmd != GW_CMD_STOP)
            {
                xSemaphoreGive(start_semphore_handle);
                /* code */
            }

            /* code */
        }

        // vTaskDelay(1000);
    }
}

//!============================================================================================================================================

//! @brief 电机到网关的消息传递任务:独立状态转发器，不受指令门控
void motor_to_gateway_task(void *pvParameters)
{
    // 开始任务
    printf("motor_to_gateway_task start\n");

    uint32_t last_pub = 0;
    uint8_t prev_run = 0xff; // 0xff = 还没收到过，保证第一帧必发
    uint8_t prev_phase = 0xff;
    // 任务循环
    /* code */
    while (1)
    {
        // ---- 1. 无脑收，把 FIFO 掏干 ----

        CAN_Message msg[3] = {0};
        uint32_t n = 0;
        Int_CAN_ReceiveData(msg, &n);
        for (uint32_t i = 0; i < n; i++)
        {
            parse_status_frame(&msg[i]);
            /* code */
        }

        // ---- 2. 按节奏发：状态跳变立刻发，否则定时发 ----
        //    运行中 200ms（网页位置刷得平滑），空闲 1s（省 Broker 流量和堆）
        uint32_t now = xTaskGetTickCount();
        uint32_t interval = s_st.running ? pdMS_TO_TICKS(200) : pdMS_TO_TICKS(1000);
        uint8_t changed = (s_st.running != prev_run) || (s_st.home_phase != prev_phase);

        if (s_st.valid && (changed || (now - last_pub) >= interval))
        {
            publish_status();
            prev_run = s_st.running;
            prev_phase = s_st.home_phase;
            last_pub = now;
            /* code */
        }
        vTaskDelay(pdMS_TO_TICKS(50));

        // if (xSemaphoreTake(start_semphore_handle, portMAX_DELAY) == pdTRUE)
        // {

        //     while (1)
        //     {
        //         /* code */

        //         // 周期性接收电机的运行数据
        //         CAN_Message message[3] = {0};
        //         uint32_t data_len = 0;
        //         Int_CAN_ReceiveData(message, &data_len);

        //         if (data_len > 0)
        //         {
        //             for (uint8_t i = 0; i < data_len; i++)
        //             {
        //                 uint8_t *data = message[i].data;
        //                 uint16_t dis = (data[0] << 8) | data[1];
        //                 uint16_t state = (data[2] << 8) | data[3];
        //                 if (state == 1)
        //                 {
        //                     // 电机运行中
        //                     // printf("dis = %d\n", dis);
        //                     // printf("state = %d\n", state);

        //                     // 创建JSON对象
        //                     cJSON *json_obj = cJSON_CreateObject();
        //                     cJSON_AddNumberToObject(json_obj, "cur_distance", dis);
        //                     cJSON_AddStringToObject(json_obj, "motor_status", "on");

        //                     char *json_string = cJSON_PrintUnformatted(json_obj);

        //                     Int_MQTT_SendData((uint8_t *)json_string, strlen(json_string));

        //                     // 释放JSON对象
        //                     cJSON_Delete(json_obj);
        //                     free(json_string);
        //                     /* code */
        //                 }
        //                 if (state == 2)
        //                 {
        //                     // 电机停止
        //                     // printf("dis = %d\n", revolution * 8);
        //                     // printf("state = %d\n", state);

        //                     // 创建JSON对象
        //                     cJSON *json_obj = cJSON_CreateObject();
        //                     cJSON_AddNumberToObject(json_obj, "cur_distance", dis * 8);
        //                     cJSON_AddStringToObject(json_obj, "motor_status", "off");

        //                     char *json_string = cJSON_PrintUnformatted(json_obj);

        //                     Int_MQTT_SendData((uint8_t *)json_string, strlen(json_string));

        //                     // 释放JSON对象
        //                     cJSON_Delete(json_obj);
        //                     free(json_string);
        //                     goto end_while;
        //                 }

        //                 /* code */
        //             }

        //             /* code */
        //         }
        //         vTaskDelay(200);
        //     }
        // end_while:;

        //     /* code */
        // }

        // vTaskDelay(1000);
    }
}

//! @brief 栈溢出钩子：哪个任务爆栈会打出名字
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("\r\n*** STACK OVERFLOW: %s ***\r\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;)
        ; // 停住，让你能看到串口那行字
}

//! @brief heap 耗尽钩子：cJSON / xTaskCreate 分配失败会进来
void vApplicationMallocFailedHook(void)
{
    printf("\r\n*** MALLOC FAILED, free heap = %u ***\r\n",
           (unsigned int)xPortGetFreeHeapSize());
    taskDISABLE_INTERRUPTS();
    for (;;)
        ;
}

//!============================================================================================================================================
