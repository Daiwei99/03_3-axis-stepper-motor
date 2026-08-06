# 三轴步进电机 MQTT 网页控制台

## 1. 功能

- 浏览器通过 MQTT over WebSocket 连接 Mosquitto 或其他 MQTT Broker。
- X 轴“向手轮 / 向电机”支持按住持续运动，松开自动发送 `stop`。
- 支持速度设置，当前按 1/8 细分换算：1600 step/s ≈ 1 转/s ≈ 60 RPM。
- 订阅网关发布的电机状态，显示 `motor_status` 和 `cur_angle`。
- 页面不直接连接 CAN/Modbus；网关负责 MQTT 到 CAN/Modbus 的协议转换。

## 2. 运行

1. 启动 MQTT Broker，并开启 WebSocket 监听。例如 Mosquitto 配置：

```conf
listener 1883
protocol mqtt
listener 8083
protocol websockets
allow_anonymous true
```

2. 用浏览器打开 `index.html`。推荐在本目录启动本地静态服务器：

```powershell
python -m http.server 8000
```

然后访问 `http://127.0.0.1:8000/`。

3. 地址填写：`ws://127.0.0.1:8083/mqtt`。如果 Broker 在另一台电脑，把 `127.0.0.1` 改成 Broker 的局域网 IP。

## 3. MQTT 主题

- 命令：`motor/command`
- 状态：`motor/status`

可以按设备拆分成：`motor/{device_id}/command`、`motor/{device_id}/status`；只需在页面中修改主题。

## 4. 命令示例

开始连续运行：

```json
{
  "device_id": 1,
  "command_id": 1,
  "axis": "x",
  "action": "start",
  "mode": "continuous",
  "direction": "motor",
  "speed_step_s": 1600,
  "speed_rpm": 60,
  "target_angle": 360000,
  "max_speed": 60,
  "timestamp": 1760000000000
}
```

停止：

```json
{
  "device_id": 1,
  "command_id": 2,
  "axis": "x",
  "action": "stop",
  "mode": "continuous",
  "direction": "stop",
  "speed_step_s": 0,
  "speed_rpm": 0,
  "target_angle": 0,
  "max_speed": 0,
  "timestamp": 1760000001000
}
```

保活：页面运行期间每隔一段时间发送 `heartbeat`。网关可以设置看门狗：超过 2~3 个周期没有收到 `heartbeat`，自动停止电机，防止网页断网后电机继续转动。

## 5. 与当前课程网关代码的兼容说明

课程示例的 MQTT 回调只解析：

```c
{"target_angle": ..., "max_speed": ..., "device_id": ...}
```

网页保留了这三个字段，但要实现真正的“按住运行、松开停止”，网关端 MQTT 回调还需要解析 `action`、`direction` 和 `speed_step_s`：

- `action=start`：向电机板发送启动/速度/方向指令。
- `action=heartbeat`：刷新看门狗，不重复启动 PWM。
- `action=stop`：向电机板发送停止指令。
- `direction=hand`：映射为电机板的 `MOTOR_DIR_HAND`。
- `direction=motor`：映射为电机板的 `MOTOR_DIR_MOTOR`。

CAN 可以继续使用课程中的标准 ID：网关→电机板 `0x0001`，电机板→网关 `0x0002`。建议 CAN 数据使用明确的整数协议，而不是直接发送 float：

```text
byte0: 命令（1=start，2=stop，3=homing，4=status）
byte1: 方向（0=hand，1=motor）
byte2~3: 保留
byte4~7: 速度 step/s（uint32，小端）
```

如果选择 Modbus RTU，则网关作为主站，电机板作为从站，可把 `command/action/direction/speed` 映射为保持寄存器；网页端协议无需变化。

## 6. 安全建议

正式使用不要开启匿名 MQTT。网页和 Broker 使用 WSS，使用用户名/密码或短期 token；网关必须实现急停、限位、掉线看门狗和速度上限。连续运动时，停止消息不能只依赖网页的 `pointerup`，网关也必须在 MQTT 连接异常或心跳超时后主动停止。

