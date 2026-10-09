const {SimulatorMqtt} = require('./mqtt');
class VirtualRobot {
  constructor(id,home,runtime) {
    this.id = id;this.home = home;this.node = home;this.runtime = runtime;this.battery = 85;
    this.path = [];this.grants = new Map();this.lastCommand = 0;this.commandCache = new Map();this.edge = null;this.connected = false;
    this.distanceCm = 0;this.routeCost = 0;this.waitSeconds = 0;this.travelSeconds = 0;
    this.charging = false;this.paused = false;this.fault = '';this.cargo = false;this.dropAcks = 0;
    this.manualRecoveryRequired = false;this.userPaused = false;this.autoRecoverAfter = 0;this.everConnected = false;
    this.lastHeartbeat = 0;this.session = '';this.connect();
  }
  log(type,data = {}) {this.runtime.event(type,{robot:this.id,...data});}
  send(action,payload,retained = false) {this.mqtt?.publish(`agv/${this.id}/${action}`,payload,retained);}
  telemetry() {if (this.connected) this.send('telemetry',`${this.node}|${Math.round(this.battery)}|0`);}
  connect() {
    if (this.mqtt && !this.mqtt.socket.destroyed) return;
    const client = this.mqtt = new SimulatorMqtt(this.runtime.mqttPort,this.id);
    client.on('fault',error=>this.log('mqtt_error',{message:error.message}));
    client.on('connected',()=>client.subscribe([`agv/${this.id}/command`,`agv/${this.id}/mutex_grant`,'agv/server/heartbeat']));
    client.on('subscribed',()=>{
      this.connected = true;this.everConnected = true;this.connectedAt = Date.now();this.send('status','online',true);this.telemetry();this.log('online',{node:this.node});
    });
    client.on('offline',()=>{this.connected = false;this.paused = true;this.log('offline',{node:this.node});});
    client.on('message',(topic,payload)=>{
      try {
        if (topic === 'agv/server/heartbeat') {
          if (this.session && this.session !== payload) this.stopWithError('Core đổi phiên; cần xác nhận lại RFID');
          this.session = payload;this.lastHeartbeat = Date.now();return;
        }
        this.log('mqtt_rx',{topic,payload});
        if (topic.endsWith('/command')) this.command(JSON.parse(payload));
        else {
          const grant = JSON.parse(payload);
          if (typeof grant.zone !== 'string' || !Number.isInteger(grant.grant_id) || grant.grant_id <= 0 || grant.grant_id > 2147483647) return;
          if (!this.runtime.map.nodes.some(node=>node.id === grant.zone && node.type === 'mutex')) return;
          // A grant can arrive after WAIT cancelled its request. Return only
          // reservations the point model proves it has never entered.
          const unused = grant.zone !== this.waitingZone && !this.grants.has(grant.zone) &&
            this.node !== grant.zone && !(this.edge?.to === grant.zone && this.edge.progress > 0);
          this.grants.set(grant.zone,grant.grant_id);this.send('mutex_ack',JSON.stringify(grant));
          this.log('grant_ack',grant);
          if (unused) {this.log('unused_grant',{zone:grant.zone});this.release(grant.zone);}
        }
      } catch (error) {this.stopWithError('Payload không hợp lệ: '+error.message);}
    });
  }
  command(command) {
    if (!Number.isInteger(command.cmd_id) || command.cmd_id <= 0 || command.cmd_id > 2147483647 ||
        !['GOTO','STOP','WAIT','CHARGE_START'].includes(command.cmd)) throw new Error('command/cmd_id sai');
    const signature = JSON.stringify({cmd:command.cmd,path:command.path,target:command.target});
    if (this.commandCache.has(command.cmd_id) && this.commandCache.get(command.cmd_id) !== signature) throw new Error('Cùng cmd_id nhưng khác nội dung');
    if (command.cmd_id <= this.lastCommand && !this.commandCache.has(command.cmd_id)) {this.log('command_stale',{cmd_id:command.cmd_id});return;}
    if (command.cmd_id > this.lastCommand && command.cmd === 'GOTO') {
      if (!Array.isArray(command.path) || command.path.length > 64 || (command.path.length ? command.path.at(-1) !== command.target : command.target !== this.node)) throw new Error('path sai');
      let from = this.node;
      for (const to of command.path) {if (!this.runtime.edge(from,to)) throw new Error(`Cạnh ${from} → ${to} không tồn tại`);from = to;}
    }
    if (command.cmd_id > this.lastCommand && command.cmd === 'CHARGE_START' && this.node !== this.home) throw new Error('Lệnh sạc ngoài trạm riêng');
    if (this.dropAcks > 0) {this.dropAcks--;this.log('ack_dropped',{cmd_id:command.cmd_id});}
    else this.send('ack',String(command.cmd_id));
    if (command.cmd_id <= this.lastCommand) {this.log('command_duplicate',{cmd_id:command.cmd_id});return;}
    this.lastCommand = command.cmd_id;this.commandCache.set(command.cmd_id,signature);
    if (this.commandCache.size > 8) this.commandCache.delete(this.commandCache.keys().next().value);
    this.log('command_applied',command);
    if (command.cmd === 'GOTO') {
      if (this.edge && this.edge.to !== command.path[0]) return this.stopWithError('Đổi hướng giữa cạnh; cần xác nhận RFID trong mô phỏng');
      this.path = [...command.path];this.charging = false;
      let from = this.node,distanceCm = 0,routeCost = 0;
      for (const to of command.path) {const e = this.runtime.edge(from,to);distanceCm += e.distance;routeCost += e.weight;from = to;}
      this.log('route_plan',{cmd_id:command.cmd_id,path:command.path,distanceCm,routeCost,travelEstimateSeconds:distanceCm/this.runtime.settings.speedCmS});
    } else {
      this.path = [];this.waitingZone = null;this.charging = command.cmd === 'CHARGE_START';
      if (this.charging) this.chargeReported = false;
      if (this.charging && this.node !== this.home) this.stopWithError('Lệnh sạc ngoài trạm riêng');
    }
    for (const zone of this.grants.keys()) if (zone !== this.node && zone !== this.path[0] &&
        !(this.edge?.to === zone && this.edge.progress > 0)) this.release(zone);
  }
  release(zone) {
    const grant_id = this.grants.get(zone);if (!grant_id) return;
    this.send('mutex_release',JSON.stringify({zone,grant_id}));this.grants.delete(zone);this.log('zone_exit',{zone,grant_id});
  }
  stopWithError(message) {
    if (this.fault === message) return;
    this.fault = message;this.paused = true;this.charging = false;this.send('error',message);this.log('failsafe',{message});
  }
  step(dt) {
    if (!this.connected || this.paused || this.fault) return;
    if (Date.now()-(this.lastHeartbeat || this.connectedAt) > 3000) return this.stopWithError('Mất heartbeat Core >3s');
    if (!this.lastHeartbeat) return;
    if (this.charging) {
      this.battery = Math.min(100,this.battery+this.runtime.settings.chargeRate*dt);
      if (this.battery >= 90 && !this.chargeReported) {this.send('charging_done',String(Math.round(this.battery)));this.chargeReported = true;}
      return;
    }
    this.chargeReported = false;
    if (!this.path.length) return;
    if (this.battery <= 0) return this.stopWithError('Hết pin mô phỏng; xe dừng tại chỗ');
    const next = this.path[0];
    if (this.runtime.coreState?.map?.obstacles?.includes(next)) {this.waitSeconds += dt;return;}
    const mutex = this.runtime.map.nodes.find(node=>node.id === next)?.type === 'mutex';
    if (mutex && !this.grants.has(next)) {
      this.waitSeconds += dt;this.waitingZone = next;
      if (!this.lastRequest || Date.now()-this.lastRequest >= 1000) {
        this.lastRequest = Date.now();this.send('mutex_request',next);this.log('zone_request',{zone:next});
      }
      return;
    }
    this.waitingZone = null;this.lastRequest = 0;
    if (!this.edge) {
      const edge = this.runtime.edge(this.node,next);if (!edge) return this.stopWithError('Đường Core gửi không có trên map');
      this.edge = {from:this.node,to:next,progress:0,distance:edge.distance,weight:edge.weight};
      this.log('edge_start',{...this.edge});
    }
    const travelled = Math.min(this.edge.distance*(1-this.edge.progress),this.runtime.settings.speedCmS*dt);
    this.edge.progress = Math.min(1,this.edge.progress+travelled/this.edge.distance);
    this.distanceCm += travelled;this.routeCost += this.edge.weight*travelled/this.edge.distance;
    this.travelSeconds += travelled/this.runtime.settings.speedCmS;
    this.battery = Math.max(0,this.battery-travelled*this.runtime.settings.drainPerCm);
    if (this.edge.progress < 1) return;
    const previous = this.node;this.node = next;this.path.shift();this.edge = null;
    if (previous !== next) this.release(previous);
    this.log('rfid',{node:this.node,battery:this.battery});
    if (this.pendingBattery != null) {this.battery = this.pendingBattery;this.pendingBattery = null;this.chargeReported = false;this.log('battery_applied',{battery:this.battery});}
    this.telemetry();
  }
  batteryChange(value) {
    if (value > 0 && !this.fault && !this.paused && this.edge && this.edge.progress > 0 && this.edge.progress < 1) {
      this.pendingBattery = value;this.log('battery_scheduled',{battery:value,node:this.edge.to});
    } else {this.pendingBattery = null;this.battery = value;this.chargeReported = false;this.telemetry();}
    if (value === 0) this.stopWithError('Hết pin mô phỏng; xe dừng tại chỗ');
  }
  confirmRfid({hold = false} = {}) {
    // Explicit simulated reposition to last read RFID; never done automatically.
    this.edge = null;this.path = [];this.fault = '';this.paused = hold;this.waitingZone = null;this.pendingBattery = null;
    for (const zone of this.grants.keys()) if (zone !== this.node) this.release(zone);
    this.telemetry();this.log('manual_rfid_confirmation',{node:this.node});
  }
  snapshot() {
    return {id:this.id,home:this.home,node:this.node,battery:this.battery,connected:this.connected,
      paused:this.paused,fault:this.fault,charging:this.charging,cargo:this.cargo,path:this.path,edge:this.edge,
      waitingZone:this.waitingZone || null,grants:[...this.grants.keys()],pendingBattery:this.pendingBattery,
      distanceCm:this.distanceCm,routeCost:this.routeCost,waitSeconds:this.waitSeconds,travelSeconds:this.travelSeconds};
  }
  close() {this.mqtt?.close();}
}
module.exports = {VirtualRobot};
