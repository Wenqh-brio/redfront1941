# 关卡数据 Schema（RedFront 1941 — 协议 v1）

所有关卡数据文件放在 `data/levels/`，UTF-8 JSON，单个文件包含一个 `levels` 数组。
顶层结构固定为：

```json
{
  "$schema": "redfront.levels/1",
  "campaign_id": "<本文件所属战役 id，必须来自 data/campaigns.json>",
  "author_note": "可选一句话",
  "levels": [ /* 下面定义的关卡对象数组 */ ]
}
```

## 关卡对象字段（全部必填，除非标注可选）

| 字段 | 类型 | 约束 |
|---|---|---|
| `id` | string | `sov_01_brest_fortress` 形式：`<faction>_<两位序号>_<短名>`，全库唯一，小写+下划线 |
| `index` | integer | 全局序号，1..56，必须与分配给你的区间完全一致且连续 |
| `number_in_campaign` | integer | 该战役内序号，从 1 开始 |
| `campaign_id` | string | 必须等于文件顶层 `campaign_id` |
| `name_zh` | string | 中文关卡名，6-14 字，有画面感 |
| `name_en` | string | 英文关卡名 |
| `date` | string | `YYYY-MM-DD`，必须落在该战役 start_date..end_date 之间（可等于边界） |
| `location_zh` | string | 真实地名中文 |
| `location_en` | string | 真实地名英文 |
| `coordinates` | string | `48.71N 37.53E` 形式，尽量真实 |
| `mission_type` | enum | `defense` / `delay` / `breakthrough` / `assault` / `urban_clearing` / `river_crossing` / `armored` / `ambush` / `relief` / `sabotage` / `siege` / `amphibious` |
| `faction` | enum | `soviet` / `us` |
| `player_role_zh` | string | 一句话：玩家在本关的身份与处境 |
| `player_class_recommended` | string[] | 1-3 个 `data/classes.json` 中的 class id |
| `objectives` | object[] | 2-5 条；`{ "id":"obj1", "text_zh":"...", "type":"primary"/"secondary", "required":true/false, "time_limit_s": 900 }`（`time_limit_s` 可为 `null`）；可选 `condition:"hold_position"` 配合 `hold_time_s`（>0 秒）与 `radius_m`（1-50 米）设置连续守点目标，有限 `time_limit_s` 必须大于 `hold_time_s`；或 `condition:"eliminate_count"` 配合正整数 `target_count` 与可选 `target_enemy_ids`（敌军合同 ID 数组）跟踪击杀；或 `condition:"reach_location"` 配合 `radius_m` 设置进入目标标记范围即完成的撤离/抵达目标；或 `condition:"rescue_count"` 配合正整数 `target_count` 和不少于该人数的 `rescue_points`（`{ "x_m": -16, "y_m": 4 }` 数组），玩家靠近幸存者后按 E 逐人护送撤离；或 `condition:"destroy_target"` 配合正整数 `target_count` 和不少于该数量的 `target_points`（`{ "x_m": -8, "y_m": 4, "health": 300, "name_zh": "入口工事" }` 数组），玩家需用枪弹、爆炸或炮火摧毁地图中的目标实体；可选 `marker_x_m` 与 `marker_y_m` 配对设置目标标记世界位置（米）；未指定条件时保持 E 键近距交互 |
| `enemy_composition` | object | 键必须来自 `data/enemies.json` 的 id，值为正整数；3-8 种 |
| `enemy_tactics_zh` | string | 60-140 字，敌军在此地的真实战术（依托地形、反击、地雷、反坦克炮埋伏等） |
| `friendly_forces_zh` | string | 60-140 字，友军编制（如第 13 近卫步兵师第 42 团 2 营） |
| `terrain` | enum | `city` / `forest` / `field` / `village` / `river` / `rail` / `industrial` / `fortress` / `trench` |
| `weather` | enum | `clear` / `rain` / `snow` / `fog` / `mud` / `storm` |
| `time_of_day` | enum | `dawn` / `day` / `dusk` / `night` |
| `visibility_m` | integer | 50-1200 |
| `difficulty` | integer | 1-10 |
| `par_time_s` | integer | 600-2400 |
| `weight_budget_kg` | number | 苏军 24 基准 / 美军 26 基准，可按关卡调整（如 22-28） |
| `resupply_points` | integer | 0-4 个单次使用的野战补给箱；靠近后按 E 补充当前武器弹药，并尝试将本阵营口粮与 1L 水壶加入空余补给槽 |
| `available_support` | string[] | 来自 `data/support.json` 的 call_in id，0-4 个；必须与 `faction` 匹配 |
| `vehicle_available` | string[] | 可为坦克呼叫 id（`sov_tank_t34_76` / `sov_tank_isu152` / `us_sherman_platoon`），也可为场景载具描述用的 enemies.json 车辆 id；无则 `[]` |
| `historical_note_zh` | string | **150-260 字**真实历史背景，包含番号、日期、地名与具体事件 |
| `historical_accuracy` | object | `{ "verified_events": ["..."], "dramatized": ["..."] }`，各 1-3 条 |
| `design_note_zh` | string | 80-150 字关卡玩法设计意图（节奏、机制教学、失败条件） |
| `music_mood` | string | 一句话配乐情绪 |
| `unlock_reward` | object | `{ "xp": 800-3000, "weapon": "<weapons.json id 或 null>", "class": "<classes.json id 或 null>" }` |
| `endless_seed_modifier` | object | `{ "era": 1941/1942/1943/1945, "weight_mult": 0.9-1.2 }` |

## 硬性质量要求

1. 日期、地名、番号、事件必须真实可考；不确定的细节写进 `dramatized`，不要发明"著名战役"。
2. `historical_note_zh` 必须提到至少一个具体番号（如"第 62 集团军第 13 近卫步兵师"）与一个具体地点。
3. `enemy_composition` 的兵力规模要与关卡难度和 `historical_note_zh` 相称：防守关守军少、进攻关敌军多；1941 年不要出现豹式/虎式。
4. 每关必须教一个机制（`design_note_zh` 里点明），例如"首次引入负重超载惩罚"。
5. 相邻关卡的 `mission_type` 不要连续三关相同。
6. JSON 必须能被 `JSON.parse` 解析（不要注释、不要尾随逗号）。
7. 严禁复制粘贴同一段 `historical_note_zh`，每关内容独一无二。
