/* Minimal MQTT 3.1.1 over WebSocket client for the control console. */
class SimpleMqttClient {
  constructor(url, options = {}) {
    this.url = url;
    this.options = options;
    this.connected = false;
    this.packetId = 1;
    this.handlers = {};
    this.ws = null;
    this.buffer = new Uint8Array(0);
    this.pingTimer = null;
    this.reconnectTimer = null;
    this.closedByUser = false;
  }
  on(name, fn) { (this.handlers[name] ||= []).push(fn); return this; }
  emit(name, ...args) { (this.handlers[name] || []).forEach(fn => fn(...args)); }
  nextPacketId() { this.packetId = this.packetId >= 65535 ? 1 : this.packetId + 1; return this.packetId; }
  connect() {
    this.closedByUser = false;
    // MQTT over WebSocket 必须声明子协议 'mqtt'，否则 EMQX/Mosquitto 会直接关闭连接。
    try { this.ws = new WebSocket(this.url, 'mqtt'); }
    catch (e) { this.emit('error', e); this.scheduleReconnect(); return this; }
    this.ws.binaryType = 'arraybuffer';
    this.ws.onopen = () => this.sendConnect();
    this.ws.onmessage = async e => this.receive(new Uint8Array(e.data));
    this.ws.onerror = () => this.emit('error', new Error('WebSocket 连接失败，请检查 Broker 的 WebSocket 地址和端口'));
    this.ws.onclose = (e) => {
      this.stopPing();
      const wasConnected = this.connected;
      this.connected = false;
      if (!wasConnected && !this.closedByUser) {
        this.emit('error', new Error(`WebSocket 已关闭（code=${e.code}${e.reason ? ' ' + e.reason : ''}）。若 code=1006，多为地址/端口/路径错误或网络不通。`));
      }
      if (wasConnected) this.emit('close');
      if (!this.closedByUser) { this.emit('reconnect'); this.scheduleReconnect(); }
    };
    return this;
  }
  scheduleReconnect() {
    if (this.closedByUser || this.reconnectTimer) return;
    this.reconnectTimer = setTimeout(() => { this.reconnectTimer = null; this.connect(); }, this.options.reconnectPeriod || 2000);
  }
  sendConnect() {
    const o = this.options;
    let flags = 0x02; // clean session
    if (o.username) flags |= 0x80;
    if (o.password) flags |= 0x40;
    const body = concat(
      utf8('MQTT'), Uint8Array.from([4, flags, 0, 60]),
      utf8(o.clientId || `web-${Date.now()}`),
      o.username ? utf8(o.username) : new Uint8Array(0),
      o.password ? utf8(o.password) : new Uint8Array(0)
    );
    this.sendPacket(0x10, body);
  }
  subscribe(topic, options = {}, callback) {
    if (typeof options === 'function') { callback = options; options = {}; }
    const id = this.nextPacketId();
    const qos = options.qos === 1 ? 1 : 0;
    this.sendPacket(0x82, concat(u16(id), utf8(topic), Uint8Array.from([qos])));
    if (callback) setTimeout(() => callback(null), 0);
  }
  publish(topic, payload, options = {}, callback) {
    if (typeof options === 'function') { callback = options; options = {}; }
    const bytes = typeof payload === 'string' ? new TextEncoder().encode(payload) : payload;
    const qos = options.qos === 1 ? 1 : 0;
    const packet = qos ? concat(utf8(topic), u16(this.nextPacketId()), bytes) : concat(utf8(topic), bytes);
    this.sendPacket(qos ? 0x32 : 0x30, packet);
    if (callback) setTimeout(() => callback(null), 0);
  }
  end(force = false) {
    this.closedByUser = true;
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
    this.reconnectTimer = null;
    this.stopPing();
    if (this.ws && this.ws.readyState === WebSocket.OPEN && !force) this.sendPacket(0xe0, new Uint8Array(0));
    if (this.ws) this.ws.close();
    this.connected = false;
  }
  sendPacket(type, body) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
    const remaining = encodeRemainingLength(body.length);
    this.ws.send(concat(Uint8Array.from([type]), remaining, body));
  }
  receive(chunk) {
    this.buffer = concat(this.buffer, chunk);
    while (this.buffer.length >= 2) {
      const decoded = decodeRemainingLength(this.buffer, 1);
      if (!decoded || this.buffer.length < 1 + decoded.bytes + decoded.value) return;
      const type = this.buffer[0] >> 4;
      const start = 1 + decoded.bytes;
      const body = this.buffer.slice(start, start + decoded.value);
      this.buffer = this.buffer.slice(start + decoded.value);
      if (type === 2) {
        if (body[0] !== 0) { this.emit('error', new Error(`MQTT CONNACK 失败：${body[1]}`)); return; }
        this.connected = true; this.emit('connect'); this.startPing();
      } else if (type === 3) {
        const topicLen = (body[0] << 8) | body[1];
        const topic = new TextDecoder().decode(body.slice(2, 2 + topicLen));
        this.emit('message', topic, body.slice(2 + topicLen));
      } else if (type === 13) { /* PINGRESP */ }
    }
  }
  startPing() { this.stopPing(); this.pingTimer = setInterval(() => this.sendPacket(0xc0, new Uint8Array(0)), 30000); }
  stopPing() { if (this.pingTimer) clearInterval(this.pingTimer); this.pingTimer = null; }
}
function utf8(value) { const data = new TextEncoder().encode(String(value)); return concat(u16(data.length), data); }
function u16(value) { return Uint8Array.from([(value >> 8) & 255, value & 255]); }
function concat(...parts) { const size = parts.reduce((n, p) => n + p.length, 0); const out = new Uint8Array(size); let at = 0; parts.forEach(p => { out.set(p, at); at += p.length; }); return out; }
function encodeRemainingLength(value) { const out = []; do { let digit = value % 128; value = Math.floor(value / 128); if (value > 0) digit |= 128; out.push(digit); } while (value > 0); return Uint8Array.from(out); }
function decodeRemainingLength(bytes, offset) { let value = 0, multiplier = 1, index = offset, digit; do { if (index >= bytes.length || multiplier > 128 * 128 * 128) return null; digit = bytes[index++]; value += (digit & 127) * multiplier; multiplier *= 128; } while (digit & 128); return { value, bytes: index - offset }; }


