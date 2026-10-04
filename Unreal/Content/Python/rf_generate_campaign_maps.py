#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Build one UE 5.8 top-down blockout map for each campaign level contract."""

import glob
import json
import os
import traceback

import unreal


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
LEVEL_DATA = os.path.join(ROOT, "data", "levels")
LEVEL_ROOT = "/Game/RedFront/Levels"
CAMPAIGN_FOLDERS = {
    "sov_barbarossa_1941": "C01_Barbarossa_1941",
    "sov_stalingrad_1942": "C02_Stalingrad_1942",
    "sov_kursk_1943": "C03_Kursk_Dnieper_1943",
    "sov_berlin_1945": "C04_Bagration_Berlin_1944",
    "us_western_1945": "C05_US_Western_1944_45",
}

LOG_PATH = os.path.join(os.path.dirname(__file__), "rf_campaign_maps_report.txt")


def log(message):
    unreal.log("[RedFrontMaps] " + message)


def short_name(level):
    faction = "US" if level["faction"] == "us" else "SOV"
    words = level["id"].split("_")[2:]
    suffix = "".join(word[:1].upper() + word[1:] for word in words)
    return "LV_%s_%02d_%s" % (faction, level["number_in_campaign"], suffix)


def add_tags(actor, values):
    actor.tags = [unreal.Name(value) for value in values]


def spawn_marker(actor_class, label, location, tags):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, unreal.Vector(location[0], location[1], location[2]), unreal.Rotator())
    if actor:
        actor.set_actor_label(label)
        add_tags(actor, tags)
    return actor


def spawn_cube(label, location, scale, tags, cube_mesh):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator())
    if not actor:
        return None
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.static_mesh_component.set_static_mesh(cube_mesh)
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
    add_tags(actor, tags)
    return actor


def sync_rescue_markers(level):
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    for actor in actors:
        if actor.get_actor_label().startswith(("RF_Rescue_", "RF_RescueVisual_")):
            if not unreal.EditorLevelLibrary.destroy_actor(actor):
                raise RuntimeError("Could not replace existing survivor marker: "
                                   + actor.get_actor_label())

    cylinder_mesh = unreal.load_object(None, "/Engine/BasicShapes/Cylinder.Cylinder")
    rescue_index = 0
    for objective in level.get("objectives", []):
        for point in objective.get("rescue_points", []):
            if not cylinder_mesh:
                raise RuntimeError("Could not load cylinder mesh for rescue markers")
            rescue_index += 1
            marker_tag = "RF_RescueMarker:%s:%02d" % (objective["id"], rescue_index)
            x = float(point["x_m"]) * 100.0
            y = float(point["y_m"]) * 100.0
            marker = spawn_marker(unreal.TargetPoint,
                                  "RF_Rescue_%s_%02d" % (objective["id"], rescue_index),
                                  (x, y, 48),
                                  ["RF_RescueFor:" + objective["id"], marker_tag])
            visual = unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(x, y, 100), unreal.Rotator())
            if not marker or not visual:
                raise RuntimeError("Could not create survivor marker %s/%02d" % (
                    objective["id"], rescue_index))
            visual.set_actor_label(
                "RF_RescueVisual_%s_%02d" % (objective["id"], rescue_index))
            visual.static_mesh_component.set_static_mesh(cylinder_mesh)
            visual.static_mesh_component.set_collision_profile_name("NoCollision")
            visual.set_actor_scale3d(unreal.Vector(0.25, 0.25, 1.2))
            add_tags(visual, ["RF_RescueVisual", marker_tag])


def sync_resupply_markers(level, cube_mesh):
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    for actor in actors:
        if actor.get_actor_label().startswith(("RF_Resupply_", "RF_ResupplyVisual_")):
            if not unreal.EditorLevelLibrary.destroy_actor(actor):
                raise RuntimeError("Could not replace existing resupply marker: "
                                   + actor.get_actor_label())

    for index in range(level.get("resupply_points", 0)):
        x = -2500 + index * 1250
        y = 350
        point_tag = "RF_ResupplyPoint:%02d" % (index + 1)
        marker = spawn_marker(unreal.TargetPoint, "RF_Resupply_%02d" % (index + 1),
                              (x, y, 48), ["RF_Resupply", point_tag])
        visual = spawn_cube("RF_ResupplyVisual_%02d" % (index + 1),
                            (x, y, 24), (0.45, 0.45, 0.45),
                            ["RF_ResupplyVisual", point_tag], cube_mesh)
        if not marker or not visual:
            raise RuntimeError("Could not create resupply marker %02d" % (index + 1))


def sync_destroy_target_actors(level):
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    for actor in actors:
        if actor.get_actor_label().startswith("RF_DestroyTarget_"):
            if not unreal.EditorLevelLibrary.destroy_actor(actor):
                raise RuntimeError("Could not replace existing destroy target: "
                                   + actor.get_actor_label())

    target_class = unreal.load_class(None, "/Script/RedFront1941.RFObjectiveTargetActor")
    if not target_class:
        raise RuntimeError("Could not load RFObjectiveTargetActor")

    for objective in level.get("objectives", []):
        for index, point in enumerate(objective.get("target_points", []), 1):
            x = float(point["x_m"]) * 100.0
            y = float(point["y_m"]) * 100.0
            target = unreal.EditorLevelLibrary.spawn_actor_from_class(
                target_class, unreal.Vector(x, y, 58), unreal.Rotator())
            if not target:
                raise RuntimeError("Could not spawn destroy target %s/%02d" % (
                    objective["id"], index))
            target.set_actor_label("RF_DestroyTarget_%s_%02d" % (
                objective["id"], index))
            target.initialize_target(unreal.Name(objective["id"]),
                                     float(point.get("health", 300.0)),
                                     unreal.Name(point.get("name_zh", objective["id"])))


def build_map(level, level_path, cube_mesh):
    folder = os.path.dirname(level_path)
    unreal.EditorAssetLibrary.make_directory(folder)
    if unreal.EditorAssetLibrary.does_asset_exist(level_path):
        if not unreal.EditorLoadingAndSavingUtils.load_map(level_path):
            raise RuntimeError("Could not load existing map: " + level_path)
        status = "updated"
    else:
        if not unreal.EditorLevelLibrary.new_level(level_path):
            raise RuntimeError("EditorLevelLibrary.new_level failed: " + level_path)
        status = "created"

    ground = None
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == "RF_Ground":
            ground = actor
            break
    if not ground:
        raise RuntimeError("Missing RF_Ground actor: " + level_path)
    ground_tags = {str(tag) for tag in ground.tags}
    ground_tags.add("RF_LevelId:" + level["id"])
    ground_tags.add("RF_Faction:" + level["faction"])
    ground.tags = [unreal.Name(tag) for tag in sorted(ground_tags)]

    if status == "updated":
        objectives = level.get("objectives", [])
        for index, objective in enumerate(objectives):
            label = "RF_Objective_%02d" % (index + 1)
            marker = next((actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
                           if actor.get_actor_label() == label), None)
            if not marker:
                raise RuntimeError("Missing objective marker %s in %s" % (label, level_path))
            tags = {str(tag) for tag in marker.tags}
            tags.update([
                "RF_Objective:" + objective["id"],
                "RF_Required:" + str(bool(objective.get("required", False))).lower(),
                "RF_ObjectiveText:" + objective.get("text_zh", ""),
            ])
            marker.tags = [unreal.Name(tag) for tag in sorted(tags)]
            if "marker_x_m" in objective and "marker_y_m" in objective:
                marker.set_actor_location(unreal.Vector(
                    float(objective["marker_x_m"]) * 100.0,
                    float(objective["marker_y_m"]) * 100.0,
                    marker.get_actor_location().z), False, False)
        sync_rescue_markers(level)
        sync_resupply_markers(level, cube_mesh)
        sync_destroy_target_actors(level)
        if not unreal.EditorLevelLibrary.save_current_level():
            raise RuntimeError("save_current_level failed: " + level_path)
        return "updated"

    spawn_cube("RF_Ground", (0, 0, -12), (80, 50, 0.24),
               ["RF_Terrain:" + level["terrain"], "RF_Weather:" + level["weather"],
                "RF_LevelId:" + level["id"], "RF_Faction:" + level["faction"]], cube_mesh)
    spawn_marker(unreal.PlayerStart, "RF_PlayerStart",
                 (-3400, 0, 88), ["RF_PlayerRole:" + level["player_role_zh"]])

    # Consistent route anchors keep the prototype-sized battle space readable while
    # the terrain and mission type shape each historical map's cover layout.
    mission_type = level["mission_type"]
    terrain = level["terrain"]
    cover_count = 10 + min(level["difficulty"], 10)
    if terrain in ("city", "industrial", "fortress"):
        for index in range(cover_count):
            x = -2200 + (index % 5) * 1050
            y = -1350 + (index // 5) * 2700
            scale = (2.0 if terrain == "fortress" else 1.35, 0.48, 0.9)
            spawn_cube("RF_Cover_%02d" % (index + 1), (x, y, 45), scale,
                       ["RF_Cover", "RF_Terrain:" + terrain], cube_mesh)
    elif terrain in ("river",):
        for index in range(7):
            x = -1600 + index * 520
            spawn_cube("RF_Riverbank_%02d" % (index + 1), (x, 1050, 35),
                       (2.2, 0.35, 0.7), ["RF_Cover", "RF_Riverbank"], cube_mesh)
            spawn_cube("RF_Riverbank_South_%02d" % (index + 1), (x, -1050, 35),
                       (2.2, 0.35, 0.7), ["RF_Cover", "RF_Riverbank"], cube_mesh)
        spawn_cube("RF_Bridge", (0, 0, 8), (3.0, 0.55, 0.12),
                   ["RF_Bridge", "RF_ObjectiveRoute"], cube_mesh)
    elif terrain in ("trench", "field", "village"):
        for index in range(cover_count):
            x = -2450 + (index % 5) * 1150
            y = -1250 + (index // 5) * 2450
            scale = (1.6, 0.32, 0.42) if terrain == "trench" else (0.75, 0.65, 0.55)
            spawn_cube("RF_FieldCover_%02d" % (index + 1), (x, y, 30), scale,
                       ["RF_Cover", "RF_Terrain:" + terrain], cube_mesh)
    else:
        for index in range(cover_count):
            x = -2300 + (index % 5) * 1120
            y = -1200 + (index // 5) * 2400
            spawn_cube("RF_Cover_%02d" % (index + 1), (x, y, 40), (0.8, 0.65, 0.8),
                       ["RF_Cover", "RF_Terrain:" + terrain], cube_mesh)

    if mission_type in ("defense", "delay", "siege", "relief"):
        spawn_cube("RF_Defense_Line", (0, 0, 32), (0.3, 17.0, 0.55),
                   ["RF_DefenseLine", "RF_Mission:" + mission_type], cube_mesh)
    elif mission_type in ("river_crossing", "amphibious"):
        spawn_cube("RF_Crossing_Approach", (0, 0, 12), (18.0, 0.42, 0.12),
                   ["RF_Crossing", "RF_Mission:" + mission_type], cube_mesh)
    else:
        spawn_cube("RF_Assault_Axis", (200, 0, 8), (22.0, 0.16, 0.08),
                   ["RF_AssaultAxis", "RF_Mission:" + mission_type], cube_mesh)

    objectives = level.get("objectives", [])
    for index, objective in enumerate(objectives):
        x = -1800 + index * 950
        y = 850 if index % 2 == 0 else -850
        if "marker_x_m" in objective and "marker_y_m" in objective:
            x = float(objective["marker_x_m"]) * 100.0
            y = float(objective["marker_y_m"]) * 100.0
        required = str(bool(objective.get("required", False))).lower()
        spawn_marker(unreal.TargetPoint, "RF_Objective_%02d" % (index + 1),
                     (x, y, 48), ["RF_Objective:" + objective["id"],
                                  "RF_Required:" + required,
                                  "RF_ObjectiveText:" + objective.get("text_zh", "")])

    sync_rescue_markers(level)
    sync_resupply_markers(level, cube_mesh)
    sync_destroy_target_actors(level)

    composition = level.get("enemy_composition", {})
    enemy_entries = list(composition.items())
    for index, (enemy_id, count) in enumerate(enemy_entries):
        x = 700 + (index % 3) * 720
        y = -1450 + (index // 3) * 1450
        spawn_marker(unreal.TargetPoint, "RF_EnemySpawn_%02d" % (index + 1),
                     (x, y, 48), ["RF_EnemyClass:" + enemy_id,
                                  "RF_EnemyCount:" + str(count)])

    if level.get("available_support"):
        spawn_marker(unreal.TargetPoint, "RF_Radio", (-2800, -350, 48),
                     ["RF_Radio", "RF_Support:" + ",".join(level["available_support"])])

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("save_current_level failed: " + level_path)
    return "created"


def main():
    try:
        cube_mesh = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")
        if not cube_mesh:
            raise RuntimeError("Could not load /Engine/BasicShapes/Cube.Cube")

        created = 0
        existing = 0
        failures = []
        for json_path in sorted(glob.glob(os.path.join(LEVEL_DATA, "campaign_*.json"))):
            with open(json_path, encoding="utf-8") as stream:
                contract = json.load(stream)
            campaign_folder = CAMPAIGN_FOLDERS[contract["campaign_id"]]
            for level in contract["levels"]:
                package = "%s/%s/%s" % (LEVEL_ROOT, campaign_folder, short_name(level))
                try:
                    result = build_map(level, package, cube_mesh)
                    if result == "created":
                        created += 1
                    else:
                        existing += 1
                except Exception:
                    failures.append("%s\n%s" % (level["id"], traceback.format_exc()))
                    log("FAILED " + level["id"])

        summary = "Created %d; already existed %d; failed %d" % (
            created, existing, len(failures))
        with open(LOG_PATH, "w", encoding="utf-8") as stream:
            stream.write(summary + "\n\n" + "\n".join(failures))
        log(summary)
        log("Report: " + LOG_PATH)
        if failures:
            raise RuntimeError(summary)
    except Exception:
        log(traceback.format_exc())
        raise


main()
