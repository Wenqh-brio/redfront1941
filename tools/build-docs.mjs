#!/usr/bin/env node
/* =============================================================================
 * build-docs.mjs — 用真实关卡数据生成 docs/05-关卡设计与教学曲线.md
 * 目的：56 关总表由数据生成，永不与 data/levels/*.json 脱节。
 * 用法：node tools/build-docs.mjs
 * ===========================================================================*/
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const DATA = path.join(ROOT, 'data');
const read = (p) => JSON.parse(fs.readFileSync(p, 'utf8'));

const campaigns = read(path.join(DATA, 'campaigns.json'));
const enemies = read(path.join(DATA, 'enemies.json'));
const support = read(path.join(DATA, 'support.json'));

const enemyName = new Map();
for (const g of ['infantry', 'vehicles', 'air']) for (const e of enemies[g] || []) enemyName.set(e.id, e.name_zh);
const supportName = new Map(support.call_ins.map((s) => [s.id, s.name_zh]));

const levelsDir = path.join(DATA, 'levels');
let levels = [];
for (const f of fs.readdirSync(levelsDir).filter((x) => x.endsWith('.json')).sort()) {
  const doc = read(path.join(levelsDir, f));
  levels.push(...doc.levels);
}
levels.sort((a, b) => a.index - b.index);

const MT = { defense: '防守', delay: '迟滞', breakthrough: '突破', assault: '强攻', urban_clearing: '巷战清剿', river_crossing: '渡河', armored: '装甲战', ambush: '伏击', relief: '解围/会师', sabotage: '破袭', siege: '围城', amphibious: '两栖登陆' };
const TR = { city: '城市', forest: '森林', field: '旷野', village: '村落', river: '河流', rail: '铁路', industrial: '工业区', fortress: '要塞', trench: '战壕' };
const WE = { clear: '晴', rain: '雨', snow: '雪', fog: '雾', mud: '泥泞', storm: '暴风雪' };

/* 统计 */
const byCampaign = new Map(campaigns.campaigns.map((c) => [c.id, levels.filter((l) => l.campaign_id === c.id)]));
const typeCount = {};
for (const l of levels) typeCount[l.mission_type] = (typeCount[l.mission_type] || 0) + 1;
const diffHist = {};
for (const l of levels) diffHist[l.difficulty] = (diffHist[l.difficulty] || 0) + 1;
const enemyTotal = levels.reduce((a, l) => a + Object.values(l.enemy_composition).reduce((x, y) => x + y, 0), 0);

const rows = (list) => list.map((l) => {
  const comp = Object.entries(l.enemy_composition).map(([k, v]) => `${enemyName.get(k) || k}×${v}`).join('、');
  const sup = (l.available_support || []).map((s) => supportName.get(s) || s).join('、') || '—';
  return `| ${l.index} | ${l.name_zh} | ${l.date} | ${l.location_zh} | ${MT[l.mission_type] || l.mission_type} | ${TR[l.terrain] || l.terrain}/${WE[l.weather] || l.weather} | ${l.difficulty} | ${Math.round(l.par_time_s / 60)}′ | ${l.weight_budget_kg}kg | ${comp} | ${sup} |`;
}).join('\n');

const HEAD = '| # | 关卡 | 日期 | 地点 | 任务 | 地形/天气 | 难度 | 时长 | 负重上限 | 敌军构成 | 可用支援 |\n|---|---|---|---|---|---|---|---|---|---|---|';

const detail = (list) => list.map((l) => {
  const objs = (l.objectives || []).map((o) => `  - ${o.type === 'primary' ? '**主**' : '次'} ${o.text_zh}${o.time_limit_s ? `（限时 ${Math.round(o.time_limit_s / 60)} 分钟）` : ''}`).join('\n');
  return `### ${l.index}. ${l.name_zh}（${l.name_en}）
- **${l.date} · ${l.location_zh} · ${l.coordinates}**｜${MT[l.mission_type]}｜${TR[l.terrain]}/${WE[l.weather]}｜能见度 ${l.visibility_m}m｜难度 ${l.difficulty}/10｜建议 ${Math.round(l.par_time_s / 60)} 分钟
- 玩家身份：${l.player_role_zh}
- 推荐专长：${(l.player_class_recommended || []).map((c) => c).join('、')}
- 目标：
${objs}
- 敌军战术：${l.enemy_tactics_zh}
- 友军：${l.friendly_forces_zh}
- 设计意图：${l.design_note_zh}
- 历史：${l.historical_note_zh}
`;
}).join('\n');

const text = `# 05 · 关卡设计与教学曲线

> 本文件由 \`node tools/build-docs.mjs\` 从 \`data/levels/*.json\` 生成，**56 关总表与数据表永远一致**。
> 生成时间：${new Date().toISOString().slice(0, 16).replace('T', ' ')}｜关卡总数：${levels.length}｜敌军单位投放总量：${enemyTotal}

---

## 1. 设计原则

1. **每关只教一个新机制**（写在该关 \`design_note_zh\`），并且用"后果"教，而不是用弹窗教。
2. **任务类型不连续重复三关**，避免节奏疲劳（数据校验器会自动检查）。
3. **敌军规模与历史相称**：防守关守军少而精，进攻关敌军多而散；1941 年不会出现豹式/虎式。
4. **补给点即节奏器**：\`resupply_points\` 为 0 的关卡（许特根、巴斯托涅、斯大林格勒部分关卡）是整部游戏生存压力最高的地方。
5. **史实优先于戏剧性**：每关历史注记必须含真实番号与真实地点，虚构部分在 \`historical_accuracy.dramatized\` 中显式声明。

## 2. 教学曲线（机制引入顺序）

| 关卡区间 | 引入机制 | 教学方式 |
|---|---|---|
| 1-3 | 负重档位、栓动步枪节奏、掩体 | 超载时任务提示"你已无法冲刺"，玩家自己卸装备 |
| 4-6 | 食物与饮水消耗、烟幕掩护 | 强渡/撤退关卡的补给点稀少，逼迫管理 |
| 7-9 | 手榴弹引信与破片、敌军机枪压制 | 首次遭遇 MG42 用固定桥段教"烟幕 + 炮击" |
| 10-12 | 反坦克手榴弹、燃烧瓶、严寒 | 坦克伏击关卡，近距离投掷风险自负 |
| 13-19 | 巷战三维空间、室内近战、电台被破坏 | 电台摧毁事件后 60 秒禁呼叫 |
| 20-24 | 呼叫炮击的延迟与校射、观察视线 | 无校射时散布翻倍，玩家自己学会用望远镜 |
| 25-29 | 反坦克支撑点、雷区、强渡大河 | 工兵破障与烟幕弹幕的组合教学 |
| 30-36 | 多兵种协同、坦克搭载步兵、城市瓦解 | 坦克搭载冲锋 + 逐楼清剿 |
| 37-44 | 堡垒攻坚、地道战、全支援体系 | 152mm 最小安全距离的惨痛教训 |
| 45-56 | 两栖登陆、灌木篱墙、森林地狱、冬季防御 | 美军的快速火力响应与班组跃进 |

## 3. 难度与节奏分布

**难度直方图**（1-10）：${Object.keys(diffHist).sort((a, b) => a - b).map((k) => `${k} 分×${diffHist[k]}`).join(' · ')}

**任务类型分布**：${Object.entries(typeCount).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${MT[k] || k}×${v}`).join(' · ')}

节奏规则：
- 每 3-4 关安排一次"喘息关卡"（\`relief\` / 低难度城市关），第 43 关托尔高会师是全游戏的情绪高点。
- 难度 9-10 的关卡必须紧跟一次 4-6 难度关卡，防止连续挫败。
- 每部战役的最后一关难度递增（剧本格式：第 12、22、32、44、56 关）。

## 4. 战役总表

${campaigns.campaigns.map((c) => {
  const list = byCampaign.get(c.id) || [];
  return `### ${c.name_zh}\n- 时间：${c.start_date} → ${c.end_date}｜关卡：${list.length} 关｜派系：${c.faction === 'us' ? '美军' : '苏军'}\n- 基调：${c.tone}\n- 玩家弧线：${c.player_arc_zh}\n- 机制重点：${(c.mechanics_focus || []).join('、')}\n- 解锁节奏：${c.unlock_progression}\n\n${HEAD}\n${rows(list)}\n`;
}).join('\n')}

## 5. 全部 56 关速查表

${HEAD}
${rows(levels)}

## 6. 关卡明细

${detail(levels)}

## 7. 关卡制作检查单（每一关都要过）

- [ ] 出生点 30 秒内不会立即被机枪扫射（无"开门杀"）
- [ ] 至少两条可行进攻/防守路线（侧翼可绕）
- [ ] 敌机枪位有可摧毁掩体或可用烟幕/炮击的解法
- [ ] 至少一处补给点，或者明确告知"本关无补给"
- [ ] 弹药消耗预估 ≤ 玩家携带量的 1.2 倍（略紧但不至于弹尽）
- [ ] 目标文本与 HUD 显示一致
- [ ] 历史注记含真实番号与地点，戏剧化内容已声明
- [ ] 难度 9-10 的关卡已由内测打过 5 次以上
- [ ] 重玩价值：存在可选的次要目标或不同配装解法

## 8. 无限模式关卡设计

- **地形**：从 9 种地形随机拼装（城市/森林/旷野/村落/河流/铁路/工业区/要塞/战壕），每次开始随机
- **波次**：敌军数量 = \`4 + floor(n × 1.6)\`，精英单位从第 6 波出现，第 5/10/15 波为 Boss 波
- **年代演进**：1-3 波 1941（德军步兵）→ 4-8 波 1942（党卫军 + 三号/四号）→ 9-14 波 1943（豹式、铁拳）→ 15+ 波 1945（虎式、国民突击队、Pak 40）
- **整备**：每波之间 45 秒，可补给食物/水/弹药并构筑掩体；每 5 波获得一次免费支援与一张复活卡
- **天气**：每 3 波随机变化，影响视野、炮击精度与空中支援可用性
`;

fs.writeFileSync(path.join(ROOT, 'docs', '05-关卡设计与教学曲线.md'), text, 'utf8');
console.log(`✔ 生成 docs/05-关卡设计与教学曲线.md（${levels.length} 关，${(text.length / 1024).toFixed(0)} KB）`);
