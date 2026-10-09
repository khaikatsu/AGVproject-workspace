// Keep the MCU's compact directed graph in sync with the Core map.
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const source = path.join(root, 'data', 'map_layout.json');
const destination = path.join(root, 'firmware', 'NCKHAGV', 'NCKHAGV', 'src', 'core', 'RouteMap.generated.h');
const map = JSON.parse(fs.readFileSync(source, 'utf8'));

const names = new Set();
for (const node of map.nodes) {
    if (!/^[A-Z][A-Z0-9]{0,6}$/.test(node.id) || names.has(node.id) ||
        !Number.isSafeInteger(node.x) || !Number.isSafeInteger(node.y) ||
        node.x < -32768 || node.x > 32767 || node.y < -32768 || node.y > 32767) {
        throw new Error(`Invalid route node: ${node.id}`);
    }
    names.add(node.id);
}
for (const edge of map.edges) {
    if (!names.has(edge.from) || !names.has(edge.to) || edge.from === edge.to ||
        typeof edge.two_way !== 'boolean') {
        throw new Error(`Invalid route edge: ${edge.from} -> ${edge.to}`);
    }
}

const lines = [
    '#pragma once',
    '#include <cstdint>',
    '// Generated from data/map_layout.json. Run node scripts/generate-firmware-route.js after map changes.',
    'namespace RouteMap {',
    'struct Node { const char* id; int16_t x, y; };',
    'struct Edge { const char* from; const char* to; bool twoWay; };',
    `inline constexpr Node NODES[${map.nodes.length}] = {`,
    ...map.nodes.map(n => `    {"${n.id}", ${n.x}, ${n.y}},`),
    '};',
    `inline constexpr Edge EDGES[${map.edges.length}] = {`,
    ...map.edges.map(e => `    {"${e.from}", "${e.to}", ${e.two_way ? 'true' : 'false'}},`),
    '};',
    '}',
    ''
];
const expected = lines.join('\n');
if (process.argv.includes('--check')) {
    if (!fs.existsSync(destination) || fs.readFileSync(destination, 'utf8').replace(/\r\n/g, '\n') !== expected) {
        throw new Error('Firmware route map is out of sync with data/map_layout.json');
    }
} else {
    fs.writeFileSync(destination, expected);
}
