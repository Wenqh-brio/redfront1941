# 《红色前线 1941 — RedFront 1941》

**GitHub 仓库名：`redfront1941` · [MIT License](LICENSE)**

> 二战题材 2D 小队射击开发原型 · UE 5.8 C++ 工程 · 56 个历史关卡 blockout + 浏览器玩法原型

> **项目状态：开发中，不是已完成的商业成品。** UE 关卡仍以可玩 blockout 和程序化占位美术为主；正式手绘美术、关卡润色、音频与完整发布验收仍在路线图中。浏览器原型比 UE 版本覆盖更多玩法流程。

一个玩家从 **巴巴罗萨计划前夜的边境哨兵**，一路打到 **柏林国会大厦的废墟**；另有一条完整的 **美军西线战役**（奥马哈海滩 → 雷马根大桥）。核心不是爽快射击，而是**真实**：你会因为多带两罐水而跑不动，会因为忘记吃饭而手抖，会因为炮击呼叫得太近而把自己一起炸掉。

---

## 一句话定位

**《红色前线 1941》= 史实考据的 2D 高清战役 × 硬核负重与生存 × 可呼叫的营连级火力支援。**

| 维度 | 设定 |
|---|---|
| 引擎 | Unreal Engine **5.8**（Paper2D / PaperZD 精灵渲染，正交相机，DX12） |
| 视角 | 2D 侧俯视 / 横版混合（正交 + 微视差分层） |
| 美术 | 程序化生成的 2D 占位贴图 / 精灵；最终手绘资产尚未完成 |
| 玩法 | 小队战术射击 + 生存管理 + 支援火力指挥 |
| 关卡 | **56 关**（苏军 44 + 美军 12），5 大战役，另有 **无限模式** |
| 难度 | 新兵 / 标准 / 老兵 / **史实** 四档（史实档单命、无敌人标记、弹药按史实基数） |
| 平台 | Windows PC（键鼠；尚未在 Steam 发布） |

---

## 目录结构

```
RedFront1941/
├─ README.md                     ← 本文件：项目总览与验收入口
├─ docs/                         ← 设计文档（创意、系统、关卡、美术、技术、路线图）
│   ├─ 01-游戏设计文档_GDD.md
│   ├─ 02-系统设计_负重与生存.md
│   ├─ 03-战斗系统与支援火力.md
│   ├─ 04-兵种与专长.md
│   ├─ 05-关卡设计与教学曲线.md
│   ├─ 06-历史考证与表现准则.md
│   ├─ 07-美术管线_高清2D.md
│   ├─ 08-UE5.8技术架构.md
│   └─ 09-开发路线图与验收标准.md
├─ data/                         ← 机器可读的单一数据源（JSON）
│   ├─ weapons.json              武器表（27 件）
│   ├─ classes.json              兵种/专长表（9 个）+ 负重档位
│   ├─ equipment.json            手榴弹/食物/水/医疗/工具/护甲/弹药
│   ├─ enemies.json              敌军步兵、车辆、飞机
│   ├─ support.json              炮击/坦克/空中/步兵支援 + 无限模式波次表
│   ├─ campaigns.json            5 大战役与难度倍率
│   ├─ levels/                   关卡数据（5 个战役文件 + SCHEMA.md）
│   │   └─ campaign_0X_*.json
│   └─ generated/                由工具生成的 UE DataTable CSV（勿手改）
├─ tools/                        ← 校验与生成工具（Node，无依赖）
│   ├─ validate-data.mjs         数据完整性校验（ID 引用、日期区间、范围校验）
│   ├─ build-data.mjs            生成 CSV + 原型使用的 JS 数据
│   └─ smoke-test-prototype.mjs  原型无头冒烟测试
├─ prototype/                    ← 可直接在浏览器游玩的核心机制原型（零依赖）
│   └─ index.html
├─ Unreal/                       ← UE 5.8 工程骨架
│   ├─ RedFront1941.uproject
│   ├─ Config/                   引擎/输入配置（Enhanced Input）
│   ├─ Source/RedFront1941/      C++ 玩法系统（负重、生存、弹道、支援、AI）
│   └─ Content/
│       ├─ RedFront/Data/        同步到 UE 的 56 关与全部玩法 JSON 合约
│       ├─ RedFront/Art2D/       程序化生成的美术资产（102 张贴图 + art-manifest.json）
│       ├─ RedFront/Levels/      5 个战役文件夹，共 56 张 UE blockout 地图
│       └─ Python/               地图/材质/角色 Blueprint 自动生成脚本
```

---

## 美术资产：不依赖外部提供

**没有模型/贴图文件也能跑起来**：所有材质贴图、天气覆盖层、贴花、特效序列帧、占位角色精灵与 UI 素材
都由 `tools/generate_art.py` 程序化生成（Pillow + numpy，固定种子、可复现），
再由 `Unreal/Content/Python/rf_generate_materials.py` 在 UE 编辑器内**自动创建材质与 Paper2D 资产**。

| 产物 | 数量 | 说明 |
|---|---|---|
| 地形 PBR | 9 种 × (BaseColor 2048² + Normal 1024² + Roughness 1024²) | **可无缝平铺**，含裂缝/烟熏/苔藓叠加 |
| 天气覆盖层 | 8 张 | 新雪/踩踏雪/湿泥/水膜/干尘/焦土/霜冻/雨幕 |
| 贴花 | 12 张 | 弹坑 3 级、废墟、血迹 3 种、履带 2 种、燃烧、脚印、水洼 |
| VFX 序列帧 | 15 组 | 枪口焰 ×5 口径、爆炸 8 帧、烟幕 12 帧、火焰、命中材质 ×6、弹道 |
| 占位角色精灵 | 苏军/美军/德军 5 种单位 × 5 动画 × 4 方向（560 帧） | 朝向/帧数/动画节奏/图集布局按最终规格制作 |
| 视差背景 / UI | 5 张 + 6 张 | 城市废墟/树线/工厂/雪原/旷野；九宫格面板/暗角/颗粒/准星 |
| 材质 | 7 个 Master + 69 个 Instance | 参数化：覆盖层强度、湿润度、战损、视差系数、烟幕浓度 |

```powershell
python tools/generate_art.py            # 全量生成（2048 高清，约 53 秒 / 19 MB）
python tools/generate_art.py --quick    # 半分辨率快速迭代
python tools/verify_art.py              # 自检：尺寸/接缝/空白/alpha/法线/材质引用闭合
python Unreal/Content/Python/rf_generate_materials.py --dry-run   # 校验清单与创建计划
```

详细说明、替换手绘素材的方式、扩展指南见 [程序化材质与美术资产生成](docs/10-程序化材质与美术资产生成.md)。

---

## 五分钟上手

### 1) 马上玩原型（无需引擎）

直接在浏览器中打开 `prototype/index.html`。

原型实现了**完整核心机制**，不是静态示意图：

- **战役/关卡选择**：56 关全列表（按战役分组，含日期、地点、历史简介）
- **装备与负重**：选主武器/副武器/手榴弹/食物/水/护甲/工具/无线电，实时显示总重与负重档位（轻装 / 标准 / 重装 / 超载）对速度、体力的影响
- **生存**：饱食度、饮水、体力、体温；进食/饮水有施法时间，移动可以吃 D 口粮
- **战斗**：弹道命中、弹匣/换弹、后坐与散布、掩体减伤
- **手榴弹**：RGD-33 / F-1 / Mk2 / 烟幕 / 燃烧瓶 / 炸药包，带引信与破片衰减
- **支援火力**：指挥点（CP）+ 通讯延迟 + 观察视线；82mm 迫击炮、122mm 榴弹炮、喀秋莎、IL-2、T-34 坦克排、近卫步兵增援（美军战线为 105mm / P-47 / 谢尔曼）
- **无限模式**：波次生成、每 5 波精英波、整备阶段补给与换装

操作（游戏内也有提示）：

| 按键 | 功能 | 按键 | 功能 |
|---|---|---|---|
| `WASD` / 方向键 | 移动 | `R` | 换弹 |
| `鼠标` | 瞄准 | `1` `2` | 切换主/副武器 |
| `左键` | 射击 | `G` | 投掷手榴弹（按住计时） |
| `Shift` | 冲刺（消耗体力） | `F` | 进食 |
| `Ctrl` | 蹲伏 | `E` | 饮水 |
| `Q` | 呼叫炮击（指向目标点） | `T` | 呼叫坦克/装甲支援 |
| `V` | 呼叫空中支援 | `B` | 望远镜（提升炮击精度） |
| `Space` | 使用支援确认 / 下一波 | `Esc` | 暂停 / 返回 |

### 2) 跑数据校验与生成

```powershell
node tools/build-data.mjs            # JSON → UE DataTable CSV + 原型数据 + 合并关卡表
node tools/validate-data.mjs         # 校验 56 关与所有 ID 引用/枚举/区间/日期/唯一性
node tools/build-docs.mjs            # 由真实数据生成 docs/05 的 56 关总表
node tools/check-unreal.mjs          # UE 骨架静态校验（UHT 结构 / 模块 / 数据契约字段）
node tools/check-ui.mjs              # 原型 UI 接线校验（元素 id / 资源 / 快捷键）
node tools/smoke-test-prototype.mjs  # 玩法仿真无头测试（752 项断言，含 56 关全量推进）
node tools/smoke-test-ui.mjs         # UI 流程无头测试（46 项断言，假 DOM 驱动全流程）
python tools/generate_art.py --quick # 程序化生成美术资产（快速模式）
python tools/verify_art.py           # 美术资产自检（尺寸/接缝/空白/alpha/法线/材质引用）
python Unreal/Content/Python/rf_generate_materials.py --dry-run   # 材质生成计划校验
```

一条命令跑完整回归：

```powershell
node tools/build-data.mjs; node tools/validate-data.mjs; node tools/build-docs.mjs; node tools/check-unreal.mjs; node tools/check-ui.mjs; node tools/smoke-test-prototype.mjs; node tools/smoke-test-ui.mjs; python tools/verify_art.py; python Unreal/Content/Python/rf_generate_materials.py --dry-run
```

### 3) 打开 UE 工程

用 Unreal Engine 5.8 打开 `Unreal/RedFront1941.uproject`。首次打开会提示重新生成 C++ 模块（需要 Visual Studio 2022 + MSVC + Windows SDK）。数据表导入路径见 `Unreal/README.md`。

### 4) 构建 Windows Shipping 包

安装 UE 5.8、Visual Studio C++ 工具链和 Windows SDK 后，在 PowerShell 运行：

```powershell
.\Unreal\Scripts\Package-Windows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

脚本会生成缺失的 VS 工程文件，通过 MSBuild 编译，并用 UE Automation Tool Cook/Stage/Package 56 张地图；默认输出到 `dist\WindowsShippingFinal`。直接启动该目录中的 `RedFront1941.exe`。包内附有 VC++ Redistributable 安装程序；目标电脑若尚未安装兼容的 VC++ 运行库，先运行 `Engine\Extras\Redist\en-us\vc_redist.x64.exe`。游戏不需要安装 Unreal Editor、Node.js 或 Python。

**数据导入说明**：`data/generated/*.csv` 中所有嵌套对象都已**展开为标量列**（如 `effect_damage`、`passive_1_effect`、`ai_awareness_m`、`objective_1_text_zh`、`reward_xp`），可直接用 `Tools → Import DataTable` 导入；只有无法定长表达的深层结构（`enemy_composition`、`historical_accuracy`、`best_against`）保留为 JSON 列，由运行时加载器 `URFDataTableLoader::LoadAllContracts()` 读取 `Content/RedFront/Data/*.json` 解析。两条路径并存，详见 `data/generated/README.md`。

---

## 核心系统速览

### 负重（Weight）——本作的灵魂

- 苏军基准负重 **24 kg**，美军 **26 kg**；每个档位直接改变速度与体力消耗：

| 档位 | 负重比 | 移速 | 体力消耗 | 后果 |
|---|---|---|---|---|
| 轻装 | ≤60% | ×1.08 | ×0.8 | 跑得快，但火力/续航不足 |
| 标准 | ≤85% | ×1.00 | ×1.0 | 推荐区间 |
| 重装 | ≤100% | ×0.92 | ×1.25 | 冲刺更短，翻越变慢 |
| 超载 | >100% | ×0.78 | ×1.7 | **无法冲刺**，倒地起身更慢 |

水壶喝完会**真的减重 1 kg**；弹药消耗同样减重——轻装是动态结果，不是开局选择。

### 食物与水

饱食度与水份随时间下降；归零后不是立刻死亡，而是**精度下降 → 体力上限下降 → 持续掉血**（史实档消耗 ×1.5）。
进食需要停下 4-7 秒（仅 D 口粮/巧克力支持移动进食），伏特加恢复士气但 40 秒内瞄准抖动 +25%。

### 兵种与专长（9 个）

步兵、突击兵、机枪手、狙击手、反坦克兵、工兵、医护兵、侦察兵、迫击炮手。
每个专长 = 槽位修正 + 3 被动 + 1 主动技能（例：狙击手的「校射呼叫」让炮击散布缩小 60%；工兵的「定向爆破」必破碉堡门）。

### 支援火力——呼叫，而不是按按钮

所有支援受 **指挥点（CP）+ 通讯延迟 + 冷却** 三重限制：

- 82mm 迫击炮：1 CP，12 秒弹着，打掩体后的机枪位
- 122mm 榴弹炮：3 CP，35 秒，需观察视线
- 152mm 重炮：5 CP，50 秒，拆楼拆碉堡
- 喀秋莎齐射：4 CP，覆盖 60 m，最小安全距离 150 m（会炸到你自己）
- IL-2 扫射 / P-47：需要目标标记，敌方 Flak 36 会把你指望的飞机打下来
- T-34/76 排、ISU-152、谢尔曼排：从后方道路进场，油弹有限，8 分钟后撤离

无线电台被摧毁 → 60 秒内无法呼叫；敌人无线电测向 → 硬核难度下会招来反炮兵火力。

### 无限模式

浏览器原型提供随机战区、阶段性敌军构成和整备流程。UE 版本可用 `-endless` 启动：复用当前战役 blockout 地图，按 `support.json` 的稀疏年代编组生成敌军，按空格发起首波/后续波；清波后需等待 45 秒整备。UE 版本目前没有独立随机战区，也不提供复活卡或天气昼夜轮换。

---

## 五个战役 × 56 关

| # | 战役 | 关卡 | 时间 | 主题 |
|---|---|---|---|---|
| 1 | 巴巴罗萨 1941 | 1-12 | 1941-06 → 1942-01 | 溃退、迟滞、莫斯科城下的第一次反击 |
| 2 | 斯大林格勒 1942 | 13-22 | 1942-05 → 1943-02 | 废墟近战、伏尔加河、合围与解围 |
| 3 | 库尔斯克与第聂伯河 1943 | 23-32 | 1943-07 → 1944-08 | 反坦克支撑点、坦克会战、强渡大河 |
| 4 | 巴格拉季昂到柏林 1944-45 | 33-44 | 1944-06 → 1945-05 | 纵深突破、要塞攻坚、国会大厦 |
| 5 | 西线 1944-45（美军） | 45-56 | 1944-06 → 1945-03 | 奥马哈、灌木篱墙、许特根、阿登、雷马根 |

完整关卡表（含日期、地点、敌军构成、推荐专长、历史注记）见 `data/levels/*.json` 与 [关卡设计与教学曲线](docs/05-关卡设计与教学曲线.md)。

---

## 真实性准则

1. **装备与年份匹配**：1941 年不会出现豹式或虎式；喀秋莎 1942 年后才出现在关卡支援表里。
2. **番号与地名可考**：每关的历史注记必须包含真实番号与真实地点；无法考证的戏剧化处理在 `historical_accuracy.dramatized` 中显式声明。
3. **战术符合条令**：苏军步兵以连/排为单位冲击、机枪压制 + 侧翼；德军以 MG42 为火力基点、装甲掷弹兵反冲击；美军以班组跃进 + 烟幕 + 前进观察员呼叫火力。
4. **代价存在**：误伤、弹药短缺、通讯中断、泥泞与严寒都是机制，不是背景描述。

---

## 验收状态

| 交付物 | 状态 | 验证方式 |
|---|---|---|
| 设计文档 9 篇 | ✅ | `docs/`（05 篇由数据生成，永不脱节） |
| 武器 30 / 专长 9 / 装备 28 / 敌军 24 / 支援 15 | ✅ | `data/*.json` |
| **56 个关卡定义**（苏军 44 + 美军 12） | ✅ 校验通过 | `tools/validate-data.mjs`（引用、枚举、区间、日期与战役区间、index 连续性、历史注记唯一性） |
| UE 5.8 C++ 工程（25 头文件 + 25 源文件 + Config） | ✅ MSBuild 编译通过 | `tools/check-unreal.mjs` + UE 5.8 targets |
| UE 战役地图（苏军 44 + 美军 12） | ✅ 56 张 blockout 地图 | `Unreal/Content/Python/rf_generate_campaign_maps.py` |
| 可玩原型（加载/战役/无限/负重/生存/支援） | ✅ 752 项断言通过 | `tools/smoke-test-prototype.mjs` |
| 原型 UI 流程（菜单→选关→配装→战斗→结算） | ✅ 46 项断言通过 | `tools/smoke-test-ui.mjs`（假 DOM 驱动） |
| 美术/材质资产（102 张贴图 + 7 Master + 69 实例 + 560 精灵） | ✅ 自检与 UE 导入通过 | `tools/verify_art.py` / `rf_materials_report.txt` |
| Paper2D 角色动画 | ✅ 560 张方向帧 + 100 个动画 Flipbook + 苏/美军角色 BP | `Content/RedFront/Art2D/Characters/Flipbooks` |
| UE 运行时 JSON 数据 | ✅ 56 关与全部玩法契约已同步 | `node tools/build-data.mjs` |
| UE 战役玩法接线 | ✅ 基础版：步兵交战、合同车辆/飞机、救援/补给、守点/击毁/抵达/摧毁目标 | `RFGameMode` / `RFVehicleActor` / `RFPlayerController` |
| UE 无限模式 | ✅ 合同驱动波次基础版 | `-endless` 启动；空格发波；45 秒整备；稀疏波次表、递增兵力与每波最多 64 单位；仍复用战役地图 |
| UE 5.8 / Visual Studio 编译 | ✅ Editor Development 与 Game Shipping 均构建通过 | `Unreal/Scripts/Package-Windows.ps1` |
| Win64 Shipping 封装 | ✅ 56 张地图 Cook；归档启动冒烟通过 | `dist/WindowsShippingFinal/RedFront1941.exe`（本地生成，不提交到 Git） |
| 车辆 AI/移动/炮击 | ✅ 基础合同驱动版本 | `RFVehicleActor` 按速度/固定阵地/交战距离机动，搜索目标并经弹道/装甲规则开火；仍需游戏内平衡验证 |
| 敌机标记→空中攻击实体 | ✅ 基础合同驱动版本 | `RFAircraftActor` 具备限时飞越、俯冲炸弹/机枪扫射和可受击击落；目前为几何占位模型 |
| UE 战斗 HUD | ✅ 原生 Canvas 占位版 | 显示任务时钟、目标状态、指挥点、生命/生存值、武器弹药；仍待视觉打磨及无障碍选项 |
| 目标专属条件、最终关卡美术、音频、手绘角色替换 | ⏳ 待做 | 当前地图、装甲单位与士兵美术为可运行 blockout / 占位资产 |

### 已修复的真实缺陷（由上述测试发现）

| 缺陷 | 影响 | 修复 |
|---|---|---|
| 弹丸按点碰撞判定，46 u/帧穿过 20 u 目标 | 玩家射击几乎无法命中 | 改为线段×圆连续碰撞 |
| 战役关卡敌军构成查表路径错误 | 56 关全部不生成敌军 | 统一走 `findEnemyDef` |
| 武器定义未转运行时参数（读 `mag`/`cat` 为 undefined） | 无法射击 | 引入 `weaponProfiles` 转换层 |
| 地图边缘敌军无进攻命令（感知 220m vs 2600px 地图） | 敌人原地不动，关卡不打起来 | 加入敌军态势、突击方位与无线电警戒传播 |
| 无寻路的敌人被掩体卡死 | AI 原地抖动 | 卡住时沿切线绕行 |
| 死亡后生命值未钳制 | 生命显示负值 | `playerDied` 归零 |
| 无限模式波次被 `wavesTotal` 挡住 | 只能打 1 波 | 无限模式改用独立波次判定 |
| UI 脚本加载期绑定未创建的 canvas | **打开页面即崩溃** | 改为首次进入关卡时接线 |
| 点击"无限模式"后立刻切到不存在的屏幕 | 无限模式按钮无反应 | 修正 `data-go` 分发逻辑 |
| 部署按钮硬编码 `campaign` 模式 | 无限模式部署时崩溃 | 改用 `state.mode` |
| UE DataTable CSV + 生成式关卡文档 | ✅ | `tools/build-data.mjs` / `build-docs.mjs` |
| UE 自动化测试 | ✅ 8 项 | 无限编组选择/兵力上限、到达、摧毁目标、击毁、守点、救援、弹药容量 |
| 56 个 UE 关卡 blockout | ✅ | `rf_generate_campaign_maps.py`，每关源数据来自 `data/levels/*.json` |
| 最终美术、音频与关卡打磨 | ⏳ 待做 | 见 `docs/07` 首期美术清单与 `docs/09` 里程碑 |

### 冒烟测试覆盖（最近一次运行）

```
配装与负重      18 组组合，档位边界（60/85/100%）全部正确
56 关全量推进   每关 60 秒：射击/换弹/投弹/进食/饮水/医疗/呼叫支援，无 NaN、无实体泄漏
命中判定靶场    开阔地 120m 命中 3/3、500m 命中 2/3、沙袋后命中 0（正确阻断）
装甲判定        35mm 对 100mm 正面跳弹；120mm 击穿
支援火力        15 项支援全部完成「呼叫→延迟→生效→冷却」链路；烟幕/通场/增援各自验证
敌方反制        40 次呼叫触发 7 次无线电测向反炮兵；斯图卡/Bf 109 转为空袭事件
无限模式        推进 5 波、整备 4 次；兵力 1 波 10 → 16 波 49；年代演进到虎式/国民突击队
长时间稳定性    单局 20 分钟连续仿真，无异常、无泄漏
```

> ⚠️ 说明：本仓库是**持续开发中的游戏项目**（设计 + 数据 + UE blockout + 浏览器玩法原型 + 程序化占位美术），不等同于已完成的完整游戏。
> 材质贴图与占位精灵已由 `tools/generate_art.py` 自行生成，**不需要外部提供模型文件**；
> UE 工程现已能用本机 VS/UE 5.8 编译，56 个关卡 blockout、苏军/美军/德军角色精灵与动画资产已生成；
> UE 关卡已接入预算限制下的步兵生成与基础 AI 交战、按合同机动/搜索/炮击的装甲单位、限时飞越的敌机攻击实体、近距离目标交互及限时失败；布列斯特、莫扎伊斯克和会让站三关有连续守点合同，六关按敌军合同统计击毁数，布列斯特撤离桥头、奥马哈海堤与巴黎默里斯饭店均需实际抵达标记区域；布列斯特另有七名可交互幸存者，救出六人即可完成撤离目标。国会大厦入口封锁工事和布拉格伏尔塔瓦河桥爆破装置均可被摧毁。野战补给箱可一次性补充弹药并向空余携行槽加入口粮和水壶。条件目标的 HUD 与 E 键限制均已接入。其余目标专属逻辑、正式空战与防空、最终关卡构图、音频与手绘美术替换仍待完成（见 `docs/07`、`docs/09`、`docs/10`）。

## License

Original code, game data, documentation and project-generated assets are released under the MIT License. Unreal Engine and Epic Games materials are not relicensed by this repository; follow the applicable Unreal Engine EULA and third-party notices when building or distributing the game.
