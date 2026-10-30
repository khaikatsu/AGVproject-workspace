'use strict';

const headingLabels = {N: '↑ Lên', E: '→ Phải', S: '↓ Xuống', W: '← Trái'};
const headingAngles = {N: -90, E: 0, S: 90, W: 180};
const SVG_NS = 'http://www.w3.org/2000/svg';
const robotMarkerMotion = new Map();
const robotMarkerElements = new Map();
const mapStaticCache = {live: null, preview: null};
let mapDockFrame = 0;
let robotMotionFrame = 0;

function encoderFresh(robot) {
  return dataFresh() && lastState?.mqtt_connected !== false && lastState?.storage_healthy !== false &&
    robot?.online === true && robot.position_valid !== false && !['ERROR', 'OFFLINE', 'UNLOCALIZED'].includes(robot.state) &&
    robot.odometry?.valid === true && typeof robot.odometry.age_ms === 'number' &&
    robot.odometry.age_ms + snapshotAge() * 1000 <= (robot.odometry.stale_after_ms || 1000);
}

function robotSpeedText(robot) {
  return encoderFresh(robot) && typeof robot.speed_cm_s === 'number' && Number.isFinite(robot.speed_cm_s)
    ? `${Math.max(0, robot.speed_cm_s).toFixed(1)} cm/s`
    : '— cm/s';
}

function updateRobotMotionLabels() {
  document.querySelectorAll('[data-robot-speed]').forEach(element => {
    const robot = lastState?.fleet.agvs.find(candidate => candidate.id === element.dataset.robotSpeed);
    element.textContent = robotSpeedText(robot);
  });
  const focus = singleWarehouseRobot() || selectedAgvId || '01';
  const robot = lastState?.fleet.agvs.find(candidate => candidate.id === focus);
  const estimate = robot?.estimated_position;
  $('robot01Speed').textContent = (lastState?.fleet.agvs || []).filter(candidate => ['01', '02'].includes(candidate.id))
    .map(candidate => `Xe ${candidate.id}: ${robotSpeedText(candidate)}`).join(' · ') || 'Chờ dữ liệu tốc độ';
  $('robot01Speed').title = `Tốc độ ước tính từ hai encoder${robot?.odometry?.pulses_per_100cm
    ? `, hệ số thử ${robot.odometry.pulses_per_100cm} xung/100 cm`
    : ''}`;
  const live = encoderFresh(robot);
  const held = robotMarkerMotion.get(focus)?.source === 'encoder_stale';
  $('robot01Position').textContent = live && estimate
    ? `Xe ${focus}: ${robotSpeedText(robot)} · ước tính ${locationText(estimate.from)} → ${locationText(estimate.to)} · ${finite(estimate.distance_cm).toFixed(1)} / ${finite(estimate.edge_length_cm).toFixed(1)} cm · RFID gần nhất: ${locationText(robot.node)}`
    : held || robot?.encoder_supported && !live
      ? `Xe ${focus}: ${robotSpeedText(robot)} · ${live ? 'vị trí ước tính cuối, chờ chặng/RFID mới' : 'dữ liệu encoder cũ / chưa có, giữ vị trí hiển thị'} · RFID gần nhất: ${locationText(robot?.node)}`
      : `Xe ${focus}: vị trí RFID ${locationText(robot?.node)}${live ? ' · tốc độ encoder' : ' · chờ encoder'}`;
  if (robot?.obstacle_alert) {
    const alert = robot.obstacle_alert;
    $('robot01Position').textContent += ` · VẬT CẢN ${alert.direction === 'rear' ? 'PHÍA SAU' : 'PHÍA TRƯỚC'} ${finite(alert.range_cm).toFixed(1)} cm — đang dừng chờ`;
  }
}

function markerPoint(motion, now) {
  const progress = motion.duration ? Math.min(1, Math.max(0, (now - motion.startedAt) / motion.duration)) : 1;
  return {
    x: motion.fromX + (motion.x - motion.fromX) * progress,
    y: motion.fromY + (motion.y - motion.fromY) * progress
  };
}

function robotMarkerPoint(robot, fallback, transform) {
  const now = performance.now();
  const estimate = robot.estimated_position;
  const context = `${lastState?.core_session_id}:${lastState?.map?.revision}:${robot.node}:${transform.scale}:${transform.ox}:${transform.oy}`;
  let motion = robotMarkerMotion.get(robot.id);
  if (motion?.context !== context) motion = null;
  const live = encoderFresh(robot) && estimate?.source === 'encoder' &&
    typeof estimate.x === 'number' && Number.isFinite(estimate.x) &&
    typeof estimate.y === 'number' && Number.isFinite(estimate.y);
  if (live) {
    const x = estimate.x * transform.scale + transform.ox;
    const y = estimate.y * transform.scale + transform.oy;
    const key = `${robot.odometry.boot_id}:${robot.odometry.connection_session}:${robot.odometry.seq}`;
    if (!motion || motion.key !== key) {
      const current = motion ? {x: motion.displayX, y: motion.displayY} : fallback;
      motion = {
        context, key, fromX: current.x, fromY: current.y, x, y, startedAt: now,
        duration: motion && motion.route === estimate.route_cmd_id ? 180 : 0,
        route: estimate.route_cmd_id, source: 'encoder'
      };
    } else {
      motion.source = 'encoder';
    }
  } else if (motion && robot.encoder_supported && (motion.source.startsWith('encoder') || !encoderFresh(robot))) {
    motion.fromX = motion.x = motion.displayX;
    motion.fromY = motion.y = motion.displayY;
    motion.duration = 0;
    motion.source = motion.source.startsWith('encoder') ? 'encoder_stale' : 'rfid';
  } else {
    motion = {
      context, fromX: fallback.x, fromY: fallback.y, x: fallback.x, y: fallback.y,
      startedAt: now, duration: 0, source: 'rfid'
    };
  }
  const point = markerPoint(motion, now);
  motion.displayX = point.x;
  motion.displayY = point.y;
  robotMarkerMotion.set(robot.id, motion);
  return {...point, source: motion.source};
}

function animateRobotMarkers() {
  robotMotionFrame = 0;
  const now = performance.now();
  let active = false;
  for (const [id, motion] of robotMarkerMotion) {
    const group = robotMarkerElements.get(id);
    const robot = lastState?.fleet.agvs.find(candidate => candidate.id === id);
    if (!group || !robot) continue;
    if (!encoderFresh(robot) && motion.source.startsWith('encoder')) {
      motion.fromX = motion.x = motion.displayX;
      motion.fromY = motion.y = motion.displayY;
      motion.duration = 0;
      motion.source = 'encoder_stale';
      group.dataset.positionSource = motion.source;
    }
    const point = markerPoint(motion, now);
    motion.displayX = point.x;
    motion.displayY = point.y;
    group.setAttribute('transform', `translate(${point.x + finite(motion.offset)},${point.y})`);
    if (motion.duration && now - motion.startedAt < motion.duration && encoderFresh(robot)) active = true;
  }
  updateRobotMotionLabels();
  if (active) robotMotionFrame = requestAnimationFrame(animateRobotMarkers);
}

function scheduleMapDocking() {
  if (mapDockFrame) return;
  mapDockFrame = requestAnimationFrame(() => {
    mapDockFrame = 0;
    const body = document.body;
    const viewport = $('mapViewport');
    const main = $('main-container');
    const graph = $('nodesG').getBoundingClientRect();
    const canvas = viewport.getBoundingClientRect();
    const frame = main.getBoundingClientRect();
    const leftSpace = graph.left - frame.left - 40;
    const rightSpace = frame.right - graph.right - 40;
    const docked = body.dataset.mapExpanded === 'true' && body.dataset.view !== 'reports' &&
      innerWidth > 1100 && viewport.dataset.zoom === 'fit' && graph.width > 0 && leftSpace >= 260 && rightSpace >= 260;
    body.dataset.mapDocked = String(docked);
    if (docked) {
      main.style.setProperty('--map-tools-top', `${canvas.top - frame.top}px`);
      main.style.setProperty('--map-tools-height', `${canvas.height}px`);
      main.style.setProperty('--map-left-width', `${Math.min(300, leftSpace)}px`);
      main.style.setProperty('--map-right-width', `${Math.min(280, rightSpace)}px`);
    }
  });
}

function svgElement(tag, attributes, text) {
  const element = document.createElementNS(SVG_NS, tag);
  Object.entries(attributes).forEach(([key, value]) => element.setAttribute(key, value));
  if (text != null) element.textContent = text;
  return element;
}

function computeMapTransform(nodes) {
  if (!nodes.length) return {scale: 1, ox: 80, oy: 80};
  const xs = nodes.map(node => finite(node.x));
  const ys = nodes.map(node => finite(node.y));
  const minX = Math.min(...xs);
  const minY = Math.min(...ys);
  const width = Math.max(...xs) - minX || 1;
  const height = Math.max(...ys) - minY || 1;
  const scale = Math.min(900 / width, 600 / height);
  return {scale, ox: 80 + (900 - width * scale) / 2 - minX * scale, oy: 80 + (600 - height * scale) / 2 - minY * scale};
}

function roundedEdgePath(a, via, b, clearance = 0) {
  const first = Math.hypot(via.x - a.x, via.y - a.y);
  const last = Math.hypot(b.x - via.x, b.y - via.y);
  const ux = (via.x - a.x) / first;
  const uy = (via.y - a.y) / first;
  const vx = (b.x - via.x) / last;
  const vy = (b.y - via.y) / last;
  const radius = Math.min(30, first / 4, last / 4);
  return `M ${a.x} ${a.y} L ${via.x - ux * radius} ${via.y - uy * radius} Q ${via.x} ${via.y} ${via.x + vx * radius} ${via.y + vy * radius} L ${b.x - vx * clearance} ${b.y - vy * clearance}`;
}

function sonarDetections(agvs, preview) {
  if (preview) return [];
  return agvs.filter(robot => robot.online && robot.obstacle_detection &&
    Number.isFinite(robot.obstacle_detection.x) && Number.isFinite(robot.obstacle_detection.y));
}

function mapStructureKey(map, detections) {
  return JSON.stringify([
    (map.nodes || []).map(node => [node.id, node.type, node.x, node.y]),
    (map.edges || []).map(edge => [edge.from, edge.to, edge.weight, edge.type, edge.path_class, edge.two_way, edge.geometry]),
    map.obstacles || [], map.blocked_edges || [],lastState?.location_names?.revision,
    detections.map(robot => [robot.id, robot.obstacle_detection])
  ]);
}

function renderStaticMap(map, agvs, prefix) {
  const preview = !!prefix;
  const cacheName = preview ? 'preview' : 'live';
  const nodes = Array.isArray(map.nodes) ? map.nodes : [];
  const edges = Array.isArray(map.edges) ? map.edges : [];
  const detections = sonarDetections(agvs, preview);
  const key = mapStructureKey(map, detections);
  if (mapStaticCache[cacheName]?.key === key) return mapStaticCache[cacheName];

  const nodesG = $(preview ? 'previewNodesG' : 'nodesG');
  const edgesG = $(preview ? 'previewEdgesG' : 'edgesG');
  nodesG.replaceChildren();
  edgesG.replaceChildren();
  if (!preview) $('sonarObstaclesG').replaceChildren();
  const {scale, ox, oy} = computeMapTransform(nodes);
  const position = new Map();
  nodes.forEach(node => position.set(node.id, {x: finite(node.x) * scale + ox, y: finite(node.y) * scale + oy}));
  const seenEdges = new Set();
  edges.forEach(edge => {
    const a = position.get(edge.from);
    const b = position.get(edge.to);
    if (!a || !b) return;
    const edgeKey = edge.two_way ? [edge.from, edge.to].sort().join('\n') : `${edge.from}\n${edge.to}`;
    if (seenEdges.has(edgeKey)) return;
    seenEdges.add(edgeKey);
    const blockedBySonar = detections.some(robot => {
      const detection = robot.obstacle_detection;
      return detection.from === edge.from && detection.to === edge.to ||
        edge.two_way && detection.from === edge.to && detection.to === edge.from;
    });
    const lineAttributes = {x1: a.x, y1: a.y, x2: b.x, y2: b.y, class: blockedBySonar ? 'edge sonar-obstacle-edge' : 'edge'};
    const via = edge.geometry?.via ? {x: edge.geometry.via.x * scale + ox, y: edge.geometry.via.y * scale + oy} : null;
    const line = via
      ? svgElement('path', {class: lineAttributes.class, fill: 'none', d: roundedEdgePath(a, via, b, preview ? 19 : 23), 'data-rounded-edge': `${edge.from}:${edge.to}`})
      : svgElement('line', lineAttributes);
    if (blockedBySonar) line.setAttribute('data-sonar-obstacle-edge', `${edge.from}:${edge.to}`);
    const length = Math.hypot(b.x - a.x, b.y - a.y) || 1;
    const dx = (b.x - a.x) / length;
    const dy = (b.y - a.y) / length;
    const clearance = preview ? 19 : 23;
    if (!via) {
      line.setAttribute('x2', b.x - dx * clearance);
      line.setAttribute('y2', b.y - dy * clearance);
    }
    if (edge.two_way) {
      line.setAttribute('x1', a.x + dx * clearance);
      line.setAttribute('y1', a.y + dy * clearance);
      line.setAttribute('marker-start', 'url(#arrow)');
    }
    line.append(svgElement('title', {}, `${edge.from} ${edge.two_way ? '↔' : '→'} ${edge.to}; trọng số ${edge.weight}`));
    edgesG.append(line);
    const parts = via
      ? [[a, via, edge.geometry.segment_lengths_cm[0]], [via, b, edge.geometry.segment_lengths_cm[1]]]
      : [[a, b, edge.weight]];
    parts.forEach(([start, end, label]) => {
      const horizontal = Math.abs(end.x - start.x) > Math.abs(end.y - start.y);
      edgesG.append(svgElement('text', {
        x: (start.x + end.x) / 2 + (horizontal ? 0 : 10),
        y: (start.y + end.y) / 2 + (horizontal ? -10 : 4),
        'text-anchor': horizontal ? 'middle' : 'start', class: 'edge-label'
      }, label));
    });
  });
  detections.forEach(robot => {
    const detection = robot.obstacle_detection;
    const dot = svgElement('circle', {
      cx: detection.x * scale + ox, cy: detection.y * scale + oy, r: 8,
      fill: 'var(--status-error)', stroke: '#fee2e2', 'stroke-width': 2, 'data-sonar-obstacle': robot.id
    });
    dot.append(svgElement('title', {}, `Xe ${robot.id}: vật cản ${detection.direction === 'rear' ? 'phía sau' : 'phía trước'}, đo ${detection.range_cm.toFixed(1)} cm; vị trí ước tính theo encoder`));
    $('sonarObstaclesG').append(dot);
  });
  const obstacles = new Set(map.obstacles || []);
  const nodeTypes = {mutex: 'RFID / vùng chung', station: 'Kho', charging: 'Trạm sạc', waypoint: 'Điểm đường / cửa'};
  const nodeColors = {mutex: 'var(--node-mutex)', charging: 'var(--node-charging)', station: 'var(--node-station)'};
  nodes.forEach(node => {
    const point = position.get(node.id);
    const blocked = obstacles.has(node.id);
    const circle = svgElement('circle', {
      cx: point.x, cy: point.y, r: preview ? 16 : 20,
      fill: blocked ? '#7f1d1d' : 'var(--bg-panel, #ffffff)',
      stroke: blocked ? 'var(--status-error)' : nodeColors[node.type] || '#475569',
      'stroke-width': 2, class: 'node-clickable'
    });
    circle.setAttribute('data-node', node.id);
    circle.append(svgElement('title', {}, `${preview ? node.id : locationText(node.id)}; ${node.type}; ${nodeTypes[node.type] || node.type}; (${node.x}, ${node.y})`));
    if (!preview) circle.onclick = () => onNodeClick(node.id);
    const compact = String(node.id).length <= 4;
    nodesG.append(circle, svgElement('text', {
      x: compact ? point.x : point.x + 25, y: point.y + 5,
      'text-anchor': compact ? 'middle' : 'start', class: 'node-label', 'data-node-label': node.id
    }, preview ? node.id : locationName(node.id)));
  });
  const svg = $(preview ? 'mapPreviewSvg' : 'mapSvg');
  if (nodes.length) {
    const points = nodes.map(node => ({...position.get(node.id), label: String(node.id)}));
    const left = Math.min(...points.map(point => point.x - 16));
    const top = Math.min(...points.map(point => point.y - 16));
    const right = Math.max(...points.map(point => point.x + (point.label.length <= 4 ? 20 : 29 + point.label.length * (preview ? 8 : 10))));
    const bottom = Math.max(...points.map(point => point.y + 16));
    svg.setAttribute('viewBox', `${left - 40} ${top - 48} ${right - left + 80} ${bottom - top + 96}`);
  } else {
    svg.setAttribute('viewBox', '20 20 1020 720');
  }
  const cache = {key, nodes, edges, position, scale, ox, oy};
  mapStaticCache[cacheName] = cache;
  return cache;
}

function renderDynamicMap(context, agvs) {
  const routesG = $('routesG');
  const agvsG = $('agvsG');
  routesG.replaceChildren();
  agvsG.replaceChildren();
  robotMarkerElements.clear();
  agvs.forEach((robot, index) => {
    const color = selectedAgvId === robot.id
      ? 'var(--vehicle-selected)'
      : index % 2 ? 'var(--vehicle-secondary)' : 'var(--vehicle-primary)';
    const path = [robot.node, ...(robot.path || [])];
    for (let index = 1; index < path.length; index++) {
      const a = context.position.get(path[index - 1]);
      const b = context.position.get(path[index]);
      if (!a || !b) continue;
      const edge = context.edges.find(candidate => candidate.from === path[index - 1] && candidate.to === path[index]);
      const via = edge?.geometry?.via;
      const attributes = {stroke: color, 'stroke-width': 5, opacity: 0.7, 'stroke-dasharray': '8 4', 'data-robot-route': robot.id};
      const segment = via
        ? svgElement('path', {...attributes, fill: 'none', d: roundedEdgePath(a, {x: via.x * context.scale + context.ox, y: via.y * context.scale + context.oy}, b)})
        : svgElement('line', {...attributes, x1: a.x, y1: a.y, x2: b.x, y2: b.y});
      segment.append(svgElement('title', {}, `Kế hoạch ${robot.id}: ${locationText(path[index - 1])} → ${locationText(path[index])}`));
      routesG.append(segment);
    }
    const point = context.position.get(robot.node);
    if (!point || robot.position_valid === false) {
      robotMarkerMotion.delete(robot.id);
      return;
    }
    const display = robotMarkerPoint(robot, point, context);
    const siblings = agvs.filter(candidate => candidate.node === robot.node);
    const offset = siblings.length > 1 ? siblings.indexOf(robot) * 20 - 10 : 0;
    robotMarkerMotion.get(robot.id).offset = offset;
    if (display.source.startsWith('encoder')) {
      agvsG.append(svgElement('circle', {cx: point.x, cy: point.y, r: 7, fill: 'none', stroke: color, opacity: 0.55, 'data-rfid-anchor': robot.id, 'pointer-events': 'none'}));
    }
    const group = svgElement('g', {
      transform: `translate(${display.x + offset},${display.y})`, class: 'agv-group',
      'data-robot': robot.id, 'data-position-source': display.source, 'pointer-events': 'bounding-box'
    });
    group.append(
      svgElement('circle', {
        cx: 0, cy: 0, r: 14, fill: colors[robot.state] || 'var(--status-offline)', stroke: color,
        'stroke-width': selectedAgvId === robot.id ? 4 : 2,
        'stroke-dasharray': display.source.startsWith('encoder') ? '4 2' : 'none'
      }),
      svgElement('text', {x: -14, y: -43, class: 'agv-label'}, robot.id),
      svgElement('title', {}, `${robot.id}: ${labels[robot.state] || robot.state}; ${display.source.startsWith('encoder') ? 'vị trí ước tính encoder; ' : ''}RFID gần nhất ${locationText(robot.node)}; ${robotSpeedText(robot)}; đầu xe ${robot.heading_supported && headingLabels[robot.heading] || 'chưa biết'}`)
    );
    if (robot.virtual_cargo === true) {
      const cargo = svgElement('g', {'data-virtual-cargo': robot.id, opacity: robot.virtual_cargo_known && robot.online !== false ? 1 : 0.45, 'pointer-events': 'none'});
      cargo.append(
        svgElement('rect', {x: -7, y: -8, width: 14, height: 14, rx: 1, fill: '#d99a46', stroke: '#5b3715', 'stroke-width': 1.5}),
        svgElement('path', {d: 'M-7,-4 L7,-4 M0,-8 L0,-4', stroke: '#ffe0a3', 'stroke-width': 2, fill: 'none'}),
        svgElement('title', {}, 'Kệ ảo theo chu kỳ servo; không phải cảm biến xác nhận tải thật')
      );
      group.append(cargo);
    }
    if (robot.online && robot.heading_supported && headingLabels[robot.heading]) {
      group.append(svgElement('path', {
        d: 'M18,-6 L29,-6 L29,-10 L40,0 L29,10 L29,6 L18,6 Z',
        transform: `rotate(${headingAngles[robot.heading]})`, fill: color, stroke: '#0b1220', 'stroke-width': 1.5,
        'data-robot-heading': robot.id, 'data-heading': robot.heading
      }));
    } else {
      group.append(svgElement('text', {x: 21, y: 5, fill: '#94a3b8', 'font-size': 16, 'data-heading-unknown': robot.id}, '?'));
    }
    group.onclick = event => {
      event.stopPropagation();
      selectAgv(robot.id);
    };
    agvsG.append(group);
    robotMarkerElements.set(robot.id, group);
  });
  for (const id of robotMarkerMotion.keys()) {
    if (!agvs.some(robot => robot.id === id)) robotMarkerMotion.delete(id);
  }
  if (robotMotionFrame) cancelAnimationFrame(robotMotionFrame);
  robotMotionFrame = requestAnimationFrame(animateRobotMarkers);
}

function drawMap(map, agvs, prefix = '') {
  const context = renderStaticMap(map, agvs, prefix);
  if (!prefix) renderDynamicMap(context, agvs);
}

function renderMap(map, agvs) {
  drawMap(map, agvs);
  scheduleMapDocking();
}

function clearMapRenderCache() {
  mapStaticCache.live = null;
  mapStaticCache.preview = null;
  robotMarkerElements.clear();
  robotMarkerMotion.clear();
}
