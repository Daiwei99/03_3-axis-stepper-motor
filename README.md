# 步进电机项目总结

> 更新时间：2026-08-06  
> 说明：本节是在每日学习记录基础上整理的项目总览；后文“原始学习记录”完整保留原有内容、代码和图片引用。

## 一、项目目标

本项目以三轴步进电机控制系统为最终目标，当前先完成单轴丝杆滑台的功能验证。系统通过网关板连接网页控制台和电机控制板，实现远程运动控制、状态反馈和基础安全保护。

核心目标包括：

- 控制滑台向电机方向或手轮方向运动。
- 支持连续运动、停止、回零和指定距离定位。
- 支持速度、方向和目标距离参数下发。
- 通过光电开关建立零点并限制危险方向运动。
- 通过 CAN 在网关板与电机板之间传递控制命令和状态。
- 通过 W5500 和 MQTT 实现网页控制台与网关板之间的数据交互。
- 后续扩展到三轴控制、编码器闭环和更完整的安全机制。

## 二、系统架构

```text
网页控制台
    │ MQTT/JSON
    ▼
MQTT Broker（192.168.48.25:1883）
    │ 以太网
    ▼
网关板 STM32F103C8T6 + W5500 + FreeRTOS
    │ CAN 500 kbit/s
    ▼
电机控制板 STM32F407ZGT6
    │ DIR + PWM/STEP + EN
    ▼
步进电机驱动器 → 42 步进电机 → 丝杆滑台
    ▲
    └── 光电限位开关 / 编码器反馈
```

## 三、硬件与软件模块

| 模块       | 主要器件或技术                | 作用                                           |
| ---------- | ----------------------------- | ---------------------------------------------- |
| 电机控制板 | STM32F407ZGT6                 | 生成 PWM 脉冲、执行运动状态机、处理回零和限位  |
| 网关板     | STM32F103C8T6                 | 运行 FreeRTOS，完成 MQTT、JSON 与 CAN 协议转换 |
| 网络接口   | W5500 + SPI                   | 提供硬件 TCP/IP 和以太网连接                   |
| 总线通信   | CAN，500 kbit/s               | 网关与电机控制板之间的可靠通信                 |
| 执行机构   | 42 步进电机、驱动器、丝杆滑台 | 将旋转运动转换为直线运动                       |
| 位置基准   | 槽型光电开关                  | 回零检测和行程边界保护                         |
| 位置反馈   | ABZ 编码器                    | 检测实际转动方向、速度和位置，为闭环控制做准备 |
| 应用协议   | MQTT + JSON                   | 网页命令下发和电机状态上报                     |

## 四、当前已实现功能

- 使用 TIM2 PWM 驱动步进电机，支持动态调整速度。
- 完成 1/8 细分下的步数、圈数和距离换算。
- 完成正反转、固定圈数运动、按键点动和连续运动。
- 完成光电开关回零及零点方向保护。
- 引入梯形加减速，降低高速启动时的堵转和丢步风险。
- 完成 CAN 正常模式通信及接收过滤器配置。
- 完成网关板 W5500 初始化、TCP 连接和 MQTT Broker 连接。
- 完成 MQTT 订阅、发布和 JSON 命令解析。
- 完成网关到电机的 8 字节控制帧，以及电机到网关的 8 字节状态帧。
- 使用 FreeRTOS 任务、互斥量和二值信号量组织数据流。
- 增加栈溢出和堆分配失败钩子，便于定位运行时错误。

## 五、每日进展摘要

| 阶段   | 主要进展                                        | 阶段结果                                             |
| ------ | ----------------------------------------------- | ---------------------------------------------------- |
| Day 01 | 明确应用场景、项目目标和机械硬件组成            | 建立三轴项目总体认识，确定先做单轴验证               |
| Day 02 | 搭建 F407 工程，比较 GPIO 与定时器驱动方式      | 使用 TIM2 输出 PWM，电机能够按设定速度和步数运行     |
| Day 03 | 编写五按键扫描、方向控制、回零和连续运动        | 支持手动控制、按住运动、松开停止及光电回零           |
| Day 04 | 学习并实现梯形加减速，了解 ABZ 编码器           | 电机启动和停止更加平稳，建立闭环控制基础             |
| Day 05 | 配置编码器分辨率和计数逻辑，启动网关开发        | 形成电机实际位置检测思路，完成网关基础工程配置       |
| Day 06 | 学习 CAN 物理层、帧格式、仲裁、位时序和过滤器   | 完成 500 kbit/s 参数配置及环回/正常模式验证          |
| Day 07 | 移植 W5500、MQTT、cJSON，建立三个 FreeRTOS 任务 | 打通网页命令到 CAN、电机状态到 MQTT 的网关数据流     |
| Day 08 | 完善电机板 CAN 指令解析和状态上报，学习 CANopen | 电机板可执行网关命令并返回运行、回零、速度和位置状态 |

## 六、电机控制关键参数

| 参数          | 当前值或计算方式            | 说明                                         |
| ------------- | --------------------------- | -------------------------------------------- |
| 电机整步数    | 200 step/rev                | 每步 1.8°                                    |
| 细分          | 1/8                         | 一圈需要 1600 个脉冲                         |
| 丝杆位移      | 8 mm/rev                    | 一圈对应 8 mm 直线位移                       |
| TIM2 输入时钟 | 84 MHz                      | STM32F407 APB1 定时器时钟                    |
| TIM2 预分频   | PSC = 83                    | 计数频率降为 1 MHz，即每计数 1 μs            |
| PWM 周期      | `ARR = 1000000 / speed - 1` | `speed` 的单位为 step/s                      |
| PWM 占空比    | 约 50%                      | `CCR = (ARR + 1) / 2`，主要保证脉冲波形稳定  |
| CAN 波特率    | 500 kbit/s                  | 当前两块板使用相同位时序参数                 |
| 网页速度上限  | 10000 step/s                | 网关解析后进行上限钳位                       |
| 网页行程上限  | 235 mm                      | 需要与机械实际行程及电机板圈数限制进一步统一 |

电机速度换算示例：

```text
1600 step/s = 1 rev/s = 8 mm/s
3200 step/s = 2 rev/s = 16 mm/s
6400 step/s = 4 rev/s = 32 mm/s
```

## 七、运动控制逻辑

当前电机控制主要包含以下模式：

| 模式         | 行为                                                       |
| ------------ | ---------------------------------------------------------- |
| 固定距离运动 | 设置方向、目标圈数和目标速度，按梯形曲线完成运动           |
| 连续运动     | 不设置目标步数，按指定方向持续运行，收到停止命令后减速停止 |
| 回零         | 向手轮方向运行，光电开关触发后停止并建立零点状态           |
| 点动控制     | 按键按下时运动，松开后停止                                 |
| 状态反馈     | 周期上报运行状态、回零阶段、当前速度和当前位置             |

梯形加减速将运动过程划分为加速、匀速和减速三个阶段。当目标距离不足以容纳完整的加速段和减速段时，重新计算可达到的最大速度，形成无匀速段的三角形速度曲线。

## 八、CAN 通信协议

### 1. CAN 标识符

| 数据方向    | 标准 ID | 十六进制 |
| ----------- | ------: | -------: |
| 网关 → 电机 |    1001 |  `0x3E9` |
| 电机 → 网关 |    1002 |  `0x3EA` |

两端均采用标准帧、数据帧和 FIFO0，过滤器使用 ID 列表模式。

### 2. 网关下发控制帧

长度固定为 8 字节，数值采用小端序：

| 字节         | 内容     | 类型/单位               |
| ------------ | -------- | ----------------------- |
| `data[0]`    | 控制命令 | `uint8_t`               |
| `data[1]`    | 方向     | `0=hand`，`1=motor`     |
| `data[2..3]` | 目标距离 | `uint16_t`，单位 0.1 mm |
| `data[4..7]` | 目标速度 | `uint32_t`，单位 step/s |

控制命令：

| 命令值 | 含义               |
| -----: | ------------------ |
|      0 | 无命令             |
|      1 | 连续运动           |
|      2 | 停止               |
|      3 | 回零               |
|      4 | 状态请求，当前保留 |
|      5 | 指定距离定位       |

### 3. 电机上报状态帧

| 字节         | 内容     | 类型/单位                  |
| ------------ | -------- | -------------------------- |
| `data[0]`    | 运行状态 | `0=停止`，`1=运行`         |
| `data[1]`    | 回零阶段 | 当前使用 `0/1/6` 粗略表示  |
| `data[2..3]` | 当前速度 | `uint16_t`，step/s，小端序 |
| `data[4..7]` | 当前位置 | `int32_t`，step，小端序    |

## 九、MQTT 与 JSON 协议

### 1. 网络参数

| 参数        | 当前配置                        |
| ----------- | ------------------------------- |
| W5500 IP    | `192.168.48.211`                |
| 子网掩码    | `255.255.255.0`                 |
| 默认网关    | `192.168.48.1`                  |
| MQTT Broker | `192.168.48.25:1883`            |
| 控制主题    | `daiwei9527/console_to_gateway` |
| 状态主题    | `daiwei9527/gateway_to_console` |
| QoS         | QoS 0                           |

### 2. 控制台下发字段

| 字段              | 示例                                   | 作用                     |
| ----------------- | -------------------------------------- | ------------------------ |
| `action`          | `start`、`stop`、`homing`、`heartbeat` | 指定主要动作             |
| `mode`            | `continuous`、`position`、`homing`     | 指定运动模式             |
| `direction`       | `motor`、`hand`                        | 指定运动方向             |
| `speed_step_s`    | `3200`                                 | 目标速度，优先使用该字段 |
| `max_speed`       | `3200`                                 | 兼容旧版控制台的速度字段 |
| `target_distance` | `100.0`                                | 定位距离，单位 mm        |

连续运动示例：

```json
{"action":"start","mode":"continuous","direction":"motor","speed_step_s":3200}
```

定位运动示例：

```json
{"action":"start","mode":"position","direction":"motor","target_distance":100.0,"speed_step_s":3200}
```

### 3. 网关上报状态字段

```json
{
  "device_id": 1,
  "motor_status": "on",
  "direction": "motor",
  "cur_speed_step_s": 3200,
  "cur_position_mm": 80.0,
  "cur_angle": 3600.0,
  "home_phase": 1,
  "homed": false
}
```

网关运行中约每 200 ms 发布一次状态，停止时约每 1 s 发布一次；运行状态或回零阶段发生变化时立即发布。

## 十、FreeRTOS 任务与同步机制

| 对象                    | 职责                                                         |
| ----------------------- | ------------------------------------------------------------ |
| `mqtt_task`             | 初始化 W5500/MQTT，周期调用 `MQTTYield()` 接收消息和维护连接 |
| `gateway_to_motor_task` | 等待 MQTT 命令信号量，组装 8 字节 CAN 控制帧并发送           |
| `motor_to_gateway_task` | 接收 CAN 状态帧，生成 JSON 并发布到网页状态主题              |
| `mqtt_mutex`            | 防止多个任务同时操作 MQTT 客户端                             |
| `mqtt_semphore_handle`  | 通知网关发送任务有新的控制命令                               |
| `start_semphore_handle` | 原设计用于启动状态接收流程，当前状态任务已改为独立轮询       |

## 十一、当前需要继续完善的事项

1. 当前主要完成单轴验证，三轴设备编号、任务调度和协议扩展尚未实现。
2. 编码器已完成原理学习和基础配置，闭环位置校正、速度环及 PID 参数仍需继续验证。
3. MQTT 初始化失败后目前直接返回，需要增加断线检测、自动重连和重新订阅。
4. `Int_MQTT_SendData()` 的成功日志应只在 `MQTTPublish()` 返回成功时打印。
5. 心跳消息目前记录了时间戳，还需要增加超时判断并在超时后主动停止连续运动。
6. 二值信号量只能表示“有命令”，连续快速到来的命令可能被覆盖；后续可改为 FreeRTOS 队列保存完整命令。
7. 网页行程上限 235 mm 与电机板早期 28 圈限制（约 224 mm）需要根据机械实测统一。
8. CAN 初始化、接收和发送函数需要补全返回值检查、错误计数和总线异常恢复。
9. 需要完成长时间运行、断网恢复、急停、限位、反复回零和高负载测试。
10. CANopen 当前处于知识学习阶段，尚未替代现有自定义 CAN 应用协议。

## 十二、阶段结论

当前项目已经完成从网页控制台到物理电机的基本闭环数据链路：控制台通过 MQTT 下发 JSON，网关解析后转换为 CAN 控制帧，电机板执行运动并返回状态，网关再通过 MQTT 将状态发布给控制台。下一阶段的重点不再是“让电机转起来”，而是提高协议可靠性、运动安全性、位置精度，并从单轴扩展到三轴。

---

# 原始学习记录（完整保留）

# Day 01

## 一、项目介绍

1. 今天是电机项目的第一天，这个项目是经过改进之后的项目，可以实现三轴转动，但是教学的话先使用单轴，这个项目用到了Modbus/CAN总线通信协议与上位机或其他控制设备进行通信，可以实现对步进电机丝杆的远程控制、状态检测和参数配置。

   <hr/>

2. 可以应用到打印机（喷墨、激光、3D）、数控机械（CNC机床、激光切割机）、自动化设备（机器人的机械臂、自动装配线）、医疗仪器，这些领域几乎都是需要用到电机进行驱动工作的。

   <hr/>

3. 我们这个三轴步进电机项目最后要实现的目标就是要利用丝杆滑台将原本只能原地旋转的电机运动转化为直线运动。

   - 支持基本的运动指令控制：控制系统接收外部的控制指令，并按照指令来驱动滑台。例如：让滑台前进或者后退一步。
   - 支持增量运动控制：实现类似"向前移动1mm"或"向后移动1mm"这样的增量式运动。
   - 支持基于零点的定位控制：在完成回零（**Homing**）操作后，系统将建立一个统一的坐标基准。在此基础上，可以让滑台移动到指定的位置，例如移动到某一个设定的距离。
   - 具备基本的运动安全保护能力：在运动过程中，系统需要结合限位开关或光电传感器，防止滑台超出行程范围，避免因程序错误导致结构损坏。

<hr/>

## 二、项目硬件

1. 丝杆滑台：![1785150344879](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150344879.png)![1785150351244](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150351244.png)

   <hr/>

2. 42步进电机：![1785150406732](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150406732.png)

   <hr/>

3. 机械限位开关：![1785150430194](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150430194.png)![1785150437879](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150437879.png)

   <hr/>

4. 槽型光电开关:![1785150469378](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150469378.png)

   <hr/>

5. 铝型材:![1785150502109](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150502109.png)

   <hr/>

6. 丝杆、光轴

   <hr/>

7. 直线轴承、法兰螺母：![1785150595482](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150595482.png)![1785150605924](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150605924.png)

   <hr/>

8. 联轴器、轴承座、手轮：![1785150641377](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150641377.png)![1785150658802](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785150658802.png)

   <hr/>

<hr/>
<hr/>

# Day 02

## 一、项目架构

1. 我们要实现使用ModbusRS485/CAN通过网关板子控制电机开发板的转速、方向，然后电机开发板接收到指令之后控制丝杆滑台转动。![1785235692495](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785235692495.png)

   <hr/>

2. 首先我们需要在cubeMX中选择我们所使用的芯片STM32F407ZGT6芯片，这款芯片相较于之前开发板的芯片，比如C8T6、ZET6芯片，在性能方面做了很大的升级，CPU内核升级成了Cortex-M4，168MHz主频率，Flash：1MB，RAM：192KB，144引脚，定时器有14个。

   <hr/>

<hr/>

## 二、电机控制逻辑

1. 接下来我们就要了解一下怎么让电机转动起来。

   * 第一种方式就是通过GPIO引脚发送高低电平，电机收到一个上升沿就会走一步，一圈是360度，一般一圈是走200步，那么一步就是1.8度。一步1.8度这个精度还是不够，所以我需要做一个1/8细分，让一步更小，做了1/8细分之后，一圈就是1600步。那么我们需要给电机1600个上升沿他就可以实现转动一圈。
     * 第二种方式就是定时器方式，使用TIM2发送周期性的PWM方波信号。

   <hr/>

2. 还有就是我们需要注意一下延时函数，因为HAL库自带的HAL_DELAY()延时函数，他很可能被其他中断打断，而且我们设定走一步是3.2秒，实际上走了6、7秒。偏差很大，所以我们需要引入一个DWT组件函数，自己计算延时函数。但是这样的时间也有误差，不是特别准确所以，我们需要使用定时器来控制电机转动几圈。 

   <hr/>

3. TIM2是在APB1外设总线上，他的主频是84MHz，接下来我们就需要计算一下驱动电机（PWM）的定时器配置，现在输入频率是84，我们的输出频率需要1秒钟发送1600个PWM方波信号，也就是1600Hz/s，所以我们设置PSC=83，ARR=624，CCR（输出比较）设置ARR的一半，这个就是设置占空比。我们现在步进电机是根据一步一步转动，所以不需要设置占空比，设置百分之50，只是为了波形好看、上升沿干净。

   <hr/>

```c
   /**
    * @brief 设置电机速度,step/s
    *
    *        TIM2 计数时钟 = 84MHz / (PSC+1) = 84MHz / 84 = 1MHz(每个计数 1us)
    *        脉冲频率 f = 1000000 / (ARR+1)  ==>  ARR = 1000000 / speed - 1
    *        1600 step/s = 1 圈/s(1/8 细分:200 * 8 = 1600)
    *
    * @param speed 每秒脉冲数,step/s
    */
   void __Int_Motor_Set_Speed(double speed)
   {
       // 防止除零
       if (speed == 0)
       {
           return;
       }
   
       uint32_t arr = 1000000.0 / speed;   // 一个完整周期占多少个计数(us)
   
       __HAL_TIM_SET_AUTORELOAD(&htim2, arr - 1);              // 设置自动重装载寄存器的值
       __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, arr / 2);  // 设置比较寄存器（CCR）的值,50%占空比
   
       // ARR 已开启预装载(ARPE=1),新值要等更新事件才生效。
       // 定时器未运行时不会产生更新事件,这里手动产生一次,让新速度立即写入影子寄存器
       // if ((htim2.Instance->CR1 & TIM_CR1_CEN) == 0)
       // {
       //     htim2.Instance->EGR = TIM_EGR_UG;
       // }
   }
   
   
   
   //!========================================================================================================================================================================
   
   
   /**
    * @brief 使用定时器输出PWM方波信号，驱动电机的移动
    *
    */
   
   void __Int_Motor_Move_Start(void)
   {
   
       // 使能开关:PG0(拉高有效)
       HAL_GPIO_WritePin(STEPPER_ALL_EN_GPIO_Port, STEPPER_ALL_EN_Pin, GPIO_PIN_SET);
   
       printf("Motor Start Move\n");
   
       // 启动定时器
       HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_1);
   }
   //!========================================================================================================================================================================
   
   void __Int_Motor_Move_Stop(void)
   {
   
       // 关闭定时器
       HAL_TIM_PWM_Stop_IT(&htim2, TIM_CHANNEL_1);
   
       printf("Motor End Move\n");
   
       // 关闭电机使能
       HAL_GPIO_WritePin(STEPPER_ALL_EN_GPIO_Port, STEPPER_ALL_EN_Pin, GPIO_PIN_RESET);
   }
   
   //!========================================================================================================================================================================
   
   /**
    * @brief 定时器PWM脉冲完成回调函数====》当PWM方波高电平发送完毕后，会触发当前的回调函数
    *        但是PWM方波整个周期信号并没有发送完毕，还要发送无效电平信号
    *
    * @param htim
    */
   uint16_t step = 0;
   void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
   {
       step++;
       if (step == 1600)
       {
           step = 0;
           __Int_Motor_Move_Stop();
           /* code */
       }
   }
   
   //!========================================================================================================================================================================
   
   /**
    * @brief 初始化---电机
    *
    */
   void Int_Motor_Init(void)
   {
   
       // 基本参数配置:PA4:方向(顺时针，逆时针) ==》 丝杆滑台（前进，后退）
       HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_RESET);
   }
   
   //!========================================================================================================================================================================
   
   /**
    * @brief 移动---电机
    *
    */
   void Int_Motor_Move(void)
   {
   
       // 设置电机速度:step/s
       __Int_Motor_Set_Speed(6400);
       // 开始移动
       __Int_Motor_Move_Start();
   
       // 关闭移动
       // __Int_Motor_Move_Stop();
   
       // 计算速率
   
       // uint8_t cnt = 0;
       // while (cnt < 50)
       // {
   
       //     HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_RESET);
       //     // 移动逻辑:PA0
       //     for (uint16_t i = 0; i < 1600 * 25; i++)
       //     {
       //         // 向指定引脚发送脉冲信号(一个完整周期的信号)
       //         HAL_GPIO_WritePin(STEPPER_1_STEP_GPIO_Port, STEPPER_1_STEP_Pin, GPIO_PIN_SET);
       //         // HAL_Delay(1);
       //         // Com_Delay_us(100);
       //         Com_DWT_delay_us(100);
   
       //         HAL_GPIO_WritePin(STEPPER_1_STEP_GPIO_Port, STEPPER_1_STEP_Pin, GPIO_PIN_RESET);
       //         // HAL_Delay(1);
       //         // Com_Delay_us(100);
       //         Com_DWT_delay_us(100);
   
       //         /* code */
       //     }
   
       //     HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_SET);
       //     // 移动逻辑:PA0
       //     for (uint16_t i = 0; i < 1600 * 25; i++)
       //     {
       //         // 向指定引脚发送脉冲信号(一个完整周期的信号)
       //         HAL_GPIO_WritePin(STEPPER_1_STEP_GPIO_Port, STEPPER_1_STEP_Pin, GPIO_PIN_SET);
       //         // HAL_Delay(1);
       //         // Com_Delay_us(100);
       //         Com_DWT_delay_us(100);
   
       //         HAL_GPIO_WritePin(STEPPER_1_STEP_GPIO_Port, STEPPER_1_STEP_Pin, GPIO_PIN_RESET);
       //         // HAL_Delay(1);
       //         // Com_Delay_us(100);
       //         Com_DWT_delay_us(100);
   
       //         /* code */
       //     }
       //     cnt++;
       //     /* code */
       // }
   }
```

<hr/>

# Day 03

## 一、按键控制

1. 昨天我们实现了让电机转动起来了，但是还是有点不太方便，无法自主控制电机的转向和速度。今天要做的就实现按键控制电机的转向和还有归零操作，碰到光电开关他会自动停止，光电开关上面有一个亮光，如果我们不遮挡住这个光的话，他默认是高电平，如果遮挡住这个光的话，他就会拉低。接下来我们就先实现按键控制逻辑。

   <hr/>

2. 开发板上面有5个KEY，第一个KEY是回零按键，第二个KEY是向电机方向移动，第三个KEY是向手轮方向移动，第四个按键是按着就向左边运动，松开就停止转动，第五个按键是按着向右边运动，松开就停止转动。

   <hr/>

3. 首先我们在cubeMX里面配置这五个按键模式都是输出模式，上拉输出，默认高电平，低电平有效。先写检查按键函数。首先在KEY.h文件中定义一个枚举类型的KEY，不同的KEY对应不同的值。扫描按键的逻辑就是，先检查现在按下的按键是KEY1还是其他，然后先做一个10ms的延时消抖，在判断那个按键是低电平，如果是低电平，就证明按下了那个按键，然后循环等待按键抬起。返回KEY的值。但是KEY4和KEY5不需要循环等待是否抬起，因为KEY4和KEY5是要做按下之后一直转动。一直按着按键才会一直转动，如果循环等待按键抬起的话，就无法实现按着按键一直转动了。

   ```c
   typedef enum{
       KEY_NONE,
       KEY1,
       KEY2,
       KEY3,
       KEY4,
       KEY5,
   }KEY_TYPE;
   
   
   #define READ_KEY1 HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)
   #define READ_KEY2 HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)
   #define READ_KEY3 HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)
   #define READ_KEY4 HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin)
   #define READ_KEY5 HAL_GPIO_ReadPin(KEY5_GPIO_Port, KEY5_Pin)
   
   /**
    * @brief 扫描按键
    *
    * @return KEY_TYPE
    */
   KEY_TYPE Int_KEY_Scan(void)
   {
   
       if (READ_KEY1 == GPIO_PIN_RESET || READ_KEY2 == GPIO_PIN_RESET || READ_KEY3 == GPIO_PIN_RESET || READ_KEY4 == GPIO_PIN_RESET || READ_KEY5 == GPIO_PIN_RESET)
       {
           HAL_Delay(10); // 延时消抖
   
           // 扫描按键1
           if (READ_KEY1 == GPIO_PIN_RESET)
           {
               // KEY1被按下，需要确认是否抬起
               while (READ_KEY1 == GPIO_PIN_RESET)
               {
                   /* code */
               }
               return KEY1;
   
               /* code */
           }
   
           // 扫描按键2
           if (READ_KEY2 == GPIO_PIN_RESET)
           {
               // KEY2被按下，需要确认是否抬起
               while (READ_KEY2 == GPIO_PIN_RESET)
               {
                   /* code */
               }
               return KEY2;
   
               /* code */
           }
   
           // 扫描按键3
           if (READ_KEY3 == GPIO_PIN_RESET)
           {
               // KEY3被按下，需要确认是否抬起
               while (READ_KEY3 == GPIO_PIN_RESET)
               {
                   /* code */
               }
               return KEY3;
   
               /* code */
           }
   
           // 扫描按键4
           if (READ_KEY4 == GPIO_PIN_RESET)
           {
               // KEY4被按下，需要确认是否抬起
               // while (READ_KEY4 == GPIO_PIN_RESET)
               // {
               //     /* code */
               // }
               return KEY4;
   
               /* code */
           }
   
           // 扫描按键5
           if (READ_KEY5 == GPIO_PIN_RESET)
           {
               // KEY5被按下，需要确认是否抬起
               // while (READ_KEY5 == GPIO_PIN_RESET)
               // {
               //     /* code */
               // }
               return KEY5;
   
               /* code */
           }
   
           /* code */
       }
   
       //没有按键按下
       return KEY_NONE;
   }
   
   ```

   <hr/>

4. 现在已经编写完成了按键检测逻辑，接下来在主函数中循环的判断是那个按键按下，实现具体的按键按下逻辑。

   ```c
   void App_Main(void)
   {
       KEY_TYPE key = KEY_NONE;
   
       printf("App Main Start...!\n");
   
       Com_DWT_Init();
   
       // Initialize the X-axis motor.
       Int_Motor_Init();
   
       HAL_Delay(1000);
   
       while (1)
       {
           key = Int_KEY_Scan();
   
           switch (key)
           {
           case KEY1:
               // KEY1/KEY2/KEY3 keep the original one-shot behavior.
               Int_Motor_Stop_Continuous();
               Int_Motor_Move_To_Homing();
               break;
   
           case KEY2:
               Int_Motor_Stop_Continuous();
               Int_Motor_Move_To_Motor();
               break;
   
           case KEY3:
               Int_Motor_Stop_Continuous();
               Int_Motor_Move_To_Hand();
               break;
   
           case KEY4:
               // Hold KEY4: continuously move toward the handwheel.
               Int_Motor_Move_Continuous(MOTOR_DIR_MOTOR);
   
               break;
   
           case KEY5:
               // Hold KEY5: continuously move toward the motor.
               Int_Motor_Move_Continuous(MOTOR_DIR_HAND);
               break;
   
           case KEY_NONE:
           default:
               // Releasing KEY4/KEY5 stops continuous movement immediately.
               Int_Motor_Stop_Continuous();
               break;
           }
       }
   }
   ```

   

<hr/>

## 二、控制转向

1. 现在要让KEY2和KEY3按键控制电机的转向，首先电机的转向函数就不能写死，定义一个枚举类型。我们自己去判断到底是手轮方向还是电机方向。

   ```c
   /**
    * @brief 设置电机运行方向
    *
    * @param dir 电机运行方向
    */
   
   void __Int_Motor_Set_Dir(Motor_Dir dir)
   {
       if (dir == MOTOR_DIR_HAND)
       {
           // 手轮方向
           HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_RESET);
           /* code */
       }
       else
       {
           // 电机方向
           HAL_GPIO_WritePin(STEPPER_1_DIR_GPIO_Port, STEPPER_1_DIR_Pin, GPIO_PIN_SET);
       }
   
       motor_para.dir = dir;
   }
   ```

   <hr/>

2. 现在我们这个丝杆滑台最多可以运行28圈，每次我们设置的时候，不能超过28圈也不能低于1圈。

   ```c
   /**
    * @brief 设置电机运行圈数
    *
    * @param revolution 运行圈数
    */
   void __Int_Motor_Set_Revolution(double revolution)
   {
       if (revolution > 28)
       {
           revolution = 28;
           /* code */
       }
       if (revolution < 0)
       {
           revolution = 1;
           /* code */
       }
   
       // 计算步数
       motor_para.step.target_step = revolution * STEP_PER_REVOLUTION;
   
       motor_para.revolution = revolution;
   }
   ```

   <hr/>

3. 点击的转速的话还是通过修改ARR的值来控制。

   ```c
   /**
    * @brief 设置电机速度,step/s
    *
    *        TIM2 计数时钟 = 84MHz / (PSC+1) = 84MHz / 84 = 1MHz(每个计数 1us)
    *        脉冲频率 f = 1000000 / (ARR+1)  ==>  ARR = 1000000 / speed - 1
    *        1600 step/s = 1 圈/s(1/8 细分:200 * 8 = 1600)
    *
    * @param speed 每秒脉冲数,step/s
    */
   void __Int_Motor_Set_Speed(double speed)
   {
       // 防止除零
       if (speed == 0)
       {
           return;
       }
   
       uint32_t arr = 1000000.0 / speed; // 一个完整周期占多少个计数(us)
   
       __HAL_TIM_SET_AUTORELOAD(&htim2, arr - 1);             // 设置自动重装载寄存器的值
       __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, arr / 2); // 设置比较寄存器（CCR）的值,50%占空比
   
       // ARR 已开启预装载(ARPE=1),新值要等更新事件才生效。
       // 定时器未运行时不会产生更新事件,这里手动产生一次,让新速度立即写入影子寄存器
       // if ((htim2.Instance->CR1 & TIM_CR1_CEN) == 0)
       // {
       //     htim2.Instance->EGR = TIM_EGR_UG;
       // }
   }
   ```

   <hr/>

4. 设置完这些之后，我们在设计两个转向函数，一个是向手轮方向，一个是向电机方向。这个两个函数内部逻辑时差不多的，不一样的地方就是设置方向。每一个独立的按键操作都不允许被其他按键打断，所以在进行移动的操作前，需要判断一下状态标志位是否是持续移动、手轮/电机、回零运行中。但是考虑到，滑台到达回零位的时候就不能向手轮方向移动了，所以在向手轮方向移动的时候，我们需要额外的判断一下，读取光电开关的PG2位，判断是否是低电平，如果是低电平，证明现在是回零位，不能让滑台向手轮方向移动。

   ```c
   /**
    * @brief 移动---手轮方向
    *
    */
   void Int_Motor_Move_To_Hand(void)
   {
   
       if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) ||
           (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN) ||
           (motor_para.state == MOTOR_STATE_HOMING_RUN))
       {
           return;
           /* code */
       }
   
       if (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_SET)
       {
           // 配置参数
           // 3 RPS
           __Int_Motor_Set_Speed(SPEED_MULTI_4X); // 一秒钟转几圈
           __Int_Motor_Set_Dir(MOTOR_DIR_HAND);   // 方向
           __Int_Motor_Set_Revolution(15);        // 距离
           motor_para.state = MOTOR_STATE_REVOLUTION_RUN;
           // 开始移动
           __Int_Motor_Move_Start();
           /* code */
       }
   }
   
   
   
   
   /**
    * @brief 移动---电机方向
    *
    */
   void Int_Motor_Move_To_Motor(void)
   {
   
       if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) ||
           (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN) ||
           (motor_para.state == MOTOR_STATE_HOMING_RUN))
       {
           return;
           /* code */
       }
   
       // 配置参数
       // 3 RPS
       __Int_Motor_Set_Speed(SPEED_MULTI_4X); // 一秒钟转几圈
       __Int_Motor_Set_Dir(MOTOR_DIR_MOTOR);  // 方向
       __Int_Motor_Set_Revolution(15);        // 距离
       motor_para.state = MOTOR_STATE_REVOLUTION_RUN;
       // 开始移动
       __Int_Motor_Move_Start();
   }
   ```

   <hr/>

5. 接下来我们就去编写一下回零操作，光电开关是一个常开的开关，他默认是高电平，如果我们遮挡光线之后，他的电平就会被拉低。我们可以通过这个性质来进行判断，首先将这个开关引脚配置成EXTI，在ISR中进行回零操作，进行回零操作就是先关闭定时器，将现在的状态置位为回零的状态（MOTOR_STATE_HOMING），然后不管当前的转向是什么，我们都需要转到向电机方向。在回零操作函数中，也是不会打断其他运行状态，之后判断一下，如果现在PG2引脚是高电平，证明没有光遮挡，那我们就进行回零操作，将转向转到手轮方向，开始移动直到遮挡住光电开关停止。

   ```c
   /**
    * @brief 当光电开关触发（下降沿）时，会触发当前的回调函数
    *
    * @param GPIO_Pin
    * @return __weak
    */
   void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
   {
   
       if (GPIO_Pin == X_ZERO_Pin)
       {
           // 进行回零操作
           __Int_Motor_Move_Stop();
           motor_para.state = MOTOR_STATE_HOMING;
           __Int_Motor_Set_Dir(MOTOR_DIR_MOTOR); // 换成电机方向
           /* code */
       }
   }
   ```

   ```c
   /**
    * @brief 移动---回零方向
    *
    */
   void Int_Motor_Move_To_Homing(void)
   {
   
       if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) ||
           (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN) ||
           (motor_para.state == MOTOR_STATE_HOMING_RUN))
       {
           return;
           /* code */
       }
   
       if (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_SET)
       {
           __Int_Motor_Set_Speed(SPEED_MULTI_4X); // 一秒钟转几圈
           __Int_Motor_Set_Dir(MOTOR_DIR_HAND);   // 方向
   
           motor_para.state = MOTOR_STATE_HOMING_RUN;
           // Start PWM pulse output; configuring speed/direction alone does not move the motor.
           __Int_Motor_Move_Start();
   
       }
   }
   ```

   <hr/>

6. 回零操作做完之后，接下来我们就要去做KEY4和KEY5按键一直按下实现一直转动的效果。编写持续移动代码的时候，我们首先需要判断一下当前的方向是否是电机或者手轮方向。然后判断一下当前的运行状态，如果是持续运行状态，并且方向跟现在的方向一致，那就什么也不做，让移动一直保持下去。如果方向相反，那么我们就先停止移动，在最后转换一下方向即可。回零状态和圈数运行状态不能被打断的，也就是相当于这两个状态的优先级比持续运行状态的优先级高，再判断一下当前的转向是不是在手轮方向并且当前是否已经到达了回零位，如果都满足的话，将状态置位回零状态，就不会再继续向手轮方向移动了。接下来就是设置一下转速和方向，并且不设置目标步数，不设置目标就不会进脉冲的回调函数，也就不会停止运行，最后将状态设置为持续运行状态。

   ```c
   /**
    * @brief 持续移动
    *
    * @param dir 移动方向
    */
   void Int_Motor_Move_Continuous(Motor_Dir dir)
   {
       // 如果不是向电机方向和手轮方向，就拒绝无效方向值
       if ((dir != MOTOR_DIR_HAND) && (dir != MOTOR_DIR_MOTOR))
       {
           return;
       }
   
       //如果当前的状态是持续运行状态，并且和现在移动的方向一致，就什么也不做，如果方向不一致，就先停止现在的移动，等到后面换了方向再去移动
       if (motor_para.state == MOTOR_STATE_CONTINUOUS_RUN)
       {
           if (motor_para.dir == dir)
           {
               return;
           }
   
           __Int_Motor_Move_Stop();
           motor_para.state = MOTOR_STATE_STOPPED;
       }
   
       // 如果当前是圈数移动或者是回零状态的话，当前的持续移动是不会去打断的
       if ((motor_para.state == MOTOR_STATE_REVOLUTION_RUN) ||
           (motor_para.state == MOTOR_STATE_HOMING_RUN))
       {
           return;
       }
   
       // 判断一下现在的转向是向手轮方向并且已经遮挡到了光电开关，那么就改变现在的状态并且退出，不再向手轮方向移动
       if ((dir == MOTOR_DIR_HAND) &&
           (HAL_GPIO_ReadPin(X_ZERO_GPIO_Port, X_ZERO_Pin) == GPIO_PIN_RESET))
       {
           motor_para.state = MOTOR_STATE_HOMING;
           return;
       }
   
       __Int_Motor_Set_Speed(SPEED_MULTI_4X);
       __Int_Motor_Set_Dir(dir);
   
       // 连续模式下是不需要设置目标步数的，重置当前的步数
       motor_para.step.current_step = 0;
   
       //设置当前的状态
       motor_para.state = MOTOR_STATE_CONTINUOUS_RUN;
   
       //开始移动
       __Int_Motor_Move_Start();
   }
   ```

   <hr/>

7. 其实到这里已经可以了，但是我们在设计一个停止持续运行函数，在KEY1、KEY2、KEY3按键调用自己的逻辑前，先进行停止操作。将当前步数清0，停止持续移动。

   ```c
   /**
    * @brief 停止持续移动
    */
   void Int_Motor_Stop_Continuous(void)
   {
       // Only stop continuous mode; do not stop fixed-distance or homing moves.
       if (motor_para.state != MOTOR_STATE_CONTINUOUS_RUN)
       {
           return;
       }
   
       __Int_Motor_Move_Stop();
   
       motor_para.step.current_step = 0;
       motor_para.state = MOTOR_STATE_STOPPED;
   }
   ```

<hr/>

# Day 04 

## 一、梯形加减速

1. 步进电机最重要的就是怎么控制速度，像我们之前没有一直没有处理到上电启动的时候，只是考虑先让电机转动起来，电机是不可以瞬间达到我们设定的很大的速度的，电机刚开始启动的时候，它有一个突跳频率（启动的时候电机的最大速度，超过这个值电机就转不起来），这个值大概在几百左右，然后我们一上电就给电机很大的速度，电机是无法承受的。

   <hr/>

2. 所以现在我们就需要通过梯形加减速这个算法来计算ARR的值，假如我们的距离是要跑10圈，也就是16000步，这个就是我们最终要达到的终点值。我们需要把这段距离拆分成三个阶段，一个是加速阶段，一个是匀速阶段，一个是减速阶段，在初始的时候，电机有一个初始速度，我们按一步加一点速度，知道完成加速阶段，他就是达到了我们设定的最大速度，电机就会以这个最大速度一直保持转动，最后快到终点的时候，跟加速阶段所走的距离一样，做减速运动。通常减速度是加速度的2倍。刚上电的时候，磁场跑在转子前面，拉着转子跑，转子跟不上磁场的速度，它就会丢步，所以需要温柔一点。减速时，磁场慢下来，挡在前面转子只会被拽住不会丢步，可以猛一点。

   ![1785496291908](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785496291908.png)

   

   <hr/>

3. 接下来我们还需要考虑到一种特殊情况，就是当目标步数<加速度+减速度步数时，中间就会没有匀速阶段，所以我们需要重新计算一下最大速度。![1785496512323](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785496512323.png)

   <hr/>

```c
   
   /**
    * 设置电机运行圈数
   */
   void __Int_Motor_Set_Revolution(double rev) {
     if (rev > 28) {
       rev = 28;
     }
     if ( rev <= 0 ) {
       rev = 1;
     }
   
     // 计算步数(距离)
     motor_attr.step.target = STEP_PER_REVOLUTION * rev;
     motor_attr.revolution = rev;
   
     double Vmax = motor_attr.speed.target;
     double Vinit = SPEED_BASE * SPEED_MULTI_2X;
   
     motor_attr.speed.min = Vinit; // 1600 最低速度
     motor_attr.speed.current = Vinit; // 1600 初始速度
     motor_attr.speed.acc = Vinit; // 1600 加速度
     motor_attr.speed.dec = Vinit * 2; // 3200 减速度
   
     // !计算加速步数
     motor_attr.step.acc_step = ( Vmax * Vmax - Vinit * Vinit ) / ( 2 * motor_attr.speed.acc );
     // !计算减速步数
     motor_attr.step.dec_step = ( Vmax * Vmax - Vinit * Vinit ) / ( 2 * motor_attr.speed.dec );
     // !计算匀速步数
     if ( (motor_attr.step.acc_step + motor_attr.step.dec_step) > motor_attr.step.target ) {
       // 匀速距离为0，需要使用三角形速度变化公式
       double tmp1 = motor_attr.step.target * 2 * motor_attr.speed.acc * motor_attr.speed.dec;
       double tmp2 = motor_attr.speed.acc + motor_attr.speed.dec;
       double tmp3 = Vinit * Vinit;
   
       double Vnew_max2 = ( tmp1 / tmp2 ) + tmp3;
   
       motor_attr.step.acc_step = ( Vnew_max2 - Vinit * Vinit ) / ( 2 * motor_attr.speed.acc );
       motor_attr.step.dec_step = motor_attr.step.target - motor_attr.step.acc_step;
       motor_attr.step.const_step = 0;
     } else {
       motor_attr.step.const_step = motor_attr.step.target - (motor_attr.step.acc_step + motor_attr.step.dec_step);
     }
   }
```

<hr/>

   ```c
   /**
    * 当PWM方波高电平（有效电平）发送完毕后，会调用当前回调函数
    * 但是PWM方波整个周期信号并没有发送完毕，还要发送无效电平信号
    */
   void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
   {
     if ( motor_attr.state == MOTOR_STATE_MOVE_4_REVOLUTION ) {
       motor_attr.step.current++;
       if (motor_attr.step.current == motor_attr.step.target) {
         motor_attr.step.current = 0;
         __Int_Motor_Move_Stop();
         motor_attr.state = MOTOR_STATE_STOP;
       } else {
         if ( motor_attr.step.current < motor_attr.step.acc_step ) {
           // 加速阶段 : v2 = v1 + at;
           motor_attr.speed.current += ( motor_attr.speed.acc / motor_attr.speed.current );
           if ( motor_attr.speed.current > motor_attr.speed.target ) {
             motor_attr.speed.current = motor_attr.speed.target;
           }
           __Int_Motor_Set_Speed(motor_attr.speed.current);
         } else {
           if ( motor_attr.step.const_step == 0 ) {
             // 减速阶段:v2 = v1 - dt;
             motor_attr.speed.current -= ( motor_attr.speed.dec / motor_attr.speed.current );
             if ( motor_attr.speed.current < motor_attr.speed.min ) {
               motor_attr.speed.current = motor_attr.speed.min;
             }
             __Int_Motor_Set_Speed(motor_attr.speed.current);
           } else {
             if ( motor_attr.step.current >= ( motor_attr.step.acc_step + motor_attr.step.const_step ) ) {
               // 减速阶段
               motor_attr.speed.current -= ( motor_attr.speed.dec / motor_attr.speed.current );
               if ( motor_attr.speed.current < motor_attr.speed.min ) {
                 motor_attr.speed.current = motor_attr.speed.min;
               }
               __Int_Motor_Set_Speed(motor_attr.speed.current);
             }
           }
         }
       }
     }
   }
   ```

   <hr/>
<hr/>

## 二、编码器

1. 我们在前面已经学到了梯形加减速算法，让电机平稳运行和停止，但现在的问题电机的旋转依然会有误差，因为我们现在做的是开环的，所以这个误差我们是感知不到的，这个时候我们就需要用到编码器通过调PID来控制这个误差以达到精确。

   <hr/>

2. 编码器的作用就是把电机的实际运动转换成电信号，让控制器知道轴转了多少，转得多块、方向是否正确。

   * 编码器可以测量位置：通过累计脉冲数量计算电机角度或滑台位置。
   * 测量速度：统计固定时间内增加的脉冲数，计算转速。
   * 方向：根据A、B两相信号先后判断正转和反转。
   * 零点：Z相信号，没转一圈输出一个零点脉冲，可用于位置校准。

   <hr/>

3. 编码器有A、B、Z三相，A相和B相各有两个电平信号，总共四个信号，一个信号90度，四个信号就是360度。四个信号发完之后，Z相计数值加1。



<hr/>

# Day 05

## 一、调节编码器

1. 昨天老师介绍了一下编码器的工作原理，通过编码器让我们缩小误差，我们发出来去1000个pwm脉冲信号，但是电机不一定很精确的走1000步。我们需要通过编码的A、B、Z三相来控制这个误差，A相和B相分别都有两个电平信号，显然4个信号显然是不够的，编码器手册告诉我们，A、B、Z的分辨率，三相支持的最大分辨率是1024,这个需要我们自己读写编码器寄存器。注意些寄存器的操作步骤。

   ![1785637994096](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785637994096.png)

   

   <hr/>

2. 在编码器的参数中，将电机转过一圈对应AB相的周期数称为编码器的线数，比如现在我们准备要设置编码器最大线数1024，1024线的编码器就代表着电机转过一圈之后A相和B相都会采集到1024个脉冲。定时器的四倍频，其实就是对A相和B相的每一个边沿进行采样，那么采样数据就是4 *  1024。

   <hr/>

3. 首先我要去初始化编码器，将编码器对应的GPIO使能拉高表示启动编码器，首先我需要通过I2C协议去读写寄存器，设置编码器的分辨率，将I2C对应的GPIO拉高，然后设定分辨率，因为I2C的GPIO和ABZ三相的GPIO冲突，所以我们要先拉高I2C使用I2C修改寄存器之后，重新配置一下编码器的初始化，让编码器进入正常工做模式。

   ```c
   /**
    * 编码器 - 初始化
    */
   void Int_Encoder_Init(void)
   {
   
       // 使能:拉高
       HAL_GPIO_WritePin(ENCODER_1_EN_GPIO_Port, ENCODER_1_EN_Pin, GPIO_PIN_SET);
   
       // 工作模式 (I2C)：拉高;
       HAL_GPIO_WritePin(ENCODER_1_EN_GPIO_Port, ENCODER_1_EN_Pin, GPIO_PIN_SET);
   
       // 设定分辨率
       __Int_Encoder_Set_Resolution(0x3fff); // 1024分辨率
   
       // 将引脚恢复成原始配置方式
       HAL_TIM_Encoder_MspInit(&htim1);
   
       // 工作模式 (ABZ)：拉低
       HAL_GPIO_WritePin(ENCODER_1_MODE_GPIO_Port, ENCODER_1_MODE_Pin, GPIO_PIN_RESET);
   
       HAL_Delay(50);
   }
   ```

   <hr/>

4. 接下来我们要去修改寄存器。

   ```c
   void __Int_Encoder_Set_Resolution(uint16_t resolution)
   {
   
       Dri_I2C_Init();
   
       // 1. 写寄存器
       // 先读取
       uint8_t reg_data = Dri_I2C_ReadReg(ENCODER_I2C_ADDR_READ, ENCODER_ABZ_RES_H);
       uint8_t reso_l = resolution & 0xff;
       uint8_t reso_h = resolution >> 8;
   
       for (uint8_t i = 0; i < 2; i++)
       {
           if (reso_h & (1 << i) == 0)
           {
               reg_data &= ~(1 << i);
               /* code */
           }
           else
           {
               reg_data |= (1 << i);
           }
   
           /* code */
       }
   
       // 将旧的值更新
       Dri_I2C_Start();
       Dri_I2C_WriteAddr(ENCODER_I2C_ADDR_WRITE);
       Dri_I2C_WriteReg(ENCODER_ABZ_RES_L, reso_l);
       Dri_I2C_Stop();
   
       Dri_I2C_Start();
       Dri_I2C_WriteAddr(ENCODER_I2C_ADDR_WRITE);
       Dri_I2C_WriteReg(ENCODER_ABZ_RES_H, reg_data);
       Dri_I2C_Stop();
   
       // 2. 编程密钥
       Dri_I2C_Start();
       Dri_I2C_WriteAddr(ENCODER_I2C_ADDR_WRITE);
       Dri_I2C_WriteReg(0x09, 0xb3);
       Dri_I2C_Stop();
   
       // 3. 编程指令
       Dri_I2C_Start();
       Dri_I2C_WriteAddr(ENCODER_I2C_ADDR_WRITE);
       Dri_I2C_WriteReg(0x0a, 0x05);
       Dri_I2C_Stop();
   
       // 4. 写周期
       HAL_Delay(1000);
   
       // 5. 重新上电
       HAL_GPIO_WritePin(ENCODER_1_EN_GPIO_Port, ENCODER_1_EN_Pin, GPIO_PIN_RESET);
       HAL_Delay(100);
       HAL_GPIO_WritePin(ENCODER_1_EN_GPIO_Port, ENCODER_1_EN_Pin, GPIO_PIN_SET);
       HAL_Delay(100);
   }
   ```

   <hr/>

5. 现在我们要去获取一下编码实际前进了多少步， 与我们想要让电机走的步数做一个比较，看一下误差是否在可控范围之内。如果编码器实际前进的步数与我们想要让电机走的步数差值大于5，那么我们我就将实际走的步数赋值给我们想要让他走的步数。我们在配置一个TIM3，让这个定时器只做计数操作。

   ```c
   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
   {
     if (htim->Instance == TIM3)
     {
       //! 获取编码器的数据
       uint16_t encoder_cnt = __HAL_TIM_GetCounter(&htim1);
       uint16_t encoder_step = encoder_cnt * 1600 / 4096; // 实际前进的距离
       if (encoder_step > motor_attr.step.current)
       {
         if ((encoder_step - motor_attr.step.current) > 5)
         {
           motor_attr.step.current = encoder_step;
           /* code */
         }
   
         /* code */
       }
       else
       {
         if ((motor_attr.step.current - encoder_step) > 5)
         {
           motor_attr.step.current = encoder_step;
           /* code */
         }
         /* code */
       }
     }
   }
   ```

<hr/>

## 二、网关开发板

1. 点击驱动板已经实现了运动控制，接下来我们需要去开发网关板子，网关其实它做到了一个中转的效果，我们现在是通过是通过五个按键来控制的移动。但在正在的电机控制架构中，往往是会有一个网关设备来控制多个机械节点的电机移动，网关的主要功能就是让不能联网的设备可以进行联网和组网，形成所谓的互联网。![1785642872939](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785642872939.png)

   <hr/>

2. 当前我们这款网关开发板所用到的主控芯片是STM32C8T6,可以支持的硬件通信方式有：LORA、TCP、4G、RS485、CAN。

   <hr/>

3. 首先还是在cubeMX中进行进本配置，网关开发板我们需要用到freeRTOS，所以我们不需要让cubeMX生成SVC、pendSV、的代码，上次在学习freeRTOS中我们是将系统滴答定时器给了freeRTOS用，然后把TIM2配置成了系统滴答定时器，当时systick的代码也没有让cubeMX生成。这一次，我们没有将系统滴答定时器给freeRTOS，我们只需要在systick中做一个判断逻辑，开启宏定义即可。

   ```c
      FreeRTOSConfig.h
      /*基础的必须的配置 */
      #define xPortPendSVHandler PendSV_Handler /* 用来处理PendSV中断:系统的任务的切换 */
      #define vPortSVCHandler SVC_Handler  /*  启动第一个任务时需要特权操作 */
      //#define xPortSysTickHandler SysTick_Handler   因为系统时钟  已经处理了 xPortSysTickHandler  所以不再需要替换
      
      #define INCLUDE_xTaskGetSchedulerState 1 /* 用来获取调度状态  */
      
      
      
      stm32f1xx_it.c
      
      /* USER CODE BEGIN Includes */
      #include "FreeRTOS.h"
      #include "task.h"
      
      /* USER CODE END Includes */
      
      /* Private typedef -----------------------------------------------------------*/
      /* USER CODE BEGIN TD */
      extern void xPortSysTickHandler(void);
      /* USER CODE END TD */
      void SysTick_Handler(void)
      {
        /* USER CODE BEGIN SysTick_IRQn 0 */
      
        /* USER CODE END SysTick_IRQn 0 */
        HAL_IncTick();
        /* USER CODE BEGIN SysTick_IRQn 1 */
      
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        {
          xPortSysTickHandler();
        }
       
        /* USER CODE END SysTick_IRQn 1 */
      }
      
   ```

   <hr/>





<hr/>

# Day 06

## 一、CAN介绍

1. 今天我们学习了一种新的通信协议，是基于消息传递协议的车用级总线CAN，最初是给汽车行业使用的，后面被广泛推广到其他行业。

   <hr/>

2. 首先介绍一下CAN总线的物理层，一个是CAN收发器和CAN控制器，CAN控制器一般都是MCU提供的，而CAN收发器则是需要专门芯片提供，我们所使用的CAN收发器是XL1050，控制器和收发器之间通过CAN_TX和CAN_RX两根线相连，而收发器与CAN总线之间使用的CAN_HIGH和CAN_LOW两根线相连。![1785754558105](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785754558105.png)

   CAN节点发送数据时，控制器把要发送的二进制0或1通过TX线发送给CAN收发器，然后CAN收发器将二进制0或1转换成差分信号传输到CAN总线网络。接收信号也是一样的。CAN总线上可以接入多个设备，接法可以分为闭环总线和开环总线，闭环总线两端需要各接一个120Ω的电阻，而开环网络需要各接2.2kΩ的大电阻。

   ![1785754795098](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785754795098.png)

   ![1785754808657](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785754808657.png)

   CAN中的差分信号，当CAN_HIGH=CAN_LOW=2.5V时，他们之间没有电势差，也就没有电流流过，也就是0，表示隐形电平，看不到电流，代表逻辑1。而当CAN_HIGH=3.5V，CAN_LOW=1.5V时，他们的电势差是2V，表示有电流流过，表示显性电平，代表逻辑0。

   <hr/>

3. 接下来是协议层，CAN总线的报文（帧）格式有多种，比如数据帧，也就是我们需要发送的数据。远程帧（遥控帧），这一帧跟数据帧的格式一样，只是里面没有携带数据，他用于请求自己想要接收的ID发送自己想要数据的信号。错误帧、过载帧、帧间隔都是硬件来控制的。我们的标准帧有11位ID，而扩展帧有29位ID。

   <hr/>

4. 我们具体讲一下数据帧，首先数据帧有两种，一种是标准帧（11位），另一种是扩展帧（29位）。扩展帧和标志帧其实是一样的，扩展帧只是在仲裁段的多加了18位标识符ID。![1785756252883](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785756252883.png)

   * SOF起始帧：默认是高电平，我们需要先拉低，因为0是显性，表示要开始发送数据了，但是只有在总线空闲的时候在可以发送。
   * 仲裁段：
     * ID：有11位标识符ID，表明CAN总线设备的名称。
     * RTR：远程发送请求位，区分当前的帧是数据帧还是远程帧。0代表数据帧，1代表远程帧。
   * 控制端：
     * IDE:用来区分是标准帧还是扩展帧。0代表标准帧，1代表数据帧。
     * R0：是保留位。后续可能会用到。
     * DLC：表示数据段有多少字节，虽然他是4位的，可以表示16个字节，但是数据最多可以是8个字节。
   * 数据段：一共8个字节，高位先行。
   * CRC段：15位CRC校验码和一位CRC界定符，1表示帧结束
   * ACK段：ACK确认位和界定符位
   * EOF段：帧结束

   <hr/>

5. 所谓的CAN总线仲裁就是如果两个节点都想发送数据，那么他们两个人就需要battle一下，看谁厉害，失败就下总线，成功则发送数据。规则就是ID越小优先级越高。

   <hr/>

6. 接下来我们还需要讲一下CAN的位时序，确保通讯时序。首先一位被拆分成了四个段，第一段就是SS（位同步段），第二段是PTS（传播时间段），第三段是PBS1(相位缓冲段1)，第四段是PBS2（相位缓冲段2），这里说的是四个时段，但是我们在cubeMX中配置的时候其实是三段，没有PTS段。4段的总时间段就构成了位时间，就是传输一个位所需要的时间。位时间通常被分为若干等长的时间单元，称为时间量化器，也就是tq。

   * SS段：就是固定的1个tq，如果总线上的信号的跳变沿在SS段范围之内，那说明节点和总线是同步的。
   * PTS段：其实就是补偿时间延迟的。
   * PBS1:用来补偿边沿阶段的误差。在重同步阶段有时候会被拉长。在PBS1之后我们就需要采集一下这个电平是高还是低。
   * PBS2：和PBS1功能差不多

   采样点就是PBS1之后PBS2之前。

   数据同步还分为硬同步和再同步

   * 硬同步：当一个节点检查到起始位时，他会执行，保证时间基准与数据帧的时间基准对齐。
   * 再同步：当我们收到CAN总线的信号时，或多或少都会有延迟，而每一位也会有延迟，日积月累下来，后面的位的跳变沿可能就不在SS段上，所以我们需要再同步，将跳变沿拉回到SS段上。

   这些都是由硬件自动完成的。

   ![1785757587798](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785757587798.png)

   约定tq时间其实就是约定波特率，因为CAN总线是单双工，没有时钟线，所以需要配一下波特率保证接收时序同步。![1785757656091](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785757656091.png)

   <hr/>

7. CAN控制器还有三种工作模式，上电复位默认是睡眠模式，这样是为了降低功耗，在正常使用之前，我们需要先配置寄存器，然后再进入到正常模式

   * 睡眠模式
   * 初始化模式
   * 正常模式

   <hr/>

8. CAN控制器的三种测试模式：

   * 静默模式：![1785757808349](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785757808349.png)
   * 环回模式：![1785757817749](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785757817749.png)
   * 静默环回模式：![1785757824387](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785757824387.png)

   在测试阶段我们可以使用静默环回模式测试一下配置是否正常，用于自检，不会影响总线。

   

<hr/>

## 二、功能框图

![1785758513873](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785758513873.png)

1. 这个是CAN的功能框图，发送数据时，我们需要将数据送到3个发送邮箱中，接收的时候也有接收邮箱，两个接收邮箱，但是接收邮箱内部就像一个队列一样遵循FIFO规则

   <hr/>




<hr/>

## 三丶过滤器

1. 再接收数据时，还有一步前置操作就是要配置一下接收过滤器，对接收的报文进行过滤，将自己不需要的报文通过标识符ID进行过滤，而过滤模式又可以分为两种，一种是标识符列表模式，另一种是掩码模式（模糊匹配）。

   * 标识符列表模式：相当于是白名单模式，只要在列表内的ID都是我需要的数据。
   * 掩码模式：相当于需要符合一些标准，比如，我给出一个标准ID10101010，然后设置掩码1111000，表示，高四位必须和我设置的1010相同，然后低四位你可以随便，我不管，但是高四位必须和1010相同。

   每个CAN都提供了14个位宽可变的、可配置的过滤器组（0-13），每个过滤器组由2个32位寄存器，CAN_FxR1和 CAN_FxR2组成。

   **！！！注意：在配置过滤器组的时候必须将每一位都配置，如果不配置的话就会默认给一个垃圾值。过滤器不是"配了才生效"，而是"不配就全拦"。你要是一个过滤器都不配，一帧都收不到。**

   ![1785759447628](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1785759447628.png)

   

```c
/**
 * @brief 初始化CAN
 *
 */
void Int_CAN_Init(void)
{

    // 1.配置设备过滤器
    CAN_FilterTypeDef can_filter_config;
    can_filter_config.FilterActivation = CAN_FILTER_ENABLE;   // 激活过滤器
    can_filter_config.FilterBank = 0;                         // 14个过滤器，0-13
    can_filter_config.FilterFIFOAssignment = CAN_FilterFIFO0; // 分配邮箱0
    can_filter_config.FilterMode = CAN_FILTERMODE_IDLIST;     // 匹配ID列表
    can_filter_config.FilterIdHigh = (1001 << 5);
    can_filter_config.FilterIdLow = 0;
    can_filter_config.FilterMaskIdHigh = 0;
    can_filter_config.FilterMaskIdLow = 0;
    can_filter_config.FilterScale = CAN_FILTERSCALE_32BIT; // ID位宽
    HAL_CAN_ConfigFilter(&hcan, &can_filter_config);
    // CAN通信需要启动
    HAL_CAN_Start(&hcan);
}

//!============================================================================================================================================

/**
 * @brief 发送CAN数据
 *
 * @param data  数据指针
 * @param len   数据长度
 */
void Int_CAN_SendData(uint8_t *data, uint32_t len)
{
    // 发数据前，先检查发送邮箱是否为空

    uint8_t cnt = 0;
    while (1)
    {
        cnt = HAL_CAN_GetTxMailboxesFreeLevel(&hcan);
        if (cnt > 0)
        {
            break;
        }

        /* code */
    }

    CAN_TxHeaderTypeDef can_message_header;
    uint32_t mailbox_num = 0;

    can_message_header.DLC = len;          // 数据长度，最大8字节，最小1字节
    can_message_header.StdId = 1001;       // 标准ID
    can_message_header.IDE = CAN_ID_STD;   // 标准帧
    can_message_header.RTR = CAN_RTR_DATA; // 数据帧

    // 将发送的数据放置到发送邮箱
    HAL_CAN_AddTxMessage(&hcan, &can_message_header, data, &mailbox_num);
    printf("Int_CAN_SendData mailbox_num = %d\n", mailbox_num);

    /* code */
}

//!============================================================================================================================================

/**
 * @brief 接收CAN数据
 *
 * @param data  数据指针
 * @param len   数据长度
 */
void Int_CAN_ReceiveData(CAN_Message *message, uint32_t *len)
{

    // 接收数据前，先检查接收邮箱是否为空

    while (1)
    {
        *len = HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0);
        if (*len > 0)
        {
            break;

            /* code */
        }

        /* code */
    }

    for (uint8_t i = 0; i < *len; i++)
    {
        CAN_RxHeaderTypeDef can_message_header;
        // 将要接收的消息存到接收邮箱队列0中
        HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &can_message_header, message[i].data);
        message[i].stdID = can_message_header.StdId;
        message[i].data_len = can_message_header.DLC;
        /* code */
    }

    /* code */

    /* code */
}

```



<hr/>

# Day 07

## 一、安装MQTT服务器和客户端

1. 昨天我们将CAN通信需要的引脚都配置好了，并且使用环回静默模式完成了自发自收测试。今天我们编写网关向电机发送控制命令让电机转动起来，然后电机返回状态给网关。

   <hr/>

2. ![1786012866913](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786012866913.png)

   第一个**mosquitto**可以将自己本机作为服务器，然后第二个MQTTX是客户端，里面可以订阅邮箱和发布数据。

   

   <hr/>

![1786012955055](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786012955055.png)![1786012969334](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786012969334.png)![1786012976862](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786012976862.png)



<hr/>

## 二、移植W5500模块和MQTT模块

1. 我们的网关板需要联网，网页的控制台才可以通过IP地址将数据发送到网关板，然后网关板内将控制台发送过来的JSON数据解析成一个字符串，通过CAN总线发送给电机板从而控制电机转动。而控制台要想将数据发送过来就需要MQTT消息传递协议来讲数据发送给网关板。

   <hr/>

2. 接下来我们先移植一下这部分代码，在畜牧定位器的时候我们就使用过W5500这款芯片，他内部是使用SPI串行总线进行传输的，所以我们直接移植代码即可。在cubeMX中配置SPI协议，配置PA8为W5500_CS片选引脚，PA9为W5500_RST复位引脚。![1786013294813](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786013294813.png)

   <hr/>

3. 添加MQTT官方文件

   ![1786013334912](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786013334912.png)

   之后就是在W5500模块中配置网关板的IP和端口号。

   ```c
   uint8_t GATEWAY[4] = {192, 168, 48, 1};
   uint8_t SUB[4] = {255, 255, 255, 0};
   uint8_t MAC[6] = {110, 120, 130, 140, 150, 160};
   uint8_t SIP[4] = {192, 168, 48, 211};
   uint8_t SERVERIP[4] = {192, 168, 48, 23};
   
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
   
       getSIPR(read_ip); // 获取源IP地址
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
               int8_t state = socket(W5500_SOCKET_NUM, Sn_MR_TCP, 8888, NULL);
               if (state >= 0)
               {
                   printf("Create Socket Success\n");
                   /* code */
               }
               else
               {
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
               }
               else
               {
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
   ```

   



<hr/>

## 三、网关板功能实现

1. 首先我们先编写网关到电机的逻辑，创建三个任务，一个是网关到电机发送消息，另一个是电机给网关发送消息（网关接收电机传过来的状态），还有一个MQTT消息传递任务，我们需要解析从网页发送给网关板的JSON数据。

   

   ```c
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
       xTaskCreate(
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
   ```

   <hr/>

2. 先写MQTT消息传递任务，先初始化MQTT，然后周期性的去接收网页传送过来的JSON数据。

   ```c
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
   
       // Int_MQTT_SendData((uint8_t *)"daiwei666", 9);
       while (1)
       {
           // 周期性的获取数据
           Int_MQTT_Refresh();
           vTaskDelay(pdMS_TO_TICKS(20));
           // vTaskDelay(1000);
       }
   }
   ```

   在Int_MQTT.h文件中，我们先把MQTT的框架搭建起来，先宏定义一个客户端编号。然后就是初始化MQTT函数，发送数据给网页函数和刷新MQTT函数（不断循环刷新的接收网页发过来的数据）。

   ```c
   #ifndef __INT_MQTT_H__
   #define __INT_MQTT_H__
   
   #include "stm32f1xx_hal.h"
   #include "MQTTClient.h"
   #include "Int_W5500.h"
   #include "socket.h"
   #include "string.h"
   #include "semphr.h"
   
   #define SOCKET_NUM 0
   
   /**
    * @brief 初始化MQTT
    *
    */
   void Int_MQTT_Init(void);
   
   /**
    * @brief 发送MQTT数据
    *
    * @param data 数据指针
    * @param len 数据长度
    */
   void Int_MQTT_SendData(uint8_t *data, uint16_t len);
   
   
   /**
    * @brief 刷新MQTT连接
    *
    */
   void Int_MQTT_Refresh(void);
   
   
   #endif /* __INT_MQTT_H__ */
   ```

   <hr/>

3. 首先编写MQTT初始化函数，首先在全局函数定义一个互斥信号量，目的是为了，在MQTT初始化的时候，不被别人打断，等初始化完成之后，MQTT才可以送数据。创建网络客户端的时候需要发送缓冲区和接收缓冲区，定义客户端结构体，网络初始化结构体，定义一个连接标志位，定义好服务器IP地址和端口号。

   首先先初始化W5500，初始化网络结构体，然后连接服务器IP，创建网络客户端，最后订阅控制台主题，传入回调函数。之后就是发送MQTT数据，首先判断一下是否连接服务器成功并且互斥量不为NULL，不是第一次拿互斥量，目的是为了先连接好服务器，才能继续发送消息。然后获取到互斥量之后开始发送MQTT消息。

   刷新MQTT函数就是周期性的去获取控制台发送过来的数据，释放互斥量，只要获取到控制台发送过来的数据就调回调函数**Int_MQTT_parse_handle**。这个回调函数再**APP_Main**函数中实现。函数的功能就是解析控制台发送过来的JSON数据。

   ```c
   uint8_t sendbuff[1024];
   uint8_t recvbuff[1024];
   
   MQTTClient mqtt_client;
   Network mqtt_network;
   uint8_t mqtt_connected = 0;
   
   uint8_t MQTT_SERVER_IP[4] = {192, 168, 48, 23};
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
   
   //!============================================================================================================================================
   
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
       // printf("mqtt subscribe success\n");
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
       MQTTMessage message = {0};
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
   
       if (!mqtt_connected || xSemaphoreTake(mqtt_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
       {
           return;
           /* code */
       }
   
       // 周期性获取MQTT数据
       MQTTYield(&mqtt_client, 10);
       xSemaphoreGive(mqtt_mutex);
   }
   
   ```

   <hr/>

4. 之后就是主函数App_Main里面的解析JSON数据，获取电机传过来的状态数据将他们封装成JSON数据通过MQTT发送给控制台显示。上面已经创建好三个主要的任务。我们先定义一个**GW_CMD**枚举类型用来接收控制台发送过来的状态，定义一个与控制台上限速度和最大行程一样的宏定义**SPEED_MAX_STEP_S**，初始化一个结构体**Motor_Status**用来解析电机传过来的数据。

   <hr/>

5. 接下来就是解析控制台传过来的JSON数据，之前在MQTT.c文件中只是定义出来这个回调函数，它是一个弱实现函数**Int_MQTT_parse_handle**，主要用在App_Main主函数中解析控制台的JSON数据，首先定义一个JSON结构体，这个是需要给JSON结构分配内存空间的，但是一般JSON占用内存资料比较大，所以需要判断是否给JSON结构体开辟出来空间。前面还有两个前置小工具函数**json_str_eq**和**json_num**，这两个小工具就是判断一下字段是否存在并且是否为字符串，内容是否相等。接下来就是具体解析内容，最后解析完成之后一定要删除JSON结构体。

   ```c
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
   ```

   <hr/>

6. 解析完成JSON数据之后，我们继续编写网关发送给电机的数据任务。创建一个二值信号量，先获取到二值信号量，接下来就是具体发送刚刚解析完成的JSON数据，刚刚用全局变量接收下来，现在将这些数据整合在一个长度为8的数组中，因为CAN一帧可以发送8个字节数据。

   ```c
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
   ```

   <hr/>

7. 将控制台的数据发送给电机之后，接下来我们需要解析电报反馈过来的数据，将解析下来的数据存到一个 结构体中。

   ```c
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
   ```

   然后我们要将这个结构组成JSON发送给控制台。还是先创建一个JSON结构体，*F103堆很紧*，看一下是否分配空间成功，通过这个**cJSON_PrintUnformatted**函数将JSON结构体转换成字符串，然后使用MQTT发送函数发送给控制台。最后一定要删除JSON结构体并且释放字符串内存空间。

   ```c
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
   ```

   <hr/>

8. 接下来我们编写一下接收电机发过来的数据任务。**motor_to_gateway_task**，通过CAN总线的接收函数将电机发送到过来的数据从接收邮箱的队列中取出数据，然后调用上面的解析电机数据函数**parse_status_frame**。

   ```c
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
   
   
   }
   ```

   <hr/>

9. 接下来还有两个钩子函数。

   ```c
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
   ```







# Day 08

## 一、电机控制板

1. 前一天我们编写完成了网关开发板的逻辑，今天完善一下电机控制板的**Int_Motor_Refresh**刷新函数，也就是不断的接收从网关板传过来的数据。通过状态机来判断电机应该怎么转动。

   <hr/>

2. 首先通过CAN的接收函数接收网关板的数据，将网关板的发送过来的8个字节数据，分别拆解出来，分成控制命令、方向、距离和速度。通过状态机来判断控制命令去控制电机转动。然后定义一个长度为8的数组，用来返回电机的状态、速度、位置。

   ```c
   /**
    * @brief 刷新电机状态
    *
    */
   void Int_Motor_Refresh(void)
   {
   
       // 原来只是在state == MOTOR_STATE_INIT时，才需要刷新状态，改成无条件收（停止/回零指令运行中也要能进来）
       CAN_Message message[3] = {0};
       uint32_t n = 0;
       Int_CAN_ReceiveData(message, &n); // 改成非阻塞
   
       for (uint32_t i = 0; i < n; i++)
       {
           uint8_t *data = message[i].data;
           if (message[i].data_len < 8)
           {
               continue;
               /* code */
           }
           uint8_t cmd = data[0];
           uint8_t dir = data[1];
           uint16_t dist_01mm = (uint16_t)data[2] | (uint16_t)data[3] << 8;                                                       // 小端
           uint32_t speed = (uint32_t)data[4] | ((uint32_t)data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24); // 小端
   
           switch (cmd)
           {
           case 2:
   
               // 停止
               // 持续运动中：不要直接切脉冲，改成进入减速段
               // 让PWM回调里那段现成的减速代码把速度平滑降到min再停
               if (motor_para.state == MOTOR_STATE_POINT_RUN)
               {
                   motor_para.speed_phase = MOTOR_SPEED_DEC;
                   /* code */
               }
               else
               {
                   Int_Motor_Move_Stop(); // 其他状态（圈数/回零）保持原来的立即停
               }
   
               s_remote_speed = 0.0; // ★ 新增：清掉，KEY 操作回到默认 6X
               break;
           case 3:
               Int_Motor_Move_To_Homing();
               break;
           case 1: // 持续移动指令
               if (speed == 0)
               {
                   break;
                   /* code */
               }
               // motor_para.speed.target_speed = speed;
               s_remote_speed = (double)speed;
               dir ? Int_Motor_Move_To_Motor_Point() : Int_Motor_Move_To_Hand_Point();
               break;
   
           case 5: // 移动指令
           {
   
               if (dist_01mm == 0 || speed == 0)
               {
                   break;
                   /* code */
               }
               if (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_POINT_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN)
               {
                   break; // 运动中不接新定位
               }
               motor_para.speed.target_speed = speed;
               __Int_Motor_Set_Dir(dir ? MOTOR_DIR_MOTOR : MOTOR_DIR_HAND);
               __Int_Motor_Set_Revolution(dist_01mm / 80.0); // 一圈=8mm=80个0.1mm
               motor_para.step.current = 0;
               motor_para.state = MOTOR_STATE_REVOLUTION_RUN;
               // 开始移动
               __Int_Motor_Move_Start();
               break;
               /* code */
           }
   
           default:
               break;
           }
   
           /* code */
       }
   
       uint8_t data[8] = {0};
       // byte0:是否运行中
       uint8_t running = (motor_para.state == MOTOR_STATE_REVOLUTION_RUN || motor_para.state == MOTOR_STATE_HOMING_RUN || motor_para.state == MOTOR_STATE_POINT_RUN) ? 1 : 0;
       // byte1：回零阶段。★ 12工程没有细分阶段(无 homing_phase 字段)，
       //        只能用 state 粗映射：1=回零中 / 6=已在零点 / 0=其他
       uint8_t phase = 0;
       if (motor_para.state == MOTOR_STATE_HOMING_RUN)
       {
           phase = 1; // 网页显示"快速接近"
           /* code */
       }
       else if (motor_para.state == MOTOR_STATE_HOMING)
       {
   
           phase = 6; // 网页显示"已在零点"
       }
   
       // byte2~3：当前速度。current_speed 是 double，转窄类型前必须钳位
       double spd_d = motor_para.speed.current_speed;
       if (spd_d < 0.0)
       {
           spd_d = 0.0;
   
           /* code */
       }
       if (spd_d > 65535.0)
       {
           spd_d = 65535.0;
           /* code */
       }
       uint16_t sp = (uint16_t)spd_d;
   
       // byte4~7：当前位置。step.current 本身是 uint16_t，转 int32 安全
       int32_t pos = (int32_t)motor_para.step.current;
       data[0] = running;
       data[1] = phase;
       data[2] = (uint8_t)sp;
       data[3] = (uint8_t)(sp >> 8);
       data[4] = (uint8_t)pos;
       data[5] = (uint8_t)(pos >> 8);
       data[6] = (uint8_t)(pos >> 16);
       data[7] = (uint8_t)(pos >> 24);
   
       Int_CAN_SendData(data, 8);
   }
   ```

   



<hr/>

## 二、CANopen

1. 今天下午老师介绍了一下扩展知识点**CANopen**，**CANopen**其实是一种架构在CAN上的高层通讯规定，包括通讯子协定及设备子协定。它是工业控制领域常用的一种总线协议，简单说就是CAN通讯明确了设备之间数据传输的结构：先发什么后发什么，以及哪些必要的部分：![1786023956299](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786023956299.png)

   而这里的数据内容却没有明确的规定，为了便于不同厂家的设备可以彼此适配，那么就必须明确发送数据内容的规则，比如说0x10表示胎压数据，0x20表示电机温度数据。只要CAN总线上的其他设备都接受这种约定，就可以正确的解析这个数据。但是不用厂家都有自己一套规定，为此工业领域指定了统一的标准协议。**CANopen**就是一种标准协议，它规定了![1786024144660](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024144660.png)

   通过**CANopen**，不同厂家的设备也可以在同一条CAN网络中实现互联互通。

   ![1786024387453](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024387453.png)



<hr/>

## 三、架构流程图



1. ![1786024556057](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024556057.png)

   <hr/>

2. ![1786024584867](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024584867.png)

   <hr/>

3. ![1786024477677](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024477677.png)

<hr/>

4. ![1786024493199](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024493199.png)

   <hr/>

5. ![1786024524363](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024524363.png)

   <hr/>

6. ![1786024676005](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024676005.png)

   <hr/>

7. ![1786024701675](C:\Users\16156\AppData\Roaming\Typora\typora-user-images\1786024701675.png)

   
