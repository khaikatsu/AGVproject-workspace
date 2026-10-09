const fs = require('fs');
const {performance} = require('perf_hooks');
const {VirtualRobot} = require('./robot');
const {normalizeOrder,chooseRobot} = require('./order-planner');
const fail = (message,status = 409)=>Object.assign(new Error(message),{status});
class SimulationRuntime {
  constructor({directory,map,mqttPort}) {
    this.directory = directory;this.map = map;this.mqttPort = mqttPort;
    this.settings = {speedCmS:25,timeScale:1,drainPerCm:0.002,chargeRate:2,serviceSeconds:2};
    this.started = Date.now();this.simSeconds = 0;this.events = [];this.jobs = [];this.orders = [];this.sequence = 0;this.orderAdvanceBusy = false;
    this.tasks = new Map();this.coreState = null;this.health = 'Đang khởi động';this.base = null;this.actionBusy = new Set();
    this.logStream = fs.createWriteStream(directory+'/events.jsonl',{flags:'a'});
    this.logStream.on('error',error=>{this.health = 'Lỗi ghi nhật ký: '+error.message;for (const robot of this.robots || []) robot.paused = true;});
    this.event('run_start',{settings:this.settings,map:map.map_name,model:'S1 point robot; map coordinates assumed cm'});
    this.robots = ['01','02'].map((id,index)=>new VirtualRobot(id,`S${index+1}`,this));
  }
  event(type,data = {}) {
    const event = {seq:++this.sequence,time:new Date().toISOString(),simSeconds:this.simSeconds,type,...data};
    this.events.push(event);if (this.events.length > 200) this.events.shift();
    if (!this.logStream.writableEnded) this.logStream.write(JSON.stringify(event)+'\n');
  }
  edge(from,to) {
    const edge = this.map.edges.find(e=>e.from === from && e.to === to || e.two_way && e.from === to && e.to === from);
    const a = this.map.nodes.find(n=>n.id === from),b = this.map.nodes.find(n=>n.id === to);
    if (!edge || !a || !b) return null;
    return {weight:edge.weight,distance:Math.max(0.001,Math.hypot(b.x-a.x,b.y-a.y))};
  }
  robot(id) {const robot = this.robots.find(r=>r.id === id);if (!robot) throw fail('Chỉ có hai xe mô phỏng 01 và 02',400);return robot;}
  async api(method,route,body) {
    const response = await fetch(this.base+route,{method,signal:AbortSignal.timeout(7000),headers:body ? {'Content-Type':'application/json'} : undefined,body:body ? JSON.stringify(body) : undefined});
    const data = await response.json();if (!response.ok) throw fail(data.error || 'Core từ chối',response.status);return data;
  }
  start(base) {
    this.base = base;this.lastTick = performance.now();this.lastTelemetry = 0;
    this.timer = setInterval(()=>{
      const now = performance.now(),dt = Math.min(0.5,(now-this.lastTick)/1000)*this.settings.timeScale;this.lastTick = now;
      this.simSeconds += dt;for (const robot of this.robots) robot.step(dt);
      if (now-this.lastTelemetry > 400) {this.lastTelemetry = now;for (const robot of this.robots) robot.telemetry();}
      this.advanceJobs();
      this.advanceOrders().catch(error=>this.event('order_scheduler_error',{message:error.message}));
    },50);
    const poll = async()=>{
      if (this.closed) return;
      try {
        const state = await this.api('GET','/api/state');if (this.closed) return;this.coreState = state;
        if (state.map?.nodes?.length) {
          const mapSignature = JSON.stringify(state.map);
          if (mapSignature !== this.mapSignature) {this.event('map_state',{map:state.map});this.mapSignature = mapSignature;}
          this.map = state.map;
        }
        this.health = state.core_ready && state.mqtt_connected ? 'Sẵn sàng' : 'Đang chờ Core / MQTT';
        for (const task of state.fleet?.tasks || []) {
          const previous = this.tasks.get(task.task_id);
          if (previous?.status !== task.status || previous?.assigned_agv !== task.assigned_agv) {
            this.event('task_state',task);this.tasks.set(task.task_id,{...task,firstWall:previous?.firstWall || Date.now(),firstSim:previous?.firstSim ?? this.simSeconds,
              endWall:['completed','cancelled','interrupted'].includes(task.status) ? Date.now() : null,
              endSim:['completed','cancelled','interrupted'].includes(task.status) ? this.simSeconds : null});
          }
        }
        this.autoRecoverIdleRobots().catch(error=>this.event('auto_recovery_error',{message:error.message}));
      } catch (error) {this.health = 'Mất liên kết Core: '+error.message;}
      if (!this.closed) this.pollTimer = setTimeout(poll,200);
    };
    poll();
  }
  activeJob(id) {return this.jobs.find(j=>j.robot === id && !j.ended);}
  async cargoJob(id,pickup,dropoff,orderItem = null) {
    const robot = this.robot(id),state = this.coreState?.fleet?.agvs?.find(r=>r.id === id);
    if (this.coreState?.shift?.phase !== 'RUNNING' || state?.state !== 'IDLE' || !robot.connected || robot.paused || robot.fault || robot.cargo || this.activeJob(id))
      throw fail('Bắt đầu ca và chọn xe rảnh, không có hàng/lỗi');
    for (const target of [pickup,dropoff]) if (!this.map.nodes.some(n=>n.id === target && n.type === 'station')) throw fail('Lấy/trả hàng phải chọn node kho',400);
    const job = {id:'job-'+(this.jobs.length+1),robot:id,pickup,dropoff,phase:'to_pickup',taskIds:[],started:Date.now(),startSim:this.simSeconds,
      startDistance:robot.distanceCm,startCost:robot.routeCost,startWait:robot.waitSeconds,
      ...(orderItem ? {orderId:orderItem.orderId,itemNumber:orderItem.itemNumber} : {})};
    this.jobs.push(job);this.event('job_start',{...job});
    try {await this.dispatchStage(job,pickup,'pickup_service');}
    catch (error) {this.finish(job,'failed',error.message);throw error;}
    return job;
  }
  async dispatchStage(job,target,service) {
    const robot = this.robot(job.robot);
    if (robot.node === target && !robot.edge) {job.phase = service;job.serviceUntil = this.simSeconds+this.settings.serviceSeconds;return;}
    job.busy = true;
    try {
      const result = await this.api('POST',`/api/agvs/${robot.id}/goto`,{target,priority:0});
      if (!result.task_id) throw fail('Thiếu task_id từ Core');
      job.task_id = result.task_id;job.taskIds.push(result.task_id);this.event('job_task',{job:job.id,task_id:result.task_id,target});
    } finally {job.busy = false;}
  }
  advanceJobs() {
    for (const job of this.jobs) {
      if (job.ended || job.busy) continue;
      const robot = this.robot(job.robot),task = this.tasks.get(job.task_id);
      if (!robot.connected || robot.fault || this.coreState?.shift?.phase !== 'RUNNING') {this.finish(job,'interrupted','Xe lỗi/mất mạng hoặc ca đang kết thúc');continue;}
      if (job.phase === 'to_pickup' || job.phase === 'to_dropoff') {
        if (!task) continue;
        if (task.assigned_agv && task.assigned_agv !== job.robot || ['interrupted','cancelled','pending'].includes(task.status)) {
          this.finish(job,'interrupted','Core hủy/gián đoạn hoặc phân công lại task; chuỗi hàng không được coi là hoàn thành');continue;
        }
        if (task.status !== 'completed') continue;
        if (robot.edge || robot.node !== (job.phase === 'to_pickup' ? job.pickup : job.dropoff)) {this.finish(job,'failed','Task không trùng vị trí xe lấy/trả hàng');continue;}
        job.phase = job.phase === 'to_pickup' ? 'pickup_service' : 'dropoff_service';job.serviceUntil = this.simSeconds+this.settings.serviceSeconds;
      }
      if (!job.phase.endsWith('_service') || this.simSeconds < job.serviceUntil || robot.paused) continue;
      if (job.phase === 'pickup_service') {
        robot.cargo = true;this.event('cargo_pickup',{robot:robot.id,node:robot.node,job:job.id});job.phase = 'to_dropoff';
        this.dispatchStage(job,job.dropoff,'dropoff_service').catch(error=>this.finish(job,'failed',error.message));
      } else {
        robot.cargo = false;this.event('cargo_dropoff',{robot:robot.id,node:robot.node,job:job.id});this.finish(job,'completed');
      }
    }
  }
  finish(job,status,reason = '') {
    if (job.ended) return;
    const robot = this.robot(job.robot);Object.assign(job,{phase:status,reason,ended:Date.now(),endSim:this.simSeconds,
      wallSeconds:(Date.now()-job.started)/1000,simSeconds:this.simSeconds-job.startSim,
      distanceCm:robot.distanceCm-job.startDistance,routeCost:robot.routeCost-job.startCost,waitSeconds:robot.waitSeconds-job.startWait});
    this.event('job_result',{...job});
  }
  createOrder(body) {
    if (this.health !== 'Sẵn sàng' || this.coreState?.shift?.phase !== 'RUNNING')
      throw fail('Bắt đầu ca và chờ Core/MQTT sẵn sàng trước khi tạo đơn');
    let input;
    try {input = normalizeOrder(this.map,body);} catch (error) {throw fail(error.message,400);}
    const items = [];
    for (const line of input.deliveries) for (let i=0;i<line.quantity;i++)
      items.push({number:items.length+1,dropoff:line.dropoff,status:'queued',robot:null,jobId:null});
    const order = {id:'order-'+(this.orders.length+1),pickup:input.pickup,deliveries:input.deliveries,total:input.total,
      items,destinationOwners:{},status:'active',created:Date.now(),startSim:this.simSeconds};
    this.orders.push(order);
    this.event('order_start',{order:order.id,pickup:order.pickup,deliveries:order.deliveries,total:order.total});
    return order;
  }
  reconcileOrders() {
    for (const order of this.orders) {
      if (order.status === 'completed') continue;
      for (const item of order.items) {
        if (item.status !== 'in_progress') continue;
        const job = this.jobs.find(candidate=>candidate.id === item.jobId);
        if (!job?.ended) continue;
        item.status = job.phase === 'completed' ? 'delivered' : 'interrupted';
        item.reason = job.reason || '';
        item.ended = job.ended;item.endSim = job.endSim;
        this.event('order_item_result',{order:order.id,item:item.number,robot:item.robot,job:job.id,status:item.status,reason:item.reason});
        if (item.status === 'interrupted') {
          order.status = 'needs_attention';order.reason = `Kiện ${item.number} gián đoạn: ${item.reason || job.phase}`;
        }
      }
      if (order.status === 'active' && this.coreState?.shift?.phase !== 'RUNNING') {
        order.status = 'needs_attention';order.reason = 'Ca kết thúc trước khi giao đủ kiện';
      }
      if (order.status === 'active' && order.items.every(item=>item.status === 'delivered')) {
        order.status = 'completed';order.ended = Date.now();order.endSim = this.simSeconds;
        this.event('order_result',{order:order.id,status:'completed',total:order.total});
      }
    }
  }
  eligibleOrderRobots() {
    return this.robots.filter(robot=>{
      const state = this.coreState?.fleet?.agvs?.find(agv=>agv.id === robot.id);
      return state?.state === 'IDLE' && state.online && robot.connected && !robot.paused && !robot.fault && !robot.cargo && !this.activeJob(robot.id);
    });
  }
  async advanceOrders() {
    if (this.orderAdvanceBusy || this.closed) return;
    this.orderAdvanceBusy = true;
    try {
      this.reconcileOrders();
      if (this.health !== 'Sẵn sàng' || this.coreState?.shift?.phase !== 'RUNNING') return;
      for (const order of this.orders) {
        if (order.status !== 'active') continue;
        for (const item of order.items) {
          if (order.status !== 'active') break;
          if (item.status !== 'queued' || item.retryAfter && Date.now() < item.retryAfter) continue;
          // Preserve order within one destination, but let another destination use an idle robot.
          if (order.items.some(previous=>previous.number < item.number && previous.dropoff === item.dropoff && previous.status === 'queued')) continue;
          const owner = order.destinationOwners[item.dropoff];
          const available = this.eligibleOrderRobots().filter(robot=>
            (!owner || robot.id === owner) &&
            this.robots.every(other=>other.id === robot.id || other.node !== item.dropoff));
          if (!available.length) continue;
          const tripCount = id=>order.items.filter(candidate=>candidate.robot === id && ['in_progress','delivered'].includes(candidate.status)).length;
          const fewestTrips = Math.min(...available.map(robot=>tripCount(robot.id)));
          const balanced = available.filter(robot=>tripCount(robot.id) === fewestTrips);
          const choice = chooseRobot(this.map,balanced,order.pickup,item.dropoff);
          if (!choice) continue;
          item.status = 'dispatching';item.robot = choice.robot.id;item.estimatedCost = choice.cost;item.note = '';
          try {
            const job = await this.cargoJob(choice.robot.id,order.pickup,item.dropoff,{orderId:order.id,itemNumber:item.number});
            item.jobId = job.id;item.status = 'in_progress';
            order.destinationOwners[item.dropoff] = choice.robot.id;
            this.event('order_item_dispatched',{order:order.id,item:item.number,robot:item.robot,job:job.id,estimatedCost:item.estimatedCost});
          } catch (error) {
            const failedJob = this.jobs.findLast(job=>job.orderId === order.id && job.itemNumber === item.number && job.robot === item.robot);
            if (failedJob && !failedJob.taskIds.length && !choice.robot.cargo && /Khong tim duoc duong|Không tìm được đường/i.test(error.message)) {
              failedJob.retryableRoute = true;
              item.status = 'queued';item.robot = null;item.jobId = null;
              item.retryAfter = Date.now()+1000;
              item.note = 'Đường đang bị xe khác chiếm; sẽ thử lại khi thông tuyến';
              this.event('order_item_wait',{order:order.id,item:item.number,robot:choice.robot.id,reason:error.message});
              continue;
            }
            item.jobId = failedJob?.id || null;
            item.status = 'interrupted';item.reason = error.message;
            order.status = 'needs_attention';order.reason = `Kiện ${item.number} chưa được cấp việc: ${error.message}`;
            this.event('order_item_result',{order:order.id,item:item.number,robot:item.robot,job:item.jobId,status:item.status,reason:item.reason});
            break;
          }
        }
      }
    } finally {this.orderAdvanceBusy = false;}
  }
  async autoRecoverIdleRobots() {
    if (this.closed || !this.coreState?.core_ready || !this.coreState?.mqtt_connected) return;
    for (const robot of this.robots) {
      if (robot.connected || !robot.everConnected || robot.manualRecoveryRequired || robot.userPaused || robot.fault || robot.battery <= 0 ||
          robot.node !== robot.home || robot.edge || robot.path.length || robot.cargo || robot.waitingZone ||
          robot.grants.size || this.activeJob(robot.id) || this.actionBusy.has(robot.id) || Date.now() < robot.autoRecoverAfter) continue;
      robot.autoRecoverAfter = Date.now()+5000;
      this.actionBusy.add(robot.id);
      this.event('auto_recovery_start',{robot:robot.id,node:robot.node});
      try {
        await this.recoverRobot(robot);
        this.event('auto_recovery_completed',{robot:robot.id,node:robot.node});
      } catch (error) {
        this.event('auto_recovery_retry',{robot:robot.id,message:error.message});
      } finally {this.actionBusy.delete(robot.id);}
    }
  }
  async action(id,body) {
    if (!body || typeof body !== 'object' || Array.isArray(body) || typeof body.action !== 'string') throw fail('Thao tác cần object có action',400);
    if (this.actionBusy.has(id)) throw fail('Xe đang xử lý thao tác; chờ kết quả trước khi thử lại');
    this.actionBusy.add(id);
    try {return await this.applyAction(id,body);} finally {this.actionBusy.delete(id);}
  }
  async applyAction(id,body) {
    const robot = this.robot(id);this.event('user_action',{robot:id,...body});
    switch (body.action) {
      case 'battery':
        if (!Number.isFinite(body.value) || body.value < 0 || body.value > 100) throw fail('Pin phải từ 0 đến 100',400);
        robot.batteryChange(body.value);break;
      case 'pause': robot.userPaused = true;robot.paused = true;break;
      case 'resume':
        if (robot.fault || !robot.connected || ['ERROR','OFFLINE'].includes(this.coreState?.fleet?.agvs?.find(r=>r.id === id)?.state))
          throw fail('Xe đang lỗi/mất mạng. Nạp pin nếu đã hết pin, rồi bấm Phục hồi xe / RFID trước');
        robot.userPaused = false;robot.paused = false;break;
      case 'offline': robot.manualRecoveryRequired = true;robot.mqtt.close();break;
      case 'online': robot.connect();break;
      case 'drop_ack': robot.dropAcks = 1;break;
      case 'recover': await this.recoverRobot(robot);break;
      case 'unload':
        if (this.activeJob(id) || robot.edge || robot.path.length) throw fail('Chỉ gỡ hàng khi xe đã dừng và chuỗi hàng kết thúc');
        robot.cargo = false;break;
      default: throw fail('Thao tác mô phỏng không hợp lệ',400);
    }
    return robot.snapshot();
  }
  async recoverRobot(robot) {
    const id = robot.id;
    if (robot.battery <= 0) throw fail('Xe còn 0% pin. Bấm Nạp pin 85% hoặc Pin thấp 15%, rồi Phục hồi xe / RFID');
    robot.paused = true;
    if (!robot.connected) robot.connect();
    const deadline = Date.now()+5000;
    while (!robot.connected && Date.now() < deadline) await new Promise(resolve=>setTimeout(resolve,50));
    if (!robot.connected) throw fail('Chưa kết nối được MQTT; xe tiếp tục giữ dừng');
    robot.confirmRfid({hold:true});
    let fresh = null;
    while (Date.now() < deadline) {
      const state = await this.api('GET','/api/state'),vehicle = state.fleet?.agvs?.find(r=>r.id === id);
      if (state.core_ready && state.mqtt_connected && vehicle?.online && vehicle.position_valid && vehicle.node === robot.node && vehicle.battery === Math.round(robot.battery) &&
          robot.lastHeartbeat >= robot.connectedAt && Date.now()-robot.lastHeartbeat <= 3000) {
        if (vehicle.state === 'ERROR') {fresh = state;break;}
        // A fast broker restart can hide LWT. Cancel the old Core plan before unlocking the point model.
        robot.send('error','Xac minh RFID mo phong; dung va huy ke hoach cu');
      }
      robot.telemetry();await new Promise(resolve=>setTimeout(resolve,80));
    }
    if (!fresh) throw fail('Core chưa xác nhận RFID/pin mới; xe tiếp tục giữ dừng');
    for (const [zone,grant] of Object.entries(fresh.mutex || {})) {
      if (grant.owner !== id || zone === robot.node) continue;
      // RFID confirmation locates only this simulated point; never infer this from missing telemetry.
      await this.api('POST',`/api/mutex/${zone}/confirm-clear`,{owner:id,grant_id:grant.grant_id,physically_clear:true});
    }
    await this.api('POST',`/api/agvs/${id}/clear-error`,{});
    if (fresh.mutex?.[robot.node]?.owner === id) {
      let renewed = false;
      while (Date.now() < deadline) {
        robot.send('mutex_request',robot.node);
        await new Promise(resolve=>setTimeout(resolve,80));
        const current = await this.api('GET','/api/state'),grant = current.mutex?.[robot.node];
        if (current.core_session_id !== fresh.core_session_id) throw fail('Core đổi phiên trong lúc phục hồi; xe giữ dừng');
        if (grant?.owner === id && !grant.recovery_required && robot.grants.get(robot.node) === grant.grant_id) {renewed = true;break;}
      }
      if (!renewed) throw fail('Chưa nhận lại token mutex sau restart; xe giữ dừng');
    }
    robot.paused = false;robot.userPaused = false;robot.manualRecoveryRequired = false;
    this.event('recovery_completed',{robot:id,node:robot.node,battery:robot.battery});
  }
  actionMessage(id,body) {
    const robot = this.robot(id),core = this.coreState?.fleet?.agvs?.find(r=>r.id === id);
    if (body.action === 'battery' && body.value > 0) {
      if (!robot.connected) return {warning:true,message:`Pin xe ảo đã đổi thành ${body.value}%, nhưng MQTT đang offline. Bấm Phục hồi xe / RFID để kết nối và xác nhận lại vị trí`};
      if (robot.fault || core?.state === 'ERROR') return {warning:true,message:`Pin đã đổi thành ${body.value}%; xe còn khóa lỗi. Bấm Phục hồi xe / RFID để Core cho chạy lại`};
      if (robot.paused) return {warning:true,message:`Pin đã đổi thành ${body.value}%; xe đang tạm dừng. Bấm Tiếp tục xe để thực hiện tuyến Core gửi`};
      if (robot.pendingBattery != null) return {message:`Đã đặt ${body.value}% pin; sẽ gửi Core khi quét RFID ${robot.edge.to}`};
      return {message:body.value <= 20 ? `Đã gửi ${body.value}% pin cho Core; chờ tuyến về trạm sạc` : `Đã gửi ${body.value}% pin cho Core`};
    }
    if (body.action === 'recover') return {message:'Đã kết nối, xác nhận RFID/pin và xóa lỗi Core; xe có thể thực hiện lệnh mới'};
    return {message:'Mô phỏng đã nhận thao tác'};
  }
  async goto(id,target) {
    this.robot(id);
    if (this.activeJob(id)) throw fail('Xe đang trong chuỗi lấy/trả hàng');
    this.event('user_goto',{robot:id,target});
    return this.api('POST',`/api/agvs/${id}/goto`,{target,priority:0});
  }
  configure(body) {
    const ranges = {timeScale:[0.25,10],speedCmS:[1,200],drainPerCm:[0,0.1],chargeRate:[0.1,20],serviceSeconds:[0,60]};
    if (!body || typeof body !== 'object' || Array.isArray(body)) throw fail('Thông số cần là object',400);
    for (const [key,value] of Object.entries(body)) if (!Object.hasOwn(ranges,key) || !Number.isFinite(value) || value < ranges[key][0] || value > ranges[key][1]) throw fail('Thông số mô phỏng không hợp lệ',400);
    Object.assign(this.settings,body);this.event('settings',{settings:this.settings});return this.settings;
  }
  snapshot() {
    return {simulation:true,model:'S1 point robots with linear display interpolation',health:this.health,directory:this.directory,started:this.started,wallSeconds:(Date.now()-this.started)/1000,
      simSeconds:this.simSeconds,settings:this.settings,robots:this.robots.map(r=>r.snapshot()),jobs:this.jobs.slice(-100),orders:this.orders.slice(-20),
      tasks:[...this.tasks.values()].slice(-100),totals:{completed:[...this.tasks.values()].filter(t=>t.status === 'completed').length,
        interrupted:[...this.tasks.values()].filter(t=>t.status === 'interrupted').length},events:this.events.slice(-60),ports:this.ports};
  }
  report() {return {...this.snapshot(),tasks:[...this.tasks.values()],jobs:this.jobs,orders:this.orders};}
  async close() {
    this.closed = true;clearInterval(this.timer);clearTimeout(this.pollTimer);
    for (const job of this.jobs) if (!job.ended) this.finish(job,'interrupted','Đóng phiên mô phỏng');
    this.reconcileOrders();
    this.event('run_end');fs.writeFileSync(this.directory+'/report.json',JSON.stringify(this.report(),null,2));
    for (const robot of this.robots) robot.close();await new Promise(resolve=>this.logStream.end(resolve));
  }
}
module.exports = {SimulationRuntime};
