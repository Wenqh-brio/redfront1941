# data/generated —— 自动生成，请勿手改

由 `node tools/build-data.mjs` 生成。

| 文件 | 行数 | UE 目标 DataTable |
|---|---|---|
| weapons.csv | 30 | DT_Weapons（行结构 FRFWeaponDef） |
| classes.csv | 9 | DT_Classes |
| equipment.csv | 28 | DT_Equipment |
| enemies_infantry.csv | 12 | DT_Enemies_Infantry |
| enemies_vehicles.csv | 9 | DT_Enemies_Vehicles |
| enemies_air.csv | 3 | DT_Enemies_Air |
| support_callins.csv | 15 | DT_SupportCallIns |
| campaigns.csv | 5 | DT_Campaigns |
| levels.csv | 56 | DT_Levels |
| levels.json | 56 | 运行时可读的合并关卡表 |

## 导入 UE 5.8
1. 打开工程 → Content Browser → `Content/RedFront/Data`。
2. 右键 → Import → 选择 CSV → DataTable → 选择对应行结构（`FRFWeaponDef` 等，定义见 `Source/RedFront1941/Data/RFDataTypes.h`）。
3. 首列 `Name` 会成为 RowName；若提示列不匹配，选择「忽略多余列（Ignore Extra Columns）」。

## 嵌套字段的处理（重要）
DataTable 导入只能按列 1:1 映射结构体字段，因此**所有嵌套对象都已展开为标量列**，可直接导入：

| 来源 | 展开后的列（示例） |
|---|---|
| equipment.csv `effect` | `effect_damage` `effect_radius_m` `effect_fuse_s` `effect_calories` `effect_hydration` `effect_heal` `effect_use_time_s` … |
| classes.csv `passives[3]` / `active` | `passive_1_id` `passive_1_name_zh` `passive_1_effect` … `active_name_zh` `active_cooldown_s` `active_effect` |
| enemies_infantry.csv `ai` | `ai_awareness_m` `ai_reaction_s` `ai_suppression_resist` `ai_calls_support` … |
| levels.csv `objectives[5]` | `objective_1_id` `objective_1_text_zh` `objective_1_type` `objective_1_required` … 以及派生列 `enemy_total` `enemy_types` `support_count` `primary_objective_zh` |
| levels.csv `unlock_reward` / `endless_seed_modifier` | `reward_xp` `reward_weapon` `reward_class` / `endless_era` `endless_weight_mult` |

无法用定长列表达的深层结构（`enemy_composition`、`historical_accuracy`、`best_against`、`requires`、`load_bands`）
仍以 JSON 字符串列保留，由运行时加载器 `URFDataTableLoader::LoadAllContracts()`（非 Shipping）
从 `Content/RedFront/Data/*.json` 读取并解析。两条路径并存：DataTable 供编辑器可视化与蓝图访问，JSON 加载器提供无损完整数据。
