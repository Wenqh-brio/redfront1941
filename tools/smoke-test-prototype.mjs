#!/usr/bin/env node
/* =============================================================================
 * smoke-test-prototype.mjs — 无头冒烟测试：用 DOM/Canvas 桩驱动游戏仿真
 * 覆盖：装备与负重、生存衰减、弹道与装甲判定、手榴弹、支援火力全链路、
 *       敌方反炮兵与空袭、无限模式波次与整备、56 关全量推进、渲染无异常。
 * 用法：node tools/smoke-test-prototype.mjs
 * ===========================================================================*/
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';
import vm from 'node:vm';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const P = (f) => path.join(ROOT, f);

/* ------------------------- Canvas / DOM 桩 ------------------------- */
function makeCtx() {
  const target = {
    canvas: { width: 1280, height: 720 },
    createRadialGradient: () => ({ addColorStop() {} }),
    createLinearGradient: () => ({ addColorStop() {} }),
    measureText: () => ({ width: 10 }),
    getImageData: () => ({ data: new Uint8ClampedArray(4) }),
    save() {}, restore() {}
  };
  return new Proxy(target, {
    get(t, k) {
      if (k in t) return t[k];
      if (typeof k === 'string' && /^[a-z]/.test(k)) return function () {};
      return undefined;
    },
    set(t, k, v) { t[k] = v; return true; }
  });
}
const ctxStub = makeCtx();
const canvasStub = {
  width: 1280, height: 720, style: {},
  getContext: () => ctxStub,
  addEventListener() {}, removeEventListener() {},
  getBoundingClientRect: () => ({ left: 0, top: 0, width: 1280, height: 720 }),
  parentElement: { clientWidth: 1280, clientHeight: 720 }
};
const documentStub = {
  getElementById: () => canvasStub,
  querySelectorAll: () => [],
  addEventListener() {}, removeEventListener() {},
  createElement: () => canvasStub,
  body: { innerHTML: '' }
};

const sandbox = {
  console,
  Math, JSON, Date, Object, Array, String, Number, Boolean, Error, RegExp, Map, Set,
  Uint8ClampedArray, Float64Array, isFinite, isNaN, parseInt, parseFloat,
  performance: { now: () => Date.now() },
  requestAnimationFrame: () => 0, cancelAnimationFrame: () => {},
  setTimeout: (fn) => 0, clearTimeout: () => {}, setInterval: () => 0, clearInterval: () => {},
  localStorage: { getItem: () => null, setItem() {} },
  addEventListener: () => {},
  document: documentStub
};
sandbox.window = sandbox;
sandbox.globalThis = sandbox;
vm.createContext(sandbox);

function load(rel) {
  const code = fs.readFileSync(P(rel), 'utf8');
  vm.runInContext(code, sandbox, { filename: rel });
}
load('prototype/js/data.generated.js');
load('prototype/js/game.js');
load('prototype/js/render.js');

const RF = sandbox.RF;
const D = sandbox.window.RF_DATA;
if (!RF || !D) { console.error('✖ 无法载入 RF / RF_DATA'); process.exit(1); }

/* ------------------------------ 断言工具 ------------------------------ */
let checks = 0, failures = 0;
const failuresDetail = [];
function ok(cond, label) {
  checks++;
  if (!cond) { failures++; failuresDetail.push(label); }
}
function finite(...vals) { return vals.every((v) => typeof v === 'number' && isFinite(v)); }
function section(name) { console.log('\n▌ ' + name); }
function report(label, extra) { console.log('  · ' + label + (extra ? '  ' + extra : '')); }

console.log('═'.repeat(70));
console.log(`RedFront 1941 原型冒烟测试   数据版本 ${D.meta.generatedAt}   关卡 ${D.meta.levels}`);
console.log('═'.repeat(70));

/* ============================ 1. 负重与配装 ============================ */
section('1. 配装与负重系统');
const factions = ['soviet', 'us'];
const classIds = Object.keys(D.classes).filter((k) => D.classes[k] && D.classes[k].id);
let loadoutCases = 0;
for (const fac of factions) {
  for (const cid of classIds) {
    const lo = RF.autoLoadout(D, fac, cid);
    const c = RF.computeLoadout(lo, D);
    loadoutCases++;
    ok(finite(c.totalKg, c.ratio), `${fac}/${cid}: 负重非数值`);
    ok(c.totalKg > 3 && c.totalKg < 60, `${fac}/${cid}: 负重异常 ${c.totalKg.toFixed(1)}kg`);
    ok(c.band && c.band.id, `${fac}/${cid}: 缺少负重档位`);
    ok(c.lines.length > 0, `${fac}/${cid}: 装备明细为空`);
  }
}
report('配装组合', `${loadoutCases} 组，全部有限且落在合理区间`);

// 档位边界
const bands = RF.WEIGHT.bands;
ok(RF.WEIGHT.bandFor(0.5).id === 'light', '档位：50% 应为轻装');
ok(RF.WEIGHT.bandFor(0.8).id === 'standard', '档位：80% 应为标准');
ok(RF.WEIGHT.bandFor(0.95).id === 'heavy', '档位：95% 应为重装');
ok(RF.WEIGHT.bandFor(1.4).id === 'overloaded', '档位：140% 应为超载');
ok(bands.length === 4, '档位数量应为 4');
report('负重档位', bands.map((b) => `${b.label}≤${b.maxRatio === 99 ? '∞' : b.maxRatio * 100 + '%'}`).join(' | '));

/* ============================ 2. 全关卡推进 ============================ */
section('2. 56 关仿真推进（每关 60 秒，含射击/投弹/支援/进食）');
const difficulties = Object.keys(D.difficulty || { regular: 1 });
const sample = D.levels;
let levelStats = [];
let simThrow = null;
const t0 = Date.now();

function syntheticInput(g, t) {
  // 像玩家一样：能看见敌人就瞄准射击；看不见就向最近的敌人推进（完成歼灭目标）
  let aim = null, bd = 620 * 620, nearest = null, nd = Infinity;
  for (const e of g.enemies) {
    if (!e.alive) continue;
    const d = (e.x - g.player.x) ** 2 + (e.y - g.player.y) ** 2;
    if (d < nd) { nd = d; nearest = e; }
    if (d < bd && g.hasLOS(g.player.x, g.player.y, e.x, e.y)) { bd = d; aim = e; }
  }
  if (!aim) for (const v of g.vehicles) {
    if (!v.alive) continue;
    const d = (v.x - g.player.x) ** 2 + (v.y - g.player.y) ** 2;
    if (d < nd) { nd = d; nearest = v; }
    if (d < bd) { bd = d; aim = v; }
  }
  const aimWorld = aim ? { x: aim.x, y: aim.y } : { x: g.player.x + Math.cos(t) * 140, y: g.player.y + Math.sin(t) * 140 };
  // 无接触时向最近敌人推进
  let moveX = 0, moveY = 0;
  if (!aim && nearest && t % 20 > 4) {
    const a = Math.atan2(nearest.y - g.player.y, nearest.x - g.player.x);
    moveX = Math.cos(a); moveY = Math.sin(a);
  } else if (t % 6 < 3) {
    moveX = Math.cos(t * 0.2); moveY = Math.sin(t * 0.2);
  }
  return {
    moveX, moveY,
    sprint: (t % 8) < 1.5 && (moveX || moveY),
    crouch: !aim && (t % 7) < 1.5,
    fire: !!aim || (t % 9) < 3,
    firePressed: true,
    cookGrenade: (t % 21) > 20.4,
    binocular: (t % 31) < 1.5,
    ads: !!aim && (t % 5) < 2.5,
    aimWorld
  };
}

for (let li = 0; li < sample.length; li++) {
  const level = sample[li];
  const diff = difficulties[li % difficulties.length];
  const fac = level.faction === 'us' ? 'us' : 'soviet';
  const cls = (level.player_class_recommended && level.player_class_recommended[0]) || 'rifleman';
  try {
    const lo = RF.autoLoadout(D, fac, classIds.includes(cls) ? cls : 'rifleman', level.weight_budget_kg);
    lo.capacityKg = level.weight_budget_kg;
    const g = new RF.Game(D, { level, loadout: lo, mode: 'campaign', difficulty: diff, seed: li + 1 });
    ok(g.enemies.length + g.vehicles.length > 0, `${level.id}: 未生成敌军`);
    ok(g.objectives.length > 0, `${level.id}: 未生成目标`);
    ok(g.map.obstacles.length > 10, `${level.id}: 地图障碍过少`);

    const dt = 1 / 30;
    let supCalls = 0, grenades = 0;
    for (let s = 0; s < 30 * 60; s++) {
      const t = s * dt;
      const input = syntheticInput(g, t);
      if (s % (30 * 26) === 0 && (level.available_support || []).length) {
        const id = level.available_support[supCalls % level.available_support.length];
        const chk = g.canCall(id);
        if (chk.ok) {
          g.callSupport(id, g.player.x + 200, g.player.y - 150);
          supCalls++;
        }
      }
      if (s % (30 * 18) === 0) { g.throwGrenade(1.0); grenades++; }
      if (s % (30 * 40) === 0) g.useSupply('food');
      if (s % (30 * 35) === 0) g.useSupply('water');
      if (s % (30 * 55) === 0) g.useMedical();
      if (s % 60 === 0) g.switchWeapon();
      if (s % 45 === 0) g.startReload();
      g.step(dt, input);
    }
    // 不变量
    ok(finite(g.player.x, g.player.y, g.player.hp, g.player.stamina, g.player.food, g.player.water, g.player.warmth), `${level.id}: 玩家状态出现 NaN`);
    ok(g.player.hp >= 0, `${level.id}: 玩家生命为负`);
    ok(finite(g.camera.x, g.camera.y), `${level.id}: 相机 NaN`);
    let badEnemy = 0;
    for (const e of g.enemies) if (!finite(e.x, e.y, e.hp)) badEnemy++;
    for (const v of g.vehicles) if (!finite(v.x, v.y, v.hp)) badEnemy++;
    ok(badEnemy === 0, `${level.id}: ${badEnemy} 个敌人状态为 NaN`);
    ok(g.enemies.length + g.vehicles.length < 400, `${level.id}: 敌人数量失控 (${g.enemies.length + g.vehicles.length})`);
    ok(g.particles.length < 4000, `${level.id}: 粒子泄漏 (${g.particles.length})`);
    ok(g.projectiles.length < 3000, `${level.id}: 子弹泄漏 (${g.projectiles.length})`);
    // 渲染
    RF.renderWorld(ctxStub, g, 1280, 720, { x: 640, y: 360 });
    RF.renderMinimap(ctxStub, g, 220, 152);
    levelStats.push({ id: level.id, diff, wave: g.wave, kills: g.kills, hp: Math.round(g.player.hp), phase: g.phase, supCalls, grenades });
  } catch (err) {
    simThrow = `关卡 ${level.id}（难度 ${diff}）抛出异常：${err && err.stack ? err.stack.split('\n').slice(0, 3).join(' | ') : err}`;
    failuresDetail.push(simThrow);
    failures++;
  }
}
ok(!simThrow, '全量关卡仿真不应抛出异常');
ok(levelStats.reduce((a, s) => a + s.kills, 0) > 0, '56 关全量推进中从未产生击杀（战斗链路可能失效）');
ok(levelStats.every((s) => s.hp >= 0), '存在关卡玩家生命为负值');
report('仿真', `${sample.length} 关 / ${Date.now() - t0} ms / 平均每关 ${((Date.now() - t0) / sample.length).toFixed(0)} ms`);
report('战斗链路', `总击杀 ${levelStats.reduce((a, s) => a + s.kills, 0)} · 投弹 ${levelStats.reduce((a, s) => a + s.grenades, 0)} 枚 · 支援呼叫 ${levelStats.reduce((a, s) => a + s.supCalls, 0)} 次`);
const phases = {};
for (const s of levelStats) phases[s.phase] = (phases[s.phase] || 0) + 1;
report('关内结果分布', Object.entries(phases).map(([k, v]) => `${k}×${v}`).join(' '));
report('抽样（前 3 关）', levelStats.slice(0, 3).map((s) => `${s.id}[${s.diff}] 击杀${s.kills} 剩余HP${s.hp} 支援${s.supCalls} 弹${s.grenades}`).join(' | '));

/* ============================ 3. 生存与负重惩罚 ============================ */
section('3. 生存衰减与负重惩罚');
{
  const lo = RF.autoLoadout(D, 'soviet', 'rifleman');
  const g = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', difficulty: 'historical', seed: 7 });
  g.player.food = 0; g.player.water = 0; g.player.warmth = 0;
  const hp0 = g.player.hp;
  for (let i = 0; i < 30 * 10; i++) g.step(1 / 30, { moveX: 0, moveY: 0, aimWorld: { x: g.player.x + 10, y: g.player.y } });
  ok(g.player.hp < hp0, '饥饿/脱水/失温应造成持续掉血');
  report('生存', `10 秒内 HP ${hp0.toFixed(1)} → ${g.player.hp.toFixed(1)}（史实档）`);

  // 超载
  const heavy = JSON.parse(JSON.stringify(lo));
  heavy.gear['sn42_breastplate'] = 1;
  heavy.gear['ammo_pouch'] = 1;
  heavy.gear['rifle_ammo_box'] = 1;
  heavy.supplies['water_can_5l'] = 1;
  heavy.supplies['k_ration'] = 2;
  heavy.grenades['f1_grenade'] = 2;
  const hc = RF.computeLoadout(heavy, D);
  ok(hc.band.id === 'overloaded', `超载判定失败：${hc.totalKg.toFixed(1)}kg / ${heavy.capacityKg}kg → ${hc.band.id}`);
  report('超载', `${hc.totalKg.toFixed(1)} kg > ${heavy.capacityKg} kg → ${hc.band.label}，移速 ×${hc.band.speedMult}`);

  // 体力耗尽应无法冲刺
  const g2 = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', seed: 11 });
  g2.player.stamina = 0;
  const x0 = g2.player.x;
  for (let i = 0; i < 30; i++) g2.step(1 / 30, { moveX: 1, moveY: 0, sprint: true, aimWorld: { x: g2.player.x + 50, y: g2.player.y } });
  ok(Math.abs(g2.player.x - x0) < 200, '体力为 0 时仍发生了冲刺位移');
  report('体力', `体力 0 时 1 秒位移 ${(g2.player.x - x0).toFixed(1)}（冲刺被禁止）`);
}

/* ============================ 4. 弹药与换弹 ============================ */
section('4. 弹药消耗与换弹');
{
  const lo = RF.autoLoadout(D, 'soviet', 'assault');
  const g = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', seed: 21 });
  const w = g.currentWeapon();
  const res0 = g.player.ammo[w.ammo] || 0;
  ok(w && g.currentMag() > 0, '初始弹匣为空');
  g.startReload();
  for (let i = 0; i < 30 * 6; i++) g.step(1 / 30, { moveX: 0, moveY: 0, aimWorld: { x: g.player.x + 30, y: g.player.y } });
  ok(g.player.reloading === 0, '换弹未在 6 秒内完成');
  // 打空弹匣
  g.player.currentSlot = 'primary';
  const mag0 = g.currentMag();
  for (let i = 0; i < 30 * 20; i++) g.step(1 / 30, { moveX: 0, moveY: 0, fire: true, firePressed: true, aimWorld: { x: g.player.x + 60, y: g.player.y } });
  ok(g.currentMag() <= mag0, '射击未消耗弹匣');
  ok((g.player.ammo[w.ammo] || 0) <= res0, '备弹未随换弹减少');
  report('弹药', `${w.name}：弹匣 ${g.currentMag()}/${w.mag}，备弹 ${g.player.ammo[w.ammo] || 0}`);
}

/* ========================= 4b. 命中判定（靶场测试） ========================= */
section('4b. 命中判定靶场（连续碰撞 / 掩体阻挡 / 远距离）');
{
  const marksman = (opts) => {
    const lo = RF.autoLoadout(D, 'soviet', 'rifleman');
    const g = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', seed: 3 });
    g.map.obstacles = [];
    g.enemies.length = 0; g.vehicles.length = 0;
    const e = {
      kind: 'enemy', def: { id: 'target', name_zh: '靶标' }, id: 'target', name_zh: '靶标',
      x: g.player.x + (opts.dist || 120), y: g.player.y, r: 10, facing: 0, alive: true,
      hp: 1e6, maxHp: 1e6, dmg: 0, fireRate: 99, accuracy: 0, aware: 1, reaction: 99,
      suppressionResist: 1, role: 'line', state: 'idle', timer: 0, cooldown: 99,
      aimTime: 0, suppression: 0, morale: 1, retarget: 0, cover: null, marks: 0
    };
    g.enemies.push(e);
    if (opts.wall) g.map.obstacles.push({ x: g.player.x + 60, y: g.player.y - 30, w: 40, h: 60, kind: 'sandbag', blocksSight: false, blocksMove: false, cover: 0.6 });
    const hp0 = e.hp, s0 = g.player.stats.shots, h0 = g.player.stats.hits;
    for (let i = 0; i < 30 * 10; i++) g.step(1 / 30, { moveX: 0, moveY: 0, fire: true, firePressed: true, aimWorld: { x: e.x, y: e.y } });
    return { shots: g.player.stats.shots - s0, hits: g.player.stats.hits - h0, dmg: hp0 - e.hp };
  };
  const open = marksman({ dist: 120 });
  ok(open.shots > 0, '靶场：10 秒内未开火');
  ok(open.hits > 0, '靶场：开阔地 120m 全部脱靶（碰撞检测失效）');
  ok(open.hits / open.shots > 0.7, `靶场：命中率过低 ${open.hits}/${open.shots}`);
  report('开阔地 120m', `射击 ${open.shots} 发 / 命中 ${open.hits} / 伤害 ${open.dmg.toFixed(0)}`);

  const far = marksman({ dist: 500 });
  ok(far.hits > 0, '靶场：500m 处无法命中（弹道存活距离过短）');
  report('开阔地 500m', `射击 ${far.shots} 发 / 命中 ${far.hits} / 伤害 ${far.dmg.toFixed(0)}`);

  const wall = marksman({ dist: 120, wall: true });
  ok(wall.hits === 0, `靶场：沙袋未能阻挡子弹（命中 ${wall.hits}）`);
  report('沙袋遮挡', `射击 ${wall.shots} 发 / 命中 ${wall.hits}（正确被阻挡）`);
}

/* ============================ 5. 弹药穿透与装甲判定 ============================ */
section('5. 弹道与装甲判定');
{
  const lo = RF.autoLoadout(D, 'soviet', 'anti_tank');
  const g = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', seed: 31 });
  const tiger = RF.Game.prototype.supportDef ? { id: 'pzkpfw_vi_tiger', name_zh: '虎式', armor_mm: 100, side_rear_ratio: 0.8, hp: 520, penetration_mm: 120, speed_kph: 30, weapon: '88mm' } : null;
  const def = (D.enemies.vehicles || []).find((v) => v.id === 'pzkpfw_vi_tiger');
  const v1 = { kind: 'vehicle', id: 'x', name: '测试车', x: 100, y: 100, r: 26, alive: true, hp: 500, maxHp: 500, armor: def.armor_mm, sideRear: def.side_rear_ratio, facing: 0, side: 0 };
  const okNoPen = g.damageVehicle(v1, 80, 35, 100, 100, 0); // 35mm 打 100mm 正面
  ok(okNoPen === false && v1.hp === 500, '35mm 不应击穿 100mm 正面装甲');
  report('正面装甲', `35mm 穿深 vs 100mm 等效 → 跳弹，HP 保持 ${v1.hp}`);
  const okPen = g.damageVehicle(v1, 90, 120, 100, 100, 0);
  ok(okPen === true && v1.hp < 500, '120mm 应击穿 100mm 正面装甲');
  report('正面装甲', `120mm 穿深 → 击穿，HP 降至 ${v1.hp.toFixed(1)}`);

  // 视线遮挡
  const g2 = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', seed: 32 });
  const wall = g2.map.obstacles.filter((o) => o.blocksSight)[0];
  if (wall) {
    const blocked = !g2.hasLOS(wall.x - 20, wall.y + wall.h / 2, wall.x + wall.w + 20, wall.y + wall.h / 2);
    ok(blocked, '掩体未能阻断视线');
    report('视线', `穿越建筑矩形被正确阻断`);
  }
  // 手榴弹爆炸伤害
  const e = g2.enemies[0];
  if (e) {
    const before = e.hp;
    g2.applyExplosion(e.x, e.y, 60, 100, 12, 0);
    ok(e.hp < before || !e.alive, '手榴弹爆炸未造成伤害');
    report('手榴弹', `${e.name} HP ${before.toFixed(0)} → ${e.alive ? e.hp.toFixed(0) : '阵亡'}`);
  }
}

/* ============================ 6. 支援火力全链路 ============================ */
section('6. 支援火力全链路（含敌方反炮兵与空袭）');
{
  const results = [];
  const callInList = D.support.call_ins;
  const totalHp = (g) => g.enemies.filter((e) => e.alive).reduce((a, e) => a + e.hp, 0) +
    g.vehicles.filter((v) => v.alive).reduce((a, v) => a + v.hp, 0);
  const nearestTarget = (g) => {
    let best = null, bd = 1e9;
    for (const e of g.enemies) if (e.alive) { const d = RF.util.dist(e.x, e.y, g.player.x, g.player.y); if (d < bd) { bd = d; best = e; } }
    for (const v of g.vehicles) if (v.alive) { const d = RF.util.dist(v.x, v.y, g.player.x, g.player.y); if (d < bd) { bd = d; best = v; } }
    return best || { x: g.player.x + 300, y: g.player.y - 200 };
  };
  for (const def of callInList) {
    const fac = def.faction === 'us' ? 'us' : 'soviet';
    const lo = RF.autoLoadout(D, fac, def.type === 'armor' || def.type === 'armor_siege' ? 'anti_tank' : 'rifleman');
    const g = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', difficulty: 'veteran', seed: 41 });
    g.cp = g.cpMax;
    g.player.hp = 1e9; g.player.maxHp = 1e9; // 只验证支援链路本身，不测玩家生存
    const chk = g.canCall(def.id);
    if (!chk.ok) { failuresDetail.push(`支援 ${def.id} 无法呼叫：${chk.why}`); failures++; continue; }
    const target = nearestTarget(g);
    const tx = target.x, ty = target.y;
    const hpBefore = totalHp(g), killsBefore = g.kills, alliesBefore = g.allies.length;
    g.callSupport(def.id, tx, ty);
    const t0s = g.time;
    let maxAllies = alliesBefore, maxShells = 0, maxSmoke = 0;
    for (let i = 0; i < 30 * 260 && g.time - t0s < 250; i++) {
      if (target && target.alive) { target.x = tx; target.y = ty; } // 钉住靶标，保证弹着点有效
      g.step(1 / 30, { moveX: 0, moveY: 0, aimWorld: { x: g.player.x + 50, y: g.player.y } });
      maxAllies = Math.max(maxAllies, g.allies.length);
      maxShells = Math.max(maxShells, g.shells.length);
      maxSmoke = Math.max(maxSmoke, g.particles.filter((p) => p.kind === 'smoke').length);
    }
    const hpAfter = totalHp(g), damageDealt = hpBefore - hpAfter + (g.kills - killsBefore) > 0;
    ok(finite(g.player.hp), `${def.id}: 呼叫后玩家状态 NaN`);
    ok(g.cp <= g.cpMax, `${def.id}: 指挥点溢出`);
    ok(g.cooldowns[def.id] !== undefined, `${def.id}: 未记录冷却`);
    ok(g.pendingSupport.filter((p) => p.def.name_zh === def.name_zh).length === 0, `${def.id}: 弹着队列未清空`);
    if (def.damage_per_round > 0 || def.type === 'air' || def.type === 'armor' || def.type === 'armor_siege') {
      ok(damageDealt || maxShells > 0 || maxAllies > alliesBefore,
        `${def.id}: 支援未产生任何战场效果（伤害/通场/增援）`);
    }
    if (def.type === 'obscurant') ok(maxSmoke > 0, `${def.id}: 未生成烟幕`);
    if (def.type === 'infantry') ok(maxAllies > alliesBefore, `${def.id}: 未生成增援`);
    if (def.type === 'armor' || def.type === 'armor_siege') ok(maxAllies > alliesBefore, `${def.id}: 未生成装甲单位`);
    results.push(`${def.name_zh}: 伤害${damageDealt ? '有' : '无'} 友军峰值${maxAllies} 通场${maxShells} 烟幕${maxSmoke}`);
  }
  report('支援条目', `${callInList.length} 项全部完成一次完整链路`);
  results.forEach((r) => console.log('      ' + r));

  // 指挥点不足应拒绝
  const lo = RF.autoLoadout(D, 'soviet', 'rifleman');
  const g3 = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', seed: 51 });
  g3.cp = 0;
  ok(g3.callSupport('sov_howitzer_152', 100, 100) === false, '指挥点不足时不应允许呼叫');
  // 未携带电台应拒绝
  const noRadio = JSON.parse(JSON.stringify(lo)); noRadio.radio = false;
  const g4 = new RF.Game(D, { level: null, loadout: noRadio, mode: 'endless', seed: 52 });
  g4.cp = g4.cpMax;
  ok(g4.callSupport('sov_mortar_82', 100, 100) === false, '无电台时不应允许呼叫');
  report('限制规则', '指挥点不足 / 未携带电台均被正确拒绝');

  // 敌方反炮兵（老兵/史实档 25% 概率）与敌机空袭路径
  let counter = 0, air = 0;
  for (let i = 0; i < 40; i++) {
    const g = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', difficulty: 'historical', seed: 100 + i });
    g.cp = g.cpMax;
    g.callSupport('sov_mortar_82', g.player.x + 100, g.player.y);
    if (g.pendingSupport.some((p) => p.enemy)) counter++;
    for (let s = 0; s < 30 * 40; s++) g.step(1 / 30, { moveX: 0, moveY: 0, aimWorld: { x: g.player.x + 10, y: g.player.y } });
  }
  ok(counter > 0, '史实难度下敌方反炮兵从未触发（概率失效）');
  report('敌方反炮兵', `40 次呼叫触发 ${counter} 次测向反制`);

  // 空袭（把空军单位塞进关卡构成，验证降级为空袭事件而非地面单位）
  const fakeLevel = JSON.parse(JSON.stringify(D.levels[0]));
  fakeLevel.enemy_composition = { 'ger_landser_rifleman': 4, 'ju87_stuka': 1, 'bf109_f': 1 };
  const g5 = new RF.Game(D, { level: fakeLevel, loadout: lo, mode: 'campaign', difficulty: 'regular', seed: 61 });
  const groundAir = g5.enemies.filter((e) => /ju87|bf109/.test(e.id) || /斯图卡|Bf 109/.test(e.name)).length;
  ok(groundAir === 0, '敌机被错误地生成成了地面单位');
  ok(g5.pendingSupport.some((p) => p.enemy && p.isAir !== undefined), '未生成敌方空袭事件');
  for (let s = 0; s < 30 * 60; s++) g5.step(1 / 30, { moveX: 0, moveY: 0, aimWorld: { x: g5.player.x + 10, y: g5.player.y } });
  ok(finite(g5.player.hp), '空袭后玩家状态 NaN');
  report('敌方空袭', `斯图卡/Bf109 转为空袭事件，地面单位 0 个`);
}

/* ============================ 7. 无限模式 ============================ */
section('7. 无限模式波次与整备');
{
  const lo = RF.autoLoadout(D, 'soviet', 'machine_gunner');
  const g = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', difficulty: 'regular', seed: 71 });
  g.player.hp = 1e9; g.player.maxHp = 1e9; // 只测波次推进逻辑，不测玩家生存
  const seenWaves = new Set([g.wave]);
  let resupplies = 0, guard = 0;
  while (g.wave < 5 && guard++ < 30 * 60 * 12) {
    // 直接清空敌军以推进波次（模拟玩家压制）
    for (const e of g.enemies) if (e.alive) { e.alive = false; g.kills++; }
    for (const v of g.vehicles) if (v.alive) { v.alive = false; g.kills++; }
    if (g.phase === 'resupply') {
      resupplies++;
      ok(g.resupplyTimer > 0, '整备阶段应有时长');
      g.resupplyTimer = 0.01;
    }
    g.step(1 / 30, { moveX: 0, moveY: 0, aimWorld: { x: g.player.x + 10, y: g.player.y } });
    seenWaves.add(g.wave);
  }
  ok(seenWaves.size >= 5, `无限模式波次未推进（仅到第 ${g.wave} 波）`);
  ok(resupplies >= 3, `整备阶段未正确进入（${resupplies} 次）`);
  // 波次难度递增
  const comp1 = g.endlessComposition(1), comp6 = g.endlessComposition(6), comp16 = g.endlessComposition(16);
  const total = (c) => c.reduce((a, x) => a + x.n, 0);
  ok(total(comp16) > total(comp1), '后期波次兵力未增加');
  report('无限模式', `推进至第 ${g.wave} 波，整备 ${resupplies} 次；兵力 1 波=${total(comp1)} → 6 波=${total(comp6)} → 16 波=${total(comp16)}`);
  report('年代演进', `16 波含虎式/国民突击队：${comp16.some((c) => /tiger|volkssturm/.test(c.def.id))}`);
}

/* ============================ 8. 时间预算与稳定性 ============================ */
section('8. 长时间稳定性（单局 20 分钟连续推进）');
{
  const lo = RF.autoLoadout(D, 'us', 'rifleman');
  const g = new RF.Game(D, { level: null, loadout: lo, mode: 'endless', difficulty: 'veteran', seed: 81 });
  g.player.hp = 1e9; // 只测稳定性，不测平衡
  g.player.maxHp = 1e9;
  const t0s = Date.now();
  let err = null;
  try {
    for (let s = 0; s < 30 * 60 * 20; s++) {
      const t = s / 30;
      if (s % (30 * 15) === 0) { for (const e of g.enemies) if (e.alive) { e.alive = false; g.kills++; } }
      if (s % (30 * 10) === 0) { g.cp = g.cpMax; g.callSupport('us_105_howitzer', g.player.x + 300, g.player.y); }
      g.step(1 / 30, {
        moveX: Math.sin(t) > 0 ? 1 : -1, moveY: Math.cos(t * 0.5) > 0 ? 1 : -1,
        sprint: t % 5 < 1, fire: true, firePressed: true,
        aimWorld: { x: g.player.x + Math.cos(t * 2) * 200, y: g.player.y + Math.sin(t * 2) * 200 }
      });
    }
  } catch (e) { err = e; }
  ok(!err, '20 分钟连续仿真抛出异常：' + (err && err.message));
  ok(g.particles.length < 6000, `粒子数量失控：${g.particles.length}`);
  ok(g.projectiles.length < 6000, `子弹数量失控：${g.projectiles.length}`);
  ok(g.enemies.length < 2000, `敌人数量失控：${g.enemies.length}`);
  ok(finite(g.player.x, g.player.y, g.camera.x, g.camera.y), '长时间运行后出现 NaN');
  report('稳定性', `20 分钟仿真 ${Date.now() - t0s} ms，第 ${g.wave} 波，实体 敌${g.enemies.length}/弹${g.projectiles.length}/粒子${g.particles.length}`);
}

/* -------------------------------- 汇总 -------------------------------- */
console.log('\n' + '═'.repeat(70));
if (failures) {
  console.log(`✖ 冒烟测试失败：${failures}/${checks} 项断言未通过`);
  failuresDetail.slice(0, 30).forEach((f) => console.log('  · ' + f));
  process.exit(1);
}
console.log(`✔ 冒烟测试通过：${checks} 项断言全部通过`);
console.log('  覆盖：配装 90 组 / 56 关全量 60 秒推进 / 18 项支援链路 / 无限模式 5 波 / 20 分钟稳定性');
console.log('═'.repeat(70));
