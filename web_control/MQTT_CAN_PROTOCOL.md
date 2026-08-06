# 完整链路协议表（唯一权威版本）

> 网页 --MQTT--> 网关(F103+W5500) --CAN--> 电机板(F407)
>
> ⚠️ 三方字段必须严格照此表，改一个字母就解析成 NULL。
> 课程 docx 里网关代码用的是 `target_angle`，老师控制台发的是 `target_distance`，
> 两者本身对不上 —— **以本表为准，不要照抄 docx**。

## 1. MQTT 主题

| 方向 | 主题 |
|---|---|
| 网页 → 网关 | `daiwei9527/console_to_gateway` |
| 网关 → 网页 | `daiwei9527/gateway_to_console` |

主题名加了前缀防公共 Broker 撞车。本地 mosquitto 可随意。

## 2. 网页 → 网关（MQTT JSON）

```json
{
  "device_id": 1,
  "command_id": 12,
  "axis": "x",
  "action": "start",
  "mode": "continuous",
  "direction": "motor",
  "target_distance": 0,
  "speed_step_s": 1600,
  "speed_rpm": 60,
  "timestamp": 1760000000000,
  "max_speed": 1600,
  "motor_status": "on"
}
```

网关**只需解析 4 个字段**，其余是给人看的/兼容用的：

| 字段 | 取值 | 用途 |
|---|---|---|
| `action` | `start` / `stop` / `heartbeat` / `homing` | 干什么 |
| `mode` | `continuous` / `position` / `homing` | 哪种运动 |
| `direction` | `hand` / `motor` / `stop` | 方向 |
| `speed_step_s` | 200~10000 | 速度，**固件原生单位 step/s，不用换算**（网关 `SPEED_MAX_STEP_S` 必须同步改，否则被砍回旧上限） |
| `target_distance` | 0~235 | 定位模式的距离（mm），其他模式为 0 |

`max_speed` / `motor_status` 是勾了"兼容"才发的，值和上面重复，给课程示例代码用。**自己写网关就忽略它们。**

三种操作对应关系：

| 网页动作 | action | mode | 网关该做什么 |
|---|---|---|---|
| 按住方向键 | `start` | `continuous` | 启动持续运动 |
| 按住期间每 1s | `heartbeat` | `continuous` | **只刷看门狗，不要重新启动 PWM** |
| 松开 / 点停止 | `stop` | `continuous` | 停止 |
| 点"启动定位" | `start` | `position` | 走 N 毫米后自停 |
| 点"回零" | `homing` | `homing` | 触发三段式回零 |

🔴 **看门狗必做**：连续运动靠心跳维持。超过 2~3 个周期（约 3s）没收到 `heartbeat`，网关必须**主动下发停止**。否则网页崩了/网线拔了，电机会一直转到撞限位。不能只依赖网页的松手事件。

## 3. 网关 → 电机板（CAN ID `0x0001`，8 字节）

```
byte0  命令   1=启动连续  2=停止  3=回零  4=请求状态  5=启动定位
byte1  方向   0=hand(向手轮)  1=motor(向电机)
byte2~3 目标距离  uint16 小端，单位 0.1mm    ← 仅 byte0=5 有效
byte4~7 速度      uint32 小端，单位 step/s
```

- 距离用 0.1mm 而不是 mm：留一位小数精度，且 235mm→2350 稳稳落在 uint16 内。
- 电机板换算：`目标步数 = byte2~3 的值 × 20`（因 200 step/mm ÷ 10）。
- 例：走 10.0mm → byte2~3 = 100 → 100×20 = 2000 步。

## 4. 电机板 → 网关（CAN ID `0x0002`，8 字节）

```
byte0  运行状态   0=停止  1=运行中
byte1  回零阶段   0~7，对应固件 Motor_Homing_Phase 枚举
byte2~3 当前速度  uint16 小端，step/s
byte4~7 当前位置  int32  小端，单位"步"（有符号，回零前可能为负）
```

回零阶段号沿用固件枚举，网页已做中文映射：

| 值 | 含义 | 值 | 含义 |
|---|---|---|---|
| 0 | 未回零(IDLE) | 4 | 退出完成 |
| 1 | 快速接近 | 5 | 低速接近 |
| 2 | 已触发 | 6 | **回零完成** |
| 3 | 反向退出 | 7 | **回零错误** |

## 5. 网关 → 网页（MQTT JSON）

```json
{
  "device_id": 1,
  "motor_status": "on",
  "direction": "motor",
  "cur_speed_step_s": 1600,
  "cur_position_mm": 12.34,
  "cur_angle": 360.0,
  "home_phase": 6,
  "homed": true
}
```

| 字段 | 说明 |
|---|---|
| `motor_status` | `"on"` / `"off"`（也接受 1/0、true/false） |
| `direction` | `hand` / `motor` / `stop` |
| `cur_speed_step_s` | 当前速度 step/s |
| `cur_position_mm` | 当前位置 mm = 步数 ÷ 200 |
| `home_phase` | 0~7，网页显示中文阶段名 |
| `homed` | 是否已完成回零 |

网页**也认** `cur_distance`（老师控制台的字段名），任选其一即可。

## 6. 关键换算常量

```c
#define STEPS_PER_MM     200      // 200整步 × 8细分 ÷ 8mm导程
#define STEPS_PER_CIRCLE 1600     // 1圈 = 1600 步 = 8mm
#define MAX_TRAVEL_MM    235      // 满行程，约29圈
```

速度参考：`1600 step/s = 1 圈/s = 8mm/s = 60 RPM`

## 7. 网关端解析骨架

```c
static void mqttRecv(uint8_t *data, uint32_t len)
{
    cJSON *root = cJSON_ParseWithLength((char *)data, len);
    if (root == NULL) { printf("json parse fail\r\n"); return; }

    const cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "action");
    const cJSON *mode   = cJSON_GetObjectItemCaseSensitive(root, "mode");
    const cJSON *dir    = cJSON_GetObjectItemCaseSensitive(root, "direction");
    const cJSON *speed  = cJSON_GetObjectItemCaseSensitive(root, "speed_step_s");
    const cJSON *dist   = cJSON_GetObjectItemCaseSensitive(root, "target_distance");

    // 每个指针都必须判空 + 判类型，网页字段缺失时不能崩
    if (!cJSON_IsString(action)) { cJSON_Delete(root); return; }

    uint8_t can_data[8] = {0};

    if (strcmp(action->valuestring, "stop") == 0) {
        can_data[0] = 2;
    } else if (strcmp(action->valuestring, "homing") == 0) {
        can_data[0] = 3;
    } else if (strcmp(action->valuestring, "heartbeat") == 0) {
        Watchdog_Refresh();          // 只喂狗，不发 CAN
        cJSON_Delete(root);
        return;
    } else if (cJSON_IsString(mode) && strcmp(mode->valuestring, "position") == 0) {
        can_data[0] = 5;
        uint16_t d = cJSON_IsNumber(dist) ? (uint16_t)(dist->valuedouble * 10) : 0;
        memcpy(&can_data[2], &d, 2);
    } else {
        can_data[0] = 1;             // 连续运动
        Watchdog_Refresh();
    }

    can_data[1] = (cJSON_IsString(dir) && strcmp(dir->valuestring, "motor") == 0) ? 1 : 0;

    uint32_t sp = cJSON_IsNumber(speed) ? (uint32_t)speed->valuedouble : 0;
    if (sp > 16000) sp = 16000;      // 速度上限，别让网页填个离谱值把电机怼堵转
    memcpy(&can_data[4], &sp, 4);

    Int_CAN_Send(0x0001, can_data, 8);
    cJSON_Delete(root);              // 每条返回路径都要 Delete，否则内存泄漏
}
```

🔴 三个必守的点：
1. **每个 cJSON 指针都判空判类型** —— 网页少发一个字段就崩是最常见的死法。
2. **每条 return 路径都 `cJSON_Delete(root)`** —— F103 只有 20KB RAM，泄漏几十次就堆耗尽。
3. **速度上限钳位** —— 别信来自网络的任何数值。

## 8. 电机板待补的功能

固件目前（09 工程）有：连续运动、圈数运动、三段式回零。**缺**：

- `byte0=5` 绝对定位（走 N 毫米）—— 主线第⑥步，复用现有梯形加减速轮廓，把目标步数从"圈数×1600"换成"距离×200"即可。
- CAN 接收 + 指令解析（整套都还没有）。
- CAN 上报任务：周期 100ms 发 `0x0002`。
- CAN 指令超时保护：网关也可能掉线。
