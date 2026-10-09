/* Add-on for the cloned dashboard. The production dashboard/API is reused unchanged. */
(() => {
  // A tab lease keeps the isolated Core/broker alive only while someone views this page.
  const viewerId = globalThis.crypto?.randomUUID?.() || `${Date.now()}-${Math.random().toString(36).slice(2)}-viewer`;
  const viewerUrl = '/sim-api/viewer';
  const heartbeat = () => fetch(viewerUrl,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:viewerId}),keepalive:true}).catch(()=>{});
  heartbeat();
  let heartbeatTimer = setInterval(heartbeat,4000);
  addEventListener('pagehide',()=>{
    clearInterval(heartbeatTimer);
    navigator.sendBeacon(viewerUrl,new Blob([JSON.stringify({id:viewerId,leaving:true})],{type:'application/json'}));
  });
  addEventListener('pageshow',event=>{
    if (event.persisted) {heartbeat();heartbeatTimer = setInterval(heartbeat,4000);}
  });
  const el = id=>document.getElementById(id),make = (tag,text,cls)=>{
    const node = document.createElement(tag);if (text != null) node.textContent = text;if (cls) node.className = cls;return node;
  };
  let snapshot = null,busy = false,lastSnapshotAt = 0;
  el('wsUrl').readOnly = true;
  el('wsUrl').title = 'Bản mô phỏng luôn nối gateway riêng trên cùng địa chỉ web';
  const phaseNames = {to_pickup:'Đến lấy hàng',pickup_service:'Đang lấy hàng',to_dropoff:'Đến trả hàng',dropoff_service:'Đang trả hàng',completed:'Hoàn thành',interrupted:'Gián đoạn',failed:'Lỗi'};
  const orderStatusNames = {active:'Đang giao',completed:'Đã giao đủ',needs_attention:'Cần đối soát'};
  const number = n=>Number(n || 0).toLocaleString('vi-VN',{maximumFractionDigits:1});
  const seconds = n=>number(n)+' s';
  async function call(route,body) {
    const response = await fetch('/sim-api'+route,{method:body ? 'POST' : 'GET',headers:body ? {'Content-Type':'application/json'} : undefined,body:body ? JSON.stringify(body) : undefined});
    const result = await response.json();if (!response.ok) throw new Error(result.error || 'Thao tác thất bại');return result;
  }
  async function run(button,fn) {
    if (button.dataset.simBusy === 'true') return;
    button.dataset.simBusy = 'true';button.disabled = true;
    try {const result = await fn();toast(result?.message || 'Mô phỏng đã nhận thao tác',result?.warning ? 'warn' : 'ok');await poll();}
    catch (error) {toast(error.message,'error');}
    finally {delete button.dataset.simBusy;button.disabled = false;}
  }
  const metrics = make('section',null,'card compact sim-card');metrics.id = 'simMetrics';
  metrics.append(make('div','Kết quả mô phỏng','card-header'));
  const status = make('div','Đang khởi động','sim-status');status.id = 'simHealth';metrics.append(status);
  const grid = make('div',null,'sim-metric-grid');
  for (const [id,label] of [['simDistance','Quãng đường (cm)'],['simCost','Chi phí trọng số'],['simWait','Chờ mutex (giây mô phỏng)'],['simCompleted','Task Core hoàn thành'],['simClock','Thời gian mô phỏng'],['simWall','Thời gian thực']]) {
    const item = make('div');item.append(make('span',label));const value = make('strong','0');value.id = id;item.append(value);grid.append(item);
  }
  metrics.append(grid);const downloads = make('div',null,'sim-downloads');
  for (const [url,label] of [['report','Báo cáo JSON'],['events','Nhật ký JSONL']]) {const a = make('a',label);a.href = '/sim-api/'+url;a.download = '';downloads.append(a);}
  metrics.append(downloads,make('p','Quãng đường từ tọa độ giả định cm; trọng số do Core dùng tìm đường.','card-help'));
  const jobs = make('div');jobs.id = 'simJobs';metrics.append(jobs);
  const taskDetails = make('details');taskDetails.append(make('summary','Thời gian từng task Core'));
  const taskMetrics = make('div');taskMetrics.id = 'simTaskMetrics';taskDetails.append(taskMetrics);metrics.append(taskDetails);
  const details = make('details');details.append(make('summary','Nhật ký mô phỏng'));const events = make('div');events.id = 'simEvents';details.append(events);metrics.append(details);
  el('left-panel').prepend(metrics);
  const control = el('simVehicleControls');
  const orderLines = el('simOrderLines'),orderList = el('simOrders');
  function addOrderLine(destination = '',quantity = 1) {
    const row = make('div',null,'sim-order-line'),select = make('select'),count = make('input'),remove = make('button','×','btn-outline');
    row.dataset.preferred = destination;
    select.className = 'sim-order-destination';select.setAttribute('aria-label','Kho giao hàng');
    select.onchange = ()=>{row.dataset.preferred = select.value;};
    count.className = 'sim-order-quantity';count.type = 'number';count.min = '1';count.max = '20';count.step = '1';count.value = String(quantity);count.setAttribute('aria-label','Số kiện');
    remove.type = 'button';remove.title = 'Bỏ điểm giao';remove.setAttribute('aria-label','Bỏ điểm giao');
    remove.onclick = ()=>{row.remove();updateOrderTotal();};count.oninput = updateOrderTotal;
    row.append(select,count,remove);orderLines.append(row);
    const warehouses = (lastState?.map?.nodes || []).filter(node=>node.type === 'station').map(node=>node.id).sort();
    syncWarehouseSelect(select,warehouses,destination || warehouses[1] || warehouses[0]);
    updateOrderTotal();return row;
  }
  function syncWarehouseSelect(select,warehouses,preferred) {
    if (select.dataset.nodes === warehouses.join(',')) return;
    const old = select.value;
    select.replaceChildren(...warehouses.map(id=>{const option = make('option',id);option.value = id;return option;}));
    select.value = warehouses.includes(old) ? old : warehouses.includes(preferred) ? preferred : warehouses[0] || '';
    select.dataset.nodes = warehouses.join(',');
  }
  function updateOrderTotal() {
    const total = [...orderLines.querySelectorAll('.sim-order-quantity')].reduce((sum,input)=>sum+(Number(input.value) || 0),0);
    el('simOrderTotal').textContent = `${total} kiện`;
  }
  addOrderLine('K2',2);addOrderLine('K4',3);
  el('simAddOrderLine').onclick = ()=>addOrderLine();
  const createOrderButton = el('simCreateOrder');
  createOrderButton.onclick = ()=>run(createOrderButton,async()=>{
    const deliveries = [...orderLines.querySelectorAll('.sim-order-line')].map(row=>({dropoff:row.querySelector('select').value,quantity:Number(row.querySelector('input').value)}));
    const result = await call('/orders',{pickup:el('simOrderPickup').value,deliveries});
    return {message:`Đã tạo ${result.order.id}: ${result.order.total} kiện. Hai xe sẽ nhận việc khi rảnh.`};
  });
  for (const id of ['01','02']) {
    const section = make('details',null,'sim-vehicle');section.open = true;section.dataset.robot = id;
    const summary = make('summary','AGV '+id+' · ẢO');const cargo = make('span','Không hàng');cargo.id = 'simCargo'+id;summary.append(cargo);section.append(summary);
    const pos = make('div','Chờ kết nối','sim-position');pos.id = 'simPosition'+id;section.append(pos);
    const goto = make('div',null,'sim-goto'),destination = make('input');destination.id = 'simTarget'+id;destination.placeholder = 'Node đích, ví dụ K2';destination.setAttribute('list','mapNodeOptions');destination.setAttribute('aria-label','Đích riêng xe '+id);
    const go = make('button','Đi tới','btn');go.id = 'simGoto'+id;go.onclick = ()=>run(go,()=>call(`/robots/${id}/goto`,{target:destination.value.trim()}));goto.append(destination,go);section.append(goto);
    const pair = make('div',null,'sim-pair');
    for (const [kind,label] of [['pickup','Lấy tại'],['dropoff','Trả tại']]) {
      const box = make('div'),select = make('select');select.id = 'sim'+kind+id;select.setAttribute('aria-label',label+' xe '+id);
      box.append(make('label',label),select);pair.append(box);
    }
    section.append(pair);const jobButton = make('button','Lấy → trả hàng','btn full-width');jobButton.id = 'simJob'+id;
    jobButton.onclick = ()=>run(jobButton,()=>call('/jobs',{robot:id,pickup:el('simpickup'+id).value,dropoff:el('simdropoff'+id).value}));section.append(jobButton);
    const actions = make('div',null,'sim-actions');
    for (const [action,label,value] of [['battery','Pin thấp 15%',15],['battery','Hết pin 0%',0],['battery','Nạp pin 85%',85],['recover','Phục hồi xe / RFID'],['pause','Tạm dừng xe'],['resume','Tiếp tục xe'],['offline','Ngắt MQTT'],['online','Kết nối lại'],['drop_ack','Mất 1 ACK'],['unload','Gỡ hàng ảo']]) {
      const button = make('button',label,value === 0 ? 'btn btn-danger' : 'btn-outline');button.dataset.simAction = action;button.dataset.robot = id;
      if (value != null) button.dataset.value = String(value);
      button.onclick = ()=>run(button,()=>call(`/robots/${id}/action`,{action,...(value != null ? {value} : {})}));actions.append(button);
    }
    const hint = make('p','', 'card-help');hint.id = 'simRecoveryHint'+id;
    section.append(actions,hint,make('p','15%: Core cho về sạc khi xe không lỗi/dừng. Đang chạy thì áp dụng tại RFID kế tiếp. 0%: dừng lỗi; cần nạp pin rồi Phục hồi xe / RFID. Phục hồi xác nhận xe ảo tại node đọc gần nhất, không tự quét RFID thật.','card-help'));
    control.append(section);
  }
  for (const [id,key] of [['simTimeScale','timeScale'],['simSpeed','speedCmS'],['simDrain','drainPerCm'],['simCharge','chargeRate'],['simService','serviceSeconds']]) {
    el(id).onchange = ()=>run(el(id),()=>call('/settings',{[key]:Number(el(id).value)}));
  }
  // Keep the existing shift controls first so the user can start work immediately.
  const shift = el('startShiftBtn').closest('section');el('right-panel').prepend(shift);
  document.querySelector('.map-live').textContent = 'Xe ảo · nội suy';
  document.querySelector('.map-foot').children[1].textContent = 'Xe ảo: nội suy mô phỏng · Dấu vuông mờ: RFID Core nhận · Nét đứt: kế hoạch tuyến · Số trên cạnh: trọng số';
  const svgNS = 'http://www.w3.org/2000/svg',motion = document.createElementNS(svgNS,'g');motion.id = 'simMotionG';el('mapSvg').append(motion);
  function svg(tag,attrs,text) {const node = document.createElementNS(svgNS,tag);for (const [k,v] of Object.entries(attrs)) node.setAttribute(k,v);if (text) node.textContent = text;return node;}
  function render() {
    const s = snapshot;if (!s) return;
    for (const [id,key] of [['simTimeScale','timeScale'],['simSpeed','speedCmS'],['simDrain','drainPerCm'],['simCharge','chargeRate'],['simService','serviceSeconds']]) {
      const input = el(id);
      if (document.activeElement !== input && input.dataset.simBusy !== 'true') input.value = String(s.settings[key]);
    }
    const displayHealth = s.health === 'Sẵn sàng' && s.robots.some(robot=>!robot.connected)
      ? 'Xe ảo mất kết nối MQTT' : s.health;
    status.textContent = `${displayHealth} · Web ${s.ports.web} · MQTT ${s.ports.mqtt} · Core ${s.ports.core}`;
    for (const [id,value] of [['simDistance',s.robots.reduce((a,r)=>a+r.distanceCm,0)],['simCost',s.robots.reduce((a,r)=>a+r.routeCost,0)],['simWait',s.robots.reduce((a,r)=>a+r.waitSeconds,0)],['simCompleted',s.totals.completed],['simClock',s.simSeconds],['simWall',s.wallSeconds]]) el(id).textContent = number(value);
    const warehouses = (lastState?.map?.nodes || []).filter(n=>n.type === 'station').map(n=>n.id).sort();
    syncWarehouseSelect(el('simOrderPickup'),warehouses,'K1');
    for (const [index,row] of [...orderLines.querySelectorAll('.sim-order-line')].entries())
      syncWarehouseSelect(row.querySelector('select'),warehouses,row.dataset.preferred || warehouses[index+1] || warehouses[0]);
    createOrderButton.disabled = createOrderButton.dataset.simBusy === 'true' || s.health !== 'Sẵn sàng' || lastState?.shift?.phase !== 'RUNNING';
    for (const r of s.robots) {
      const state = lastState?.fleet?.agvs?.find(a=>a.id === r.id);
      el('simCargo'+r.id).textContent = r.cargo ? 'Có hàng' : 'Không hàng';
      const recoveryHint = !r.connected ? 'MQTT offline: Phục hồi xe / RFID sẽ kết nối lại và xác nhận vị trí.' : r.fault || state?.state === 'ERROR' ? 'Xe đang khóa lỗi: đổi pin chưa xóa lỗi. Nạp pin nếu còn 0%, rồi Phục hồi xe / RFID.' : r.paused ? 'Xe đang tạm dừng: bấm Tiếp tục xe để chạy theo lệnh Core.' : r.pendingBattery != null ? `Đợi RFID ${r.edge?.to}: sẽ gửi pin ${r.pendingBattery}% rồi Core quyết định về sạc.` : state?.state === 'CHARGING_EN_ROUTE' ? `Core đang cho về sạc: ${state.target || r.home}.` : '';
      el('simRecoveryHint'+r.id).textContent = recoveryHint;
      el('simPosition'+r.id).textContent = `${r.node}${r.edge ? ' → '+r.edge.to : ''} · ${number(r.battery)}% · ${r.connected ? 'MQTT online' : 'MQTT offline'}\n${r.fault || (r.paused ? 'Đang tạm dừng' : r.waitingZone ? 'Chờ quyền '+r.waitingZone : r.charging ? 'Đang sạc' : 'Hoạt động')} · ${number(r.distanceCm)} cm / cost ${number(r.routeCost)}`;
      for (const kind of ['pickup','dropoff']) {
        const select = el('sim'+kind+r.id);
        if (select.dataset.nodes !== warehouses.join(',')) {
          const old = select.value;select.replaceChildren(...warehouses.map(id=>{const option = make('option',id);option.value = id;return option;}));
          select.value = warehouses.includes(old) ? old : warehouses[(kind === 'pickup' ? 0 : 1)+(r.id === '02' ? 2 : 0)] || warehouses[0];select.dataset.nodes = warehouses.join(',');
        }
      }
      el('simJob'+r.id).disabled = el('simJob'+r.id).dataset.simBusy === 'true' || s.health !== 'Sẵn sàng' || lastState?.shift?.phase !== 'RUNNING' || state?.state !== 'IDLE' || r.paused || !!r.fault || r.cargo || s.jobs.some(j=>j.robot === r.id && !j.ended);
      el('simGoto'+r.id).disabled = el('simGoto'+r.id).dataset.simBusy === 'true' || s.health !== 'Sẵn sàng' || lastState?.shift?.phase !== 'RUNNING' || state?.state !== 'IDLE' || r.paused || !!r.fault || s.jobs.some(j=>j.robot === r.id && !j.ended);
    }
    jobs.replaceChildren();
    for (const job of [...s.jobs].reverse().filter(job=>!job.retryableRoute).slice(0,6)) {
      const item = make('div',null,'sim-job');item.append(make('strong',`${job.id} · ${job.robot}: ${job.pickup} → ${job.dropoff}`),make('div',phaseNames[job.phase] || job.phase));
      item.append(make('div',`${seconds(job.simSeconds ?? s.simSeconds-job.startSim)} mô phỏng · ${seconds(job.wallSeconds ?? (Date.now()-job.started)/1000)} thực`));
      if (job.ended) item.append(make('div',`${number(job.distanceCm)} cm · cost ${number(job.routeCost)} · chờ ${seconds(job.waitSeconds)}`));
      if (job.reason) item.append(make('div',job.reason));jobs.append(item);
    }
    orderList.replaceChildren();
    for (const order of [...(s.orders || [])].reverse().slice(0,5)) {
      const delivered = order.items.filter(item=>item.status === 'delivered').length;
      const active = order.items.filter(item=>['dispatching','in_progress'].includes(item.status)).length;
      const card = make('details',null,'sim-order-card');card.open = order.status !== 'completed';
      const summary = make('summary',`${order.id} · ${order.pickup} · ${delivered}/${order.total} kiện · ${orderStatusNames[order.status] || order.status}`);
      card.append(summary);
      const progress = make('progress');progress.max = order.total;progress.value = delivered;card.append(progress);
      card.append(make('div',`${active} đang giao · ${order.items.filter(item=>item.status === 'queued').length} chờ cấp việc`,'sim-order-subtitle'));
      if (order.reason) card.append(make('div',order.reason,'sim-order-warning'));
      const itemList = make('div',null,'sim-order-items');
      for (const item of order.items) {
        const job = (s.jobs || []).find(candidate=>candidate.id === item.jobId);
        const label = item.status === 'delivered' ? 'Đã giao' : item.status === 'interrupted' ? 'Gián đoạn' : item.status === 'queued' ? 'Chờ xe' : phaseNames[job?.phase] || 'Đang cấp việc';
        itemList.append(make('div',`#${item.number} → ${item.dropoff} · ${item.robot ? 'xe '+item.robot : 'chưa phân xe'} · ${label}${item.jobId ? ' · '+item.jobId : ''}${item.note ? ' · '+item.note : ''}`));
      }
      card.append(itemList);orderList.append(card);
    }
    events.replaceChildren(...[...s.events].reverse().slice(0,40).map(e=>make('div',`${number(e.simSeconds)}s · ${e.robot || 'SIM'} · ${e.type} ${e.node || e.zone || e.message || e.task_id || ''}`)));
    taskMetrics.replaceChildren(...[...s.tasks].reverse().slice(0,12).map(t=>make('div',`${t.task_id} → ${t.target} · xe ${t.assigned_agv || '—'} · ${t.status}\n${seconds((t.endSim ?? s.simSeconds)-t.firstSim)} mô phỏng / ${seconds(((t.endWall ?? Date.now())-t.firstWall)/1000)} thực`,'sim-job')));
  }
  function animate() {
    motion.replaceChildren();
    if (snapshot && lastState?.map?.nodes) {
      const transform = computeMapTransform(lastState.map.nodes),positions = new Map(lastState.map.nodes.map(n=>[n.id,{x:n.x*transform.scale+transform.ox,y:n.y*transform.scale+transform.oy}]));
      for (const robot of snapshot.robots) {
        const a = positions.get(robot.node),b = robot.edge && positions.get(robot.edge.to);if (!a) continue;
        const age = Math.min(0.5,(performance.now()-lastSnapshotAt)/1000),progress = robot.edge ? Math.min(1,robot.edge.progress+(!robot.paused && !robot.fault && robot.connected && robot.path.length ? age*snapshot.settings.timeScale*snapshot.settings.speedCmS/robot.edge.distance : 0)) : 0;
        const p = b ? {x:a.x+(b.x-a.x)*progress,y:a.y+(b.y-a.y)*progress} : a,color = robot.id === '01' ? '#38bdf8' : '#c084fc';
        const coreMarker = document.querySelector(`#agvsG [data-robot="${robot.id}"]`);if (coreMarker) coreMarker.style.opacity = '0.3';
        const group = svg('g',{'data-sim-robot':robot.id,transform:`translate(${p.x},${p.y})`});
        group.append(svg('circle',{r:21,fill:color,opacity:'.12'}),svg('rect',{x:-12,y:-12,width:24,height:24,rx:5,fill:robot.fault ? '#ef4444' : color,stroke:'#e2e8f0','stroke-width':2}),svg('text',{x:0,y:-29,fill:color,'text-anchor':'middle','font-size':15,'font-weight':700},robot.id+' ảo'));
        if (robot.cargo) group.append(svg('rect',{x:-5,y:-5,width:10,height:10,fill:'#fbbf24'}));motion.append(group);
      }
    }
    requestAnimationFrame(animate);
  }
  async function poll() {
    if (busy) return;busy = true;
    try {snapshot = await call('/state');lastSnapshotAt = performance.now();render();}
    catch {status.textContent = 'Mất kết nối mô phỏng; dữ liệu và vị trí đang cũ';snapshot = null;}
    finally {busy = false;}
  }
  poll();setInterval(poll,250);requestAnimationFrame(animate);
})();
