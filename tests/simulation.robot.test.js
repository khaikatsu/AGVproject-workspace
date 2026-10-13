const assert = require('assert/strict');
const {VirtualRobot} = require('../simulation/robot');
function fixture() {
  const sent = [],events = [],r = Object.create(VirtualRobot.prototype);
  const map = {nodes:[{id:'S1',type:'charging'},{id:'A',type:'waypoint'},{id:'M',type:'mutex'},{id:'K',type:'station'}],obstacles:[]};
  Object.assign(r,{id:'01',home:'S1',node:'A',battery:85,path:[],grants:new Map(),lastCommand:0,commandCache:new Map(),edge:null,
    connected:true,connectedAt:Date.now(),lastHeartbeat:Date.now(),paused:false,fault:'',dropAcks:0,distanceCm:0,routeCost:0,travelSeconds:0,waitSeconds:0,
    runtime:{map,coreState:{map},settings:{speedCmS:25,drainPerCm:0,chargeRate:2},event:(type,data)=>events.push({type,...data}),edge:(a,b)=>[['A','M'],['M','K']].some(e=>e[0] === a && e[1] === b) ? {distance:10,weight:7} : null}});
  r.send = (action,payload)=>sent.push({action,payload});return {r,sent,events};
}
{
  const {r,sent,events} = fixture(),command = {cmd:'GOTO',cmd_id:1,path:['M','K'],target:'K'};
  r.command(command);r.command(command);
  assert.equal(sent.filter(m=>m.action === 'ack').length,2);assert.equal(events.filter(e=>e.type === 'command_applied').length,1);
  assert.throws(()=>r.command({...command,path:['M'],target:'M'}),/khác nội dung/);
  assert.throws(()=>r.command({cmd:'CHARGE_START',cmd_id:2}),/trạm riêng/);
  assert.throws(()=>r.command({cmd:'GOTO',cmd_id:2,path:['K'],target:'K'}),/không tồn tại/);
  assert.throws(()=>r.command({cmd:'WAIT',cmd_id:2147483648}),/cmd_id sai/);
  r.grants.set('M',900);r.command({cmd:'WAIT',cmd_id:2});
  assert.equal(r.grants.size,0);assert.deepEqual(JSON.parse(sent.find(m=>m.action === 'mutex_release').payload),{zone:'M',grant_id:900});
  r.command({cmd:'GOTO',cmd_id:3,path:['M','K'],target:'K'});r.grants.set('M',901);r.edge = {from:'A',to:'M',progress:0.3,distance:10,weight:7};
  r.command({cmd:'STOP',cmd_id:4});assert.equal(r.grants.get('M'),901,'occupied entry survives STOP');assert.equal(r.edge.progress,0.3);
  r.fault='Lỗi cần xác nhận RFID';r.paused=true;r.command({cmd:'RESET_FAULT',cmd_id:5});r.command({cmd:'RESET_FAULT',cmd_id:5});
  assert.equal(r.grants.get('M'),901);assert.equal(r.edge.progress,0.3);assert.equal(r.paused,true);assert.ok(r.fault,'ACK reset alone never clears the simulated fault or repositions the robot');
}
{
  const {r,sent} = fixture();r.command({cmd:'GOTO',cmd_id:1,path:[],target:'A'});assert.equal(r.fault,'');assert.equal(r.path.length,0);
  for (let id = 2;id < 12;id++) r.command({cmd:'WAIT',cmd_id:id});
  const count = sent.length;r.command({cmd:'GOTO',cmd_id:1,path:[],target:'A'});assert.equal(sent.length,count,'expired cache never re-ACKs stale command');
}
{
  const {r,sent} = fixture();r.command({cmd:'GOTO',cmd_id:1,path:['M'],target:'M'});r.grants.set('M',900);
  r.step(0.2);assert.equal(r.node,'A');assert.equal(r.edge.progress,0.5);
  r.batteryChange(15);assert.equal(r.battery,85);assert.equal(r.pendingBattery,15);
  r.step(0.2);assert.equal(r.node,'M');assert.equal(r.battery,15);assert.equal(r.distanceCm,10);assert.equal(r.routeCost,7);
  assert.ok(sent.some(m=>m.action === 'telemetry' && m.payload === 'M|15|0'));
  r.batteryChange(0);r.step(1);assert.equal(r.distanceCm,10);assert.equal(r.grants.get('M'),900);assert.ok(r.fault);
}
console.log('PASS virtual robot contract: duplicate/changed/stale IDs, bounds/invalid edges/home charging, empty path, unused vs occupied grant, RFID-deferred low pin, immediate zero failsafe');
