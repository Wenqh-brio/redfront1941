# Content tree — `Content/RedFront`

Intended layout for every RedFront1941 asset. Paths are relative to `Content/`.

```
RedFront/
  Data/
    DT_Weapons.uasset              weapons.json          (FRFWeaponDef)
    DT_Classes.uasset              classes.json          (FRFClassDef)
    DT_Items.uasset                equipment.json        (FRFItemDef)
    DT_Enemies.uasset              enemies.json infantry (FRFEnemyDef)
    DT_Vehicles.uasset             enemies.json vehicles (FRFVehicleDef)
    DT_AirUnits.uasset             enemies.json air      (FRFAirUnitDef)
    DT_Support.uasset              support.json call_ins (FRFSupportCallInDef)
    DT_Campaigns.uasset            campaigns.json        (FRFCampaignDef)
    DT_Difficulty.uasset           campaigns.json        (FRFDifficultyScaling)
    DT_Levels_SOV_41_42.uasset     levels/*.json slice   (FRFLevelDef)
    DT_Levels_SOV_42_43.uasset
    DT_Levels_SOV_43_44.uasset
    DT_Levels_SOV_44_45.uasset
    DT_Levels_US_44_45.uasset
    Raw/                           editor-only mirrors of ..\data\*.json (never cooked)
  Art2D/
    Characters/
      SOV/  Rifleman, Assault, MachineGunner, Sniper, AntiTank, Engineer, Medic,
            Scout, Mortarman  —  FLB_<Class>_<Anim>_<Facing> sets
      US/   same class set
      GER/  Landser, MP40 leader, MG42 team, Sniper, Panzerfaust, Pionier,
            Volkssturm, SS Grenadier, Fallschirmjager
      AUX/  Romanian, Hungarian, Italian Alpini
    Weapons/     SPR_/FLB_ per weapon: Mosin, SVT-40, PPSh-41, PPS-43, DP-27, Maxim,
                 PTRD-41, ROKS-2, Nagant, TT-33, M1 Garand, M1 Carbine, Thompson,
                 BAR, M1919A4, M1903A4, Bazooka, M1897, M2 flamethrower, M1911A1,
                 captured MP40 / StG 44 / Kar98k / MG42, sapper kit
    Vehicles/    Panzer III/IV/Tiger/Panther, StuG III G, Sd.Kfz.251/222, Pak 40,
                 Flak 36, T-34/76, ISU-152, M4A3 Sherman (+ destruction states)
    Environment/ Terrain sheets per level archetype: city ruin, forest, field, village,
                 river bank, rail yard, industrial, fortress, trench line
    UI/          WBP_* widgets, FLB_ cursor/compass, icon sheets (weapons, items, CP)
    VFX/         FLB_/NS_ muzzle flash, smoke, fire, dust, blood, explosions, tracers
  Levels/
    LV_SOV_01_… … LV_SOV_44_…   (44 Soviet levels)
    LV_US_45_…  … LV_US_56_…    (12 US levels)
    LV_SOV_Endless_Arena        (endless mode, see support.json endless_mode)
  Blueprints/
    BP_RFGameMode, BP_RFPlayerController, BP_RFCharacter_<Class>
    BP_RFWeapon_<Weapon>, BP_RFGrenade_<Item>, BP_RFProjectile
    BP_RFVehicle_<Vehicle>, BP_RFEmplacement_<Gun>
    BP_RFResupplyPoint, BP_RFRadio, BP_RFObjective
    AI/  BP_RFEnemyAI_<Unit>, BP_RFSquadCoordinator, BT_/BB_ assets
    Input/ IMC_RF_Gameplay, IMC_RF_Support, IMC_RF_Map and IA_* actions
  Audio/
    Weapons/, Ambience/, Footsteps/, Voice_SOV/, Voice_US/, Voice_GER/, MUS_<Level>
```

## Level assets to create

The 56 level contracts now exist under `..\data\levels\campaign_0*.json` (44 Soviet +
12 US), so the asset names below are the **authoritative** ones derived from each
contract's `id` and `name_en`. Create one `LV_` map per row.

### Naming rule

```
LV_<FACTION>_<II>_<PascalCaseShortName>

FACTION : SOV | US                         (data/levels/*.json "faction")
II      : two-digit global index 01..56    (data/levels/*.json "index")
NAME    : PascalCase contraction of the contract's "name_en"
          (drop articles and trailing clauses: "The", "of the", ": ..."; 3-6 words;
          no spaces, no underscores, drop diacritics)
          e.g. "Brest Fortress: A Dawn Without Orders" -> BrestFortress
```

The sync tool (or `Tools > RedFront > Sync Level List` once the editor helper lands)
reads each level's `id`/`name_en`, creates the matching empty `LV_` map and the
companion `DA_RFLevel_<index>` data asset, and adds the map to the packaging list. The
rule above is what that tool implements, so hand-created assets stay consistent.

### Campaign → index ranges (from `data/campaigns.json`)

| Campaign id | Faction | Levels (global index) | Count |
|---|---|---|---|
| `sov_barbarossa_1941` | SOV | 1–12 | 12 |
| `sov_stalingrad_1942` | SOV | 13–22 | 10 |
| `sov_kursk_1943` | SOV | 23–32 | 10 |
| `sov_berlin_1945` | SOV | 33–44 | 12 |
| `us_western_1945` | US | 45–56 | 12 |
| **Total** | | 1–56 | **56** |

### The 56 level assets

| Index | Level asset | Contract id | Mission type | Weight budget (kg) |
|---|---|---|---|---|
| 01 | `LV_SOV_01_BrestFortress` | `sov_01_brest_fortress` | defense | 26 |
| 02 | `LV_SOV_02_DubnoCrossroads` | `sov_02_raze_dubno` | armored | 25 |
| 03 | `LV_SOV_03_MinskPocket` | `sov_03_minsk_pocket` | breakthrough | 22 |
| 04 | `LV_SOV_04_Smolensk` | `sov_04_smolensk` | delay | 24 |
| 05 | `LV_SOV_05_Yelnya` | `sov_05_yelnya` | assault | 25 |
| 06 | `LV_SOV_06_KievPocket` | `sov_06_kiev_pocket` | breakthrough | 23 |
| 07 | `LV_SOV_07_VyazmaBryansk` | `sov_07_vyazma_bryansk` | delay | 23 |
| 08 | `LV_SOV_08_MozhaiskLine` | `sov_08_mozhaisk_line` | defense | 24 |
| 09 | `LV_SOV_09_TulaDefense` | `sov_09_tula_defense` | urban_clearing | 24 |
| 10 | `LV_SOV_10_Dubosekovo` | `sov_10_dubosekovo` | ambush | 25 |
| 11 | `LV_SOV_11_MoscowCounter` | `sov_11_moscow_counter` | assault | 24 |
| 12 | `LV_SOV_12_RzhevVyazma` | `sov_12_rzhev_vyazma` | siege | 23 |
| 13 | `LV_SOV_13_BarvenkovoPocket` | `sov_13_barvenkovo_pocket` | delay | 24 |
| 14 | `LV_SOV_14_DonBendLine` | `sov_14_don_bend` | defense | 25 |
| 15 | `LV_SOV_15_VolgaCrossing` | `sov_15_volga_crossing` | river_crossing | 22 |
| 16 | `LV_SOV_16_TractorFactory` | `sov_16_tractor_factory` | urban_clearing | 26 |
| 17 | `LV_SOV_17_BarrikadyRedOctober` | `sov_17_barrikady_red_october` | siege | 26 |
| 18 | `LV_SOV_18_PavlovHouse` | `sov_18_pavlov_house` | defense | 24 |
| 19 | `LV_SOV_19_LyudnikovIsland` | `sov_19_lyudnikov_island` | siege | 23 |
| 20 | `LV_SOV_20_OperationUranus` | `sov_20_operation_uranus` | breakthrough | 25 |
| 21 | `LV_SOV_21_MyshkovaLine` | `sov_21_winter_storm` | delay | 24 |
| 22 | `LV_SOV_22_OperationRing` | `sov_22_operation_ring` | assault | 26 |
| 23 | `LV_SOV_23_OboyanMinefield` | `sov_23_oboyan_minefield` | defense | 24 |
| 24 | `LV_SOV_24_ProkhorovkaClash` | `sov_24_prokhorovka_clash` | armored | 26 |
| 25 | `LV_SOV_25_BelgorodBreakthrough` | `sov_25_belgorod_breakthrough` | breakthrough | 25 |
| 26 | `LV_SOV_26_DnieperBridgehead` | `sov_26_dnieper_bridgehead` | river_crossing | 24 |
| 27 | `LV_SOV_27_KievLiberation` | `sov_27_kiev_liberation` | urban_clearing | 23 |
| 28 | `LV_SOV_28_KorsunPocket` | `sov_28_korsun_pocket` | siege | 26 |
| 29 | `LV_SOV_29_NovgorodRelief` | `sov_29_novgorod_relief` | relief | 23 |
| 30 | `LV_SOV_30_VitebskEncirclement` | `sov_30_vitebsk_encirclement` | breakthrough | 25 |
| 31 | `LV_SOV_31_MinskCauldron` | `sov_31_minsk_cauldron` | assault | 26 |
| 32 | `LV_SOV_32_VistulaBridgehead` | `sov_32_vistula_bridgehead` | river_crossing | 25 |
| 33 | `LV_SOV_33_IasiChisinau` | `sov_33_iasi_kishinev` | breakthrough | 24 |
| 34 | `LV_SOV_34_RigaKurland` | `sov_34_riga_kurland` | assault | 26 |
| 35 | `LV_SOV_35_BudapestSiege` | `sov_35_budapest_siege` | siege | 25 |
| 36 | `LV_SOV_36_VistulaOder` | `sov_36_vistula_oder` | breakthrough | 24 |
| 37 | `LV_SOV_37_Konigsberg` | `sov_37_konigsberg` | siege | 26 |
| 38 | `LV_SOV_38_KustrinBridgehead` | `sov_38_kustrin_bridgehead` | defense | 26 |
| 39 | `LV_SOV_39_ZeelowHeights` | `sov_39_zeelow_heights` | breakthrough | 25 |
| 40 | `LV_SOV_40_BerlinSpandau` | `sov_40_berlin_spandau` | assault | 26 |
| 41 | `LV_SOV_41_BerlinUBahn` | `sov_41_berlin_ubahn` | urban_clearing | 27 |
| 42 | `LV_SOV_42_Reichstag` | `sov_42_reichstag` | assault | 26 |
| 43 | `LV_SOV_43_TorgauElbe` | `sov_43_torgau_elbe` | relief | 23 |
| 44 | `LV_SOV_44_PragueOffensive` | `sov_44_prague_offensive` | breakthrough | 24 |
| 45 | `LV_US_45_OmahaBeach` | `us_45_omaha_beach` | amphibious | 26 |
| 46 | `LV_US_46_CarentanPurpleHeart` | `us_46_carentan_purple_heart` | assault | 26 |
| 47 | `LV_US_47_CherbourgFortress` | `us_47_cherbourg_fortress` | siege | 28 |
| 48 | `LV_US_48_SaintLôBocage` | `us_48_saint_lo_bocage` | assault | 25.5 |
| 49 | `LV_US_49_OperationCobra` | `us_49_operation_cobra` | breakthrough | 26 |
| 50 | `LV_US_50_MortainHill317` | `us_50_mortain_hill_317` | defense | 27 |
| 51 | `LV_US_51_FalaisePocket` | `us_51_falaise_pocket` | breakthrough | 26.5 |
| 52 | `LV_US_52_LiberationOfParis` | `us_52_liberation_of_paris` | urban_clearing | 24 |
| 53 | `LV_US_53_HurtgenForest` | `us_53_hurtgen_forest` | assault | 23 |
| 54 | `LV_US_54_AachenStreets` | `us_54_aachen_streets` | urban_clearing | 26 |
| 55 | `LV_US_55_BastogneSnowLine` | `us_55_bastogne_snow_line` | defense | 24 |
| 56 | `LV_US_56_RemagenBridge` | `us_56_remagen_bridge` | river_crossing | 26 |

Plus one non-campaign asset for endless mode: `LV_SOV_Endless_Arena` (generated terrain,
see `support.json endless_mode`).

### Five fully-worked examples

If only a handful of levels are stubbed first, create these five; they cover one campaign
opener, one winter siege, one urban clearing, one fortress/assault finale and one
amphibious landing, which exercises the widest set of `ERFMissionType` branches.

| Index | Asset | Contract id | Campaign | Mission type | Notes for the level designer |
|---|---|---|---|---|---|
| 01 | `LV_SOV_01_BrestFortress` | `sov_01_brest_fortress` | `sov_barbarossa_1941` | `defense` | Teaches weight and bolt-action fire; 26 kg budget; fortress interior rooms are separate paper-sprite backdrops. |
| 12 | `LV_SOV_12_RzhevVyazma` | `sov_12_rzhev_vyazma` | `sov_barbarossa_1941` | `siege` | Campaign finale; deep snow and temperature drain, first tank support. |
| 22 | `LV_SOV_22_OperationRing` | `sov_22_operation_ring` | `sov_stalingrad_1942` | `assault` | Urban ruin fighting; the archetypal house-to-house layout with rubble passages. |
| 42 | `LV_SOV_42_Reichstag` | `sov_42_reichstag` | `sov_berlin_1945` | `assault` | Berlin finale; every support unlocked, ISU-152 direct fire, multi-floor interior. |
| 45 | `LV_US_45_OmahaBeach` | `us_45_omaha_beach` | `us_western_1945` | `amphibious` | US campaign opener; teaches the 26 kg budget and the faster 105 mm response (30 s delay). |

Each level asset is expected to carry a `DA_RFLevel_<index>` data asset holding the
matching `FRFLevelDef` row (objective texts, enemy composition, support list, historical
notes) so the level blueprint stays thin and the contract stays testable. The row name in
`DT_Levels_*` must be the contract `id` (e.g. `sov_01_brest_fortress`), because
`ARFGameMode::StartLevelById` and every `campaign_id` reference resolve through it.
