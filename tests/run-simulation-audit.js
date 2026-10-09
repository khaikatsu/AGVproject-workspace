// Reproducible software audit; every integration suite owns its private fixtures.
const fs = require('node:fs'),path = require('node:path');
const {spawnSync} = require('node:child_process');
const {createHash} = require('node:crypto');
const root = path.resolve(__dirname,'..'),parent = path.join(root,'.qa');fs.mkdirSync(parent,{recursive:true});
const directory = fs.mkdtempSync(path.join(parent,'full-audit-')),results = [];
const hash = file=>fs.existsSync(file) ? createHash('sha256').update(fs.readFileSync(file)).digest('hex') : null;
const protectedFiles = ['data/map_layout.json','data/command_sequence.json','data/task_sequence.json','data/runtime_state.json','data/shift_state.json'];
const before = protectedFiles.map(file=>hash(path.join(root,file)));
const coreTests = process.env.AGV_CORE_TEST_BIN || path.join(root,'build',process.platform === 'win32' ? 'core_tests.exe' : 'core_tests');
const suites = [
  ['core',coreTests,[]],
  ['gateway-security',process.execPath,['--test','--test-isolation=none','tests/gateway.test.js','tests/security.test.js']],
  ...['simulation.robot.test','simulation.faults','simulation.low-battery','simulation.integration','simulation.mutex',
    'system.integration','map.integration','hardening.integration','shift.integration','firmware.integration','tls.integration',
    ...(process.platform === 'win32' ? ['startup.integration'] : [])].map(name=>[name,process.execPath,[`tests/${name}.js`]]),
  ...(process.argv.includes('--browser') ? ['browser.integration','simulation.browser'].map(name=>[name,process.execPath,[`tests/${name}.js`]]) : [])
];
console.log('Audit directory: '+directory);
for (const [name,command,args] of suites) {
  const started = Date.now(),log = path.join(directory,name+'.log'),fd = fs.openSync(log,'w');
  const run = spawnSync(command,args,{cwd:root,env:process.env,windowsHide:true,timeout:240000,stdio:['ignore',fd,fd]});fs.closeSync(fd);
  const status = run.status === 0 && !run.error ? 'PASS' : 'FAIL';
  results.push({name,status,seconds:(Date.now()-started)/1000,exitCode:run.status,error:run.error?.message,log});
  fs.writeFileSync(path.join(directory,'results.json'),JSON.stringify({date:new Date().toISOString(),level:'S1',results},null,2));
  console.log(status+' '+name+' ('+results.at(-1).seconds+'s)');
  if (status === 'FAIL') console.error(fs.readFileSync(log,'utf8').slice(-2200));
}
const unchanged = protectedFiles.map(file=>hash(path.join(root,file))).every((value,i)=>value === before[i]);
fs.writeFileSync(path.join(directory,'results.json'),JSON.stringify({date:new Date().toISOString(),level:'S1',operationalDataUnchanged:unchanged,results},null,2));
console.log('Operational map/counters/runtime/shift unchanged: '+unchanged);
console.log('Report: '+path.join(directory,'results.json'));
if (!unchanged || results.some(test=>test.status !== 'PASS')) process.exitCode = 1;
