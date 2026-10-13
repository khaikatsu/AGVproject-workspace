/* Simulation UI uses the shared form with multiple pickup and delivery warehouses. */
(async() => {
  await new Promise((resolve,reject)=>{const script=document.createElement('script');script.src='/sim-assets/motion.js';script.onload=resolve;script.onerror=reject;document.head.append(script);});
  const direction=globalThis.AgvSimMotion,headingViews=new Map();
  function displayedHeading(id,target,now){
    if(target==null)return null;
    let view=headingViews.get(id);
    if(!view){view={from:target,to:target,at:now};headingViews.set(id,view);}
    const current=view.from+(view.to-view.from)*Math.max(0,Math.min(1,(now-view.at)/650));
    if(direction.normalize(view.to)!==direction.normalize(target)){
      let delta=direction.normalize(target-current);if(delta>180)delta-=360;
      view={from:current,to:current+delta,at:now};headingViews.set(id,view);
    }
    return view.from+(view.to-view.from)*Math.max(0,Math.min(1,(now-view.at)/650));
  }
  const dashboardToast=toast;
  toast=(message,type)=>{if(!/Da xu ly lenh sim_(edge_obstacle|replan)/.test(message))dashboardToast(message,type);};
  const viewerId=crypto.randomUUID(),viewerUrl='/sim-api/viewer';
  const el=id=>document.getElementById(id),make=(tag,text,cls)=>{const n=document.createElement(tag);if(text!=null)n.textContent=text;if(cls)n.className=cls;return n;};
  const number=n=>Number(n||0).toLocaleString('vi-VN',{maximumFractionDigits:1}),edgeKey=(a,b)=>[a,b].sort().join('|');
  const freshMs=3000,requests=new Set();
  let snapshot=null,busy=false,lastSnapshotAt=0,lastSnapshotTime=null,edges=[],edgeDrawingKey='';
  let connected=false,pageActive=true,networkEpoch=0,failures=0,pollTimer=null,heartbeatTimer=null,heartbeatBusy=false;
  const isFresh=()=>pageActive&&connected&&navigator.onLine!==false&&!!snapshot&&performance.now()-lastSnapshotAt<freshMs;
  const dashboardControlsReady=controlsReady,dashboardDataFresh=dataFresh,dashboardUpdateConnection=updateConnection;
  dataFresh=()=>isFresh()&&dashboardDataFresh();
  controlsReady=()=>isFresh()&&snapshot.health==='Sẵn sàng'&&dashboardControlsReady();
  updateConnection=()=>{
    dashboardUpdateConnection();
    if(!isFresh()){el('status').textContent=navigator.onLine===false?'Web: Mất mạng':'Web: Chờ dữ liệu mô phỏng';el('status').className='status-indicator bad';}
    render();
  };
  async function call(route,body,timeoutMs=8000){
    const controller=new AbortController();requests.add(controller);
    const timer=setTimeout(()=>controller.abort(),timeoutMs);
    try{
      const r=await fetch('/sim-api'+route,{method:body?'POST':'GET',signal:controller.signal,cache:'no-store',headers:body?{'Content-Type':'application/json'}:undefined,body:body?JSON.stringify(body):undefined});
      let x;try{x=await r.json();}catch{throw Object.assign(Error('Phản hồi mô phỏng không hợp lệ'),{connectionError:true});}
      if(!r.ok)throw Error(x.error||'Thao tác thất bại');return x;
    }catch(error){
      if(controller.signal.aborted||error instanceof TypeError||error.connectionError){
        throw Object.assign(Error(body?'Chưa xác định kết quả lệnh. Kiểm tra trạng thái sau khi kết nối lại; lệnh không được tự gửi lại.':'Không nhận được dữ liệu mô phỏng'),{connectionError:true,unknown:!!body});
      }throw error;
    }finally{clearTimeout(timer);requests.delete(controller);}
  }
  function disconnected(){connected=false;updateConnection();}
  async function run(button,fn){
    if(button.dataset.simBusy)return;
    if(!controlsReady()||snapshot.health!=='Sẵn sàng'){toast('Chờ kết nối và dữ liệu mới trước khi điều khiển','warn');return;}
    button.dataset.simBusy='true';button.disabled=true;
    try{const x=await fn();toast(x?.message||'Đã cập nhật','ok');await poll();}
    catch(e){if(e.connectionError)disconnected();toast(e.message,e.unknown?'warn':'error');}
    finally{delete button.dataset.simBusy;render();}
  }
  const launcher=el('simOpenLauncher');
  if(document.body.dataset.simLauncher){launcher.href=document.body.dataset.simLauncher;}
  el('simRetryConnection').onclick=()=>{clearTimeout(pollTimer);if(!backendOnline)connect(true);poll();};
  el('wsUrl').readOnly=true;
  document.querySelectorAll('.card-help').forEach(n=>n.hidden=true);el('coreModeStatus').hidden=true;
  el('submitOrderBtn').textContent='Giao đơn cho 2 xe';
  const orderPanel=el('submitOrderBtn').closest('section');orderPanel.id='simWarehouseOrderPanel';
  orderPanel.querySelector('.card-header').textContent='Đơn hàng · Lấy → Giao';
  orderPanel.querySelectorAll('label').forEach(n=>{if(n.textContent.includes('Kho lấy'))n.textContent='Kho lấy hàng · số kiện';if(n.textContent.includes('Kho giao'))n.textContent='Kho giao hàng · số kiện';});
  orderPanel.querySelectorAll('button').forEach(n=>{if(n.textContent.includes('Thêm kho lấy'))n.textContent='+ Kho lấy';if(n.textContent.includes('Thêm kho giao'))n.textContent='+ Kho giao';});
  el('right-panel').prepend(el('startShiftBtn').closest('section'));
  el('right-panel').insertBefore(orderPanel,el('simObstaclePanel'));
  document.querySelector('.map-live').textContent='Hai xe ảo';
  document.querySelector('.map-foot').children[1].textContent='Trắng: đầu xe · Màu xe: hướng đi · Hướng/vị trí: mô phỏng';
  const metrics=make('section',null,'card compact sim-card');metrics.id='simMetrics';metrics.append(make('div','Kết quả','card-header'));
  const status=make('div','Đang mở mô phỏng','sim-status');status.id='simHealth';metrics.append(status);
  const grid=make('div',null,'sim-metric-grid');
  for(const [id,label] of [['simDistance','Quãng đường (cm)'],['simCost','Chi phí tuyến'],['simWait','Chờ (giây)'],['simCompleted','Task hoàn thành'],['simClock','Giây mô phỏng'],['simWall','Giây thực']]){const box=make('div'),value=make('strong','0');value.id=id;box.append(make('span',label),value);grid.append(box);}metrics.append(grid);
  const details=make('details',null,'sim-assumptions');details.append(make('summary','Nhật ký và báo cáo'));
  const downloads=make('div',null,'sim-downloads');for(const [url,label] of [['report','Báo cáo'],['events','Nhật ký']]){const a=make('a',label);a.href='/sim-api/'+url;a.download='';downloads.append(a);}details.append(downloads);
  const events=make('div');events.id='simEvents';details.append(events);metrics.append(details);el('left-panel').prepend(metrics);
  for(const id of ['01','02']){
    const section=make('details',null,'sim-vehicle');section.open=true;section.dataset.robot=id;
    const summary=make('summary','Xe '+id),cargo=make('span','Trống');cargo.id='simCargo'+id;summary.append(cargo);section.append(summary);
    const pos=make('div','Chờ kết nối','sim-position');pos.id='simPosition'+id;section.append(pos);
    const row=make('div',null,'sim-goto'),target=make('input');target.id='simTarget'+id;target.placeholder='Node đích';target.setAttribute('list','mapNodeOptions');target.setAttribute('aria-label','Đích xe '+id);
    const go=make('button','Đi tới','btn');go.id='simGoto'+id;go.onclick=()=>run(go,()=>call('/robots/'+id+'/goto',{target:target.value.trim()}));row.append(target,go);section.append(row);
    const actions=make('div',null,'sim-actions'),advanced=make('details',null,'sim-assumptions');advanced.append(make('summary','Pin và kết nối'));const more=make('div',null,'sim-actions');advanced.append(more);
    for(const [action,label,value] of [['pause','Dừng'],['resume','Tiếp tục'],['recover','Phục hồi'],['battery','Pin 15%',15],['battery','Pin 0%',0],['battery','Nạp 85%',85],['offline','Ngắt mạng'],['online','Kết nối'],['drop_ack','Mất ACK'],['unload','Gỡ hàng']]){
      const b=make('button',label,value===0?'btn btn-danger':'btn-outline');b.dataset.simAction=action;b.dataset.robot=id;if(value!=null)b.dataset.value=value;
      b.onclick=()=>run(b,()=>call('/robots/'+id+'/action',{action,...(value!=null?{value}:{})}));(['pause','resume','recover'].includes(action)?actions:more).append(b);
    }
    const hint=make('div','','sim-recovery');hint.id='simRecoveryHint'+id;section.append(actions,hint,advanced);el('simVehicleControls').append(section);
  }
  for(const [id,key] of [['simTimeScale','timeScale'],['simSpeed','speedCmS'],['simDrain','drainPerCm'],['simCharge','chargeRate'],['simService','serviceSeconds']])el(id).onchange=()=>run(el(id),()=>call('/settings',{[key]:Number(el(id).value)}));
  const edgeSelect=el('simObstacleEdge');
  function selectedEdge(){return edges.find(e=>edgeKey(e.from,e.to)===edgeSelect.value);}
  function changeObstacle(button,blocked,edge=selectedEdge()){if(edge)return run(button,()=>call('/edge-obstacles',{from:edge.from,to:edge.to,blocked}));}
  el('simBlockEdge').onclick=()=>changeObstacle(el('simBlockEdge'),true);el('simUnblockEdge').onclick=()=>changeObstacle(el('simUnblockEdge'),false);edgeSelect.onchange=()=>render();
  const ns='http://www.w3.org/2000/svg',motion=document.createElementNS(ns,'g'),obstacles=document.createElementNS(ns,'g');motion.id='simMotionG';obstacles.id='simEdgesG';el('mapSvg').append(obstacles,motion);
  function svg(tag,attrs,text){const n=document.createElementNS(ns,tag);for(const [k,v]of Object.entries(attrs))n.setAttribute(k,v);if(text)n.textContent=text;return n;}
  function render(){
    const s=snapshot,fresh=isFresh(),ready=controlsReady()&&s?.health==='Sẵn sàng';
    document.body.dataset.simConnection=fresh?'online':lastSnapshotTime||failures||navigator.onLine===false?'offline':'connecting';
    const connectionText=el('simConnectionText');
    connectionText.textContent=!fresh?(lastSnapshotTime?'Mất kết nối hoặc dữ liệu cũ. Lần nhận cuối: '+lastSnapshotTime.toLocaleTimeString('vi-VN')+'. Đang tự thử lại. Điều khiển đã khóa; vị trí và số liệu là dữ liệu cũ.':'Đang chờ dữ liệu mô phỏng. Điều khiển đã khóa; trang tự thử kết nối lại.'):
      !backendOnline?'Mất kết nối WebSocket. Đang tự kết nối lại; điều khiển đã khóa.':!dashboardControlsReady()||s.health!=='Sẵn sàng'?'Đã kết nối web, đang chờ Core / MQTT hoặc dữ liệu Core mới. Điều khiển đã khóa.':'Đã kết nối · Đang nhận dữ liệu mô phỏng mới.';
    if(!fresh&&navigator.onLine===false)connectionText.textContent='Trình duyệt mất mạng. Điều khiển đã khóa; dữ liệu đang hiển thị là dữ liệu cũ. Trang tự kết nối lại khi có mạng.';
    el('simRetryConnection').disabled=busy||navigator.onLine===false;
    launcher.hidden=fresh||!launcher.hasAttribute('href');el('simLauncherHint').hidden=launcher.hidden;
    for(const input of document.querySelectorAll('#simControlPanel input,#simControlPanel select'))input.disabled=!ready||!!input.dataset.simBusy;
    for(const button of document.querySelectorAll('[data-sim-action],.sim-obstacle-chip'))button.disabled=!ready||!!button.dataset.simBusy;
    for(const id of ['simBlockEdge','simUnblockEdge','simGoto01','simGoto02'])if(!ready)el(id).disabled=true;
    if(!s){status.textContent='Chờ kết nối mô phỏng';return;}
    el('homeStationList').hidden=lastState?.shift?.phase==='RUNNING';
    status.textContent=!fresh?'Dữ liệu cũ · Mất kết nối mô phỏng':!backendOnline?'Mất kết nối WebSocket':!dashboardControlsReady()?'Đang chờ Core / MQTT':s.health==='Sẵn sàng'&&s.robots.some(r=>!r.connected)?'Xe mất kết nối':s.health;
    for(const [id,key]of [['simTimeScale','timeScale'],['simSpeed','speedCmS'],['simDrain','drainPerCm'],['simCharge','chargeRate'],['simService','serviceSeconds']])if(document.activeElement!==el(id)&&!el(id).dataset.simBusy)el(id).value=s.settings[key];
    for(const [id,value]of [['simDistance',s.robots.reduce((a,r)=>a+r.distanceCm,0)],['simCost',s.robots.reduce((a,r)=>a+r.routeCost,0)],['simWait',s.robots.reduce((a,r)=>a+r.waitSeconds,0)],['simCompleted',s.totals.completed],['simClock',s.simSeconds],['simWall',s.wallSeconds]])el(id).textContent=number(value);
    const unique=new Map();for(const e of lastState?.map?.edges||[])if(!unique.has(edgeKey(e.from,e.to)))unique.set(edgeKey(e.from,e.to),e);edges=[...unique.values()].sort((a,b)=>edgeKey(a.from,a.to).localeCompare(edgeKey(b.from,b.to)));
    const signature=edges.map(e=>edgeKey(e.from,e.to)).join(',');if(edgeSelect.dataset.edges!==signature){const old=edgeSelect.value;edgeSelect.replaceChildren(...edges.map(e=>{const o=make('option',e.from+(e.two_way?' ↔ ':' → ')+e.to);o.value=edgeKey(e.from,e.to);return o;}));if(unique.has(old))edgeSelect.value=old;edgeSelect.dataset.edges=signature;}
    const blocked=new Set((s.blockedEdges||[]).map(e=>edgeKey(e.from,e.to)));el('simObstacleCount').textContent=blocked.size;
    el('simBlockEdge').disabled=!ready||!!el('simBlockEdge').dataset.simBusy||!selectedEdge()||blocked.has(edgeSelect.value);el('simUnblockEdge').disabled=!ready||!!el('simUnblockEdge').dataset.simBusy||!blocked.has(edgeSelect.value);
    const list=el('simBlockedEdges');list.replaceChildren();for(const e of s.blockedEdges||[]){const chip=make('button',e.from+' — '+e.to+' ×','sim-obstacle-chip');chip.type='button';chip.disabled=!ready;chip.setAttribute('aria-label','Gỡ vật cản '+e.from+' '+e.to);chip.onclick=()=>changeObstacle(chip,false,e);list.append(chip);}
    for(const r of s.robots){
      const a=lastState?.fleet?.agvs?.find(a=>a.id===r.id);el('simCargo'+r.id).textContent=r.cargo?'Có hàng':'Trống';
      const movement=direction.describe(r,lastState?.map||{},s.warehouseOrders,s.jobs);
      const state=r.fault||(r.blockedEdge?'Chờ gỡ vật cản':r.paused?'Tạm dừng':r.waitingZone?'Chờ '+r.waitingZone:r.routeUpdatePending?'Đợi RFID để đổi tuyến':r.charging?'Sạc':a?.state==='BLOCKED'?'Chờ đường':movement.label|| (r.edge?'Đang chạy':'Sẵn sàng'));
      el('simPosition'+r.id).textContent=r.node+(r.edge?' → '+r.edge.to:'')+' · '+number(r.battery)+'% · '+(!fresh?'Dữ liệu cũ':r.connected?'Online':'Offline')+'\n'+state;
      el('simRecoveryHint'+r.id).textContent=!r.connected?'Mất mạng: bấm Phục hồi.':r.fault||a?.state==='ERROR'?'Xe đang khóa lỗi; kiểm tra pin rồi Phục hồi.':'';
      el('simGoto'+r.id).disabled=!ready||!!el('simGoto'+r.id).dataset.simBusy||!r.connected||lastState?.shift?.phase!=='RUNNING'||a?.state!=='IDLE'||r.paused||!!r.fault||r.cargo||(s.warehouseOrders||[]).some(o=>o.status==='active'&&o.items.some(i=>i.robot===r.id&&!['queued','delivered','interrupted'].includes(i.status)));
    }
    events.replaceChildren(...s.events.slice(-12).reverse().map(e=>make('div',number(e.simSeconds)+'s · '+e.type+(e.robot?' · xe '+e.robot:'')+(e.from?' · '+e.from+' → '+e.to:'')+(e.message?' · '+e.message:''))));
  }
  function animate(){
    motion.replaceChildren();
    if(snapshot&&lastState?.map?.nodes){
      const t=computeMapTransform(lastState.map.nodes),positions=new Map(lastState.map.nodes.map(n=>[n.id,{x:n.x*t.scale+t.ox,y:n.y*t.scale+t.oy}])),blocked=new Set((snapshot.blockedEdges||[]).map(e=>edgeKey(e.from,e.to)));
      const drawingKey=JSON.stringify([t,edges.map(e=>edgeKey(e.from,e.to)),[...blocked],edgeSelect.value]);
      if(drawingKey!==edgeDrawingKey){edgeDrawingKey=drawingKey;obstacles.replaceChildren();for(const e of edges){
        const a=positions.get(e.from),b=positions.get(e.to);if(!a||!b)continue;const length=Math.hypot(b.x-a.x,b.y-a.y),cut=Math.min(22/length,.2),key=edgeKey(e.from,e.to),g=svg('g',{'data-sim-edge':key});
        const x1=a.x+(b.x-a.x)*cut,y1=a.y+(b.y-a.y)*cut,x2=b.x-(b.x-a.x)*cut,y2=b.y-(b.y-a.y)*cut,nx=-(b.y-a.y)/length*10,ny=(b.x-a.x)/length*10;
        g.append(svg('line',{x1,y1,x2,y2,stroke:blocked.has(key)?'#ef4444':key===edgeSelect.value?'#38bdf8':'transparent','stroke-width':blocked.has(key)?8:5,'pointer-events':'none'}));
        const hit=svg('polygon',{points:`${x1+nx},${y1+ny} ${x2+nx},${y2+ny} ${x2-nx},${y2-ny} ${x1-nx},${y1-ny}`,fill:'transparent',class:'sim-edge-hit'});
        hit.addEventListener('click',event=>{event.stopPropagation();edgeSelect.value=key;render();});g.append(hit);
        if(blocked.has(key)){const x=(a.x+b.x)/2,y=(a.y+b.y)/2;g.append(svg('rect',{x:x-11,y:y-11,width:22,height:22,rx:4,fill:'#ef4444',class:'sim-obstacle-marker'}),svg('text',{x,y:y+5,fill:'white','text-anchor':'middle','font-size':17,'font-weight':700,class:'sim-obstacle-marker'},'×'));}obstacles.append(g);
      }}
      for(const r of snapshot.robots){
        const a=positions.get(r.node),b=r.edge&&positions.get(r.edge.to);if(!a)continue;const age=isFresh()&&dashboardControlsReady()?Math.min(.5,(performance.now()-lastSnapshotAt)/1000):0,p=r.edge?Math.min(1,r.edge.progress+(!r.paused&&!r.fault&&!r.blockedEdge&&r.connected&&r.path.length?age*snapshot.settings.timeScale*snapshot.settings.speedCmS/r.edge.distance:0)):0;
        const pos=b?{x:a.x+(b.x-a.x)*p,y:a.y+(b.y-a.y)*p}:a,color=r.id==='01'?'#38bdf8':'#c084fc',g=svg('g',{'data-sim-robot':r.id,transform:'translate('+pos.x+','+pos.y+')'});const core=document.querySelector('#agvsG [data-robot="'+r.id+'"]');if(core)core.style.opacity='0';
        const movement=direction.describe(r,lastState.map,snapshot.warehouseOrders,snapshot.jobs,headingViews.get(r.id)?.to),heading=displayedHeading(r.id,movement.heading,isFresh()?performance.now():lastSnapshotAt);
        g.setAttribute('data-heading',movement.heading??'unknown');g.setAttribute('data-travel',movement.travel??'stopped');g.setAttribute('data-phase',movement.phase);g.setAttribute('data-reverse',movement.reverse);
        g.append(svg('circle',{r:22,fill:color,opacity:'.12'}));
        const body=svg('g',{transform:'rotate('+(heading??0)+')',class:'sim-robot-body'});
        body.append(svg('rect',{x:-12,y:-12,width:24,height:24,rx:5,fill:r.fault?'#ef4444':color,stroke:'#e2e8f0','stroke-width':2}));
        if(r.cargo)body.append(svg('rect',{x:-8,y:-4,width:7,height:8,rx:1,fill:'#fbbf24'}));
        if(heading!=null)body.append(svg('path',{d:'M 3 -6 L 10 0 L 3 6',fill:'none',stroke:'white','stroke-width':3,'stroke-linecap':'round','stroke-linejoin':'round',class:'sim-heading-arrow'}));g.append(body);
        if(isFresh()&&dashboardControlsReady()&&movement.travel!=null){const arrow=svg('g',{transform:'rotate('+movement.travel+')',class:'sim-travel-arrow'});arrow.append(svg('line',{x1:17,y1:0,x2:34,y2:0,stroke:color,'stroke-width':3}),svg('path',{d:'M 30 -5 L 39 0 L 30 5 Z',fill:color,stroke:'#0b1626','stroke-width':1}));g.append(arrow);}
        g.append(svg('text',{x:0,y:-29,fill:color,'text-anchor':'middle','font-size':15,'font-weight':700},r.id));
        const turning=!r.edge&&lastState.map.nodes.find(n=>n.id===r.node)?.type==='charging'&&heading!=null&&Math.abs(heading-(headingViews.get(r.id)?.to??heading))>.1;
        const label=turning?'Quay đầu':movement.label;
        if(label){const width=label.length*6+14;g.append(svg('rect',{x:-width/2,y:22,width,height:20,rx:5,fill:'#0b1626',stroke:color,'stroke-width':1}),svg('text',{x:0,y:36,fill:'#e2e8f0','text-anchor':'middle','font-size':11,class:'sim-motion-label'},label));}
        motion.append(g);
      }
    }
    requestAnimationFrame(animate);
  }
  function validSnapshot(s){
    return s?.simulation===true&&Number.isFinite(s.started)&&typeof s.health==='string'&&s.settings&&
      ['timeScale','speedCmS','drainPerCm','chargeRate','serviceSeconds'].every(k=>Number.isFinite(s.settings[k]))&&
      Number.isFinite(s.simSeconds)&&Number.isFinite(s.wallSeconds)&&Number.isFinite(s.totals?.completed)&&
      Array.isArray(s.robots)&&s.robots.length===2&&['01','02'].every(id=>s.robots.some(r=>r?.id===id&&typeof r.node==='string'&&typeof r.connected==='boolean'&&Array.isArray(r.path)&&
        ['battery','distanceCm','routeCost','waitSeconds'].every(k=>Number.isFinite(r[k]))&&(!r.edge||typeof r.edge.to==='string'&&Number.isFinite(r.edge.progress)&&r.edge.distance>0)))&&
      Array.isArray(s.events)&&s.events.every(e=>e&&typeof e==='object')&&
      ['jobs','warehouseOrders','blockedEdges'].every(k=>s[k]==null||Array.isArray(s[k])&&s[k].every(x=>x&&typeof x==='object'));
  }
  async function poll(){
    if(busy||!pageActive||navigator.onLine===false)return;
    clearTimeout(pollTimer);busy=true;const epoch=networkEpoch;render();
    try{
      const next=await call('/state',null,2500);
      if(epoch!==networkEpoch||!pageActive)return;
      if(!validSnapshot(next))throw Error('Snapshot mô phỏng không hợp lệ');
      if(snapshot&&snapshot.started!==next.started)headingViews.clear();
      snapshot={...next,jobs:next.jobs||[],warehouseOrders:next.warehouseOrders||[],blockedEdges:next.blockedEdges||[]};lastSnapshotAt=performance.now();lastSnapshotTime=new Date();connected=true;failures=0;
    }catch{if(epoch===networkEpoch){connected=false;failures++;}}
    finally{busy=false;updateConnection();if(pageActive&&navigator.onLine!==false)pollTimer=setTimeout(poll,failures?Math.min(4000,250*2**Math.min(failures,4)):250);}
  }
  async function heartbeat(){
    if(heartbeatBusy||!pageActive||navigator.onLine===false)return;heartbeatBusy=true;
    try{await call('/viewer',{id:viewerId},2500);}catch{}finally{heartbeatBusy=false;}
  }
  function suspend(){networkEpoch++;connected=false;clearTimeout(pollTimer);for(const controller of requests)controller.abort();}
  addEventListener('offline',()=>{suspend();updateConnection();});
  addEventListener('online',()=>{if(pageActive){heartbeat();if(!backendOnline)connect(true);poll();}});
  addEventListener('pagehide',()=>{pageActive=false;suspend();clearInterval(heartbeatTimer);navigator.sendBeacon(viewerUrl,new Blob([JSON.stringify({id:viewerId,leaving:true})],{type:'application/json'}));});
  addEventListener('pageshow',e=>{if(e.persisted){pageActive=true;networkEpoch++;heartbeat();clearInterval(heartbeatTimer);heartbeatTimer=setInterval(heartbeat,4000);if(!backendOnline)connect(true);poll();}});
  render();heartbeat();heartbeatTimer=setInterval(heartbeat,4000);poll();requestAnimationFrame(animate);
})();
