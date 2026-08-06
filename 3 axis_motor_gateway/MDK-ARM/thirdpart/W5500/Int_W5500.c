#include "Int_W5500.h"

uint8_t GATEWAY[4] = {192, 168, 48, 1};
uint8_t SUB[4] = {255, 255, 255, 0};
uint8_t MAC[6] = {110, 120, 130, 140, 150, 160};
uint8_t SIP[4] = {192, 168, 48, 211};
uint8_t SERVERIP[4] = {192, 168, 48, 25};

//!========================================================================================================================================================================

/**
 * @brief 复位W5500
 *
 */
void __Int_W5500_Reset(void)
{

    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);

    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(100);
}

//!========================================================================================================================================================================

//!========================================================================================================================================================================

/**
 * @brief 初始化W5500
 *
 */
void Int_W5500_Init(void)
{
    uint8_t tx_size[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    uint8_t rx_size[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    uint8_t version = 0;
    uint8_t read_ip[4] = {0};

    printf("Int_W5500_Init\n");
    // 1. 复位
    __Int_W5500_Reset();

    // 2. 更新回调函数
    wizchip_update_callback();

    version = getVERSIONR();
    printf("W5500 VERSIONR = 0x%02X\n", version);
    if (version != 0x04)
    {
        printf("W5500 SPI check failed\n");
        return;
    }

    if (wizchip_init(tx_size, rx_size) != 0)
    {
        printf("wizchip_init failed\n");
        return;
    }

    // 3. 寄存器配置
    setGAR(GATEWAY); // 设置网关

    setSUBR(SUB); // 设置子网掩码

    setSHAR(MAC); // 设置源MAC地址

    setSIPR(SIP); // 设置源IP地址

    getSIPR(read_ip);
    printf("W5500 IP = %d.%d.%d.%d\n", read_ip[0], read_ip[1], read_ip[2], read_ip[3]);

    HAL_Delay(1000);
}

//!========================================================================================================================================================================
/**
 * @brief 发送数据
 *
 * @param data
 * @param len
 */
void Int_W5500_SendData(uint8_t *data, uint16_t len)
{

    while (1)
    {
        // 获取Socket状态
        uint8_t sock_state = getSn_SR(W5500_SOCKET_NUM);

        if (sock_state == SOCK_CLOSED)
        {
            // 创建客户端
            int8_t state =  socket(W5500_SOCKET_NUM, Sn_MR_TCP, 8888, NULL);
            if (state >= 0)
            {
                printf("Create Socket Success\n");
                /* code */
            }else{
                printf("Create Socket Failed %d\n", state);
            }


            /* code */
        }
        if (sock_state == SOCK_INIT)
        {
            //  连接服务区
            int8_t state = connect(W5500_SOCKET_NUM, SERVERIP, SERVER_PORT);

            if (state == SOCK_OK)
            {
                printf("Connect Server Success\n");
                /* code */
            }else{
                printf("Connect Server Failed %d\n", state);
            }

            /* code */
        }
        if (sock_state == SOCK_ESTABLISHED)
        {

            printf("Connect Server Success\n");
            break;
            /* code */
        }

        HAL_Delay(1000);
        /* code */
    }

    //  发送数据
    send(W5500_SOCKET_NUM, data, len);
    printf("Send Data Success\n");

    //  关闭客户端
    close(W5500_SOCKET_NUM);
    printf("Close Socket Success\n");
}

// 获取Socket状态
