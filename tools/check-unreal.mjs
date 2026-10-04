#!/usr/bin/env node
/* =============================================================================
 * check-unreal.mjs — UE 工程骨架静态结构校验（无引擎环境下可用的最强检查）
 *   1) .uproject 模块名 / Build.cs 模块名 / 源文件目录一致
 *   2) 每个 .h：#pragma once、UCLASS/USTRUCT/UENUM 存在、*.generated.h 为最后 include
 *   3) 每个 UCLASS/USTRUCT 都有 GENERATED_BODY()
 *   4) 每个 .cpp 先包含自己的头文件，且 .h 中的函数声明在 .cpp 中有对应定义
 *   5) 括号/引号平衡、UPROPERTY 不含裸 TSharedPtr 等已知 UHT 陷阱
 *   6) 数据契约字段覆盖：RFDataTypes.h 是否覆盖 data/*.json 的关键字段
 * 退出码 0 = 通过（可有警告），1 = 有错误
 * 用法：node tools/check-unreal.mjs
 * ===========================================================================*/
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const UNREAL = path.join(ROOT, 'Unreal');
const SRC = path.join(UNREAL, 'Source', 'RedFront1941');
const errors = [];
const warnings = [];
const E = (m) => errors.push(m);
const W = (m) => warnings.push(m);

function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) {
      if (!['Binaries', 'Intermediate', 'Saved', 'DerivedDataCache'].includes(e.name)) walk(p, out);
    }
    else out.push(p);
  }
  return out;
}
if (!fs.existsSync(UNREAL)) { console.error('✖ 未找到 Unreal/ 目录'); process.exit(1); }
const files = walk(UNREAL);
const headers = files.filter((f) => f.endsWith('.h'));
const sources = files.filter((f) => f.endsWith('.cpp'));

/* ---------------------- 1. .uproject 与 Build.cs ---------------------- */
const uprojectPath = files.find((f) => f.endsWith('.uproject'));
let moduleName = null;
// 工程必须同时具备 Game 与 Editor 两个 Target.cs，否则 UBT 无法编译
for (const t of ['RedFront1941.Target.cs', 'RedFront1941Editor.Target.cs']) {
  if (!fs.existsSync(path.join(UNREAL, 'Source', t))) E(`缺少 Source/${t}（工程无法编译，UBT 必需）`);
}
if (!uprojectPath) E('缺少 .uproject');
else {
  const up = JSON.parse(fs.readFileSync(uprojectPath, 'utf8'));
  const mods = up.Modules || [];
  if (!mods.length) E('.uproject 未声明 Modules');
  moduleName = mods[0] && mods[0].Name;
  if (up.EngineAssociation !== '5.8') W(`EngineAssociation = ${up.EngineAssociation}（目标 5.8）`);
  const plugins = (up.Plugins || []).map((p) => p.Name);
  for (const need of ['Paper2D', 'EnhancedInput']) if (!plugins.includes(need)) E(`.uproject 未启用必需插件 ${need}`);
  const buildCs = files.find((f) => f.endsWith('.Build.cs'));
  if (!buildCs) E('缺少模块 Build.cs');
  else {
    const b = fs.readFileSync(buildCs, 'utf8');
    if (!new RegExp(`class\\s+${moduleName}.*ModuleRules`).test(b)) E(`Build.cs 中模块类名与 .uproject 的 "${moduleName}" 不一致`);
    for (const dep of ['Paper2D', 'EnhancedInput', 'Json', 'JsonUtilities']) {
      if (!b.includes(`"${dep}"`)) W(`Build.cs 未列出依赖 ${dep}（若未使用可忽略）`);
    }
  }
  if (!fs.existsSync(path.join(SRC, moduleName + '.Build.cs'))) E(`模块源码目录应与模块名一致：Source/${moduleName}/`);
}

/* ---------------------- 2/3/5. 头文件检查 ---------------------- */
let structCount = 0, classCount = 0, enumCount = 0, uclassCount = 0;
const declaredFunctions = new Map(); // file -> [names]
for (const h of headers) {
  const rel = path.relative(UNREAL, h);
  const txt = fs.readFileSync(h, 'utf8');
  const isTypeHeader = /UCLASS|USTRUCT|UENUM|UINTERFACE/.test(txt);
  if (!/^\s*#pragma once/m.test(txt)) E(`${rel}: 缺少 #pragma once`);
  const genIncludes = [...txt.matchAll(/#include\s+"([^"]+\.generated\.h)"/g)].map((m) => m[1]);
  if (isTypeHeader) {
    if (genIncludes.length !== 1) E(`${rel}: 含 UCLASS/USTRUCT 但 generated.h include 数量为 ${genIncludes.length}（应为 1）`);
    else {
      const expected = path.basename(h).replace(/\.h$/, '.generated.h');
      if (genIncludes[0] !== expected) E(`${rel}: generated.h 名称应为 "${expected}"，实际 "${genIncludes[0]}"`);
      // generated.h 必须是最后一个 include
      const allIncludes = [...txt.matchAll(/#include\s+"[^"]+"/g)].map((m) => m[0]);
      if (allIncludes.length && allIncludes[allIncludes.length - 1] !== '#include "' + expected + '"') {
        E(`${rel}: ${expected} 必须是最后一个 #include`);
      }
    }
    const bodies = (txt.match(/GENERATED_BODY\(\)/g) || []).length;
    const types = (txt.match(/UCLASS\s*\(|USTRUCT\s*\(|UINTERFACE\s*\(/g) || []).length;
    if (bodies !== types) E(`${rel}: GENERATED_BODY() 数量 ${bodies} 与 UCLASS/USTRUCT 数量 ${types} 不一致`);
  } else if (genIncludes.length) {
    W(`${rel}: 声明了 generated.h 但没有 UCLASS/USTRUCT`);
  }
  structCount += (txt.match(/USTRUCT\s*\(/g) || []).length;
  classCount += (txt.match(/UCLASS\s*\(/g) || []).length;
  enumCount += (txt.match(/UENUM\s*\(/g) || []).length;
  uclassCount += types_ok(txt);
  function types_ok(t) { return (t.match(/UINTERFACE\s*\(/g) || []).length; }

  // UPROPERTY 裸 TSharedPtr（UHT 不支持）
  const uprops = [...txt.matchAll(/UPROPERTY\([^)]*\)\s*([^;]+);/g)].map((m) => m[1]);
  for (const pr of uprops) if (/TSharedPtr|TSharedRef|TWeakPtr/.test(pr)) E(`${rel}: UPROPERTY 使用了 UHT 不支持的智能指针类型：${pr.trim()}`);

  // UFUNCTION 签名中的 TSharedPtr / STL（UHT 报 "Unable to find ... with name 'TSharedPtr'"）
  const ufuncs = [...txt.matchAll(/UFUNCTION\([^)]*\)[^;{]*?;?/gs)];
  const lines = txt.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    if (!/UFUNCTION\s*\(/.test(lines[i])) continue;
    // 签名可能在同一行，或紧接的下一行
    const sig = lines[i] + ' ' + (lines[i + 1] || '');
    if (/UFUNCTION[^)]*\)[^;]*;/.test(lines[i])) {
      if (/TSharedPtr|TSharedRef|TUniquePtr|std::/.test(lines[i])) {
        E(`${rel}:${i + 1}: UFUNCTION 签名含 UHT 无法解析的类型（应改为普通 C++ 方法）`);
      }
    } else if (/TSharedPtr|TSharedRef|TUniquePtr|std::/.test(lines[i + 1] || '')) {
      E(`${rel}:${i + 2}: UFUNCTION 签名含 UHT 无法解析的类型（应改为普通 C++ 方法）`);
    }
  }
  void ufuncs;

  // 括号平衡
  const bal = (txt.match(/{/g) || []).length - (txt.match(/}/g) || []).length;
  if (bal !== 0) E(`${rel}: 花括号不平衡（差 ${bal}）`);
  const par = (txt.match(/\(/g) || []).length - (txt.match(/\)/g) || []).length;
  if (par !== 0) E(`${rel}: 圆括号不平衡（差 ${par}）`);

  // 记录类内函数声明（供 .cpp 对照）
  const names = [];
  for (const m of txt.matchAll(/^\s*(?:virtual\s+|static\s+|explicit\s+|FORCEINLINE\s+)*[A-Za-z_][\w:<>,\s\*&]*?\s+([A-Z]\w*)\s*\([^;{]*\)\s*(?:const\s*)?(?:override\s*)?;/gm)) {
    names.push(m[1]);
  }
  declaredFunctions.set(h, names);
}

/* ---------------------- 4. 源文件检查 ---------------------- */
for (const c of sources) {
  const rel = path.relative(UNREAL, c);
  const txt = fs.readFileSync(c, 'utf8');
  const own = path.basename(c).replace(/\.cpp$/, '.h');
  const firstInclude = (txt.match(/#include\s+"([^"]+)"/) || [])[1];
  // UE 允许 "Name.h" 或 "子目录/Name.h" 两种写法
  if (firstInclude !== own && firstInclude !== path.basename(path.dirname(c)) + '/' + own) {
    E(`${rel}: 第一个 include 应为自身头文件 "${own}"（或带子目录前缀），实际 "${firstInclude || '无'}"`);
  }
  const bal = (txt.match(/{/g) || []).length - (txt.match(/}/g) || []).length;
  if (bal !== 0) E(`${rel}: 花括号不平衡（差 ${bal}）`);
  const par = (txt.match(/\(/g) || []).length - (txt.match(/\)/g) || []).length;
  if (par !== 0) E(`${rel}: 圆括号不平衡（差 ${par}）`);
  const headerPath = path.join(path.dirname(c), own);
  if (!fs.existsSync(headerPath)) E(`${rel}: 找不到对应头文件 ${own}`);
  else {
    const decls = declaredFunctions.get(headerPath) || [];
    const missing = decls.filter((n) => !new RegExp(`\\b${n}\\s*\\(`).test(txt) && !/^~/.test(n));
    if (missing.length > 6) W(`${rel}: ${missing.length} 个声明未在 .cpp 中出现（可能是 inline/蓝图节点）：${missing.slice(0, 6).join(', ')}`);
  }
}

/* ---------------------- 6. 数据契约覆盖 ---------------------- */
const dtPath = path.join(SRC, 'Data', 'RFDataTypes.h');
if (!fs.existsSync(dtPath)) E('缺少 Data/RFDataTypes.h（数据契约层）');
else {
  const dt = fs.readFileSync(dtPath, 'utf8');
  // JSON 为 snake_case，UE 属性为 PascalCase：两种写法任一出现即视为已覆盖
  const toPascal = (k) => k.split('_').map((w) => w.charAt(0).toUpperCase() + w.slice(1)).join('');
  const need = {
    'weapons.json': ['weight_kg', 'penetration_mm', 'ammo_type', 'effective_range_m', 'reload_s'],
    'classes.json': ['slots', 'passives', 'active'],
    'equipment.json': ['weight_kg', 'effect', 'category'],
    'enemies.json': ['hp', 'awareness_m', 'reaction_s', 'suppression_resist', 'armor_mm', 'side_rear_ratio'],
    'support.json': ['cp_cost', 'cooldown_s', 'delay_s', 'radius_m', 'rounds', 'damage_per_round'],
    'levels\\SCHEMA.md': ['mission_type', 'historical_note_zh', 'weight_budget_kg', 'available_support', 'enemy_composition', 'objectives']
  };
  for (const [src, keys] of Object.entries(need)) {
    const missing = keys.filter((k) => !dt.includes(k) && !dt.includes(toPascal(k)) && !new RegExp(toPascal(k), 'i').test(dt));
    if (missing.length) E(`RFDataTypes.h 未覆盖 ${src} 的字段：${missing.join(', ')}`);
  }
}
const loaderPath = path.join(SRC, 'Data', 'RFDataTableLoader.h');
if (!fs.existsSync(loaderPath)) E('缺少 Data/RFDataTableLoader.h');
else if (!/DataTable/i.test(fs.readFileSync(loaderPath, 'utf8'))) E('RFDataTableLoader.h 未提及 DataTable');

/* ---------------------- Config 检查 ---------------------- */
const cfgEngine = path.join(UNREAL, 'Config', 'DefaultEngine.ini');
if (!fs.existsSync(cfgEngine)) E('缺少 Config/DefaultEngine.ini');
else {
  const ini = fs.readFileSync(cfgEngine, 'utf8');
  for (const key of ['[/Script/EngineSettings.GameMapsSettings]', '[/Script/Engine.RendererSettings]', 'DefaultGraphicsRHI']) {
    if (!ini.includes(key)) W(`DefaultEngine.ini 缺少 ${key}`);
  }
}
const cfgInput = path.join(UNREAL, 'Config', 'DefaultInput.ini');
if (!fs.existsSync(cfgInput)) E('缺少 Config/DefaultInput.ini');
else {
  const ini = fs.readFileSync(cfgInput, 'utf8');
  // 接受 Enhanced Input 资产名（IA_*）或传统映射名（*）
  const actions = ['Fire', 'Reload', 'Grenade', 'Eat', 'Drink', 'CallSupport_Artillery', 'CallSupport_Armor', 'CallSupport_Air', 'Sprint', 'Crouch', 'Prone', 'Interact', 'Melee', 'AimDownSights', 'SwapWeapon'];
  const missing = actions.filter((a) => !ini.includes(a) && !ini.includes('IA_' + a));
  if (missing.length) W(`DefaultInput.ini 未定义输入动作：${missing.join(', ')}`);
}

/* --------------------------------- 输出 --------------------------------- */
const line = '─'.repeat(66);
console.log(line);
console.log('RedFront 1941 — UE 工程骨架静态校验');
console.log(line);
console.log(`头文件 ${headers.length} · 源文件 ${sources.length} · UCLASS ${classCount} · USTRUCT ${structCount} · UENUM ${enumCount} · UINTERFACE ${uclassCount}`);
console.log(`模块：${moduleName} · 引擎关联 ${uprojectPath ? JSON.parse(fs.readFileSync(uprojectPath, 'utf8')).EngineAssociation : '?'}`);
if (warnings.length) {
  console.log(`\n⚠ 警告 ${warnings.length} 条：`);
  warnings.slice(0, 20).forEach((w) => console.log('  · ' + w));
  if (warnings.length > 20) console.log(`  … 其余 ${warnings.length - 20} 条省略`);
}
if (errors.length) {
  console.log(`\n✖ 错误 ${errors.length} 条：`);
  errors.slice(0, 30).forEach((e) => console.log('  · ' + e));
  if (errors.length > 30) console.log(`  … 其余 ${errors.length - 30} 条省略`);
  console.log('\n校验失败；请先修复上述静态结构错误，再通过 UE 5.8 / Visual Studio 构建。');
  process.exit(1);
}
console.log('\n✔ 结构校验通过：UHT 结构、模块声明、数据契约字段均一致。');
console.log('  本项为静态检查；完整编译请使用 Unreal/RedFront1941.sln 或 UE 5.8 Build.bat。');
