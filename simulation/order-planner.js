// Assignment estimates use the same directed edge weights as Core's Dijkstra.
// Core remains responsible for the actual route, reservations and mutex grants.
function shortestCost(map,from,to) {
  const nodes = new Set((map?.nodes || []).map(node=>node.id));
  if (!nodes.has(from) || !nodes.has(to)) return Infinity;
  if (from === to) return 0;
  const blocked = new Set(map.obstacles || []);
  if (blocked.has(to)) return Infinity;
  const neighbors = new Map([...nodes].map(id=>[id,[]]));
  for (const edge of map.edges || []) {
    if (!nodes.has(edge.from) || !nodes.has(edge.to) || !Number.isFinite(edge.weight) || edge.weight < 0) continue;
    neighbors.get(edge.from).push([edge.to,edge.weight]);
    if (edge.two_way) neighbors.get(edge.to).push([edge.from,edge.weight]);
  }
  const distances = new Map([[from,0]]),visited = new Set();
  while (true) {
    let current = null,cost = Infinity;
    for (const [id,value] of distances) if (!visited.has(id) && value < cost) {current = id;cost = value;}
    if (current === null) return Infinity;
    if (current === to) return cost;
    visited.add(current);
    for (const [next,weight] of neighbors.get(current)) {
      if (blocked.has(next) || visited.has(next)) continue;
      const nextCost = cost+weight;
      if (nextCost < (distances.get(next) ?? Infinity)) distances.set(next,nextCost);
    }
  }
}

function normalizeOrder(map,body) {
  if (!body || typeof body !== 'object' || Array.isArray(body) || typeof body.pickup !== 'string' || !Array.isArray(body.deliveries))
    throw new Error('Đơn hàng cần kho lấy và danh sách điểm giao');
  const warehouses = new Set((map?.nodes || []).filter(node=>node.type === 'station').map(node=>node.id));
  if (!warehouses.has(body.pickup)) throw new Error('Kho lấy không tồn tại');
  if (body.deliveries.length < 1 || body.deliveries.length > 20) throw new Error('Đơn hàng cần 1–20 dòng giao');
  let total = 0;
  const deliveries = body.deliveries.map(line=>{
    if (!line || typeof line !== 'object' || Array.isArray(line) || !warehouses.has(line.dropoff) || line.dropoff === body.pickup)
      throw new Error('Điểm giao phải là kho khác kho lấy');
    if (!Number.isInteger(line.quantity) || line.quantity < 1 || line.quantity > 20)
      throw new Error('Số kiện mỗi dòng phải từ 1 đến 20');
    if (!Number.isFinite(shortestCost(map,body.pickup,line.dropoff)))
      throw new Error(`Không có đường có hướng từ ${body.pickup} đến ${line.dropoff}`);
    total += line.quantity;
    return {dropoff:line.dropoff,quantity:line.quantity};
  });
  if (total > 20) throw new Error('Một đơn tối đa 20 kiện');
  return {pickup:body.pickup,deliveries,total};
}

function chooseRobot(map,robots,pickup,dropoff) {
  const deliveryCost = shortestCost(map,pickup,dropoff);
  return robots.map(robot=>({robot,cost:shortestCost(map,robot.node,pickup)+deliveryCost}))
    .filter(choice=>Number.isFinite(choice.cost))
    .sort((a,b)=>a.cost-b.cost || a.robot.id.localeCompare(b.robot.id))[0] || null;
}

module.exports = {shortestCost,normalizeOrder,chooseRobot};
