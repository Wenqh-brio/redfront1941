#!/usr/bin/env node
/* =============================================================================
 * smoke-test-ui.mjs — 原型 UI 运行时冒烟测试
 * 用一个轻量假 DOM 真正加载 prototype/js/ui.js，并驱动完整流程：
 *   主菜单 → 关卡列表 → 战役切换 → 选中关卡 → 简报 → 装备（改负重/切换分栏）
 *   → 部署 → 逐帧运行游戏 → 暂停/继续 → 结算 → 重打 → 无限模式设置 → 返回
 * 目的：捕获"脚本加载期空引用"「元素 id 拼错」「流程中 null 崩溃」这类静态检查抓不到的问题。
 * 用法：node tools/smoke-test-ui.mjs
 * ===========================================================================*/
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';
import vm from 'node:vm';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const P = (f) => path.join(ROOT, f);
let failures = 0, checks = 0;
const detail = [];
const ok = (c, label) => { checks++; if (!c) { failures++; detail.push(label); } };

/* ------------------------------ 假 DOM ------------------------------ */
function makeCtx() {
  const t = {
    canvas: { width: 1280, height: 720 },
    createRadialGradient: () => ({ addColorStop() {} }),
    createLinearGradient: () => ({ addColorStop() {} }),
    measureText: () => ({ width: 10 }),
    save() {}, restore() {}
  };
  return new Proxy(t, { get: (o, k) => (k in o ? o[k] : function () {}), set: (o, k, v) => (o[k] = v, true) });
}
const ctxStub = makeCtx();

let elementSeq = 0;
function makeElement(id, tag) {
  const el = {
    _id: id || ('el' + (++elementSeq)), tagName: (tag || 'div').toUpperCase(),
    children: [], _attrs: {}, _html: '', _handlers: {},
    style: {}, dataset: {}, classList: null, hidden: false, value: '', checked: false,
    classList_manual: new Set(),
    addEventListener(type, fn) { (this._handlers[type] = this._handlers[type] || []).push(fn); },
    removeEventListener() {},
    dispatch(type, ev) { (this._handlers[type] || []).forEach((f) => f(Object.assign({ preventDefault() {}, stopPropagation() {}, target: this, button: 0 }, ev))); },
    getAttribute(k) { return Object.prototype.hasOwnProperty.call(this._attrs, k) ? this._attrs[k] : null; },
    setAttribute(k, v) { this._attrs[k] = String(v); if (k === 'hidden') this.hidden = true; },
    removeAttribute(k) { delete this._attrs[k]; if (k === 'hidden') this.hidden = false; },
    getBoundingClientRect() { return { left: 0, top: 0, width: 1280, height: 720, right: 1280, bottom: 720 }; },
    getContext() { return ctxStub; },
    appendChild(c) { this.children.push(c); return c; },
    insertAdjacentHTML(pos, html) { this._html += html; this._parse(html); },
    closest(sel) { return this._matchSelf(sel) ? this : null; },
    focus() {}, blur() {}, click() { this.dispatch('click', {}); },
    _parse(html) {
      // 极简解析：抓取带 data-* / id / class 的元素，供 querySelectorAll 使用
      const re = /<(div|button|section|canvas|span|li|table|input)([^>]*)>/g;
      let m;
      while ((m = re.exec(html))) {
        const attrs = {};
        for (const a of m[2].matchAll(/([a-zA-Z-]+)="([^"]*)"/g)) attrs[a[1]] = a[2];
        const child = makeElement(attrs.id, m[1]);
        Object.assign(child._attrs, attrs);
        child._className = attrs.class || '';
        child._text = (html.slice(m.index + m[0].length).match(/^([^<]*)/) || ['', ''])[1];
        this.children.push(child);
      }
    },
    _matchSelf(sel) {
      const s = sel.trim();
      if (s.startsWith('#')) return this._id === s.slice(1);
      const attr = s.match(/^\[([a-zA-Z-]+)\]$/);
      if (attr) return this._attrs[attr[1]] !== undefined;
      const attrEq = s.match(/^\[([a-zA-Z-]+)="([^"]*)"\]$/);
      if (attrEq) return this._attrs[attrEq[1]] === attrEq[2];
      if (s.startsWith('.')) return (this._className || '').split(/\s+/).includes(s.slice(1));
      return this.tagName === s.toUpperCase();
    },
    querySelectorAll(sel) { return this.children.filter((c) => c._matchSelf(sel)); },
    querySelector(sel) { return this.querySelectorAll(sel)[0] || null; }
  };
  el.classList = {
    add: (c) => el.classList_manual.add(c),
    remove: (c) => el.classList_manual.delete(c),
    toggle: (c, on) => { if (on === undefined) { el.classList_manual.has(c) ? el.classList_manual.delete(c) : el.classList_manual.add(c); } else if (on) el.classList_manual.add(c); else el.classList_manual.delete(c); },
    contains: (c) => el.classList_manual.has(c)
  };
  Object.defineProperty(el, 'innerHTML', {
    get() { return this._html; },
    set(v) { this._html = String(v); this.children = []; this._parse(this._html); }
  });
  Object.defineProperty(el, 'textContent', {
    get() { return this._text || ''; }, set(v) { this._text = String(v); }
  });
  Object.defineProperty(el, 'className', {
    get() { return this._className || ''; }, set(v) { this._className = String(v); }
  });
  el.parentElement = { clientWidth: 1280, clientHeight: 720, appendChild() {} };
  if (el.tagName === 'CANVAS') { el.width = 1280; el.height = 720; }
  return el;
}

const idCache = new Map();
const documentStub = {
  readyState: 'complete',
  body: makeElement('body'),
  createElement: (t) => makeElement(null, t),
  getElementById(id) { if (!idCache.has(id)) idCache.set(id, makeElement(id)); return idCache.get(id); },
  querySelectorAll(sel) { return [].concat(documentStub.body.querySelectorAll(sel)); },
  querySelector(sel) { return documentStub.querySelectorAll(sel)[0] || null; },
  addEventListener(type, fn) { (this._h = this._h || {}), (this._h[type] = this._h[type] || []).push(fn); },
  removeEventListener() {},
  _h: {}
};

let rafQueue = [];
let nowMs = 0;
const windowHandlers = {};
const sandbox = {
  console, Math, JSON, Date, Object, Array, String, Number, Boolean, Error, RegExp, Map, Set, isFinite, isNaN, parseInt, parseFloat, Promise,
  performance: { now: () => nowMs },
  requestAnimationFrame: (fn) => { rafQueue.push(fn); return rafQueue.length; },
  cancelAnimationFrame: () => { rafQueue = []; },
  setTimeout: (fn) => { fn(); return 0; },
  clearTimeout: () => {},
  localStorage: { getItem: () => null, setItem() {} },
  innerWidth: 1280, innerHeight: 720,
  addEventListener: (t, fn) => { (windowHandlers[t] = windowHandlers[t] || []).push(fn); },
  removeEventListener: () => {},
  document: documentStub
};
sandbox.window = sandbox;
sandbox.globalThis = sandbox;
vm.createContext(sandbox);

/* --------------------------- 加载脚本并驱动 --------------------------- */
function load(rel) { vm.runInContext(fs.readFileSync(P(rel), 'utf8'), sandbox, { filename: rel }); }

function fireDocument(type, ev) { (documentStub._h[type] || []).forEach((f) => f(Object.assign({ preventDefault() {}, stopPropagation() {} }, ev))); }
function fireWindow(type, ev) { (windowHandlers[type] || []).forEach((f) => f(Object.assign({ preventDefault() {}, stopPropagation() {}, code: '' }, ev))); }
function tick(frames = 1, dtMs = 16.7) {
  for (let i = 0; i < frames; i++) {
    const q = rafQueue; rafQueue = [];
    nowMs += dtMs;
    for (const fn of q) fn(nowMs);
  }
}
function clickEl(el) {
  if (!el) return;
  if (el.onclick) el.onclick({ preventDefault() {}, stopPropagation() {}, target: el });
  el.dispatch('click', { target: el });
}
/** 用带 closest 的事件触发 document 级委托点击（模拟 data-go 场景） */
function clickDelegate(el) {
  const ev = { preventDefault() {}, stopPropagation() {}, target: { closest: (sel) => (el && el._matchSelf && el._matchSelf(sel) ? el : null) } };
  if (el && el.onclick) el.onclick(ev);
  fireDocument('click', ev);
}

console.log('═'.repeat(70));
console.log('RedFront 1941 原型 UI 运行时冒烟测试');
console.log('═'.repeat(70));

const stage = [];
function step(label, fn) {
  try { fn(); stage.push('✔ ' + label); }
  catch (e) { failures++; checks++; detail.push(`${label} 抛出异常：${e && e.message}`); stage.push('✖ ' + label + ' → ' + (e && e.message)); }
}

step('加载 data.generated.js', () => load('prototype/js/data.generated.js'));
step('加载 game.js', () => load('prototype/js/game.js'));
step('加载 render.js', () => load('prototype/js/render.js'));
step('加载 ui.js（脚本加载期空引用会在此暴露）', () => load('prototype/js/ui.js'));

const RF = sandbox.RF, D = sandbox.window.RF_DATA, STATE = sandbox.RF_STATE;
ok(!!RF && !!D, '数据/引擎未挂载到 window');
ok(!!STATE && STATE.screen === 'menu', 'ui.js 未完成初始化（屏幕应为 menu）');

step('主菜单 → 关卡列表', () => {
  clickDelegate(makeElement(null).__proto__ && (() => { const e = makeElement('x'); e.setAttribute('data-go', 'levels'); return e; })());
  ok(STATE.screen === 'levels', '未进入关卡列表');
});

step('切换到第二个战役标签', () => {
  const tabs = documentStub.getElementById('campaignTabs').children.filter((c) => c.getAttribute('data-camp'));
  ok(tabs.length >= 5, `战役标签数量异常：${tabs.length}`);
  clickEl(tabs[1]);
  ok(STATE.campaign === tabs[1].getAttribute('data-camp'), '战役筛选未生效');
});

step('点击一个关卡卡片', () => {
  clickEl(documentStub.getElementById('campaignTabs').children[0]); // 回到全部
  const cards = documentStub.getElementById('levelList').children.filter((c) => c.getAttribute('data-level'));
  ok(cards.length === D.levels.length, `关卡卡片数量应等于关卡数（${cards.length} vs ${D.levels.length}）`);
  clickEl(cards[0]);
  ok(STATE.level && STATE.level.index === 1, '未选中第 1 关');
  ok(STATE.screen === 'briefing', '未进入简报界面');
});

step('简报 → 装备界面', () => {
  const go = documentStub.getElementById('briefGo');
  ok(typeof go.onclick === 'function', '简报页缺少「进入装备」按钮绑定');
  go.onclick();
  ok(STATE.screen === 'loadout', '未进入装备界面');
  ok(STATE.loadout && STATE.loadout.primary, '默认配装未生成主武器');
});

step('装备界面交互（分栏/物品增减/推荐配置/难度）', () => {
  const tabs = documentStub.getElementById('loadoutTabs').children.filter((c) => c.getAttribute('data-tab'));
  ok(tabs.length === 7, `装备分栏数量异常：${tabs.length}`);
  for (const t of tabs) clickEl(t);           // 遍历所有分栏，确保渲染不崩
  const classBtns = documentStub.getElementById('classRow').children.filter((c) => c.getAttribute('data-cls'));
  ok(classBtns.length === 9, `专长按钮数量应 9，实际 ${classBtns.length}`);
  for (const b of classBtns) if (!b.classList.contains('disabled')) clickEl(b);
  const diffs = documentStub.getElementById('difficultyPicker').children.filter((c) => c.getAttribute('data-diff'));
  ok(diffs.length === 4, `难度按钮数量应 4，实际 ${diffs.length}`);
  clickEl(diffs[3]);                          // 切到史实档
  ok(STATE.difficulty === 'historical', '难度切换未生效');
  clickEl(diffs[1]);
  documentStub.getElementById('autoBtn').onclick();
  ok(STATE.loadout && STATE.loadout.primary, '推荐配置后丢失主武器');
  const total = RF.computeLoadout(STATE.loadout, D).totalKg;
  ok(total > 5 && total < 60, `推荐配置负重异常：${total.toFixed(1)} kg`);
});

step('部署 → 进入游戏并渲染 120 帧', () => {
  documentStub.getElementById('deployBtn').onclick();
  ok(STATE.screen === 'game', '未进入游戏界面');
  ok(!!STATE.game, '未创建 Game 实例');
  tick(120);
  const g = STATE.game;
  ok(g.time > 1.5, `游戏时间未推进（${g.time.toFixed(2)}s）`);
  ok(g.enemies.length + g.vehicles.length > 0, '关卡未生成敌军');
  ok(documentStub.getElementById('hpVal').textContent !== '', 'HUD 生命值未更新');
  ok(String(documentStub.getElementById('wpnName').textContent).length > 0, 'HUD 武器名未更新');
  ok(documentStub.getElementById('objectives').children.length > 0, 'HUD 目标列表未渲染');
  const supHtml = String(documentStub.getElementById('supportPanel').innerHTML);
  ok(supHtml.length > 0, '支援面板完全为空（既无可用支援也无提示文案）');
  const supCount = documentStub.getElementById('supportPanel').children.filter((c) => c.getAttribute('data-sup')).length;
  ok(supCount === (STATE.level.available_support || []).length, `支援面板条目数 ${supCount} 与关卡授权数 ${(STATE.level.available_support || []).length} 不一致`);
});

step('键盘输入（移动/射击/换弹/进食/饮水/医疗/投弹/支援/暂停）', () => {
  const kd = (code) => fireWindow('keydown', { code, preventDefault() {} });
  for (const c of ['KeyW', 'KeyD', 'ShiftLeft', 'KeyR', 'KeyF', 'KeyE', 'KeyH', 'KeyG', 'KeyB', 'Digit2', 'Digit1', 'KeyQ', 'KeyT', 'KeyV', 'Space']) kd(c);
  tick(60);
  const g = STATE.game;
  ok(g.time > 2.5, '输入后时间未推进');
  fireWindow('keyup', { code: 'KeyG' });
  kd('Escape');
  ok(STATE.paused === true, 'Escape 未暂停');
  documentStub.getElementById('resumeBtn').onclick();
  ok(STATE.paused === false, '继续按钮未恢复');
  tick(30);
});

step('瞄准与射击事件（鼠标移动 + 按下/抬起 + 右键取消支援）', () => {
  const canvas = documentStub.getElementById('canvas');
  canvas.dispatch('mousemove', { clientX: 700, clientY: 400 });
  canvas.dispatch('mousedown', { button: 0 });
  tick(90);
  canvas.dispatch('mouseup', {});
  canvas.dispatch('mousedown', { button: 2 });
  canvas.dispatch('contextmenu', {});
  canvas.dispatch('touchstart', { touches: [{ clientX: 500, clientY: 300 }] });
  canvas.dispatch('touchmove', { touches: [{ clientX: 520, clientY: 320 }] });
  canvas.dispatch('touchend', {});
  tick(30);
  ok(true, '');
});

step('窗口缩放 / 失焦', () => {
  fireWindow('resize', {});
  fireWindow('blur', {});
  tick(10);
  ok(true, '');
});

step('强制结束关卡 → 结算界面', () => {
  const g = STATE.game;
  g.player.hp = 0;
  g.playerDied();
  tick(5);
  // 结算由 setTimeout 触发（本桩同步执行）
  ok(STATE.screen === 'result', `未进入结算界面（当前 ${STATE.screen}）`);
  ok(documentStub.getElementById('resultStats').children.length > 0, '结算统计未渲染');
  ok(String(documentStub.getElementById('resultTitle').textContent).length > 0, '结算标题未设置');
});

step('重打本关', () => {
  documentStub.getElementById('retryBtn').onclick();
  ok(STATE.screen === 'game', '重打未进入游戏');
  tick(30);
  ok(STATE.game.time >= 0, '重打后游戏未运行');
});

step('返回关卡列表 → 无限模式设置', () => {
  fireWindow('keydown', { code: 'Escape', preventDefault() {} });
  documentStub.getElementById('toLevelsBtn').onclick();
  ok(STATE.screen === 'levels', '未返回关卡列表');
  clickDelegate((() => { const e = makeElement('x'); e.setAttribute('data-go', 'endless'); return e; })());
  ok(STATE.screen === 'loadout' && STATE.mode === 'endless', '未进入无限模式配装');
  ok(STATE.loadout.capacityKg === D.classes.base_capacity_kg, '无限模式苏军容量应为 24kg');
});

step('无限模式部署并运行 200 帧', () => {
  documentStub.getElementById('deployBtn').onclick();
  ok(STATE.screen === 'game', '无限模式未进入游戏');
  tick(200);
  const g = STATE.game;
  ok(g.mode === 'endless', '模式不是 endless');
  ok(g.time > 3, '无限模式时间未推进');
  ok(g.enemies.length > 0, '无限模式未生成敌军');
});

step('暂停 / 重新开始 / 返回菜单', () => {
  fireWindow('keydown', { code: 'Escape', preventDefault() {} });
  documentStub.getElementById('restartBtn').onclick();
  tick(20);
  ok(STATE.screen === 'game', '重新开始失败');
  documentStub.getElementById('toLevelsBtn').onclick();
  clickDelegate((() => { const e = makeElement('x'); e.setAttribute('data-go', 'about'); return e; })());
  ok(STATE.screen === 'about', '未进入项目说明页');
  ok(String(documentStub.getElementById('campaignSummary').innerHTML).includes('巴巴罗萨'), '项目说明未列出战役');
});

/* --------------------------------- 输出 --------------------------------- */
console.log(stage.join('\n'));
console.log('\n' + '═'.repeat(70));
if (failures) {
  console.log(`✖ UI 冒烟测试失败：${failures}/${checks} 项未通过`);
  detail.forEach((d) => console.log('  · ' + d));
  process.exit(1);
}
console.log(`✔ UI 冒烟测试通过：${checks} 项断言全部通过`);
console.log('  流程：主菜单 → 关卡列表 → 战役切换 → 选关 → 简报 → 装备 → 部署 → 输入 → 结算 → 重打 → 无限模式');
console.log('═'.repeat(70));
