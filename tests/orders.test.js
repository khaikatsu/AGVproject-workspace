const {test} = require('node:test');
const assert = require('node:assert/strict');
const {planOrder} = require('../AGVwebserver/order-planner');
const {createOrderService,SERVICE_MS} = require('../AGVwebserver/orders');

const map = {nodes:['K1','K2','K3','K4'].map(id=>({id,type:'station'})),obstacles:[],edges:[
  {from:'K1',to:'K3',weight:10},{from:'K1',to:'K4',weight:1},
  {from:'K2',to:'K3',weight:1},{from:'K2',to:'K4',weight:10},
  {from:'K2',to:'K1',weight:3},
  {from:'K3',to:'K1',weight:1},{from:'K4',to:'K2',weight:1}
]};

test('multi-warehouse matching respects quantities, directed costs and validation',()=>{
  const plan = planOrder(map,{pickups:[{pickup:'K1',quantity:2},{pickup:'K2',quantity:1}],
    deliveries:[{dropoff:'K3',quantity:1},{dropoff:'K4',quantity:2}]});
  assert.equal(plan.total,3);
  assert.deepEqual(plan.items.map(item=>[item.pickup,item.dropoff]),[['K1','K4'],['K1','K4'],['K2','K3']]);
  assert.throws(()=>planOrder(map,{pickups:[{pickup:'K1',quantity:2}],deliveries:[{dropoff:'K3',quantity:1}]}),/Tổng số kiện/);
  assert.throws(()=>planOrder(map,{pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K1',quantity:1}]}),/Không ghép/);
});

test('matching can reroute an earlier pair to minimize the whole order',()=>{
  const graph = {nodes:map.nodes,obstacles:[],edges:[
    {from:'K1',to:'K3',weight:1},{from:'K1',to:'K4',weight:2},
    {from:'K2',to:'K3',weight:2},{from:'K2',to:'K4',weight:100}
  ]};
  const plan = planOrder(graph,{pickups:[{pickup:'K1',quantity:1},{pickup:'K2',quantity:1}],
    deliveries:[{dropoff:'K3',quantity:1},{dropoff:'K4',quantity:1}]});
  assert.deepEqual(plan.items.map(item=>[item.pickup,item.dropoff]),[['K1','K4'],['K2','K3']]);
});

test('order waits at each RFID for ten simulated seconds; Core task ID and robot location are required',async()=>{
  const records = new Map(),commands = [];
  const db = {getOrders:()=>[...records.values()].map(value=>structuredClone(value)),saveOrder:order=>records.set(order.id,structuredClone(order))};
  const robot = {id:'01',node:'K1',state:'IDLE',online:true,position_valid:true,task_id:null};
  const state = {map,fleet:{agvs:[robot],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  const send = async payload=>{
    commands.push(payload);
    const task = {task_id:`task-${commands.length}`,target:payload.target,status:'dispatched',assigned_agv:payload.agv_id};
    state.fleet.tasks.push(task);robot.state = 'WORKING';robot.task_id = task.task_id;
    return {task_id:task.task_id};
  };
  const service = createOrderService({db,getState:()=>state,getSession:()=> 'session-1',send,publish:()=>{}});
  try {
    const order = service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K3',quantity:1}]});
    await service.tick();
    assert.equal(order.items[0].status,'pickup_service');
    assert.equal(commands.length,0,'already at pickup, no GOTO needed');
    assert.ok(Date.parse(order.items[0].service_until)-Date.now() <= SERVICE_MS);
    await service.tick();assert.equal(commands.length,0);
    order.items[0].service_until = new Date(Date.now()-1).toISOString();
    await service.tick();assert.equal(commands.length,1);
    assert.equal(commands[0].target,'K3');assert.equal(order.items[0].task_id,'task-1');
    state.fleet.tasks[0].status = 'completed';robot.node = 'K2';robot.state = 'IDLE';robot.task_id = null;
    await service.tick();assert.equal(order.status,'needs_attention','wrong RFID must not count as delivered');
    assert.equal(order.items[0].status,'interrupted');
  } finally {await service.close();}
});

test('completed dropoff requires dwell; restart leaves unfinished orders for reconciliation',async()=>{
  const records = new Map();
  const db = {getOrders:()=>[...records.values()].map(value=>structuredClone(value)),saveOrder:order=>records.set(order.id,structuredClone(order))};
  const robot = {id:'02',node:'K2',state:'IDLE',online:true,position_valid:true,task_id:null};
  const state = {map,fleet:{agvs:[robot],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  let seq = 0;
  const send = async payload=>{
    const task = {task_id:`task-${++seq}`,target:payload.target,status:'dispatched',assigned_agv:payload.agv_id};
    state.fleet.tasks.push(task);robot.state = 'WORKING';robot.task_id = task.task_id;return {task_id:task.task_id};
  };
  const args = {db,getState:()=>state,getSession:()=> 'session-1',send,publish:()=>{}};
  const service = createOrderService(args);
  try {
    const order = service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K3',quantity:1}]});
    await service.tick();assert.equal(order.items[0].status,'to_pickup');
    state.fleet.tasks[0].status = 'completed';robot.node = 'K1';robot.state = 'IDLE';robot.task_id = null;
    await service.tick();assert.equal(order.items[0].status,'pickup_service');
    order.items[0].service_until = new Date(Date.now()-1).toISOString();await service.tick();
    assert.equal(order.items[0].status,'to_dropoff');
    state.fleet.tasks[1].status = 'completed';robot.node = 'K3';robot.state = 'IDLE';robot.task_id = null;
    await service.tick();assert.equal(order.items[0].status,'dropoff_service');assert.equal(order.status,'active');
    const restarted = createOrderService(args);
    try {assert.equal(restarted.list()[0].status,'needs_attention');}
    finally {await restarted.close();}
    order.items[0].service_until = new Date(Date.now()-1).toISOString();await service.tick();
    assert.equal(order.status,'completed');
  } finally {await service.close();}
});

test('two idle robots can receive different items in one order without double booking',async()=>{
  const records = new Map(),commands = [];
  const db = {getOrders:()=>[],saveOrder:order=>records.set(order.id,structuredClone(order))};
  const state = {map,fleet:{agvs:[
    {id:'01',node:'K1',state:'IDLE',online:true,position_valid:true,task_id:null},
    {id:'02',node:'K2',state:'IDLE',online:true,position_valid:true,task_id:null}
  ],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  const service = createOrderService({db,getState:()=>state,getSession:()=> 'session-1',
    send:async payload=>{commands.push(payload);return {task_id:`task-${commands.length}`};},publish:()=>{}});
  try {
    const order = service.create({pickups:[{pickup:'K1',quantity:1},{pickup:'K2',quantity:1}],
      deliveries:[{dropoff:'K3',quantity:1},{dropoff:'K4',quantity:1}]});
    await service.tick();
    assert.deepEqual(order.items.map(item=>item.robot).sort(),['01','02']);
    assert.equal(order.items.filter(item=>item.status === 'pickup_service').length,2);
    assert.equal(commands.length,0);
  } finally {await service.close();}
});

test('offline robot interrupts dwell and cannot mark cargo delivered',async()=>{
  const db = {getOrders:()=>[],saveOrder:()=>{}};
  const robot = {id:'01',node:'K1',state:'IDLE',online:true,position_valid:true,task_id:null};
  const state = {map,fleet:{agvs:[robot],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  const service = createOrderService({db,getState:()=>state,getSession:()=> 'session-1',send:async()=>{throw Error('unexpected');},publish:()=>{}});
  try {
    const order = service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K3',quantity:1}]});
    await service.tick();assert.equal(order.items[0].status,'pickup_service');
    robot.online = false;order.items[0].service_until = new Date(Date.now()-1).toISOString();
    await service.tick();assert.equal(order.status,'needs_attention');assert.equal(order.items[0].status,'interrupted');
    assert.throws(()=>service.resolve(order.id,false),/Cần xác nhận/);
    assert.equal(service.resolve(order.id,true).status,'closed');
  } finally {await service.close();}
});
