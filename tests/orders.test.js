const {test} = require('node:test');
const assert = require('node:assert/strict');
const {planOrder,shortestRoute} = require('../AGVwebserver/order-planner');
const {createOrderService,SERVICE_MS} = require('../AGVwebserver/orders');

const liftCapable = robot=>({...robot,
  lift:{supported:true,ready:true,fresh:true,observed_node:robot.node,active:false,state:'IDLE',command_id:0},
  virtual_cargo_known:false,virtual_cargo:false});
function setLiftAt(robot,node) {
  robot.node=node;Object.assign(robot.lift,{supported:true,ready:true,fresh:true,observed_node:node,active:false,state:'IDLE'});
}
function completeLift(robot,commandId,up) {
  robot.state='IDLE';robot.task_id=null;robot.virtual_cargo_known=true;robot.virtual_cargo=up;
  Object.assign(robot.lift,{supported:true,ready:true,fresh:true,observed_node:robot.node,active:false,
    state:up?'DONE_UP':'DONE_DOWN',command_id:commandId});
}

const map = {nodes:['K1','K2','K3','K4'].map(id=>({id,type:'station'})),obstacles:[],edges:[
  {from:'K1',to:'K3',weight:10},{from:'K1',to:'K4',weight:1},
  {from:'K2',to:'K3',weight:1},{from:'K2',to:'K4',weight:10},
  {from:'K2',to:'K1',weight:3},
  {from:'K3',to:'K1',weight:1},{from:'K4',to:'K2',weight:1}
]};
for (const fixtureMap of [map]) for (const warehouse of fixtureMap.nodes.filter(node=>node.id.startsWith('K'))) {
  const gate=warehouse.id.replace('K','G');fixtureMap.nodes.push({id:gate,type:'waypoint'});
  fixtureMap.edges.push({from:warehouse.id,to:gate,weight:1,two_way:true});
}

const selectionMap={nodes:['S1','S2','A','B','K1','K2'].map(id=>({id,type:id.startsWith('K')?'station':'waypoint'})),obstacles:[],edges:[
  {from:'S1',to:'K1',weight:100},{from:'S2',to:'A',weight:1},{from:'A',to:'B',weight:1},
  {from:'B',to:'K1',weight:1},{from:'K1',to:'K2',weight:5}
]};
for (const warehouse of selectionMap.nodes.filter(node=>node.id.startsWith('K'))) {
  const gate=warehouse.id.replace('K','G');selectionMap.nodes.push({id:gate,type:'waypoint'});
  selectionMap.edges.push({from:warehouse.id,to:gate,weight:1,two_way:true});
}

test('nearest ready robot uses directed weight, not node count; previews both cargo legs',async()=>{
  const commands=[],state={map:selectionMap,fleet:{agvs:[
    liftCapable({id:'01',node:'S1',state:'IDLE',online:true,position_valid:true}),
    liftCapable({id:'02',node:'S2',state:'IDLE',online:true,position_valid:true})
  ],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  const service=createOrderService({db:{getOrders:()=>[],saveOrder:()=>{}},getState:()=>state,getSession:()=> 'nearest',
    send:async payload=>{commands.push(payload);return {task_id:'nearest-task'};},publish:()=>{}});
  try {
    const order=service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});await service.tick();
    assert.equal(order.items[0].robot,'02');assert.equal(commands[0].agv_id,'02');
    assert.deepEqual(order.items[0].route_plan.pickup,{path:['S2','A','B','K1'],cost:3});
    assert.deepEqual(order.items[0].route_plan.delivery,{path:['K1','K2'],cost:5});
    assert.deepEqual(shortestRoute(selectionMap,'K2','S1'),{path:[],cost:Infinity});
  } finally {await service.close();}
});

test('trial selection defaults to 01 before dispatch, explains fault, and keeps nearest 02 inactive',async()=>{
  const commands=[],records=new Map(),state={map:selectionMap,fleet:{agvs:[
    liftCapable({id:'01',node:'S1',state:'ERROR',error:'LINE_LOST',online:true,position_valid:true}),
    liftCapable({id:'02',node:'S2',state:'IDLE',online:true,position_valid:true})
  ],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING',trial_robot_id:'01'},mqtt_connected:true};
  const db={getOrders:()=>[...records.values()].map(order=>structuredClone(order)),saveOrder:order=>records.set(order.id,structuredClone(order))};
  const options={db,getState:()=>state,getSession:()=> 'fixed-session',
    send:async payload=>{commands.push(payload);return {task_id:'fixed-task'};},publish:()=>{}};
  let service=createOrderService(options);
  try {
    const order=service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});await service.tick();
    assert.equal(order.selection_policy,'fixed:01');assert.equal(order.items[0].robot,'01');
    assert.match(order.items[0].waiting_reason,/LINE_LOST/);assert.equal(order.items[0].status,'queued');assert.equal(commands.length,0);
    assert.deepEqual(order.items[0].route_plan.pickup,{path:['S1','K1'],cost:100});
    await service.close();service=createOrderService(options);await service.tick();
    assert.equal(service.list()[0].status,'active','a wholly queued order has no uncertain movement to replay');
    Object.assign(state.fleet.agvs[0],{state:'IDLE',error:''});await service.tick();
    assert.equal(commands.length,1);assert.equal(commands[0].agv_id,'01');assert.equal(commands[0].target,'K1');
    assert.equal(service.list()[0].items[0].waiting_reason,'');
  } finally {await service.close();}
});

test('startup solo selection excludes the closer inactive robot from warehouse orders',async()=>{
  for (const selected of ['01','02']) {
    const commands=[];
    const state={map,fleet:{agvs:[
      liftCapable({id:'01',node:'K1',state:'IDLE',online:true,position_valid:true,task_id:null}),
      liftCapable({id:'02',node:'K2',state:'IDLE',online:true,position_valid:true,task_id:null})
    ],tasks:[],accepting_tasks:true,trial_robot_id:selected},shift:{phase:'RUNNING',trial_robot_id:selected},mqtt_connected:true};
    const service=createOrderService({db:{getOrders:()=>[],saveOrder:()=>{}},getState:()=>state,getSession:()=> 'selected-test',
      send:async payload=>{commands.push(payload);return {task_id:'selected-order-task'};},publish:()=>{}});
    try {
      const pickup=selected==='01'?'K3':'K4',dropoff=selected==='01'?'K4':'K3';
      const order=service.create({pickups:[{pickup,quantity:1}],deliveries:[{dropoff,quantity:1}]});
      await service.tick();
      assert.equal(order.items[0].robot,selected);assert.equal(order.status,'active');
      assert.equal(commands.length,1);assert.equal(commands[0].agv_id,selected);assert.equal(commands[0].target,pickup);
    } finally {await service.close();}
  }
});

test('unified scoped operation never selects a non-participating robot',async()=>{
  const commands=[],state={map:selectionMap,fleet:{agvs:[
    liftCapable({id:'01',node:'S1',state:'IDLE',online:true,position_valid:true,participating:true}),
    liftCapable({id:'02',node:'S2',state:'IDLE',online:true,position_valid:true,participating:false})
  ],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING',test_mode:'normal',operation_profile:'scoped',active_robot_ids:['01','02']},mqtt_connected:true};
  const service=createOrderService({db:{getOrders:()=>[],saveOrder:()=>{}},getState:()=>state,getSession:()=> 'scoped',
    send:async payload=>{commands.push(payload);return {task_id:'scoped-task'};},publish:()=>{}});
  try{
    const order=service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});await service.tick();
    assert.equal(order.items[0].robot,'01');assert.equal(commands[0].agv_id,'01');
  }finally{await service.close();}
});

test('automatic order does not dispatch a robot without live lift capability',async()=>{
  const commands=[],robot={id:'01',node:'S1',state:'IDLE',online:true,position_valid:true,task_id:null};
  const state={map:selectionMap,fleet:{agvs:[robot],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  const service=createOrderService({db:{getOrders:()=>[],saveOrder:()=>{}},getState:()=>state,getSession:()=> 'no-lift',
    send:async payload=>{commands.push(payload);return {task_id:'unexpected'};},publish:()=>{}});
  try {
    const order=service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});
    await service.tick();assert.equal(order.items[0].status,'queued');assert.equal(commands.length,0);
    assert.match(order.items[0].waiting_reason,/capability nâng\/hạ servo/);
  } finally {await service.close();}
});

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

test('automatic order waits for matching lift completion; Core task ID and robot location are required',async()=>{
  const records = new Map(),commands = [];
  const db = {getOrders:()=>[...records.values()].map(value=>structuredClone(value)),saveOrder:order=>records.set(order.id,structuredClone(order))};
  const robot = liftCapable({id:'01',node:'K1',state:'IDLE',online:true,position_valid:true,task_id:null});
  const state = {map,fleet:{agvs:[robot],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  const send = async payload=>{
    commands.push(payload);
    if(payload.cmd==='warehouse_lift') {
      const commandId=100+commands.length;robot.state=payload.action==='pickup'?'LIFTING':'LOWERING';
      Object.assign(robot.lift,{ready:false,active:true,state:payload.action==='pickup'?'LIFTING':'LOWERING',command_id:commandId});
      return {command_id:commandId};
    }
    const task = {task_id:`task-${commands.length}`,target:payload.target,status:'dispatched',assigned_agv:payload.agv_id};
    state.fleet.tasks.push(task);robot.state = 'WORKING';robot.task_id = task.task_id;
    return {task_id:task.task_id};
  };
  const service = createOrderService({db,getState:()=>state,getSession:()=> 'session-1',send,publish:()=>{}});
  try {
    const order = service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K3',quantity:1}]});
    await service.tick();await service.tick();
    assert.equal(order.items[0].status,'pickup_lift');
    assert.equal(commands.length,1,'already at pickup, first command is the lift');
    assert.equal(commands[0].cmd,'warehouse_lift');assert.equal(commands[0].action,'pickup');
    assert.equal(SERVICE_MS,5000);
    await service.tick();assert.equal(commands.length,1,'ACK/state LIFTING never starts delivery');
    completeLift(robot,order.items[0].lift_command_id+1,true);
    await service.tick();assert.equal(commands.length,1,'DONE_UP của command khác không được làm xe chạy tiếp');
    completeLift(robot,order.items[0].lift_command_id,true);
    await service.tick();assert.equal(commands.length,2);
    assert.equal(commands[1].target,'K3');assert.equal(order.items[0].task_id,'task-2');
    state.fleet.tasks[0].status = 'completed';robot.node = 'K2';robot.state = 'IDLE';robot.task_id = null;
    await service.tick();assert.equal(order.status,'needs_attention','wrong RFID must not count as delivered');
    assert.equal(order.items[0].status,'interrupted');
  } finally {await service.close();}
});

test('completed pickup and dropoff lifts gate movement; restart keeps unfinished cargo for reconciliation',async()=>{
  const records = new Map();
  const db = {getOrders:()=>[...records.values()].map(value=>structuredClone(value)),saveOrder:order=>records.set(order.id,structuredClone(order))};
  const robot = liftCapable({id:'02',node:'K2',state:'IDLE',online:true,position_valid:true,task_id:null});
  const state = {map,fleet:{agvs:[robot],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  let seq = 0;
  const send = async payload=>{
    if(payload.cmd==='warehouse_lift') {
      const commandId=200+(++seq);robot.state=payload.action==='pickup'?'LIFTING':'LOWERING';
      Object.assign(robot.lift,{ready:false,active:true,state:payload.action==='pickup'?'LIFTING':'LOWERING',command_id:commandId});
      return {command_id:commandId};
    }
    const task = {task_id:`task-${++seq}`,target:payload.target,status:'dispatched',assigned_agv:payload.agv_id};
    state.fleet.tasks.push(task);robot.state = 'WORKING';robot.task_id = task.task_id;return {task_id:task.task_id};
  };
  const args = {db,getState:()=>state,getSession:()=> 'session-1',send,publish:()=>{}};
  const service = createOrderService(args);
  try {
    const order = service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K3',quantity:1}]});
    await service.tick();assert.equal(order.items[0].status,'to_pickup');
    state.fleet.tasks[0].status = 'completed';setLiftAt(robot,'K1');robot.state = 'IDLE';robot.task_id = null;
    await service.tick();assert.equal(order.items[0].status,'pickup_lift');
    completeLift(robot,order.items[0].lift_command_id,true);await service.tick();
    assert.equal(order.items[0].status,'to_dropoff');
    state.fleet.tasks[1].status = 'completed';setLiftAt(robot,'K3');robot.state = 'IDLE';robot.task_id = null;
    await service.tick();assert.equal(order.items[0].status,'dropoff_lift');assert.equal(order.status,'active');
    const restarted = createOrderService(args);
    try {assert.equal(restarted.list()[0].status,'needs_attention');}
    finally {await restarted.close();}
    completeLift(robot,order.items[0].lift_command_id,false);await service.tick();
    assert.equal(order.items[0].status,'to_exit');assert.equal(state.fleet.tasks[2].target,'G3');
    assert.equal(order.status,'active','delivery is not finalized while the vehicle is still inside the warehouse');
    state.fleet.tasks[2].status='completed';robot.node='G3';robot.state='IDLE';robot.task_id=null;
    await service.tick();
    assert.equal(order.status,'completed');
  } finally {await service.close();}
});

test('only dispatched unresolved cargo reserves a robot; operator resolution releases the old hold',async()=>{
  const commands=[],state={map:selectionMap,mqtt_connected:true,shift:{phase:'RUNNING',trial_robot_id:'01'},
    fleet:{accepting_tasks:true,tasks:[],agvs:[liftCapable({id:'01',node:'S1',state:'IDLE',online:true,position_valid:true})]}};
  const retained=[{id:'never-started',status:'needs_attention',created:new Date(0).toISOString(),items:[{status:'queued',robot:'01',task_id:null}]},
    {id:'old-cargo',status:'needs_attention',created:new Date(1).toISOString(),items:[{status:'interrupted',robot:'01',task_id:'old-task'}]}];
  const service=createOrderService({db:{getOrders:()=>retained,saveOrder:()=>{}},getState:()=>state,getSession:()=> 'new-session',
    send:async payload=>{commands.push(payload);return {task_id:'new-task'};},publish:()=>{}});
  try {
    const order=service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K2',quantity:1}]});await service.tick();
    assert.match(order.items[0].waiting_reason,/old-cargo/);assert.equal(commands.length,0);
    service.resolve('old-cargo',true);await service.tick();
    assert.equal(commands.length,1);assert.equal(order.items[0].status,'to_pickup');
    assert.equal(service.list()[0].id,order.id,'newest order stays first after reading history');
  } finally {await service.close();}
});

test('robot01 warehouse test never dispatches stored orders or accepts a new order',async()=>{
  const commands=[],state={map:selectionMap,mqtt_connected:true,shift:{phase:'RUNNING',test_mode:'robot01'},
    fleet:{accepting_tasks:true,tasks:[],agvs:[{id:'01',node:'S1',state:'IDLE',online:true,position_valid:true}]}};
  const queued={id:'old-queued',status:'active',items:[{number:1,pickup:'K1',dropoff:'K2',robot:'01',status:'queued',task_id:null}],core_session_id:'test-session'};
  const service=createOrderService({db:{getOrders:()=>[queued],saveOrder:()=>{}},getState:()=>state,getSession:()=> 'test-session',
    send:async payload=>commands.push(payload),publish:()=>{}});
  try {await service.tick();assert.equal(commands.length,0);assert.throws(()=>service.create({pickups:[],deliveries:[]}),/Lấy hàng \/ Hạ hàng/);}
  finally {await service.close();}
});

test('two idle robots can receive different items in one order without double booking',async()=>{
  const records = new Map(),commands = [];
  const db = {getOrders:()=>[],saveOrder:order=>records.set(order.id,structuredClone(order))};
  const state = {map,fleet:{agvs:[
    liftCapable({id:'01',node:'K1',state:'IDLE',online:true,position_valid:true,task_id:null}),
    liftCapable({id:'02',node:'K2',state:'IDLE',online:true,position_valid:true,task_id:null})
  ],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  const service = createOrderService({db,getState:()=>state,getSession:()=> 'session-1',
    send:async payload=>{commands.push(payload);return payload.cmd==='warehouse_lift'?{command_id:commands.length}:{task_id:`task-${commands.length}`};},publish:()=>{}});
  try {
    const order = service.create({pickups:[{pickup:'K1',quantity:1},{pickup:'K2',quantity:1}],
      deliveries:[{dropoff:'K3',quantity:1},{dropoff:'K4',quantity:1}]});
    await service.tick();await service.tick();
    assert.deepEqual(order.items.map(item=>item.robot).sort(),['01','02']);
    assert.equal(order.items.filter(item=>item.status === 'pickup_lift').length,2);
    assert.equal(commands.filter(command=>command.cmd==='warehouse_lift').length,2);
  } finally {await service.close();}
});

test('offline robot interrupts lift and cannot mark cargo delivered',async()=>{
  const db = {getOrders:()=>[],saveOrder:()=>{}};
  const robot = liftCapable({id:'01',node:'K1',state:'IDLE',online:true,position_valid:true,task_id:null});
  const state = {map,fleet:{agvs:[robot],tasks:[],accepting_tasks:true},shift:{phase:'RUNNING'},mqtt_connected:true};
  const service = createOrderService({db,getState:()=>state,getSession:()=> 'session-1',send:async()=>({command_id:88}),publish:()=>{}});
  try {
    const order = service.create({pickups:[{pickup:'K1',quantity:1}],deliveries:[{dropoff:'K3',quantity:1}]});
    await service.tick();await service.tick();assert.equal(order.items[0].status,'pickup_lift');
    robot.online = false;
    await service.tick();assert.equal(order.status,'needs_attention');assert.equal(order.items[0].status,'interrupted');
    assert.throws(()=>service.resolve(order.id,false),/Cần xác nhận/);
    assert.equal(service.resolve(order.id,true).status,'closed');
  } finally {await service.close();}
});
