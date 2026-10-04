#!/usr/bin/env node
/* =============================================================================
 * validate-data.mjs — 数据完整性校验（引用、区间、枚举、唯一性、连续性）
 * 退出码：0 = 通过（允许仅有警告），1 = 存在错误
 * 用法：node tools/validate-data.mjs
 * ===========================================================================*/
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const DATA = path.join(ROOT, 'data');
const read = (p) => JSON.parse(fs.readFileSync(p, 'utf8'));

const errors = [];
const warnings = [];
const E = (m) => errors.push(m);
const W = (m) => warnings.push(m);

const weaponsFile = read(path.join(DATA, 'weapons.json'));
const classesFile = read(path.join(DATA, 'classes.json'));
const equipmentFile = read(path.join(DATA, 'equipment.json'));
const enemiesFile = read(path.join(DATA, 'enemies.json'));
const supportFile = read(path.join(DATA, 'support.json'));
const campaignsFile = read(path.join(DATA, 'campaigns.json'));

const weaponIds = new Set(weaponsFile.weapons.map((w) => w.id));
const classIds = new Set(classesFile.classes.map((c) => c.id));
const equipIds = new Set(equipmentFile.items.map((i) => i.id));
const enemyIds = new Set([...(enemiesFile.infantry || []), ...(enemiesFile.vehicles || []), ...(enemiesFile.air || [])].map((e) => e.id));
const callInIds = new Set((supportFile.call_ins || []).map((s) => s.id));
const campaignIds = new Set(campaignsFile.campaigns.map((c) => c.id));
const callInById = new Map((supportFile.call_ins || []).map((s) => [s.id, s]));
const campaignById = new Map(campaignsFile.campaigns.map((c) => [c.id, c]));

const MISSION = new Set(['defense', 'delay', 'breakthrough', 'assault', 'urban_clearing', 'river_crossing', 'armored', 'ambush', 'relief', 'sabotage', 'siege', 'amphibious']);
const TERRAIN = new Set(['city', 'forest', 'field', 'village', 'river', 'rail', 'industrial', 'fortress', 'trench']);
const WEATHER = new Set(['clear', 'rain', 'snow', 'fog', 'mud', 'storm']);
const TOD = new Set(['dawn', 'day', 'dusk', 'night']);
const FACTIONS = new Set(['soviet', 'us']);

/* ------------------------------ 键值唯一性 ------------------------------ */
function dupCheck(list, label) {
  const seen = new Set();
  for (const x of list) {
    if (!x.id) E(`${label}: 存在缺少 id 的条目`);
    else if (seen.has(x.id)) E(`${label}: 重复 id "${x.id}"`);
    seen.add(x.id);
  }
}
dupCheck(weaponsFile.weapons, 'weapons');
dupCheck(classesFile.classes, 'classes');
dupCheck(equipmentFile.items, 'equipment');
dupCheck([...(enemiesFile.infantry || []), ...(enemiesFile.vehicles || []), ...(enemiesFile.air || [])], 'enemies');
dupCheck(supportFile.call_ins || [], 'support');
dupCheck(campaignsFile.campaigns, 'campaigns');

/* -------------------------------- 武器表 -------------------------------- */
for (const w of weaponsFile.weapons) {
  for (const f of ['name_zh', 'category', 'caliber', 'weight_kg', 'damage', 'ammo_type']) {
    if (w[f] === undefined || w[f] === null || w[f] === '') E(`weapons/${w.id}: 缺少字段 ${f}`);
  }
  if (!(w.weight_kg > 0)) E(`weapons/${w.id}: weight_kg 必须 > 0`);
  if (!(w.damage >= 0)) E(`weapons/${w.id}: damage 非法`);
}

/* -------------------------------- 装备表 -------------------------------- */
for (const it of equipmentFile.items) {
  if (!it.category) E(`equipment/${it.id}: 缺少 category`);
  if (!(it.weight_kg >= 0)) E(`equipment/${it.id}: weight_kg 非法`);
}

/* -------------------------------- 专长表 -------------------------------- */
for (const c of classesFile.classes) {
  if (!c.slots || !c.slots.grenade || !c.slots.supply) E(`classes/${c.id}: slots 不完整`);
  if (!Array.isArray(c.passives) || c.passives.length !== 3) W(`classes/${c.id}: 被动数量应为 3（当前 ${c.passives ? c.passives.length : 0}）`);
  if (!c.active || !c.active.name_zh) E(`classes/${c.id}: 缺少主动技能`);
  for (const f of c.factions || []) if (!FACTIONS.has(f)) E(`classes/${c.id}: 未知派系 ${f}`);
  for (const wid of c.signature_weapons || []) if (!weaponIds.has(wid)) E(`classes/${c.id}: signature_weapons 引用不存在武器 ${wid}`);
}
const bands = (classesFile.weight_system && classesFile.weight_system.load_bands) || [];
if (bands.length !== 4) E(`classes.json: load_bands 应为 4 档（当前 ${bands.length}）`);

/* -------------------------------- 敌军表 -------------------------------- */
for (const e of [...(enemiesFile.infantry || []), ...(enemiesFile.vehicles || []), ...(enemiesFile.air || [])]) {
  if (!e.name_zh) E(`enemies/${e.id}: 缺少 name_zh`);
  if (!(e.hp > 0)) E(`enemies/${e.id}: hp 非法`);
  for (const f of e.fronts || []) if (f !== 'east' && f !== 'west') E(`enemies/${e.id}: fronts 非法值 ${f}`);
}
for (const v of enemiesFile.vehicles || []) {
  if (!(v.armor_mm >= 0)) E(`enemies/${v.id}: armor_mm 非法`);
}

/* -------------------------------- 支援表 -------------------------------- */
for (const s of supportFile.call_ins) {
  if (!(s.cp_cost >= 0)) E(`support/${s.id}: cp_cost 非法`);
  if (!(s.delay_s >= 0)) E(`support/${s.id}: delay_s 非法`);
  if (!(s.cooldown_s >= 0)) E(`support/${s.id}: cooldown_s 非法`);
  for (const b of s.best_against || []) {
    if (!enemyIds.has(b) && !['infantry', 'vehicle', 'fortification', 'building', 'mg_nest', 'at_gun', 'convoy', 'assembly_area', 'strongpoint', 'built_up_area', 'infantry_in_open', 'vehicle_column', 'soft_skin', 'light_tank', 'heavy_tank', 'heavy_tank_side', 'armor', 'pak40', 'fortification'].includes(b)) {
      W(`support/${s.id}: best_against 含未识别标签 "${b}"`);
    }
  }
}
const cp = supportFile.command_points || {};
if (!cp.start || !cp.max || cp.start > cp.max) E('support.json: command_points 配置非法');

/* -------------------------------- 战役表 -------------------------------- */
for (const c of campaignsFile.campaigns) {
  if (!c.id || !c.name_zh || !c.start_date || !c.end_date) E(`campaigns/${c.id}: 缺少必填字段`);
  if (c.start_date > c.end_date) E(`campaigns/${c.id}: start_date 晚于 end_date`);
  if (!Array.isArray(c.levels) || c.levels.length !== 2) E(`campaigns/${c.id}: levels 应为 [起, 止]`);
}

/* -------------------------------- 关卡表 -------------------------------- */
const levelDir = path.join(DATA, 'levels');
const files = fs.existsSync(levelDir) ? fs.readdirSync(levelDir).filter((f) => f.endsWith('.json')).sort() : [];
let levels = [];
for (const f of files) {
  let doc;
  try { doc = read(path.join(levelDir, f)); } catch (e) { E(`${f}: JSON 解析失败 — ${e.message}`); continue; }
  if (!doc.campaign_id || !campaignIds.has(doc.campaign_id)) E(`${f}: campaign_id "${doc.campaign_id}" 不在 campaigns.json 中`);
  if (!Array.isArray(doc.levels) || !doc.levels.length) { E(`${f}: levels 数组为空`); continue; }
  for (const lv of doc.levels) {
    levels.push({ ...lv, _file: f });
    const where = `${f}#${lv.index || '?'} (${lv.id || '无 id'})`;
    if (lv.campaign_id !== doc.campaign_id) E(`${where}: campaign_id 与文件顶层不一致`);
    if (!/^[a-z]{2,3}_\d{2}_[a-z0-9_]+$/.test(lv.id || '')) E(`${where}: id 命名不符合 <faction>_<NN>_<slug>`);
    if (!FACTIONS.has(lv.faction)) E(`${where}: faction 非法`);
    if (!MISSION.has(lv.mission_type)) E(`${where}: mission_type 非法 "${lv.mission_type}"`);
    if (!TERRAIN.has(lv.terrain)) E(`${where}: terrain 非法 "${lv.terrain}"`);
    if (!WEATHER.has(lv.weather)) E(`${where}: weather 非法 "${lv.weather}"`);
    if (!TOD.has(lv.time_of_day)) E(`${where}: time_of_day 非法 "${lv.time_of_day}"`);
    if (!(lv.visibility_m >= 50 && lv.visibility_m <= 1200)) E(`${where}: visibility_m 越界 (${lv.visibility_m})`);
    if (!(lv.difficulty >= 1 && lv.difficulty <= 10)) E(`${where}: difficulty 越界 (${lv.difficulty})`);
    if (!(lv.par_time_s >= 600 && lv.par_time_s <= 2400)) E(`${where}: par_time_s 越界 (${lv.par_time_s})`);
    if (!(lv.weight_budget_kg >= 15 && lv.weight_budget_kg <= 40)) E(`${where}: weight_budget_kg 越界 (${lv.weight_budget_kg})`);
    if (!(lv.resupply_points >= 0 && lv.resupply_points <= 4)) E(`${where}: resupply_points 越界 (${lv.resupply_points})`);
    if (!/^\d{4}-\d{2}-\d{2}$/.test(lv.date || '')) E(`${where}: date 格式错误`);
    else {
      const camp = campaignById.get(lv.campaign_id);
      if (camp && (lv.date < camp.start_date || lv.date > camp.end_date)) {
        E(`${where}: date ${lv.date} 超出战役 ${lv.campaign_id} 区间 ${camp.start_date}..${camp.end_date}`);
      }
    }
    if (!lv.objectives || !lv.objectives.length) E(`${where}: 缺少 objectives`);
    else {
      if (lv.objectives.length < 2 || lv.objectives.length > 5) W(`${where}: objectives 数量 ${lv.objectives.length}（建议 2-5）`);
      for (const o of lv.objectives) {
        if (!o.id || !o.text_zh) E(`${where}: objective 缺少 id/text_zh`);
        if (o.type !== 'primary' && o.type !== 'secondary') E(`${where}: objective "${o.id}" type 非法`);
        if (o.condition != null
          && !['interact', 'hold_position', 'eliminate_count', 'reach_location', 'rescue_count', 'destroy_target'].includes(o.condition)) {
          E(`${where}: objective "${o.id}" condition 非法`);
        }
        if (o.condition === 'hold_position') {
          const holdTimeS = Number(o.hold_time_s);
          const radiusM = Number(o.radius_m);
          const timeLimitS = o.time_limit_s == null ? null : Number(o.time_limit_s);
          if (!Number.isFinite(holdTimeS) || holdTimeS <= 0
            || !Number.isFinite(radiusM) || radiusM < 1 || radiusM > 50) {
            E(`${where}: objective "${o.id}" hold_position 需要正数 hold_time_s 和 1-50 米 radius_m`);
          }
          if (timeLimitS != null
            && (!Number.isFinite(timeLimitS) || timeLimitS <= holdTimeS)) {
            E(`${where}: objective "${o.id}" time_limit_s 必须大于 hold_time_s`);
          }
        } else if (o.hold_time_s != null) {
          E(`${where}: objective "${o.id}" 只有 hold_position 可设置 hold_time_s`);
        } else if (o.radius_m != null && o.condition !== 'reach_location') {
          E(`${where}: objective "${o.id}" 只有 hold_position/reach_location 可设置 radius_m`);
        }
        if (o.condition === 'eliminate_count' || o.condition === 'rescue_count'
          || o.condition === 'destroy_target') {
          if (!Number.isInteger(o.target_count) || o.target_count <= 0) {
            E(`${where}: objective "${o.id}" ${o.condition} 需要正整数 target_count`);
          }
        }
        if (o.condition === 'eliminate_count') {
          if (o.target_enemy_ids != null && !Array.isArray(o.target_enemy_ids)) {
            E(`${where}: objective "${o.id}" target_enemy_ids 必须是数组`);
          } else {
            for (const enemyId of o.target_enemy_ids || []) {
              if (!enemyIds.has(enemyId)) {
                E(`${where}: objective "${o.id}" target_enemy_ids 引用不存在敌军 ${enemyId}`);
              }
            }
            const roster = lv.enemy_composition || {};
            const eligibleCount = (o.target_enemy_ids || []).length > 0
              ? o.target_enemy_ids.reduce((count, enemyId) => count + (roster[enemyId] || 0), 0)
              : Object.values(roster).reduce((count, quantity) => count + quantity, 0);
            if (Number.isInteger(o.target_count) && eligibleCount < o.target_count) {
              E(`${where}: objective "${o.id}" 目标数量 ${o.target_count} 超过敌军合同配置数 ${eligibleCount}`);
            }
          }
        } else if (o.condition !== 'rescue_count' && o.condition !== 'destroy_target'
          && (o.target_count != null || o.target_enemy_ids != null)) {
          E(`${where}: objective "${o.id}" 只有 eliminate_count/rescue_count 可设置 target_count`);
        } else if (o.condition !== 'eliminate_count' && o.target_enemy_ids != null) {
          E(`${where}: objective "${o.id}" ${o.condition} 不可设置 target_enemy_ids`);
        }
        if (o.condition === 'rescue_count') {
          if (!Array.isArray(o.rescue_points) || o.rescue_points.length < o.target_count) {
            E(`${where}: objective "${o.id}" rescue_count 需要不少于 target_count 个 rescue_points`);
          } else {
            const pointKeys = new Set();
            for (const [index, point] of o.rescue_points.entries()) {
              if (!point || typeof point !== 'object'
                || typeof point.x_m !== 'number' || !Number.isFinite(point.x_m)
                || typeof point.y_m !== 'number' || !Number.isFinite(point.y_m)) {
                E(`${where}: objective "${o.id}" rescue_points[${index}] 需要有限的 x_m/y_m`);
                continue;
              }
              const key = `${point.x_m},${point.y_m}`;
              if (pointKeys.has(key)) E(`${where}: objective "${o.id}" rescue_points 存在重复坐标 ${key}`);
              pointKeys.add(key);
            }
          }
        } else if (o.rescue_points != null) {
          E(`${where}: objective "${o.id}" 只有 rescue_count 可设置 rescue_points`);
        }
        if (o.condition === 'destroy_target') {
          if (!Array.isArray(o.target_points) || o.target_points.length < o.target_count) {
            E(`${where}: objective "${o.id}" destroy_target 需要不少于 target_count 个 target_points`);
          } else {
            for (const [index, point] of o.target_points.entries()) {
              if (!point || typeof point !== 'object'
                || typeof point.x_m !== 'number' || !Number.isFinite(point.x_m)
                || typeof point.y_m !== 'number' || !Number.isFinite(point.y_m)
                || (point.health != null
                  && (typeof point.health !== 'number' || !Number.isFinite(point.health) || point.health <= 0))
                || (point.name_zh != null
                  && (typeof point.name_zh !== 'string' || point.name_zh.trim() === ''))) {
                E(`${where}: objective "${o.id}" target_points[${index}] 需要有限坐标、正数 health 和非空 name_zh`);
              }
            }
          }
        } else if (o.target_points != null) {
          E(`${where}: objective "${o.id}" 只有 destroy_target 可设置 target_points`);
        }
        if (o.condition === 'reach_location'
          && (!Number.isFinite(Number(o.radius_m)) || Number(o.radius_m) < 1 || Number(o.radius_m) > 50)) {
          E(`${where}: objective "${o.id}" reach_location 需要 1-50 米 radius_m`);
        }
        if ((o.marker_x_m == null) !== (o.marker_y_m == null)
          || (o.marker_x_m != null
            && (!Number.isFinite(Number(o.marker_x_m)) || !Number.isFinite(Number(o.marker_y_m))))) {
          E(`${where}: objective "${o.id}" marker_x_m/marker_y_m 必须同时为有限数值`);
        }
      }
      if (!lv.objectives.some((o) => o.type === 'primary')) E(`${where}: 缺少 primary 目标`);
    }
    const comp = lv.enemy_composition || {};
    const compKeys = Object.keys(comp);
    if (!compKeys.length) E(`${where}: enemy_composition 为空`);
    for (const k of compKeys) {
      if (!enemyIds.has(k)) E(`${where}: enemy_composition 引用不存在敌军 "${k}"`);
      if (!Number.isInteger(comp[k]) || comp[k] <= 0) E(`${where}: enemy_composition["${k}"] 必须为正整数`);
    }
    if (compKeys.length < 3) W(`${where}: enemy_composition 只有 ${compKeys.length} 种（建议 3-8）`);
    for (const s of lv.available_support || []) {
      if (!callInIds.has(s)) { E(`${where}: available_support 引用不存在支援 "${s}"`); continue; }
      const def = callInById.get(s);
      if (def.faction !== 'both' && def.faction !== lv.faction) E(`${where}: 支援 "${s}" 属于 ${def.faction}，与关卡派系 ${lv.faction} 不匹配`);
    }
    if ((lv.available_support || []).length > 4) E(`${where}: available_support 超过 4 项`);
    for (const v of lv.vehicle_available || []) {
      if (!callInIds.has(v) && !enemyIds.has(v)) E(`${where}: vehicle_available 引用未知 id "${v}"`);
    }
    for (const c of lv.player_class_recommended || []) if (!classIds.has(c)) E(`${where}: player_class_recommended 引用不存在专长 "${c}"`);
    const acc = lv.historical_accuracy || {};
    if (!Array.isArray(acc.verified_events) || !acc.verified_events.length) E(`${where}: historical_accuracy.verified_events 缺失`);
    if (!Array.isArray(acc.dramatized)) E(`${where}: historical_accuracy.dramatized 缺失`);
    const noteLen = (lv.historical_note_zh || '').length;
    if (noteLen < 150 || noteLen > 300) E(`${where}: historical_note_zh 长度 ${noteLen}（要求 150-260，容差 300）`);
    const designLen = (lv.design_note_zh || '').length;
    if (designLen < 60 || designLen > 200) E(`${where}: design_note_zh 长度 ${designLen}（要求 80-150，容差 200）`);
    const rw = lv.unlock_reward || {};
    if (!(rw.xp >= 0)) E(`${where}: unlock_reward.xp 非法`);
    if (rw.weapon && !weaponIds.has(rw.weapon)) E(`${where}: unlock_reward.weapon 引用不存在武器 "${rw.weapon}"`);
    if (rw.class && !classIds.has(rw.class)) E(`${where}: unlock_reward.class 引用不存在专长 "${rw.class}"`);
    const esm = lv.endless_seed_modifier || {};
    if (![1941, 1942, 1943, 1945].includes(esm.era)) E(`${where}: endless_seed_modifier.era 非法 (${esm.era})`);
    if (!(esm.weight_mult >= 0.8 && esm.weight_mult <= 1.3)) E(`${where}: endless_seed_modifier.weight_mult 越界 (${esm.weight_mult})`);
    if (!lv.terrain || !lv.player_role_zh || !lv.enemy_tactics_zh || !lv.friendly_forces_zh || !lv.music_mood) E(`${where}: 缺少必填文本字段`);
    for (const f of ['name_zh', 'location_zh', 'coordinates', 'name_en', 'location_en']) if (!lv[f]) E(`${where}: 缺少字段 ${f}`);
  }
}

/* --------------------------- 全局连续性 / 唯一性 --------------------------- */
levels.sort((a, b) => a.index - b.index);
const idSeen = new Map();
const noteSeen = new Map();
for (const lv of levels) {
  if (idSeen.has(lv.id)) E(`全局重复关卡 id "${lv.id}"（${idSeen.get(lv.id)} 与 ${lv._file}）`);
  idSeen.set(lv.id, lv._file);
  const n = lv.historical_note_zh;
  if (n) {
    if (noteSeen.has(n)) E(`关卡 ${lv.id} 的 historical_note_zh 与 ${noteSeen.get(n)} 完全相同`);
    noteSeen.set(n, lv.id);
  }
}
const expected = Array.from({ length: levels.length }, (_, i) => i + 1);
const got = levels.map((l) => l.index);
for (let i = 0; i < expected.length; i++) {
  if (got[i] !== expected[i]) { E(`关卡 index 不连续：第 ${i + 1} 位应为 ${expected[i]}，实际 ${got[i]}`); break; }
}
if (new Set(got).size !== got.length) E('存在重复的关卡 index');

/* 战役覆盖一致性 */
for (const c of campaignsFile.campaigns) {
  const [a, b] = c.levels;
  const inCamp = levels.filter((l) => l.campaign_id === c.id).length;
  if (inCamp !== b - a + 1) W(`战役 ${c.id} 声明 ${a}-${b}（${b - a + 1} 关），实际 ${inCamp} 关`);
}
/* 每个战役至少一关使用该战役 id */
for (const lv of levels) if (!campaignIds.has(lv.campaign_id)) E(`关卡 ${lv.id} 的 campaign_id 不存在`);

/* 三连相同 mission_type */
let run = 1;
for (let i = 1; i < levels.length; i++) {
  if (levels[i].mission_type === levels[i - 1].mission_type) run++; else run = 1;
  if (run >= 3) W(`关卡 ${levels[i].id} 起连续 ${run} 关 mission_type 相同（${levels[i].mission_type}）`);
}

/* -------------------------------- 覆盖统计 -------------------------------- */
const usedWeapons = new Set();
for (const lv of levels) {
  const rw = lv.unlock_reward || {};
  if (rw.weapon) usedWeapons.add(rw.weapon);
}
for (const c of classesFile.classes) for (const w of c.signature_weapons || []) usedWeapons.add(w);
const unusedWeapons = [...weaponIds].filter((w) => !usedWeapons.has(w));

/* --------------------------------- 输出 --------------------------------- */
const line = '─'.repeat(64);
console.log(line);
console.log('RedFront 1941 — 数据校验报告');
console.log(line);
console.log(`武器 ${weaponIds.size} · 专长 ${classIds.size} · 装备 ${equipIds.size} · 敌军 ${enemyIds.size} · 支援 ${callInIds.size} · 战役 ${campaignIds.size} · 关卡 ${levels.length}`);
if (levels.length) {
  const byCamp = {};
  for (const l of levels) byCamp[l.campaign_id] = (byCamp[l.campaign_id] || 0) + 1;
  for (const c of campaignsFile.campaigns) console.log(`  · ${c.name_zh.padEnd(30)} ${String(byCamp[c.id] || 0).padStart(2)} 关  ${c.start_date} → ${c.end_date}`);
  const diffs = levels.map((l) => l.difficulty);
  const types = {};
  for (const l of levels) types[l.mission_type] = (types[l.mission_type] || 0) + 1;
  console.log(`  难度 1-10 分布：最低 ${Math.min(...diffs)} / 平均 ${(diffs.reduce((a, b) => a + b, 0) / diffs.length).toFixed(1)} / 最高 ${Math.max(...diffs)}`);
  console.log(`  任务类型：` + Object.entries(types).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k}×${v}`).join(' '));
  console.log(`  历史注记平均长度：${Math.round(levels.reduce((a, l) => a + l.historical_note_zh.length, 0) / levels.length)} 字`);
}
if (unusedWeapons.length) console.log(`\n未被任何关卡解锁或专长推荐的武器（${unusedWeapons.length}）：${unusedWeapons.join(', ')}`);
if (warnings.length) {
  console.log(`\n⚠ 警告 ${warnings.length} 条：`);
  warnings.slice(0, 25).forEach((w) => console.log('  · ' + w));
  if (warnings.length > 25) console.log(`  … 其余 ${warnings.length - 25} 条省略`);
}
if (errors.length) {
  console.log(`\n✖ 错误 ${errors.length} 条：`);
  errors.slice(0, 40).forEach((e) => console.log('  · ' + e));
  if (errors.length > 40) console.log(`  … 其余 ${errors.length - 40} 条省略`);
  console.log('\n校验失败。');
  process.exit(1);
}
console.log('\n✔ 校验通过：所有引用、枚举与区间有效。');
