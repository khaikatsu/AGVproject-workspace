const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {once}=require('node:events');
const WebSocket=require('../AGVwebserver/node_modules/ws');
const {createDatabase}=require('../AGVwebserver/db');
const {createGateway}=require('../AGVwebserver/gateway');

async function main() {
  const playwright=require(process.env.AGV_PLAYWRIGHT_MODULE||'playwright');
  fs.mkdirSync(path.resolve(__dirname,'../.qa'),{recursive:true});
  const directory=fs.mkdtempSync(path.resolve(__dirname,'../.qa/reports-browser-'));
  const db=createDatabase(path.join(directory,'test.db'));
  const core=new WebSocket.Server({host:'127.0.0.1',port:0});await once(core,'listening');
  const model={protocol_version:1,core_session_id:'reports-session',mqtt_connected:true,
    map:{nodes:[],edges:[],obstacles:[]},fleet:{agvs:[],tasks:[],pending_tasks:[]},mutex:{},logs:[],shift:{phase:'CLOSED'}};
  let peer,gateway,browser,pump;
  const push=()=>{if(peer?.readyState===WebSocket.OPEN)peer.send(JSON.stringify(model));};
  core.on('connection',socket=>{peer=socket;push();socket.on('message',()=>assert.fail('Report browsing must not command Core'));});
  try {
    gateway=createGateway({db,coreUrl:`ws://127.0.0.1:${core.address().port}`});
    for(const id of [1,2]) {
      const report={period_start:'2026-10-25 08:00:00',period_end:'2026-10-25 09:00:00',total_tasks:id===1?8:10,
        completed_tasks:id===1?4:8,pending_tasks:1,dispatched_tasks:2,cancelled_tasks:1,interrupted_tasks:1,rejected_tasks:0,
        completed_in_period:9,avg_completion_seconds:120,avg_queue_seconds:25,avg_travel_seconds:95,agv_error_events:2,obstacle_events:3,
        agv_utilization:Object.fromEntries(['1','01','02','AGV_01','AGV_02','<img src=x onerror="window.__reportXss=1">'].map(id=>
          [id,{known_seconds:3700,breakdown:[{state:'WORKING',percent:60},{state:'IDLE',percent:40}]}]))};
      db.db.prepare('INSERT INTO kpi_reports(id,reason,report_json,created_at) VALUES(?,?,?,?)')
        .run(id,'fixture',JSON.stringify(report),`2026-10-25 0${id+8}:00:00`);
    }
    const inventory=where=>({initialized:true,slots:[{node:'K1',rack_id:where==='K1'?'R01':null},{node:'K2',rack_id:where==='K2'?'R01':null}],
      racks:[{id:'R01',status:'stored',location:{type:'warehouse',id:where}}],moves:[]});
    const saved=[];
    for(const [index,id] of ['warehouse-old','warehouse-new'].entries()) {
      const report={id,reason:'shift_closed',started_at:'2026-10-25T01:00:00Z',created_at:`2026-10-25T0${index+3}:00:00Z`,
        observation_gap:!!index,summary:{pickups:index+1,dropoffs:1,on_robots:0,needs_attention:index},
        initial_state:inventory('K1'),final_state:inventory('K2'),unfinished_orders:[],unfinished_actions:[],
        events:[{created_at:'2026-10-25T02:00:00Z',message:'Kệ R01: K1 → K2'}]};
      saved.push(report);db.db.prepare('INSERT INTO warehouse_shift_reports(id,report_json,created_at) VALUES(?,?,?)')
        .run(id,JSON.stringify(report),report.created_at);
    }
    db.insertSystemLog(1,'09:00:00','Lỗi <img src=x onerror="window.__reportXss=1">','reports-session');
    db.db.prepare('INSERT INTO warehouse_events(event_key,event_json) VALUES(?,?)')
      .run('report-fixture',JSON.stringify({created_at:'2026-10-25T02:00:00Z',message:'Xe 01 đã chuyển kệ R01'}));
    const base=`http://127.0.0.1:${(await gateway.start(0,'127.0.0.1')).port}`;pump=setInterval(push,250);
    browser=await playwright.chromium.launch({headless:true,channel:process.env.AGV_BROWSER_CHANNEL||'msedge'});
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];page.setDefaultTimeout(12000);
    page.on('pageerror',error=>errors.push(error.message));
    await page.goto(base);await page.locator('#agvLinkStatus').filter({hasText:'Có dữ liệu mới'}).waitFor();
    await page.locator('[data-view-target="reports"]').click();
    await page.waitForFunction(()=>document.querySelector('#kpiHistoryBox').value==='2');
    assert.equal(await page.locator('#reportsWorkspace [data-report-panel]:visible').count(),1);
    assert.equal(await page.locator('#kpiHistoryBox option').count(),2);
    assert.equal(await page.locator('#kpiBox .report-metric strong').first().innerText(),'8 / 10');
    assert.equal(await page.locator('#kpiBox details[open]').count(),0,'technical detail is collapsed initially');
    await page.locator('#reportsWorkspace').screenshot({path:path.join(directory,'desktop-performance.png')});
    await page.locator('#kpiBox summary').filter({hasText:'Hoạt động xe'}).click();
    assert.equal(await page.locator('#kpiBox .utilization-bar').count(),2,'only current robot IDs remain visible');
    assert.deepEqual(await page.locator('#kpiBox details').filter({hasText:'Hoạt động xe'}).locator('tbody tr td:first-child').allTextContents(),['01','02']);
    assert.deepEqual(await page.locator('#kpiBox details').filter({hasText:'Hoạt động xe'}).locator('tbody tr td:nth-child(2)').allTextContents(),['1h 1p','1h 1p'],'old aliases are not added to current robot durations');
    assert.equal(Object.keys(db.getLatestKpiReport().agv_utilization).length,6,'stored report is preserved');
    assert.equal(await page.evaluate(()=>window.__reportXss),undefined);
    await page.locator('#kpiBox summary').filter({hasText:'Hoạt động xe'}).click();
    await page.locator('#kpiHistoryBox').selectOption('1');
    assert.equal(await page.locator('#kpiBox .report-metric strong').first().innerText(),'4 / 8');
    await page.locator('[data-report-target="warehouse"]').click();
    await page.waitForFunction(()=>document.querySelector('#warehouseReportList').value==='warehouse-new');
    assert.equal(await page.locator('#kpiPanel').isVisible(),false);
    assert.equal(await page.locator('#warehouseReportDetail .report-table').count(),1,'one before/after table replaces duplicate tables');
    const first=page.locator('#warehouseReportDetail tbody tr').first();assert.match(await first.innerText(),/K1.*R01.*—/s);
    await page.locator('#reportsWorkspace').screenshot({path:path.join(directory,'desktop-warehouse.png')});
    await page.locator('#warehouseReportList').selectOption('warehouse-old');
    await page.evaluate(()=>loadWarehouseReports());assert.equal(await page.locator('#warehouseReportList').inputValue(),'warehouse-old');
    const download=page.waitForEvent('download');await page.locator('#warehouseReportDetail .report-download').click();
    const downloadPath=path.join(directory,'report.json');await (await download).saveAs(downloadPath);
    assert.deepEqual(JSON.parse(fs.readFileSync(downloadPath,'utf8')),saved[0],'download keeps the immutable report exactly');
    await page.locator('[data-report-target="performance"]').click();
    await page.waitForTimeout(200);assert.equal(await page.locator('#kpiHistoryBox').inputValue(),'1','switching sections preserves selected KPI');
    await page.locator('#generateKpiBtn').click();await page.waitForFunction(()=>document.querySelector('#kpiHistoryBox').value==='3');
    assert.equal(await page.locator('#kpiHistoryBox option').count(),3);
    await page.locator('[data-report-target="logs"]').click();
    assert.equal(await page.locator('#reportsWorkspace [data-report-panel]:visible').count(),1);
    await page.locator('#warehouseEventList').filter({hasText:'Xe 01 đã chuyển kệ R01'}).waitFor();
    await page.locator('#reportLogSource').selectOption('system');
    await page.locator('#systemLogBox').filter({hasText:'<img src=x'}).waitFor();
    assert.equal(await page.locator('#warehouseEventList').isVisible(),false);
    assert.equal(await page.locator('#systemLogBox img').count(),0);
    await page.locator('#reportsWorkspace').screenshot({path:path.join(directory,'desktop-logs.png')});
    await page.setViewportSize({width:390,height:844});
    for(const section of ['performance','warehouse','logs']) {
      await page.locator(`[data-report-target="${section}"]`).click();
      assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),`${section}: mobile fits`);
      await page.locator('#reportsWorkspace').screenshot({path:path.join(directory,`mobile-${section}.png`)});
    }
    await page.evaluate(()=>{kpiHistoryData=[];renderKpi(null);showWarehouseReport(null);});
    await page.locator('[data-report-target="performance"]').evaluate(button=>{
      document.querySelectorAll('[data-report-panel]').forEach(panel=>panel.hidden=panel.dataset.reportPanel!==button.dataset.reportTarget);
    });
    assert.match(await page.locator('#kpiBox').innerText(),/Chưa có báo cáo/);
    assert.equal(await page.locator('#kpiHistoryBox').isDisabled(),true);
    assert.deepEqual(errors,[]);await page.goto('about:blank');
    console.log(`PASS reports: one workspace, selected report persistence, KPI generate/details, only 01/02 without alias totals or stored report changes, warehouse before/after and immutable download, log switch/XSS, empty states, desktop/mobile. Artifacts: ${directory}`);
  } finally {
    clearInterval(pump);if(browser)await browser.close();if(gateway)await gateway.close();
    for(const socket of core.clients)socket.terminate();await new Promise(resolve=>core.close(resolve));db.close();
  }
}
main().catch(error=>{console.error(error);process.exitCode=1;});
