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
module.exports={cargoReading,cargoResult};
