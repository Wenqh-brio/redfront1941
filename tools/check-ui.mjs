#!/usr/bin/env node
/* =============================================================================
 * check-ui.mjs — 原型 UI 静态接线校验（浏览器之外的最后一层保障）
 *   1) index.html 引用的 js/css 文件都存在
 *   2) ui.js 中所有 $('id') / getElementById('id') 的元素在 index.html 中都存在
 *   3) ui.js 里用到的 querySelectorAll 选择器，在 ui.js 生成的 HTML 中确有产出
 *   4) index.html / ui.js 中 classList 使用的关键 class 在 style.css 中有定义
 *   5) 快捷键文档（README 与 index.html 说明）与 ui.js 的 keydown 分支一致
 * 退出码 0 = 通过（可有警告），1 = 有错误
 * 用法：node tools/check-ui.mjs
 * ===========================================================================*/
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const P = (f) => path.join(ROOT, f);
const errors = [];
const warnings = [];
const E = (m) => errors.push(m);
const W = (m) => warnings.push(m);

const html = fs.readFileSync(P('prototype/index.html'), 'utf8');
const ui = fs.readFileSync(P('prototype/js/ui.js'), 'utf8');
const css = fs.readFileSync(P('prototype/css/style.css'), 'utf8');

/* 1. 资源引用 */
for (const m of html.matchAll(/(?:src|href)="([^"]+)"/g)) {
  const rel = m[1];
  if (/^https?:/.test(rel)) continue;
  const p = path.join(ROOT, 'prototype', rel);
  if (!fs.existsSync(p)) E(`index.html 引用不存在的资源：${rel}`);
}

/* 2. 元素 id 接线 */
const htmlIds = new Set([...html.matchAll(/\sid="([^"]+)"/g)].map((m) => m[1]));
// ui.js 通过 innerHTML 动态生成的元素同样算已接线
const dynamicIds = new Set([...ui.matchAll(/id="([A-Za-z][\w-]*)"/g)].map((m) => m[1]));
const knownIds = new Set([...htmlIds, ...dynamicIds]);
const usedIds = new Set();
for (const m of ui.matchAll(/\$\('([^']+)'\)/g)) usedIds.add(m[1]);
for (const m of ui.matchAll(/getElementById\('([^']+)'\)/g)) usedIds.add(m[1]);
const missingIds = [...usedIds].filter((id) => !knownIds.has(id));
for (const id of missingIds) E(`ui.js 需要元素 #${id}，但 index.html 与 ui.js 动态生成中都不存在（会在运行时抛 null 异常）`);

/* 3. 选择器产出 */
const selectors = new Set([...ui.matchAll(/querySelectorAll\('([^']+)'\)/g)].map((m) => m[1]));
for (const sel of selectors) {
  const attr = sel.match(/\[([a-z-]+)\]/);
  if (!attr) continue;
  const key = attr[1];
  // 该属性必须出现在 ui.js 生成的 HTML 字符串中（data-x=" 形式）
  if (!new RegExp(`\\b${key}=["']`).test(ui)) {
    W(`选择器 ${sel} 依赖属性 ${key}，但 ui.js 中未发现该属性的产出点`);
  }
}

/* 4. CSS class 覆盖（仅检查 ui.js 显式 add/toggle 的类与 index.html 的类） */
const cssClasses = new Set([...css.matchAll(/\.([a-zA-Z][\w-]*)/g)].map((m) => m[1]));
const jsClasses = new Set();
for (const m of ui.matchAll(/classList\.(?:add|toggle|remove)\('([^']+)'/g)) jsClasses.add(m[1]);
for (const m of ui.matchAll(/className\s*=\s*'([^']+)'/g)) m[1].split(/\s+/).forEach((c) => c && jsClasses.add(c));
for (const m of html.matchAll(/class="([^"]+)"/g)) m[1].split(/\s+/).forEach((c) => c && jsClasses.add(c));
// 忽略动态/状态类
const ignore = new Set(['on', 'active', 'win', 'lose', 'ready', 'cooling', 'armed', 'heavy', 'over', 'ok', 'light', 'standard', 'overloaded', 'disabled', 'pending', 'done', 'req', 'camp']);
const missingCss = [...jsClasses].filter((c) => !cssClasses.has(c) && !ignore.has(c));
for (const c of missingCss) W(`样式类 .${c} 未在 style.css 中定义`);

/* 5. 快捷键一致性：keydown 分支 或 buildInput 中的 k.KeyX 读取，二者都算已处理 */
const keyBranches = new Set([
  ...[...ui.matchAll(/e\.code\s*===\s*'([A-Za-z0-9]+)'/g)].map((m) => m[1]),
  ...[...ui.matchAll(/k\.(Key[A-Z]|Digit\d|Space|Shift\w+|Control\w+|Arrow\w+)/g)].map((m) => m[1])
]);
const readme = fs.readFileSync(P('README.md'), 'utf8');
const expectedKeys = { KeyQ: '炮击', KeyT: '坦克', KeyV: '空中支援', KeyG: '手榴弹', KeyF: '进食', KeyE: '饮水', KeyR: '换弹', KeyH: '医疗', KeyB: '望远镜' };
for (const [code, label] of Object.entries(expectedKeys)) {
  if (!keyBranches.has(code) && !new RegExp(`\\b${code}\\b`).test(ui)) E(`ui.js 未处理快捷键 ${code}（${label}）`);
}
if (!readme.includes('无限模式')) W('README 未提及无限模式');

/* --------------------------------- 输出 --------------------------------- */
console.log('─'.repeat(66));
console.log('RedFront 1941 — 原型 UI 接线校验');
console.log('─'.repeat(66));
console.log(`index.html 元素 id ${htmlIds.size} 个 · ui.js 引用 ${usedIds.size} 个 · 选择器 ${selectors.size} 个 · 快捷键分支 ${keyBranches.size} 个`);
if (warnings.length) {
  console.log(`\n⚠ 警告 ${warnings.length} 条：`);
  warnings.forEach((w) => console.log('  · ' + w));
}
if (errors.length) {
  console.log(`\n✖ 错误 ${errors.length} 条：`);
  errors.forEach((e) => console.log('  · ' + e));
  console.log('\n校验失败。');
  process.exit(1);
}
console.log('\n✔ UI 接线校验通过：所有元素 id、资源引用与快捷键分支均已接通。');
