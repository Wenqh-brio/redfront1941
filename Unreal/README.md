# RedFront1941 — Unreal Engine 5.8 project

High-resolution 2D (Paper2D / PaperZD) WWII squad shooter. One shared simulation
(ballistics, weight, food/water, support) drives two campaigns: a Soviet Red Army
soldier from Operation Barbarossa to the fall of Berlin, and a US Western Front campaign.

* Engine: **UE 5.8**, module `RedFront1941` (Runtime, Default loading phase)
* Plugins: **Paper2D**, **EnhancedInput** (+ JsonBlueprintUtilities, PythonScriptPlugin)
* Target platform: **Windows**, DX12 / SM6 (Lumen + Virtual Shadow Maps)

---

## 1. Opening the project

1. Install UE 5.8 and register it as the engine association for `RedFront1941.uproject`
   (right-click the `.uproject` → *Switch Unreal Engine version…*, or set
   `"EngineAssociation": "5.8"` by hand — it is already set).
2. Open the generated `RedFront1941.sln` in Visual Studio and build
   `RedFront1941Editor` with `Development Editor | Win64` (or run the UE 5.8
   `Build.bat` command documented in `Build_Editor.log`).
3. Open `RedFront1941.uproject`. The startup map is
   `/Game/RedFront/Levels/C01_Barbarossa_1941/LV_SOV_01_BrestFortress`.
   Fifty-six campaign blockout maps are generated from the JSON contracts under
   `Content/RedFront/Levels/C01_*` through `C05_*`; each contains a player start,
   terrain-specific cover, mission route, objective markers, enemy spawn markers,
   resupply points, and radio/support metadata.
4. Play-in-editor uses `ARFGameMode` with the legacy axis/action bindings from
   `Config/DefaultInput.ini`, so the project is playable before any Input Action asset
   exists. Once `IMC_RF_Gameplay` and the `IA_*` assets are authored, assign them on the
   `BP_RFPlayerController` and the Enhanced Input path takes over automatically.

### Data contracts

Gameplay data lives **outside** this project, in `..\data\*.json`, owned by the data
author:

| Contract | Consumed by |
|---|---|
| `weapons.json` | `FRFWeaponDef`, `URFWeaponBase`, `RFBallistics` |
| `classes.json` | `FRFClassDef`, `FRFWeightBand`, `URFWeightComponent`, `URFLoadoutSubsystem` |
| `equipment.json` | `FRFItemDef`, `URFInventoryComponent`, `ARFGrenade`, `URFSurvivalComponent` |
| `enemies.json` | `FRFEnemyDef`, `FRFVehicleDef`, `ARFEnemyAIController`, `URFVehicleArmorComponent` |
| `support.json` | `FRFSupportCallInDef`, `URFSupportSubsystem`, `ARFArtilleryStrike`, `ARFTankCallIn`, `ARFAirStrike` |
| `campaigns.json` | `FRFCampaignDef`, `FRFDifficultyScaling`, `ARFGameState` difficulty |
| `levels/*.json` | `FRFLevelDef` (`data/levels/SCHEMA.md` defines the format) |

Two data routes are supported (details in `RFDataTableLoader.h`):

* **Editor importer (optional):** `Tools > Import DataTable` from the CSVs in
  `..\data\generated\` into `DT_Weapons`, `DT_Classes`, `DT_Items`, `DT_Enemies`,
  `DT_Vehicles`, `DT_Support`, `DT_Campaigns`, `DT_Levels_*`, `DT_Difficulty`.
* **Runtime JSON loader:** `URFDataTableLoader::LoadAllContracts()` reads
  `Content/RedFront/Data/*.json` and builds the same rows into transient `UDataTable`s.
  Packaging stages `RedFront/Data` as NonUFS so the same contract-backed missions,
  weapon definitions and enemy data are available in Shipping.

### Windows packaged build

From the repository root, run `Unreal\Scripts\Package-Windows.ps1`. It builds the game
target and creates a self-contained UE 5.8 Win64 Shipping archive under
`dist\WindowsShippingFinal`.
The archive includes the UE runtime, cooked campaign content, direct-read JSON contracts,
and the UE prerequisite installer; no editor, Node.js, Python or separately installed
Unreal Engine is required to launch it. Windows system components and the bundled VC++
prerequisite installer remain standard platform requirements.

The JSON → CSV → DataTable mapping is documented in section 4.

`node tools/build-data.mjs` mirrors the six top-level JSON contracts and all five
`levels/*.json` campaign files into `Content/RedFront/Data` for the development loader.
Generate any missing UE maps from those source contracts with:

```powershell
UnrealEditor-Cmd.exe Unreal/RedFront1941.uproject -run=pythonscript `
  -script="Unreal/Content/Python/rf_generate_campaign_maps.py" -unattended -nop4
```

The generator preserves existing map geometry and adds/updates each ground actor's
`RF_LevelId` and `RF_Faction` tags, which the game mode uses to start the right level
contract. At runtime the game mode equips a faction rifle, reads enemy spawn marker
tags, and creates contract-driven infantry with AI controllers and weapons. Vehicle
markers create damageable armored blockout actors with contract HP and armor; impacts
resolve front versus side/rear armor against each hull's facing. Spawn counts are
bounded to 8 infantry per marker / 32 infantry per map and 4 vehicles per marker;
any trimmed counts are reported in the log. Validate the generated character Blueprints
and all 56 map packages with:

```powershell
UnrealEditor-Cmd.exe Unreal/RedFront1941.uproject -run=pythonscript `
  -script="Unreal/Content/Python/rf_validate_campaign_content.py" -unattended -nop4
```

  Vehicle actors now use each contract's speed, emplaced flag, reaction time, engagement
  range, and gun penetration to seek a target, maneuver, and fire through the shared
  ballistics system. Air markers spawn limited-lifetime 2D flyby actors: bomb contracts
  release one area attack, strafing contracts make repeated low-damage passes, and both
  can be shot down. These are functional blockout behaviors, not a finished vehicle or
  air-combat simulation.

  The material/Paper2D generator also creates Soviet and US player Blueprints plus a
German enemy Blueprint, binding each to its faction's directional idle flipbooks.
The German class is resolved at runtime so a clean content-generation pass does not
require the generated asset to exist during the GameMode class-default load.

Primary and optional objective markers are available from deployment. Plain interaction
objectives can be secured by approaching within 220 cm and pressing **E**; conditional
objectives cannot be skipped this way. `hold_position` contracts track continuous time
inside their authored radius (leaving resets progress), while `eliminate_count` contracts
count neutralized enemy units, optionally filtered by enemy contract IDs. The HUD shows
both kinds of progress. `reach_location` objectives complete on entering their marker
radius and can author marker positions in metres. Active objectives with a positive
`time_limit_s` fail when that mission-clock deadline elapses; map data validation requires
hold deadlines to exceed their hold duration.

These are playable blockouts, not finished hand-authored art levels: they use simple
2D collision geometry and contract-tagged `TargetPoint` markers. Character sheets and
their directional Paper2D flipbooks are procedural placeholders, ready to be replaced
by hand-painted assets.

### Endless mode in the UE build

Launch the packaged executable with `-endless` to use the current battle map as an
endless arena. The first wave becomes available after the 45-second preparation period;
press **Space** to start it. Each next wave unlocks after the previous wave is eliminated
and another 45-second resupply window expires. The runtime reads the sparse
`support.json` wave table, selects the latest authored composition, and scales
intervening/future waves using the contract's `4 + floor(wave * 1.6)` population curve
(capped at 64 units per wave). Every fifth wave receives a boss composition when one is
authored and the existing command-point bonus. Endless runs do not spawn the map's
scripted campaign enemies or finish because campaign objectives were completed; player
death still ends the run. Levels remain authored campaign blockouts rather than a
separately generated random-arena map.

---

## 2. Folder conventions

```
Unreal/
  Config/                      DefaultEngine / DefaultGame / DefaultInput
  Content/RedFront/
    Data/                      DT_* assets (and raw JSON mirrors for the dev loader)
    Art2D/
      Characters/              SPR_ / FLB_ per soldier animation and direction (Soviet / US / German)
      Weapons/
      Vehicles/
      Environment/
      UI/
      VFX/
    Levels/C01_* ... C05_*/    LV_<faction>_<index>_<Name> campaign blockouts
    Blueprints/                BP_ actors, DA_ data assets, AI, player
      Input/                   IMC_ / IA_ Enhanced Input assets
    Audio/
  Source/RedFront1941/
    Core/                      GameMode, GameState, PlayerController, Character, Loadout
    Survival/                  Weight, Survival, Inventory
    Combat/                    Weapon, Ballistics, Grenade, DamageTypes
    Support/                   Support subsystem, artillery, armour, air
    AI/                        Enemy AI controller, squad coordinator
    Data/                      Data types, DataTable loader
```

## 3. Naming conventions

| Prefix | Meaning | Example |
|---|---|---|
| `BP_` | Blueprint class | `BP_RFGrenade_F1` |
| `DA_` | Data asset | `DA_RFLevel_SOV_01` |
| `DT_` | DataTable | `DT_Weapons` |
| `SPR_` | Paper2D sprite | `SPR_SOV_Rifleman_Idle_E` |
| `FLB_` | PaperFlipbook | `FLB_SOV_Rifleman_Idle_E` |
| `LV_` | Level / map | `LV_SOV_01_BrestFortress` |
| `IMC_` / `IA_` | Enhanced Input context / action | `IMC_RF_Gameplay`, `IA_Fire` |
| `WBP_` | UMG widget | `WBP_RF_HudWeightBand` |
| `SFX_` / `MUS_` | Sound cue / music | `MUS_SOV_Stalingrad` |
| `MI_` / `M_` | Material instance / material | `MI_RF_SpriteUnlit` |

Suffixes: `_E`, `_NE`, `_N`, `_NW`, `_W`, `_SW`, `_S`, `_SE` are the eight facing
buckets of `ERFCharacterFacing` (see below).

---

## 4. The 2D art pipeline

High-resolution hand-drawn/painted art (not pixel art). One soldier or weapon is
authored as a sprite sheet with **8 facing columns** and one row per animation state.

```
painted frames (e.g. 512x512 per frame, PNG, straight alpha)
        |
        v
  sprite sheet (8 columns x N rows, power-of-two)
        |
        v
Paper2D: right-click sheet > Create Sprite  ->  SPR_<Unit>_<Anim>_<Facing>
        (pivot = bottom-centre = the feet; see PaperRuntimeSettings in DefaultEngine.ini)
        |
        v
Paper2D: select the 8 sprites of one animation > Right-click > Create Flipbook
        ->  FLB_<Unit>_<Anim>_<Facing>   (one flipbook per facing)
        |
        v
ARFCharacter.FacingFlipbooks[8]   indexed by ERFCharacterFacing
        |
        v
ARFCharacter::Tick computes the facing from the aim vector (not from velocity) with
ARFCharacter::FacingFromDirection2D, which buckets the planar angle into 8 x 45 degrees:
        East=0, NE=45, North=90, NW=135, West=180, SW=225, South=270, SE=315
        |
        v
ARFCharacter::RefreshFlipbookForState sets the flipbook for the current
(stance, facing) pair. Stances are stand / crouch / prone / sprint, so a complete
soldier set is 4 stance sets x 8 facings = up to 32 flipbooks; idle and walk rows are
separate animations inside each facings' set.
```

Notes for artists and TDs:

* **Screen-space axes:** +X is East (screen right), +Y is North (screen up), Z is
  height. The character sprite is rotated 0 degrees; facing changes which flipbook
  plays, never the actor rotation.
* **Pivot:** every sprite is pivoted at the feet so that a flipbook swap does not make
  the soldier "pop" vertically. `DefaultPivotPoint=(X=0.5,Y=1.0)` is set project-wide.
* **Material:** sprites use a masked (not translucent) unlit material by default.
  The project anti-aliases with TSR (`r.DefaultFeature.AntiAliasing=2`), which needs a
  temporally stable image; masked edges are stable, dithered alpha is not.
* **Resolution:** keep the character about 256-512 px tall at 1:1 ortho zoom; the camera
  is orthographic (`r.DefaultFeature` block in `DefaultEngine.ini`), so pixel density is
  constant and hand-painted detail survives at any zoom level.
* **Collision:** 2D actors use the `RF_2DSprite` profile (thin box on the plane);
  gameplay traces run on `RF_Plane`. Never enable complex collision on sprite geometry.

---

## 5. How `data\*.json` maps onto the DataTable CSVs

`tools/build-data.mjs` flattens each contract into one CSV per row struct under
`..\data\generated\`. The first column is always `Name` — `URFDataTableLoader` writes the
contract `id` into the DataTable row name, and that is what the importer expects — and the
remaining columns keep the JSON key names verbatim, so the importer is a straight match to
the `FRF*` structs instead of a hand-mapping exercise.

| JSON source | Generated CSV | Row struct | Notes |
|---|---|---|---|
| `weapons.json` → `weapons[]` | `weapons.csv` | `FRFWeaponDef` | `realism`→`RealismNote`, `note`→`DesignNote`; `category` also fills `Category`/`CategoryRaw` |
| `classes.json` → `classes[]` | `classes.csv` | `FRFClassDef` | `slots` is flattened into `slot_primary`/`slot_secondary`/`slot_grenade`/`slot_supply`; `passives`/`active` stay JSON-blob columns and are parsed at runtime |
| `classes.json` → `weight_system` | (not emitted) | `FRFWeightBand` | `load_bands` is read by `URFDataTableLoader::BuildClassTable` and applied by `URFWeightComponent` |
| `equipment.json` → `items[]` | `equipment.csv` | `FRFItemDef` | the whole `effect` object is a single column; `effect.*` keys (`damage`, `radius_m`, `fuse_s`, `calories`, `hydration`, `heal`, …) are parsed into flat fields by `FRFItemDef::FromJson` |
| `enemies.json` → `infantry[]` | `enemies_infantry.csv` | `FRFEnemyDef` | `ai.*` is flattened into `awareness_m`/`reaction_s`/`suppression_resist`/`calls_support` columns feeding `FRFEnemyAIProfile` |
| `enemies.json` → `vehicles[]` | `enemies_vehicles.csv` | `FRFVehicleDef` | `armor_mm` + `side_rear_ratio` are the armour contract used by `RFBallistics` |
| `enemies.json` → `air[]` | `enemies_air.csv` | `FRFAirUnitDef` | |
| `support.json` → `call_ins[]` | `support_callins.csv` | `FRFSupportCallInDef` | `type`→`CallType`, `weapon`/`penetration_mm` describe the delivered armour, not infantry |
| `campaigns.json` → `campaigns[]` | `campaigns.csv` | `FRFCampaignDef` | `levels:[a,b]` becomes `LevelRangeStart`/`LevelRangeEnd` |
| `campaigns.json` → `difficulty_scaling` | (read at runtime) | `FRFDifficultyScaling` | the drain multipliers are stated in prose in the contract and normalized in `RFDataTypes.cpp` |
| `levels/campaign_0*.json` → `levels[]` | `levels.csv` | `FRFLevelDef` | `objectives`, `enemy_composition`, `historical_accuracy`, `unlock_reward`, `endless_seed_modifier` are JSON-blob columns parsed at runtime; `author_note` is dropped |

Import walkthrough (repeat per CSV):

1. `Tools > Import DataTable`, choose the CSV (they are UTF-8 with a `Name` column).
2. Set *Row Struct* to the matching `FRF*` struct (they all derive from `FTableRowBase`).
3. Save as `Content/RedFront/Data/DT_<Name>` following the tree in
   `Content/RedFront/README.md` (`DT_Weapons`, `DT_Classes`, `DT_Items`, `DT_Enemies`,
   `DT_Vehicles`, `DT_AirUnits`, `DT_Support`, `DT_Campaigns`, `DT_Levels_*`).
4. The row name is the contract `id`, which is also what every other contract references.

---

## 6. Systems in one screen

* **Weight** (`URFWeightComponent`): total kg / capacity → `ERFLoadBand`
  (light ≤0.60, standard ≤0.85, heavy ≤1.00, overloaded >1.00) with the contract's
  speed and stamina multipliers. Overloaded disables sprint.
* **Survival** (`URFSurvivalComponent`): food, hydration, stamina, temperature, fatigue,
  blood loss; drains are per second on a 0-100 scale, scaled by the difficulty tier
  (veteran ×1.2, historical ×1.5). `EatItem`/`DrinkItem`/`UseMedical` resolve through
  `URFInventoryComponent`, and consuming water immediately reduces carried mass.
* **Combat** (`URFWeaponBase`, `RFBallistics`): rpm-driven fire modes, magazine and
  reserve ammo, ADS and bipod states, spread from accuracy + movement + load band +
  stance + stamina. Penetration is mm RHA vs `armor_mm` scaled by `side_rear_ratio`;
  ricochets past 70 degrees; damage falls off linearly to 35% at twice effective range.
* **Support** (`URFSupportSubsystem`): CP (start 2, max 10, 0.035/s × difficulty),
  per-call-in cooldowns, comms delay, radio-destroyed lockout (60 s) and line-break delay
  doubling. Artillery/armour/air missions are separate actors with their own timers.
* **AI** (`ARFEnemyAIController`, `ARFSquadCoordinator`): awareness radius, reaction
  time, suppression with `suppression_resist`, morale break chance, sniper relocation,
  support calls after `call_support_s`; squad-level bounding overwatch, MG42 base of
  fire, and a counter-attack trigger.

## 7. Quality rules for this codebase

* Every header: `#pragma once`, `#include "CoreMinimal.h"`, project includes, then the
  matching `*.generated.h` **last**.
* Every `.cpp`: its own header first.
* `UCLASS(ClassGroup=(RedFront), meta=(BlueprintSpawnableComponent))` for components.
* No raw `TArray<TSharedPtr<...>>` in `UPROPERTY` — JSON shared pointers stay in
  non-UPROPERTY locals or plain `TMap<FName, Struct>` members.
* Every public function documents its units (kg, m, s, cm, mm RHA, 0-100 scale) and its
  design intent, in the header.
