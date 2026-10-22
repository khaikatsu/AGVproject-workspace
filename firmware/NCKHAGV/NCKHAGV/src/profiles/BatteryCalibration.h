#pragma once

// Hieu chuan pin tach khoi file chinh toc do de tranh mat khi dan lai profile.
// Xe01: 21/10/2026, khay dang bat 11.9 V; ADC nghi 16 mau, TB 2420.94 mV.
// Xe02: cap so do 20/10/2026: khay 12.5 V, ADC trung binh 2388 mV.
// Can do lai neu doi bo ESP32/module chia ap; ghep dong ho va ADC cung luc.
#ifndef AGV_BENCH_BATTERY_VOLTAGE_SCALE
#if defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
#define AGV_BENCH_BATTERY_VOLTAGE_SCALE (12.5f / 2.388f)
#elif defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
#define AGV_BENCH_BATTERY_VOLTAGE_SCALE (11.9f / 2.421f)
#else
#define AGV_BENCH_BATTERY_VOLTAGE_SCALE (12.5f / 2.3f)
#endif
#endif
