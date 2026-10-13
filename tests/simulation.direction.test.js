const assert=require('node:assert/strict');
const map=require('../data/map_layout.json'),motion=require('../simulation/public/motion');
const {VirtualRobot}=require('../simulation/robot');
assert.equal(motion.parkedHeading(map,'S1'),90);
assert.equal(motion.parkedHeading(map,'S2'),270);
for(let n=1;n<=6;n++){
  const bay='K'+n,door='G'+n,expected=n<=3?180:0;
  assert.equal(motion.parkedHeading(map,bay),expected);
  const enter=motion.describe({id:'01',node:door,edge:{from:door,to:bay}},map);
  assert.equal(enter.heading,enter.travel);assert.equal(enter.label,'Tiến vào kho');
  const exit=motion.describe({id:'01',node:bay,edge:{from:bay,to:door}},map);
  assert.equal(exit.heading,expected);assert.equal(exit.reverse,true);assert.equal(exit.label,'Lùi ra kho');
  assert.equal(motion.describe({id:'01',node:bay,edge:{from:bay,to:door}},map,[{status:'active',items:[{robot:'01',status:'pickup_service'}]}]).label,'Lùi ra kho','moving edge overrides a stale service snapshot');
  for(const [phase,label]of [['pickup_service','Bốc hàng'],['dropoff_service','Dỡ hàng']]){
    const result=motion.describe({id:'01',node:bay},map,[{status:'active',items:[{robot:'01',status:phase}]}]);
    assert.equal(result.heading,expected);assert.equal(result.travel,null);assert.equal(result.label,label);
  }
}
for(const id of ['S1','S2']){
  const door='GS'+id.slice(1),out=motion.describe({id:'01',node:id,edge:{from:id,to:door}},map),entry=motion.describe({id:'01',node:door,edge:{from:door,to:id}},map);
  assert.equal(out.heading,out.travel);assert.equal(out.reverse,false);assert.equal(out.label,'Tiến ra trạm');
  assert.equal(entry.heading,entry.travel);assert.equal(entry.label,'Tiến vào trạm');
  assert.equal(motion.normalize(entry.heading+180),motion.parkedHeading(map,id));
}
// Snapshot keeps the last edge orientation, then faces out after returning to S1.
const robot=Object.create(VirtualRobot.prototype);
Object.assign(robot,{id:'01',home:'S1',node:'GS1',path:['S1'],edge:null,battery:85,headingDeg:90,connected:true,lastHeartbeat:Date.now(),grants:new Map(),distanceCm:0,routeCost:0,travelSeconds:0,waitSeconds:0,
  runtime:{map,settings:{speedCmS:1,drainPerCm:0},event:()=>{},edge:(a,b)=>({distance:20,weight:20})}});
robot.send=()=>{};robot.step(1);assert.equal(robot.headingDeg,270);assert.equal(robot.edge.progress,.05);
robot.step(20);assert.equal(robot.node,'S1');assert.equal(robot.snapshot().headingDeg,90);
console.log('PASS simulated headings: 6 warehouses forward in/reverse out, 2 stations forward in/turn/out, pickup/dropoff labels, saved heading');
