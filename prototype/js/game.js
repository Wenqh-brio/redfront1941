/* =============================================================================
 *  RedFront 1941 — 核心机制原型（仿真层）
 *  ---------------------------------------------------------------------------
 *  职责：地图生成、实体、弹道、AI、支援火力、生存与负重、无限模式。
 *  不触碰 DOM：渲染只依赖传入的 2D context，UI 全部在 ui.js 中。
 *  这样 tools/smoke-test-prototype.mjs 可以在 Node 里无头驱动 step()。
 * ===========================================================================*/
(function (global) {
  'use strict';

  var RF = global.RF = global.RF || {};
  RF.VERSION = '1.0.0-prototype';

  /* ------------------------------ 工具 ------------------------------ */

  function mulberry32(a) {
    return function () {
      a |= 0; a = (a + 0x6D2B79F5) | 0;
      var t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
  function hashStr(s) {
    var h = 2166136261;
    for (var i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
    return h >>> 0;
  }
  function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
  function lerp(a, b, t) { return a + (b - a) * t; }
  function dist(ax, ay, bx, by) { var dx = ax - bx, dy = ay - by; return Math.sqrt(dx * dx + dy * dy); }
  function dist2(ax, ay, bx, by) { var dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; }
  function angleTo(ax, ay, bx, by) { return Math.atan2(by - ay, bx - ax); }
  function norm(a) { while (a > Math.PI) a -= Math.PI * 2; while (a < -Math.PI) a += Math.PI * 2; return a; }
  function round1(v) { return Math.round(v * 10) / 10; }

  RF.util = { mulberry32: mulberry32, hashStr: hashStr, clamp: clamp, lerp: lerp, dist: dist, norm: norm };

  /* -------------------- 线段 × 矩形 相交（视线/弹道） -------------------- */

  function segRect(ax, ay, bx, by, r) {
    // Liang-Barsky
    var dx = bx - ax, dy = by - ay;
    var t0 = 0, t1 = 1;
    var p = [-dx, dx, -dy, dy];
    var q = [ax - r.x, r.x + r.w - ax, ay - r.y, r.y + r.h - ay];
    for (var i = 0; i < 4; i++) {
      if (p[i] === 0) { if (q[i] < 0) return null; }
      else {
        var t = q[i] / p[i];
        if (p[i] < 0) { if (t > t1) return null; if (t > t0) t0 = t; }
        else { if (t < t0) return null; if (t < t1) t1 = t; }
      }
    }
    return { t: t0, x: ax + dx * t0, y: ay + dy * t0 };
  }
  function blocksLine(obstacles, ax, ay, bx, by) {
    for (var i = 0; i < obstacles.length; i++) {
      var o = obstacles[i];
      if (!o.blocksSight) continue;
      var hit = segRect(ax, ay, bx, by, o);
      if (hit && hit.t > 0.001 && hit.t < 0.999) return hit;
    }
    return null;
  }
  function circleRect(cx, cy, cr, r) {
    var nx = clamp(cx, r.x, r.x + r.w), ny = clamp(cy, r.y, r.y + r.h);
    return dist2(cx, cy, nx, ny) < cr * cr;
  }
  /* 线段 × 圆：用于高速弹丸的连续碰撞（避免每帧 46px 位移穿过 20px 直径的目标） */
  function segCircle(ax, ay, bx, by, cx, cy, cr) {
    var dx = bx - ax, dy = by - ay;
    var l2 = dx * dx + dy * dy;
    var t = l2 > 0 ? ((cx - ax) * dx + (cy - ay) * dy) / l2 : 0;
    t = clamp(t, 0, 1);
    var px = ax + dx * t, py = ay + dy * t;
    return dist2(px, py, cx, cy) < cr * cr;
  }

  /* --------------------------- 负重与生存规则 --------------------------- */

  RF.WEIGHT = {
    // 与 data/classes.json load_bands 一致
    bands: [
      { id: 'light', maxRatio: 0.60, speedMult: 1.08, staminaDrain: 0.80, label: '轻装' },
      { id: 'standard', maxRatio: 0.85, speedMult: 1.00, staminaDrain: 1.00, label: '标准' },
      { id: 'heavy', maxRatio: 1.00, speedMult: 0.92, staminaDrain: 1.25, label: '重装' },
      { id: 'overloaded', maxRatio: 99, speedMult: 0.78, staminaDrain: 1.70, label: '超载' }
    ],
    bandFor: function (ratio) {
      for (var i = 0; i < this.bands.length; i++) if (ratio <= this.bands[i].maxRatio) return this.bands[i];
      return this.bands[this.bands.length - 1];
    }
  };

  /* 装备清单 → 总重（水壶按满水计，喝掉后减重） */
  RF.computeLoadout = function (loadout, data) {
    var lines = [];
    var total = 0;
    function add(label, kg, note) {
      if (kg <= 0) return;
      total += kg;
      lines.push({ label: label, kg: kg, note: note || '' });
    }
    function weapon(id) {
      var w = data.weapons[id]; if (!w) return null;
      add(w.name_zh, w.weight_kg, w.caliber);
      return w;
    }
    var prim = loadout.primary ? weapon(loadout.primary) : null;
    var sec = loadout.secondary ? weapon(loadout.secondary) : null;

    Object.keys(loadout.grenades || {}).forEach(function (id) {
      var it = data.equipment[id];
      if (!it || !loadout.grenades[id]) return;
      add(it.name_zh + ' ×' + loadout.grenades[id], it.weight_kg * loadout.grenades[id], '手榴弹');
    });
    Object.keys(loadout.supplies || {}).forEach(function (id) {
      var it = data.equipment[id];
      if (!it || !loadout.supplies[id]) return;
      var kg = it.weight_kg * loadout.supplies[id];
      // 水：按满水计入，缺水后由运行时扣减
      add(it.name_zh + ' ×' + loadout.supplies[id], kg, it.category === 'water' ? '满水计重' : '');
    });
    Object.keys(loadout.gear || {}).forEach(function (id) {
      var it = data.equipment[id];
      if (!it || !loadout.gear[id]) return;
      add(it.name_zh, it.weight_kg * loadout.gear[id], it.category);
    });
    if (loadout.radio) add('无线电台', (data.support.comms.radio_weight_kg) || 2.4, '呼叫支援必需');

    var cls = data.classes[loadout.classId];
    var band = RF.WEIGHT.bandFor(total / loadout.capacityKg);
    var speedMult = band.speedMult * (cls && cls.passives.some(function (p) { return p.id === 'light_foot'; }) ? 1.04 : 1);
    return {
      totalKg: total, capacityKg: loadout.capacityKg, ratio: total / loadout.capacityKg,
      band: band, speedMult: speedMult, lines: lines,
      primary: prim, secondary: sec, cls: cls || null
    };
  };

  /* ------------------------------ 地图生成 ------------------------------ */

  var TERRAIN_PALETTE = {
    city: { ground: '#4a4640', patch: '#3d3934', accent: '#5b564e', fog: '#6b6a66' },
    forest: { ground: '#33402c', patch: '#2a3624', accent: '#41512f', fog: '#5d6b58' },
    field: { ground: '#5b5c33', patch: '#4e4f2b', accent: '#6b6b3d', fog: '#8b8a63' },
    village: { ground: '#514a37', patch: '#453f2f', accent: '#5f5741', fog: '#7a735c' },
    river: { ground: '#3c4a3a', patch: '#33402f', accent: '#2f4a55', fog: '#6f7d78' },
    rail: { ground: '#474643', patch: '#3c3b38', accent: '#57554f', fog: '#75736d' },
    industrial: { ground: '#43413e', patch: '#393734', accent: '#555148', fog: '#6d6a63' },
    fortress: { ground: '#4f4a42', patch: '#443f38', accent: '#5d574c', fog: '#736c62' },
    trench: { ground: '#544c34', patch: '#463f2b', accent: '#5f563b', fog: '#7d7458' }
  };

  function genMap(level, rnd) {
    var W = 2600, H = 1800;
    var obstacles = [], decals = [], zones = [];
    var t = level.terrain || 'field';
    var pal = TERRAIN_PALETTE[t] || TERRAIN_PALETTE.field;

    // 战场边界（视觉）
    zones.push({ x: 40, y: 40, w: W - 80, h: H - 80, kind: 'bounds' });

    var dense = (t === 'city' || t === 'industrial' || t === 'fortress') ? 26 : (t === 'forest' ? 34 : 16);
    var i, x, y, o;
    for (i = 0; i < dense; i++) {
      if (t === 'city' || t === 'industrial' || t === 'fortress') {
        var bw = 120 + rnd() * 260, bh = 110 + rnd() * 220;
        x = 140 + rnd() * (W - 320 - bw); y = 140 + rnd() * (H - 320 - bh);
        o = { x: x, y: y, w: bw, h: bh, kind: 'building', blocksSight: true, blocksMove: true, cover: 1.0, hp: 400 };
      } else if (t === 'forest') {
        var r = 26 + rnd() * 24;
        x = 120 + rnd() * (W - 240); y = 120 + rnd() * (H - 240);
        o = { x: x - r, y: y - r, w: r * 2, h: r * 2, kind: 'tree', blocksSight: true, blocksMove: false, cover: 0.75, round: true };
      } else if (t === 'rail') {
        o = { x: 100 + rnd() * (W - 300), y: 120 + rnd() * (H - 260), w: 60 + rnd() * 40, h: 28 + rnd() * 12, kind: 'wagon', blocksSight: true, blocksMove: true, cover: 0.95 };
      } else {
        var box = 90 + rnd() * 160;
        x = 140 + rnd() * (W - 320 - box); y = 140 + rnd() * (H - 320 - box);
        o = { x: x, y: y, w: box, h: 40 + rnd() * 40, kind: rnd() < 0.5 ? 'shed' : 'hedge', blocksSight: true, blocksMove: rnd() < 0.5, cover: 0.85 };
      }
      obstacles.push(o);
    }
    // 沙袋与掩体（不挡视线，挡子弹、提供掩蔽）
    for (i = 0; i < 22; i++) {
      obstacles.push({
        x: 120 + rnd() * (W - 300), y: 120 + rnd() * (H - 260),
        w: 70 + rnd() * 60, h: 22 + rnd() * 10, kind: 'sandbag',
        blocksSight: false, blocksMove: false, cover: 0.6
      });
    }
    // 弹坑（地形与掩体）
    for (i = 0; i < 14; i++) {
      decals.push({ x: 120 + rnd() * (W - 240), y: 120 + rnd() * (H - 240), r: 34 + rnd() * 34, kind: rnd() < 0.55 ? 'crater' : 'rubble' });
    }
    // 植被/地面色斑（视觉）
    for (i = 0; i < 140; i++) {
      decals.push({ x: rnd() * W, y: rnd() * H, r: 30 + rnd() * 90, kind: 'patch', a: 0.05 + rnd() * 0.12 });
    }

    return {
      w: W, h: H, palette: pal, obstacles: obstacles, decals: decals, zones: zones,
      playerSpawn: { x: 220, y: H - 260 },
      enemySpawns: [
        { x: W - 200, y: 200 }, { x: W - 320, y: H - 300 }, { x: W * 0.55, y: 140 }
      ],
      objectiveZone: { x: W * 0.62, y: H * 0.32, w: 300, h: 240 }
    };
  }

  /* --------------------------- 武器运行时参数 --------------------------- */

  var SPREAD_BASE = 0.030; // 弧度基准（约 1.7°）

  function weaponProfile(w) {
    var acc = w.accuracy || 0.7;
    var spread = SPREAD_BASE * (1.35 - acc) * 3.2;
    return {
      id: w.id, name: w.name_zh, dmg: w.damage, rpm: Math.max(1, w.rpm || 40),
      mag: w.magazine || 5, reload: w.reload_s || 3, range: w.effective_range_m || 300,
      pen: w.penetration_mm || 0, spread: spread, weight: w.weight_kg,
      ammo: w.ammo_type, cat: w.category, mobility: w.mobility_mod || 1
    };
  }

  /* ------------------------------- 实体 ------------------------------- */

  function makePlayer(loadout, data, computed, diff) {
    var cls = computed.cls;
    var prim = computed.primary, sec = computed.secondary;
    var ammo = {};
    function pool(w, mags) {
      if (!w) return;
      ammo[w.ammo_type] = (ammo[w.ammo_type] || 0) + (w.magazine || 5) * mags;
    }
    pool(prim, prim && prim.category === 'lmg' ? 5 : 7);
    pool(sec, 3);
    pool(prim && prim.category === 'sniper' ? prim : sec, 2);
    var ammoPouch = (loadout.gear && loadout.gear['ammo_pouch']) ? 1.4 : 1;
    Object.keys(ammo).forEach(function (k) { ammo[k] = Math.round(ammo[k] * ammoPouch); });

    return {
      kind: 'player', x: 0, y: 0, r: 11, facing: 0, alive: true,
      hp: 100, maxHp: 100, stamina: 100, food: 100, water: 100, warmth: 100,
      suppressed: 0, bleeding: false, morphineCount: 0,
      ammo: ammo, magPrim: prim ? prim.magazine : 0, magSec: sec ? sec.magazine : 0,
      loadout: loadout, computed: computed, cls: cls, data: data,
      reloading: 0, reloadWeapon: null, firing: 0, recoil: 0, ads: false, bipod: false,
      currentSlot: prim ? 'primary' : 'secondary',
      grenades: Object.assign({}, loadout.grenades || {}),
      supplies: Object.assign({}, loadout.supplies || {}),
      gear: Object.assign({}, loadout.gear || {}),
      binocular: 0, actionTimer: 0, actionLabel: '',
      grenadeCook: 0, cooking: false, vodkaTimer: 0, morphinePenalty: 0,
      marks: [], supportMark: null, radioBroken: 0,
      noSprint: false, band: computed.band,
      stats: { shots: 0, hits: 0, kills: 0, grenadesThrown: 0, supportCalls: 0, damageTaken: 0 },
      speedMult: computed.speedMult,
      weatherColdMult: 1
    };
  }

  function makeEnemy(def, x, y, scale) {
    return {
      kind: 'enemy', def: def, id: def.id, name: def.name_zh,
      x: x, y: y, r: 10, facing: Math.PI, alive: true,
      hp: def.hp * scale.hp, maxHp: def.hp * scale.hp,
      armor: def.armor_value || 0,
      dmg: (def.damage || 20) * scale.dmg, fireRate: def.fire_rate_s || 1.5,
      accuracy: clamp((def.accuracy || 0.5) * scale.acc, 0.05, 0.97),
      aware: def.ai.awareness_m || 220, reaction: def.ai.reaction_s || 1.2,
      suppressionResist: def.ai.suppression_resist || 1,
      moraleBreak: def.ai.morale_break_chance || 0,
      role: def.role || 'line', state: 'idle', timer: 0, cooldown: 0,
      target: null, aimTime: 0, suppression: 0, morale: 1,
      retarget: 0, cover: null, marks: 0
    };
  }

  function makeVehicle(def, x, y, scale) {
    return {
      kind: 'vehicle', def: def, id: def.id, name: def.name_zh,
      x: x, y: y, r: 26, facing: Math.PI, alive: true, hostile: def.faction !== 'soviet' && def.faction !== 'us',
      hp: (def.hp || 200) * scale.hp, maxHp: (def.hp || 200) * scale.hp,
      armor: def.armor_mm || 30, sideRear: def.side_rear_ratio || 0.5,
      weapon: def.weapon, dmg: 45 * scale.dmg, pen: def.penetration_mm || 50,
      engage: (def.ai && def.ai.engage_range_m) || 800, fireRate: 6.0,
      cooldown: 0, aimTime: 0, immobilized: 0, isEmplaced: !!(def.ai && def.ai.emplaced),
      tracks: 1
    };
  }

  /* ------------------------------- 游戏 ------------------------------- */

  function Game(data, opts) {
    opts = opts || {};
    this.data = data;
    this.difficultyId = opts.difficulty || 'regular';
    this.difficulty = (data.difficulty && data.difficulty[this.difficultyId]) || { enemy_hp_mult: 1, enemy_accuracy_mult: 1, player_damage_mult: 1, cp_regen_mult: 1 };
    this.mode = opts.mode || 'campaign';
    this.level = opts.level || null;
    if (!this.level) this.mode = 'endless'; // 防御：无关卡即无限模式，避免读取 null 关卡
    this.loadout = opts.loadout;
    this.seed = opts.seed == null ? hashStr((this.level && this.level.id) || 'endless') : opts.seed;
    this.rnd = mulberry32(this.seed);
    this.map = this.level ? genMap(this.level, this.rnd) : genMap({ terrain: 'field' }, this.rnd);

    var computed = RF.computeLoadout(this.loadout, data);
    this.computed = computed;
    // 武器定义 → 运行时参数（rpm/散布/穿深等统一在本层换算）
    this.profiles = {
      primary: computed.primary ? weaponProfile(computed.primary) : null,
      secondary: computed.secondary ? weaponProfile(computed.secondary) : null
    };
    this.player = makePlayer(this.loadout, data, computed, this.difficulty);
    this.player.x = this.map.playerSpawn.x;
    this.player.y = this.map.playerSpawn.y;

    this.enemies = []; this.vehicles = []; this.allies = [];
    this.projectiles = []; this.grenades = []; this.shells = []; this.particles = []; this.pickups = [];
    this.log = [];
    this.time = 0; this.kills = 0; this.wave = 0; this.wavesTotal = 1;
    this.cp = data.support.command_points.start;
    this.cpMax = data.support.command_points.max;
    this.cpRegen = data.support.command_points.regen_per_s * (this.difficulty.cp_regen_mult || 1);
    this.cooldowns = {};
    this.pendingSupport = [];
    this.radioBroken = 0;
    this.phase = 'combat';
    this.camera = { x: this.player.x, y: this.player.y };
    this.endless = null;
    /* 敌军态势：防守/迟滞/伏击关卡的敌人是进攻方（向我军推进）；
       强攻/突破/巷战等关卡的敌人依托阵地固守，遭到接触或超时后投入反击。 */
    var mt = (this.level && this.level.mission_type) || 'defense';
    this.enemyPosture = ['defense', 'delay', 'ambush'].indexOf(mt) >= 0 ? 'attacker' : 'defender';
    if (this.mode === 'endless') this.enemyPosture = 'attacker';
    this.enemyWaypoint = { x: this.player.x, y: this.player.y };
    this.waypointTimer = 25;
    this.counterAttackAt = mt === 'defense' ? 45 : 90;
    this.alertedAll = this.enemyPosture === 'attacker';
    this.noiseTimer = 0;
    this.objectives = [];
    this.buildObjectives();
    this.spawnInitial();
    this.pushLog('作战开始：' + (this.level ? this.level.name_zh : '无限模式'));
  }

  Game.prototype.pushLog = function (msg) {
    this.log.push({ t: this.time, msg: msg });
    if (this.log.length > 60) this.log.shift();
    if (RF.onLog) RF.onLog(msg);
  };

  /* ---------------------- 关卡目标（运行时） ---------------------- */
  Game.prototype.buildObjectives = function () {
    var L = this.level;
    this.totalEnemyBudget = 0;
    if (!L) {
      this.objectives = [{ id: 'endless', text_zh: '抵御无穷波次', type: 'endless', required: true, target: 0, current: 0, done: false }];
      return;
    }
    var comp = L.enemy_composition || {};
    var count = 0;
    Object.keys(comp).forEach(function (k) { count += comp[k]; });
    this.totalEnemyBudget = count;
    var dt = this.difficulty;
    var mt = L.mission_type;
    var obj = [];
    obj.push({
      id: 'elim', text_zh: '歼灭敌军主力', kind: 'eliminate', required: true,
      target: Math.max(4, Math.round(count * 0.8 * (dt.enemy_hp_mult || 1))), current: 0, done: false
    });
    if (mt === 'defense' || mt === 'siege' || mt === 'delay' || mt === 'ambush') {
      obj.push({ id: 'hold', text_zh: '守住阵地', kind: 'hold', required: true, target: 210, current: 0, done: false });
    }
    if (mt === 'river_crossing' || mt === 'breakthrough' || mt === 'relief' || mt === 'amphibious') {
      obj.push({ id: 'reach', text_zh: '抵达会合点', kind: 'reach', required: true, target: 1, current: 0, done: false });
    }
    if (mt === 'armored') {
      obj.push({ id: 'armor', text_zh: '摧毁敌军装甲', kind: 'armor', required: true, target: 2, current: 0, done: false });
    }
    obj.push({
      id: 'survive', text_zh: '活着回来', kind: 'survive', required: false,
      target: Math.round((L.par_time_s || 900) * 0.4), current: 0, done: false
    });
    this.objectives = obj;
  };

  /* --------------------------- 敌军部署 --------------------------- */

  Game.prototype.enemyScale = function () {
    var dt = this.difficulty;
    var waveFactor = 1 + (this.wave - 1) * 0.06;
    return {
      hp: (dt.enemy_hp_mult || 1) * waveFactor,
      acc: (dt.enemy_accuracy_mult || 1),
      dmg: (dt.enemy_hp_mult || 1) * 0.9 + 0.1
    };
  };

  Game.prototype.compositionForWave = function (wave) {
    if (this.mode === 'endless') return this.endlessComposition(wave);
    var comp = this.level.enemy_composition || {};
    var pools = [];
    Object.keys(comp).forEach(function (k) {
      var def = this.findEnemyDef(k);
      if (def) pools.push({ def: def, n: comp[k] });
      else this.pushLog('⚠ 数据缺失：敌军 id "' + k + '" 未在 enemies.json 中找到');
    }, this);
    // 分 3 波投放
    var waves = 3;
    this.wavesTotal = waves;
    var out = [];
    pools.forEach(function (p) {
      var per = Math.max(1, Math.round(p.n / waves));
      var left = p.n;
      for (var w = 0; w < waves; w++) {
        var take = w === waves - 1 ? left : Math.min(per, left);
        left -= take;
        if (take > 0) out.push({ wave: w, def: p.def, n: take });
      }
    });
    return out.filter(function (e) { return e.wave === wave - 1; });
  };

  Game.prototype.endlessComposition = function (wave) {
    var era = wave <= 3 ? 1941 : (wave <= 8 ? 1942 : (wave <= 14 ? 1943 : 1945));
    var table = {
      1941: [['ger_landser_rifleman', 8], ['ger_mp40_leader', 1], ['ger_mg42_team', 1]],
      1942: [['ger_landser_rifleman', 8], ['ger_ss_grenadier', 4], ['ger_mg42_team', 2], ['ger_sniper', 1]],
      1943: [['ger_ss_grenadier', 8], ['ger_mg42_team', 2], ['ger_panzerfaust', 2], ['stug_iii_g', 1]],
      1945: [['ger_ss_grenadier', 8], ['ger_volkssturm', 8], ['ger_panzerfaust', 3], ['pzkpfw_vi_tiger', 1], ['pak40', 1]]
    }[era];
    var scale = 1 + Math.floor(wave * 0.16);
    var out = [];
    table.forEach(function (row) {
      var def = this.findEnemyDef(row[0]);
      if (!def) return;
      out.push({ wave: wave, def: def, n: Math.max(1, Math.round(row[1] * (wave < 4 ? 1 : Math.min(scale, 2.4)))) });
    }, this);
    return out;
  };

  Game.prototype.findEnemyDef = function (id) {
    var lists = ['infantry', 'vehicles', 'air'];
    for (var i = 0; i < lists.length; i++) {
      var arr = this.data.enemies[lists[i]] || [];
      for (var j = 0; j < arr.length; j++) if (arr[j].id === id) return arr[j];
    }
    return null;
  };

  Game.prototype.spawnInitial = function () {
    this.wave = 1;
    this.spawnWave(this.wave);
  };

  Game.prototype.spawnWave = function (wave) {
    var list = this.compositionForWave(wave);
    var scale = this.enemyScale();
    var spawns = this.map.enemySpawns;
    var si = 0;
    list.forEach(function (entry) {
      // 敌机不做地面单位生成：改为一次敌方空袭事件（真实：Ju 87 俯冲轰炸）
      if (/^(bf109|ju87|fw190|bf_|ju_|fw_)/.test(entry.def.id) || entry.def.fronts && entry.def.weapon && /机炮|炸弹/.test(entry.def.weapon || '') && !entry.def.speed_kph) {
        for (var ai = 0; ai < entry.n; ai++) {
          this.pendingSupport.push({
            id: 'enemy_air', def: { name_zh: '敌方空袭：' + entry.def.name_zh, radius_m: (entry.def.ai && entry.def.ai.bomb_radius_m) || 40, rounds: 1, damage_per_round: 120 },
            x: this.player.x + (this.rnd() - 0.5) * 200, y: this.player.y + (this.rnd() - 0.5) * 200,
            t: 18 + this.rnd() * 20, spreadMult: 1.0, roundsLeft: 1, interval: 0.5, enemy: true, danger: 'danger', isAir: false
          });
        }
        this.pushLog('⚠ 空中威胁：' + entry.def.name_zh + ' 正在接近，寻找掩体！');
        return;
      }
      var isVeh = !!(entry.def.penetration_mm !== undefined && entry.def.speed_kph !== undefined) || /^(pz|stug|sdkfz|pak|flak)/.test(entry.def.id);
      for (var i = 0; i < entry.n; i++) {
        var sp = spawns[si % spawns.length]; si++;
        var x = clamp(sp.x + (this.rnd() - 0.5) * 340, 80, this.map.w - 80);
        var y = clamp(sp.y + (this.rnd() - 0.5) * 340, 80, this.map.h - 80);
        if (isVeh) this.vehicles.push(makeVehicle(entry.def, x, y, scale));
        else this.enemies.push(makeEnemy(entry.def, x, y, scale));
      }
    }, this);
    this.pushLog('第 ' + wave + ' 波敌军进入战场（' + (this.mode === 'endless' ? '无限模式' : '战役') + '）');
  };

  /* --------------------------- 支援火力 --------------------------- */

  Game.prototype.supportDef = function (id) {
    var arr = this.data.support.call_ins || [];
    for (var i = 0; i < arr.length; i++) if (arr[i].id === id) return arr[i];
    return null;
  };

  Game.prototype.canCall = function (id) {
    if (this.radioBroken > 0) return { ok: false, why: '无线电台受损（' + Math.ceil(this.radioBroken) + 's）' };
    if (!this.loadout.radio) return { ok: false, why: '未携带无线电台' };
    var def = this.supportDef(id);
    if (!def) return { ok: false, why: '未知支援' };
    if ((this.cooldowns[id] || 0) > 0) return { ok: false, why: '冷却中 ' + Math.ceil(this.cooldowns[id]) + 's' };
    if (this.cp < def.cp_cost) return { ok: false, why: '指挥点不足（需 ' + def.cp_cost + '）' };
    return { ok: true, def: def };
  };

  Game.prototype.callSupport = function (id, x, y) {
    var chk = this.canCall(id);
    if (!chk.ok) { this.pushLog('呼叫失败：' + chk.why); return false; }
    var def = chk.def;
    var delay = def.delay_s;
    var spreadMult = 1;
    if (this.player.binocular > 0) spreadMult -= 0.35;
    var scoutPerk = this.player.cls && this.player.cls.passives.some(function (p) { return p.id === 'pathfinder'; });
    if (scoutPerk) spreadMult -= 0.2;
    if (this.player.marks.some(function (m) { return dist(m.x, m.y, x, y) < 200; })) spreadMult -= 0.25;
    spreadMult = clamp(spreadMult, 0.25, 1.6);
    // 危险距离检查（自己人也会被炸）
    var friendlyDist = dist(this.player.x, this.player.y, x, y);
    var danger = null;
    if (def.radius_m) {
      if (friendlyDist < def.radius_m * 1.2) danger = 'danger';
      else if (friendlyDist < def.radius_m * 2.4) danger = 'caution';
    }
    this.cp -= def.cp_cost;
    this.cooldowns[id] = def.cooldown_s;
    this.pendingSupport.push({
      id: id, def: def, x: x, y: y, t: delay, spreadMult: spreadMult,
      roundsLeft: def.rounds || 1, interval: def.rounds ? Math.max(0.12, (def.rounds > 1 ? 6 : 1) / def.rounds) : 0,
      isAir: def.type === 'air', isArmor: def.type === 'armor' || def.type === 'armor_siege',
      isInfantry: def.type === 'infantry', isObscurant: def.type === 'obscurant',
      danger: danger
    });
    this.player.stats.supportCalls++;
    this.pushLog('已呼叫：' + def.name_zh + '，弹着延迟 ' + delay + 's' +
      (danger === 'danger' ? '（⚠ 极近距离，会误伤！）' : danger === 'caution' ? '（距离偏近，注意掩蔽）' : ''));
    // 敌方无线电测向 → 反炮兵
    if (this.difficultyId === 'veteran' || this.difficultyId === 'historical') {
      if (this.rnd() < 0.25) {
        this.pendingSupport.push({
          id: 'counter_battery', def: { name_zh: '敌方反炮兵火力', radius_m: 26, rounds: 4, damage_per_round: 90 }, x: this.player.x, y: this.player.y,
          t: delay + 12, spreadMult: 1.0, roundsLeft: 4, interval: 1.2, enemy: true, danger: 'danger'
        });
        this.pushLog('⚠ 敌方无线电测向成功，反炮兵火力正在路上！');
      }
    }
    return true;
  };

  Game.prototype.pendingWarnings = function () {
    return this.pendingSupport.map(function (p) {
      return { name: p.def.name_zh, x: p.x, y: p.y, t: p.t, danger: p.danger, enemy: !!p.enemy };
    });
  };

  /* --------------------------- 主循环 --------------------------- */

  Game.prototype.step = function (dt, input) {
    dt = Math.min(dt, 0.05);
    input = input || {};
    if (this.phase === 'won' || this.phase === 'lost') { this.stepParticles(dt); return; }
    this.time += dt;
    if (this.noiseTimer > 0) this.noiseTimer = Math.max(0, this.noiseTimer - dt);
    this.updatePlayer(dt, input);
    this.updateSupport(dt);
    this.updateEnemies(dt, input);
    this.updateVehicles(dt);
    this.updateAllies(dt);
    this.updateProjectiles(dt);
    this.updateGrenades(dt);
    this.stepParticles(dt);
    this.updateObjectives(dt);
    this.updateCamera(dt, input);
  };

  /* ---- 玩家 ---- */
  Game.prototype.updatePlayer = function (dt, input) {
    var p = this.player;
    if (!p.alive) return;
    var band = p.band;

    // 生存
    var cold = this.isCold();
    p.food = clamp(p.food - dt * (100 / 1100) * (this.difficultyId === 'historical' ? 1.5 : this.difficultyId === 'veteran' ? 1.2 : 1) * (p.cls && p.cls.passives.some(function (x) { return x.id === 'ration_discipline'; }) ? 0.85 : 1), 0, 100);
    var waterRate = dt * (100 / 800) * (input.sprint ? 1.6 : 1) * (this.map.palette === TERRAIN_PALETTE.field ? 1.15 : 1);
    p.water = clamp(p.water - waterRate, 0, 100);
    if (cold) p.warmth = clamp(p.warmth - dt * (100 / 420), 0, 100);
    else p.warmth = clamp(p.warmth + dt * 2.5, 0, 100);

    var starving = p.food <= 0, dehydrated = p.water <= 0, freezing = p.warmth <= 0;
    if (starving) p.hp -= dt * 1.6;
    if (dehydrated) p.hp -= dt * 2.2;
    if (freezing) p.hp -= dt * 3.0;
    if (p.bleeding) p.hp -= dt * 2.0;

    // 体力
    var moving = input.moveX || input.moveY;
    var sprint = !!input.sprint && moving && p.stamina > 3 && !p.noSprint && p.water > 0;
    var drain = sprint ? 9.5 : (moving ? 2.2 : -6.5);
    p.stamina = clamp(p.stamina - drain * dt * band.staminaDrain, 0, 100);
    if (p.stamina <= 0) sprint = false;

    // 移动
    var speed = 132 * band.speedMult * (p.cls && p.cls.passives.some(function (x) { return x.id === 'light_foot'; }) ? 1.04 : 1);
    if (input.crouch) speed *= 0.58;
    if (sprint) speed *= 1.45;
    if (p.stamina < 20) speed *= 0.9;
    if (starving) speed *= 0.92;
    if (dehydrated) speed *= 0.88;
    if (p.legWound) speed *= 0.75;
    var mx = input.moveX || 0, my = input.moveY || 0;
    var ml = Math.sqrt(mx * mx + my * my);
    if (ml > 0) { mx /= ml; my /= ml; }
    var nx = p.x + mx * speed * dt, ny = p.y + my * speed * dt;
    p.x = this.resolveMove(p.x, p.y, nx, ny, p.r);
    p.y = this.resolveMoveY(p.x, p.y, nx, ny, p.r);

    // 朝向
    var aim = input.aimWorld;
    if (aim) p.facing = angleTo(p.x, p.y, aim.x, aim.y);
    p.ads = !!input.ads;
    p.binocular = input.binocular ? 1 : 0;
    p.crouch = !!input.crouch;
    // 两脚架：机枪/自动步枪在蹲伏静止时架设（数据：machine_gunner 的 bipod_discipline）
    var cw = this.currentWeapon();
    p.bipod = !!(cw && (cw.cat === 'lmg' || cw.cat === 'hmg_emplacement') && input.crouch && !moving);

    // 伏特加 / 吗啡
    if (p.vodkaTimer > 0) p.vodkaTimer -= dt;

    // 动作计时（进食/饮水/医疗共用）
    if (p.actionTimer > 0) {
      p.actionTimer -= dt;
      if (p.actionTimer <= 0) { if (p.actionDone) p.actionDone(); p.actionDone = null; }
    }

    // 换弹
    if (p.reloading > 0) {
      p.reloading -= dt;
      if (p.reloading <= 0) { this.finishReload(); }
    }

    // 射击
    if (input.fire && !p.reloading && p.actionTimer <= 0) this.tryFire(dt, input);
    else p.firing = Math.max(0, p.firing - dt);

    // 后坐恢复
    p.recoil = Math.max(0, p.recoil - dt * 3.4);

    // 手榴弹
    if (input.cookGrenade && p.grenadeTime === undefined) p.grenadeTime = 0;
    if (input.cookGrenade) p.grenadeTime = (p.grenadeTime || 0) + dt;
    if (!input.cookGrenade && p.grenadeTime > 0) { this.throwGrenade(Math.min(p.grenadeTime, 4.0)); p.grenadeTime = 0; }
    p.cooking = !!input.cookGrenade;

    // 压制与标记衰减
    p.suppressed = Math.max(0, p.suppressed - dt * 0.7);
    p.radioBroken = Math.max(0, p.radioBroken - dt);
    this.radioBroken = p.radioBroken;

    // 支援标记（用于精度加成）
    for (var i = p.marks.length - 1; i >= 0; i--) { p.marks[i].t -= dt; if (p.marks[i].t <= 0) p.marks.splice(i, 1); }

    // 拾取
    for (var k = this.pickups.length - 1; k >= 0; k--) {
      var it = this.pickups[k];
      if (dist(p.x, p.y, it.x, it.y) < 22) {
        if (it.type === 'food') { p.food = clamp(p.food + it.value, 0, 100); this.pushLog('拾取补给：' + it.label); }
        if (it.type === 'water') { p.water = clamp(p.water + it.value, 0, 100); this.pushLog('拾取饮水：' + it.label); }
        if (it.type === 'ammo') { p.ammo[it.ammo] = (p.ammo[it.ammo] || 0) + it.value; this.pushLog('拾取弹药：' + it.label); }
        if (it.type === 'med') { p.hp = clamp(p.hp + it.value, 0, 100); p.bleeding = false; this.pushLog('拾取医疗：' + it.label); }
        this.pickups.splice(k, 1);
      }
    }

    // 炮击/爆炸对玩家的伤害在其他更新里处理
    // 上限
    p.hp = clamp(p.hp, 0, p.maxHp);
    if (p.hp <= 0 && p.alive) this.playerDied();
    p.speedMult = band.speedMult;
    p.cold = cold;
    p.starving = starving; p.dehydrated = dehydrated; p.freezing = freezing;
    this.lastSprint = sprint;
  };

  Game.prototype.isCold = function () {
    var w = (this.level && this.level.weather) || 'clear';
    return w === 'snow' || w === 'storm' || w === 'rain';
  };

  Game.prototype.resolveMove = function (x, y, nx, ny, r) {
    for (var i = 0; i < this.map.obstacles.length; i++) {
      var o = this.map.obstacles[i];
      if (!o.blocksMove) continue;
      if (circleRect(nx, y, r, o)) return x;
    }
    return clamp(nx, 30, this.map.w - 30);
  };
  Game.prototype.resolveMoveY = function (x, y, nx, ny, r) {
    for (var i = 0; i < this.map.obstacles.length; i++) {
      var o = this.map.obstacles[i];
      if (!o.blocksMove) continue;
      if (circleRect(x, ny, r, o)) return y;
    }
    return clamp(ny, 30, this.map.h - 30);
  };

  Game.prototype.currentWeapon = function () {
    var p = this.player;
    return p.currentSlot === 'primary' ? this.profiles.primary : this.profiles.secondary;
  };
  Game.prototype.currentMag = function () {
    var p = this.player;
    return p.currentSlot === 'primary' ? p.magPrim : p.magSec;
  };

  Game.prototype.loadedWeaponIds = function () {
    var out = [];
    if (this.profiles.primary) out.push(this.profiles.primary.id);
    if (this.profiles.secondary) out.push(this.profiles.secondary.id);
    return out;
  };

  Game.prototype.switchWeapon = function () {
    var p = this.player;
    if (this.profiles.primary && this.profiles.secondary) {
      p.currentSlot = p.currentSlot === 'primary' ? 'secondary' : 'primary';
      p.reloading = 0;
      this.pushLog('切换武器：' + this.currentWeapon().name);
    }
  };

  Game.prototype.tryFire = function (dt, input) {
    var p = this.player;
    var w = this.currentWeapon();
    if (!w) return;
    if (p.cooldown === undefined) p.cooldown = 0;
    p.cooldown -= dt;
    var cat = w.cat;
    // 自动/半自动
    var auto = cat === 'smg' || cat === 'lmg' || cat === 'flamethrower' || cat === 'assault_rifle' || cat === 'at_rocket';
    if (!auto) {
      if (input.firePressed !== true) { p.cooldown = Math.max(p.cooldown, 0); return; }
    }
    if (p.cooldown > 0) return;
    var mag = this.currentMag();
    if (mag <= 0) { this.startReload(); return; }
    p.cooldown = 60 / w.rpm;
    this.spendMag(1);
    this.spawnPlayerShot(w, input);
    p.stats.shots++;
  };

  Game.prototype.spendMag = function (n) {
    var p = this.player;
    if (p.currentSlot === 'primary') p.magPrim = Math.max(0, p.magPrim - n);
    else p.magSec = Math.max(0, p.magSec - n);
  };

  Game.prototype.playerSpread = function (w, input) {
    var p = this.player;
    var s = w.spread;
    var move = Math.sqrt((input.moveX || 0) * (input.moveX || 0) + (input.moveY || 0) * (input.moveY || 0));
    if (this.lastSprint) s *= 2.2; else if (move > 0.1) s *= 1.45;
    if (input.crouch) s *= 0.68;
    if (p.ads) s *= 0.45;
    if (p.bipod) s *= 0.5;
    var ratio = this.computed.ratio;
    if (ratio > 1) s *= 1.5; else if (ratio > 0.85) s *= 1.15;
    if (p.stamina < 30) s *= 1.25;
    if (p.food < 25 || p.water < 25) s *= 1.3;
    if (p.vodkaTimer > 0) s *= 1.25;
    if (p.suppressed > 0) s *= 1 + Math.min(p.suppressed, 3) * 0.15;
    if (p.warmth < 30) s *= 1.2;
    s *= 1 + p.recoil * 0.08;
    return s;
  };

  Game.prototype.spawnPlayerShot = function (w, input) {
    var p = this.player;
    var spread = this.playerSpread(w, input);
    var a = (p.facing || 0) + (this.rnd() - 0.5) * spread * 2;
    p.facing = a;
    var muzzle = w.cat === 'flamethrower' ? 34 : 14;
    var speed = w.cat === 'flamethrower' ? 260 : (w.cat === 'at_rocket' ? 420 : 1400);
    var dmg = w.dmg * (this.difficulty.player_damage_mult || 1);
    this.projectiles.push({
      x: p.x + Math.cos(a) * muzzle, y: p.y + Math.sin(a) * muzzle,
      vx: Math.cos(a) * speed, vy: Math.sin(a) * speed,
      dmg: dmg, pen: w.pen, life: w.range / speed + 0.25, owner: 'player',
      tracer: w.cat !== 'flamethrower', cat: w.cat, flame: w.cat === 'flamethrower'
    });
    p.recoil += w.dmg / 120;
    this.muzzleFlash(p.x + Math.cos(a) * muzzle, p.y + Math.sin(a) * muzzle, a);
    // 噪音：敌人警戒
    this.makeNoise(p.x, p.y, 520);
  };

  Game.prototype.startReload = function () {
    var p = this.player;
    if (p.reloading > 0) return;
    var w = this.currentWeapon();
    if (!w) return;
    var pool = p.ammo[w.ammo] || 0;
    var mag = this.currentMag();
    if (pool <= 0) { this.pushLog('弹药耗尽！(' + w.name + ')'); return; }
    if (mag >= w.mag && w.cat !== 'flamethrower') return;
    var mult = 1;
    if (p.cls && p.cls.passives.some(function (x) { return x.id === 'fast_reload'; }) && w.cat === 'smg') mult *= 0.7;
    if (this.computed.ratio > 1) mult *= 1.25;
    if ((p.gear && p.gear['ammo_pouch'])) mult *= 0.92;
    p.reloading = w.reload * mult;
    p.reloadWeapon = w.id;
    this.pushLog('换弹：' + w.name + '（' + round1(p.reloading) + 's）');
  };

  Game.prototype.finishReload = function () {
    var p = this.player;
    var w = this.currentWeapon();
    if (!w) return;
    var need = w.mag - this.currentMag();
    var pool = p.ammo[w.ammo] || 0;
    var take = Math.min(need, pool);
    p.ammo[w.ammo] = pool - take;
    if (p.currentSlot === 'primary') p.magPrim += take; else p.magSec += take;
    p.reloading = 0;
  };

  Game.prototype.throwGrenade = function (cook) {
    var p = this.player;
    var ids = Object.keys(p.grenades).filter(function (k) { return p.grenades[k] > 0; });
    // 优先杀伤手榴弹，其次烟幕
    ids.sort(function (a, b) {
      var ea = this.data.equipment[a] || {}, eb = this.data.equipment[b] || {};
      return (eb.effect ? eb.effect.damage : 0) - (ea.effect ? ea.effect.damage : 0);
    }.bind(this));
    if (!ids.length) { this.pushLog('没有手榴弹了'); return; }
    var id = ids[0];
    var it = this.data.equipment[id];
    p.grenades[id]--;
    var fuse = Math.max(0.6, (it.effect.fuse_s || 3) - cook);
    var speed = 300;
    this.grenades.push({
      x: p.x + Math.cos(p.facing) * 16, y: p.y + Math.sin(p.facing) * 16,
      vx: Math.cos(p.facing) * speed, vy: Math.sin(p.facing) * speed,
      fuse: fuse, item: it, owner: 'player', r: 5, bounce: 0.35
    });
    p.stats.grenadesThrown++;
    this.pushLog('投掷 ' + it.name_zh + '（引信 ' + round1(fuse) + 's）');
  };

  Game.prototype.useSupply = function (kind) {
    var p = this.player;
    if (p.actionTimer > 0) return false;
    var ids = Object.keys(p.supplies).filter(function (k) { return p.supplies[k] > 0; });
    var picked = null, isWater = kind === 'water';
    for (var i = 0; i < ids.length; i++) {
      var it = this.data.equipment[ids[i]];
      if (!it) continue;
      if (isWater && it.category === 'water' && (!picked || p.water < 80)) picked = ids[i];
      if (!isWater && it.category === 'food' && (!picked || p.food < 80)) picked = ids[i];
    }
    if (!picked) { this.pushLog(isWater ? '没有可用的水' : '没有食物'); return false; }
    var item = this.data.equipment[picked];
    p.supplies[picked]--;
    var sub = p.picked;
    p.actionLabel = (isWater ? '饮水：' : '进食：') + item.name_zh;
    p.actionTimer = item.effect.drink_time_s || item.effect.eat_time_s || (isWater ? 3.5 : 6);
    p.actionDone = function () {
      if (isWater) p.water = clamp(p.water + (item.effect.hydration || 40), 0, 100);
      else p.food = clamp(p.food + (item.effect.calories || 30), 0, 100);
      if (item.id === 'vodka_ration') { p.vodkaTimer = 40; p.hp = clamp(p.hp + 5, 0, p.maxHp); }
      if (item.id === 'morphine') { p.morphineCount++; if (p.morphineCount > 2) p.morphinePenalty = 0.85; }
      this.pushLog((isWater ? '已饮水：' : '已进食：') + item.name_zh);
    }.bind(this);
    return true;
  };

  Game.prototype.useMedical = function () {
    var p = this.player;
    var band = p.gear['bandage'] > 0 ? 'bandage' : (p.gear['medkit'] > 0 ? 'medkit' : null);
    if (!band) { this.pushLog('没有医疗物品'); return false; }
    var it = this.data.equipment[band];
    p.gear[band]--;
    p.actionLabel = '使用医疗：' + it.name_zh;
    p.actionTimer = it.effect.use_time_s || 7;
    p.actionDone = function () {
      p.hp = clamp(p.hp + (it.effect.heal || 20), 0, p.maxHp);
      if (it.effect.stop_bleed) p.bleeding = false;
      this.pushLog('医疗完成：' + it.name_zh);
    }.bind(this);
    return true;
  };

  /* 敌军发现距离：基础感知 × 地形通透度 × 玩家暴露程度，并受能见度上限约束。
     现实依据：开阔地形 300-500m 内可发现移动目标，城市/森林/废墟因遮蔽降到 100-200m。 */
  var TERRAIN_SIGHT = { field: 1.7, trench: 1.5, river: 1.25, rail: 1.1, village: 1.05, fortress: 1.0, forest: 0.95, industrial: 0.85, city: 0.8 };

  Game.prototype.spotRange = function (e) {
    var vis = (this.level && this.level.visibility_m) || 600;
    var terr = (this.level && this.level.terrain) || 'field';
    var base = (e.aware || 220) * (TERRAIN_SIGHT[terr] || 1.1);
    if (this.noiseTimer > 0) base *= 1.5;          // 开枪/爆炸暴露位置
    if (this.player.crouch) base *= 0.75;
    if (this.player.binocular > 0) base *= 1.1;
    if (this.isCold()) base *= 0.9;                // 风雪降低观察距离
    return Math.min(base, vis, 900);
  };

  Game.prototype.playerDied = function () {    this.player.alive = false;
    this.player.hp = 0;
    this.phase = 'lost';
    this.pushLog('✖ 阵亡。' + (this.level ? '关卡失败：' + this.level.name_zh : '无限模式结束'));
  };

  /* ---- 支援火力更新 ---- */
  Game.prototype.updateSupport = function (dt) {
    for (var id in this.cooldowns) { this.cooldowns[id] = Math.max(0, this.cooldowns[id] - dt); }
    var gain = this.cpRegen * dt;
    this.cp = clamp(this.cp + gain, 0, this.cpMax);

    for (var i = this.pendingSupport.length - 1; i >= 0; i--) {
      var s = this.pendingSupport[i];
      s.t -= dt;
      if (s.t <= 0) {
        if (s.isArmor) { this.deployArmor(s); this.pendingSupport.splice(i, 1); continue; }
        if (s.isInfantry) { this.deployInfantry(s); this.pendingSupport.splice(i, 1); continue; }
        if (s.isObscurant) { this.spawnSmoke(s.x, s.y, s.def.radius_m, s.def.duration_s || 30); this.pendingSupport.splice(i, 1); continue; }
        if (s.isAir) { this.airStrike(s); this.pendingSupport.splice(i, 1); continue; }
        // 间接火力：逐发落下
        if (s.roundsLeft > 0) {
          var spread = (s.def.spread_base_m || 15) * s.spreadMult;
          var ang = this.rnd() * Math.PI * 2, rr = Math.sqrt(this.rnd()) * spread;
          this.impact(s.x + Math.cos(ang) * rr, s.y + Math.sin(ang) * rr, s.def.radius_m || 25, s.def.damage_per_round || 90, s.enemy ? 'enemy' : 'friendly');
          s.roundsLeft--;
          s.t = s.interval;
        } else this.pendingSupport.splice(i, 1);
      }
    }
  };

  Game.prototype.deployArmor = function (s) {
    var n = s.def.units || 3;
    for (var i = 0; i < n; i++) {
      var tank = {
        kind: 'ally_tank', name: s.def.name_zh, x: this.player.x - 240 - i * 70, y: this.player.y + 60 + i * 40,
        r: 28, hp: (s.def.hp_each || 340), maxHp: (s.def.hp_each || 340), armor: s.def.armor_mm || 45,
        pen: s.def.penetration_mm || 78, dmg: 120, cooldown: 0, facing: 0, alive: true, desant: s.def.desant_slots || 0
      };
      this.allies.push(tank);
    }
    this.pushLog('装甲支援抵达：' + s.def.name_zh + ' ×' + n + '（8 分钟后撤离）');
    this.armorTimer = 480;
  };

  Game.prototype.deployInfantry = function (s) {
    var n = s.squad_size || 6;
    for (var i = 0; i < n; i++) {
      this.allies.push({
        kind: 'ally_inf', name: s.def.name_zh, x: this.player.x - 120 + (this.rnd() - 0.5) * 120,
        y: this.player.y + 80 + (this.rnd() - 0.5) * 120, r: 10, hp: 100, maxHp: 100,
        dmg: 18, cooldown: 0, alive: true, facing: 0, mode: 'advance'
      });
    }
    this.pushLog('步兵增援抵达：' + s.def.name_zh + ' ×' + n);
  };

  Game.prototype.airStrike = function (s) {
    // 敌方高炮拦截判定
    var flak = this.vehicles.filter(function (v) { return v.alive && /flak/.test(v.id); });
    if (flak.length && this.rnd() < 0.35 * Math.min(flak.length, 2)) {
      this.pushLog('✖ 空中支援被高炮拦截，攻击机撤离（敌方 Flak 36 生效）');
      return;
    }
    var passes = s.def.passes || 2;
    for (var p = 0; p < passes; p++) {
      this.shells.push({
        kind: 'air', x: s.x - 400 + (this.rnd() - 0.5) * 60, y: s.y - 300,
        tx: s.x, ty: s.y, t: 1.2 + p * 2.2, pass: p,
        radius: s.def.radius_m || 25, dmg: s.def.damage_per_pass || 130, pen: s.def.armor_penetration_mm || 45
      });
    }
    this.pushLog('空中支援进入：' + s.def.name_zh + ' ×' + passes + ' 次通场');
  };

  Game.prototype.impact = function (x, y, radius, dmg, source) {
    this.explosionFx(x, y, radius);
    var i;
    for (i = 0; i < this.enemies.length; i++) {
      var e = this.enemies[i];
      if (!e.alive) continue;
      var d = dist(x, y, e.x, e.y);
      if (d < radius) {
        var f = 1 - d / radius;
        this.damageEnemy(e, dmg * f * f * (0.6 + 0.4 * f), 'explosion');
        e.suppression += 2.2 * f;
      }
    }
    for (i = 0; i < this.vehicles.length; i++) {
      var v = this.vehicles[i];
      if (!v.alive) continue;
      var dv = dist(x, y, v.x, v.y);
      if (dv < radius * 1.1) {
        var fv = 1 - dv / (radius * 1.1);
        if (dmg * fv > 40) this.damageVehicle(v, dmg * fv * 0.8, 60, x, y);
      }
    }
    // 友军误伤
    for (i = 0; i < this.allies.length; i++) {
      var a = this.allies[i];
      if (!a.alive) continue;
      var da = dist(x, y, a.x, a.y);
      if (da < radius) { a.hp -= dmg * (1 - da / radius) * 0.7; if (a.hp <= 0) { a.alive = false; this.pushLog('⚠ 友军被己方火力击中阵亡：' + a.name); } }
    }
    // 玩家误伤
    var pd = dist(x, y, this.player.x, this.player.y);
    if (pd < radius && this.player.alive) {
      var fp = 1 - pd / radius;
      this.hurtPlayer(dmg * fp * 0.75 * (source === 'friendly' ? 0.9 : 1), 'explosion');
      this.player.suppressed += 3 * fp;
      if (source === 'friendly') this.pushLog('⚠ 你在己方火力覆盖范围内，被冲击波击中！');
    }
    // 覆盖物被摧毁
    for (i = 0; i < this.map.obstacles.length; i++) {
      var o = this.map.obstacles[i];
      if (o.kind === 'building' || o.kind === 'shed') {
        if (circleRect(x, y, radius * 0.5, o)) {
          o.hp = (o.hp || 400) - dmg;
          if (o.hp <= 0 && !o.destroyed) { o.destroyed = true; o.blocksSight = false; o.blocksMove = false; o.cover = 0.2; this.pushLog('建筑被炮火摧毁，出现瓦砾通道'); }
        }
      }
    }
  };

  Game.prototype.spawnSmoke = function (x, y, radius, duration) {
    this.particles.push({ kind: 'smoke', x: x, y: y, r: 40, targetR: radius, t: duration, maxT: duration });
    this.pushLog('烟幕展开，遮蔽范围 ' + Math.round(radius) + 'm');
  };

  Game.prototype.smokeAt = function (x, y) {
    for (var i = 0; i < this.particles.length; i++) {
      var p = this.particles[i];
      if (p.kind === 'smoke' && dist(x, y, p.x, p.y) < (p.r || 40)) return true;
    }
    return false;
  };

  /* ---- 敌人 AI ---- */
  Game.prototype.makeNoise = function (x, y, radius) {
    this.noiseTimer = 4;
    for (var i = 0; i < this.enemies.length; i++) {
      var e = this.enemies[i];
      if (!e.alive) continue;
      if (dist(x, y, e.x, e.y) < radius) { e.alerted = true; e.state = 'advance'; }
    }
    // 枪声沿战线传播：附近的班排也会进入警戒
    for (var j = 0; j < this.enemies.length; j++) {
      var e2 = this.enemies[j];
      if (e2.alive && dist(x, y, e2.x, e2.y) < radius * 2.2) e2.alerted = true;
    }
  };

  Game.prototype.updateEnemies = function (dt, input) {
    var p = this.player;
    // 敌军指挥节奏：定期更新「敌军掌握的方位」，久无接触则投入反击
    this.waypointTimer -= dt;
    if (this.waypointTimer <= 0) {
      this.waypointTimer = this.alertedAll ? 12 : 25;
      if (p.alive && (this.alertedAll || this.enemyPosture === 'attacker')) this.enemyWaypoint = { x: p.x, y: p.y };
    }
    if (!this.alertedAll && this.enemyPosture === 'defender' && this.time > this.counterAttackAt) {
      this.alertedAll = true;
      if (p.alive) this.enemyWaypoint = { x: p.x, y: p.y };
      this.pushLog('◆ 敌军预备队投入反击（Gegenangriff），开始向我军方向推进');
    }
    for (var i = 0; i < this.enemies.length; i++) {
      var e = this.enemies[i];
      if (!e.alive) continue;
      e.timer += dt;
      e.cooldown -= dt;
      if (e.suppression > 0) e.suppression = Math.max(0, e.suppression - dt * 0.5);

      var d = dist(e.x, e.y, p.x, p.y);
      var los = this.hasLOS(e.x, e.y, p.x, p.y) && !this.smokeAt(p.x, p.y);
      var spot = this.spotRange(e);
      var sees = p.alive && d < spot && los;
      if (sees) {
        e.alerted = true; e.lastSeen = { x: p.x, y: p.y, t: 0 };
        if (!this.alertedAll) {
          this.alertedAll = true;
          this.enemyWaypoint = { x: p.x, y: p.y };
          this.pushLog('◆ 敌军发现我军位置，全线进入战斗状态');
        }
      }

      // 士气
      if (e.moraleBreak && e.hp < e.maxHp * 0.4 && this.rnd() < e.moraleBreak * dt) {
        if (e.state !== 'flee') { e.state = 'flee'; this.pushLog(e.name + ' 士气崩溃，向后方逃散'); }
      }
      if (e.suppression > 2.5 && e.morale < 0.6 && e.state !== 'flee') e.state = 'cover';
      if (e.suppression > 4 && this.rnd() < 0.5 * dt) e.state = 'cover';

      if (e.state === 'idle') {
        if (e.alerted || this.alertedAll) e.state = 'advance';
        else if (d < spot * 0.6) e.state = 'advance';
      }

      if (e.state === 'flee') {
        var fa = angleTo(p.x, p.y, e.x, e.y);
        var nfx = e.x + Math.cos(fa) * 70 * dt, nfy = e.y + Math.sin(fa) * 70 * dt;
        e.x = this.resolveMove(e.x, e.y, nfx, nfy, e.r);
        e.y = this.resolveMoveY(e.x, e.y, nfx, nfy, e.r);
        continue;
      }

      // 敌方前沿观察员/班长呼叫迫击炮（历史：德军无线电请求火力支援）
      if (e.alerted && e.def.ai && e.def.ai.calls_support && (e.def.ai.call_support_s || 45)) {
        e.supportTimer = (e.supportTimer || 0) + dt;
        if (e.supportTimer > e.def.ai.call_support_s) {
          e.supportTimer = -1e9 + this.rnd() * 1e9; // 只呼叫一次
          if (this.rnd() < 0.6) {
            this.pendingSupport.push({
              id: 'enemy_mortar', def: { name_zh: '敌方迫击炮火力（' + e.name + ' 呼叫）', radius_m: 22, rounds: 6, damage_per_round: 75 },
              x: p.x + (this.rnd() - 0.5) * 120, y: p.y + (this.rnd() - 0.5) * 120,
              t: 22, spreadMult: 1.0, roundsLeft: 6, interval: 1.4, enemy: true, danger: 'caution'
            });
            this.pushLog('⚠ ' + e.name + ' 呼叫了迫击炮火力，注意隐蔽！');
          }
        }
      }

      var engageBand = e.role === 'sniper' ? 420 : (e.role === 'support' ? 320 : 200);
      var stopDist = e.role === 'support' ? 260 : (e.role === 'sniper' ? 380 : 150);

      if (e.state === 'advance') {
        var target = sees ? p : (e.lastSeen || this.enemyWaypoint);
        var a = angleTo(e.x, e.y, target.x, target.y);
        // 无寻路：被掩体卡住时沿切线绕行（现实中的利用掩体跃进）
        if (e.detourT > 0) {
          e.detourT -= dt;
          a += (e.detourSign || 1) * 1.15;
        }
        e.facing = a;
        if (d > stopDist) {
          var sp = (e.role === 'elite' ? 78 : 64) * (1 + (1 - e.suppressionResist) * 0.15);
          if (e.suppression > 1.5) sp *= 0.7;
          var nxp = e.x + Math.cos(a) * sp * dt, nyp = e.y + Math.sin(a) * sp * dt;
          var ox = e.x, oy = e.y;
          e.x = this.resolveMove(e.x, e.y, nxp, nyp, e.r);
          e.y = this.resolveMoveY(e.x, e.y, nxp, nyp, e.r);
          var moved = dist(ox, oy, e.x, e.y);
          var intended = dist(ox, oy, nxp, nyp);
          if (e.detourT <= 0 && intended > 0.001 && moved < intended * 0.45) {
            e.detourT = 0.7 + this.rnd() * 0.9;
            e.detourSign = this.rnd() < 0.5 ? -1 : 1;
          }
        } else if (sees) {
          e.state = 'fire';
          e.aimTime = 0;
        }
      } else if (e.state === 'fire') {
        if (!sees) {
          if (e.lastSeen && (e.lastSeen.t = (e.lastSeen.t || 0), true)) {
            // 失去视线：向最后位置推进
            e.lastSeen.t += dt;
            if (e.lastSeen.t > 2.2) { e.state = 'advance'; }
          }
        } else {
          e.facing = angleTo(e.x, e.y, p.x, p.y);
          e.aimTime += dt;
          if (e.aimTime > e.reaction && e.cooldown <= 0) {
            this.enemyFire(e, d);
            e.cooldown = e.fireRate;
          }
        }
      } else if (e.state === 'cover') {
        // 就近寻找掩体
        if (!e.cover) e.cover = this.findCover(e.x, e.y, p.x, p.y);
        if (e.cover) {
          var ca = angleTo(e.x, e.y, e.cover.x, e.cover.y);
          e.x += Math.cos(ca) * 42 * dt; e.y += Math.sin(ca) * 42 * dt;
          if (dist(e.x, e.y, e.cover.x, e.cover.y) < 30) { e.cover = null; e.state = 'fire'; e.aimTime = 0; }
        } else e.state = 'fire';
      }
    }
  };

  Game.prototype.findCover = function (x, y, tx, ty) {
    var best = null, bd = 1e9;
    for (var i = 0; i < this.map.obstacles.length; i++) {
      var o = this.map.obstacles[i];
      if (!o.cover || o.cover < 0.5) continue;
      var ox = o.x + o.w / 2, oy = o.y + o.h / 2;
      var d = dist(x, y, ox, oy);
      if (d < 260 && d < bd) { bd = d; best = { x: ox, y: oy }; }
    }
    return best;
  };

  Game.prototype.enemyFire = function (e, d) {
    var p = this.player;
    var acc = e.accuracy;
    // 距离与压制惩罚
    var rangePenalty = clamp(1 - Math.max(0, d - 120) / 900, 0.25, 1);
    acc *= rangePenalty;
    acc *= 1 / (1 + e.suppression * 0.35);
    if (this.smokeAt(e.x, e.y)) acc *= 0.45;
    if (p.crouch) acc *= 0.8;
    if (p.speedMoving) acc *= 0.85;
    var hit = this.rnd() < acc;
    var a = angleTo(e.x, e.y, p.x, p.y) + (this.rnd() - 0.5) * 0.25;
    var speed = 1200;
    this.projectiles.push({
      x: e.x + Math.cos(a) * 14, y: e.y + Math.sin(a) * 14,
      vx: Math.cos(a) * speed, vy: Math.sin(a) * speed,
      dmg: e.dmg, pen: 5, life: 1.1, owner: 'enemy',
      tracer: true, willHit: hit, targetId: 'player', cat: 'rifle'
    });
    this.muzzleFlash(e.x + Math.cos(a) * 14, e.y + Math.sin(a) * 14, a);
    // 子弹擦过造成压制
    p.suppressed += 0.25;
  };

  Game.prototype.hasLOS = function (ax, ay, bx, by) {
    return !blocksLine(this.map.obstacles, ax, ay, bx, by);
  };

  Game.prototype.hurtPlayer = function (dmg, cause) {
    var p = this.player;
    if (!p.alive) return;
    // 掩体与护甲减免
    var reduce = 0;
    if (p.crouch) reduce += 0.12;
    if (p.gear['sn42_breastplate']) reduce += 0.35;
    if (p.gear['ssh40_helmet'] || p.gear['m1_helmet']) reduce += 0.08;
    reduce = Math.min(reduce, 0.62);
    var final = dmg * (1 - reduce);
    p.hp -= final;
    p.stats.damageTaken += final;
    if (cause === 'bullet' && this.rnd() < 0.3) p.bleeding = true;
    if (p.hp <= 0) this.playerDied();
    else if (final > 18) this.pushLog('中弹！生命 ' + Math.round(p.hp));  };

  /* ---- 载具 ---- */
  Game.prototype.updateVehicles = function (dt) {
    var p = this.player;
    for (var i = 0; i < this.vehicles.length; i++) {
      var v = this.vehicles[i];
      if (!v.alive) continue;
      if (v.immobilized > 0) v.immobilized -= dt;
      v.cooldown -= dt;
      var d = dist(v.x, v.y, p.x, p.y);
      if (!v.isEmplaced && v.immobilized <= 0 && d > v.engage * 0.7) {
        var a = angleTo(v.x, v.y, p.x, p.y);
        v.facing = a;
        var sp = 60;
        var nx = v.x + Math.cos(a) * sp * dt, ny = v.y + Math.sin(a) * sp * dt;
        v.x = this.resolveMove(v.x, v.y, nx, ny, v.r);
        v.y = this.resolveMoveY(v.x, v.y, nx, ny, v.r);
      } else v.facing = angleTo(v.x, v.y, p.x, p.y);
      if (d < v.engage && v.cooldown <= 0 && this.hasLOS(v.x, v.y, p.x, p.y)) {
        v.cooldown = v.fireRate * (2.5 + this.rnd() * 2);
        var ang = v.facing + (this.rnd() - 0.5) * 0.06;
        this.projectiles.push({
          x: v.x + Math.cos(ang) * 30, y: v.y + Math.sin(ang) * 30,
          vx: Math.cos(ang) * 900, vy: Math.sin(ang) * 900,
          dmg: v.dmg, pen: v.pen, life: 1.4, owner: 'enemy', tracer: true, heavy: true, cat: 'cannon'
        });
        this.muzzleFlash(v.x + Math.cos(ang) * 30, v.y + Math.sin(ang) * 30, ang, 2.2);
        this.pushLog(v.name + ' 开火！');
        p.suppressed += 1.4;
      }
    }
  };

  Game.prototype.damageVehicle = function (v, dmg, pen, hx, hy, hitAngle) {
    // 装甲判定：命中面朝向决定等效厚度
    var face = 'front';
    if (hitAngle !== undefined) {
      var rel = Math.abs(norm(hitAngle - (v.facing + Math.PI)));
      if (rel > 2.1) face = 'rear';
      else if (rel > 1.0) face = 'side';
    }
    var armor = v.armor * (face === 'front' ? 1 : (face === 'side' ? (v.sideRear || 0.5) : (v.sideRear || 0.5) * 0.7));
    if (pen < armor) {
      this.impactSpark(hx || v.x, hy || v.y, '跳弹（穿深 ' + Math.round(pen) + 'mm < 等效装甲 ' + Math.round(armor) + 'mm）');
      return false;
    }
    v.hp -= dmg * (pen > armor * 1.4 ? 1.15 : 1);
    if (this.rnd() < 0.22 && dmg > 40) v.immobilized = 6;
    this.impactSpark(hx || v.x, hy || v.y, '击穿！');
    if (v.hp <= 0) {
      v.alive = false;
      this.explosionFx(v.x, v.y, 48);
      this.kills++;
      this.player.stats.kills++;
      this.cp = clamp(this.cp + 0.6, 0, this.cpMax);
      this.pushLog('✔ 摧毁 ' + v.name);
      this.checkWaveClear();
    }
    return true;
  };

  Game.prototype.damageEnemy = function (e, dmg, cause) {
    if (!e.alive) return;
    e.hp -= dmg;
    e.suppression += 0.6;
    this.bloodFx(e.x, e.y);
    if (e.hp <= 0) {
      e.alive = false;
      this.kills++;
      this.player.stats.kills++;
      this.cp = clamp(this.cp + (e.role === 'leader' || e.role === 'elite' ? 0.5 : 0.2), 0, this.cpMax);
      if (this.rnd() < 0.25) {
        this.pickups.push({ x: e.x, y: e.y, type: this.rnd() < 0.5 ? 'ammo' : 'med', value: 20, ammo: 'rifle_762x54', label: '缴获弹药/绷带' });
      }
      this.checkWaveClear();
    }
  };

  /* ---- 友军 ---- */
  Game.prototype.updateAllies = function (dt) {
    for (var i = 0; i < this.allies.length; i++) {
      var a = this.allies[i];
      if (!a.alive) continue;
      a.cooldown -= dt;
      var target = this.nearestEnemy(a.x, a.y, a.kind === 'ally_tank' ? 900 : 320);
      if (target) {
        a.facing = angleTo(a.x, a.y, target.x, target.y);
        if (a.cooldown <= 0) {
          a.cooldown = a.kind === 'ally_tank' ? 6.5 : 1.6;
          var a1 = a.facing;
          if (a.kind === 'ally_tank') {
            this.damageVehicleOrEnemy(target, a.dmg, a.pen, a1);
            this.muzzleFlash(a.x + Math.cos(a1) * 28, a.y + Math.sin(a1) * 28, a1, 2.0);
          } else {
            this.damageEnemy(target, a.dmg * 0.6, 'bullet');
            this.projectiles.push({
              x: a.x, y: a.y, vx: Math.cos(a1) * 1100, vy: Math.sin(a1) * 1100,
              dmg: a.dmg, pen: 5, life: 0.4, owner: 'ally', tracer: true, cat: 'rifle'
            });
          }
        }
      } else if (a.kind === 'ally_inf' && this.player.alive) {
        var dp = dist(a.x, a.y, this.player.x, this.player.y);
        if (dp > 160) {
          var ang = angleTo(a.x, a.y, this.player.x, this.player.y);
          a.x += Math.cos(ang) * 55 * dt; a.y += Math.sin(ang) * 55 * dt;
        }
      }
    }
    this.allies = this.allies.filter(function (x) { return x.alive; });
    if (this.armorTimer > 0) {
      this.armorTimer -= dt;
      if (this.armorTimer <= 0) {
        var had = this.allies.some(function (a) { return a.kind === 'ally_tank'; });
        this.allies = this.allies.filter(function (a) { return a.kind !== 'ally_tank'; });
        if (had) this.pushLog('装甲支援油弹耗尽，按命令撤离战场');
      }
    }
  };

  Game.prototype.damageVehicleOrEnemy = function (t, dmg, pen, ang) {
    if (t.kind === 'vehicle') this.damageVehicle(t, dmg, pen, t.x, t.y, ang + Math.PI);
    else this.damageEnemy(t, dmg * 0.7, 'shell');
  };

  Game.prototype.nearestEnemy = function (x, y, maxD) {
    var best = null, bd = maxD;
    for (var i = 0; i < this.enemies.length; i++) {
      var e = this.enemies[i];
      if (!e.alive) continue;
      var d = dist(x, y, e.x, e.y);
      if (d < bd && this.hasLOS(x, y, e.x, e.y)) { bd = d; best = e; }
    }
    if (!best) {
      for (var j = 0; j < this.vehicles.length; j++) {
        var v = this.vehicles[j];
        if (!v.alive) continue;
        var dv = dist(x, y, v.x, v.y);
        if (dv < bd && this.hasLOS(x, y, v.x, v.y)) { bd = dv; best = v; }
      }
    }
    return best;
  };

  /* ---- 子弹 ---- */
  Game.prototype.updateProjectiles = function (dt) {
    var p = this.player;
    for (var i = this.projectiles.length - 1; i >= 0; i--) {
      var b = this.projectiles[i];
      var nx = b.x + b.vx * dt, ny = b.y + b.vy * dt;
      // 障碍
      var hitObs = null;
      for (var oi = 0; oi < this.map.obstacles.length; oi++) {
        var o = this.map.obstacles[oi];
        if (o.blocksSight === false && o.kind !== 'sandbag') continue;
        if (o.kind === 'sandbag' && !o.blocksSight) { /* 沙袋可挡子弹 */ }
        var hr = segRect(b.x, b.y, nx, ny, o);
        if (hr && hr.t <= 1) { hitObs = { o: o, t: hr.t }; break; }
      }
      if (hitObs && b.owner !== 'enemy') {
        this.impactSpark(nx, ny, hitObs.o.kind === 'sandbag' ? '沙袋' : '掩体');
        this.projectiles.splice(i, 1); continue;
      } else if (hitObs && b.owner === 'enemy' && p.alive) {
        // 敌人子弹被掩体挡住（玩家在掩体后）
        var blockedBefore = hitObs.t < 0.98;
        if (blockedBefore) { this.impactSpark(nx, ny, '掩体'); this.projectiles.splice(i, 1); continue; }
      }

      if (b.owner === 'enemy' || b.owner === 'vehicle') {
        // 命中玩家判定（连续碰撞）
        if (p.alive && segCircle(b.x, b.y, nx, ny, p.x, p.y, p.r + 3)) {
          if (b.willHit !== false) {
            this.hurtPlayer(b.dmg, 'bullet');
            if (b.heavy) this.explosionFx(nx, ny, 20);
            this.projectiles.splice(i, 1); continue;
          }
        }
      } else if (b.owner === 'player') {
        var hitSomething = false;
        for (var ei = 0; ei < this.enemies.length; ei++) {
          var e = this.enemies[ei];
          if (!e.alive) continue;
          if (segCircle(b.x, b.y, nx, ny, e.x, e.y, e.r + 3)) {
            this.damageEnemy(e, b.dmg, 'bullet');
            p.stats.hits++;
            hitSomething = true; break;
          }
        }
        if (!hitSomething) {
          for (var vi = 0; vi < this.vehicles.length; vi++) {
            var v = this.vehicles[vi];
            if (!v.alive) continue;
            if (segCircle(b.x, b.y, nx, ny, v.x, v.y, v.r)) {
              var hitAng = Math.atan2(b.vy, b.vx);
              this.damageVehicle(v, b.dmg, b.pen, nx, ny, hitAng);
              p.stats.hits++;
              hitSomething = true; break;
            }
          }
        }
        if (hitSomething) { this.projectiles.splice(i, 1); continue; }
      }

      b.x = nx; b.y = ny;
      b.life -= dt;
      if (b.life <= 0) this.projectiles.splice(i, 1);
    }
  };

  /* ---- 手榴弹 ---- */
  Game.prototype.updateGrenades = function (dt) {
    for (var i = this.grenades.length - 1; i >= 0; i--) {
      var g = this.grenades[i];
      g.fuse -= dt;
      var nx = g.x + g.vx * dt, ny = g.y + g.vy * dt;
      // 简易碰撞反弹
      var blocked = false;
      for (var oi = 0; oi < this.map.obstacles.length; oi++) {
        var o = this.map.obstacles[oi];
        if (o.blocksMove || o.kind === 'building' || o.kind === 'sandbag') {
          if (circleRect(nx, g.y, 4, o)) { g.vx *= -g.bounce; blocked = true; }
          if (circleRect(g.x, ny, 4, o)) { g.vy *= -g.bounce; blocked = true; }
        }
      }
      g.x = blocked ? g.x : nx; g.y = blocked ? g.y : ny;
      g.vx *= (1 - 1.6 * dt); g.vy *= (1 - 1.6 * dt);
      if (g.fuse <= 0) {
        var eff = g.item.effect || {};
        if (eff.smoke_duration_s) this.spawnSmoke(g.x, g.y, eff.radius_m || 12, eff.smoke_duration_s);
        if (eff.fire_duration_s) {
          this.particles.push({ kind: 'fire', x: g.x, y: g.y, r: (eff.radius_m || 4) * 6, t: eff.fire_duration_s, maxT: eff.fire_duration_s, dmg: 14 });
        }
        if (eff.damage) {
          this.explosionFx(g.x, g.y, (eff.radius_m || 6) * 5);
          this.applyExplosion(g.x, g.y, (eff.radius_m || 6) * 5, eff.damage, eff.fragments || 12, eff.penetration_mm || 0);
        }
        this.grenades.splice(i, 1);
      }
    }
    // 火焰持续伤害
    for (var j = this.particles.length - 1; j >= 0; j--) {
      var f = this.particles[j];
      if (f.kind === 'fire') {
        for (var ei = 0; ei < this.enemies.length; ei++) {
          var e = this.enemies[ei];
          if (e.alive && dist(e.x, e.y, f.x, f.y) < f.r * 0.55) this.damageEnemy(e, 14 * dt, 'fire');
        }
        if (this.player.alive && dist(this.player.x, this.player.y, f.x, f.y) < f.r * 0.5) this.hurtPlayer(10 * dt, 'fire');
      }
    }
  };

  Game.prototype.applyExplosion = function (x, y, radius, damage, fragments, pen) {
    var i;
    for (i = 0; i < this.enemies.length; i++) {
      var e = this.enemies[i];
      if (!e.alive) continue;
      var d = dist(x, y, e.x, e.y);
      if (d < radius) {
        var f = Math.pow(1 - d / radius, 1.6);
        this.damageEnemy(e, damage * f, 'grenade');
        e.suppression += 2 * f;
      }
    }
    for (i = 0; i < this.vehicles.length; i++) {
      var v = this.vehicles[i];
      if (!v.alive) continue;
      if (dist(x, y, v.x, v.y) < radius * 0.6 && pen > 0) this.damageVehicle(v, damage * 0.5, pen, x, y);
    }
    for (i = 0; i < this.allies.length; i++) {
      var a = this.allies[i];
      if (a.alive && dist(x, y, a.x, a.y) < radius) a.hp -= damage * 0.5;
    }
    if (this.player.alive && dist(x, y, this.player.x, this.player.y) < radius) {
      var pf = Math.pow(1 - dist(x, y, this.player.x, this.player.y) / radius, 1.5);
      this.hurtPlayer(damage * pf * 0.7, 'explosion');
      this.player.suppressed += 3;
      this.pushLog('⚠ 手榴弹破片波及自身！');
    }
  };

  /* ---- 粒子/特效（纯表现，不参与逻辑） ---- */
  Game.prototype.muzzleFlash = function (x, y, a, scale) {
    scale = scale || 1;
    this.particles.push({ kind: 'muzzle', x: x, y: y, a: a, t: 0.06, maxT: 0.06, s: scale });
  };
  Game.prototype.bloodFx = function (x, y) {
    this.particles.push({ kind: 'blood', x: x, y: y, t: 0.5, maxT: 0.5, r: 3 + this.rnd() * 3 });
  };
  Game.prototype.impactSpark = function (x, y, label) {
    this.particles.push({ kind: 'spark', x: x, y: y, t: 0.22, maxT: 0.22, label: label });
  };
  Game.prototype.explosionFx = function (x, y, r) {
    this.particles.push({ kind: 'boom', x: x, y: y, r: 8, targetR: r, t: 0.7, maxT: 0.7 });
    this.pushLog('爆炸（半径 ' + Math.round(r / 5) + 'm）');
  };
  Game.prototype.stepParticles = function (dt) {
    for (var i = this.particles.length - 1; i >= 0; i--) {
      var p = this.particles[i];
      p.t -= dt;
      if (p.kind === 'boom' && p.targetR) p.r = lerp(p.r, p.targetR, 1 - Math.pow(0.001, dt));
      if (p.kind === 'smoke' && p.targetR) p.r = lerp(p.r || 40, p.targetR, 1 - Math.pow(0.2, dt));
      if (p.t <= 0) this.particles.splice(i, 1);
    }
  };

  /* ---- 目标/波次 ---- */
  Game.prototype.checkWaveClear = function () {
    var aliveEnemies = this.enemies.filter(function (e) { return e.alive; }).length;
    var aliveVeh = this.vehicles.filter(function (v) { return v.alive; }).length;
    if (aliveEnemies === 0 && aliveVeh === 0) {
      if (this.mode === 'endless') { this.beginResupply(); return; }
      if (this.wave < this.wavesTotal) {
        this.wave++;
        this.spawnWave(this.wave);
        this.pushLog('推进：第 ' + this.wave + '/' + this.wavesTotal + ' 波');
      }
    }
  };

  Game.prototype.beginResupply = function () {
    this.phase = 'resupply';
    this.resupplyTimer = 45;
    this.cp = clamp(this.cp + 2, 0, this.cpMax);
    this.pushLog('◆ 波次间隙：45 秒整备。补充食物/水/弹药，构筑掩体。');
    // 波间补给点
    this.pickups.push({ x: this.player.x + 60, y: this.player.y, type: 'water', value: 70, label: '整备饮水' });
    this.pickups.push({ x: this.player.x - 60, y: this.player.y, type: 'food', value: 60, label: '整备口粮' });
    this.pickups.push({ x: this.player.x, y: this.player.y + 70, type: 'ammo', value: 60, ammo: this.profiles.primary ? this.profiles.primary.ammo : 'rifle_762x54', label: '整备弹药' });
  };

  Game.prototype.updateObjectives = function (dt) {
    if (this.phase === 'resupply') {
      this.resupplyTimer -= dt;
      if (this.resupplyTimer <= 0) {
        this.wave++;
        this.phase = 'combat';
        this.spawnWave(this.wave);
      }
      return;
    }
    // 战场清空时推进波次（敌人被火焰/炮击/逃散清除也会触发）
    if (this.phase === 'combat' && (this.mode === 'endless' || this.wave <= this.wavesTotal)) {
      var aliveNow = this.enemies.some(function (e) { return e.alive; }) || this.vehicles.some(function (v) { return v.alive; });
      if (!aliveNow) this.checkWaveClear();
    }
    for (var i = 0; i < this.objectives.length; i++) {
      var o = this.objectives[i];
      if (o.done) continue;
      if (o.kind === 'eliminate') {
        o.current = Math.min(o.target, this.kills);
        if (this.kills >= o.target) o.done = true;
      } else if (o.kind === 'hold') {
        o.current += dt;
        if (o.current >= o.target) o.done = true;
      } else if (o.kind === 'survive') {
        o.current += dt;
        if (o.current >= o.target) o.done = true;
      } else if (o.kind === 'reach') {
        var z = this.map.objectiveZone;
        if (this.player.x > z.x && this.player.x < z.x + z.w && this.player.y > z.y && this.player.y < z.y + z.h) o.done = true;
      } else if (o.kind === 'armor') {
        o.current = Math.min(o.target, this.vehicleKills || 0);
        if (o.current >= o.target) o.done = true;
      }
    }
    var requiredDone = this.objectives.filter(function (x) { return x.required; }).every(function (x) { return x.done; });
    var anyEnemyLeft = this.enemies.some(function (e) { return e.alive; }) || this.vehicles.some(function (v) { return v.alive; });
    if (requiredDone && !anyEnemyLeft && this.phase === 'combat') {
      this.phase = 'won';
      this.pushLog('✔ 作战目标达成：' + (this.level ? this.level.name_zh : '无限模式'));
    }
  };

  Game.prototype.updateCamera = function (dt, input) {
    var p = this.player;
    var tx = p.x, ty = p.y;
    if (input && input.aimWorld) { tx = lerp(p.x, input.aimWorld.x, 0.25); ty = lerp(p.y, input.aimWorld.y, 0.25); }
    this.camera.x = lerp(this.camera.x, tx, 1 - Math.pow(0.001, dt));
    this.camera.y = lerp(this.camera.y, ty, 1 - Math.pow(0.001, dt));
  };

  Game.prototype.progress = function () {
    var req = this.objectives.filter(function (o) { return o.required; });
    var done = req.filter(function (o) { return o.done; }).length;
    return { required: req.length, done: done, objectives: this.objectives };
  };

  RF.Game = Game;

  /* --------------------------- 无头测试接口 --------------------------- */

  RF.TestAPI = {
    create: function (data, opts) { return new Game(data, opts); },
    defaultLoadout: function (data, faction, classId) {
      return RF.autoLoadout(data, faction, classId);
    },
    step: function (game, dt, input) { game.step(dt, input); }
  };

  /* 自动配装：按派系与专长给一套不超载的默认装备 */
  RF.autoLoadout = function (data, faction, classId, budgetKg) {
    var caps = { soviet: 24, us: 26 };
    var capacity = budgetKg || caps[faction] || 24;
    var clsList = data.classes;
    var cls = clsList[classId] || clsList[faction === 'us' ? 'rifleman' : 'rifleman'];
    var weapons = Object.keys(data.weapons).map(function (k) { return data.weapons[k]; });
    var own = weapons.filter(function (w) { return w.faction === faction && (w.category !== 'sidearm'); });
    var sig = cls.signature_weapons || [];
    var prim = null;
    for (var i = 0; i < sig.length; i++) {
      var cand = data.weapons[sig[i]];
      if (cand && (cand.faction === faction || cand.faction === 'captured') && cand.category !== 'sidearm' && cand.category !== 'at_grenade_thrower') { prim = cand; break; }
    }
    if (!prim) prim = own.find(function (w) { return w.category === 'rifle_bolt' || w.category === 'rifle_semi'; }) || own[0];
    var sidearms = weapons.filter(function (w) { return w.category === 'sidearm' && w.faction === faction; });
    var sec = sidearms[0] || null;

    var eq = data.equipment;
    var gren = faction === 'soviet' ? ['rgd33', 'f1_grenade', 'rg42_smoke', 'molotov'] : ['mk2_grenade', 'anm8_smoke'];
    var food = faction === 'soviet' ? ['black_bread', 'canned_meat'] : ['k_ration', 'd_bar'];
    var medical = 'bandage';
    var loadout = {
      classId: cls.id, capacityKg: capacity, primary: prim ? prim.id : null, secondary: sec ? sec.id : null,
      grenades: {}, supplies: {}, gear: {}, radio: false
    };
    function tryAdd(bucket, id, count, slotLimit) {
      var it = eq[id]; if (!it) return false;
      var cur = loadout[bucket][id] || 0;
      if (cur >= slotLimit) return false;
      loadout[bucket][id] = cur + 1;
      return true;
    }
    // 手榴弹：2 种 × 2 个
    tryAdd('grenades', gren[0], 1, 2);
    tryAdd('grenades', gren[0], 1, 2);
    if (gren[2]) tryAdd('grenades', gren[2], 1, 1);
    // 食物 2 份 + 水 1 壶
    tryAdd('supplies', food[0], 1, 2);
    tryAdd('supplies', food[0], 1, 2);
    tryAdd('supplies', 'canteen_1l', 1, 1);
    // 医疗与护甲
    tryAdd('gear', medical, 1, 3);
    tryAdd('gear', faction === 'soviet' ? 'ssh40_helmet' : 'm1_helmet', 1, 1);
    tryAdd('gear', 'entrenching_tool', 1, 1);
    // 无线电台（呼叫支援必需）
    loadout.radio = true;
    var computed = RF.computeLoadout(loadout, data);
    // 超载则依次卸掉：工具 → 手榴弹 → 食物
    var guard = 0;
    while (computed.totalKg > capacity && guard++ < 20) {
      if (loadout.gear['entrenching_tool']) delete loadout.gear['entrenching_tool'];
      else if (loadout.grenades[gren[2]]) delete loadout.grenades[gren[2]];
      else if (loadout.supplies[food[0]] > 1) loadout.supplies[food[0]]--;
      else if (gren[1] && loadout.grenades[gren[1]] === undefined) { /* noop */ }
      else if (loadout.supplies[food[0]] > 0) delete loadout.supplies[food[0]];
      else if (loadout.secondary) loadout.secondary = null;
      else break;
      computed = RF.computeLoadout(loadout, data);
    }
    loadout.computed = computed;
    return loadout;
  };

})(typeof window !== 'undefined' ? window : globalThis);
