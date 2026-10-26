'use strict';

let locationNamesFormKey = '';
function locationName(id,config = lastState?.location_names) {return config?.labels?.[id] || id || '—';}
function locationText(id,config = lastState?.location_names) {
  const name=locationName(id,config);return name===id || !id ? name : `${name} (vị trí ${id})`;
}
function locationRequest(method,route,body) {
  if(method!=='POST' || !body || !lastState?.location_names)return body;
  if(route==='/api/tasks' || /^\/api\/agvs\/[^/]+\/goto$/.test(route))
    return {...body,location_names_revision:lastState.location_names.revision};
  if(route==='/api/orders')return {...body,location_names_revision:lastState.location_names.revision,
    pickups:body.pickups.map(line=>({...line,pickup:locationName(line.pickup)})),
    deliveries:body.deliveries.map(line=>({...line,dropoff:locationName(line.dropoff)}))};
  if(/^\/api\/agvs\/[^/]+\/warehouse-action$/.test(route))
    return {...body,warehouse:locationName(body.warehouse),location_names_revision:lastState.location_names.revision};
  return body;
}
function renderLocationNames() {
  const config=lastState?.location_names;
  const key=JSON.stringify([config?.revision,config?.labels,lastState?.map?.nodes.map(node=>[node.id,node.type])]);
  const first=$('locationNameFirst'),second=$('locationNameSecond');
  if(locationNamesFormKey!==key) {
    locationNamesFormKey=key;
    const prior=first.value;first.replaceChildren();
    for(const node of lastState?.map?.nodes || [])if(config?.labels?.[node.id] &&
      (node.id.startsWith('K')&&node.type==='station' || node.id.startsWith('S')&&node.type==='charging'))
      first.add(new Option(locationText(node.id),locationName(node.id)));
    if([...first.options].some(option=>option.value===prior))first.value=prior;
    updateLocationNameChoices();
  }
  const enabled=controlsReady() && config?.can_edit===true;
  first.disabled=second.disabled=!enabled || $('swapLocationNamesBtn').dataset.busy==='true';
  $('swapLocationNamesBtn').disabled=!enabled || !first.value || !second.value || $('swapLocationNamesBtn').dataset.busy==='true';
  $('swapLocationNamesBtn').title=!config?'Khởi động lại gateway để dùng chức năng đổi tên.':
    !controlsReady()?'Chờ kết nối và dữ liệu Core mới.':config.reason || '';
}
function updateLocationNameChoices() {
  const first=$('locationNameFirst'),second=$('locationNameSecond'),prior=second.value;
  second.replaceChildren();
  for(const option of first.options)if(option.value!==first.value && option.value[0]===first.value[0])
    second.add(new Option(option.text,option.value));
  if([...second.options].some(option=>option.value===prior))second.value=prior;
}
async function swapLocationNames() {
  const body={first:$('locationNameFirst').value,second:$('locationNameSecond').value,
    expected_revision:lastState?.location_names?.revision};
  if(!body.first || !body.second || !lastState?.location_names?.can_edit)return;
  const result=await control('POST','/api/map/location-names/swap',body,$('swapLocationNamesBtn'));
  if(!result?.location_names)return;
  if(result.location_names.revision >= (lastState.location_names?.revision ?? 0))lastState.location_names=result.location_names;
  cancelSelection();$('task_target').value='';
  render(lastState);updateConnection();
  toast(`Đã hoán đổi tên ${body.first} ↔ ${body.second}.`);
}
