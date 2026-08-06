/* MQTT WebSocket motor-control client. Works with Mosquitto + mqtt.js 5. */
(() => {
  'use strict';
  const $ = (id) => document.getElementById(id);
  let client = null;
  let activeDirection = null;
  let heartbeatTimer = null;
  let commandSeq = 0;
  let lastPayload = null;

  const log = (message, type = '') => {
    const box = $('log');
    const empty = box.querySelector('.log-empty');
    if (empty) empty.remove();
    const line = document.createElement('div');
    line.className = `log-line ${type}`;
    const now = new Date().toLocaleTimeString();
    const time = document.createElement('span');
    time.className = 'time';
    time.textContent = now;
    line.append(time, document.createTextNode(message));
    box.appendChild(line);
    box.scrollTop = box.scrollHeight;
  };

  const setConnected = (connected, text) => {
    const badge = $('connectionBadge');
    badge.classList.toggle('online', connected);
    badge.classList.toggle('offline', !connected);
    badge.querySelector('span:last-child').textContent = text;
    $('connectBtn').disabled = connected;
    $('disconnectBtn').disabled = !connected;
  };

  const getConfig = () => ({
    url: $('brokerUrl').value.trim() || 'ws://127.0.0.1:8083/mqtt',
    username: $('mqttUsername').value,
    password: $('mqttPassword').value,
    commandTopic: $('commandTopic').value.trim() || 'console_to_gateway',
    statusTopic: $('statusTopic').value.trim() || 'gateway_to_console',
    deviceId: Number($('deviceId').value) || 1,
    speed: Number($('speedRange').value) || 1600,
    targetDistance: Number($('targetDistance').value) || 0
  });

  const rpm = (speed) => speed / 1600 * 60;
  const updateSpeed = () => {
    const speed = Number($('speedRange').value);
    $('speedOutput').textContent = `${speed} step/s · ${rpm(speed).toFixed(1)} RPM`;
  };

  // 统一指令构造。mode: continuous(按住持续) / position(走N毫米) / homing(回零)
  const makePayload = (action, direction = activeDirection, mode = 'continuous') => {
    const c = getConfig();
    const stopped = action === 'stop';
    const speed = stopped ? 0 : c.speed;
    const distance = mode === 'position' && !stopped ? c.targetDistance : 0;
    const payload = {
      device_id: c.deviceId,
      command_id: ++commandSeq,
      axis: 'x',
      action,                                   // start / stop / heartbeat / homing
      mode,                                     // continuous / position / homing
      direction: direction || 'stop',           // hand / motor / stop
      target_distance: distance,                // mm，定位模式才非 0
      speed_step_s: speed,                      // step/s，固件的原生单位
      speed_rpm: stopped ? 0 : Number(rpm(speed).toFixed(2)),
      timestamp: Date.now()
    };
    // 课程示例网关只认 max_speed / motor_status，勾上兼容就一起发，网关两种都能解析。
    if ($('compatMode').checked) {
      payload.max_speed = speed;
      payload.motor_status = stopped ? 'off' : 'on';
    }
    return payload;
  };

  const renderPayload = (payload) => {
    lastPayload = payload;
    $('payloadPreview').textContent = JSON.stringify(payload, null, 2);
  };

  const publish = (payload) => {
    renderPayload(payload);
    if (!client || !client.connected) {
      log('未连接 MQTT：指令只显示在预览区，没有发送。', 'err');
      return false;
    }
    const topic = getConfig().commandTopic;
    client.publish(topic, JSON.stringify(payload), { qos: 1 }, (err) => {
      if (err) log(`发送失败：${err.message}`, 'err');
      else log(`已发布 ${topic}：${payload.action}`, 'ok');
    });
    return true;
  };

  const startContinuous = (direction) => {
    if (activeDirection === direction) return;
    if (activeDirection) stopContinuous();
    activeDirection = direction;
    document.querySelectorAll('.hold-button').forEach((b) => b.classList.toggle('active', b.dataset.direction === direction));
    publish(makePayload('start', direction));
    const interval = Math.max(200, Number($('heartbeatMs').value) || 1000);
    heartbeatTimer = window.setInterval(() => publish(makePayload('heartbeat', direction)), interval);
    $('motorStatus').textContent = direction === 'hand' ? '向手轮运行' : '向电机运行';
  };

  const stopContinuous = () => {
    if (!activeDirection) return;
    if (heartbeatTimer) window.clearInterval(heartbeatTimer);
    heartbeatTimer = null;
    publish(makePayload('stop', activeDirection));
    activeDirection = null;
    document.querySelectorAll('.hold-button').forEach((b) => b.classList.remove('active'));
    $('motorStatus').textContent = '停止';
  };

  // 定位方向：分段开关，两个按钮互斥。默认向手轮（那端有零点开关兜底；
  // 向电机那端只有硬限位且不进 MCU，撞上会闭锁并丢位置）。
  let positionDir = 'hand';

  // 定位模式：走固定距离，发一次就够，固件走完自动停，不需要心跳。
  const startPosition = () => {
    const distance = getConfig().targetDistance;
    if (!distance) return log('前进距离为 0，先填一个距离再启动定位。', 'err');
    if (activeDirection) stopContinuous();   // 先收掉持续运动，避免两种模式打架
    publish(makePayload('start', positionDir, 'position'));
    $('motorStatus').textContent = `定位中 ${distance}mm（${DIRECTION_TEXT[positionDir] || positionDir}）`;
  };

  // 回零：对应固件的三段式 Homing，网关下发 CAN byte0=3。
  const startHoming = () => {
    if (activeDirection) stopContinuous();
    publish(makePayload('homing', 'hand', 'homing'));
    $('motorStatus').textContent = '回零中';
    $('homedStatus').textContent = '回零中';
  };

  // 解析网关回传的状态 JSON，逐字段更新面板。字段缺失就保持原值。
  const DIRECTION_TEXT = { hand: '向手轮', motor: '向电机', stop: '--' };
  const HOME_PHASE = ['未回零', '快速接近', '已触发', '反向退出', '退出完成', '低速接近', '回零完成', '回零错误'];
  const applyStatus = (data) => {
    if (!data || typeof data !== 'object') return;

    const running = data.motor_status === 'on' || data.motor_status === 1 || data.motor_status === true;
    if (data.motor_status !== undefined) {
      $('motorStatus').textContent = running ? '运行中' : '停止';
      $('motorStatus').classList.toggle('on', running);
      $('motorStatus').parentElement.classList.toggle('live', running);
      if (!running) {
        $('motorDir').textContent = '--';
        $('curSpeed').textContent = '0 step/s';
      }
    }
    if (data.direction !== undefined) {
      $('motorDir').textContent = DIRECTION_TEXT[data.direction] || data.direction;
    }
    if (data.cur_speed_step_s !== undefined) {
      $('curSpeed').textContent = `${data.cur_speed_step_s} step/s`;
    }
    if (data.cur_angle !== undefined) {
      $('currentAngle').textContent = `${Number(data.cur_angle).toFixed(1)} °`;
    }
    // 位置：优先用自己的 cur_position_mm，也认老师控制台/课程示例的 cur_distance
    const posMm = data.cur_position_mm !== undefined ? data.cur_position_mm : data.cur_distance;
    if (posMm !== undefined) {
      $('curPosition').textContent = `${Number(posMm).toFixed(2)} mm`;
    }
    if (data.homed !== undefined) {
      const homed = data.homed === true || data.homed === 1;
      $('homedStatus').textContent = homed ? '已回零' : '未回零';
      $('homedStatus').classList.toggle('on', homed);
    }
    // 固件的回零阶段号 0~7 直接映射成中文，排障比看数字直观
    if (data.home_phase !== undefined) {
      const phase = Number(data.home_phase);
      $('homedStatus').textContent = HOME_PHASE[phase] || `阶段 ${phase}`;
      $('homedStatus').classList.toggle('on', phase === 6);
      $('homedStatus').classList.toggle('err', phase === 7);
    }
    $('lastMessage').textContent = new Date().toLocaleTimeString();
  };

  $('connectBtn').addEventListener('click', () => {
    if (!getConfig().url) return log('请输入 Broker WebSocket 地址。', 'err');
    if (client) client.end(true);
    const c = getConfig();
    log(`正在连接 ${c.url} ...`);
    client = new SimpleMqttClient(c.url, {
      username: c.username || undefined,
      password: c.password || undefined,
      clientId: `motor-web-${c.deviceId}-${Math.random().toString(16).slice(2)}`,
      clean: true,
      reconnectPeriod: 2000,
      connectTimeout: 8000
    });
    client.on('connect', () => {
      setConnected(true, 'MQTT 已连接');
      log('MQTT 连接成功。', 'ok');
      client.subscribe(c.statusTopic, { qos: 1 }, (err) => {
        if (err) log(`订阅状态主题失败：${err.message}`, 'err');
        else log(`已订阅 ${c.statusTopic}`, 'ok');
      });
    });
    client.on('reconnect', () => { setConnected(false, '正在重连'); log('MQTT 正在重连...'); });
    client.on('close', () => { setConnected(false, '未连接'); });
    client.on('error', (err) => log(`MQTT 错误：${err.message}`, 'err'));
    client.on('message', (topic, raw) => {
      const text = new TextDecoder().decode(raw);
      log(`收到 ${topic}：${text}`, 'ok');
      try {
        applyStatus(JSON.parse(text));
      } catch (_) { /* keep raw log for non-JSON status */ }
    });
    client.connect();

  });

  $('disconnectBtn').addEventListener('click', () => {
    stopContinuous();
    if (client) client.end(true);
    client = null;
    setConnected(false, '未连接');
    log('已断开 MQTT。');
  });

  document.querySelectorAll('.hold-button').forEach((button) => {
    const down = (event) => { event.preventDefault(); button.setPointerCapture?.(event.pointerId); startContinuous(button.dataset.direction); };
    const up = (event) => { event.preventDefault(); if (activeDirection === button.dataset.direction) stopContinuous(); };
    button.addEventListener('pointerdown', down);
    button.addEventListener('pointerup', up);
    button.addEventListener('pointercancel', up);
    button.addEventListener('pointerleave', (event) => { if (event.buttons === 0 && activeDirection === button.dataset.direction) stopContinuous(); });
  });
  $('stopBtn').addEventListener('click', () => {
    if (activeDirection) stopContinuous();
    else publish(makePayload('stop', 'stop'));
    $('motorStatus').textContent = '停止';
  });
  $('startPositionBtn').addEventListener('click', startPosition);
  $('homingBtn').addEventListener('click', startHoming);
  // 定位方向分段开关：点哪个哪个高亮，只改变量不发指令
  document.querySelectorAll('.seg-btn[data-posdir]').forEach((button) => {
    button.addEventListener('click', () => {
      positionDir = button.dataset.posdir;
      document.querySelectorAll('.seg-btn[data-posdir]').forEach((b) => {
        const on = b === button;
        b.classList.toggle('active', on);
        b.setAttribute('aria-checked', String(on));
      });
    });
  });
  $('speedRange').addEventListener('input', updateSpeed);
  $('copyPayload').addEventListener('click', async () => {
    if (!lastPayload) return;
    await navigator.clipboard?.writeText(JSON.stringify(lastPayload));
    log('最近发送 JSON 已复制。', 'ok');
  });
  $('clearLog').addEventListener('click', () => { $('log').innerHTML = '<div class="log-empty">暂无日志</div>'; });
  window.addEventListener('blur', stopContinuous);
  document.addEventListener('visibilitychange', () => { if (document.hidden) stopContinuous(); });
  updateSpeed();
})();





