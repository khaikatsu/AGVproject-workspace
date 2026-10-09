// Client MQTT 3.1.1 tối thiểu cho fixture cục bộ; không phải thư viện firmware.
const net = require('net');
const tls = require('tls');
const {EventEmitter} = require('events');
const string = value=>{const body = Buffer.from(value);const size = Buffer.alloc(2);size.writeUInt16BE(body.length);return Buffer.concat([size,body]);};
function packet(header,body) {
  let length = body.length;const digits = [];
  do {let digit = length%128;length = Math.floor(length/128);if (length) digit |= 128;digits.push(digit);} while (length);
  return Buffer.concat([Buffer.from([header,...digits]),body]);
}
class MqttFixture extends EventEmitter {
  constructor(port,options = {}) {
    super();this.buffer = Buffer.alloc(0);this.connected = false;this.subscribed = false;
    const connect = options.ca ? tls.connect : net.connect;
    this.socket = connect({host:'127.0.0.1',port,...(options.ca ? {ca:options.ca,servername:'localhost'} : {})},()=>{
      let flags = 2;const fields = [string(options.clientId || 'fixture-'+process.pid+'-'+Math.random().toString(16).slice(2))];
      if (options.will) {flags |= 4 | (options.will.retained ? 32 : 0);fields.push(string(options.will.topic),string(options.will.payload));}
      if (options.username) {flags |= 128;fields.push(string(options.username));}
      if (options.password) {flags |= 64;fields.push(string(options.password));}
      this.socket.write(packet(0x10,Buffer.concat([string('MQTT'),Buffer.from([4,flags,0,60]),...fields])));
    });
    this.socket.on('error',error=>this.emit('error',error));
    this.socket.on('data',data=>{
      this.buffer = Buffer.concat([this.buffer,data]);
      while (this.buffer.length >= 2) {
        let length = 0,multiplier = 1,index = 1,digit;
        do {if (index >= this.buffer.length) return;digit = this.buffer[index++];length += (digit&127)*multiplier;multiplier *= 128;} while (digit&128);
        if (this.buffer.length < index+length) return;
        const header = this.buffer[0],body = this.buffer.subarray(index,index+length);this.buffer = this.buffer.subarray(index+length);
        if ((header>>4) === 2) {this.connected = body[1] === 0;this.emit('connected');}
        if ((header>>4) === 9) {this.subscribed = true;this.emit('subscribed');}
        if ((header>>4) === 3) {const size = body.readUInt16BE(0);this.emit('publish',body.subarray(2,2+size).toString(),body.subarray(2+size).toString());}
      }
    });
  }
  subscribe(topic) {this.socket.write(packet(0x82,Buffer.concat([Buffer.from([0,1]),string(topic),Buffer.from([0])])));}
  publish(topic,payload,retained = false) {this.socket.write(packet(retained ? 0x31 : 0x30,Buffer.concat([string(topic),Buffer.from(payload)])));}
  close() {this.socket.destroy();}
}
module.exports = {MqttFixture};
