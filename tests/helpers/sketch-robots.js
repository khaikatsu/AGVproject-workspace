// Hai receiver dùng trực tiếp .ino host. Chỉ I/O RC522/NVS/WiFi được adapter.
const path=require('path'),fs=require('fs'),readline=require('readline');
const tags=require('../fixtures/rfid-tags.json');
function sketchRobots({root,child,mqtt,map,ids}) {
  const executable=process.env.AGV_SKETCH_PROBE || path.join(root,'.qa/firmware-host-build/sketch_probe.exe');
  if (!fs.existsSync(executable)) throw new Error('Run scripts/test-firmware.ps1 first');
  const devices=new Map(),stats=new Map(),events=[];
  const drive=(id,event)=>devices.get(id).stdin.write(JSON.stringify(event)+'\n');
  for (const id of ids) {
    const process=child(executable,['--robot',id]);devices.set(id,process);
    readline.createInterface({input:process.stdout}).on('line',line=>{
      const event=JSON.parse(line);events.push({robot:id,...event});
      if (event.type==='publish') {
        if (event.retained && !event.topic.endsWith('/status')) throw new Error('Movement/telemetry must not be retained');
        mqtt.publish(event.topic,event.payload,event.retained);
      } else if (event.type==='stats') stats.set(id,event);
    });
  }
  return {
    stats,events,
    receive(id,topic,payload) {if(devices.has(id)) drive(id,{type:'mqtt',topic,payload});},
    serial(id,line) {drive(id,{type:'serial',line});},
    scan(id,node) {
      if (!map.nodes.some(n=>n.id===node) || !tags[node]) throw new Error('Unknown fixture node');
      drive(id,{type:'rfid',uid:tags[node]});
    },
    telemetry() {for(const id of ids) drive(id,{type:'telemetry'});},
    resetLocalization() {for(const id of ids) drive(id,{type:'rfid_reset'});}
  };
}
module.exports={sketchRobots};
