/*
 * 回归验证：node verify.js
 *
 * 静态层：JS 语法、CFG 与 JSON 一致、C# / C 括号与符号、三版本函数名对齐、源码编码。
 * 运行层：用本机 Edge 无头模式加载临时页面，注入探针读取画布像素与 JS 异常，
 *         覆盖「标题 / 键盘启动 / 按钮启动 / 长跑不崩 / 60Hz 压测」。
 *
 * 不依赖任何 npm 包。浏览器路径找不到时只做静态层并给出提示。
 */
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const cp = require('child_process');

const dir = __dirname;
const read = f => fs.readFileSync(path.join(dir, f), 'utf8');
let failed = 0;
function ok(name, cond, detail) {
  console.log((cond ? '  PASS  ' : '  FAIL  ') + name + (detail ? '  ' + detail : ''));
  if (!cond) failed++;
}

// ---------- 静态层 ----------
console.log('[静态检查]');
const html = read('index.html');
const script = html.match(/<script>([\s\S]*?)<\/script>/);
ok('内联 JS 可被解析', !!script, script ? '(' + script[1].length + ' 字符)' : '');
let jsOk = false;
try { new Function(script[1]); jsOk = true; } catch (e) { console.log('    ' + e.message); }
ok('JS 语法有效', jsOk);

const cfg = JSON.parse(read('game-config.json'));
const block = html.match(/const CFG = ([\s\S]*?);\n  \/\/ <<< CONFIG:END >>>/);
let cfgOk = false;
try { cfgOk = !!block && JSON.stringify(JSON.parse(block[1])) === JSON.stringify(cfg); } catch (e) { /* ignore */ }
ok('CFG 与 game-config.json 完全一致', cfgOk);

function balance(src) {
  let b = 0, p = 0, q = 0;
  for (const c of src) {
    if (c === '{') b++;
    else if (c === '}') b--;
    else if (c === '(') p++;
    else if (c === ')') p--;
    else if (c === '"') q++;
  }
  return { b: b, p: p, q: q };
}
for (const f of ['Program.cs', 'Config.cs']) {
  const r = balance(read(f));
  ok(f + ' 括号/引号平衡', r.b === 0 && r.p === 0 && r.q % 2 === 0,
     'braces=' + r.b + ' parens=' + r.p + ' quotes=' + r.q);
}

const shared = [
  'reset', 'start', 'gameOver', 'panel', 'drawTitle', 'drawPause', 'drawOver',
  'makeEnemy', 'spawnCoin', 'burst', 'update', 'render',
  'drawCar', 'drawRoad', 'drawCoin', 'drawHUD', 'drawWarn', 'drawFloaters',
  'drawSceneryFar', 'drawSceneryNear', 'drawSpeedLines',
  'queueWave', 'emitWave', 'ensureReachable', 'waveInterval',
  'addFloater', 'hitEnemy'
];
const pascal = s => s.charAt(0).toUpperCase() + s.slice(1);
const htmlFns = new Set(), csFns = new Set(), cFns = new Set();
let m;
const reH = /function\s+([A-Za-z_$][\w$]*)\s*\(/g;
while ((m = reH.exec(html))) htmlFns.add(m[1]);
const reCsFn = /\b(?:void|double|int|bool|string|Bitmap|Enemy|float)\s+([A-Z]\w*)\s*\(/g;
while ((m = reCsFn.exec(read('Program.cs')))) csFns.add(m[1]);
const hasC = fs.existsSync(path.join(dir, 'main.c'));
const cSrc = hasC ? read('main.c') : '';
const hSrc = hasC ? read('game-config.h') : '';
if (hasC) {
  const reCFn = /\b(?:void|int|double|Enemy|Wave|float)\s+([a-z]\w*)\s*\(/g;
  while ((m = reCFn.exec(cSrc))) cFns.add(m[1]);
}
const missing = [];
for (const fn of shared) {
  if (!htmlFns.has(fn)) missing.push('index.html:' + fn);
  if (!csFns.has(pascal(fn))) missing.push('Program.cs:' + pascal(fn));
  if (hasC && !cFns.has(fn)) missing.push('main.c:' + fn);
}
ok((hasC ? '三版本' : '双版本') + '逻辑函数一一对齐', missing.length === 0,
   missing.length ? '缺失: ' + missing.join(', ') : '(' + shared.length + ' 个)');

// 编译器调用被本机策略拦截时，用符号级校验兜住最常见的两类 C# 编译错误：
// 引用了不存在的 Config 成员，或误用了 csc v4 不支持的 C# 6+ 语法。
const progRaw = read('Program.cs');
const progCode = progRaw.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const cfgDefined = new Set();
let dm;
const reDef = /^\s*public\s+(?:const|static readonly)\s+.+?\s+(\w+)\s*=/gm;
while ((dm = reDef.exec(read('Config.cs')))) cfgDefined.add(dm[1]);
const refs = [];
let rm;
const reRef = /Config\.(\w+)/g;
while ((rm = reRef.exec(progCode))) refs.push(rm[1]);
const undef = refs.filter((x, i, a) => a.indexOf(x) === i && !cfgDefined.has(x));
ok('C# 引用的 Config 成员全部已定义', undef.length === 0,
   undef.length ? '未定义: ' + undef.join(', ') : '(' + new Set(refs).size + ' 个引用)');
const modern = [];
if (/\$"/.test(progCode)) modern.push('字符串插值');
if (/\?\./.test(progCode)) modern.push('null 条件运算符');
if (/=>/.test(progCode)) modern.push('lambda / 表达式体');
if (/\bnameof\s*\(/.test(progCode)) modern.push('nameof');
if (/\bout\s+var\b/.test(progCode)) modern.push('out var');
ok('Program.cs 保持 C# 5 兼容（csc 可直接编译）', modern.length === 0,
   modern.length ? modern.join(', ') : '禁用语法 0 处');

// C 版：编码、括号、配置符号，以及两个 MSVC 特有的坑。
if (hasC) {
  const strip = s => s.replace(/\/\*[\s\S]*?\*\//g, '')
                      .replace(/\/\/[^\n]*/g, '')
                      .replace(/"(?:\\.|[^"\\])*"/g, '""')
                      .replace(/'(?:\\.|[^'\\])*'/g, "''");
  for (const pair of [['main.c', cSrc], ['game-config.h', hSrc]]) {
    const r = balance(strip(pair[1]));
    ok(pair[0] + ' 括号/引号平衡', r.b === 0 && r.p === 0 && r.q % 2 === 0,
       'braces=' + r.b + ' parens=' + r.p + ' quotes=' + r.q);
  }
  ok('C 源码为纯 ASCII（避免 CP936 误读）',
     !/[^\x00-\x7F]/.test(cSrc) && !/[^\x00-\x7F]/.test(hSrc));
  const idRe = /\b(?:GC_[A-Z0-9_]+|GCW|GCP|GCPOOL)\b/g;
  const hIds = new Set(hSrc.match(idRe) || []);
  const cIds = Array.from(new Set(cSrc.match(idRe) || []));
  const undefC = cIds.filter(x => !hIds.has(x));
  ok('main.c 引用的配置符号全部已在 game-config.h 定义', undefC.length === 0,
     undefC.length ? '未定义: ' + undefC.join(', ') : '(' + cIds.length + ' 个符号)');
  const codeOnly = strip(cSrc);
  ok('main.c 未误用 windows.h 的 near 空宏', !/\bnear\b/.test(codeOnly));
  ok('main.c 未对 double 使用 % 运算符', !/\(int\)\s*\(\s*g_scroll\s*%/.test(codeOnly));
}

// ---------- 运行层 ----------
const edgeCandidates = [
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Microsoft/Edge/Application/msedge.exe'
];
const edge = edgeCandidates.filter(p => fs.existsSync(p))[0];
if (!edge) {
  console.log('\n[运行检查] 未找到 Edge，跳过浏览器回归（静态检查仍有效）。');
  console.log('\n结果: ' + (failed ? failed + ' 项失败' : '全部通过') + '（运行层未执行）');
  process.exit(failed ? 1 : 0);
}

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'pixel-racer-verify-'));
const profile = path.join(tmp, 'profile');

function probe(body) {
  return '<script>(function(){' +
    'var errs=[],frames=0,samples=[];' +
    'window.addEventListener("error",function(e){errs.push(String(e.message))});' +
    'window.addEventListener("unhandledrejection",function(){errs.push("rejection")});' +
    'function tick(){frames++;requestAnimationFrame(tick)}requestAnimationFrame(tick);' +
    'function px(){var c=document.getElementById("game");var d=c.getContext("2d").getImageData(80,120,1,1).data;return d[0]+","+d[1]+","+d[2]}' +
    'function lives(){var c=document.getElementById("game"),x=c.getContext("2d"),a=[];for(var i=0;i<5;i++){var d=x.getImageData(7+i*7,14,1,1).data;a.push(d[0]+","+d[1]+","+d[2])}return a.join("/")}' +
    'function emit(tag){samples.push(tag+":"+px()+":"+lives())}' +
    body +
    'setTimeout(function(){var el=document.createElement("div");el.id="probe";' +
    'el.textContent="PROBE:"+encodeURIComponent(JSON.stringify({errs:errs,frames:frames,samples:samples}));' +
    'document.body.appendChild(el)},FINAL);' +
    '})();<\/script>';
}

function runCase(name, body, budget, finalAt) {
  const page = html.replace('</body>', probe(body).replace(/FINAL/g, String(finalAt)) + '\n</body>');
  const file = path.join(tmp, name + '.html');
  fs.writeFileSync(file, page, 'utf8');
  const url = 'file:///' + file.replace(/\\/g, '/');
  let out = '';
  try {
    out = cp.execFileSync(edge, [
      '--headless=new', '--disable-gpu', '--no-sandbox', '--disable-extensions',
      '--hide-scrollbars', '--user-data-dir=' + profile,
      '--virtual-time-budget=' + budget, '--dump-dom', url
    ], { encoding: 'utf8', timeout: 90000, maxBuffer: 32 * 1024 * 1024, stdio: ['ignore', 'pipe', 'ignore'] });
  } catch (e) {
    console.log('  FAIL  ' + name + ' 浏览器执行异常: ' + e.message);
    failed++;
    return null;
  }
  const hit = out.match(/<div id="probe">PROBE:([^<]*)<\/div>/);
  if (!hit) { console.log('  FAIL  ' + name + ' 未取到探针输出'); failed++; return null; }
  try { return JSON.parse(decodeURIComponent(hit[1])); }
  catch (e) { console.log('  FAIL  ' + name + ' 探针解析失败'); failed++; return null; }
}

const bright = s => { const p = s.split(',').map(Number); return (p[0] + p[1] + p[2]) / 3; };
console.log('\n[浏览器回归]  ' + path.basename(edge));

const base = runCase('title',
  'setTimeout(function(){emit("title")},900);', 1400, 1200);

const keyAction = 'setTimeout(function(){window.dispatchEvent(new KeyboardEvent("keydown",{code:"Space",key:" ",bubbles:true}))},250);' +
  'setTimeout(function(){emit("keyboard")},900);setTimeout(function(){emit("keyboard2")},1500);';
const keyRun = runCase('keyboard', keyAction, 2000, 1800);

const btnAction = 'setTimeout(function(){document.getElementById("btnGo").dispatchEvent(new Event("pointerdown",{bubbles:true,cancelable:true}))},250);' +
  'setTimeout(function(){emit("button")},900);';
const btnRun = runCase('button', btnAction, 1600, 1400);

const longAction = 'setTimeout(function(){window.dispatchEvent(new KeyboardEvent("keydown",{code:"Space",key:" ",bubbles:true}))},250);' +
  'setTimeout(function(){emit("t1")},1500);setTimeout(function(){emit("t3")},3500);setTimeout(function(){emit("t5")},5500);';
const longRun = runCase('longrun', longAction, 6500, 6200);

// 无头环境的 rAF 会被冻结到个位数帧，这里用 setTimeout 按 60Hz 驱动渲染循环，
// 真正跑过数十秒游戏时间，覆盖阶段升级、波次、碰撞掉命、连击等新逻辑。
const stressAction =
  'var fakeNow=performance.now();' +
  'window.requestAnimationFrame=function(cb){return setTimeout(function(){fakeNow+=16.7;cb(fakeNow)},1)};' +
  'setTimeout(function(){window.dispatchEvent(new KeyboardEvent("keydown",{code:"Space",key:" ",bubbles:true}))},50);' +
  'setTimeout(function(){emit("s1")},400);' +
  'setTimeout(function(){emit("s2")},1600);' +
  'setTimeout(function(){emit("s3")},2900);';
const stress = runCase('stress', stressAction, 3500, 3300);

ok('标题页正常渲染', !!base && base.errs.length === 0 && base.samples.length === 1,
   base ? '中心像素=' + base.samples[0].split(':')[1] : '');
if (base) ok('标题面板为暗色', bright(base.samples[0].split(':')[1]) < 45);

for (const [label, r] of [['键盘启动', keyRun], ['按钮启动', btnRun]]) {
  ok(label + ' 无 JS 异常', !!r && r.errs.length === 0, r && r.errs.length ? r.errs.join(';') : '');
  const started = r && r.samples.length > 0 && bright(r.samples[0].split(':')[1]) > 45;
  ok(label + ' 已进入游戏（中心变路面色）', started,
     r && r.samples.length ? '中心像素=' + r.samples[0].split(':')[1] : '');
}
ok('长跑 6 秒无异常且渲染循环持续', !!longRun && longRun.errs.length === 0 && longRun.frames > 2,
   longRun ? 'frames=' + longRun.frames + ' samples=' + longRun.samples.length +
             (longRun.errs.length ? ' errs=' + longRun.errs.join(';') : '') : '');

const stressFrames = stress ? stress.frames : 0;
const stressPixels = stress ? stress.samples.map(s => s.split(':')[1]) : [];
ok('压测：60Hz 驱动下真的跑满帧', stressFrames > 300, 'frames=' + stressFrames);
ok('压测：长时间运行无 JS 异常', !!stress && stress.errs.length === 0,
   stress && stress.errs.length ? stress.errs.join(';') : '');
ok('压测：画面持续变化（世界在推进）',
   stressPixels.length >= 2 && new Set(stressPixels).size >= 2,
   stressPixels.join(' | '));
const lifeStates = stress ? stress.samples.map(s => s.split(':')[2]) : [];
ok('压测：生命 HUD 随碰撞递减（三条命生效）',
   new Set(lifeStates).size >= 2,
   lifeStates.map((s, i) => 't' + i + '=' + s).join(' | '));

try { fs.rmSync(tmp, { recursive: true, force: true }); } catch (e) { /* ignore */ }
console.log('\n结果: ' + (failed ? failed + ' 项失败' : '全部通过'));
process.exit(failed ? 1 : 0);
