'use strict';

const $ = id => document.getElementById(id);
const finite = (value, fallback = 0) => typeof value === 'number' && Number.isFinite(value) ? value : fallback;
const labels = {
  IDLE: 'Sẵn sàng',
  WORKING: 'Đang chạy',
  LIFTING: 'Đang nâng kệ',
  LOWERING: 'Đang hạ kệ',
  CHARGING_EN_ROUTE: 'Đi tới trạm sạc',
  CHARGING: 'Ở trạm sạc',
  BLOCKED: 'Chờ đường',
  ERROR: 'Lỗi / cần xác minh',
  OFFLINE: 'Mất telemetry',
  UNKNOWN: 'Chưa xác định',
  UNLOCALIZED: 'Chờ quét RFID',
  submitting: 'Chờ core xác nhận',
  unknown: 'Chưa xác định kết quả',
  pending: 'Chờ phân công',
  dispatched: 'Đã giao xe',
  completed: 'Hoàn thành',
  cancelled: 'Đã hủy',
  interrupted: 'Gián đoạn',
  rejected: 'Bị từ chối'
};
const colors = {
  WORKING: 'var(--status-working)',
  LIFTING: 'var(--status-warn)',
  LOWERING: 'var(--status-warn)',
  IDLE: 'var(--status-ok)',
  CHARGING: 'var(--status-warn)',
  CHARGING_EN_ROUTE: 'var(--status-warn)',
  ERROR: 'var(--status-error)',
  BLOCKED: 'var(--status-blocked)',
  OFFLINE: 'var(--status-offline)',
  UNKNOWN: 'var(--status-offline)',
  UNLOCALIZED: 'var(--status-warn)'
};

function fmtDate(value) {
  if (!value) return '—';
  const date = new Date(/^\d{4}-\d\d-\d\d \d\d:/.test(value) ? value.replace(' ', 'T') + 'Z' : value);
  return Number.isNaN(date.getTime()) ? String(value) : date.toLocaleString('vi-VN');
}
