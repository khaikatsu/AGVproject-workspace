// MQTT 3.1.1 QoS 0 for the isolated, loopback-only simulator.
const net = require('net');
const {EventEmitter} = require('events');
function field(value) {
  const body = Buffer.from(value),length = Buffer.alloc(2);
  length.writeUInt16BE(body.length);return Buffer.concat([length,body]);
}
function packet(header,body = Buffer.alloc(0)) {
  let length = body.length;const digits = [];
  do {let digit = length % 128;length = Math.floor(length / 128);if (length) digit |= 128;digits.push(digit);} while (length);
  return Buffer.concat([Buffer.from([header,...digits]),body]);
}
class SimulatorMqtt extends EventEmitter {
  constructor(port,id) {
    super();this.connected = false;this.buffer = Buffer.alloc(0);this.sequence = 0;
    this.socket = net.connect({host:'127.0.0.1',port},()=>{
      this.socket.write(packet(0x10,Buffer.concat([field('MQTT'),Buffer.from([4,38,0,30]),
        field(`simulation-${id}`),field(`agv/${id}/status`),field('offline')])));
    });
    this.socket.on('error',error=>this.emit('fault',error));
    this.socket.on('close',()=>{this.connected = false;clearInterval(this.ping);this.emit('offline');});
    this.socket.on('data',data=>this.read(data));
  }
  read(data) {
    this.buffer = Buffer.concat([this.buffer,data]);
    if (this.buffer.length > 1048576) return this.close();
    while (this.buffer.length >= 2) {
      let length = 0,multiplier = 1,index = 1,digit;
      do {
        if (index >= this.buffer.length) return;
        digit = this.buffer[index++];length += (digit & 127)*multiplier;multiplier *= 128;
        if (index > 5) return this.close();
      } while (digit & 128);
      if (this.buffer.length < index+length) return;
      const header = this.buffer[0],body = this.buffer.subarray(index,index+length);
      this.buffer = this.buffer.subarray(index+length);
      if ((header >> 4) === 2) {
        if (body.length !== 2 || body[1] !== 0) {this.emit('fault',new Error('Broker rejected connection'));return this.close();}
        this.connected = true;this.ping = setInterval(()=>this.socket.write(packet(0xc0)),10000);this.emit('connected');
      }
      if ((header >> 4) === 9) this.emit('subscribed');
      if ((header >> 4) === 3 && body.length >= 2) {
        const size = body.readUInt16BE(0);
        if (size+2 <= body.length && !(header & 6)) this.emit('message',body.subarray(2,2+size).toString(),body.subarray(2+size).toString());
      }
    }
  }
  subscribe(topics) {
    const id = Buffer.alloc(2);id.writeUInt16BE(++this.sequence);
    this.socket.write(packet(0x82,Buffer.concat([id,...topics.flatMap(topic=>[field(topic),Buffer.from([0])])])));
  }
  publish(topic,payload,retained = false) {
    if (this.connected) this.socket.write(packet(retained ? 0x31 : 0x30,Buffer.concat([field(topic),Buffer.from(payload)])));
  }
  close() {clearInterval(this.ping);this.socket.destroy();}
}
module.exports = {SimulatorMqtt};
