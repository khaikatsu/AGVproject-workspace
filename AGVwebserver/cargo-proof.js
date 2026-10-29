'use strict';

// The Core validates session, age and STOP; the gateway checks its live proof.
function cargoReading(robot,node) {
  const sensor=robot?.cargo_sensor;
  if(sensor?.required!==true)return {required:false,known:false,present:null};
  const known=sensor.supported===true&&sensor.fresh===true&&sensor.stable===true&&
    sensor.observed_node===node&&sensor.stopped_verified===true&&typeof sensor.present==='boolean';
  return {required:true,known,present:known?sensor.present:null};
}
function cargoResult(robot,node,expected) {
  const value=cargoReading(robot,node);
  return !value.required||value.known&&value.present===expected;
}
function loweredForExit(robot,node,commandId) {
  const lift=robot?.lift;
  return lift?.dropoff_exit_supported===true&&lift.dropoff_exit_pending===true&&lift.fresh===true&&
    lift.active===false&&lift.state==='LOWERED'&&lift.command_id===commandId&&lift.node===node&&
    lift.observed_node===node&&robot.node===node&&robot.online===true&&robot.position_valid===true&&
    robot.state==='IDLE'&&!robot.error&&!robot.task_id&&
    robot.virtual_cargo_known===true&&robot.virtual_cargo===true&&cargoReading(robot,node).known;
}
function dropoffDoneAtGate(robot,node,commandId) {
  const gate=node.replace(/^K/,'G'),lift=robot?.lift;
  return lift?.dropoff_exit_supported===true&&lift.dropoff_exit_pending===false&&lift.fresh===true&&
    lift.active===false&&lift.state==='DONE_DOWN'&&lift.command_id===commandId&&lift.node===node&&
    lift.observed_node===gate&&robot.node===gate&&robot.online===true&&robot.position_valid===true&&
    robot.state==='IDLE'&&!robot.error&&!robot.task_id&&
    robot.virtual_cargo_known===true&&robot.virtual_cargo===false&&cargoReading(robot,gate).known&&cargoResult(robot,gate,false);
}
module.exports={cargoReading,cargoResult,loweredForExit,dropoffDoneAtGate};
