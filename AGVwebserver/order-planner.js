const {warehouseNodes} = require('./public/warehouse-rules');

// Planning estimates only. Core remains the authority for routes and motion.
function shortestRoute(map,from,to) {
  const unreachable = {path:[],cost:Infinity};
  const nodes = new Set((map?.nodes || []).map(node=>node.id));
  if (!nodes.has(from) || !nodes.has(to)) return unreachable;
  const blocked = new Set(map.obstacles || []);
  if (blocked.has(from) || blocked.has(to)) return unreachable;
  if (from === to) return {path:[from],cost:0};
  const adjacent = new Map([...nodes].map(node=>[node,[]]));
  for (const edge of map.edges || []) {
    if ((map.blocked_edges || []).some(e=>e.from===edge.from && e.to===edge.to || e.from===edge.to && e.to===edge.from)) continue;
    if (!nodes.has(edge.from) || !nodes.has(edge.to) || !Number.isFinite(edge.weight) || edge.weight < 0) continue;
    adjacent.get(edge.from).push([edge.to,edge.weight]);
    if (edge.two_way) adjacent.get(edge.to).push([edge.from,edge.weight]);
  }
  const distance = new Map([[from,0]]),previous = new Map(),visited = new Set();
  while (true) {
    let current = null,cost = Infinity;
    for (const [node,value] of distance) if (!visited.has(node) && value < cost) {current = node;cost = value;}
    if (current === null) return unreachable;
    if (current === to) {
      const path=[to];
      while (path[0] !== from) path.unshift(previous.get(path[0]));
      return {path,cost};
    }
    visited.add(current);
    for (const [next,weight] of adjacent.get(current)) {
      if (blocked.has(next) || visited.has(next)) continue;
      const candidate = cost+weight;
      if (candidate < (distance.get(next) ?? Infinity)) {distance.set(next,candidate);previous.set(next,current);}
    }
  }
}

function shortestCost(map,from,to) {return shortestRoute(map,from,to).cost;}

function planOrder(map,body) {
  if (!body || typeof body !== 'object' || Array.isArray(body)) throw new Error('Đơn hàng không hợp lệ');
  const warehouses = new Set(warehouseNodes(map));
  function lines(value,key) {
    if (!Array.isArray(value) || !value.length || value.length > 20) throw new Error(`Cần 1–20 kho ${key}`);
    const seen = new Set();
    return value.map(line=>{
      const node = line?.[key],quantity = line?.quantity;
      if (!warehouses.has(node) || seen.has(node)) throw new Error(`Kho ${key} không tồn tại hoặc bị lặp`);
      if (!Number.isInteger(quantity) || quantity < 1 || quantity > 20) throw new Error('Mỗi kho cần 1–20 kiện');
      seen.add(node);
      return {[key]:node,quantity};
    });
  }
  const pickups = lines(body.pickups,'pickup'),deliveries = lines(body.deliveries,'dropoff');
  const total = pickups.reduce((sum,line)=>sum+line.quantity,0);
  if (total > 20 || total !== deliveries.reduce((sum,line)=>sum+line.quantity,0))
    throw new Error('Tổng số kiện lấy và giao phải bằng nhau, tối đa 20 kiện');

  // Successive shortest augmenting paths solve the capacitated directed matching.
  const count = 2+pickups.length+deliveries.length,source = 0,sink = count-1;
  const graph = Array.from({length:count},()=>[]),pairs = [];
  function edge(from,to,capacity,cost) {
    const forward = {to,capacity,cost,reverse:graph[to].length};
    const reverse = {to:from,capacity:0,cost:-cost,reverse:graph[from].length};
    graph[from].push(forward);graph[to].push(reverse);
    return forward;
  }
  pickups.forEach((line,index)=>edge(source,1+index,line.quantity,0));
  deliveries.forEach((line,index)=>edge(1+pickups.length+index,sink,line.quantity,0));
  pickups.forEach((pickup,i)=>deliveries.forEach((delivery,j)=>{
    if (pickup.pickup === delivery.dropoff) return;
    const cost = shortestCost(map,pickup.pickup,delivery.dropoff);
    if (Number.isFinite(cost)) pairs.push({i,j,edge:edge(1+i,1+pickups.length+j,total,cost)});
  }));
  let flow = 0;
  while (flow < total) {
    const distance = Array(count).fill(Infinity),previous = Array(count).fill(null);distance[source] = 0;
    for (let pass=0;pass<count-1;pass++) {
      let changed = false;
      for (let from=0;from<count;from++) for (let index=0;index<graph[from].length;index++) {
        const arc = graph[from][index];
        if (arc.capacity && distance[from]+arc.cost < distance[arc.to]) {
          distance[arc.to] = distance[from]+arc.cost;previous[arc.to] = {from,index};changed = true;
        }
      }
      if (!changed) break;
    }
    if (!previous[sink]) throw new Error('Không ghép được các kho qua đường có hướng');
    let amount = total-flow;
    for (let at=sink;at!==source;at=previous[at].from) {
      const {from,index} = previous[at];amount = Math.min(amount,graph[from][index].capacity);
    }
    for (let at=sink;at!==source;at=previous[at].from) {
      const {from,index} = previous[at],arc = graph[from][index];
      arc.capacity -= amount;graph[at][arc.reverse].capacity += amount;
    }
    flow += amount;
  }
  const items = [];
  for (const {i,j,edge:arc} of pairs) {
    const quantity = graph[arc.to][arc.reverse].capacity;
    for (let unit=0;unit<quantity;unit++) items.push({number:items.length+1,pickup:pickups[i].pickup,
      dropoff:deliveries[j].dropoff,status:'queued',robot:null,task_id:null});
  }
  return {pickups,deliveries,total,items};
}

module.exports = {shortestCost,shortestRoute,planOrder};
