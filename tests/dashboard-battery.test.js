const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');

// Exercise the actual dashboard functions without starting the operational gateway/DB.
const source=fs.readFileSync(path.join(__dirname,'../AGVwebserver/public/dashboard.js'),'utf8');
const functions=source.slice(source.indexOf('function populateTaskAgvSelect(agvs)'),source.indexOf('function render(state)'))+
  source.slice(source.indexOf('function renderShift(shift)'),source.indexOf('async function changeShift('));
function fixture(state) {
  const elements=new Map();
  function element() {return {value:'',options:[],children:[],style:{},replaceChildren(...items){this.children=items;this.options=items;},
    add(item){this.options.push(item);},append(item){this.children.push(item);}};}
  const get=id=>{if(!elements.has(id)) elements.set(id,element());return elements.get(id);};
  const context={lastState:state,$:get,labels:{},colors:{IDLE:'green',BLOCKED:'orange'},
    document:{createElement:element},Option:function(text,value){this.text=text;this.value=value;this.disabled=false;}};
  vm.runInNewContext(functions,context);
  context.populateTaskAgvSelect(state.fleet.agvs);context.renderShift(state.shift);
  return get;
}
for(const active of [['01'],['02'],['01','02']]) {
  test(`dashboard shows temporary battery bypass for ${active.join('+')}, preserving scope`,()=>{
    const state={fleet:{battery_gate_enabled:false,automatic_charging_enabled:false,agvs:['01','02'].map(id=>({
      id,state:'IDLE',online:true,position_valid:true,participating:active.includes(id),battery:0}))},
      shift:{supported:true,operation_profile:'scoped',active_robot_ids:active,phase:'CLOSED',can_start:false,robots:[]}};
    const ui=fixture(state);
    assert.match(ui('taskBatteryHint').textContent,/tạm bỏ chốt pin/);
    assert.match(ui('coreModeStatus').textContent,/sạc thủ công/);
    assert.match(ui('coreModeStatus').textContent,/mutex giữ nguyên/);
    assert.match(ui('homeStationList').children.at(-1).textContent,/Pin đang không chặn/);
    for(const option of ui('task_agv').options.filter(option=>option.value))
      assert.equal(option.disabled,!active.includes(option.value));
  });
}
test('dashboard keeps old Core behavior when new flags are absent',()=>{
  const ui=fixture({fleet:{agvs:[]},shift:{supported:true,operation_profile:'scoped',active_robot_ids:['01'],
    phase:'CLOSED',can_start:false,robots:[]}});
  assert.match(ui('taskBatteryHint').textContent,/pin 20%\/40%/);
  assert.doesNotMatch(ui('coreModeStatus').textContent,/tạm bỏ chốt pin/);
});
