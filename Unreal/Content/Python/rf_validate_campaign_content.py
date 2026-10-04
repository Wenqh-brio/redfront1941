#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Load and verify all generated campaign maps and faction player characters in UE."""

import glob
import json
import os
import traceback

import unreal


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
LEVEL_DATA = os.path.join(ROOT, "data", "levels")
RUNTIME_LEVEL_DATA = os.path.join(ROOT, "Unreal", "Content", "RedFront", "Data", "levels")
ENEMY_DATA = os.path.join(ROOT, "data", "enemies.json")
CAMPAIGN_FOLDERS = {
    "sov_barbarossa_1941": "C01_Barbarossa_1941",
    "sov_stalingrad_1942": "C02_Stalingrad_1942",
    "sov_kursk_1943": "C03_Kursk_Dnieper_1943",
    "sov_berlin_1945": "C04_Bagration_Berlin_1944",
    "us_western_1945": "C05_US_Western_1944_45",
}


def map_asset_name(level):
    faction = "US" if level["faction"] == "us" else "SOV"
    suffix = "".join(word[:1].upper() + word[1:] for word in level["id"].split("_")[2:])
    return "LV_%s_%02d_%s" % (faction, level["number_in_campaign"], suffix)


def validate_characters():
    for faction, role in (("SOV", "PlayerCharacter"), ("US", "PlayerCharacter"),
                          ("GER", "EnemyCharacter")):
        asset_path = "/Game/RedFront/Blueprints/Characters/BP_%s_%s" % (faction, role)
        blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not blueprint:
            raise RuntimeError("Missing character Blueprint: " + asset_path)
        pawn_class = blueprint.generated_class()
        default_object = unreal.get_default_object(pawn_class)
        flipbooks = default_object.get_editor_property("facing_flipbooks")
        if len(flipbooks) != 8 or any(not book or book.get_num_frames() < 1 for book in flipbooks):
            raise RuntimeError("%s character does not have eight animated facing slots" % faction)
        unreal.log("[RedFrontValidate] %s %s pawn: 8 animated facing slots" % (faction, role))

    game_mode_class = unreal.load_class(None, "/Script/RedFront1941.RFGameMode")
    if not game_mode_class:
        raise RuntimeError("Could not load ARFGameMode")
    game_mode_default = unreal.get_default_object(game_mode_class)
    for faction in ("Soviet", "US"):
        field = "%s_player_character_class" % faction.lower()
        pawn_class = game_mode_default.get_editor_property(field)
        if not pawn_class:
            raise RuntimeError("ARFGameMode has no %s player Blueprint" % faction)
        unreal.log("[RedFrontValidate] GameMode %s pawn: %s" % (faction, pawn_class.get_name()))
    german_enemy_class = unreal.load_class(
        None,
        "/Game/RedFront/Blueprints/Characters/BP_GER_EnemyCharacter.BP_GER_EnemyCharacter_C",
    )
    if not german_enemy_class:
        raise RuntimeError("Could not load German enemy Blueprint class")
    unreal.log("[RedFrontValidate] GameMode runtime German pawn: %s" % german_enemy_class.get_name())


def validate_maps():
    with open(ENEMY_DATA, encoding="utf-8") as stream:
        enemy_contracts = json.load(stream)
    supported_enemy_ids = {
        entry["id"]
        for group in ("infantry", "vehicles", "air")
        for entry in enemy_contracts[group]
    }
    total = 0
    objective_total = 0
    for json_path in sorted(glob.glob(os.path.join(LEVEL_DATA, "campaign_*.json"))):
        with open(json_path, encoding="utf-8") as stream:
            contract = json.load(stream)
        runtime_path = os.path.join(RUNTIME_LEVEL_DATA, os.path.basename(json_path))
        with open(runtime_path, encoding="utf-8") as stream:
            runtime_contract = json.load(stream)
        runtime_levels = {level["id"]: level for level in runtime_contract["levels"]}
        folder = CAMPAIGN_FOLDERS[contract["campaign_id"]]
        for level in contract["levels"]:
            runtime_level = runtime_levels.get(level["id"])
            if runtime_level is None:
                raise RuntimeError("Missing runtime contract for " + level["id"])
            runtime_objectives = {
                objective["id"]: objective
                for objective in runtime_level.get("objectives", [])
            }
            for objective in level.get("objectives", []):
                runtime_objective = runtime_objectives.get(objective["id"])
                if runtime_objective is None:
                    raise RuntimeError("Missing runtime objective %s in %s" % (
                        objective["id"], level["id"]))
                for field in ("condition", "hold_time_s", "radius_m",
                              "target_count", "target_enemy_ids",
                              "marker_x_m", "marker_y_m", "rescue_points",
                              "target_points"):
                    if runtime_objective.get(field) != objective.get(field):
                        raise RuntimeError("Runtime objective field %s is stale for %s/%s" % (
                            field, level["id"], objective["id"]))

            name = map_asset_name(level)
            package = "/Game/RedFront/Levels/%s/%s" % (folder, name)
            world = unreal.EditorLoadingAndSavingUtils.load_map(package)
            if not world:
                raise RuntimeError("Could not load campaign map: " + package)

            actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            if not actor_subsystem:
                raise RuntimeError("Could not access the UE editor actor subsystem")
            actors = actor_subsystem.get_all_level_actors()
            labels = {actor.get_actor_label() for actor in actors}
            if "RF_Ground" not in labels or "RF_PlayerStart" not in labels:
                raise RuntimeError("Missing ground or player start in " + package)
            ground = next(actor for actor in actors if actor.get_actor_label() == "RF_Ground")
            ground_tags = {str(tag) for tag in ground.tags}
            if "RF_LevelId:" + level["id"] not in ground_tags:
                raise RuntimeError("Missing level contract tag in " + package)
            if "RF_Faction:" + level["faction"] not in ground_tags:
                raise RuntimeError("Missing faction tag in " + package)
            objectives = sum(1 for label in labels if label.startswith("RF_Objective_"))
            if objectives != len(level.get("objectives", [])):
                raise RuntimeError("Objective marker mismatch in " + package)
            resupply_markers = sum(
                1 for label in labels
                if label.startswith("RF_Resupply_")
                and not label.startswith("RF_ResupplyVisual_"))
            if resupply_markers != level.get("resupply_points", 0):
                raise RuntimeError("Resupply marker mismatch in " + package)
            resupply_visuals = sum(
                1 for label in labels if label.startswith("RF_ResupplyVisual_"))
            if resupply_visuals != resupply_markers:
                raise RuntimeError("Resupply visual mismatch in " + package)
            target_actors = [actor for actor in actors
                             if actor.get_actor_label().startswith("RF_DestroyTarget_")]
            for point_index in range(1, resupply_markers + 1):
                point_tag = "RF_ResupplyPoint:%02d" % point_index
                matching_resupply = next((actor for actor in actors
                                          if point_tag in {str(tag) for tag in actor.tags}
                                          and "RF_Resupply" in {str(tag) for tag in actor.tags}), None)
                if matching_resupply is None:
                    raise RuntimeError("Missing resupply point %02d in %s" % (
                        point_index, package))
                location = matching_resupply.get_actor_location()
                if abs(location.x - (-2500 + (point_index - 1) * 1250)) > 1.0 \
                        or abs(location.y - 350.0) > 1.0:
                    raise RuntimeError("Resupply point position mismatch %02d in %s" % (
                        point_index, package))
            objective_tags = {
                str(tag)
                for actor in actors
                for tag in actor.tags
                if str(tag).startswith("RF_Objective:")
            }
            for objective in level.get("objectives", []):
                matching_marker = next((actor for actor in actors
                                        if "RF_Objective:" + objective["id"]
                                        in {str(tag) for tag in actor.tags}), None)
                if matching_marker is None:
                    raise RuntimeError("Missing objective ID marker %s in %s" % (
                        objective["id"], package))
                if "marker_x_m" in objective and "marker_y_m" in objective:
                    location = matching_marker.get_actor_location()
                    expected_x = float(objective["marker_x_m"]) * 100.0
                    expected_y = float(objective["marker_y_m"]) * 100.0
                    if abs(location.x - expected_x) > 1.0 or abs(location.y - expected_y) > 1.0:
                        raise RuntimeError("Objective marker position mismatch for %s in %s" % (
                            objective["id"], package))
                rescue_points = objective.get("rescue_points", [])
                for point_index, point in enumerate(rescue_points, 1):
                    expected_tag = "RF_RescueFor:" + objective["id"]
                    matching_rescue = next((actor for actor in actors
                                            if expected_tag in {str(tag) for tag in actor.tags}
                                            and ("RF_RescueMarker:%s:%02d" % (
                                                objective["id"], point_index))
                                            in {str(tag) for tag in actor.tags}), None)
                    if matching_rescue is None:
                        raise RuntimeError("Missing survivor marker %s/%02d in %s" % (
                            objective["id"], point_index, package))
                    location = matching_rescue.get_actor_location()
                    if abs(location.x - float(point["x_m"]) * 100.0) > 1.0 \
                            or abs(location.y - float(point["y_m"]) * 100.0) > 1.0:
                        raise RuntimeError("Survivor marker position mismatch for %s/%02d in %s" % (
                            objective["id"], point_index, package))
                actual_rescue_markers = sum(
                    1 for actor in actors
                    if "RF_RescueFor:" + objective["id"]
                    in {str(tag) for tag in actor.tags})
                if actual_rescue_markers != len(rescue_points):
                    raise RuntimeError("Survivor marker count mismatch for %s in %s" % (
                        objective["id"], package))
                target_points = objective.get("target_points", [])
                objective_targets = [
                    actor for actor in target_actors
                    if "RF_DestroyTargetFor:" + objective["id"]
                    in {str(tag) for tag in actor.tags}
                ]
                if len(objective_targets) != len(target_points):
                    raise RuntimeError("Destroy target count mismatch for %s in %s" % (
                        objective["id"], package))
                for target_index, point in enumerate(target_points, 1):
                    target_label = "RF_DestroyTarget_%s_%02d" % (
                        objective["id"], target_index)
                    target = next((actor for actor in objective_targets
                                   if actor.get_actor_label() == target_label), None)
                    if target is None:
                        raise RuntimeError("Missing destroy target %s in %s" % (
                            target_label, package))
                    location = target.get_actor_location()
                    if abs(location.x - float(point["x_m"]) * 100.0) > 1.0 \
                            or abs(location.y - float(point["y_m"]) * 100.0) > 1.0:
                        raise RuntimeError("Destroy target position mismatch for %s in %s" % (
                            target_label, package))
                    if abs(target.get_max_health() - float(point.get("health", 300.0))) > 0.1:
                        raise RuntimeError("Destroy target health mismatch for %s in %s" % (
                            target_label, package))
                    if str(target.get_display_name()) != point.get(
                            "name_zh", objective["id"]):
                        raise RuntimeError("Destroy target display name mismatch for %s in %s" % (
                            target_label, package))
            expected_destroy_targets = sum(
                len(objective.get("target_points", []))
                for objective in level.get("objectives", []))
            if len(target_actors) != expected_destroy_targets:
                raise RuntimeError("Unexpected destroy target actors in " + package)
            if not any(label.startswith("RF_EnemySpawn_") for label in labels):
                raise RuntimeError("Missing enemy spawn markers in " + package)
            for enemy_id in level.get("enemy_composition", {}):
                if enemy_id not in supported_enemy_ids:
                    raise RuntimeError("Unknown enemy composition id %s in %s" % (
                        enemy_id, package))
            total += 1
            objective_total += objectives

    if total != 56:
        raise RuntimeError("Expected 56 maps, loaded %d" % total)
    unreal.log("[RedFrontValidate] Loaded %d maps and %d objective markers" % (total, objective_total))
    if not unreal.load_class(None, "/Script/RedFront1941.RFVehicleActor"):
        raise RuntimeError("Could not load runtime armored vehicle actor class")
    if not unreal.load_class(None, "/Script/RedFront1941.RFAircraftActor"):
        raise RuntimeError("Could not load runtime aircraft actor class")
    unreal.log("[RedFrontValidate] Runtime vehicle/aircraft classes and enemy contracts resolved")


try:
    validate_characters()
    validate_maps()
    unreal.log("[RedFrontValidate] Campaign content validation passed")
except Exception:
    unreal.log_error(traceback.format_exc())
    raise
