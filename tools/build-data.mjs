#!/usr/bin/env node
/* =============================================================================
 * build-data.mjs — 从 data/*.json 生成：
 *   1) data/generated/levels.json          合并后的 56 关
 *   2) data/generated/*.csv                UE5 DataTable 可直接导入的 CSV
 *   3) prototype/js/data.generated.js      原型使用的 window.RF_DATA
 *   4) Unreal/Content/RedFront/Data/       UE 开发运行时 JSON 合约镜像
 * 用法：node tools/build-data.mjs
 * ===========================================================================*/
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const DATA = path.join(ROOT, 'data');
const GEN = path.join(DATA, 'generated');
const PROTO = path.join(ROOT, 'prototype', 'js');
const UNREAL_DATA = path.join(ROOT, 'Unreal', 'Content', 'RedFront', 'Data');

const readJson = (p) => JSON.parse(fs.readFileSync(p, 'utf8'));
const log = (...a) => console.log(...a);

/* ------------------------------- CSV 工具 ------------------------------- */
function csvCell(v) {
  if (v === null || v === undefined) return '';
  let s;
  if (typeof v === 'object') s = JSON.stringify(v);
  else s = String(v);
  if (/[",\n\r]/.test(s)) s = '"' + s.replace(/"/g, '""') + '"';
  return s;
}
function toCsv(rows, fields) {
  const head = ['Name', ...fields].join(',');
  const body = rows.map((r) => fields.map((f) => csvCell(r[f])).join(',')).join('\n');
  return head + '\n' + body + '\n';
}
function writeCsv(file, rows, fields) {
  fields = [...new Set(fields)]; // 去重：展开列可能与基础列同名
  const head = ['Name', ...fields].join(',');
  const body = rows.map((r) => fields.map((f) => csvCell(r[f])).join(',')).join('\n');
  fs.writeFileSync(path.join(GEN, file), head + '\n' + body + '\n', 'utf8');
  return rows.length;
}

/* ------------------------------- 读入数据 ------------------------------- */
fs.mkdirSync(GEN, { recursive: true });
fs.mkdirSync(PROTO, { recursive: true });

const weaponsArr = readJson(path.join(DATA, 'weapons.json')).weapons;
const classesFile = readJson(path.join(DATA, 'classes.json'));
const classesArr = classesFile.classes;
const equipmentArr = readJson(path.join(DATA, 'equipment.json')).items;
const enemiesFile = readJson(path.join(DATA, 'enemies.json'));
const supportFile = readJson(path.join(DATA, 'support.json'));
const campaignsFile = readJson(path.join(DATA, 'campaigns.json'));

/* 合并关卡 */
const levelDir = path.join(DATA, 'levels');
let levels = [];
const levelFiles = fs.existsSync(levelDir)
  ? fs.readdirSync(levelDir).filter((f) => f.endsWith('.json')).sort()
  : [];
for (const f of levelFiles) {
  const doc = readJson(path.join(levelDir, f));
  const arr = doc.levels || (Array.isArray(doc) ? doc : []);
  for (const lv of arr) levels.push({ ...lv, _file: f });
  log(`  · ${f.padEnd(34)} ${String(arr.length).padStart(2)} 关`);
}
levels.sort((a, b) => a.index - b.index);

/* --------------------------- 键值化（原型用） --------------------------- */
const byId = (arr) => arr.reduce((m, x) => { m[x.id] = x; return m; }, {});
const weapons = byId(weaponsArr);
const classes = byId(classesArr);
const equipment = byId(equipmentArr);
const enemies = {
  infantry: enemiesFile.infantry || [],
  vehicles: enemiesFile.vehicles || [],
  air: enemiesFile.air || []
};

const RF_DATA = {
  meta: {
    generatedAt: new Date().toISOString().slice(0, 16).replace('T', ' '),
    generator: 'tools/build-data.mjs',
    weapons: weaponsArr.length,
    classes: classesArr.length,
    equipment: equipmentArr.length,
    enemies: enemies.infantry.length + enemies.vehicles.length + enemies.air.length,
    callIns: (supportFile.call_ins || []).length,
    levels: levels.length,
    campaigns: campaignsFile.campaigns.length
  },
  campaigns: campaignsFile.campaigns,
  difficulty: campaignsFile.difficulty_scaling,
  levels: levels.map((l) => { const c = { ...l }; delete c._file; return c; }),
  weapons,
  classes: Object.assign({
    base_capacity_kg: (classesFile.weight_system || {}).base_capacity_kg,
    us_capacity_kg: (classesFile.weight_system || {}).us_capacity_kg,
    load_bands: (classesFile.weight_system || {}).load_bands
  }, classes),
  equipment,
  enemies,
  support: supportFile
};

fs.writeFileSync(path.join(GEN, 'levels.json'), JSON.stringify({ $schema: 'redfront.levels.merged/1', count: RF_DATA.levels.length, levels: RF_DATA.levels }, null, 2), 'utf8');
fs.writeFileSync(path.join(PROTO, 'data.generated.js'),
  '/* 自动生成，请勿手改：node tools/build-data.mjs */\nwindow.RF_DATA = ' + JSON.stringify(RF_DATA) + ';\n', 'utf8');

/* --------------------------------- CSV --------------------------------- */
/* UE 的 Tools > Import DataTable 只能按列 1:1 映射结构体字段，因此把嵌套对象
   展开成标量列（blob 列仍保留，用于无损的数据管线与运行时 JSON 加载器）。 */
function expand(rows, specs) {
  const out = rows.map((r) => {
    const o = { ...r };
    for (const s of specs) {
      if (s.kind === 'object') {
        const src = r[s.from] || {};
        for (const [k, v] of Object.entries(src)) o[s.prefix + k] = (v && typeof v === 'object') ? JSON.stringify(v) : v;
      } else if (s.kind === 'array') {
        const arr = Array.isArray(r[s.from]) ? r[s.from] : [];
        for (let i = 0; i < s.max; i++) {
          const it = arr[i] || {};
          if (typeof it === 'object') for (const [k, v] of Object.entries(it)) o[`${s.prefix}${i + 1}_${k}`] = (v && typeof v === 'object') ? JSON.stringify(v) : v;
          else o[`${s.prefix}${i + 1}`] = it;
        }
      }
    }
    return o;
  });
  const extra = [];
  for (const o of out) for (const k of Object.keys(o)) if (!extra.includes(k) && !k.startsWith('_') && !(k in (rows[0] || {}))) extra.push(k);
  return { rows: out, extra };
}

const nWeapon = writeCsv('weapons.csv', weaponsArr, [
  'id', 'name_zh', 'name_en', 'faction', 'category', 'caliber', 'damage', 'rpm', 'magazine', 'reload_s',
  'weight_kg', 'accuracy', 'effective_range_m', 'penetration_mm', 'mobility_mod', 'ammo_type', 'unlock_level', 'realism', 'note'
]);

const classBase = ['id', 'name_zh', 'name_en', 'factions', 'slot_primary', 'slot_secondary', 'slot_grenade', 'slot_supply', 'unlock_level', 'passives', 'active'];
const classRows = classesArr.map((c) => ({
  ...c, factions: c.factions.join('|'),
  slot_primary: c.slots.primary, slot_secondary: c.slots.secondary,
  slot_grenade: c.slots.grenade, slot_supply: c.slots.supply
}));
const classExp = expand(classRows, [{ kind: 'array', from: 'passives', prefix: 'passive_', max: 3 }, { kind: 'object', from: 'active', prefix: 'active_' }]);
const nClass = writeCsv('classes.csv', classExp.rows, [...classBase, ...classExp.extra]);

const equipBase = ['id', 'category', 'name_zh', 'name_en', 'faction', 'weight_kg', 'count_per_slot', 'capacity', 'realism', 'note', 'effect'];
const equipExp = expand(equipmentArr, [{ kind: 'object', from: 'effect', prefix: 'effect_' }]);
const nEquip = writeCsv('equipment.csv', equipExp.rows, [...equipBase, ...equipExp.extra]);

const infantryRows = enemies.infantry.map((e) => ({
  ...e, fronts: e.fronts.join('|'), ai: e.ai,
  awareness_m: e.ai.awareness_m, reaction_s: e.ai.reaction_s,
  suppression_resist: e.ai.suppression_resist, calls_support: !!e.ai.calls_support
}));
const infantryExp = expand(infantryRows, [{ kind: 'object', from: 'ai', prefix: 'ai_' }]);
const nInf = writeCsv('enemies_infantry.csv', infantryExp.rows, ['id', 'name_zh', 'name_en', 'faction', 'fronts', 'hp', 'armor_value', 'weapon', 'damage', 'fire_rate_s', 'accuracy', 'role', 'threat', 'awareness_m', 'reaction_s', 'suppression_resist', 'calls_support', 'note', ...infantryExp.extra]);

const vehicleRows = enemies.vehicles.map((v) => ({ ...v, fronts: v.fronts.join('|'), ai: v.ai, engage_range_m: v.ai.engage_range_m, emplaced: !!v.ai.emplaced }));
const nVeh = writeCsv('enemies_vehicles.csv', vehicleRows, ['id', 'name_zh', 'name_en', 'faction', 'fronts', 'armor_mm', 'side_rear_ratio', 'hp', 'weapon', 'penetration_mm', 'speed_kph', 'engage_range_m', 'emplaced', 'threat', 'first_seen', 'note']);
const airRows = enemies.air.map((a) => ({ ...a, fronts: a.fronts.join('|'), ai: a.ai }));
const nAir = writeCsv('enemies_air.csv', airRows, ['id', 'name_zh', 'name_en', 'faction', 'fronts', 'hp', 'weapon', 'threat', 'note']);
const callIns = supportFile.call_ins || [];
const callExp = expand(callIns, [{ kind: 'object', from: 'ai', prefix: 'ai_' }]);
const nCall = writeCsv('support_callins.csv', callExp.rows, ['id', 'name_zh', 'name_en', 'faction', 'type', 'cp_cost', 'cooldown_s', 'delay_s', 'radius_m', 'rounds', 'damage_per_round', 'shell', 'spread_base_m', 'units', 'hp_each', 'weapon', 'penetration_mm', 'requires', 'best_against', 'realism', 'note', ...callExp.extra]);
const campRows = campaignsFile.campaigns.map((c) => ({ ...c, levels: c.levels.join('-') }));
const nCamp = writeCsv('campaigns.csv', campRows, ['id', 'name_zh', 'name_en', 'faction', 'front', 'levels', 'start_date', 'end_date', 'tone', 'player_arc_zh', 'mechanics_focus', 'unlock_progression']);

const levelBase = ['index', 'id', 'number_in_campaign', 'campaign_id', 'name_zh', 'name_en', 'date', 'location_zh', 'location_en', 'coordinates', 'mission_type', 'faction', 'player_role_zh', 'player_class_recommended', 'enemy_tactics_zh', 'friendly_forces_zh', 'terrain', 'weather', 'time_of_day', 'visibility_m', 'difficulty', 'par_time_s', 'weight_budget_kg', 'resupply_points', 'available_support', 'vehicle_available', 'historical_note_zh', 'design_note_zh', 'music_mood', 'author_note',
  'enemy_total', 'enemy_types', 'support_count', 'primary_objective_zh', 'primary_objective_count',
  'objectives', 'enemy_composition', 'historical_accuracy', 'unlock_reward', 'endless_seed_modifier'];
const levelRowsPre = levels.map((l) => {
  const prim = (l.objectives || []).filter((o) => o.type === 'primary');
  return {
    ...l,
    available_support: (l.available_support || []).join('|'),
    vehicle_available: (l.vehicle_available || []).join('|'),
    player_class_recommended: (l.player_class_recommended || []).join('|'),
    enemy_total: Object.values(l.enemy_composition || {}).reduce((a, b) => a + b, 0),
    enemy_types: Object.keys(l.enemy_composition || {}).length,
    support_count: (l.available_support || []).length,
    primary_objective_zh: prim.length ? prim[0].text_zh : '',
    primary_objective_count: prim.length
  };
});
const levelExp = expand(levelRowsPre, [
  { kind: 'array', from: 'objectives', prefix: 'objective_', max: 5 },
  { kind: 'object', from: 'unlock_reward', prefix: 'reward_' },
  { kind: 'object', from: 'endless_seed_modifier', prefix: 'endless_' }
]);
const nLevel = writeCsv('levels.csv', levelExp.rows, [...levelBase, ...levelExp.extra]);

fs.writeFileSync(path.join(GEN, 'README.md'), `# data/generated —— 自动生成，请勿手改

由 \`node tools/build-data.mjs\` 生成。

| 文件 | 行数 | UE 目标 DataTable |
|---|---|---|
| weapons.csv | ${nWeapon} | DT_Weapons（行结构 FRFWeaponDef） |
| classes.csv | ${nClass} | DT_Classes |
| equipment.csv | ${nEquip} | DT_Equipment |
| enemies_infantry.csv | ${nInf} | DT_Enemies_Infantry |
| enemies_vehicles.csv | ${nVeh} | DT_Enemies_Vehicles |
| enemies_air.csv | ${nAir} | DT_Enemies_Air |
| support_callins.csv | ${nCall} | DT_SupportCallIns |
| campaigns.csv | ${nCamp} | DT_Campaigns |
| levels.csv | ${nLevel} | DT_Levels |
| levels.json | ${RF_DATA.levels.length} | 运行时可读的合并关卡表 |

## 导入 UE 5.8
1. 打开工程 → Content Browser → \`Content/RedFront/Data\`。
2. 右键 → Import → 选择 CSV → DataTable → 选择对应行结构（\`FRFWeaponDef\` 等，定义见 \`Source/RedFront1941/Data/RFDataTypes.h\`）。
3. 首列 \`Name\` 会成为 RowName；若提示列不匹配，选择「忽略多余列（Ignore Extra Columns）」。

## 嵌套字段的处理（重要）
DataTable 导入只能按列 1:1 映射结构体字段，因此**所有嵌套对象都已展开为标量列**，可直接导入：

| 来源 | 展开后的列（示例） |
|---|---|
| equipment.csv \`effect\` | \`effect_damage\` \`effect_radius_m\` \`effect_fuse_s\` \`effect_calories\` \`effect_hydration\` \`effect_heal\` \`effect_use_time_s\` … |
| classes.csv \`passives[3]\` / \`active\` | \`passive_1_id\` \`passive_1_name_zh\` \`passive_1_effect\` … \`active_name_zh\` \`active_cooldown_s\` \`active_effect\` |
| enemies_infantry.csv \`ai\` | \`ai_awareness_m\` \`ai_reaction_s\` \`ai_suppression_resist\` \`ai_calls_support\` … |
| levels.csv \`objectives[5]\` | \`objective_1_id\` \`objective_1_text_zh\` \`objective_1_type\` \`objective_1_required\` … 以及派生列 \`enemy_total\` \`enemy_types\` \`support_count\` \`primary_objective_zh\` |
| levels.csv \`unlock_reward\` / \`endless_seed_modifier\` | \`reward_xp\` \`reward_weapon\` \`reward_class\` / \`endless_era\` \`endless_weight_mult\` |

无法用定长列表达的深层结构（\`enemy_composition\`、\`historical_accuracy\`、\`best_against\`、\`requires\`、\`load_bands\`）
仍以 JSON 字符串列保留，由运行时加载器 \`URFDataTableLoader::LoadAllContracts()\`（非 Shipping）
从 \`Content/RedFront/Data/*.json\` 读取并解析。两条路径并存：DataTable 供编辑器可视化与蓝图访问，JSON 加载器提供无损完整数据。
`, 'utf8');

/* Mirror runtime contracts into the UE project's Content folder. */
fs.mkdirSync(path.join(UNREAL_DATA, 'levels'), { recursive: true });
for (const file of ['weapons.json', 'classes.json', 'equipment.json', 'enemies.json', 'support.json', 'campaigns.json']) {
  fs.copyFileSync(path.join(DATA, file), path.join(UNREAL_DATA, file));
}
for (const file of levelFiles) {
  fs.copyFileSync(path.join(levelDir, file), path.join(UNREAL_DATA, 'levels', file));
}

log('\n生成完成：');
log(`  data/generated/  CSV ×9（武器 ${nWeapon} / 专长 ${nClass} / 装备 ${nEquip} / 敌军 ${nInf + nVeh + nAir} / 支援 ${nCall} / 战役 ${nCamp} / 关卡 ${nLevel}）`);
log(`  data/generated/levels.json          ${RF_DATA.levels.length} 关`);
log(`  prototype/js/data.generated.js      ${(fs.statSync(path.join(PROTO, 'data.generated.js')).size / 1024).toFixed(0)} KB`);
log(`  Unreal/Content/RedFront/Data/       JSON 合约 ×${6 + levelFiles.length}`);
log(`  数据版本 ${RF_DATA.meta.generatedAt}`);
