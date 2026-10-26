'use strict';

// Names belong to the gateway. Physical node IDs, graph and robot commands stay fixed.
const GROUPS = [['K1','K2','K3','K4','K5','K6'], ['S1','S2']];
const DEFAULT_LABELS = Object.fromEntries(GROUPS.flat().map(id => [id,id]));
const fail = (status,message) => {throw Object.assign(new Error(message),{status});};
function validateNames(value) {
    if (!value || !Number.isSafeInteger(value.revision) || value.revision < 0 || !value.labels ||
        Object.keys(value.labels).length !== Object.keys(DEFAULT_LABELS).length)
        fail(500,'Cấu hình tên vị trí đã lưu không hợp lệ');
    for (const group of GROUPS) if (new Set(group.map(id=>value.labels[id])).size !== group.length ||
        group.some(id=>!group.includes(value.labels[id]))) fail(500,'Cấu hình tên vị trí đã lưu không hợp lệ');
    return value;
}
function createLocationNames(db) {
    let current = validateNames(db.loadLocationNames() || {revision:0,labels:{...DEFAULT_LABELS},updated_at:null});
    const snapshot = () => JSON.parse(JSON.stringify(current));
    function assertRevision(revision) {
        if (!Number.isSafeInteger(revision) || revision < 0) fail(400,'Thiếu phiên bản tên kho/trạm hợp lệ');
        if (revision !== current.revision) fail(409,'Tên kho/trạm đã thay đổi. Tải lại dữ liệu rồi chọn lại đích.');
    }
    function resolve(name,revision) {
        // Existing integrations continue to address physical IDs without this opt-in field.
        if (revision === undefined) return name;
        assertRevision(revision);
        return Object.keys(current.labels).find(id=>current.labels[id]===name) || name;
    }
    function swap(body,map) {
        assertRevision(body?.expected_revision);
        const {first,second} = body;
        const group = GROUPS.find(group=>group.includes(first) && group.includes(second));
        if (!group || first === second) fail(400,'Chọn hai kho khác nhau hoặc hai trạm khác nhau để hoán đổi tên');
        const a = resolve(first,current.revision), b = resolve(second,current.revision);
        const type = group[0][0]==='K' ? 'station' : 'charging';
        if (![a,b].every(id=>map.nodes.some(node=>node.id===id && node.type===type)))
            fail(409,'Vị trí kho/trạm không còn khớp bản đồ hiện tại');
        const next = {revision:current.revision+1,labels:{...current.labels},updated_at:new Date().toISOString()};
        [next.labels[a],next.labels[b]] = [next.labels[b],next.labels[a]];
        if (!db.saveLocationNames(current.revision,next)) fail(409,'Tên kho/trạm đã được cập nhật ở nơi khác. Tải lại trang.');
        current = next;return snapshot();
    }
    return {snapshot,resolve,swap,assertRevision};
}
module.exports = {createLocationNames,DEFAULT_LABELS};
