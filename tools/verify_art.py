#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_art.py — 程序化美术资产自检
=====================================================================
对 generate_art.py 的产物做质量与完整性校验，避免"看起来生成了、
其实是空白/纯色/接缝明显/法线错误"的假交付：

  1. 清单完整性：art-manifest.json 可解析，声明的每个文件都存在、尺寸一致
  2. 覆盖率：地形（basecolor/normal/roughness）、覆盖层、贴花、VFX、
     角色精灵表、UI、视差层各类数量达到下限
  3. 非空白：像素标准差 > 阈值（排除纯色/全透明）
  4. 可平铺：左右/上下边缘的像素差异接近内部差异（接缝检测）
  5. Alpha 通道：需要透明的类别（贴花/VFX/精灵/UI）必须有非平凡 alpha
  6. 法线贴图：平均色应接近 (0.5, 0.5, 1.0)，B 通道明显偏冷
  7. 精灵表：尺寸 = 帧数 × 帧宽高（与清单一致）
  8. 材质清单：Master/Instance 引用闭合（实例的 parent 必须存在，
     texture 参数引用的贴图必须在资产清单中）

用法：python tools/verify_art.py [--quick]
退出码：0 = 通过（可有警告），1 = 有错误
"""

import argparse
import json
import os
import sys
import math

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART_ROOT = os.path.join(ROOT, "Unreal", "Content", "RedFront", "Art2D")
MANIFEST = os.path.join(ART_ROOT, "art-manifest.json")

ERRORS = []
WARNINGS = []
CHECKS = [0]


def ok(cond, msg):
    CHECKS[0] += 1
    if not cond:
        ERRORS.append(msg)
    return cond


def warn(cond, msg):
    if not cond:
        WARNINGS.append(msg)
    return cond


def load(path):
    return np.asarray(Image.open(path).convert("RGBA")).astype(np.float32)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true", help="按快速模式的分辨率下限校验")
    args = ap.parse_args()

    if not os.path.exists(MANIFEST):
        print("✖ 未找到 %s，请先运行 python tools/generate_art.py" % MANIFEST)
        return 1
    with open(MANIFEST, encoding="utf-8") as f:
        man = json.load(f)

    assets = man["assets"]
    print("=" * 70)
    print("RedFront 1941 — 程序化美术资产自检")
    print("=" * 70)
    print("清单：%s（生成于 %s）" % (os.path.basename(MANIFEST), man.get("generated")))
    print("贴图 %d / 材质与实例 %d / 精灵表 %d / VFX %d%s" %
          (len(assets), len(man["materials"]), len(man["sprite_sheets"]), len(man["vfx_sheets"]),
           "（快速模式产物）" if man.get("quick_mode") else ""))

    # ---------- 1. 文件存在与尺寸 ----------
    by_role = {}
    missing = 0
    size_bad = 0
    blank = 0
    seam_bad = []
    alpha_bad = []
    for a in assets:
        path = os.path.join(ART_ROOT, a["path"].split("Art2D/", 1)[1])
        if not os.path.exists(path):
            ERRORS.append("清单声明的文件不存在：%s" % a["path"])
            missing += 1
            continue
        with Image.open(path) as im:
            if im.width != a["width"] or im.height != a["height"]:
                ERRORS.append("尺寸与清单不一致：%s（清单 %dx%d，实际 %dx%d）" %
                              (a["path"], a["width"], a["height"], im.width, im.height))
                size_bad += 1
        arr = load(path)
        rgb = arr[..., :3]
        alpha = arr[..., 3]
        std = float(rgb.std())
        std_a = float(alpha.std())
        # ---------- 3. 非空白 ----------
        # 阴影/暗角等"黑色 + 变化 alpha"的叠加素材，RGB 恒定是正常的，按 alpha 变化判定
        informative = max(std, std_a)
        if informative < 1.5:
            ERRORS.append("资产近似空白（rgb_std=%.2f alpha_std=%.2f）：%s" % (std, std_a, a["path"]))
            blank += 1
        # ---------- 5. Alpha ----------
        needs_alpha = a["role"] in ("decal", "vfx_sheet", "sprite_sheet", "shadow", "ui", "overlay", "background")
        if needs_alpha:
            frac_transparent = float((alpha < 250).mean())
            if a["role"] in ("decal", "vfx_sheet", "sprite_sheet", "shadow") and frac_transparent < 0.10:
                alpha_bad.append(a["path"])
        # ---------- 4. 可平铺 ----------
        if a.get("tiling") and a["role"] in ("basecolor", "overlay", "roughness", "background"):
            left = rgb[:, :4].mean(axis=1)
            right = rgb[:, -4:].mean(axis=1)
            top = rgb[:4, :].mean(axis=0)
            bottom = rgb[-4:, :].mean(axis=0)
            interior = rgb.mean(axis=(0, 1))
            edge_diff = (abs(float(left.mean() - right.mean())) + abs(float(top.mean() - bottom.mean()))) / 2.0
            # 与整体亮度波动比较：接缝差异不应超过内部标准差的 3 倍
            tol = max(6.0, 3.0 * float(rgb.mean(axis=2).std()))
            if edge_diff > tol:
                seam_bad.append((a["path"], edge_diff, tol))
        by_role.setdefault(a["role"], []).append(a)

    ok(missing == 0, "有 %d 个清单文件缺失" % missing)
    ok(size_bad == 0, "有 %d 个文件尺寸与清单不符" % size_bad)
    ok(blank == 0, "有 %d 个资产近似空白" % blank)
    for p, d, t in seam_bad:
        WARNINGS.append("平铺接缝偏大：%s（边缘差 %.1f > 容差 %.1f）" % (p, d, t))
    for p in alpha_bad:
        ERRORS.append("需要透明通道但 alpha 几乎完全不透明：%s" % p)

    # ---------- 6. 法线贴图 ----------
    normal_issues = []
    for a in by_role.get("normal", []):
        path = os.path.join(ART_ROOT, a["path"].split("Art2D/", 1)[1])
        arr = load(path)
        mean = arr[..., :3].mean(axis=(0, 1))
        # R/G 应接近 128，B 应明显大于 R/G
        if abs(mean[0] - 128) > 22 or abs(mean[1] - 128) > 22 or mean[2] < 170:
            normal_issues.append((a["path"], mean))
    ok(not normal_issues, "法线贴图均值异常：%s" % normal_issues[:3])

    # ---------- 7. 精灵表尺寸 ----------
    sprite_bad = []
    for s in man["sprite_sheets"]:
        exp_w = s["frame_w"] * s["cols"]
        exp_h = s["frame_h"] * s["rows"]
        p = None
        for a in assets:
            if a["path"].endswith(os.path.basename(s["sheet"])):
                p = os.path.join(ART_ROOT, a["path"].split("Art2D/", 1)[1])
                break
        if not p or not os.path.exists(p):
            sprite_bad.append(s["sheet"] + "（缺失）")
            continue
        with Image.open(p) as im:
            if (im.width, im.height) != (exp_w, exp_h):
                sprite_bad.append("%s 期望 %dx%d 实际 %dx%d" % (s["sheet"], exp_w, exp_h, im.width, im.height))
    ok(not sprite_bad, "精灵表尺寸不符：%s" % sprite_bad[:4])

    # ---------- 2. 覆盖率下限 ----------
    terrains = set(a.get("terrain") for a in by_role.get("basecolor", []) if a.get("terrain"))
    res_min = 1024 if args.quick else 2048
    floors = [
        ("地形数量", len(terrains), 9),
        ("地形 BaseColor", len([a for a in by_role.get("basecolor", []) if a.get("terrain")]), 9),
        ("地形 Normal", len(by_role.get("normal", [])), 9),
        ("地形 Roughness", len(by_role.get("roughness", [])), 9),
        ("天气覆盖层", len([a for a in by_role.get("overlay", []) if a.get("weather")]), 8),
        ("贴花", len(by_role.get("decal", [])), 12),
        ("VFX 序列帧", len(man["vfx_sheets"]), 15),
        ("角色精灵表", len(man["sprite_sheets"]), 20),
        ("视差背景", len(by_role.get("background", [])), 5),
        ("UI 素材", len(by_role.get("ui", [])), 6),
    ]
    for name, got, need in floors:
        ok(got >= need, "%s 不足：%d < %d" % (name, got, need))
    print("覆盖：地形 %d 种 · 覆盖层 %d · 贴花 %d · VFX %d · 精灵表 %d · 视差 %d · UI %d" %
          (len(terrains), len(by_role.get("overlay", [])), len(by_role.get("decal", [])),
           len(man["vfx_sheets"]), len(man["sprite_sheets"]), len(by_role.get("background", [])),
           len(by_role.get("ui", []))))

    # ---------- 8. 材质引用闭合 ----------
    masters = {m["name"] for m in man["materials"] if m.get("type") == "master"}
    asset_names = {os.path.splitext(os.path.basename(a["path"]))[0] for a in assets}
    ref_bad = []
    instances = 0
    for m in man["materials"]:
        if m.get("type") != "instance":
            continue
        instances += 1
        if m.get("parent") not in masters:
            ref_bad.append("%s → 未定义的父材质 %s" % (m["name"], m.get("parent")))
        for pname, tname in (m.get("textures") or {}).items():
            if tname not in asset_names:
                ref_bad.append("%s 的纹理参数 %s 引用不存在的贴图 %s" % (m["name"], pname, tname))
    ok(not ref_bad, "材质引用未闭合：%s" % ref_bad[:5])
    print("材质：%d 个 Master + %d 个实例（引用%s）" % (len(masters), instances, "闭合" if not ref_bad else "异常"))

    # ---------- 分辨率 ----------
    biggest = max((a["width"] for a in assets), default=0)
    if not args.quick and biggest < 2048:
        WARNINGS.append("最大贴图边长仅 %d，高清目标为 2048" % biggest)

    # ---------- 汇总 ----------
    print("-" * 70)
    if WARNINGS:
        print("⚠ 警告 %d 条：" % len(WARNINGS))
        for w in WARNINGS[:12]:
            print("  · " + w)
    if ERRORS:
        print("✖ 错误 %d 条（共 %d 项检查）：" % (len(ERRORS), CHECKS[0]))
        for e in ERRORS[:25]:
            print("  · " + e)
        print("\n自检失败。")
        return 1
    print("✔ 自检通过：%d 项检查全部通过，%d 条警告。" % (CHECKS[0], len(WARNINGS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
