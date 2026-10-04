#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rf_generate_materials.py — 在 UE 5.8 编辑器内自动创建材质与 Paper2D 资产
=====================================================================
消费 `Content/RedFront/Art2D/art-manifest.json`（由 tools/generate_art.py 生成）：

  1. 导入全部贴图，并按用途设置 sRGB / 压缩（BaseColor=sRGB+TC_Default，
     Normal=线性+TC_Normalmap，Roughness=线性+TC_Grayscale）
  2. 创建 7 个 Master Material（地形/Sprite/贴花/加法VFX/烟幕/视差/UI）
  3. 创建全部 MaterialInstance，并按清单写入纹理与标量参数
  4. 依据 sprite_sheets 创建 Paper2D Sprite 与 Flipbook（若 Paper2D Python API 可用）
  5. 输出报告 rf_materials_report.txt

运行方式（二选一）：
  · 编辑器内：Window → Developer Tools → Output Log，切到 Python 模式后执行
        exec(open(r"<项目>/Unreal/Content/Python/rf_generate_materials.py").read())
    或 Tools → Execute Python Script… 选择本文件
  · 命令行：UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<本文件>"

dry-run（不需要引擎，用于校验清单与创建计划）：
    python rf_generate_materials.py --dry-run --manifest <art-manifest.json>

注意：本脚本按 UE 5.8 的 Python API 编写，每步都有 try/except 保护；
若某个 API 在你的版本上名称不同，脚本会记录到报告并继续，不会中断整个导入。
"""

import argparse
import json
import os
import sys
import traceback

try:
    import unreal  # type: ignore
    HAS_UNREAL = True
except Exception:
    HAS_UNREAL = False

LOG = []


def log(msg):
    LOG.append(msg)
    print(msg, flush=True)


# ---------------------------------------------------------------------------
# 资产路径
# ---------------------------------------------------------------------------
ART_DIR = "/Game/RedFront/Art2D"
MAT_DIR = "/Game/RedFront/Materials"
TEX_DIRS = {
    "basecolor": ART_DIR + "/Environment",
    "normal": ART_DIR + "/Environment",
    "roughness": ART_DIR + "/Environment",
    "overlay": None,          # 按清单 path 推断
    "decal": ART_DIR + "/Decals",
    "vfx_sheet": ART_DIR + "/VFX",
    "sprite_sheet": ART_DIR + "/Characters",
    "shadow": ART_DIR + "/Characters",
    "background": ART_DIR + "/Parallax",
    "ui": ART_DIR + "/UI",
    "vfx": ART_DIR + "/VFX",
}


def ue_path_for(asset_entry):
    """把清单里的相对路径（RedFront/Art2D/xxx/yy.png）映射为 /Game/... 资产路径。"""
    rel = asset_entry["path"]
    rel = rel.split("Art2D/", 1)[1] if "Art2D/" in rel else rel
    folder, name = os.path.split(rel)
    return "%s/%s" % (ART_DIR + "/" + folder, os.path.splitext(name)[0])


def source_path_for(root, asset_entry):
    """清单相对路径 → 磁盘绝对路径（Unreal/Content/RedFront/Art2D/...）。"""
    rel = asset_entry["path"]
    rel = rel.split("Art2D/", 1)[1] if "Art2D/" in rel else rel
    return os.path.join(root, "Unreal", "Content", "RedFront", "Art2D", rel.replace("/", os.sep))


def compression_for(role):
    if not HAS_UNREAL:
        return "N/A"
    m = {
        "basecolor": unreal.TextureCompressionSettings.TC_DEFAULT,
        "overlay": unreal.TextureCompressionSettings.TC_DEFAULT,
        "decal": unreal.TextureCompressionSettings.TC_DEFAULT,
        "background": unreal.TextureCompressionSettings.TC_DEFAULT,
        "vfx_sheet": unreal.TextureCompressionSettings.TC_DEFAULT,
        "sprite_sheet": unreal.TextureCompressionSettings.TC_DEFAULT,
        "ui": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        "normal": unreal.TextureCompressionSettings.TC_NORMALMAP,
        "roughness": unreal.TextureCompressionSettings.TC_GRAYSCALE,
        "shadow": unreal.TextureCompressionSettings.TC_GRAYSCALE,
        "vfx": unreal.TextureCompressionSettings.TC_DEFAULT,
    }
    return m.get(role, unreal.TextureCompressionSettings.TC_DEFAULT)


# ---------------------------------------------------------------------------
# 1. 贴图导入
# ---------------------------------------------------------------------------
def import_textures(manifest, root):
    if not HAS_UNREAL:
        return {}
    tasks = []
    mapping = {}
    for a in manifest["assets"]:
        src = source_path_for(root, a)
        dest = os.path.dirname(ue_path_for(a))
        name = os.path.basename(ue_path_for(a))
        t = unreal.AssetImportTask()
        t.filename = src
        t.destination_path = dest
        t.destination_name = name
        t.automated = True
        t.replace_existing = True
        t.save = True
        t.factory = unreal.TextureFactory()
        tasks.append(t)
        mapping[name] = dest + "/" + name
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        log("导入贴图：%d 张" % len(tasks))
    # 设置 sRGB / 压缩
    fixed = 0
    for a in manifest["assets"]:
        path = ue_path_for(a)
        tex = unreal.EditorAssetLibrary.load_asset(path)
        if not tex:
            log("  ⚠ 未能载入贴图：%s" % path)
            continue
        try:
            tex.set_editor_property("srgb", bool(a.get("srgb", True)))
            tex.set_editor_property("compression_settings", compression_for(a["role"]))
            if a.get("role") in ("vfx_sheet", "sprite_sheet", "decal", "shadow", "ui", "overlay"):
                tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
                                        if a.get("role") == "ui" else unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
                tex.set_editor_property("never_stream", False)
            unreal.EditorAssetLibrary.save_loaded_asset(tex)
            fixed += 1
        except Exception as e:
            log("  ⚠ 设置贴图属性失败 %s：%s" % (path, e))
    log("贴图属性设置完成：%d/%d" % (fixed, len(manifest["assets"])))
    return mapping


# ---------------------------------------------------------------------------
# 2. Master Material 构建
# ---------------------------------------------------------------------------
def _tex_param(mat, tex, param_name, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", param_name)
    if tex:
        node.set_editor_property("texture", tex)
    return node


def _scalar(mat, name, value, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", float(value))
    return node


def _vector(mat, name, value, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(value[0], value[1], value[2], 1.0))
    return node


def _mul(mat, a, b, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionMultiply, x, y)
    unreal.MaterialEditingLibrary.connect_material_expressions(a, "", node, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(b, "", node, "B")
    return node


def _lerp(mat, a, b, alpha, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    unreal.MaterialEditingLibrary.connect_material_expressions(a, "", node, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(b, "", node, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(alpha, "", node, "Alpha")
    return node


def _connect(mat, from_node, from_out, prop):
    unreal.MaterialEditingLibrary.connect_material_property(from_node, from_out, prop)


def build_master_materials(manifest, tex_by_name):
    """返回 {material_name: unreal.Material}"""
    if not HAS_UNREAL:
        return {}
    out = {}
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    g = lambda n: tex_by_name.get(n)

    for spec in manifest["materials"]:
        if spec.get("type") != "master":
            continue
        name = spec["name"]
        try:
            asset_path = MAT_DIR + "/" + name
            existing = unreal.EditorAssetLibrary.load_asset(asset_path)
            if existing:
                out[name] = existing
                log("  ✔ 使用已有 Master Material：%s" % name)
                continue
            mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
            if not mat:
                log("  ⚠ 创建材质失败：%s" % name)
                continue
            M = unreal.MaterialProperty
            blend = {"opaque": unreal.BlendMode.BLEND_OPAQUE,
                     "masked": unreal.BlendMode.BLEND_MASKED,
                     "translucent": unreal.BlendMode.BLEND_TRANSLUCENT,
                     "additive": unreal.BlendMode.BLEND_ADDITIVE}[spec.get("blend", "opaque")]
            mat.set_editor_property("blend_mode", blend)
            if spec.get("domain") in ("deferred_decal",):
                try:
                    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
                except Exception:
                    pass
            if spec["name"] in ("M_VFX_Additive", "M_Smoke_Translucent", "M_Parallax_Layer", "M_UI_Panel"):
                mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

            if name == "M_Terrain_2D":
                base = _tex_param(mat, g("T_field_BaseColor"), "BaseColorTex", -800, -300)
                normal = _tex_param(mat, g("T_field_Normal"), "NormalTex", -800, -60)
                rough = _tex_param(mat, g("T_field_Roughness"), "RoughnessTex", -800, 180)
                over = _tex_param(mat, g("T_snow_fresh"), "OverlayTex", -800, 420)
                amt = _scalar(mat, "OverlayAmount", 0.0, -800, 660)
                wet = _scalar(mat, "Wetness", 0.0, -520, 660)
                tint = _vector(mat, "Tint", [1, 1, 1], -800, 840)
                mixed = _lerp(mat, base, over, amt, -380, -260)
                tinted = _mul(mat, mixed, tint, -160, -260)
                _connect(mat, tinted, "RGB", M.MP_BASE_COLOR)
                # 湿润时降低粗糙度
                wet_val = _mul(mat, wet, _scalar(mat, "_WetTarget", 0.05, -520, 900), -320, 900)
                rough_mix = _lerp(mat, rough, wet_val, wet, -160, 200)
                _connect(mat, rough_mix, "R", M.MP_ROUGHNESS)
                _connect(mat, normal, "RGB", M.MP_NORMAL)

            elif name == "M_Sprite_Lit":
                spr = _tex_param(mat, g("SPR_sov_rifleman_idle"), "SpriteTex", -800, -200)
                nm = _tex_param(mat, g("T_field_Normal"), "NormalTex", -800, 60)
                flash = _scalar(mat, "DamageFlash", 0.0, -800, 320)
                snow = _scalar(mat, "SnowDust", 0.0, -800, 480)
                white = _vector(mat, "FlashColor", [1, 1, 1], -800, 640)
                snowc = _vector(mat, "SnowColor", [0.86, 0.88, 0.92], -800, 800)
                body = _lerp(mat, spr, white, flash, -420, -180)
                body2 = _lerp(mat, body, snowc, snow, -220, -180)
                _connect(mat, body2, "RGB", M.MP_BASE_COLOR)
                _connect(mat, nm, "RGB", M.MP_NORMAL)
                _connect(mat, spr, "A", M.MP_OPACITY_MASK)

            elif name == "M_Decal_2D":
                d = _tex_param(mat, g("D_crater_medium"), "DecalTex", -800, -200)
                op = _scalar(mat, "Opacity", 1.0, -800, 120)
                wet = _scalar(mat, "WetShine", 0.0, -800, 300)
                alpha = _mul(mat, d, op, -400, 120)
                _connect(mat, d, "RGB", M.MP_BASE_COLOR)
                # Decal 的透明度使用 Alpha 通道
                _connect(mat, alpha, "", M.MP_OPACITY)
                shine = _lerp(mat, _scalar(mat, "_DryRough", 0.95, -520, 460), _scalar(mat, "_WetRough", 0.08, -520, 600), wet, -280, 520)
                _connect(mat, shine, "", M.MP_ROUGHNESS)

            elif name == "M_VFX_Additive":
                v = _tex_param(mat, g("VFX_Explosion"), "VFXTex", -800, -200)
                boost = _scalar(mat, "EmissiveBoost", 6.0, -800, 120)
                fade = _scalar(mat, "Fade", 1.0, -800, 300)
                emis = _mul(mat, _mul(mat, v, boost, -420, -160), fade, -200, -160)
                _connect(mat, emis, "RGB", M.MP_EMISSIVE_COLOR)
                _connect(mat, fade, "", M.MP_OPACITY)

            elif name == "M_Smoke_Translucent":
                v = _tex_param(mat, g("VFX_Smoke"), "SmokeTex", -800, -200)
                dens = _scalar(mat, "Density", 0.55, -800, 120)
                tint = _vector(mat, "Tint", [0.82, 0.8, 0.76], -800, 300)
                col = _mul(mat, _mul(mat, v, tint, -420, -160), dens, -200, -160)
                _connect(mat, col, "RGB", M.MP_EMISSIVE_COLOR)
                _connect(mat, _mul(mat, v, dens, -420, 120), "A", M.MP_OPACITY)

            elif name == "M_Parallax_Layer":
                v = _tex_param(mat, g("BG_city_ruins"), "LayerTex", -800, -200)
                fog = _scalar(mat, "FogDensity", 0.35, -800, 120)
                fogc = _vector(mat, "FogColor", [0.72, 0.74, 0.76], -800, 300)
                mixed = _lerp(mat, v, fogc, fog, -420, -180)
                _connect(mat, mixed, "RGB", M.MP_EMISSIVE_COLOR)
                _connect(mat, v, "A", M.MP_OPACITY)

            elif name == "M_UI_Panel":
                v = _tex_param(mat, g("UI_PanelFrame"), "PanelTex", -800, -200)
                tint = _vector(mat, "Tint", [0.1, 0.11, 0.08], -800, 120)
                op = _scalar(mat, "Opacity", 1.0, -800, 300)
                col = _mul(mat, v, tint, -420, -160)
                _connect(mat, col, "RGB", M.MP_EMISSIVE_COLOR)
                _connect(mat, _mul(mat, v, op, -420, 120), "A", M.MP_OPACITY)

            unreal.MaterialEditingLibrary.recompile_material(mat)
            unreal.EditorAssetLibrary.save_loaded_asset(mat)
            out[name] = mat
            log("  ✔ 创建 Master Material：%s（%s/%s）" % (name, spec.get("domain"), spec.get("blend")))
        except Exception as e:
            log("  ✖ 创建材质失败 %s：%s" % (name, e))
            log(traceback.format_exc())
    return out


# ---------------------------------------------------------------------------
# 3. Material Instance
# ---------------------------------------------------------------------------
def build_instances(manifest, masters, tex_by_name):
    if not HAS_UNREAL:
        return 0
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    made = 0
    for spec in manifest["materials"]:
        if spec.get("type") != "instance":
            continue
        parent = masters.get(spec.get("parent"))
        if not parent:
            log("  ⚠ 跳过实例 %s：父材质 %s 不存在" % (spec["name"], spec.get("parent")))
            continue
        try:
            instance_path = MAT_DIR + "/Instances/" + spec["name"]
            mi = unreal.EditorAssetLibrary.load_asset(instance_path)
            if mi:
                made += 1
                continue
            mi = tools.create_asset(spec["name"], MAT_DIR + "/Instances", unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
            if not mi:
                raise RuntimeError("MaterialInstance creation returned None")
            unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
            for pname, tname in (spec.get("textures") or {}).items():
                tex = tex_by_name.get(tname)
                if tex:
                    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, pname, tex)
            for pname, val in (spec.get("scalars") or {}).items():
                if isinstance(val, (list, tuple)):
                    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
                        mi, pname, unreal.LinearColor(val[0], val[1], val[2], 1.0))
                else:
                    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, pname, float(val))
            unreal.EditorAssetLibrary.save_loaded_asset(mi)
            made += 1
        except Exception as e:
            log("  ⚠ 创建实例失败 %s：%s" % (spec["name"], e))
    log("创建 Material Instance：%d/%d" % (made, sum(1 for m in manifest["materials"] if m.get("type") == "instance")))
    return made


# ---------------------------------------------------------------------------
# 4. Paper2D Sprite / Flipbook
# ---------------------------------------------------------------------------
def build_paper2d(manifest, tex_by_name):
    if not HAS_UNREAL:
        return 0, 0
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    sprites = 0
    books = 0
    for s in manifest["sprite_sheets"]:
        try:
            sheet_name = os.path.splitext(os.path.basename(s["sheet"]))[0]
            tex = tex_by_name.get(sheet_name)
            if not tex:
                log("  ⚠ 精灵表贴图缺失：%s" % sheet_name)
                continue
            frames_by_direction = [[] for _ in range(s["directions"])]
            for fi in range(s["frames"]):
                for di in range(s["directions"]):
                    sname = "SPR_%s_%s_d%d_f%02d" % (s["unit"], s["anim"], di, fi)
                    sprite_path = ART_DIR + "/Characters/Sprites/" + sname
                    sprite = unreal.EditorAssetLibrary.load_asset(sprite_path)
                    b_existing_sprite = sprite is not None
                    if not sprite:
                        sprite = tools.create_asset(sname, ART_DIR + "/Characters/Sprites", unreal.PaperSprite,
                                                    unreal.PaperSpriteFactory())
                    if not sprite:
                        continue
                    if not b_existing_sprite:
                        sprite.set_editor_property("source_texture", tex)
                        sprite.set_editor_property("source_uv", unreal.Vector2D(di * s["frame_w"], fi * s["frame_h"]))
                        sprite.set_editor_property("source_dimension", unreal.Vector2D(s["frame_w"], s["frame_h"]))
                    unreal.EditorAssetLibrary.save_loaded_asset(sprite)
                    frames_by_direction[di].append(sprite)
                    sprites += 1
            for di, direction_frames in enumerate(frames_by_direction):
                if not direction_frames:
                    continue
                fb_name = "FLB_%s_%s_d%d" % (s["unit"], s["anim"], di)
                fb_path = ART_DIR + "/Characters/Flipbooks/" + fb_name
                flipbook = unreal.EditorAssetLibrary.load_asset(fb_path)
                if not flipbook:
                    flipbook = tools.create_asset(fb_name, ART_DIR + "/Characters/Flipbooks",
                                                  unreal.PaperFlipbook, unreal.PaperFlipbookFactory())
                if flipbook:
                    key_frames = []
                    for sprite in direction_frames:
                        key_frame = unreal.PaperFlipbookKeyFrame()
                        key_frame.set_editor_property("sprite", sprite)
                        key_frame.set_editor_property("frame_run", 1)
                        key_frames.append(key_frame)
                    flipbook.set_editor_property("key_frames", key_frames)
                    flipbook.set_editor_property("frames_per_second", float(s["fps"]))
                    unreal.EditorAssetLibrary.save_loaded_asset(flipbook)
                    books += 1
            legacy_flipbook = ART_DIR + "/Characters/Flipbooks/FLB_%s_%s" % (s["unit"], s["anim"])
            if unreal.EditorAssetLibrary.does_asset_exist(legacy_flipbook):
                unreal.EditorAssetLibrary.delete_asset(legacy_flipbook)
        except Exception as e:
            log("  ⚠ Paper2D 资产创建失败 %s：%s" % (s.get("unit"), e))
    log("创建 Paper2D：Sprite %d 个 / 定向动画 Flipbook %d 个" % (sprites, books))
    return sprites, books


def build_character_blueprints():
    """Create faction player pawns and bind their eight facing slots to four art directions."""
    if not HAS_UNREAL:
        return 0
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    parent_class = unreal.load_class(None, "/Script/RedFront1941.RFCharacter")
    if not parent_class:
        log("  ⚠ 找不到 ARFCharacter，跳过角色 Blueprint")
        return 0

    made = 0
    blueprint_dir = "/Game/RedFront/Blueprints/Characters"
    unreal.EditorAssetLibrary.make_directory(blueprint_dir)
    blueprints = (
        ("SOV", "sov_rifleman", "PlayerCharacter"),
        ("US", "us_rifleman", "PlayerCharacter"),
        ("GER", "ger_rifleman", "EnemyCharacter"),
    )
    for faction, unit, role in blueprints:
        name = "BP_%s_%s" % (faction, role)
        path = blueprint_dir + "/" + name
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
        if not blueprint:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent_class)
            blueprint = tools.create_asset(name, blueprint_dir, unreal.Blueprint, factory)
        if not blueprint:
            log("  ⚠ 创建角色 Blueprint 失败：" + name)
            continue

        directional_books = []
        for direction in range(4):
            book_path = ART_DIR + "/Characters/Flipbooks/FLB_%s_idle_d%d" % (unit, direction)
            book = unreal.EditorAssetLibrary.load_asset(book_path)
            if not book:
                directional_books = []
                break
            directional_books.append(book)
        if not directional_books:
            log("  ⚠ 缺少角色待机动画，未绑定：" + name)
            continue

        facing_books = [
            directional_books[0], directional_books[0],
            directional_books[1], directional_books[1],
            directional_books[2], directional_books[2],
            directional_books[3], directional_books[3],
        ]
        default_object = unreal.get_default_object(blueprint.generated_class())
        default_object.set_editor_property("facing_flipbooks", facing_books)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
        made += 1
        log("  ✔ 角色 Blueprint 已绑定四向待机动画：" + name)
    return made


# ---------------------------------------------------------------------------
# dry-run：只校验清单与创建计划
# ---------------------------------------------------------------------------
def dry_run(manifest, root):
    print("=" * 70)
    print("dry-run：校验清单与创建计划（不接触编辑器）")
    print("=" * 70)
    problems = []
    assets = manifest["assets"]
    names = {os.path.splitext(os.path.basename(a["path"]))[0] for a in assets}
    print("贴图：%d 张，最大边长 %d" % (len(assets), max(a["width"] for a in assets)))
    for a in assets:
        p = source_path_for(root, a)
        if not os.path.exists(p):
            problems.append("文件缺失：%s" % p)
    masters = [m for m in manifest["materials"] if m.get("type") == "master"]
    instances = [m for m in manifest["materials"] if m.get("type") == "instance"]
    print("Master Material：%d 个 → %s" % (len(masters), ", ".join(m["name"] for m in masters)))
    print("Material Instance：%d 个" % len(instances))
    master_names = {m["name"] for m in masters}
    for m in instances:
        if m.get("parent") not in master_names:
            problems.append("实例 %s 的父材质 %s 不存在" % (m["name"], m.get("parent")))
        for pname, tname in (m.get("textures") or {}).items():
            if tname not in names:
                problems.append("实例 %s 的纹理参数 %s 引用不存在的贴图 %s" % (m["name"], pname, tname))
    print("Sprite 计划：%d 张精灵表 → %d 帧精灵，%d 个 Flipbook" %
          (len(manifest["sprite_sheets"]),
           sum(s["frames"] * s["directions"] for s in manifest["sprite_sheets"]),
           sum(s["directions"] for s in manifest["sprite_sheets"])))
    print("VFX 计划：%d 组序列帧" % len(manifest["vfx_sheets"]))
    tex_dir_plan = sorted({os.path.dirname(ue_path_for(a)) for a in assets})
    print("目标资产目录：%s" % ", ".join(tex_dir_plan))
    if problems:
        print("\n✖ 发现 %d 个问题：" % len(problems))
        for p in problems[:20]:
            print("  · " + p)
        return 1
    print("\n✔ dry-run 通过：清单自洽，可以在 UE 编辑器内执行。")
    return 0


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=None)
    ap.add_argument("--project-root", default=None)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    root = args.project_root or os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    manifest_path = args.manifest or os.path.join(root, "Unreal", "Content", "RedFront", "Art2D", "art-manifest.json")
    if not os.path.exists(manifest_path):
        print("✖ 未找到清单 %s\n   请先运行：python tools/generate_art.py" % manifest_path)
        return 1
    with open(manifest_path, encoding="utf-8") as f:
        manifest = json.load(f)

    if args.dry_run or not HAS_UNREAL:
        if not HAS_UNREAL and not args.dry_run:
            print("⚠ 未检测到 unreal 模块（本脚本需在 UE 编辑器内运行）。自动切换为 dry-run。")
        return dry_run(manifest, root)

    log("=" * 70)
    log("RedFront 1941 — UE 编辑器内材质与 Paper2D 资产生成")
    log("清单：%s" % manifest_path)
    log("=" * 70)
    tex_by_name = import_textures(manifest, root)
    # 建立 名称 → unreal.Texture 映射
    tex_objs = {}
    for a in manifest["assets"]:
        nm = os.path.splitext(os.path.basename(a["path"]))[0]
        t = unreal.EditorAssetLibrary.load_asset(ue_path_for(a))
        if t:
            tex_objs[nm] = t
    masters = build_master_materials(manifest, tex_objs)
    n_inst = build_instances(manifest, masters, tex_objs)
    n_spr, n_fb = build_paper2d(manifest, tex_objs)
    n_characters = build_character_blueprints()

    log("-" * 70)
    log("完成：Master Material %d · Instance %d · Sprite %d · Flipbook %d · Character BP %d"
        % (len(masters), n_inst, n_spr, n_fb, n_characters))
    report_path = os.path.join(root, "Unreal", "Content", "Python", "rf_materials_report.txt")
    try:
        os.makedirs(os.path.dirname(report_path), exist_ok=True)
        with open(report_path, "w", encoding="utf-8") as f:
            f.write("\n".join(LOG))
        log("报告已写入 %s" % report_path)
    except Exception as e:
        log("⚠ 报告写入失败：%s" % e)
    return 0


if __name__ == "__main__":
    sys.exit(main())
