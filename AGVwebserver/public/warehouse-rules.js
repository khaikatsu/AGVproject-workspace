'use strict';

const WAREHOUSE_NODE_PATTERN = /^K[1-6]$/;

function isWarehouseNode(node) {
  return node?.type === 'station' && WAREHOUSE_NODE_PATTERN.test(node.id);
}

function warehouseNodes(map) {
  return (map?.nodes || []).filter(isWarehouseNode).map(node => node.id).sort();
}

if (typeof module === 'object' && module.exports) {
  module.exports = {WAREHOUSE_NODE_PATTERN, isWarehouseNode, warehouseNodes};
}
