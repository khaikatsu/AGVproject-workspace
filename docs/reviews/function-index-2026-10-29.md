# Mục lục hàm/method ứng dụng — 29/10/2026

Đi kèm [báo cáo review](system-review-2026-10-29.md). Chỉ mục các định nghĩa có tên nhận diện từ source; không thực thi ứng dụng hoặc đọc giá trị cấu hình mạng cục bộ. Bộ nhận diện không phải AST, có thể bỏ sót lambda, arrow function, macro, khai báo không có thân hàm và cấu trúc phức tạp. Danh sách dùng để tra mã, không phải bằng chứng mọi hàm/nhánh đã được kiểm chứng. Tên trùng giữa các class/overload giữ theo dòng. Thư viện vendored và bảng route sinh tự động không nằm trong danh mục.

## AGVwebserver/cargo-proof.js

[Mở source](../../AGVwebserver/cargo-proof.js) · 31 dòng · 4 định nghĩa nhận diện.

- [cargoReading — dòng 4](../../AGVwebserver/cargo-proof.js#L4)
- [cargoResult — dòng 11](../../AGVwebserver/cargo-proof.js#L11)
- [loweredForExit — dòng 15](../../AGVwebserver/cargo-proof.js#L15)
- [dropoffDoneAtGate — dòng 23](../../AGVwebserver/cargo-proof.js#L23)

## AGVwebserver/db.js

[Mở source](../../AGVwebserver/db.js) · 478 dòng · 26 định nghĩa nhận diện.

- [batteryForDisplay — dòng 7](../../AGVwebserver/db.js#L7)
- [createDatabase — dòng 14](../../AGVwebserver/db.js#L14)
- [computeAgvUtilization — dòng 179](../../AGVwebserver/db.js#L179)
- [loadLocationNames — dòng 223](../../AGVwebserver/db.js#L223)
- [saveLocationNames — dòng 228](../../AGVwebserver/db.js#L228)
- [saveOrder — dòng 238](../../AGVwebserver/db.js#L238)
- [getOrders — dòng 242](../../AGVwebserver/db.js#L242)
- [logAgvEventIfChanged — dòng 246](../../AGVwebserver/db.js#L246)
- [getAgvHistory — dòng 252](../../AGVwebserver/db.js#L252)
- [getOpenTasks — dòng 256](../../AGVwebserver/db.js#L256)
- [getTaskHistory — dòng 260](../../AGVwebserver/db.js#L260)
- [getTaskReference — dòng 265](../../AGVwebserver/db.js#L265)
- [deleteTaskHistory — dòng 269](../../AGVwebserver/db.js#L269)
- [logObstacle — dòng 275](../../AGVwebserver/db.js#L275)
- [getObstacleHistory — dòng 279](../../AGVwebserver/db.js#L279)
- [saveMapSnapshot — dòng 283](../../AGVwebserver/db.js#L283)
- [getLatestMapSnapshot — dòng 287](../../AGVwebserver/db.js#L287)
- [getAllMapSnapshots — dòng 292](../../AGVwebserver/db.js#L292)
- [getMapSnapshotById — dòng 296](../../AGVwebserver/db.js#L296)
- [deleteMapSnapshot — dòng 302](../../AGVwebserver/db.js#L302)
- [insertSystemLog — dòng 308](../../AGVwebserver/db.js#L308)
- [getRecentSystemLogs — dòng 313](../../AGVwebserver/db.js#L313)
- [insertAgvError — dòng 317](../../AGVwebserver/db.js#L317)
- [computeKpiReport — dòng 327](../../AGVwebserver/db.js#L327)
- [getLatestKpiReport — dòng 373](../../AGVwebserver/db.js#L373)
- [getKpiHistory — dòng 379](../../AGVwebserver/db.js#L379)

## AGVwebserver/gateway.js

[Mở source](../../AGVwebserver/gateway.js) · 454 dòng · 19 định nghĩa nhận diện.

- [batteryForDisplay — dòng 19](../../AGVwebserver/gateway.js#L19)
- [httpError — dòng 25](../../AGVwebserver/gateway.js#L25)
- [id — dòng 26](../../AGVwebserver/gateway.js#L26)
- [number — dòng 31](../../AGVwebserver/gateway.js#L31)
- [limit — dòng 36](../../AGVwebserver/gateway.js#L36)
- [createGateway — dòng 39](../../AGVwebserver/gateway.js#L39)
- [broadcast — dòng 111](../../AGVwebserver/gateway.js#L111)
- [onState — dòng 137](../../AGVwebserver/gateway.js#L137)
- [failRequests — dòng 166](../../AGVwebserver/gateway.js#L166)
- [connectCore — dòng 171](../../AGVwebserver/gateway.js#L171)
- [requireCore — dòng 210](../../AGVwebserver/gateway.js#L210)
- [command — dòng 216](../../AGVwebserver/gateway.js#L216)
- [locationNamesBlockReason — dòng 238](../../AGVwebserver/gateway.js#L238)
- [target — dòng 253](../../AGVwebserver/gateway.js#L253)
- [task — dòng 258](../../AGVwebserver/gateway.js#L258)
- [snapshotName — dòng 397](../../AGVwebserver/gateway.js#L397)
- [snapshot — dòng 415](../../AGVwebserver/gateway.js#L415)
- [start — dòng 437](../../AGVwebserver/gateway.js#L437)
- [close — dòng 442](../../AGVwebserver/gateway.js#L442)

## AGVwebserver/location-names.js

[Mở source](../../AGVwebserver/location-names.js) · 44 dòng · 5 định nghĩa nhận diện.

- [validateNames — dòng 7](../../AGVwebserver/location-names.js#L7)
- [createLocationNames — dòng 15](../../AGVwebserver/location-names.js#L15)
- [assertRevision — dòng 18](../../AGVwebserver/location-names.js#L18)
- [resolve — dòng 22](../../AGVwebserver/location-names.js#L22)
- [swap — dòng 28](../../AGVwebserver/location-names.js#L28)

## AGVwebserver/order-planner.js

[Mở source](../../AGVwebserver/order-planner.js) · 107 dòng · 5 định nghĩa nhận diện.

- [shortestRoute — dòng 4](../../AGVwebserver/order-planner.js#L4)
- [shortestCost — dòng 37](../../AGVwebserver/order-planner.js#L37)
- [planOrder — dòng 39](../../AGVwebserver/order-planner.js#L39)
- [lines — dòng 42](../../AGVwebserver/order-planner.js#L42)
- [edge — dòng 61](../../AGVwebserver/order-planner.js#L61)

## AGVwebserver/orders.js

[Mở source](../../AGVwebserver/orders.js) · 475 dòng · 26 định nghĩa nhận diện.

- [createOrderService — dòng 10](../../AGVwebserver/orders.js#L10)
- [orderView — dòng 22](../../AGVwebserver/orders.js#L22)
- [interrupt — dòng 32](../../AGVwebserver/orders.js#L32)
- [interruptAll — dòng 44](../../AGVwebserver/orders.js#L44)
- [endShift — dòng 47](../../AGVwebserver/orders.js#L47)
- [resume — dòng 58](../../AGVwebserver/orders.js#L58)
- [resolve — dòng 92](../../AGVwebserver/orders.js#L92)
- [remove — dòng 135](../../AGVwebserver/orders.js#L135)
- [linkTaskHistory — dòng 146](../../AGVwebserver/orders.js#L146)
- [robotAt — dòng 170](../../AGVwebserver/orders.js#L170)
- [liftReady — dòng 174](../../AGVwebserver/orders.js#L174)
- [beginLiftWait — dòng 179](../../AGVwebserver/orders.js#L179)
- [delivered — dòng 184](../../AGVwebserver/orders.js#L184)
- [warehouseGate — dòng 191](../../AGVwebserver/orders.js#L191)
- [processItem — dòng 198](../../AGVwebserver/orders.js#L198)
- [eligible — dòng 271](../../AGVwebserver/orders.js#L271)
- [selectedRobot — dòng 279](../../AGVwebserver/orders.js#L279)
- [standaloneScope — dòng 284](../../AGVwebserver/orders.js#L284)
- [preview — dòng 288](../../AGVwebserver/orders.js#L288)
- [waitingReason — dòng 294](../../AGVwebserver/orders.js#L294)
- [planQueued — dòng 313](../../AGVwebserver/orders.js#L313)
- [dispatch — dòng 329](../../AGVwebserver/orders.js#L329)
- [dispatchLift — dòng 363](../../AGVwebserver/orders.js#L363)
- [tick — dòng 376](../../AGVwebserver/orders.js#L376)
- [runTick — dòng 385](../../AGVwebserver/orders.js#L385)
- [create — dòng 448](../../AGVwebserver/orders.js#L448)

## AGVwebserver/public/dashboard-location-names.js

[Mở source](../../AGVwebserver/public/dashboard-location-names.js) · 55 dòng · 6 định nghĩa nhận diện.

- [locationName — dòng 4](../../AGVwebserver/public/dashboard-location-names.js#L4)
- [locationText — dòng 5](../../AGVwebserver/public/dashboard-location-names.js#L5)
- [locationRequest — dòng 8](../../AGVwebserver/public/dashboard-location-names.js#L8)
- [renderLocationNames — dòng 19](../../AGVwebserver/public/dashboard-location-names.js#L19)
- [updateLocationNameChoices — dòng 38](../../AGVwebserver/public/dashboard-location-names.js#L38)
- [swapLocationNames — dòng 45](../../AGVwebserver/public/dashboard-location-names.js#L45)

## AGVwebserver/public/dashboard-map.js

[Mở source](../../AGVwebserver/public/dashboard-map.js) · 399 dòng · 17 định nghĩa nhận diện.

- [encoderFresh — dòng 12](../../AGVwebserver/public/dashboard-map.js#L12)
- [robotSpeedText — dòng 19](../../AGVwebserver/public/dashboard-map.js#L19)
- [updateRobotMotionLabels — dòng 25](../../AGVwebserver/public/dashboard-map.js#L25)
- [markerPoint — dòng 51](../../AGVwebserver/public/dashboard-map.js#L51)
- [robotMarkerPoint — dòng 59](../../AGVwebserver/public/dashboard-map.js#L59)
- [animateRobotMarkers — dòng 100](../../AGVwebserver/public/dashboard-map.js#L100)
- [scheduleMapDocking — dòng 125](../../AGVwebserver/public/dashboard-map.js#L125)
- [svgElement — dòng 149](../../AGVwebserver/public/dashboard-map.js#L149)
- [computeMapTransform — dòng 156](../../AGVwebserver/public/dashboard-map.js#L156)
- [roundedEdgePath — dòng 168](../../AGVwebserver/public/dashboard-map.js#L168)
- [sonarDetections — dòng 179](../../AGVwebserver/public/dashboard-map.js#L179)
- [mapStructureKey — dòng 185](../../AGVwebserver/public/dashboard-map.js#L185)
- [renderStaticMap — dòng 194](../../AGVwebserver/public/dashboard-map.js#L194)
- [renderDynamicMap — dòng 303](../../AGVwebserver/public/dashboard-map.js#L303)
- [drawMap — dòng 384](../../AGVwebserver/public/dashboard-map.js#L384)
- [renderMap — dòng 389](../../AGVwebserver/public/dashboard-map.js#L389)
- [clearMapRenderCache — dòng 394](../../AGVwebserver/public/dashboard-map.js#L394)

## AGVwebserver/public/dashboard-missions.js

[Mở source](../../AGVwebserver/public/dashboard-missions.js) · 194 dòng · 10 định nghĩa nhận diện.

- [setMissionFilter — dòng 15](../../AGVwebserver/public/dashboard-missions.js#L15)
- [missionCell — dòng 21](../../AGVwebserver/public/dashboard-missions.js#L21)
- [missionText — dòng 29](../../AGVwebserver/public/dashboard-missions.js#L29)
- [missionBadge — dòng 37](../../AGVwebserver/public/dashboard-missions.js#L37)
- [missionTaskCanCancel — dòng 43](../../AGVwebserver/public/dashboard-missions.js#L43)
- [missionTaskActions — dòng 47](../../AGVwebserver/public/dashboard-missions.js#L47)
- [renderMissionTaskDetail — dòng 56](../../AGVwebserver/public/dashboard-missions.js#L56)
- [renderMissionOrderDetail — dòng 68](../../AGVwebserver/public/dashboard-missions.js#L68)
- [renderMissionRow — dòng 111](../../AGVwebserver/public/dashboard-missions.js#L111)
- [renderMissions — dòng 168](../../AGVwebserver/public/dashboard-missions.js#L168)

## AGVwebserver/public/dashboard-recovery.js

[Mở source](../../AGVwebserver/public/dashboard-recovery.js) · 121 dòng · 10 định nghĩa nhận diện.

- [openVehicleRecovery — dòng 4](../../AGVwebserver/public/dashboard-recovery.js#L4)
- [updateVehicleRecovery — dòng 12](../../AGVwebserver/public/dashboard-recovery.js#L12)
- [submitVehicleRecovery — dòng 56](../../AGVwebserver/public/dashboard-recovery.js#L56)
- [recoveryAllowsResume — dòng 69](../../AGVwebserver/public/dashboard-recovery.js#L69)
- [resumeStandaloneTask — dòng 77](../../AGVwebserver/public/dashboard-recovery.js#L77)
- [resumeWarehouseItem — dòng 83](../../AGVwebserver/public/dashboard-recovery.js#L83)
- [resumeWarehouseAction — dòng 90](../../AGVwebserver/public/dashboard-recovery.js#L90)
- [openShiftReview — dòng 96](../../AGVwebserver/public/dashboard-recovery.js#L96)
- [refreshShiftReview — dòng 99](../../AGVwebserver/public/dashboard-recovery.js#L99)
- [confirmShiftReview — dòng 116](../../AGVwebserver/public/dashboard-recovery.js#L116)

## AGVwebserver/public/dashboard-reports.js

[Mở source](../../AGVwebserver/public/dashboard-reports.js) · 101 dòng · 14 định nghĩa nhận diện.

- [reportText — dòng 4](../../AGVwebserver/public/dashboard-reports.js#L4)
- [reportMetric — dòng 7](../../AGVwebserver/public/dashboard-reports.js#L7)
- [reportDetails — dòng 10](../../AGVwebserver/public/dashboard-reports.js#L10)
- [reportTable — dòng 14](../../AGVwebserver/public/dashboard-reports.js#L14)
- [showReportSection — dòng 19](../../AGVwebserver/public/dashboard-reports.js#L19)
- [showReportLog — dòng 28](../../AGVwebserver/public/dashboard-reports.js#L28)
- [refreshReportLogs — dòng 33](../../AGVwebserver/public/dashboard-reports.js#L33)
- [fmtKpiDuration — dòng 38](../../AGVwebserver/public/dashboard-reports.js#L38)
- [syncKpiReportOptions — dòng 42](../../AGVwebserver/public/dashboard-reports.js#L42)
- [selectKpiReport — dòng 49](../../AGVwebserver/public/dashboard-reports.js#L49)
- [renderKpi — dòng 54](../../AGVwebserver/public/dashboard-reports.js#L54)
- [loadLatestKpi — dòng 86](../../AGVwebserver/public/dashboard-reports.js#L86)
- [loadKpiHistory — dòng 90](../../AGVwebserver/public/dashboard-reports.js#L90)
- [generateKpi — dòng 96](../../AGVwebserver/public/dashboard-reports.js#L96)

## AGVwebserver/public/dashboard-shared.js

[Mở source](../../AGVwebserver/public/dashboard-shared.js) · 44 dòng · 1 định nghĩa nhận diện.

- [fmtDate — dòng 40](../../AGVwebserver/public/dashboard-shared.js#L40)

## AGVwebserver/public/dashboard-vehicles.js

[Mở source](../../AGVwebserver/public/dashboard-vehicles.js) · 179 dòng · 10 định nghĩa nhận diện.

- [vehicleParticipates — dòng 9](../../AGVwebserver/public/dashboard-vehicles.js#L9)
- [vehicleCanSelect — dòng 17](../../AGVwebserver/public/dashboard-vehicles.js#L17)
- [vehicleBattery — dòng 24](../../AGVwebserver/public/dashboard-vehicles.js#L24)
- [vehicleCardKey — dòng 31](../../AGVwebserver/public/dashboard-vehicles.js#L31)
- [renderVehicleTelemetry — dòng 60](../../AGVwebserver/public/dashboard-vehicles.js#L60)
- [renderVehicleActions — dòng 88](../../AGVwebserver/public/dashboard-vehicles.js#L88)
- [updateVehicleCardControls — dòng 115](../../AGVwebserver/public/dashboard-vehicles.js#L115)
- [renderVehiclePlaceholder — dòng 128](../../AGVwebserver/public/dashboard-vehicles.js#L128)
- [markVehicleRemoved — dòng 141](../../AGVwebserver/public/dashboard-vehicles.js#L141)
- [renderVehicleCards — dòng 145](../../AGVwebserver/public/dashboard-vehicles.js#L145)

## AGVwebserver/public/dashboard.js

[Mở source](../../AGVwebserver/public/dashboard.js) · 702 dòng · 67 định nghĩa nhận diện.

- [snapshotAge — dòng 11](../../AGVwebserver/public/dashboard.js#L11)
- [dataFresh — dòng 15](../../AGVwebserver/public/dashboard.js#L15)
- [controlsReady — dòng 16](../../AGVwebserver/public/dashboard.js#L16)
- [tasksReady — dòng 17](../../AGVwebserver/public/dashboard.js#L17)
- [canEditMap — dòng 18](../../AGVwebserver/public/dashboard.js#L18)
- [setTaskMode — dòng 19](../../AGVwebserver/public/dashboard.js#L19)
- [showView — dòng 24](../../AGVwebserver/public/dashboard.js#L24)
- [toggleMapExpanded — dòng 37](../../AGVwebserver/public/dashboard.js#L37)
- [toggleMapZoom — dòng 44](../../AGVwebserver/public/dashboard.js#L44)
- [renderSummary — dòng 52](../../AGVwebserver/public/dashboard.js#L52)
- [populateNodeOptions — dòng 69](../../AGVwebserver/public/dashboard.js#L69)
- [renderObstacleAction — dòng 75](../../AGVwebserver/public/dashboard.js#L75)
- [updateConnection — dòng 83](../../AGVwebserver/public/dashboard.js#L83)
- [connect — dòng 116](../../AGVwebserver/public/dashboard.js#L116)
- [toast — dòng 165](../../AGVwebserver/public/dashboard.js#L165)
- [api — dòng 173](../../AGVwebserver/public/dashboard.js#L173)
- [control — dòng 193](../../AGVwebserver/public/dashboard.js#L193)
- [appendRow — dòng 203](../../AGVwebserver/public/dashboard.js#L203)
- [action — dòng 208](../../AGVwebserver/public/dashboard.js#L208)
- [emptyTable — dòng 213](../../AGVwebserver/public/dashboard.js#L213)
- [recordDeleteButton — dòng 214](../../AGVwebserver/public/dashboard.js#L214)
- [deleteDashboardRecord — dòng 223](../../AGVwebserver/public/dashboard.js#L223)
- [warehouseRollbackLabel — dòng 243](../../AGVwebserver/public/dashboard.js#L243)
- [resolveWarehouseOrder — dòng 249](../../AGVwebserver/public/dashboard.js#L249)
- [updateRobotAges — dòng 262](../../AGVwebserver/public/dashboard.js#L262)
- [renderTasks — dòng 276](../../AGVwebserver/public/dashboard.js#L276)
- [addOrderLine — dòng 282](../../AGVwebserver/public/dashboard.js#L282)
- [populateOrderWarehouses — dòng 294](../../AGVwebserver/public/dashboard.js#L294)
- [orderLines — dòng 308](../../AGVwebserver/public/dashboard.js#L308)
- [updateOrderTotals — dòng 312](../../AGVwebserver/public/dashboard.js#L312)
- [submitOrder — dòng 319](../../AGVwebserver/public/dashboard.js#L319)
- [singleWarehouseRobot — dòng 327](../../AGVwebserver/public/dashboard.js#L327)
- [updateWarehouseControls — dòng 332](../../AGVwebserver/public/dashboard.js#L332)
- [warehouseAction — dòng 339](../../AGVwebserver/public/dashboard.js#L339)
- [renderWarehouseActionStatus — dòng 344](../../AGVwebserver/public/dashboard.js#L344)
- [updateOrderTimers — dòng 360](../../AGVwebserver/public/dashboard.js#L360)
- [renderOrderProgress — dòng 367](../../AGVwebserver/public/dashboard.js#L367)
- [renderOrders — dòng 394](../../AGVwebserver/public/dashboard.js#L394)
- [renderMutex — dòng 405](../../AGVwebserver/public/dashboard.js#L405)
- [populateTaskAgvSelect — dòng 425](../../AGVwebserver/public/dashboard.js#L425)
- [render — dòng 470](../../AGVwebserver/public/dashboard.js#L470)
- [renderEmpty — dòng 483](../../AGVwebserver/public/dashboard.js#L483)
- [setSelectionText — dòng 491](../../AGVwebserver/public/dashboard.js#L491)
- [selectAgv — dòng 492](../../AGVwebserver/public/dashboard.js#L492)
- [cancelSelection — dòng 498](../../AGVwebserver/public/dashboard.js#L498)
- [onNodeClick — dòng 502](../../AGVwebserver/public/dashboard.js#L502)
- [addTask — dòng 516](../../AGVwebserver/public/dashboard.js#L516)
- [renderShift — dòng 525](../../AGVwebserver/public/dashboard.js#L525)
- [changeShift — dòng 569](../../AGVwebserver/public/dashboard.js#L569)
- [cancelTask — dòng 575](../../AGVwebserver/public/dashboard.js#L575)
- [removeAGV — dòng 580](../../AGVwebserver/public/dashboard.js#L580)
- [setObstacle — dòng 589](../../AGVwebserver/public/dashboard.js#L589)
- [toggleObstacle — dòng 593](../../AGVwebserver/public/dashboard.js#L593)
- [saveMap — dòng 598](../../AGVwebserver/public/dashboard.js#L598)
- [saveNamedMap — dòng 602](../../AGVwebserver/public/dashboard.js#L602)
- [loadMapList — dòng 606](../../AGVwebserver/public/dashboard.js#L606)
- [viewSavedMap — dòng 616](../../AGVwebserver/public/dashboard.js#L616)
- [closeMapPreview — dòng 621](../../AGVwebserver/public/dashboard.js#L621)
- [applyPreviewedMap — dòng 622](../../AGVwebserver/public/dashboard.js#L622)
- [deletePreviewedMap — dòng 626](../../AGVwebserver/public/dashboard.js#L626)
- [processServerLogs — dòng 630](../../AGVwebserver/public/dashboard.js#L630)
- [renderSystemLog — dòng 641](../../AGVwebserver/public/dashboard.js#L641)
- [loadStoredLogs — dòng 648](../../AGVwebserver/public/dashboard.js#L648)
- [showKpiAlert — dòng 652](../../AGVwebserver/public/dashboard.js#L652)
- [loadRobotHistory — dòng 656](../../AGVwebserver/public/dashboard.js#L656)
- [checkAuthentication — dòng 667](../../AGVwebserver/public/dashboard.js#L667)
- [logout — dòng 677](../../AGVwebserver/public/dashboard.js#L677)

## AGVwebserver/public/mission-model.js

[Mở source](../../AGVwebserver/public/mission-model.js) · 73 dòng · 4 định nghĩa nhận diện.

- [missionLocationText — dòng 3](../../AGVwebserver/public/mission-model.js#L3)
- [missionCategory — dòng 8](../../AGVwebserver/public/mission-model.js#L8)
- [missionTime — dòng 15](../../AGVwebserver/public/mission-model.js#L15)
- [buildMissionRows — dòng 20](../../AGVwebserver/public/mission-model.js#L20)

## AGVwebserver/public/warehouse-operations.js

[Mở source](../../AGVwebserver/public/warehouse-operations.js) · 212 dòng · 14 định nghĩa nhận diện.

- [operationText — dòng 10](../../AGVwebserver/public/warehouse-operations.js#L10)
- [operationBadge — dòng 11](../../AGVwebserver/public/warehouse-operations.js#L11)
- [transferRoute — dòng 14](../../AGVwebserver/public/warehouse-operations.js#L14)
- [transferStage — dòng 23](../../AGVwebserver/public/warehouse-operations.js#L23)
- [robotTransfer — dòng 31](../../AGVwebserver/public/warehouse-operations.js#L31)
- [renderRobotTransfer — dòng 40](../../AGVwebserver/public/warehouse-operations.js#L40)
- [renderDeletedOrderAttention — dòng 82](../../AGVwebserver/public/warehouse-operations.js#L82)
- [renderWarehouseSlot — dòng 94](../../AGVwebserver/public/warehouse-operations.js#L94)
- [renderWarehouseOperations — dòng 118](../../AGVwebserver/public/warehouse-operations.js#L118)
- [renderWarehouseInventory — dòng 128](../../AGVwebserver/public/warehouse-operations.js#L128)
- [orderWarehouseAvailability — dòng 161](../../AGVwebserver/public/warehouse-operations.js#L161)
- [refreshOrderWarehouseOptions — dòng 169](../../AGVwebserver/public/warehouse-operations.js#L169)
- [validateTransferForm — dòng 184](../../AGVwebserver/public/warehouse-operations.js#L184)
- [renderTransferForm — dòng 200](../../AGVwebserver/public/warehouse-operations.js#L200)

## AGVwebserver/public/warehouse-rules.js

[Mở source](../../AGVwebserver/public/warehouse-rules.js) · 15 dòng · 2 định nghĩa nhận diện.

- [isWarehouseNode — dòng 5](../../AGVwebserver/public/warehouse-rules.js#L5)
- [warehouseNodes — dòng 9](../../AGVwebserver/public/warehouse-rules.js#L9)

## AGVwebserver/public/warehouse-ui.js

[Mở source](../../AGVwebserver/public/warehouse-ui.js) · 175 dòng · 15 định nghĩa nhận diện.

- [warehouseText — dòng 7](../../AGVwebserver/public/warehouse-ui.js#L7)
- [warehouseRows — dòng 10](../../AGVwebserver/public/warehouse-ui.js#L10)
- [renderWarehouse — dòng 22](../../AGVwebserver/public/warehouse-ui.js#L22)
- [renderWarehouseEvents — dòng 30](../../AGVwebserver/public/warehouse-ui.js#L30)
- [updateWarehouseInventoryControls — dòng 35](../../AGVwebserver/public/warehouse-ui.js#L35)
- [fillWarehouseFields — dòng 51](../../AGVwebserver/public/warehouse-ui.js#L51)
- [editWarehouseLayout — dòng 68](../../AGVwebserver/public/warehouse-ui.js#L68)
- [warehouseSlots — dòng 73](../../AGVwebserver/public/warehouse-ui.js#L73)
- [saveWarehouseLayout — dòng 76](../../AGVwebserver/public/warehouse-ui.js#L76)
- [openShiftWarehouse — dòng 85](../../AGVwebserver/public/warehouse-ui.js#L85)
- [cancelShiftWarehouse — dòng 96](../../AGVwebserver/public/warehouse-ui.js#L96)
- [confirmShiftWarehouse — dòng 100](../../AGVwebserver/public/warehouse-ui.js#L100)
- [loadWarehouseReports — dòng 127](../../AGVwebserver/public/warehouse-ui.js#L127)
- [selectWarehouseReport — dòng 139](../../AGVwebserver/public/warehouse-ui.js#L139)
- [showWarehouseReport — dòng 142](../../AGVwebserver/public/warehouse-ui.js#L142)

## AGVwebserver/security.js

[Mở source](../../AGVwebserver/security.js) · 36 dòng · 3 định nghĩa nhận diện.

- [createSecurity — dòng 2](../../AGVwebserver/security.js#L2)
- [originAllowed — dòng 14](../../AGVwebserver/security.js#L14)
- [middleware — dòng 27](../../AGVwebserver/security.js#L27)

## AGVwebserver/server.js

[Mở source](../../AGVwebserver/server.js) · 14 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## AGVwebserver/shift-review.js

[Mở source](../../AGVwebserver/shift-review.js) · 62 dòng · 4 định nghĩa nhận diện.

- [createShiftReview — dòng 5](../../AGVwebserver/shift-review.js#L5)
- [observe — dòng 13](../../AGVwebserver/shift-review.js#L13)
- [snapshot — dòng 17](../../AGVwebserver/shift-review.js#L17)
- [reconcile — dòng 51](../../AGVwebserver/shift-review.js#L51)

## AGVwebserver/warehouse-actions.js

[Mở source](../../AGVwebserver/warehouse-actions.js) · 188 dòng · 14 định nghĩa nhận diện.

- [createWarehouseActions — dòng 9](../../AGVwebserver/warehouse-actions.js#L9)
- [interrupt — dòng 15](../../AGVwebserver/warehouse-actions.js#L15)
- [endShift — dòng 24](../../AGVwebserver/warehouse-actions.js#L24)
- [resume — dòng 28](../../AGVwebserver/warehouse-actions.js#L28)
- [at — dòng 54](../../AGVwebserver/warehouse-actions.js#L54)
- [complete — dòng 56](../../AGVwebserver/warehouse-actions.js#L56)
- [servoReady — dòng 57](../../AGVwebserver/warehouse-actions.js#L57)
- [startServo — dòng 60](../../AGVwebserver/warehouse-actions.js#L60)
- [beginService — dòng 70](../../AGVwebserver/warehouse-actions.js#L70)
- [inspectAndStart — dòng 77](../../AGVwebserver/warehouse-actions.js#L77)
- [dispatch — dòng 85](../../AGVwebserver/warehouse-actions.js#L85)
- [create — dòng 94](../../AGVwebserver/warehouse-actions.js#L94)
- [advance — dòng 127](../../AGVwebserver/warehouse-actions.js#L127)
- [tick — dòng 182](../../AGVwebserver/warehouse-actions.js#L182)

## AGVwebserver/warehouse-inventory.js

[Mở source](../../AGVwebserver/warehouse-inventory.js) · 341 dòng · 19 định nghĩa nhận diện.

- [createWarehouseInventory — dòng 11](../../AGVwebserver/warehouse-inventory.js#L11)
- [reconciliationFor — dòng 37](../../AGVwebserver/warehouse-inventory.js#L37)
- [change — dòng 52](../../AGVwebserver/warehouse-inventory.js#L52)
- [markInterrupted — dòng 68](../../AGVwebserver/warehouse-inventory.js#L68)
- [interrupt — dòng 75](../../AGVwebserver/warehouse-inventory.js#L75)
- [observationGap — dòng 79](../../AGVwebserver/warehouse-inventory.js#L79)
- [configure — dòng 97](../../AGVwebserver/warehouse-inventory.js#L97)
- [checkMove — dòng 123](../../AGVwebserver/warehouse-inventory.js#L123)
- [assertPlan — dòng 148](../../AGVwebserver/warehouse-inventory.js#L148)
- [reserve — dòng 156](../../AGVwebserver/warehouse-inventory.js#L156)
- [checkResume — dòng 165](../../AGVwebserver/warehouse-inventory.js#L165)
- [resumeMove — dòng 188](../../AGVwebserver/warehouse-inventory.js#L188)
- [confirmLift — dòng 199](../../AGVwebserver/warehouse-inventory.js#L199)
- [confirmMissing — dòng 229](../../AGVwebserver/warehouse-inventory.js#L229)
- [rollbackPlan — dòng 250](../../AGVwebserver/warehouse-inventory.js#L250)
- [rollbackOrder — dòng 280](../../AGVwebserver/warehouse-inventory.js#L280)
- [finishShift — dòng 298](../../AGVwebserver/warehouse-inventory.js#L298)
- [observeShift — dòng 317](../../AGVwebserver/warehouse-inventory.js#L317)
- [currentSnapshot — dòng 331](../../AGVwebserver/warehouse-inventory.js#L331)

## firmware/AGV_Connection_Test/AGV_Connection_Test.ino

[Mở source](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino) · 660 dòng · 32 định nghĩa nhận diện.

- [knownNode — dòng 116](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L116)
- [homeStation — dòng 121](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L121)
- [modeName — dòng 126](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L126)
- [nodeIndex — dòng 130](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L130)
- [defaultUidForNode — dòng 134](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L134)
- [normalizeUid — dòng 139](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L139)
- [enterFailsafe — dòng 153](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L153)
- [invalidatePosition — dòng 158](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L158)
- [observeNode — dòng 164](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L164)
- [initRfid — dòng 189](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L189)
- [onUidRead — dòng 230](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L230)
- [pollRfid — dòng 254](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L254)
- [zoneIndex — dòng 274](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L274)
- [sameCommand — dòng 279](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L279)
- [parseCommand — dòng 285](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L285)
- [publishText — dòng 315](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L315)
- [sendAck — dòng 322](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L322)
- [processCommand — dòng 331](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L331)
- [onMqttMessage — dòng 381](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L381)
- [processGrant — dòng 400](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L400)
- [retryMutexRequests — dòng 417](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L417)
- [sendTelemetry — dòng 423](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L423)
- [parsePercent — dòng 431](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L431)
- [printHelp — dòng 438](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L438)
- [requestZoneForNextSegment — dòng 447](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L447)
- [hasZoneGrantsForNextSegment — dòng 457](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L457)
- [nextSegmentPermitted — dòng 465](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L465)
- [handleSerialLine — dòng 474](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L474)
- [readSerial — dòng 549](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L549)
- [serviceConnections — dòng 564](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L564)
- [setup — dòng 594](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L594)
- [loop — dòng 642](../../firmware/AGV_Connection_Test/AGV_Connection_Test.ino#L642)

## firmware/AGV_Connection_Test/Config.h

[Mở source](../../firmware/AGV_Connection_Test/Config.h) · 20 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/AGV_Connection_Test/TestConfig.h

[Mở source](../../firmware/AGV_Connection_Test/TestConfig.h) · 48 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/AGV_Connection_Test/driver.cpp

[Mở source](../../firmware/AGV_Connection_Test/driver.cpp) · 187 dòng · 19 định nghĩa nhận diện.

- [begin — dòng 8](../../firmware/AGV_Connection_Test/driver.cpp#L8)
- [stop — dòng 9](../../firmware/AGV_Connection_Test/driver.cpp#L9)
- [tick — dòng 10](../../firmware/AGV_Connection_Test/driver.cpp#L10)
- [obstacle — dòng 11](../../firmware/AGV_Connection_Test/driver.cpp#L11)
- [lineFault — dòng 12](../../firmware/AGV_Connection_Test/driver.cpp#L12)
- [motorRunning — dòng 13](../../firmware/AGV_Connection_Test/driver.cpp#L13)
- [setSide — dòng 27](../../firmware/AGV_Connection_Test/driver.cpp#L27)
- [drive — dòng 39](../../firmware/AGV_Connection_Test/driver.cpp#L39)
- [echoChangeIsr — dòng 45](../../firmware/AGV_Connection_Test/driver.cpp#L45)
- [startPing — dòng 60](../../firmware/AGV_Connection_Test/driver.cpp#L60)
- [pollSonar — dòng 74](../../firmware/AGV_Connection_Test/driver.cpp#L74)
- [setServoAngle — dòng 102](../../firmware/AGV_Connection_Test/driver.cpp#L102)
- [pollLine — dòng 109](../../firmware/AGV_Connection_Test/driver.cpp#L109)
- [begin — dòng 130](../../firmware/AGV_Connection_Test/driver.cpp#L130)
- [stop — dòng 158](../../firmware/AGV_Connection_Test/driver.cpp#L158)
- [tick — dòng 159](../../firmware/AGV_Connection_Test/driver.cpp#L159)
- [obstacle — dòng 183](../../firmware/AGV_Connection_Test/driver.cpp#L183)
- [lineFault — dòng 184](../../firmware/AGV_Connection_Test/driver.cpp#L184)
- [motorRunning — dòng 185](../../firmware/AGV_Connection_Test/driver.cpp#L185)

## firmware/AGV_Connection_Test/driver.h

[Mở source](../../firmware/AGV_Connection_Test/driver.h) · 11 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/Config.h

[Mở source](../../firmware/NCKHAGV/src/Config.h) · 867 dòng · 3 định nghĩa nhận diện.

- [benchVoltageReady — dòng 681](../../firmware/NCKHAGV/src/Config.h#L681)
- [batteryStepPwmOffset — dòng 738](../../firmware/NCKHAGV/src/Config.h#L738)
- [pinsUnique — dòng 816](../../firmware/NCKHAGV/src/Config.h#L816)

## firmware/NCKHAGV/src/Types.h

[Mở source](../../firmware/NCKHAGV/src/Types.h) · 154 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/actuators/BuzzerModule.h

[Mở source](../../firmware/NCKHAGV/src/actuators/BuzzerModule.h) · 17 dòng · 4 định nghĩa nhận diện.

- [begin — dòng 7](../../firmware/NCKHAGV/src/actuators/BuzzerModule.h#L7)
- [setReverse — dòng 12](../../firmware/NCKHAGV/src/actuators/BuzzerModule.h#L12)
- [off — dòng 15](../../firmware/NCKHAGV/src/actuators/BuzzerModule.h#L15)
- [pinLevel — dòng 16](../../firmware/NCKHAGV/src/actuators/BuzzerModule.h#L16)

## firmware/NCKHAGV/src/actuators/ServoModule.cpp

[Mở source](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp) · 57 dòng · 8 định nghĩa nhận diện.

- [ServoModule::rolesValid — dòng 3](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp#L3)
- [ServoModule::begin — dòng 12](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp#L12)
- [ServoModule::liftConfigured — dòng 23](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp#L23)
- [ServoModule::setAngle — dòng 36](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp#L36)
- [ServoModule::liftUp — dòng 46](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp#L46)
- [ServoModule::liftDown — dòng 51](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp#L51)
- [ServoModule::update — dòng 56](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp#L56)
- [ServoModule::getAngle — dòng 57](../../firmware/NCKHAGV/src/actuators/ServoModule.cpp#L57)

## firmware/NCKHAGV/src/actuators/ServoModule.h

[Mở source](../../firmware/NCKHAGV/src/actuators/ServoModule.h) · 21 dòng · 1 định nghĩa nhận diện.

- [busy — dòng 14](../../firmware/NCKHAGV/src/actuators/ServoModule.h#L14)

## firmware/NCKHAGV/src/core/AGVController.cpp

[Mở source](../../firmware/NCKHAGV/src/core/AGVController.cpp) · 758 dòng · 10 định nghĩa nhận diện.

- [AGVController::updateSonars — dòng 10](../../firmware/NCKHAGV/src/core/AGVController.cpp#L10)
- [AGVController::logSonars — dòng 23](../../firmware/NCKHAGV/src/core/AGVController.cpp#L23)
- [AGVController::begin — dòng 39](../../firmware/NCKHAGV/src/core/AGVController.cpp#L39)
- [AGVController::input — dòng 77](../../firmware/NCKHAGV/src/core/AGVController.cpp#L77)
- [AGVController::consumePriority — dòng 100](../../firmware/NCKHAGV/src/core/AGVController.cpp#L100)
- [AGVController::handleCommand — dòng 122](../../firmware/NCKHAGV/src/core/AGVController.cpp#L122)
- [AGVController::update — dòng 280](../../firmware/NCKHAGV/src/core/AGVController.cpp#L280)
- [AGVController::observeState — dòng 590](../../firmware/NCKHAGV/src/core/AGVController.cpp#L590)
- [AGVController::publishTelemetry — dòng 603](../../firmware/NCKHAGV/src/core/AGVController.cpp#L603)
- [AGVController::applyStartupBoostCheck — dòng 673](../../firmware/NCKHAGV/src/core/AGVController.cpp#L673)

## firmware/NCKHAGV/src/core/AGVController.h

[Mở source](../../firmware/NCKHAGV/src/core/AGVController.h) · 100 dòng · 2 định nghĩa nhận diện.

- [state — dòng 23](../../firmware/NCKHAGV/src/core/AGVController.h#L23)
- [getMotionGroup — dòng 72](../../firmware/NCKHAGV/src/core/AGVController.h#L72)

## firmware/NCKHAGV/src/core/CommandCodec.cpp

[Mở source](../../firmware/NCKHAGV/src/core/CommandCodec.cpp) · 104 dòng · 6 định nghĩa nhận diện.

- [read — dòng 10](../../firmware/NCKHAGV/src/core/CommandCodec.cpp#L10)
- [readBytes — dòng 11](../../firmware/NCKHAGV/src/core/CommandCodec.cpp#L11)
- [identifier — dòng 19](../../firmware/NCKHAGV/src/core/CommandCodec.cpp#L19)
- [text — dòng 26](../../firmware/NCKHAGV/src/core/CommandCodec.cpp#L26)
- [departure — dòng 31](../../firmware/NCKHAGV/src/core/CommandCodec.cpp#L31)
- [CommandCodec::decode — dòng 41](../../firmware/NCKHAGV/src/core/CommandCodec.cpp#L41)

## firmware/NCKHAGV/src/core/CommandCodec.h

[Mở source](../../firmware/NCKHAGV/src/core/CommandCodec.h) · 6 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/core/CommandReplayGuard.h

[Mở source](../../firmware/NCKHAGV/src/core/CommandReplayGuard.h) · 19 dòng · 1 định nghĩa nhận diện.

- [classify — dòng 10](../../firmware/NCKHAGV/src/core/CommandReplayGuard.h#L10)

## firmware/NCKHAGV/src/core/ControlLogic.cpp

[Mở source](../../firmware/NCKHAGV/src/core/ControlLogic.cpp) · 452 dòng · 33 định nghĩa nhận diện.

- [headingName — dòng 5](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L5)
- [parseHeading — dòng 14](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L14)
- [stateName — dòng 23](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L23)
- [errorName — dòng 36](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L36)
- [motorActionName — dòng 80](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L80)
- [motionVoltageError — dòng 98](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L98)
- [motionStartError — dòng 105](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L105)
- [copyText — dòng 118](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L118)
- [reverseBuzzerRequired — dòng 123](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L123)
- [distanceCm — dòng 128](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L128)
- [batteryVoltage — dòng 132](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L132)
- [batteryPercentage — dòng 137](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L137)
- [normalizeLine — dòng 142](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L142)
- [lineAction — dòng 146](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L146)
- [reverseLineAction — dòng 158](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L158)
- [validNode — dòng 166](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L166)
- [validTask — dòng 172](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L172)
- [ControlLogic::begin — dòng 189](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L189)
- [ControlLogic::safeStop — dòng 193](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L193)
- [ControlLogic::fail — dòng 201](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L201)
- [ControlLogic::pause — dòng 205](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L205)
- [ControlLogic::cancelTask — dòng 216](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L216)
- [ControlLogic::setTask — dòng 222](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L222)
- [ControlLogic::motionClear — dòng 234](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L234)
- [ControlLogic::enterServoState — dòng 242](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L242)
- [ControlLogic::beginSegment — dòng 250](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L250)
- [ControlLogic::setNodeOrder — dòng 293](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L293)
- [ControlLogic::authorizeLift — dòng 317](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L317)
- [ControlLogic::start — dòng 333](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L333)
- [ControlLogic::restart — dòng 347](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L347)
- [ControlLogic::reachedNode — dòng 360](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L360)
- [ControlLogic::checkSafety — dòng 386](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L386)
- [ControlLogic::update — dòng 402](../../firmware/NCKHAGV/src/core/ControlLogic.cpp#L402)

## firmware/NCKHAGV/src/core/ControlLogic.h

[Mở source](../../firmware/NCKHAGV/src/core/ControlLogic.h) · 70 dòng · 8 định nghĩa nhận diện.

- [state — dòng 43](../../firmware/NCKHAGV/src/core/ControlLogic.h#L43)
- [error — dòng 44](../../firmware/NCKHAGV/src/core/ControlLogic.h#L44)
- [task — dòng 45](../../firmware/NCKHAGV/src/core/ControlLogic.h#L45)
- [currentNode — dòng 46](../../firmware/NCKHAGV/src/core/ControlLogic.h#L46)
- [pathIndex — dòng 47](../../firmware/NCKHAGV/src/core/ControlLogic.h#L47)
- [pickupDone — dòng 48](../../firmware/NCKHAGV/src/core/ControlLogic.h#L48)
- [waitingForOrder — dòng 49](../../firmware/NCKHAGV/src/core/ControlLogic.h#L49)
- [segmentReverse — dòng 50](../../firmware/NCKHAGV/src/core/ControlLogic.h#L50)

## firmware/NCKHAGV/src/core/CoreNavigator.cpp

[Mở source](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp) · 1019 dòng · 30 định nghĩa nhận diện.

- [CoreNavigator::resetLineTracking — dòng 7](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L7)
- [CoreNavigator::rememberLine — dòng 24](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L24)
- [CoreNavigator::recoverLine — dòng 51](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L51)
- [CoreNavigator::confirmTrackedLine — dòng 153](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L153)
- [CoreNavigator::alignTurnLine — dòng 172](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L172)
- [CoreNavigator::zoneIndex — dòng 327](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L327)
- [CoreNavigator::hasGrant — dòng 331](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L331)
- [CoreNavigator::currentZonePermitted — dòng 335](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L335)
- [CoreNavigator::missingZone — dòng 338](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L338)
- [CoreNavigator::requestedZone — dòng 345](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L345)
- [CoreNavigator::canPassThrough — dòng 353](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L353)
- [CoreNavigator::start — dòng 363](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L363)
- [CoreNavigator::stop — dòng 407](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L407)
- [CoreNavigator::clearFault — dòng 420](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L420)
- [CoreNavigator::fault — dòng 435](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L435)
- [CoreNavigator::reachedNode — dòng 448](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L448)
- [CoreNavigator::grant — dòng 498](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L498)
- [CoreNavigator::revokeGrant — dòng 512](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L512)
- [CoreNavigator::completeZoneExit — dòng 516](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L516)
- [CoreNavigator::resetGrants — dòng 524](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L524)
- [CoreNavigator::sampleTurnLine — dòng 529](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L529)
- [CoreNavigator::observeTurnAroundLine — dòng 537](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L537)
- [CoreNavigator::confirmTurnAround — dòng 579](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L579)
- [CoreNavigator::observeTurnLine — dòng 641](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L641)
- [CoreNavigator::refreshFollowing — dòng 673](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L673)
- [CoreNavigator::update — dòng 680](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L680)
- [CoreNavigator::state — dòng 991](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L991)
- [CoreNavigator::nextNode — dòng 999](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L999)
- [CoreNavigator::needsNodeOrder — dòng 1001](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L1001)
- [CoreNavigator::authorizeNodeOrder — dòng 1005](../../firmware/NCKHAGV/src/core/CoreNavigator.cpp#L1005)

## firmware/NCKHAGV/src/core/CoreNavigator.h

[Mở source](../../firmware/NCKHAGV/src/core/CoreNavigator.h) · 169 dòng · 28 định nghĩa nhận diện.

- [stationTurnRequired — dòng 14](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L14)
- [routeCommandId — dòng 15](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L15)
- [capturingTurnLine — dòng 34](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L34)
- [capturingTurnAroundLine — dòng 35](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L35)
- [fastTurnSampling — dòng 37](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L37)
- [turnMaxSampleGapMs — dòng 42](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L42)
- [settlingTurnLine — dòng 43](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L43)
- [turnCaptureEnabled — dòng 44](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L44)
- [turnCapturePwm — dòng 49](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L49)
- [precisionPivot — dòng 55](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L55)
- [precisionPivotPwm — dòng 59](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L59)
- [active — dòng 72](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L72)
- [following — dòng 73](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L73)
- [recoveringLine — dòng 74](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L74)
- [cornerRecovering — dòng 75](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L75)
- [turn180Recovering — dòng 76](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L76)
- [obstaclePaused — dòng 77](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L77)
- [networkPaused — dòng 78](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L78)
- [forwardLineTracking — dòng 79](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L79)
- [stationOriented — dòng 82](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L82)
- [error — dòng 84](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L84)
- [pathIndex — dòng 86](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L86)
- [pathLength — dòng 87](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L87)
- [reverseAligned — dòng 88](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L88)
- [segmentReverse — dòng 91](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L91)
- [isDockingTurn — dòng 98](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L98)
- [isBayExit — dòng 101](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L101)
- [g1TurnAroundPrecision — dòng 104](../../firmware/NCKHAGV/src/core/CoreNavigator.h#L104)

## firmware/NCKHAGV/src/core/CoreProtocol.cpp

[Mở source](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp) · 199 dòng · 10 định nghĩa nhận diện.

- [read — dòng 13](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L13)
- [readBytes — dòng 14](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L14)
- [nodeText — dòng 23](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L23)
- [CoreProtocol::decodeCommand — dòng 31](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L31)
- [CoreProtocol::sameCommand — dòng 125](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L125)
- [CoreProtocol::decodeGrant — dòng 136](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L136)
- [CoreProtocol::grantAck — dòng 151](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L151)
- [CoreProtocol::telemetry — dòng 159](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L159)
- [CoreProtocol::ack — dòng 168](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L168)
- [CoreProtocol::odometry — dòng 174](../../firmware/NCKHAGV/src/core/CoreProtocol.cpp#L174)

## firmware/NCKHAGV/src/core/CoreProtocol.h

[Mở source](../../firmware/NCKHAGV/src/core/CoreProtocol.h) · 17 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/core/LiftCycle.h

[Mở source](../../firmware/NCKHAGV/src/core/LiftCycle.h) · 62 dòng · 14 định nghĩa nhận diện.

- [start — dòng 8](../../firmware/NCKHAGV/src/core/LiftCycle.h#L8)
- [update — dòng 15](../../firmware/NCKHAGV/src/core/LiftCycle.h#L15)
- [exitRouteAllowed — dòng 27](../../firmware/NCKHAGV/src/core/LiftCycle.h#L27)
- [observeExit — dòng 34](../../firmware/NCKHAGV/src/core/LiftCycle.h#L34)
- [exitPending — dòng 45](../../firmware/NCKHAGV/src/core/LiftCycle.h#L45)
- [interrupt — dòng 46](../../firmware/NCKHAGV/src/core/LiftCycle.h#L46)
- [busy — dòng 47](../../firmware/NCKHAGV/src/core/LiftCycle.h#L47)
- [up — dòng 48](../../firmware/NCKHAGV/src/core/LiftCycle.h#L48)
- [cargoKnown — dòng 49](../../firmware/NCKHAGV/src/core/LiftCycle.h#L49)
- [cargo — dòng 50](../../firmware/NCKHAGV/src/core/LiftCycle.h#L50)
- [cmdId — dòng 51](../../firmware/NCKHAGV/src/core/LiftCycle.h#L51)
- [node — dòng 52](../../firmware/NCKHAGV/src/core/LiftCycle.h#L52)
- [state — dòng 53](../../firmware/NCKHAGV/src/core/LiftCycle.h#L53)
- [elapsed — dòng 54](../../firmware/NCKHAGV/src/core/LiftCycle.h#L54)

## firmware/NCKHAGV/src/core/LinePdController.h

[Mở source](../../firmware/NCKHAGV/src/core/LinePdController.h) · 49 dòng · 2 định nghĩa nhận diện.

- [reset — dòng 14](../../firmware/NCKHAGV/src/core/LinePdController.h#L14)
- [update — dòng 15](../../firmware/NCKHAGV/src/core/LinePdController.h#L15)

## firmware/NCKHAGV/src/core/MutexClearance.h

[Mở source](../../firmware/NCKHAGV/src/core/MutexClearance.h) · 188 dòng · 15 định nghĩa nhận diện.

- [same — dòng 9](../../firmware/NCKHAGV/src/core/MutexClearance.h#L9)
- [node — dòng 13](../../firmware/NCKHAGV/src/core/MutexClearance.h#L13)
- [squared — dòng 17](../../firmware/NCKHAGV/src/core/MutexClearance.h#L17)
- [straight35 — dòng 23](../../firmware/NCKHAGV/src/core/MutexClearance.h#L23)
- [reset — dòng 53](../../firmware/NCKHAGV/src/core/MutexClearance.h#L53)
- [canAcceptGrant — dòng 55](../../firmware/NCKHAGV/src/core/MutexClearance.h#L55)
- [grant — dòng 62](../../firmware/NCKHAGV/src/core/MutexClearance.h#L62)
- [seen — dòng 73](../../firmware/NCKHAGV/src/core/MutexClearance.h#L73)
- [reached — dòng 80](../../firmware/NCKHAGV/src/core/MutexClearance.h#L80)
- [progress — dòng 115](../../firmware/NCKHAGV/src/core/MutexClearance.h#L115)
- [pending — dòng 132](../../firmware/NCKHAGV/src/core/MutexClearance.h#L132)
- [queued — dòng 149](../../firmware/NCKHAGV/src/core/MutexClearance.h#L149)
- [zoneIndex — dòng 167](../../firmware/NCKHAGV/src/core/MutexClearance.h#L167)
- [exitStage — dòng 172](../../firmware/NCKHAGV/src/core/MutexClearance.h#L172)
- [secondTag — dòng 182](../../firmware/NCKHAGV/src/core/MutexClearance.h#L182)

## firmware/NCKHAGV/src/core/OdometryTracker.h

[Mở source](../../firmware/NCKHAGV/src/core/OdometryTracker.h) · 60 dòng · 7 định nghĩa nhận diện.

- [OdometryTracker — dòng 10](../../firmware/NCKHAGV/src/core/OdometryTracker.h#L10)
- [cmPerTick_ — dòng 11](../../firmware/NCKHAGV/src/core/OdometryTracker.h#L11)
- [begin — dòng 12](../../firmware/NCKHAGV/src/core/OdometryTracker.h#L12)
- [update — dòng 16](../../firmware/NCKHAGV/src/core/OdometryTracker.h#L16)
- [context — dòng 32](../../firmware/NCKHAGV/src/core/OdometryTracker.h#L32)
- [snapshot — dòng 49](../../firmware/NCKHAGV/src/core/OdometryTracker.h#L49)
- [translationTicks — dòng 54](../../firmware/NCKHAGV/src/core/OdometryTracker.h#L54)

## firmware/NCKHAGV/src/core/RoutePlanner.h

[Mở source](../../firmware/NCKHAGV/src/core/RoutePlanner.h) · 209 dòng · 18 định nghĩa nhận diện.

- [confirmedIncoming — dòng 20](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L20)
- [turnName — dòng 25](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L25)
- [node — dòng 37](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L37)
- [directedEdge — dòng 44](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L44)
- [edgeHeading — dòng 53](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L53)
- [turnFromHeading — dòng 68](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L68)
- [station — dòng 77](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L77)
- [bayGate — dòng 80](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L80)
- [bayHeading — dòng 90](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L90)
- [bayExit — dòng 94](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L94)
- [dockingReverse — dòng 98](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L98)
- [reverseHome — dòng 101](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L101)
- [straightThrough — dòng 106](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L106)
- [physicalBranch — dòng 111](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L111)
- [reverseAllowed — dòng 118](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L118)
- [oneRobotTrialRoute — dòng 131](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L131)
- [turn — dòng 149](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L149)
- [plan — dòng 159](../../firmware/NCKHAGV/src/core/RoutePlanner.h#L159)

## firmware/NCKHAGV/src/core/StoppedPosition.h

[Mở source](../../firmware/NCKHAGV/src/core/StoppedPosition.h) · 73 dòng · 2 định nghĩa nhận diện.

- [update — dòng 9](../../firmware/NCKHAGV/src/core/StoppedPosition.h#L9)
- [attach — dòng 55](../../firmware/NCKHAGV/src/core/StoppedPosition.h#L55)

## firmware/NCKHAGV/src/main.cpp

[Mở source](../../firmware/NCKHAGV/src/main.cpp) · 16 dòng · 2 định nghĩa nhận diện.

- [setup — dòng 8](../../firmware/NCKHAGV/src/main.cpp#L8)
- [loop — dòng 13](../../firmware/NCKHAGV/src/main.cpp#L13)

## firmware/NCKHAGV/src/motion/LineFollowModule.cpp

[Mở source](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp) · 55 dòng · 9 định nghĩa nhận diện.

- [LineFollowModule::begin — dòng 4](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L4)
- [LineFollowModule::readSensors — dòng 9](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L9)
- [LineFollowModule::followLine — dòng 18](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L18)
- [LineFollowModule::correctLeft — dòng 28](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L28)
- [LineFollowModule::correctRight — dòng 33](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L33)
- [LineFollowModule::gentleCorrectLeft — dòng 38](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L38)
- [LineFollowModule::gentleCorrectRight — dòng 43](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L43)
- [LineFollowModule::followPd — dòng 48](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L48)
- [LineFollowModule::stop — dòng 55](../../firmware/NCKHAGV/src/motion/LineFollowModule.cpp#L55)

## firmware/NCKHAGV/src/motion/LineFollowModule.h

[Mở source](../../firmware/NCKHAGV/src/motion/LineFollowModule.h) · 21 dòng · 2 định nghĩa nhận diện.

- [LineFollowModule — dòng 7](../../firmware/NCKHAGV/src/motion/LineFollowModule.h#L7)
- [resetPd — dòng 16](../../firmware/NCKHAGV/src/motion/LineFollowModule.h#L16)

## firmware/NCKHAGV/src/motion/MotorModule.cpp

[Mở source](../../firmware/NCKHAGV/src/motion/MotorModule.cpp) · 114 dòng · 13 định nghĩa nhận diện.

- [MotorModule::configured — dòng 3](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L3)
- [MotorModule::begin — dòng 6](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L6)
- [MotorModule::setLeftSpeed — dòng 30](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L30)
- [MotorModule::setRightSpeed — dòng 31](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L31)
- [MotorModule::setMotorSpeed — dòng 32](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L32)
- [MotorModule::motorForward — dòng 36](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L36)
- [MotorModule::motorBackward — dòng 37](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L37)
- [MotorModule::motorLeft — dòng 39](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L39)
- [MotorModule::motorRight — dòng 40](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L40)
- [MotorModule::motorStop — dòng 41](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L41)
- [MotorModule::drive — dòng 47](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L47)
- [MotorModule::applyStartupBoost — dòng 63](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L63)
- [MotorModule::writeSide — dòng 77](../../firmware/NCKHAGV/src/motion/MotorModule.cpp#L77)

## firmware/NCKHAGV/src/motion/MotorModule.h

[Mở source](../../firmware/NCKHAGV/src/motion/MotorModule.h) · 30 dòng · 4 định nghĩa nhận diện.

- [leftDuty — dòng 17](../../firmware/NCKHAGV/src/motion/MotorModule.h#L17)
- [rightDuty — dòng 18](../../firmware/NCKHAGV/src/motion/MotorModule.h#L18)
- [setBatteryCompOffset — dòng 20](../../firmware/NCKHAGV/src/motion/MotorModule.h#L20)
- [batteryCompOffset — dòng 21](../../firmware/NCKHAGV/src/motion/MotorModule.h#L21)

## firmware/NCKHAGV/src/motion/MovementModule.cpp

[Mở source](../../firmware/NCKHAGV/src/motion/MovementModule.cpp) · 108 dòng · 10 định nghĩa nhận diện.

- [MovementModule::moveForward — dòng 5](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L5)
- [MovementModule::moveBackward — dòng 6](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L6)
- [MovementModule::correctBackwardLeft — dòng 13](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L13)
- [MovementModule::correctBackwardRight — dòng 16](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L16)
- [MovementModule::applyReverseCorrection — dòng 19](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L19)
- [MovementModule::turnLeft — dòng 39](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L39)
- [MovementModule::turnRight — dòng 44](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L44)
- [MovementModule::stop — dòng 49](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L49)
- [MovementModule::followLine — dòng 50](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L50)
- [MovementModule::apply — dòng 51](../../firmware/NCKHAGV/src/motion/MovementModule.cpp#L51)

## firmware/NCKHAGV/src/motion/MovementModule.h

[Mở source](../../firmware/NCKHAGV/src/motion/MovementModule.h) · 28 dòng · 3 định nghĩa nhận diện.

- [MovementModule — dòng 7](../../firmware/NCKHAGV/src/motion/MovementModule.h#L7)
- [begin — dòng 9](../../firmware/NCKHAGV/src/motion/MovementModule.h#L9)
- [action — dòng 21](../../firmware/NCKHAGV/src/motion/MovementModule.h#L21)

## firmware/NCKHAGV/src/network/MQTTModule.cpp

[Mở source](../../firmware/NCKHAGV/src/network/MQTTModule.cpp) · 503 dòng · 28 định nghĩa nhận diện.

- [resetReasonName — dòng 18](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L18)
- [number — dòng 34](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L34)
- [base — dòng 38](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L38)
- [serialize — dòng 44](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L44)
- [topic — dòng 48](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L48)
- [MQTTModule::configured — dòng 54](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L54)
- [MQTTModule::buildTopics — dòng 68](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L68)
- [MQTTModule::begin — dòng 84](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L84)
- [MQTTModule::workerEntry — dòng 128](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L128)
- [MQTTModule::isConnected — dòng 134](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L134)
- [MQTTModule::subscribeTopics — dòng 141](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L141)
- [MQTTModule::connect — dòng 146](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L146)
- [MQTTModule::loop — dòng 186](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L186)
- [MQTTModule::popCommand — dòng 241](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L241)
- [MQTTModule::popGrant — dòng 244](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L244)
- [MQTTModule::requestZone — dòng 247](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L247)
- [MQTTModule::releaseZone — dòng 254](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L254)
- [MQTTModule::persistCommandId — dòng 263](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L263)
- [MQTTModule::sendAck — dòng 272](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L272)
- [MQTTModule::onMessage — dòng 283](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L283)
- [MQTTModule::enqueue — dòng 343](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L343)
- [MQTTModule::publishStatus — dòng 348](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L348)
- [MQTTModule::publishEvent — dòng 351](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L351)
- [MQTTModule::publishCargo — dòng 365](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L365)
- [MQTTModule::publishError — dòng 377](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L377)
- [MQTTModule::sendTelemetry — dòng 387](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L387)
- [MQTTModule::publishOdometry — dòng 493](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L493)
- [MQTTModule::sendOdometry — dòng 496](../../firmware/NCKHAGV/src/network/MQTTModule.cpp#L496)

## firmware/NCKHAGV/src/network/MQTTModule.h

[Mở source](../../firmware/NCKHAGV/src/network/MQTTModule.h) · 72 dòng · 8 định nghĩa nhận diện.

- [MQTTModule — dòng 13](../../firmware/NCKHAGV/src/network/MQTTModule.h#L13)
- [session — dòng 19](../../firmware/NCKHAGV/src/network/MQTTModule.h#L19)
- [consumeStop — dòng 24](../../firmware/NCKHAGV/src/network/MQTTModule.h#L24)
- [consumeQueueFault — dòng 25](../../firmware/NCKHAGV/src/network/MQTTModule.h#L25)
- [consumeStorageFault — dòng 26](../../firmware/NCKHAGV/src/network/MQTTModule.h#L26)
- [consumeCoreRestart — dòng 27](../../firmware/NCKHAGV/src/network/MQTTModule.h#L27)
- [publishPosition — dòng 30](../../firmware/NCKHAGV/src/network/MQTTModule.h#L30)
- [publishDistance — dòng 31](../../firmware/NCKHAGV/src/network/MQTTModule.h#L31)

## firmware/NCKHAGV/src/network/NetworkHealth.h

[Mở source](../../firmware/NCKHAGV/src/network/NetworkHealth.h) · 20 dòng · 1 định nghĩa nhận diện.

- [connected — dòng 13](../../firmware/NCKHAGV/src/network/NetworkHealth.h#L13)

## firmware/NCKHAGV/src/network/WiFiModule.cpp

[Mở source](../../firmware/NCKHAGV/src/network/WiFiModule.cpp) · 28 dòng · 5 định nghĩa nhận diện.

- [WiFiModule::begin — dòng 5](../../firmware/NCKHAGV/src/network/WiFiModule.cpp#L5)
- [WiFiModule::connect — dòng 17](../../firmware/NCKHAGV/src/network/WiFiModule.cpp#L17)
- [WiFiModule::reconnect — dòng 24](../../firmware/NCKHAGV/src/network/WiFiModule.cpp#L24)
- [WiFiModule::loop — dòng 25](../../firmware/NCKHAGV/src/network/WiFiModule.cpp#L25)
- [WiFiModule::isConnected — dòng 28](../../firmware/NCKHAGV/src/network/WiFiModule.cpp#L28)

## firmware/NCKHAGV/src/network/WiFiModule.h

[Mở source](../../firmware/NCKHAGV/src/network/WiFiModule.h) · 13 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/profiles/BatteryCalibration.h

[Mở source](../../firmware/NCKHAGV/src/profiles/BatteryCalibration.h) · 15 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/profiles/Robot01Config.h

[Mở source](../../firmware/NCKHAGV/src/profiles/Robot01Config.h) · 149 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/profiles/Robot02Config.h

[Mở source](../../firmware/NCKHAGV/src/profiles/Robot02Config.h) · 149 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/profiles/SharedRobotConfig.h

[Mở source](../../firmware/NCKHAGV/src/profiles/SharedRobotConfig.h) · 113 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## firmware/NCKHAGV/src/sensors/BatteryModule.cpp

[Mở source](../../firmware/NCKHAGV/src/sensors/BatteryModule.cpp) · 95 dòng · 3 định nghĩa nhận diện.

- [BatteryModule::begin — dòng 5](../../firmware/NCKHAGV/src/sensors/BatteryModule.cpp#L5)
- [BatteryModule::update — dòng 13](../../firmware/NCKHAGV/src/sensors/BatteryModule.cpp#L13)
- [BatteryModule::calibrated — dòng 93](../../firmware/NCKHAGV/src/sensors/BatteryModule.cpp#L93)

## firmware/NCKHAGV/src/sensors/BatteryModule.h

[Mở source](../../firmware/NCKHAGV/src/sensors/BatteryModule.h) · 28 dòng · 3 định nghĩa nhận diện.

- [readVoltage — dòng 9](../../firmware/NCKHAGV/src/sensors/BatteryModule.h#L9)
- [getBatteryPercentage — dòng 10](../../firmware/NCKHAGV/src/sensors/BatteryModule.h#L10)
- [adcMillivolts — dòng 11](../../firmware/NCKHAGV/src/sensors/BatteryModule.h#L11)

## firmware/NCKHAGV/src/sensors/CargoModule.cpp

[Mở source](../../firmware/NCKHAGV/src/sensors/CargoModule.cpp) · 21 dòng · 2 định nghĩa nhận diện.

- [CargoModule::begin — dòng 3](../../firmware/NCKHAGV/src/sensors/CargoModule.cpp#L3)
- [CargoModule::update — dòng 11](../../firmware/NCKHAGV/src/sensors/CargoModule.cpp#L11)

## firmware/NCKHAGV/src/sensors/CargoModule.h

[Mở source](../../firmware/NCKHAGV/src/sensors/CargoModule.h) · 22 dòng · 8 định nghĩa nhận diện.

- [stable — dòng 9](../../firmware/NCKHAGV/src/sensors/CargoModule.h#L9)
- [present — dòng 10](../../firmware/NCKHAGV/src/sensors/CargoModule.h#L10)
- [absent — dòng 11](../../firmware/NCKHAGV/src/sensors/CargoModule.h#L11)
- [rawLevel — dòng 12](../../firmware/NCKHAGV/src/sensors/CargoModule.h#L12)
- [sequence — dòng 13](../../firmware/NCKHAGV/src/sensors/CargoModule.h#L13)
- [sampledAt — dòng 14](../../firmware/NCKHAGV/src/sensors/CargoModule.h#L14)
- [stableMs — dòng 15](../../firmware/NCKHAGV/src/sensors/CargoModule.h#L15)
- [unstableMs — dòng 16](../../firmware/NCKHAGV/src/sensors/CargoModule.h#L16)

## firmware/NCKHAGV/src/sensors/DirectionalSonar.h

[Mở source](../../firmware/NCKHAGV/src/sensors/DirectionalSonar.h) · 40 dòng · 6 định nghĩa nhận diện.

- [DirectionalSonar — dòng 8](../../firmware/NCKHAGV/src/sensors/DirectionalSonar.h#L8)
- [update — dòng 10](../../firmware/NCKHAGV/src/sensors/DirectionalSonar.h#L10)
- [active — dòng 26](../../firmware/NCKHAGV/src/sensors/DirectionalSonar.h#L26)
- [rearActive — dòng 27](../../firmware/NCKHAGV/src/sensors/DirectionalSonar.h#L27)
- [acquiring — dòng 28](../../firmware/NCKHAGV/src/sensors/DirectionalSonar.h#L28)
- [selected — dòng 33](../../firmware/NCKHAGV/src/sensors/DirectionalSonar.h#L33)

## firmware/NCKHAGV/src/sensors/EncoderModule.cpp

[Mở source](../../firmware/NCKHAGV/src/sensors/EncoderModule.cpp) · 35 dòng · 6 định nghĩa nhận diện.

- [EncoderModule::begin — dòng 3](../../firmware/NCKHAGV/src/sensors/EncoderModule.cpp#L3)
- [EncoderModule::leftInterrupt — dòng 11](../../firmware/NCKHAGV/src/sensors/EncoderModule.cpp#L11)
- [EncoderModule::rightInterrupt — dòng 12](../../firmware/NCKHAGV/src/sensors/EncoderModule.cpp#L12)
- [EncoderModule::pulse — dòng 13](../../firmware/NCKHAGV/src/sensors/EncoderModule.cpp#L13)
- [EncoderModule::update — dòng 23](../../firmware/NCKHAGV/src/sensors/EncoderModule.cpp#L23)
- [EncoderModule::context — dòng 30](../../firmware/NCKHAGV/src/sensors/EncoderModule.cpp#L30)

## firmware/NCKHAGV/src/sensors/EncoderModule.h

[Mở source](../../firmware/NCKHAGV/src/sensors/EncoderModule.h) · 33 dòng · 4 định nghĩa nhận diện.

- [snapshot — dòng 11](../../firmware/NCKHAGV/src/sensors/EncoderModule.h#L11)
- [translationTicks — dòng 12](../../firmware/NCKHAGV/src/sensors/EncoderModule.h#L12)
- [leftTicks — dòng 13](../../firmware/NCKHAGV/src/sensors/EncoderModule.h#L13)
- [rightTicks — dòng 19](../../firmware/NCKHAGV/src/sensors/EncoderModule.h#L19)

## firmware/NCKHAGV/src/sensors/RFIDModule.cpp

[Mở source](../../firmware/NCKHAGV/src/sensors/RFIDModule.cpp) · 88 dòng · 6 định nghĩa nhận diện.

- [RFIDModule::validUid — dòng 6](../../firmware/NCKHAGV/src/sensors/RFIDModule.cpp#L6)
- [RFIDModule::begin — dòng 13](../../firmware/NCKHAGV/src/sensors/RFIDModule.cpp#L13)
- [RFIDModule::getNodeFromUID — dòng 26](../../firmware/NCKHAGV/src/sensors/RFIDModule.cpp#L26)
- [RFIDModule::hasMapping — dòng 32](../../firmware/NCKHAGV/src/sensors/RFIDModule.cpp#L32)
- [RFIDModule::readCard — dòng 38](../../firmware/NCKHAGV/src/sensors/RFIDModule.cpp#L38)
- [RFIDModule::finishRead — dòng 76](../../firmware/NCKHAGV/src/sensors/RFIDModule.cpp#L76)

## firmware/NCKHAGV/src/sensors/RFIDModule.h

[Mở source](../../firmware/NCKHAGV/src/sensors/RFIDModule.h) · 30 dòng · 5 định nghĩa nhận diện.

- [update — dòng 9](../../firmware/NCKHAGV/src/sensors/RFIDModule.h#L9)
- [getUID — dòng 13](../../firmware/NCKHAGV/src/sensors/RFIDModule.h#L13)
- [clearLastNode — dòng 15](../../firmware/NCKHAGV/src/sensors/RFIDModule.h#L15)
- [isNewNode — dòng 16](../../firmware/NCKHAGV/src/sensors/RFIDModule.h#L16)
- [event — dòng 18](../../firmware/NCKHAGV/src/sensors/RFIDModule.h#L18)

## firmware/NCKHAGV/src/sensors/SensorModule.cpp

[Mở source](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp) · 106 dòng · 8 định nghĩa nhận diện.

- [SensorModule::begin — dòng 4](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp#L4)
- [SensorModule::clearReading — dòng 11](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp#L11)
- [SensorModule::echoInterrupt — dòng 22](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp#L22)
- [SensorModule::update — dòng 38](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp#L38)
- [SensorModule::valid — dòng 86](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp#L86)
- [SensorModule::readDistanceCm — dòng 89](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp#L89)
- [SensorModule::isObstacleDetected — dòng 90](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp#L90)
- [SensorModule::diagnostic — dòng 91](../../firmware/NCKHAGV/src/sensors/SensorModule.cpp#L91)

## firmware/NCKHAGV/src/sensors/SensorModule.h

[Mở source](../../firmware/NCKHAGV/src/sensors/SensorModule.h) · 35 dòng · 1 định nghĩa nhận diện.

- [SensorModule — dòng 13](../../firmware/NCKHAGV/src/sensors/SensorModule.h#L13)

## include/commands/CommandIds.hpp

[Mở source](../../include/commands/CommandIds.hpp) · 68 dòng · 4 định nghĩa nhận diện.

- [CommandIds — dòng 24](../../include/commands/CommandIds.hpp#L24)
- [next — dòng 36](../../include/commands/CommandIds.hpp#L36)
- [ensureAbove — dòng 45](../../include/commands/CommandIds.hpp#L45)
- [reserve — dòng 54](../../include/commands/CommandIds.hpp#L54)

## include/commands/WebCommands.hpp

[Mở source](../../include/commands/WebCommands.hpp) · 207 dòng · 6 định nghĩa nhận diện.

- [WebCommands — dòng 10](../../include/commands/WebCommands.hpp#L10)
- [setShiftController — dòng 13](../../include/commands/WebCommands.hpp#L13)
- [setSimulationMode — dòng 14](../../include/commands/WebCommands.hpp#L14)
- [execute — dòng 16](../../include/commands/WebCommands.hpp#L16)
- [id — dòng 192](../../include/commands/WebCommands.hpp#L192)
- [failure — dòng 198](../../include/commands/WebCommands.hpp#L198)

## include/core/EncoderOdometry.hpp

[Mở source](../../include/core/EncoderOdometry.hpp) · 122 dòng · 3 định nghĩa nhận diện.

- [parse — dòng 23](../../include/core/EncoderOdometry.hpp#L23)
- [bind — dòng 96](../../include/core/EncoderOdometry.hpp#L96)
- [accept — dòng 110](../../include/core/EncoderOdometry.hpp#L110)

## include/core/FLeetmanager.hpp

[Mở source](../../include/core/FLeetmanager.hpp) · 2662 dòng · 118 định nghĩa nhận diện.

- [toString — dòng 26](../../include/core/FLeetmanager.hpp#L26)
- [FleetManager — dòng 161](../../include/core/FLeetmanager.hpp#L161)
- [setPublishCallback — dòng 164](../../include/core/FLeetmanager.hpp#L164)
- [setStateChangedCallback — dòng 166](../../include/core/FLeetmanager.hpp#L166)
- [setEventCallback — dòng 168](../../include/core/FLeetmanager.hpp#L168)
- [setCommandIdAllocator — dòng 169](../../include/core/FLeetmanager.hpp#L169)
- [setCommandIdHighwaterCallback — dòng 170](../../include/core/FLeetmanager.hpp#L170)
- [setTaskIdAllocator — dòng 173](../../include/core/FLeetmanager.hpp#L173)
- [createTaskId — dòng 174](../../include/core/FLeetmanager.hpp#L174)
- [durableSnapshot — dòng 175](../../include/core/FLeetmanager.hpp#L175)
- [restore — dòng 194](../../include/core/FLeetmanager.hpp#L194)
- [reportOffline — dòng 246](../../include/core/FLeetmanager.hpp#L246)
- [setNetworkAvailable — dòng 263](../../include/core/FLeetmanager.hpp#L263)
- [reportNetworkLoss — dòng 265](../../include/core/FLeetmanager.hpp#L265)
- [setAcceptingTasks — dòng 282](../../include/core/FLeetmanager.hpp#L282)
- [setSingleRobotTrial — dòng 283](../../include/core/FLeetmanager.hpp#L283)
- [setTwoRobotTrial — dòng 284](../../include/core/FLeetmanager.hpp#L284)
- [setTrialRobot — dòng 285](../../include/core/FLeetmanager.hpp#L285)
- [setActiveRobots — dòng 291](../../include/core/FLeetmanager.hpp#L291)
- [isActiveRobot — dòng 296](../../include/core/FLeetmanager.hpp#L296)
- [setMonitorOnlyRobot — dòng 299](../../include/core/FLeetmanager.hpp#L299)
- [acceptingTasks — dòng 303](../../include/core/FLeetmanager.hpp#L303)
- [networkAvailable — dòng 304](../../include/core/FLeetmanager.hpp#L304)
- [setBatteryTestBypass — dòng 305](../../include/core/FLeetmanager.hpp#L305)
- [batteryGateEnabled — dòng 306](../../include/core/FLeetmanager.hpp#L306)
- [setAutomaticChargingEnabled — dòng 307](../../include/core/FLeetmanager.hpp#L307)
- [setHomeStations — dòng 308](../../include/core/FLeetmanager.hpp#L308)
- [returnToHome — dòng 313](../../include/core/FLeetmanager.hpp#L313)
- [beginWorkFromHome — dòng 361](../../include/core/FLeetmanager.hpp#L361)
- [setBatteryThresholds — dòng 376](../../include/core/FLeetmanager.hpp#L376)
- [setChargeThresholds — dòng 386](../../include/core/FLeetmanager.hpp#L386)
- [setAckTimeout — dòng 395](../../include/core/FLeetmanager.hpp#L395)
- [setMaxRetries — dòng 400](../../include/core/FLeetmanager.hpp#L400)
- [setOfflineTimeout — dòng 405](../../include/core/FLeetmanager.hpp#L405)
- [setAutoRegister — dòng 410](../../include/core/FLeetmanager.hpp#L410)
- [setReleaseZonesOnOffline — dòng 416](../../include/core/FLeetmanager.hpp#L416)
- [addAGV — dòng 422](../../include/core/FLeetmanager.hpp#L422)
- [removeAGV — dòng 433](../../include/core/FLeetmanager.hpp#L433)
- [updateTelemetry — dòng 451](../../include/core/FLeetmanager.hpp#L451)
- [updateBatteryDiagnostic — dòng 564](../../include/core/FLeetmanager.hpp#L564)
- [encoderEnabled — dòng 599](../../include/core/FLeetmanager.hpp#L599)
- [bindEncoderSession — dòng 603](../../include/core/FLeetmanager.hpp#L603)
- [observeEncoderFirmwareFault — dòng 629](../../include/core/FLeetmanager.hpp#L629)
- [observeOdometry — dòng 640](../../include/core/FLeetmanager.hpp#L640)
- [observeStoppedPosition — dòng 687](../../include/core/FLeetmanager.hpp#L687)
- [refreshStoppedOccupancy — dòng 756](../../include/core/FLeetmanager.hpp#L756)
- [observeMotionDiagnostic — dòng 785](../../include/core/FLeetmanager.hpp#L785)
- [reportCharged — dòng 952](../../include/core/FLeetmanager.hpp#L952)
- [reportError — dòng 971](../../include/core/FLeetmanager.hpp#L971)
- [observeHeadingDiagnostic — dòng 1006](../../include/core/FLeetmanager.hpp#L1006)
- [observeNodeOrderRequest — dòng 1065](../../include/core/FLeetmanager.hpp#L1065)
- [observeRobot01Reset — dòng 1110](../../include/core/FLeetmanager.hpp#L1110)
- [clearError — dòng 1143](../../include/core/FLeetmanager.hpp#L1143)
- [reportAck — dòng 1186](../../include/core/FLeetmanager.hpp#L1186)
- [setObstacleTtl — dòng 1193](../../include/core/FLeetmanager.hpp#L1193)
- [reportObstacle — dòng 1194](../../include/core/FLeetmanager.hpp#L1194)
- [clearObstacle — dòng 1206](../../include/core/FLeetmanager.hpp#L1206)
- [simulationReplan — dòng 1212](../../include/core/FLeetmanager.hpp#L1212)
- [addTask — dòng 1224](../../include/core/FLeetmanager.hpp#L1224)
- [startLift — dòng 1243](../../include/core/FLeetmanager.hpp#L1243)
- [reconcileTestCargo — dòng 1273](../../include/core/FLeetmanager.hpp#L1273)
- [cargoSensorReadyNoLock — dòng 1304](../../include/core/FLeetmanager.hpp#L1304)
- [observeCargoSensor — dòng 1312](../../include/core/FLeetmanager.hpp#L1312)
- [observeLift — dòng 1332](../../include/core/FLeetmanager.hpp#L1332)
- [directGoto — dòng 1397](../../include/core/FLeetmanager.hpp#L1397)
- [recoverVerifiedNode — dòng 1487](../../include/core/FLeetmanager.hpp#L1487)
- [confirmRecoveryHeading — dòng 1564](../../include/core/FLeetmanager.hpp#L1564)
- [mayRequestZone — dòng 1593](../../include/core/FLeetmanager.hpp#L1593)
- [endShiftWork — dòng 1615](../../include/core/FLeetmanager.hpp#L1615)
- [resumeTask — dòng 1647](../../include/core/FLeetmanager.hpp#L1647)
- [cancelTask — dòng 1688](../../include/core/FLeetmanager.hpp#L1688)
- [tick — dòng 1734](../../include/core/FLeetmanager.hpp#L1734)
- [toJson — dòng 1747](../../include/core/FLeetmanager.hpp#L1747)
- [localizedStopNoLock — dòng 1878](../../include/core/FLeetmanager.hpp#L1878)
- [verifiedNodeStopNoLock — dòng 1887](../../include/core/FLeetmanager.hpp#L1887)
- [recoveryExpectedHeadingNoLock — dòng 1895](../../include/core/FLeetmanager.hpp#L1895)
- [replanAllForStoppedOccupancy — dòng 1901](../../include/core/FLeetmanager.hpp#L1901)
- [activeRobotNoLock — dòng 1907](../../include/core/FLeetmanager.hpp#L1907)
- [encoderReadyNoLock — dòng 1910](../../include/core/FLeetmanager.hpp#L1910)
- [completeArrivalNoLock — dòng 1914](../../include/core/FLeetmanager.hpp#L1914)
- [pruneTaskRecords — dòng 1931](../../include/core/FLeetmanager.hpp#L1931)
- [nextTaskIdNoLock — dòng 1961](../../include/core/FLeetmanager.hpp#L1961)
- [setTaskStatusNoLock — dòng 1963](../../include/core/FLeetmanager.hpp#L1963)
- [addTaskInternal — dòng 1971](../../include/core/FLeetmanager.hpp#L1971)
- [invalidatePendingHeadingNoLock — dòng 1979](../../include/core/FLeetmanager.hpp#L1979)
- [interruptWorkNoLock — dòng 1987](../../include/core/FLeetmanager.hpp#L1987)
- [preservePathNoLock — dòng 2005](../../include/core/FLeetmanager.hpp#L2005)
- [checkNodeProofWarnings — dòng 2009](../../include/core/FLeetmanager.hpp#L2009)
- [checkLiftTimeouts — dòng 2023](../../include/core/FLeetmanager.hpp#L2023)
- [taskQueueSnapshotNoLock — dòng 2039](../../include/core/FLeetmanager.hpp#L2039)
- [hasWork — dòng 2049](../../include/core/FLeetmanager.hpp#L2049)
- [requeueWorkNoLock — dòng 2057](../../include/core/FLeetmanager.hpp#L2057)
- [releaseStationReservationNoLock — dòng 2072](../../include/core/FLeetmanager.hpp#L2072)
- [isChargingStationNode — dòng 2078](../../include/core/FLeetmanager.hpp#L2078)
- [pickChargingStationNoLock — dòng 2086](../../include/core/FLeetmanager.hpp#L2086)
- [buildBusyNodesNoLock — dòng 2102](../../include/core/FLeetmanager.hpp#L2102)
- [computePathNoLock — dòng 2116](../../include/core/FLeetmanager.hpp#L2116)
- [buildOccupiedNodesNoLock — dòng 2124](../../include/core/FLeetmanager.hpp#L2124)
- [buildHardBlockedNodesNoLock — dòng 2143](../../include/core/FLeetmanager.hpp#L2143)
- [agvsRoutedThroughNoLock — dòng 2175](../../include/core/FLeetmanager.hpp#L2175)
- [agvsAffectedByFailureNoLock — dòng 2189](../../include/core/FLeetmanager.hpp#L2189)
- [appendMutexWaitersForFailedOwners — dòng 2226](../../include/core/FLeetmanager.hpp#L2226)
- [replanAffectedAgvs — dòng 2240](../../include/core/FLeetmanager.hpp#L2240)
- [dispatchableNoLock — dòng 2253](../../include/core/FLeetmanager.hpp#L2253)
- [dispatchPending — dòng 2266](../../include/core/FLeetmanager.hpp#L2266)
- [assignPath — dòng 2318](../../include/core/FLeetmanager.hpp#L2318)
- [sendCommand — dòng 2352](../../include/core/FLeetmanager.hpp#L2352)
- [checkAcks — dòng 2388](../../include/core/FLeetmanager.hpp#L2388)
- [replanAGV — dòng 2434](../../include/core/FLeetmanager.hpp#L2434)
- [checkTimeouts — dòng 2473](../../include/core/FLeetmanager.hpp#L2473)
- [resumeBlocked — dòng 2511](../../include/core/FLeetmanager.hpp#L2511)
- [checkBatteryOne — dòng 2545](../../include/core/FLeetmanager.hpp#L2545)
- [checkBatteryAll — dòng 2599](../../include/core/FLeetmanager.hpp#L2599)
- [releaseAgvZones — dòng 2608](../../include/core/FLeetmanager.hpp#L2608)
- [notify — dòng 2618](../../include/core/FLeetmanager.hpp#L2618)
- [emit — dòng 2619](../../include/core/FLeetmanager.hpp#L2619)
- [supervisedTrialNoLock — dòng 2641](../../include/core/FLeetmanager.hpp#L2641)
- [diagnosticRecoveryNoLock — dòng 2643](../../include/core/FLeetmanager.hpp#L2643)

## include/core/NodeManeuver.hpp

[Mở source](../../include/core/NodeManeuver.hpp) · 78 dòng · 5 định nghĩa nhận diện.

- [heading — dòng 10](../../include/core/NodeManeuver.hpp#L10)
- [direction — dòng 13](../../include/core/NodeManeuver.hpp#L13)
- [branch — dòng 16](../../include/core/NodeManeuver.hpp#L16)
- [straightThrough — dòng 27](../../include/core/NodeManeuver.hpp#L27)
- [plan — dòng 47](../../include/core/NodeManeuver.hpp#L47)

## include/core/PhysicalLayout.hpp

[Mở source](../../include/core/PhysicalLayout.hpp) · 40 dòng · 6 định nghĩa nhận diện.

- [home — dòng 11](../../include/core/PhysicalLayout.hpp#L11)
- [isHome — dòng 13](../../include/core/PhysicalLayout.hpp#L13)
- [outwardHeading — dòng 14](../../include/core/PhysicalLayout.hpp#L14)
- [gate — dòng 15](../../include/core/PhysicalLayout.hpp#L15)
- [warehouse — dòng 21](../../include/core/PhysicalLayout.hpp#L21)
- [validate — dòng 22](../../include/core/PhysicalLayout.hpp#L22)

## include/core/RuntimeStore.hpp

[Mở source](../../include/core/RuntimeStore.hpp) · 150 dòng · 6 định nghĩa nhận diện.

- [RuntimeStore — dòng 12](../../include/core/RuntimeStore.hpp#L12)
- [mapsCompatible — dòng 13](../../include/core/RuntimeStore.hpp#L13)
- [migrateRoundedMap — dòng 31](../../include/core/RuntimeStore.hpp#L31)
- [backupBeforeRoundedMigration — dòng 120](../../include/core/RuntimeStore.hpp#L120)
- [load — dòng 129](../../include/core/RuntimeStore.hpp#L129)
- [save — dòng 136](../../include/core/RuntimeStore.hpp#L136)

## include/core/ShiftController.hpp

[Mở source](../../include/core/ShiftController.hpp) · 275 dòng · 8 định nghĩa nhận diện.

- [ShiftController — dòng 10](../../include/core/ShiftController.hpp#L10)
- [supported — dòng 55](../../include/core/ShiftController.hpp#L55)
- [end — dòng 56](../../include/core/ShiftController.hpp#L56)
- [start — dòng 64](../../include/core/ShiftController.hpp#L64)
- [tick — dòng 82](../../include/core/ShiftController.hpp#L82)
- [snapshot — dòng 126](../../include/core/ShiftController.hpp#L126)
- [snapshot — dòng 129](../../include/core/ShiftController.hpp#L129)
- [transition — dòng 254](../../include/core/ShiftController.hpp#L254)

## include/map/EdgeGeometry.hpp

[Mở source](../../include/map/EdgeGeometry.hpp) · 52 dòng · 4 định nghĩa nhận diện.

- [node — dòng 10](../../include/map/EdgeGeometry.hpp#L10)
- [direction — dòng 14](../../include/map/EdgeGeometry.hpp#L14)
- [heading — dòng 21](../../include/map/EdgeGeometry.hpp#L21)
- [point — dòng 34](../../include/map/EdgeGeometry.hpp#L34)

## include/map/MapGraph.hpp

[Mở source](../../include/map/MapGraph.hpp) · 621 dòng · 33 định nghĩa nhận diện.

- [isMutex — dòng 56](../../include/map/MapGraph.hpp#L56)
- [isCharging — dòng 57](../../include/map/MapGraph.hpp#L57)
- [loadFromFile — dòng 73](../../include/map/MapGraph.hpp#L73)
- [loadFromJson — dòng 85](../../include/map/MapGraph.hpp#L85)
- [saveToFile — dòng 200](../../include/map/MapGraph.hpp#L200)
- [addNode — dòng 220](../../include/map/MapGraph.hpp#L220)
- [removeNode — dòng 231](../../include/map/MapGraph.hpp#L231)
- [addEdge — dòng 254](../../include/map/MapGraph.hpp#L254)
- [removeEdge — dòng 283](../../include/map/MapGraph.hpp#L283)
- [setEdgeObstacle — dòng 300](../../include/map/MapGraph.hpp#L300)
- [setObstacle — dòng 311](../../include/map/MapGraph.hpp#L311)
- [expireObstacles — dòng 322](../../include/map/MapGraph.hpp#L322)
- [epochSeconds — dòng 330](../../include/map/MapGraph.hpp#L330)
- [hasObstacle — dòng 333](../../include/map/MapGraph.hpp#L333)
- [mutexComponent — dòng 339](../../include/map/MapGraph.hpp#L339)
- [findShortestPath — dòng 358](../../include/map/MapGraph.hpp#L358)
- [nearestCharging — dòng 398](../../include/map/MapGraph.hpp#L398)
- [pathCost — dòng 408](../../include/map/MapGraph.hpp#L408)
- [undirectedDistance — dòng 425](../../include/map/MapGraph.hpp#L425)
- [straightLineDistance — dòng 451](../../include/map/MapGraph.hpp#L451)
- [mutexZones — dòng 458](../../include/map/MapGraph.hpp#L458)
- [chargingStations — dòng 459](../../include/map/MapGraph.hpp#L459)
- [nodeExists — dòng 460](../../include/map/MapGraph.hpp#L460)
- [edgeGeometry — dòng 463](../../include/map/MapGraph.hpp#L463)
- [toJson — dòng 478](../../include/map/MapGraph.hpp#L478)
- [loadDefaultDemoMap — dòng 509](../../include/map/MapGraph.hpp#L509)
- [validateNode — dòng 580](../../include/map/MapGraph.hpp#L580)
- [addUnique — dòng 586](../../include/map/MapGraph.hpp#L586)
- [addNodeNoLock — dòng 589](../../include/map/MapGraph.hpp#L589)
- [addTwoWay — dòng 592](../../include/map/MapGraph.hpp#L592)
- [addEdgeInternal — dòng 596](../../include/map/MapGraph.hpp#L596)
- [clearInternal — dòng 600](../../include/map/MapGraph.hpp#L600)
- [edgeKey — dòng 615](../../include/map/MapGraph.hpp#L615)

## include/utils/AtomicFile.hpp

[Mở source](../../include/utils/AtomicFile.hpp) · 67 dòng · 1 định nghĩa nhận diện.

- [durableReplaceFile — dòng 39](../../include/utils/AtomicFile.hpp#L39)

## include/utils/CoreTypes.hpp

[Mở source](../../include/utils/CoreTypes.hpp) · 8 dòng · 0 định nghĩa nhận diện.

File cấu hình/kiểu/khai báo, hoặc không có định nghĩa phù hợp bộ nhận diện; mở source để tra nội dung.

## include/utils/Identifiers.hpp

[Mở source](../../include/utils/Identifiers.hpp) · 8 dòng · 1 định nghĩa nhận diện.

- [validIdentifier — dòng 4](../../include/utils/Identifiers.hpp#L4)

## include/utils/Mutexarbiter.hpp

[Mở source](../../include/utils/Mutexarbiter.hpp) · 227 dòng · 16 định nghĩa nhận diện.

- [setGrantIdAllocator — dòng 35](../../include/utils/Mutexarbiter.hpp#L35)
- [setRetryTimeout — dòng 36](../../include/utils/Mutexarbiter.hpp#L36)
- [MutexArbiter — dòng 37](../../include/utils/Mutexarbiter.hpp#L37)
- [setBlockedZones — dòng 41](../../include/utils/Mutexarbiter.hpp#L41)
- [registerZone — dòng 46](../../include/utils/Mutexarbiter.hpp#L46)
- [requestZone — dòng 54](../../include/utils/Mutexarbiter.hpp#L54)
- [releaseZone — dòng 72](../../include/utils/Mutexarbiter.hpp#L72)
- [grant — dòng 91](../../include/utils/Mutexarbiter.hpp#L91)
- [acknowledge — dòng 97](../../include/utils/Mutexarbiter.hpp#L97)
- [restore — dòng 121](../../include/utils/Mutexarbiter.hpp#L121)
- [currentOwner — dòng 137](../../include/utils/Mutexarbiter.hpp#L137)
- [confirmClear — dòng 142](../../include/utils/Mutexarbiter.hpp#L142)
- [cancelWaiting — dòng 153](../../include/utils/Mutexarbiter.hpp#L153)
- [resetZones — dòng 185](../../include/utils/Mutexarbiter.hpp#L185)
- [toJson — dòng 192](../../include/utils/Mutexarbiter.hpp#L192)
- [assignNoLock — dòng 215](../../include/utils/Mutexarbiter.hpp#L215)

## simulation/launcher.js

[Mở source](../../simulation/launcher.js) · 63 dòng · 3 định nghĩa nhận diện.

- [createLauncher — dòng 5](../../simulation/launcher.js#L5)
- [start — dòng 51](../../simulation/launcher.js#L51)
- [close — dòng 52](../../simulation/launcher.js#L52)

## simulation/mqtt.js

[Mở source](../../simulation/mqtt.js) · 57 dòng · 7 định nghĩa nhận diện.

- [field — dòng 4](../../simulation/mqtt.js#L4)
- [packet — dòng 8](../../simulation/mqtt.js#L8)
- [constructor — dòng 14](../../simulation/mqtt.js#L14)
- [read — dòng 24](../../simulation/mqtt.js#L24)
- [subscribe — dòng 48](../../simulation/mqtt.js#L48)
- [publish — dòng 52](../../simulation/mqtt.js#L52)
- [close — dòng 55](../../simulation/mqtt.js#L55)

## simulation/order-planner.js

[Mở source](../../simulation/order-planner.js) · 59 dòng · 3 định nghĩa nhận diện.

- [shortestCost — dòng 3](../../simulation/order-planner.js#L3)
- [normalizeOrder — dòng 31](../../simulation/order-planner.js#L31)
- [chooseRobot — dòng 52](../../simulation/order-planner.js#L52)

## simulation/public-proxy.js

[Mở source](../../simulation/public-proxy.js) · 61 dòng · 5 định nghĩa nhận diện.

- [sameOrigin — dòng 5](../../simulation/public-proxy.js#L5)
- [createPublicProxy — dòng 13](../../simulation/public-proxy.js#L13)
- [upstream — dòng 15](../../simulation/public-proxy.js#L15)
- [request — dòng 23](../../simulation/public-proxy.js#L23)
- [upgrade — dòng 36](../../simulation/public-proxy.js#L36)

## simulation/public/motion.js

[Mở source](../../simulation/public/motion.js) · 46 dòng · 5 định nghĩa nhận diện.

- [bearing — dòng 8](../../simulation/public/motion.js#L8)
- [gate — dòng 13](../../simulation/public/motion.js#L13)
- [parkedHeading — dòng 18](../../simulation/public/motion.js#L18)
- [edgeHeading — dòng 22](../../simulation/public/motion.js#L22)
- [describe — dòng 27](../../simulation/public/motion.js#L27)

## simulation/public/simulation.js

[Mở source](../../simulation/public/simulation.js) · 192 dòng · 15 định nghĩa nhận diện.

- [displayedHeading — dòng 5](../../simulation/public/simulation.js#L5)
- [call — dòng 33](../../simulation/public/simulation.js#L33)
- [disconnected — dòng 46](../../simulation/public/simulation.js#L46)
- [run — dòng 47](../../simulation/public/simulation.js#L47)
- [selectedEdge — dòng 91](../../simulation/public/simulation.js#L91)
- [changeObstacle — dòng 92](../../simulation/public/simulation.js#L92)
- [svg — dòng 95](../../simulation/public/simulation.js#L95)
- [render — dòng 96](../../simulation/public/simulation.js#L96)
- [animate — dòng 128](../../simulation/public/simulation.js#L128)
- [validSnapshot — dòng 161](../../simulation/public/simulation.js#L161)
- [poll — dòng 170](../../simulation/public/simulation.js#L170)
- [heartbeat — dòng 182](../../simulation/public/simulation.js#L182)
- [suspend — dòng 186](../../simulation/public/simulation.js#L186)
- [addEventListener — dòng 188](../../simulation/public/simulation.js#L188)
- [addEventListener — dòng 190](../../simulation/public/simulation.js#L190)

## simulation/robot.js

[Mở source](../../simulation/robot.js) · 154 dòng · 13 định nghĩa nhận diện.

- [constructor — dòng 4](../../simulation/robot.js#L4)
- [log — dòng 13](../../simulation/robot.js#L13)
- [send — dòng 14](../../simulation/robot.js#L14)
- [telemetry — dòng 15](../../simulation/robot.js#L15)
- [connect — dòng 16](../../simulation/robot.js#L16)
- [command — dòng 48](../../simulation/robot.js#L48)
- [release — dòng 80](../../simulation/robot.js#L80)
- [stopWithError — dòng 84](../../simulation/robot.js#L84)
- [step — dòng 88](../../simulation/robot.js#L88)
- [batteryChange — dòng 133](../../simulation/robot.js#L133)
- [confirmRfid — dòng 139](../../simulation/robot.js#L139)
- [snapshot — dòng 146](../../simulation/robot.js#L146)
- [close — dòng 152](../../simulation/robot.js#L152)

## simulation/runtime.js

[Mở source](../../simulation/runtime.js) · 377 dòng · 28 định nghĩa nhận diện.

- [constructor — dòng 7](../../simulation/runtime.js#L7)
- [event — dòng 18](../../simulation/runtime.js#L18)
- [edge — dòng 23](../../simulation/runtime.js#L23)
- [robot — dòng 29](../../simulation/runtime.js#L29)
- [edgeBlocked — dòng 30](../../simulation/runtime.js#L30)
- [setEdgeObstacle — dòng 33](../../simulation/runtime.js#L33)
- [applyPendingReplans — dòng 48](../../simulation/runtime.js#L48)
- [api — dòng 62](../../simulation/runtime.js#L62)
- [start — dòng 66](../../simulation/runtime.js#L66)
- [activeJob — dòng 105](../../simulation/runtime.js#L105)
- [cargoJob — dòng 106](../../simulation/runtime.js#L106)
- [dispatchStage — dòng 119](../../simulation/runtime.js#L119)
- [advanceJobs — dòng 129](../../simulation/runtime.js#L129)
- [finish — dòng 152](../../simulation/runtime.js#L152)
- [createOrder — dòng 159](../../simulation/runtime.js#L159)
- [reconcileOrders — dòng 173](../../simulation/runtime.js#L173)
- [eligibleOrderRobots — dòng 197](../../simulation/runtime.js#L197)
- [advanceOrders — dòng 203](../../simulation/runtime.js#L203)
- [autoRecoverIdleRobots — dòng 252](../../simulation/runtime.js#L252)
- [action — dòng 269](../../simulation/runtime.js#L269)
- [applyAction — dòng 275](../../simulation/runtime.js#L275)
- [recoverRobot — dòng 297](../../simulation/runtime.js#L297)
- [actionMessage — dòng 338](../../simulation/runtime.js#L338)
- [goto — dòng 350](../../simulation/runtime.js#L350)
- [configure — dòng 356](../../simulation/runtime.js#L356)
- [snapshot — dòng 362](../../simulation/runtime.js#L362)
- [report — dòng 368](../../simulation/runtime.js#L368)
- [close — dòng 369](../../simulation/runtime.js#L369)

## simulation/server.js

[Mở source](../../simulation/server.js) · 145 dòng · 6 định nghĩa nhận diện.

- [freePort — dòng 12](../../simulation/server.js#L12)
- [listening — dòng 16](../../simulation/server.js#L16)
- [createSimulation — dòng 17](../../simulation/server.js#L17)
- [close — dòng 48](../../simulation/server.js#L48)
- [child — dòng 58](../../simulation/server.js#L58)
- [restartProcess — dòng 129](../../simulation/server.js#L129)

## src/core/AGVserver.cpp

[Mở source](../../src/core/AGVserver.cpp) · 1043 dòng · 32 định nghĩa nhận diện.

- [closesocket — dòng 48](../../src/core/AGVserver.cpp#L48)
- [onShutdownSignal — dòng 73](../../src/core/AGVserver.cpp#L73)
- [requestWake — dòng 88](../../src/core/AGVserver.cpp#L88)
- [envPort — dòng 104](../../src/core/AGVserver.cpp#L104)
- [stateSnapshot — dòng 122](../../src/core/AGVserver.cpp#L122)
- [nowStr — dòng 147](../../src/core/AGVserver.cpp#L147)
- [pushLog — dòng 160](../../src/core/AGVserver.cpp#L160)
- [recentLogsJson — dòng 166](../../src/core/AGVserver.cpp#L166)
- [publishTo — dòng 175](../../src/core/AGVserver.cpp#L175)
- [checkpointAndPublish — dòng 178](../../src/core/AGVserver.cpp#L178)
- [base64Encode — dòng 216](../../src/core/AGVserver.cpp#L216)
- [sha1Base64 — dòng 225](../../src/core/AGVserver.cpp#L225)
- [recvAll — dòng 231](../../src/core/AGVserver.cpp#L231)
- [sendAll — dòng 241](../../src/core/AGVserver.cpp#L241)
- [wsHandshake — dòng 250](../../src/core/AGVserver.cpp#L250)
- [wsSendText — dòng 285](../../src/core/AGVserver.cpp#L285)
- [wsRecvText — dòng 303](../../src/core/AGVserver.cpp#L303)
- [handleWebCommand — dòng 337](../../src/core/AGVserver.cpp#L337)
- [wsClientThread — dòng 365](../../src/core/AGVserver.cpp#L365)
- [wsAcceptLoop — dòng 388](../../src/core/AGVserver.cpp#L388)
- [broadcastState — dòng 404](../../src/core/AGVserver.cpp#L404)
- [createWebSocketListener — dòng 421](../../src/core/AGVserver.cpp#L421)
- [parseTopic — dòng 470](../../src/core/AGVserver.cpp#L470)
- [on_connect — dòng 478](../../src/core/AGVserver.cpp#L478)
- [on_disconnect — dòng 505](../../src/core/AGVserver.cpp#L505)
- [grantId — dòng 510](../../src/core/AGVserver.cpp#L510)
- [on_message — dòng 517](../../src/core/AGVserver.cpp#L517)
- [startMqtt — dòng 749](../../src/core/AGVserver.cpp#L749)
- [enabled — dòng 775](../../src/core/AGVserver.cpp#L775)
- [startupMode — dòng 780](../../src/core/AGVserver.cpp#L780)
- [runCore — dòng 825](../../src/core/AGVserver.cpp#L825)
- [main — dòng 1036](../../src/core/AGVserver.cpp#L1036)
