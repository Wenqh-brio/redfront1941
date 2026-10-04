#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RedFront 1941 — 程序化美术/材质资产生成器
=====================================================================
不需要任何外部模型或手绘素材：本脚本用 Pillow + numpy 生成全套
材质贴图（BaseColor / Normal / Roughness）、天气覆盖层、贴花（Decal）、
VFX 序列帧、占位角色精灵（8 方向动画）与 UI 素材，并输出
`art-manifest.json` 供 UE 编辑器脚本自动创建 Material / MaterialInstance /
Paper2D Sprite / Flipbook 使用。

产物目录： Unreal/Content/RedFront/Art2D/{Environment,Overlays,Decals,VFX,Characters,UI,Parallax}
清单文件： Unreal/Content/RedFront/Art2D/art-manifest.json

用法：
    python tools/generate_art.py                # 全量生成（默认 2048 地形贴图）
    python tools/generate_art.py --quick        # 半分辨率快速生成（用于迭代/CI）
    python tools/generate_art.py --only vfx,ui  # 只生成某几类
说明：所有随机性都由固定种子驱动，同样参数下结果完全可复现。
"""

import argparse
import json
import math
import os
import sys
import time
from datetime import datetime

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageChops

# ----------------------------------------------------------------------------
# 基础工具
# ----------------------------------------------------------------------------

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART_ROOT = os.path.join(ROOT, "Unreal", "Content", "RedFront", "Art2D")

RES = {
    "terrain": 2048,
    "normal": 1024,
    "overlay": 1024,
    "decal": 512,
    "decal_large": 1024,
    "vfx": 256,
    "character": 128,
    "parallax": (2048, 512),
    "ui_panel": 256,
}

ASSETS = []      # 清单：贴图
MATERIALS = []   # 清单：材质
SHEETS = []      # 清单：角色精灵表
VFX_SHEETS = []  # 清单：特效序列帧
WARNINGS = []


def log(*a):
    print(*a, flush=True)


def rng_for(name, salt=0):
    """由名称派生确定性随机源。"""
    seed = (abs(hash(name)) if salt == 0 else abs(hash((name, salt)))) % (2 ** 32)
    return np.random.default_rng(seed)


def smoothstep(t):
    return t * t * (3.0 - 2.0 * t)


def tileable_noise(size, cells, rng):
    """可无缝平铺的值噪声（晶格索引取模，插值连续）。"""
    g = rng.random((cells, cells)).astype(np.float32)
    y = np.linspace(0, cells, size, endpoint=False, dtype=np.float32)
    x = np.linspace(0, cells, size, endpoint=False, dtype=np.float32)
    y0 = np.floor(y).astype(np.int32) % cells
    x0 = np.floor(x).astype(np.int32) % cells
    y1 = (y0 + 1) % cells
    x1 = (x0 + 1) % cells
    fy = smoothstep((y - np.floor(y)).astype(np.float32))[:, None]
    fx = smoothstep((x - np.floor(x)).astype(np.float32))[None, :]
    v00 = g[np.ix_(y0, x0)]
    v01 = g[np.ix_(y0, x1)]
    v10 = g[np.ix_(y1, x0)]
    v11 = g[np.ix_(y1, x1)]
    top = v00 * (1 - fx) + v01 * fx
    bot = v10 * (1 - fx) + v11 * fx
    return top * (1 - fy) + bot * fy


def fbm(size, cells=6, octaves=7, gain=0.5, lacunarity=2.0, ridged=False, salt=0, name="fbm"):
    rng = rng_for(name, salt)
    total = np.zeros((size, size), np.float32)
    amp = 1.0
    norm = 0.0
    c = float(cells)
    for o in range(octaves):
        n = tileable_noise(size, max(2, int(round(c))), rng)
        if ridged:
            n = 1.0 - np.abs(n * 2.0 - 1.0)
        total += n * amp
        norm += amp
        amp *= gain
        c *= lacunarity
    out = total / max(norm, 1e-6)
    lo, hi = float(out.min()), float(out.max())
    return (out - lo) / max(hi - lo, 1e-6)


def ramp(gray, stops):
    """灰阶 0..1 映射到颜色渐变（stops: [(pos,(r,g,b)), ...]）。"""
    g = np.clip(gray, 0, 1)
    out = np.zeros(g.shape + (3,), np.float32)
    for i in range(len(stops) - 1):
        p0, c0 = stops[i]
        p1, c1 = stops[i + 1]
        m = (g >= p0) & (g <= p1 if i == len(stops) - 2 else g < p1)
        if not np.any(m):
            continue
        t = np.clip((g[m] - p0) / max(1e-6, p1 - p0), 0, 1)[:, None]
        out[m] = np.array(c0, np.float32) * (1 - t) + np.array(c1, np.float32) * t
    return out / 255.0


def normal_from_height(h, strength=3.0):
    """由高度场生成切空间法线贴图（环绕差分，保证可平铺）。"""
    gx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5
    gy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5
    nx = -gx * strength
    ny = gy * strength
    nz = np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / ln, ny / ln, nz / ln], -1)
    return ((n * 0.5 + 0.5) * 255.0).astype(np.uint8)


def pil(arr):
    if arr.dtype != np.uint8:
        arr = np.clip(arr, 0, 255).astype(np.uint8)
    return Image.fromarray(arr)


def np_of(img):
    return np.asarray(img).astype(np.float32)


def wrapped_positions(size, rng, count, margin=0):
    """返回一组位置，同时生成 9 个环绕偏移以便绘图无缝。"""
    base = [(float(rng.random()) * size, float(rng.random()) * size) for _ in range(count)]
    offs = [(-size, 0), (size, 0), (0, -size), (0, size), (-size, -size), (size, size), (-size, size), (size, -size)]
    out = []
    for (x, y) in base:
        out.append((x, y))
        for (dx, dy) in offs:
            nx, ny = x + dx, y + dy
            if -size * 0.25 <= nx <= size * 1.25 and -size * 0.25 <= ny <= size * 1.25:
                out.append((nx, ny))
    return out


def ensure_dirs():
    for sub in ["Environment", "Overlays", "Decals", "VFX", "Characters", "UI", "Parallax"]:
        os.makedirs(os.path.join(ART_ROOT, sub), exist_ok=True)


def save_image(img, sub, filename, role, **meta):
    path = os.path.join(ART_ROOT, sub, filename)
    if img.mode == "RGBA":
        img.save(path, "PNG", optimize=True)
    else:
        img.save(path, "PNG", optimize=True)
    rel = "RedFront/Art2D/%s/%s" % (sub, filename)
    entry = {"path": rel, "file": path, "role": role, "width": img.width, "height": img.height}
    entry.update(meta)
    ASSETS.append(entry)
    return entry


def _category_of(path):
    for folder in ["Environment", "Overlays", "Decals", "VFX", "Characters", "UI", "Parallax"]:
        if "/%s/" % folder in path:
            return {"Environment": "environment", "Overlays": "overlays", "Decals": "decals",
                    "VFX": "vfx", "Characters": "characters", "UI": "ui", "Parallax": "parallax"}[folder]
    return "other"


def write_manifest(quick, only):
    """部分生成（--only）时与已有清单合并，避免把其它类别从清单里抹掉。"""
    regenerated = set(only) if only else {"environment", "overlays", "decals", "vfx", "characters", "parallax", "ui"}
    full_run = len(regenerated) == 7
    out = os.path.join(ART_ROOT, "art-manifest.json")
    prev_assets, prev_sheets, prev_vfx = [], [], []
    if not full_run and os.path.exists(out):
        try:
            with open(out, encoding="utf-8") as f:
                prev = json.load(f)
            keep = lambda p: _category_of(p) not in regenerated
            prev_assets = [a for a in prev.get("assets", []) if keep(a.get("path", ""))]
            prev_sheets = [s for s in prev.get("sprite_sheets", []) if "characters" not in regenerated]
            prev_vfx = [v for v in prev.get("vfx_sheets", []) if "vfx" not in regenerated]
            log("  合并已有清单：保留 %d 张贴图 / %d 精灵表 / %d 组 VFX" %
                (len(prev_assets), len(prev_sheets), len(prev_vfx)))
        except Exception as e:
            WARNINGS.append("已有清单读取失败，改为全新写入：%s" % e)

    seen = set()
    merged_assets = []
    for a in ASSETS + prev_assets:
        if a["path"] in seen:
            continue
        seen.add(a["path"])
        merged_assets.append(a)
    seen_s = set()
    merged_sheets = []
    for s in SHEETS + prev_sheets:
        key = s["unit"] + "/" + s["anim"]
        if key in seen_s:
            continue
        seen_s.add(key)
        merged_sheets.append(s)
    seen_v = set()
    merged_vfx = []
    for v in VFX_SHEETS + prev_vfx:
        if v["name"] in seen_v:
            continue
        seen_v.add(v["name"])
        merged_vfx.append(v)

    manifest = {
        "$schema": "redfront.art-manifest/1",
        "generated": datetime.now().strftime("%Y-%m-%d %H:%M"),
        "generator": "tools/generate_art.py",
        "quick_mode": quick,
        "categories": sorted(regenerated) if not full_run else ["environment", "overlays", "decals", "vfx", "characters", "ui", "parallax"],
        "resolutions": RES,
        "note": "本清单由程序化生成器产出，供 Unreal/Content/Python/rf_generate_materials.py 自动创建材质、材质实例与 Paper2D 精灵。",
        "assets": [{k: v for k, v in a.items() if k != "file"} for a in merged_assets],
        "materials": MATERIALS,
        "sprite_sheets": merged_sheets,
        "vfx_sheets": merged_vfx,
        "warnings": WARNINGS,
    }
    with open(out, "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)
    log("  清单写入 %s（贴图 %d / 精灵表 %d / VFX %d）" %
        (out, len(manifest["assets"]), len(manifest["sprite_sheets"]), len(manifest["vfx_sheets"])))
    return manifest


# ----------------------------------------------------------------------------
# 1. 地形材质（BaseColor / Normal / Roughness）
# ----------------------------------------------------------------------------

TERRAINS = {
    "field": dict(
        stops=[(0.00, (52, 50, 30)), (0.35, (74, 74, 40)), (0.55, (98, 98, 52)), (0.75, (124, 120, 68)), (1.00, (146, 140, 88))],
        cells=5, octaves=7, rough=(0.80, 0.16), detail="grass", bump=2.2,
        desc="旷野：土壤 + 草簇 + 碎石，夏秋过渡色",
    ),
    "city": dict(
        stops=[(0.00, (40, 39, 37)), (0.45, (62, 60, 57)), (0.75, (84, 81, 76)), (1.00, (112, 108, 100))],
        cells=8, octaves=6, rough=(0.68, 0.20), detail="asphalt", bump=1.6,
        desc="城市：破碎沥青 + 砖石粉 + 弹坑尘",
    ),
    "forest": dict(
        stops=[(0.00, (26, 30, 20)), (0.40, (40, 48, 28)), (0.70, (56, 62, 34)), (1.00, (78, 80, 46))],
        cells=6, octaves=7, rough=(0.85, 0.12), detail="litter", bump=3.0,
        desc="森林：腐叶层 + 树根 + 苔藓斑",
    ),
    "village": dict(
        stops=[(0.00, (48, 42, 30)), (0.40, (70, 60, 42)), (0.70, (96, 84, 58)), (1.00, (126, 112, 78))],
        cells=6, octaves=6, rough=(0.78, 0.16), detail="straw", bump=2.4,
        desc="村落：踩实土路 + 麦秸 + 木屑",
    ),
    "river": dict(
        stops=[(0.00, (34, 40, 36)), (0.35, (48, 56, 48)), (0.60, (62, 72, 60)), (0.85, (86, 92, 76)), (1.00, (108, 110, 92))],
        cells=5, octaves=6, rough=(0.55, 0.25), detail="gravel", bump=2.6,
        desc="河岸：湿泥 + 卵石 + 浅滩水痕（低粗糙度）",
    ),
    "rail": dict(
        stops=[(0.00, (36, 35, 33)), (0.45, (54, 52, 48)), (0.75, (74, 71, 64)), (1.00, (98, 94, 84))],
        cells=7, octaves=6, rough=(0.72, 0.18), detail="ballast", bump=2.8,
        desc="铁路：道砟 + 枕木碎屑 + 油污",
    ),
    "industrial": dict(
        stops=[(0.00, (38, 38, 36)), (0.45, (58, 58, 55)), (0.78, (80, 78, 72)), (1.00, (106, 102, 94))],
        cells=8, octaves=6, rough=(0.62, 0.22), detail="concrete", bump=1.8,
        desc="工业区：混凝土板 + 铁锈 + 积尘",
    ),
    "fortress": dict(
        stops=[(0.00, (44, 38, 34)), (0.35, (74, 52, 42)), (0.62, (104, 68, 52)), (0.85, (128, 92, 70)), (1.00, (150, 118, 92))],
        cells=9, octaves=5, rough=(0.74, 0.16), detail="brick", bump=2.0,
        desc="要塞：红砖 + 灰浆 + 弹痕（布列斯特/柯尼斯堡）",
    ),
    "trench": dict(
        stops=[(0.00, (42, 36, 26)), (0.40, (64, 54, 36)), (0.72, (88, 74, 48)), (1.00, (116, 98, 66))],
        cells=6, octaves=7, rough=(0.82, 0.14), detail="mud", bump=3.2,
        desc="战壕：翻浆泥 + 积水 + 木板残骸",
    ),
}

DETAIL_FUNCS = {}


def detail(name):
    def deco(fn):
        DETAIL_FUNCS[name] = fn
        return fn
    return deco


def _shade(img, mask_box, color, alpha):
    d = ImageDraw.Draw(img, "RGBA")
    d.rectangle(mask_box, fill=tuple(color) + (alpha,))


@detail("grass")
def det_grass(img, size, rng):
    """草簇：按分辨率缩放，成簇生长、角度随机，避免看起来像雨丝。"""
    d = ImageDraw.Draw(img, "RGBA")
    clumps = int(size * 0.16)
    for _ in range(clumps):
        cx, cy = rng.random() * size, rng.random() * size
        n = 3 + int(rng.random() * 5)
        for _ in range(n):
            x = cx + (rng.random() - 0.5) * size * 0.012
            y = cy + (rng.random() - 0.5) * size * 0.010
            ln = size * (0.006 + rng.random() * 0.016)
            ang = -math.pi / 2 + (rng.random() - 0.5) * 1.5
            c = (int(84 + rng.random() * 54), int(100 + rng.random() * 58), int(42 + rng.random() * 34))
            d.line([(x, y), (x + math.cos(ang) * ln, y + math.sin(ang) * ln)],
                   fill=c + (int(130 + rng.random() * 90),), width=max(1, int(size / 1400)))
    # 土块与碎石
    for (x, y) in wrapped_positions(size, rng, int(size * 0.05)):
        r = size * (0.0015 + rng.random() * 0.0035)
        g = int(96 + rng.random() * 40)
        d.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=(g, g - 6, int(g * 0.72), 190))
    return img


@detail("asphalt")
def det_asphalt(img, size, rng):
    k = size / 2048.0
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(int(size * 0.05)):  # 裂缝
        x, y = rng.random() * size, rng.random() * size
        pts = [(x, y)]
        ang = rng.random() * math.tau
        for _ in range(int(4 + rng.random() * 8)):
            ang += (rng.random() - 0.5) * 1.1
            x += math.cos(ang) * (8 + rng.random() * 16) * k
            y += math.sin(ang) * (8 + rng.random() * 16) * k
            pts.append((x, y))
        d.line(pts, fill=(24, 23, 22, 170), width=max(1, int((1 + rng.random() * 2) * (1 + k))))
    for (x, y) in wrapped_positions(size, rng, int(size * 0.22)):  # 碎石/砖粉
        r = (1.5 + rng.random() * 4) * k
        g = int(88 + rng.random() * 70)
        c = (g, int(g * 0.94), int(g * 0.86))
        d.ellipse([x - r, y - r * 0.8, x + r, y + r * 0.8], fill=c + (200,))
    return img


@detail("litter")
def det_litter(img, size, rng):
    k = size / 2048.0
    d = ImageDraw.Draw(img, "RGBA")
    for (x, y) in wrapped_positions(size, rng, int(size * 0.4)):
        a = rng.random() * math.tau
        w = (4 + rng.random() * 9) * k
        h = w * 0.5
        pts = [(x + math.cos(a) * w / 2, y + math.sin(a) * w / 2),
               (x + math.cos(a + 2.2) * h, y + math.sin(a + 2.2) * h),
               (x - math.cos(a) * w / 2, y - math.sin(a) * w / 2)]
        c = (int(96 + rng.random() * 60), int(78 + rng.random() * 40), int(40 + rng.random() * 26))
        d.polygon(pts, fill=c + (185,))
    for (x, y) in wrapped_positions(size, rng, int(size * 0.03)):  # 苔藓
        r = (8 + rng.random() * 22) * k
        d.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=(58, 74, 40, 120))
    return img


@detail("straw")
def det_straw(img, size, rng):
    k = size / 2048.0
    d = ImageDraw.Draw(img, "RGBA")
    for (x, y) in wrapped_positions(size, rng, int(size * 0.5)):
        a = rng.random() * math.tau
        ln = (8 + rng.random() * 20) * k
        c = (int(150 + rng.random() * 60), int(130 + rng.random() * 50), int(80 + rng.random() * 40))
        d.line([(x, y), (x + math.cos(a) * ln, y + math.sin(a) * ln)], fill=c + (170,), width=max(1, int(k)))
    return img


@detail("gravel")
def det_gravel(img, size, rng):
    k = size / 2048.0
    d = ImageDraw.Draw(img, "RGBA")
    for (x, y) in wrapped_positions(size, rng, int(size * 0.5)):
        r = (2 + rng.random() * 6) * k
        g = int(96 + rng.random() * 80)
        d.ellipse([x - r, y - r * 0.75, x + r, y + r * 0.75], fill=(g, int(g * 0.98), int(g * 0.9), 210))
    for (x, y) in wrapped_positions(size, rng, int(size * 0.02)):  # 浅水洼（低粗糙度）
        r = (18 + rng.random() * 48) * k
        d.ellipse([x - r, y - r * 0.6, x + r, y + r * 0.6], fill=(70, 92, 96, 90))
    return img


@detail("ballast")
def det_ballast(img, size, rng):
    k = size / 2048.0
    d = ImageDraw.Draw(img, "RGBA")
    for (x, y) in wrapped_positions(size, rng, int(size * 0.9)):
        r = (2 + rng.random() * 5) * k
        g = int(84 + rng.random() * 90)
        d.ellipse([x - r, y - r * 0.8, x + r, y + r * 0.8], fill=(g, g - 4, int(g * 0.9), 215))
    for (x, y) in wrapped_positions(size, rng, int(size * 0.012)):  # 枕木
        w, h = size * 0.22, size * 0.035
        d.rectangle([x - w / 2, y - h / 2, x + w / 2, y + h / 2], fill=(72, 56, 40, 220))
    return img


@detail("concrete")
def det_concrete(img, size, rng):
    d = ImageDraw.Draw(img, "RGBA")
    step = size // 4
    for i in range(1, 4):  # 板缝
        d.line([(i * step, 0), (i * step, size)], fill=(40, 40, 38, 150), width=3)
        d.line([(0, i * step), (size, i * step)], fill=(40, 40, 38, 150), width=3)
    for (x, y) in wrapped_positions(size, rng, int(size * 0.06)):  # 锈斑与油污
        r = 10 + rng.random() * 60
        c = (104, 62, 34) if rng.random() < 0.6 else (34, 32, 30)
        d.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=c + (70,))
    return img


@detail("brick")
def det_brick(img, size, rng):
    d = ImageDraw.Draw(img, "RGBA")
    bw, bh = size / 24.0, size / 48.0
    mortar = (118, 112, 100, 130)
    for row in range(int(size / bh) + 1):
        y = row * bh
        offset = (bw / 2) if row % 2 else 0
        for col in range(-1, int(size / bw) + 1):
            x = col * bw + offset
            tint = 0.82 + rng.random() * 0.36
            base = np.array([132, 74, 54], np.float32) * tint
            if rng.random() < 0.06:  # 缺角/弹痕
                d.rectangle([x + 1, y + 1, x + bw - 1, y + bh - 1], fill=(96, 88, 78, 255))
                continue
            d.rectangle([x + 1.5, y + 1.5, x + bw - 1.5, y + bh - 1.5],
                        fill=(int(base[0]), int(base[1]), int(base[2]), 235))
        d.line([(0, y), (size, y)], fill=mortar, width=1)
    return img


@detail("mud")
def det_mud(img, size, rng):
    k = size / 2048.0
    d = ImageDraw.Draw(img, "RGBA")
    for (x, y) in wrapped_positions(size, rng, int(size * 0.18)):  # 泥块
        r = (6 + rng.random() * 26) * k
        g = int(56 + rng.random() * 46)
        d.ellipse([x - r, y - r * 0.62, x + r, y + r * 0.62], fill=(g, int(g * 0.86), int(g * 0.6), 120))
    for (x, y) in wrapped_positions(size, rng, int(size * 0.03)):  # 积水
        r = (14 + rng.random() * 46) * k
        d.ellipse([x - r, y - r * 0.55, x + r, y + r * 0.55], fill=(58, 60, 52, 120))
    for (x, y) in wrapped_positions(size, rng, int(size * 0.012)):  # 木板
        a = rng.random() * 0.6
        w, h = (26 + rng.random() * 60) * k, (6 + rng.random() * 5) * k
        d.polygon([(x - w / 2, y - h / 2), (x + w / 2, y - h / 2 + math.tan(a) * w / 2),
                   (x + w / 2, y + h / 2 + math.tan(a) * w / 2), (x - w / 2, y + h / 2)],
                  fill=(84, 66, 44, 190))
    return img


def build_terrain(key, cfg, size, normal_size):
    """返回 (base_img, normal_img, roughness_img, height)。"""
    # 高度/细节场
    base = fbm(size, cells=cfg["cells"], octaves=cfg["octaves"], name="terrain_" + key)
    warp = fbm(size, cells=max(2, cfg["cells"] // 2), octaves=4, salt=7, name="warp_" + key)
    height = np.clip(base * 0.72 + warp * 0.28, 0, 1)

    # 颜色
    color = ramp(height, cfg["stops"])
    rng = rng_for("detail_" + key)
    img = pil(np.clip(color * 255, 0, 255).astype(np.uint8)).convert("RGBA")
    fn = DETAIL_FUNCS.get(cfg["detail"])
    if fn:
        fn(img, size, rng)
    # 大尺度污渍与明暗
    stain = fbm(size, cells=3, octaves=4, salt=11, name="stain_" + key)
    shade = ((stain - 0.5) * 0.22 + 1.0)[..., None]
    arr = np.clip(np_of(img.convert("RGB")) * shade, 0, 255).astype(np.uint8)
    base_img = Image.fromarray(arr).convert("RGB")

    # 法线：细节层也参与
    det = np.abs(np_of(img.convert("L")).astype(np.float32) / 255.0 - base)
    h_full = np.clip(height * 0.7 + det * 0.6, 0, 1)
    h_small = np.asarray(Image.fromarray((h_full * 255).astype(np.uint8)).resize((normal_size, normal_size), Image.LANCZOS), np.float32) / 255.0
    normal_img = Image.fromarray(normal_from_height(h_small, strength=cfg["bump"])).convert("RGB")

    # 粗糙度：基数 + 噪声变化（湿润区域更光滑）
    r0, r1 = cfg["rough"]
    rough = np.clip(r0 + (fbm(normal_size, cells=4, octaves=5, salt=23, name="rough_" + key) - 0.5) * r1, 0.05, 0.99)
    if key in ("river", "trench"):
        wet = fbm(normal_size, cells=3, octaves=3, salt=31, name="wet_" + key)
        rough = np.where(wet > 0.62, rough * 0.45, rough)
    rough_img = Image.fromarray((rough * 255).astype(np.uint8)).convert("L")
    return base_img, normal_img, rough_img, height


# ----------------------------------------------------------------------------
# 2. 天气/季节覆盖层（RGBA，可平铺）
# ----------------------------------------------------------------------------

OVERLAYS = [
    ("snow_fresh", "新雪覆盖（几乎不透明，冬季关卡基底）"),
    ("snow_trodden", "踩踏积雪（带足迹与车辙压痕）"),
    ("mud_wet", "湿泥浆（雨后地面）"),
    ("water_film", "薄水膜（低粗糙度、高反射）"),
    ("dust_dry", "干尘土（夏季行军扬尘）"),
    ("ash_scorch", "焦土与灰烬（燃烧后的地表）"),
    ("frost_crystal", "霜冻结晶（清晨低温）"),
    ("rain_sheen", "雨幕水光（持续降雨）"),
]


def build_overlay(key, desc, size):
    rng = rng_for("overlay_" + key)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    if key.startswith("snow"):
        n = fbm(size, cells=4, octaves=6, name="snown_" + key)
        a = np.clip(200 + (n - 0.5) * 55, 0, 255).astype(np.uint8)
        col = ramp(n, [(0.0, (222, 226, 234)), (0.5, (238, 241, 246)), (1.0, (252, 253, 255))])
        arr = np.concatenate([np.clip(col * 255, 0, 255).astype(np.uint8), a[..., None]], axis=2)
        img = Image.fromarray(arr, "RGBA")
        d = ImageDraw.Draw(img, "RGBA")
        if key == "snow_trodden":
            for _ in range(int(size * 0.05)):  # 足迹/车辙
                x, y = rng.random() * size, rng.random() * size
                a2 = rng.random() * math.tau
                w = 14 + rng.random() * 40
                d.line([(x, y), (x + math.cos(a2) * w, y + math.sin(a2) * w)], fill=(168, 172, 180, 150), width=3 + int(rng.random() * 4))
            for (x, y) in wrapped_positions(size, rng, int(size * 0.12)):
                d.ellipse([x - 9, y - 5, x + 9, y + 5], fill=(160, 165, 172, 130))
    elif key == "mud_wet":
        n = fbm(size, cells=5, octaves=6, name="mudn_" + key)
        col = ramp(n, [(0.0, (46, 38, 26)), (0.55, (74, 60, 38)), (1.0, (104, 86, 54))])
        a = np.clip(140 + (n - 0.5) * 90, 0, 235).astype(np.uint8)
        img = Image.fromarray(np.concatenate([np.clip(col * 255, 0, 255).astype(np.uint8), a[..., None]], 2), "RGBA")
        d = ImageDraw.Draw(img, "RGBA")
        for (x, y) in wrapped_positions(size, rng, int(size * 0.2)):
            r = 8 + rng.random() * 30
            d.ellipse([x - r, y - r * 0.6, x + r, y + r * 0.6], fill=(38, 32, 22, 120))
    elif key == "water_film":
        n = fbm(size, cells=6, octaves=5, name="watn_" + key)
        a = np.clip(90 + (n - 0.5) * 70, 0, 200).astype(np.uint8)
        col = ramp(n, [(0.0, (32, 44, 52)), (0.6, (52, 70, 78)), (1.0, (78, 96, 104))])
        img = Image.fromarray(np.concatenate([np.clip(col * 255, 0, 255).astype(np.uint8), a[..., None]], 2), "RGBA")
        d = ImageDraw.Draw(img, "RGBA")
        for (x, y) in wrapped_positions(size, rng, int(size * 0.12)):
            r = 6 + rng.random() * 22
            d.ellipse([x - r, y - r * 0.4, x + r, y + r * 0.4], outline=(180, 200, 210, 90), width=1)
    elif key == "dust_dry":
        n = fbm(size, cells=3, octaves=5, name="dustn_" + key)
        col = ramp(n, [(0.0, (150, 138, 106)), (1.0, (196, 184, 150))])
        a = np.clip(60 + (n - 0.5) * 60, 0, 150).astype(np.uint8)
        img = Image.fromarray(np.concatenate([np.clip(col * 255, 0, 255).astype(np.uint8), a[..., None]], 2), "RGBA")
    elif key == "ash_scorch":
        n = fbm(size, cells=5, octaves=6, name="ashn_" + key)
        col = ramp(n, [(0.0, (18, 16, 15)), (0.5, (44, 40, 36)), (1.0, (86, 80, 74))])
        a = np.clip(120 + (n - 0.5) * 120, 0, 240).astype(np.uint8)
        img = Image.fromarray(np.concatenate([np.clip(col * 255, 0, 255).astype(np.uint8), a[..., None]], 2), "RGBA")
        d = ImageDraw.Draw(img, "RGBA")
        for (x, y) in wrapped_positions(size, rng, int(size * 0.25)):
            r = 2 + rng.random() * 6
            g = int(120 + rng.random() * 80)
            d.ellipse([x - r, y - r, x + r, y + r], fill=(g, g - 6, g - 12, 150))
    elif key == "frost_crystal":
        n = fbm(size, cells=8, octaves=6, ridged=True, name="frsn_" + key)
        a = np.clip((n - 0.45) * 520, 0, 210).astype(np.uint8)
        col = ramp(n, [(0.0, (198, 214, 226)), (1.0, (240, 248, 255))])
        arr = np.concatenate([np.clip(col * 255, 0, 255).astype(np.uint8), a[..., None]], 2)
        img = Image.fromarray(arr, "RGBA")
        d = ImageDraw.Draw(img, "RGBA")
        for (x, y) in wrapped_positions(size, rng, int(size * 0.3)):
            ln = 6 + rng.random() * 18
            a2 = rng.random() * math.tau
            d.line([(x, y), (x + math.cos(a2) * ln, y + math.sin(a2) * ln)], fill=(255, 255, 255, 130), width=1)
    elif key == "rain_sheen":
        n = fbm(size, cells=7, octaves=5, name="rainn_" + key)
        a = np.clip(50 + (n - 0.5) * 50, 0, 130).astype(np.uint8)
        col = ramp(n, [(0.0, (26, 34, 40)), (1.0, (58, 72, 84))])
        img = Image.fromarray(np.concatenate([np.clip(col * 255, 0, 255).astype(np.uint8), a[..., None]], 2), "RGBA")
        d = ImageDraw.Draw(img, "RGBA")
        for _ in range(int(size * 0.06)):
            x, y = rng.random() * size, rng.random() * size
            ln = 20 + rng.random() * 60
            d.line([(x, y), (x + ln * 0.18, y + ln)], fill=(200, 214, 226, 60), width=1)
    return img


# ----------------------------------------------------------------------------
# 3. 贴花（Decal，RGBA）
# ----------------------------------------------------------------------------

def build_decal_crater(size, scale, rng, name):
    """弹坑：不规则轮廓 + 内壁阴影 + 外抛土 + 边缘受光，避免"纯黑椭圆"。"""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = cy = size / 2
    r = size * 0.34 * scale
    d = ImageDraw.Draw(img, "RGBA")

    def rough_poly(rx, ry, wobble, fill, seed_off=0):
        pts = []
        n = 44
        for i in range(n):
            a = i / n * math.tau
            w = 1.0 + math.sin(a * 3 + seed_off) * wobble + math.sin(a * 7.3 + seed_off * 2) * wobble * 0.5
            pts.append((cx + math.cos(a) * rx * w, cy + math.sin(a) * ry * w))
        d.polygon(pts, fill=fill)

    # 外抛土
    for _ in range(int(120 * scale)):
        a = rng.random() * math.tau
        rr = r * (0.85 + rng.random() * 0.75)
        x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.9
        s = size * (0.004 + rng.random() * 0.012) * scale
        g = int(58 + rng.random() * 46)
        d.ellipse([x - s, y - s * 0.8, x + s, y + s * 0.8], fill=(g, int(g * 0.86), int(g * 0.62), 195))
    # 坑体：由外到内逐层加深（模拟内壁受光）
    rough_poly(r * 1.06, r * 0.92, 0.035, (86, 76, 60, 190), 0.0)
    rough_poly(r, r * 0.86, 0.03, (42, 34, 26, 235), 1.1)
    rough_poly(r * 0.72, r * 0.6, 0.025, (26, 21, 17, 250), 2.2)
    rough_poly(r * 0.34, r * 0.28, 0.02, (16, 13, 11, 255), 3.3)
    # 坑底积水/焦土反光
    d.ellipse([cx - r * 0.26, cy - r * 0.19, cx + r * 0.26, cy + r * 0.19], fill=(40, 40, 36, 120))
    # 上缘受光、下缘阴影
    d.arc([cx - r * 1.06, cy - r * 0.94, cx + r * 1.06, cy + r * 0.94], 190, 350, fill=(148, 132, 104, 150),
          width=max(2, int(size * 0.006 * scale)))
    d.arc([cx - r * 1.02, cy - r * 0.9, cx + r * 1.02, cy + r * 0.9], 15, 165, fill=(24, 20, 17, 170),
          width=max(2, int(size * 0.005 * scale)))
    return img.filter(ImageFilter.GaussianBlur(radius=size * 0.0015))


def build_decal_rubble(size, rng):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(180):
        x, y = rng.random() * size, rng.random() * size
        w = 4 + rng.random() * 22
        h = 3 + rng.random() * 14
        a = rng.random() * math.tau
        tone = 0.5 + rng.random() * 0.7
        base = np.array([132, 118, 100], np.float32) * tone
        if rng.random() < 0.3:
            base = np.array([116, 74, 56], np.float32) * tone
        pts = [(x + math.cos(a) * w / 2, y + math.sin(a) * w / 2),
               (x + math.cos(a + 1.6) * h / 2, y + math.sin(a + 1.6) * h / 2),
               (x - math.cos(a) * w / 2, y - math.sin(a) * w / 2),
               (x - math.cos(a + 1.6) * h / 2, y - math.sin(a + 1.6) * h / 2)]
        d.polygon(pts, fill=(int(base[0]), int(base[1]), int(base[2]), int(200 + rng.random() * 55)))
    return img


def build_decal_blood(size, rng, kind):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    cx = cy = size / 2
    if kind == 3:
        r = size * 0.3
        d.ellipse([cx - r, cy - r * 0.5, cx + r, cy + r * 0.5], fill=(88, 18, 14, 200))
        for _ in range(60):
            x = cx + (rng.random() - 0.5) * size * 0.85
            y = cy + (rng.random() - 0.5) * size * 0.55
            s = 1 + rng.random() * 5
            d.ellipse([x - s, y - s, x + s, y + s], fill=(96, 20, 16, 180))
        return img
    drops = 12 if kind == 2 else 6
    base_r = size * (0.14 if kind == 2 else 0.1)
    d.ellipse([cx - base_r, cy - base_r * 0.8, cx + base_r, cy + base_r * 0.8], fill=(92, 20, 15, 215))
    for _ in range(drops):
        a = rng.random() * math.tau
        rr = base_r * (1.1 + rng.random() * 2.6)
        x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.7
        s = 1.5 + rng.random() * (5 if kind == 2 else 3)
        d.ellipse([x - s, y - s, x + s, y + s], fill=(86, 18, 14, 200))
    return img


def build_decal_tracks(size, rng, turn=False):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    if not turn:
        for lane in (0.32, 0.68):
            x = size * lane
            d.rectangle([x - size * 0.055, 0, x + size * 0.055, size], fill=(34, 30, 26, 150))
            for i in range(int(size / 12)):
                y = i * 12
                d.line([(x - size * 0.055, y), (x + size * 0.055, y)], fill=(22, 20, 17, 190), width=3)
    else:
        for lane in (0.3, 0.7):
            d.arc([-size * 0.2, size * (lane - 0.35) - size * 0.4, size * 1.2, size * (lane + 0.45) + size * 0.4],
                  250, 290, fill=(34, 30, 26, 150), width=int(size * 0.11))
    return img


def build_decal_burn(size, rng):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cx = cy = size / 2
    r = size * 0.36
    d = ImageDraw.Draw(img, "RGBA")
    for i in range(9, 0, -1):
        rr = r * i / 9.0
        alpha = int(40 + (9 - i) * 20)
        d.ellipse([cx - rr, cy - rr * 0.8, cx + rr, cy + rr * 0.8], fill=(16 + i * 3, 14 + i * 2, 12 + i * 2, min(alpha, 210)))
    for _ in range(40):
        a = rng.random() * math.tau
        rr = r * (0.9 + rng.random() * 0.6)
        x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.8
        s = 2 + rng.random() * 9
        d.ellipse([x - s, y - s, x + s, y + s], fill=(24, 21, 18, 150))
    return img.filter(ImageFilter.GaussianBlur(1.2))


def build_decal_bootprints(size, rng):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    x, y = size * 0.5, size * 0.08
    side = 1
    while y < size * 0.95:
        d.ellipse([x + side * size * 0.05 - 7, y - 11, x + side * size * 0.05 + 7, y + 11], fill=(30, 26, 22, 130))
        y += 28
        side *= -1
    return img


def build_decal_puddle(size, rng):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    pts = []
    cx = cy = size / 2
    n = 22
    for i in range(n):
        a = i / n * math.tau
        rr = size * (0.2 + rng.random() * 0.16)
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.72))
    d.polygon(pts, fill=(46, 58, 62, 150))
    d.polygon([(px * 0.96 + cx * 0.04, py * 0.96 + cy * 0.04) for px, py in pts], fill=(60, 76, 82, 120))
    d.ellipse([cx - size * 0.07, cy - size * 0.05, cx - size * 0.01, cy - size * 0.02], fill=(180, 200, 210, 90))
    return img


# ----------------------------------------------------------------------------
# 4. VFX 序列帧
# ----------------------------------------------------------------------------

def vfx_muzzle(size, rng, caliber):
    """4 帧枪口焰：喷射形状随口径变化。"""
    frames = []
    scale = {"762x54": 1.0, "762x25": 0.72, "9mm": 0.7, "3006": 1.05, "heavy": 1.5}[caliber]
    for f in range(4):
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        cx, cy = size * 0.18, size / 2
        L = size * (0.55 + 0.12 * f) * scale
        for _ in range(14):
            a = (rng.random() - 0.5) * (0.5 + 0.1 * f)
            ln = L * (0.45 + rng.random() * 0.65)
            pts = [(cx, cy - size * 0.045 * scale),
                   (cx + ln, cy + math.sin(a) * ln * 0.5),
                   (cx, cy + size * 0.045 * scale)]
            if rng.random() < 0.5:
                d.polygon(pts, fill=(255, int(220 - 60 * rng.random()), int(150 - 60 * rng.random()), int(190 - f * 30)))
            else:
                d.polygon(pts, fill=(255, 240, 200, int(220 - f * 40)))
        d.ellipse([cx - size * 0.06, cy - size * 0.06, cx + size * 0.08, cy + size * 0.06], fill=(255, 246, 214, 255))
        frames.append(img.filter(ImageFilter.GaussianBlur(radius=0.8 + f * 0.3)))
    return frames


def vfx_explosion(size, rng, frames=8):
    """爆炸：白热核心 → 黄 → 橙 → 暗红边缘，后期带灰烟与碎屑。"""
    out = []
    for f in range(frames):
        t = f / (frames - 1.0)
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        cx = cy = size / 2
        r = size * (0.09 + 0.44 * math.sqrt(t))
        alpha = (1 - t) ** 0.65
        rings = 16
        for i in range(rings, 0, -1):
            k = i / float(rings)          # 1 = 最外 → 0 = 最内
            rr = r * k
            temp = (1.0 - k) * (1.0 - t * 0.55)   # 温度随扩散与时间下降
            if temp > 0.72:
                col = (255, 252, 232)
            elif temp > 0.48:
                col = (255, int(196 + 40 * (temp - 0.48) / 0.24), int(70 + 90 * (temp - 0.48) / 0.24))
            elif temp > 0.22:
                col = (int(255 - 40 * (0.48 - temp) / 0.26), int(120 * temp / 0.48 + 40), 34)
            else:
                col = (int(70 + 60 * temp / 0.22), int(52 + 40 * temp / 0.22), int(44 + 30 * temp / 0.22))
            base_a = 72 if k > 0.8 else (150 if k > 0.55 else 240)
            a = int(base_a * alpha)
            if a <= 0:
                continue
            d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=col + (min(255, a),))
        # 白热核心
        cr = r * max(0.04, 1 - t * 1.25)
        if cr > 1:
            d.ellipse([cx - cr, cy - cr, cx + cr, cy + cr], fill=(255, 250, 226, int(255 * alpha)))
        # 碎屑与冲击尘
        for _ in range(int(46 * t) + 6):
            a = rng.random() * math.tau
            rr = r * (0.8 + rng.random() * 1.0)
            x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.85
            s = size * (0.008 + rng.random() * 0.022) * (0.5 + t)
            g = int(52 + rng.random() * 40)
            d.ellipse([x - s, y - s, x + s, y + s], fill=(g, int(g * 0.94), int(g * 0.9), int(150 * (1 - t) + 40)))
        out.append(img.filter(ImageFilter.GaussianBlur(radius=0.5 + t * 2.2)))
    return out


def vfx_smoke(size, rng, frames=12):
    out = []
    for f in range(frames):
        t = f / (frames - 1.0)
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        cx = cy = size / 2
        grow = 0.25 + 0.45 * t
        alpha = int(190 * math.sin(math.pi * min(1.0, 0.15 + t * 0.95)))
        for _ in range(60):
            a = rng.random() * math.tau
            rr = size * grow * (0.35 + rng.random() * 0.65)
            x = cx + math.cos(a) * rr + (rng.random() - 0.5) * size * 0.06
            y = cy + math.sin(a) * rr * 0.8 - t * size * 0.12
            s = size * (0.08 + rng.random() * 0.16) * (0.6 + t * 0.8)
            g = int(150 + rng.random() * 70)
            d.ellipse([x - s, y - s, x + s, y + s], fill=(g, int(g * 0.98), int(g * 0.94), int(alpha * 0.35)))
        out.append(img.filter(ImageFilter.GaussianBlur(radius=2 + t * 6)))
    return out


def vfx_fire(size, rng, frames=8):
    out = []
    for f in range(frames):
        t = f / frames
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        for _ in range(70):
            x = size * (0.5 + (rng.random() - 0.5) * 0.55)
            y = size * (0.95 - rng.random() * 0.7)
            s = size * (0.05 + rng.random() * 0.12) * (1 - (0.95 - y / size) * 0.6)
            k = rng.random()
            col = (255, int(120 + 130 * k), int(20 + 60 * k)) if k > 0.35 else (255, 232, 170)
            d.ellipse([x - s, y - s, x + s, y + s], fill=col + (int(150 + rng.random() * 90),))
        out.append(img.filter(ImageFilter.GaussianBlur(radius=1.6)))
    return out


def vfx_impact(size, rng, kind):
    palettes = {
        "dirt": [(92, 78, 54), (132, 116, 84), (168, 152, 118)],
        "metal": [(180, 186, 196), (240, 232, 200), (255, 248, 214)],
        "wood": [(96, 70, 44), (140, 108, 66), (180, 148, 96)],
        "brick": [(150, 96, 74), (190, 140, 110), (220, 190, 160)],
        "water": [(120, 150, 160), (180, 210, 220), (230, 244, 248)],
        "flesh": [(96, 22, 18), (140, 40, 30), (180, 70, 50)],
    }
    cols = palettes.get(kind, palettes["dirt"])
    out = []
    for f in range(6):
        t = f / 5.0
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        cx, cy = size * 0.5, size * 0.55
        for _ in range(int(26 * (1 - t) + 6)):
            a = rng.random() * math.tau
            rr = size * (0.05 + rng.random() * 0.42) * (0.4 + t)
            x = cx + math.cos(a) * rr
            y = cy + math.sin(a) * rr - t * size * 0.18
            s = size * (0.01 + rng.random() * 0.035) * (1 - t * 0.6)
            c = cols[int(rng.random() * len(cols))]
            d.ellipse([x - s, y - s, x + s, y + s], fill=c + (int(220 * (1 - t) + 30),))
        if kind == "metal" and f < 3:
            d.ellipse([cx - size * 0.07, cy - size * 0.07, cx + size * 0.07, cy + size * 0.07], fill=(255, 250, 220, int(230 * (1 - t))))
        out.append(img.filter(ImageFilter.GaussianBlur(radius=0.4 + t)))
    return out


def vfx_tracer(size, rng):
    out = []
    for f in range(3):
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        w = size * (0.05 - f * 0.01)
        d.line([(0, size / 2), (size, size / 2)], fill=(255, 232, 170, 220 - f * 60), width=max(1, int(w)))
        d.line([(size * 0.55, size / 2), (size, size / 2)], fill=(255, 250, 220, 255), width=max(2, int(w * 1.6)))
        out.append(img)
    return out


def sheet_from_frames(frames, cols=None):
    cols = cols or len(frames)
    rows = int(math.ceil(len(frames) / cols))
    w, h = frames[0].size
    sheet = Image.new("RGBA", (w * cols, h * rows), (0, 0, 0, 0))
    for i, fr in enumerate(frames):
        sheet.paste(fr, ((i % cols) * w, (i // cols) * h))
    return sheet


# ----------------------------------------------------------------------------
# 5. 占位角色精灵（8 方向 → 4 方向 + 镜像；含动画）
# ----------------------------------------------------------------------------

UNITS = {
    "sov_rifleman": dict(
        uniform=(92, 88, 60), uniform_dark=(62, 58, 40), helmet=(96, 92, 62), skin=(196, 160, 124),
        weapon=(46, 36, 28), weapon_len=0.95, scale=1.0, desc="苏军步兵（莫辛-纳甘）",
    ),
    "sov_smg": dict(
        uniform=(104, 96, 64), uniform_dark=(70, 64, 42), helmet=(102, 96, 66), skin=(196, 160, 124),
        weapon=(38, 34, 30), weapon_len=0.62, scale=0.99, desc="苏军突击兵（PPSh-41）",
    ),
    "us_rifleman": dict(
        uniform=(92, 108, 76), uniform_dark=(62, 76, 52), helmet=(78, 92, 66), skin=(198, 164, 130),
        weapon=(42, 38, 32), weapon_len=0.98, scale=1.0, desc="美军步兵（M1 Garand）",
    ),
    "ger_rifleman": dict(
        uniform=(84, 88, 78), uniform_dark=(56, 60, 52), helmet=(72, 74, 68), skin=(198, 164, 130),
        weapon=(40, 34, 28), weapon_len=0.95, scale=1.0, desc="德军步兵（Kar98k）",
    ),
    "ger_mg42_team": dict(
        uniform=(80, 84, 74), uniform_dark=(52, 56, 48), helmet=(70, 72, 66), skin=(198, 164, 130),
        weapon=(36, 34, 30), weapon_len=1.1, scale=1.06, desc="德军 MG42 机枪组",
    ),
}

ANIMS = {
    "idle": (4, 6),
    "walk": (8, 14),
    "fire": (4, 16),
    "reload": (6, 10),
    "death": (6, 10),
}


def rot_pts(cx, cy, w, h, ang):
    ca, sa = math.cos(ang), math.sin(ang)
    hw, hh = w / 2.0, h / 2.0
    out = []
    for (dx, dy) in [(-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)]:
        out.append((cx + dx * ca - dy * sa, cy + dx * sa + dy * ca))
    return out


def rot_trap(cx, cy, length, w_front, w_back, ang):
    """沿朝向的梯形（肩宽腰窄），让躯干有体块感，而不是一个方块。"""
    ca, sa = math.cos(ang), math.sin(ang)
    pts = [(-w_back / 2, -length / 2), (w_back / 2, -length / 2),
           (w_front / 2, length / 2), (-w_front / 2, length / 2)]
    return [(cx + px * ca - py * sa, cy + px * sa + py * ca) for (px, py) in pts]


def draw_soldier(size, unit, cfg, direction, anim, frame):
    """俯视士兵：头盔 + 躯干 + 双臂 + 武器 + 双腿 + 描边。
    朝向由 direction 决定（右/下/左/上），左向在引擎里用镜像复用。"""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    ang = {0: 0.0, 1: math.pi / 2, 2: math.pi, 3: -math.pi / 2}[direction]
    cx, cy = size / 2, size / 2
    s = size / 74.0 * cfg["scale"]           # 让士兵填满约 70% 画幅，便于 2D 可读性
    ink = (28, 26, 22, 255)                   # 统一描边色，2D 精灵的关键可读性手段
    ow = max(1, int(size / 64))
    nframes = ANIMS[anim][0]
    phase = frame / max(1, nframes)
    bob = math.sin(phase * math.tau) * 1.1 * s
    recoil = 0.0
    if anim == "fire":
        recoil = [0, -3.4, -1.6, -0.6][frame % 4] * s
    if anim == "death":
        k = frame / max(1, nframes - 1)
        ang += k * math.pi * 0.45
        cy += k * 7 * s
    if anim == "idle":
        bob = math.sin(phase * math.tau) * 0.7 * s

    def shade(col, k):
        return (max(0, min(255, int(col[0] * k))), max(0, min(255, int(col[1] * k))), max(0, min(255, int(col[2] * k))))

    def poly(pts, fill, outline=True):
        d.polygon(pts, fill=fill, outline=ink if outline else None, width=ow)

    # 落地阴影
    d.ellipse([cx - 19 * s, cy - 15 * s, cx + 19 * s, cy + 17 * s], fill=(0, 0, 0, 70))

    # 双腿（沿朝向迈步，侧面可见）
    stride = math.sin(phase * math.tau) * 8.5 * s if anim == "walk" else (math.sin(phase * math.tau) * 1.4 * s if anim in ("idle", "fire", "reload") else 2.0 * s)
    for side in (-1, 1):
        off = side * 8.5 * s
        px = cx + math.cos(ang + math.pi / 2) * off + math.cos(ang) * stride * side
        py = cy + bob + math.sin(ang + math.pi / 2) * off + math.sin(ang) * stride * side
        poly(rot_pts(px, py, 11 * s, 19 * s, ang), shade(cfg["uniform_dark"], 0.82 if side < 0 else 0.95) + (255,))
        d.ellipse([px - 4.2 * s, py - 4.2 * s, px + 4.2 * s, py + 4.2 * s], fill=(38, 32, 26, 255))

    # 双臂（先画后侧臂再画躯干，形成前后层次）
    wl = 34 * s * cfg["weapon_len"]
    wx = cx + math.cos(ang) * (11 * s + recoil)
    wy = cy + bob + math.sin(ang) * (11 * s + recoil)
    for side in (-1, 1):
        px = cx + math.cos(ang + math.pi / 2) * side * 9.5 * s + math.cos(ang) * 4 * s
        py = cy + bob + math.sin(ang + math.pi / 2) * side * 9.5 * s + math.sin(ang) * 4 * s
        if side < 0:
            poly(rot_pts(px, py, 8.5 * s, 19 * s, ang + side * 0.55), shade(cfg["uniform"], 0.86) + (255,))

    # 躯干（梯形：肩宽腰窄）
    poly(rot_trap(cx, cy + bob, 31 * s, 30 * s, 33 * s, ang), cfg["uniform"] + (255,))
    poly(rot_trap(cx, cy + bob - 2 * s, 14 * s, 20 * s, 24 * s, ang), shade(cfg["uniform"], 1.2) + (150,))
    # 背带/装具
    d.line([(cx + math.cos(ang + math.pi / 2) * 10 * s, cy + bob + math.sin(ang + math.pi / 2) * 10 * s),
            (cx - math.cos(ang + math.pi / 2) * 10 * s, cy + bob - math.sin(ang + math.pi / 2) * 10 * s)],
           fill=shade(cfg["uniform_dark"], 0.7) + (255,), width=max(1, int(3 * s)))

    for side in (1,):
        px = cx + math.cos(ang + math.pi / 2) * side * 9.5 * s + math.cos(ang) * 4 * s
        py = cy + bob + math.sin(ang + math.pi / 2) * side * 9.5 * s + math.sin(ang) * 4 * s
        poly(rot_pts(px, py, 8.5 * s, 19 * s, ang + side * 0.55), shade(cfg["uniform"], 1.0) + (255,))

    # 武器
    poly(rot_pts(wx + math.cos(ang) * wl / 2, wy + math.sin(ang) * wl / 2, wl, 5.0 * s, ang), cfg["weapon"] + (255,))
    d.ellipse([wx - 3.6 * s, wy - 3.6 * s, wx + 3.6 * s, wy + 3.6 * s], fill=ink)
    d.ellipse([wx - 2.6 * s, wy - 2.6 * s, wx + 2.6 * s, wy + 2.6 * s], fill=cfg["skin"] + (255,))

    # 头盔
    hx = cx + math.cos(ang) * 2.2 * s
    hy = cy + bob + math.sin(ang) * 2.2 * s
    d.ellipse([hx - 12 * s, hy - 12 * s, hx + 12 * s, hy + 12 * s], fill=ink)
    d.ellipse([hx - 10.4 * s, hy - 10.4 * s, hx + 10.4 * s, hy + 10.4 * s], fill=cfg["helmet"] + (255,))
    d.ellipse([hx - 7.4 * s, hy - 7.4 * s, hx + 7.4 * s, hy + 7.4 * s], fill=shade(cfg["helmet"], 1.14) + (255,))
    d.ellipse([hx - 3.4 * s, hy - 3.4 * s, hx + 3.4 * s, hy + 3.4 * s], fill=shade(cfg["helmet"], 0.8) + (255,))
    # 前侧轮廓光（受光方向固定为左上）
    d.arc([hx - 11.6 * s, hy - 11.6 * s, hx + 11.6 * s, hy + 11.6 * s], 150, 260,
          fill=(255, 250, 232, 90), width=max(1, int(1.6 * s)))
    # 阵营识别点
    acc = (176, 62, 48) if unit.startswith("sov") else (58, 62, 58)
    ax = cx + math.cos(ang + math.pi / 2) * 11 * s
    ay = cy + bob + math.sin(ang + math.pi / 2) * 11 * s
    d.ellipse([ax - 3.4 * s, ay - 3.4 * s, ax + 3.4 * s, ay + 3.4 * s], fill=ink)
    d.ellipse([ax - 2.6 * s, ay - 2.6 * s, ax + 2.6 * s, ay + 2.6 * s], fill=acc + (235,))
    return img


# ----------------------------------------------------------------------------
# 6. 视差背景层 / UI
# ----------------------------------------------------------------------------

PARALLAX = [
    ("city_ruins", "城市废墟天际线（柏林/斯大林格勒）", [(46, 44, 42), (62, 58, 54)]),
    ("forest_line", "森林树线（白俄罗斯/许特根）", [(30, 40, 30), (46, 58, 40)]),
    ("factory", "工厂烟囱群（工业区）", [(52, 50, 48), (68, 64, 58)]),
    ("snow_plain", "雪原与孤树（莫斯科反攻）", [(196, 204, 214), (222, 228, 236)]),
    ("steppe", "旷野丘陵（库尔斯克）", [(120, 116, 78), (150, 144, 100)]),
]


def build_parallax(key, desc, size, colors):
    w, h = size
    rng = rng_for("parallax_" + key)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    dark, light = colors
    horizon = int(h * 0.55)
    if key == "city_ruins":
        for layer, (yy, col, alpha) in enumerate([(horizon + 20, dark, 220), (horizon, light, 190)]):
            x = -40
            while x < w + 40:
                bw = 40 + rng.random() * 110
                bh = 40 + rng.random() * 150 * (1.0 - layer * 0.25)
                top = yy - bh
                d.rectangle([x, top, x + bw, h], fill=col + (alpha,))
                # 破口与窗
                for _ in range(int(2 + rng.random() * 5)):
                    wx = x + 4 + rng.random() * max(4, bw - 14)
                    wy = top + 6 + rng.random() * max(4, bh - 18)
                    d.rectangle([wx, wy, wx + 7, wy + 10], fill=(20, 20, 22, 200))
                if rng.random() < 0.5:
                    d.polygon([(x, top), (x + bw * 0.5, top - 12 - rng.random() * 26), (x + bw, top)],
                              fill=(col[0] - 8, col[1] - 8, col[2] - 6, alpha))
                x += bw
    elif key in ("forest_line", "snow_plain"):
        for _ in range(140):
            x = rng.random() * w
            th = h * (0.2 + rng.random() * 0.5)
            base = horizon + h * 0.3
            tri = [(x, base - th), (x - th * 0.28, base), (x + th * 0.28, base)]
            col = dark if rng.random() < 0.6 else light
            d.polygon(tri, fill=col + (200,))
        d.rectangle([0, horizon + h * 0.28, w, h], fill=light + (230,))
    elif key == "factory":
        for _ in range(9):
            x = rng.random() * w
            bw = 24 + rng.random() * 40
            bh = h * (0.3 + rng.random() * 0.45)
            d.rectangle([x, horizon - bh + h * 0.2, x + bw, h], fill=dark + (225,))
            d.rectangle([x + bw * 0.3, horizon - bh + h * 0.2 - 16, x + bw * 0.7, horizon - bh + h * 0.2], fill=(30, 28, 26, 220))
        for _ in range(8):
            x = rng.random() * w
            d.ellipse([x, horizon - h * 0.5, x + 90 + rng.random() * 70, horizon - h * 0.5 + 40], fill=(120, 120, 118, 60))
    else:  # steppe
        for i in range(3):
            pts = [(0, h)]
            x = 0
            base = horizon + i * 30
            while x <= w:
                pts.append((x, base - math.sin(x / (160 + i * 90) + i) * (18 + i * 8)))
                x += 60
            pts.append((w, h))
            d.polygon(pts, fill=(dark if i < 2 else light) + (220,))
    # 顶部渐变天空
    sky = np.zeros((h, w, 4), np.uint8)
    grad = np.linspace(0, 1, h, dtype=np.float32)[:, None]
    rgb = ramp(grad, [(0.0, (150, 168, 186)), (1.0, (196, 200, 198))]) if key != "snow_plain" else ramp(grad, [(0.0, (168, 186, 204)), (1.0, (232, 238, 244))])
    sky[..., :3] = np.clip(rgb * 255, 0, 255).astype(np.uint8)
    sky[..., 3] = np.clip(200 - grad[:, 0] * 200, 0, 200).astype(np.uint8)[:, None]
    base_img = Image.alpha_composite(Image.fromarray(sky, "RGBA"), img)
    return base_img


def build_ui_panel(size):
    img = Image.new("RGBA", (size, size), (24, 26, 20, 214))
    d = ImageDraw.Draw(img, "RGBA")
    m = size * 0.09
    d.rectangle([0, 0, size - 1, size - 1], outline=(58, 61, 46, 255), width=3)
    d.rectangle([m, m, size - m, size - m], outline=(40, 43, 33, 190), width=2)
    d.line([(m, m + 2), (size - m, m + 2)], fill=(120, 116, 92, 90), width=2)
    for (cx, cy) in [(m * 0.55, m * 0.55), (size - m * 0.55, m * 0.55), (m * 0.55, size - m * 0.55), (size - m * 0.55, size - m * 0.55)]:
        r = size * 0.012
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(168, 152, 96, 230))
        d.ellipse([cx - r * 0.45, cy - r * 0.45, cx + r * 0.45, cy + r * 0.45], fill=(96, 88, 60, 255))
    return img


def build_vignette(w, h):
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    cx, cy = w / 2.0, h / 2.0
    r = np.sqrt(((x - cx) / cx) ** 2 + ((y - cy) / cy) ** 2)
    a = np.clip((r - 0.55) / 0.65, 0, 1) ** 1.6
    arr = np.zeros((h, w, 4), np.uint8)
    arr[..., 3] = (a * 235).astype(np.uint8)
    return Image.fromarray(arr, "RGBA")


def build_grain(size):
    """颗粒叠加：RGB 本身即噪声（可直接 multiply/overlay 使用），alpha 全不透明。"""
    n = fbm(size, cells=max(4, size // 2), octaves=3, name="grain")
    base = 150.0 + (n - 0.5) * 150.0
    rgb = np.stack([base, base, base], -1)
    alpha = np.full((size, size, 1), 255, np.float32)
    arr = np.clip(np.concatenate([rgb, alpha], -1), 0, 255).astype(np.uint8)
    return Image.fromarray(arr, "RGBA")


def build_crosshair(size):
    out = []
    for f in range(3):
        img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        c = size / 2
        gap = size * (0.11 + f * 0.07)
        ln = size * 0.16
        col = (232, 226, 208, 230) if f < 2 else (214, 96, 74, 235)
        for (dx, dy) in [(1, 0), (-1, 0), (0, 1), (0, -1)]:
            d.line([(c + dx * gap, c + dy * gap), (c + dx * (gap + ln), c + dy * (gap + ln))], fill=col, width=max(1, int(size / 42)))
        d.ellipse([c - 1.5, c - 1.5, c + 1.5, c + 1.5], fill=col)
        out.append(img)
    return out


def build_unit_shadow(size):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    for i in range(10, 0, -1):
        k = i / 10.0
        d.ellipse([size * (0.5 - 0.3 * k), size * (0.5 - 0.22 * k), size * (0.5 + 0.3 * k), size * (0.5 + 0.22 * k)],
                  fill=(0, 0, 0, int(16 * (1 - k) + 6)))
    return img.filter(ImageFilter.GaussianBlur(2))


# ----------------------------------------------------------------------------
# 主流程
# ----------------------------------------------------------------------------

def gen_environment(res):
    size = res["terrain"]
    nsize = res["normal"]
    log("[环境] 地形 PBR 贴图 %d×%d（法线/粗糙度 %d×%d）" % (size, size, nsize, nsize))
    for key, cfg in TERRAINS.items():
        base, normal, rough, _ = build_terrain(key, cfg, size, nsize)
        save_image(base, "Environment", "T_%s_BaseColor.png" % key, "basecolor",
                   srgb=True, compression="TC_Default", tiling=True, terrain=key, desc=cfg["desc"])
        save_image(normal, "Environment", "T_%s_Normal.png" % key, "normal",
                   srgb=False, compression="TC_Normalmap", tiling=True, terrain=key)
        save_image(rough, "Environment", "T_%s_Roughness.png" % key, "roughness",
                   srgb=False, compression="TC_Grayscale", tiling=True, terrain=key)
        log("    · %-10s %s" % (key, cfg["desc"]))
    # 全局细节与破损叠加
    for name, desc, stops in [
        ("T_Crack_Damage", "混凝土/沥青裂缝（叠加混合）", [(0.0, (10, 9, 8)), (1.0, (86, 80, 72))]),
        ("T_Scorch_Overlay", "烟熏/烧焦叠加", [(0.0, (6, 5, 5)), (1.0, (52, 46, 42))]),
        ("T_Moss_Overlay", "苔藓/植被侵蚀叠加", [(0.0, (18, 24, 14)), (1.0, (78, 92, 50))]),
    ]:
        n = fbm(nsize, cells=6, octaves=6, ridged=("Crack" in name), name=name)
        col = ramp(n, stops)
        img = pil(np.clip(col * 255, 0, 255).astype(np.uint8)).convert("RGB")
        save_image(img, "Environment", name + ".png", "overlay", srgb=True, compression="TC_Default", tiling=True, desc=desc)
    log("    · 叠加贴图 3 张（裂缝/烟熏/苔藓）")


def gen_overlays(res):
    size = res["overlay"]
    log("[覆盖层] 天气与季节覆盖 %d×%d" % (size, size))
    for key, desc in OVERLAYS:
        img = build_overlay(key, desc, size)
        save_image(img, "Overlays", "T_%s.png" % key, "overlay",
                   srgb=True, compression="TC_Default", tiling=True, desc=desc, weather=key)
        log("    · %-16s %s" % (key, desc))


def gen_decals(res):
    s = res["decal"]
    sl = res["decal_large"]
    log("[贴花] 弹坑/废墟/血迹/车辙/燃烧/水洼")
    specs = [
        ("crater_small", lambda: build_decal_crater(s, 0.6, rng_for("cr_s"), "s")),
        ("crater_medium", lambda: build_decal_crater(sl, 0.8, rng_for("cr_m"), "m")),
        ("crater_large", lambda: build_decal_crater(sl, 1.0, rng_for("cr_l"), "l")),
        ("rubble", lambda: build_decal_rubble(sl, rng_for("rub"))),
        ("blood_1", lambda: build_decal_blood(s, rng_for("bl1"), 1)),
        ("blood_2", lambda: build_decal_blood(s, rng_for("bl2"), 2)),
        ("blood_3", lambda: build_decal_blood(s, rng_for("bl3"), 3)),
        ("tracks_straight", lambda: build_decal_tracks(sl, rng_for("tr1"), False)),
        ("tracks_turn", lambda: build_decal_tracks(sl, rng_for("tr2"), True)),
        ("burn_scorch", lambda: build_decal_burn(sl, rng_for("burn"))),
        ("bootprints", lambda: build_decal_bootprints(s, rng_for("boot"))),
        ("puddle", lambda: build_decal_puddle(sl, rng_for("pud"))),
    ]
    for name, fn in specs:
        img = fn()
        save_image(img, "Decals", "D_%s.png" % name, "decal", srgb=True, compression="TC_Default",
                   tiling=False, usage="deferred_decal")
        log("    · D_%-16s %d×%d" % (name, img.width, img.height))


def gen_vfx(res):
    size = res["vfx"]
    log("[VFX] 序列帧 %d×%d/帧" % (size, size))
    # 枪口焰：按口径
    for cal, tag in [("762x54", "rifle"), ("762x25", "smg"), ("9mm", "smg9"), ("3006", "us_rifle"), ("heavy", "mg")]:
        frames = vfx_muzzle(size, rng_for("mz_" + cal), cal)
        sheet = sheet_from_frames(frames, cols=4)
        save_image(sheet, "VFX", "VFX_Muzzle_%s.png" % tag, "vfx_sheet", srgb=True,
                   compression="TC_Default", frames=4, frame_w=size, frame_h=size, cols=4, rows=1,
                   fps=32, blend="additive", animated=True)
        VFX_SHEETS.append({"name": "Muzzle_" + tag, "sheet": "RedFront/Art2D/VFX/VFX_Muzzle_%s.png" % tag,
                           "frames": 4, "frame_w": size, "frame_h": size, "cols": 4, "rows": 1,
                           "fps": 32, "blend": "additive", "loop": False, "pivot": [0.2, 0.5]})
    # 爆炸 / 烟幕 / 火焰
    for name, frames, cols, fps, blend, loop in [
        ("Explosion", vfx_explosion(size, rng_for("expl"), 8), 4, 24, "additive", False),
        ("Smoke", vfx_smoke(size, rng_for("smk"), 12), 4, 14, "translucent", False),
        ("Fire", vfx_fire(size, rng_for("fire"), 8), 4, 16, "additive", True),
    ]:
        sheet = sheet_from_frames(frames, cols=cols)
        rows = int(math.ceil(len(frames) / cols))
        save_image(sheet, "VFX", "VFX_%s.png" % name, "vfx_sheet", srgb=True, compression="TC_Default",
                   frames=len(frames), frame_w=size, frame_h=size, cols=cols, rows=rows, fps=fps,
                   blend=blend, animated=True)
        VFX_SHEETS.append({"name": name, "sheet": "RedFront/Art2D/VFX/VFX_%s.png" % name, "frames": len(frames),
                           "frame_w": size, "frame_h": size, "cols": cols, "rows": rows, "fps": fps,
                           "blend": blend, "loop": loop, "pivot": [0.5, 0.5]})
        log("    · VFX_%-10s %d 帧  %d×%d" % (name, len(frames), sheet.width, sheet.height))
    # 命中材质
    for kind in ["dirt", "metal", "wood", "brick", "water", "flesh"]:
        fs = vfx_impact(size, rng_for("imp_" + kind), kind)
        sheet = sheet_from_frames(fs, cols=3)
        save_image(sheet, "VFX", "VFX_Impact_%s.png" % kind, "vfx_sheet", srgb=True, compression="TC_Default",
                   frames=6, frame_w=size, frame_h=size, cols=3, rows=2, fps=24, blend="translucent", animated=True)
        VFX_SHEETS.append({"name": "Impact_" + kind, "sheet": "RedFront/Art2D/VFX/VFX_Impact_%s.png" % kind,
                           "frames": 6, "frame_w": size, "frame_h": size, "cols": 3, "rows": 2, "fps": 24,
                           "blend": "translucent", "loop": False, "pivot": [0.5, 0.6]})
    fs = vfx_tracer(64, rng_for("tracer"))
    sheet = sheet_from_frames(fs, cols=3)
    save_image(sheet, "VFX", "VFX_Tracer.png", "vfx_sheet", srgb=True, compression="TC_Default",
               frames=3, frame_w=64, frame_h=64, cols=3, rows=1, fps=30, blend="additive", animated=True)
    VFX_SHEETS.append({"name": "Tracer", "sheet": "RedFront/Art2D/VFX/VFX_Tracer.png", "frames": 3,
                       "frame_w": 64, "frame_h": 64, "cols": 3, "rows": 1, "fps": 30, "blend": "additive",
                       "loop": False, "pivot": [0.5, 0.5]})
    log("    · 命中材质 6 种 + 弹道拖尾")


def gen_characters(res):
    size = res["character"]
    log("[角色] 占位士兵精灵 %d×%d/帧（占位美术：结构、朝向与动画节奏正确，可随时替换为手绘素材）" % (size, size))
    for unit, cfg in UNITS.items():
        for anim, (frames, fps) in ANIMS.items():
            cols = 4  # 4 方向（左向由右向镜像）
            sheet = Image.new("RGBA", (size * cols, size * frames), (0, 0, 0, 0))
            for fi in range(frames):
                for di in range(cols):
                    fr = draw_soldier(size, unit, cfg, di, anim, fi)
                    sheet.paste(fr, (di * size, fi * size))
            save_image(sheet, "Characters", "SPR_%s_%s.png" % (unit, anim), "sprite_sheet",
                       srgb=True, compression="TC_EditorIcon" if False else "TC_Default",
                       frames=frames, frame_w=size, frame_h=size, cols=cols, rows=frames,
                       layout="direction_column_frame_row", animated=True, placeholder=True,
                       fps=fps, unit=unit, anim=anim)
            SHEETS.append({"unit": unit, "anim": anim, "sheet": "RedFront/Art2D/Characters/SPR_%s_%s.png" % (unit, anim),
                           "directions": cols, "frames": frames, "frame_w": size, "frame_h": size,
                           "cols": cols, "rows": frames, "fps": fps, "mirror_for_left": [2],
                           "placeholder": True, "desc": cfg["desc"]})
        log("    · %-16s idle/walk/fire/reload/death（4 方向 × 28 帧）" % unit)
    shadow = build_unit_shadow(size * 2)
    save_image(shadow, "Characters", "SPR_UnitShadow.png", "shadow", srgb=True, compression="TC_Default", animated=False)
    log("    · 单位阴影 SPR_UnitShadow.png")


def gen_parallax(res):
    log("[视差] 背景层 %d×%d" % res["parallax"])
    for key, desc, colors in PARALLAX:
        img = build_parallax(key, desc, res["parallax"], colors)
        save_image(img, "Parallax", "BG_%s.png" % key, "background", srgb=True, compression="TC_Default",
                   tiling=True, parallax_layer=1, desc=desc)
        log("    · BG_%-14s %s" % (key, desc))


def gen_ui(res):
    size = res["ui_panel"]
    log("[UI] 面板/暗角/颗粒/准星/阴影")
    panel = build_ui_panel(size)
    save_image(panel, "UI", "UI_PanelFrame.png", "ui", srgb=True, compression="TC_EditorIcon",
               tiling=False, nine_slice=[int(size * 0.16)] * 4)
    vig = build_vignette(1920, 1080)
    save_image(vig, "UI", "UI_Vignette.png", "ui", srgb=True, compression="TC_Default", tiling=False)
    grain = build_grain(512)
    save_image(grain, "UI", "UI_Grain.png", "ui", srgb=True, compression="TC_Grayscale", tiling=True)
    ch = build_crosshair(64)
    for i, img in enumerate(ch):
        save_image(img, "UI", "UI_Crosshair_%d.png" % (i + 1), "ui", srgb=True, compression="TC_EditorIcon",
                   tiling=False, state=["tight", "open", "reloading"][i])
    log("    · 面板（9 宫格 margin=%d）/暗角/颗粒/准星 3 态" % int(size * 0.16))


# ----------------------------------------------------------------------------
# 材质定义（供 UE 编辑器脚本创建 Master Material / Instance）
# ----------------------------------------------------------------------------

def build_material_defs():
    MATERIALS = [
        {
            "name": "M_Terrain_2D", "type": "master", "domain": "surface", "blend": "opaque",
            "usage": "地形地面（9 种地形 + 8 种天气覆盖由材质实例切换）",
            "texture_params": [
                {"name": "BaseColorTex", "role": "basecolor", "default": "T_field_BaseColor"},
                {"name": "NormalTex", "role": "normal", "default": "T_field_Normal"},
                {"name": "RoughnessTex", "role": "roughness", "default": "T_field_Roughness"},
                {"name": "OverlayTex", "role": "overlay", "default": "T_snow_fresh"},
            ],
            "scalar_params": [
                {"name": "OverlayAmount", "default": 0.0, "range": [0, 1], "desc": "天气覆盖强度（雪/泥/灰烬）"},
                {"name": "Wetness", "default": 0.0, "range": [0, 1], "desc": "湿润度：降低粗糙度、加深底色"},
                {"name": "UVScale", "default": 4.0, "range": [0.5, 32], "desc": "世界单位→贴图 UV 缩放（可无缝平铺）"},
                {"name": "DamageBlend", "default": 0.0, "range": [0, 1], "desc": "战损叠加（裂缝/烟熏 Decal 由该参数驱动）"},
                {"name": "Tint", "default": [1, 1, 1], "desc": "关卡色彩脚本统一色调"},
            ],
        },
        {
            "name": "M_Sprite_Lit", "type": "master", "domain": "surface", "blend": "masked",
            "usage": "受光 2D 精灵（角色、载具、掩体），Paper2D 默认材质的本作替代",
            "texture_params": [
                {"name": "SpriteTex", "role": "basecolor", "default": "SPR_sov_rifleman_idle"},
                {"name": "NormalTex", "role": "normal", "default": "T_field_Normal"},
            ],
            "scalar_params": [
                {"name": "RimPower", "default": 3.0, "range": [0.5, 8], "desc": "轮廓光强度（2D 立体感）"},
                {"name": "DamageFlash", "default": 0.0, "range": [0, 1], "desc": "受击白闪"},
                {"name": "SnowDust", "default": 0.0, "range": [0, 1], "desc": "雪地行军挂雪"},
                {"name": "Phase", "default": 0.0, "desc": "动画相位（供材质内摆动使用）"},
            ],
        },
        {
            "name": "M_Decal_2D", "type": "master", "domain": "deferred_decal", "blend": "translucent",
            "usage": "贴花：弹坑、血迹、履带印、燃烧痕、水洼",
            "texture_params": [{"name": "DecalTex", "role": "decal", "default": "D_crater_medium"}],
            "scalar_params": [
                {"name": "Opacity", "default": 1.0, "range": [0, 1], "desc": "整体不透明度（血迹可随关卡减弱）"},
                {"name": "FadeRadius", "default": 0.35, "range": [0, 1], "desc": "边缘淡出，避免硬边"},
                {"name": "WetShine", "default": 0.0, "range": [0, 1], "desc": "水洼/血迹的湿亮反射"},
            ],
        },
        {
            "name": "M_VFX_Additive", "type": "master", "domain": "surface", "blend": "additive",
            "usage": "枪口焰、爆炸、火焰、弹道拖尾",
            "texture_params": [{"name": "VFXTex", "role": "vfx", "default": "VFX_Explosion"}],
            "scalar_params": [
                {"name": "EmissiveBoost", "default": 6.0, "range": [0, 40], "desc": "自发光强度（HDR，配合 Bloom）"},
                {"name": "FrameIndex", "default": 0.0, "desc": "序列帧索引（由动画蓝图驱动）"},
                {"name": "Fade", "default": 1.0, "range": [0, 1], "desc": "生命周期淡出"},
            ],
        },
        {
            "name": "M_Smoke_Translucent", "type": "master", "domain": "surface", "blend": "translucent",
            "usage": "烟幕、尘土、烟柱（遮蔽判定与视觉烟幕共用）",
            "texture_params": [{"name": "SmokeTex", "role": "vfx", "default": "VFX_Smoke"}],
            "scalar_params": [
                {"name": "Density", "default": 0.55, "range": [0, 1], "desc": "烟幕浓度（同时驱动视觉与遮蔽查询）"},
                {"name": "FrameIndex", "default": 0.0, "desc": "序列帧索引"},
                {"name": "Tint", "default": [0.82, 0.8, 0.76], "desc": "烟色（雪地偏冷、燃烧偏暖）"},
            ],
        },
        {
            "name": "M_Parallax_Layer", "type": "master", "domain": "surface", "blend": "translucent",
            "usage": "视差背景层（远景/中景），滚动由视差系数驱动",
            "texture_params": [{"name": "LayerTex", "role": "background", "default": "BG_city_ruins"}],
            "scalar_params": [
                {"name": "ParallaxFactor", "default": 0.4, "range": [0, 1], "desc": "视差系数（0 = 不动，1 = 与战场同速）"},
                {"name": "FogDensity", "default": 0.35, "range": [0, 1], "desc": "大气透视强度（能见度越低越强）"},
            ],
        },
        {
            "name": "M_UI_Panel", "type": "master", "domain": "ui", "blend": "translucent",
            "usage": "HUD 面板（9 宫格）与暗角/颗粒",
            "texture_params": [{"name": "PanelTex", "role": "ui", "default": "UI_PanelFrame"}],
            "scalar_params": [
                {"name": "Opacity", "default": 1.0, "range": [0, 1]},
                {"name": "Tint", "default": [0.1, 0.11, 0.08]},
            ],
        },
    ]
    # 材质实例：9 地形 × 天气组合（只生成有代表性的组合，避免资产爆炸）
    instances = []
    weather_for = {
        "clear": None, "rain": "rain_sheen", "snow": "snow_fresh", "fog": None,
        "mud": "mud_wet", "storm": "snow_fresh",
    }
    for key, cfg in TERRAINS.items():
        instances.append({
            "name": "MI_Terrain_%s" % key, "parent": "M_Terrain_2D",
            "textures": {"BaseColorTex": "T_%s_BaseColor" % key, "NormalTex": "T_%s_Normal" % key,
                         "RoughnessTex": "T_%s_Roughness" % key, "OverlayTex": "T_snow_fresh"},
            "scalars": {"OverlayAmount": 0.0, "Wetness": 0.35 if key in ("river", "trench") else 0.05, "UVScale": 4.0},
        })
        for wkey, okey in weather_for.items():
            if not okey:
                continue
            inst = {
                "name": "MI_Terrain_%s_%s" % (key, wkey), "parent": "M_Terrain_2D",
                "textures": {"BaseColorTex": "T_%s_BaseColor" % key, "NormalTex": "T_%s_Normal" % key,
                             "RoughnessTex": "T_%s_Roughness" % key, "OverlayTex": "T_%s" % okey},
                "scalars": {"OverlayAmount": 0.85 if wkey in ("snow", "storm") else 0.6,
                            "Wetness": 0.7 if wkey in ("rain", "mud") else (0.25 if wkey == "snow" else 0.1),
                            "UVScale": 4.0},
            }
            instances.append(inst)
    for d in ["crater_medium", "crater_large", "rubble", "blood_1", "blood_2", "blood_3",
              "tracks_straight", "tracks_turn", "burn_scorch", "bootprints", "puddle"]:
        instances.append({"name": "MI_Decal_%s" % d, "parent": "M_Decal_2D",
                          "textures": {"DecalTex": "D_%s" % d},
                          "scalars": {"Opacity": 0.7 if d.startswith("boot") else 1.0, "FadeRadius": 0.35,
                                      "WetShine": 0.6 if d == "puddle" else 0.0}})
    for k, o in [("snow_fresh", "snow_fresh"), ("snow_trodden", "snow_trodden"), ("mud_wet", "mud_wet"),
                 ("water_film", "water_film"), ("dust_dry", "dust_dry"), ("ash_scorch", "ash_scorch"),
                 ("frost_crystal", "frost_crystal"), ("rain_sheen", "rain_sheen")]:
        instances.append({"name": "MI_Overlay_%s" % k, "parent": "M_Terrain_2D",
                          "textures": {"OverlayTex": "T_%s" % o, "BaseColorTex": "T_field_BaseColor",
                                       "NormalTex": "T_field_Normal", "RoughnessTex": "T_field_Roughness"},
                          "scalars": {"OverlayAmount": 1.0, "Wetness": 0.2, "UVScale": 2.0}})
    for key, _, _ in PARALLAX:
        instances.append({"name": "MI_BG_%s" % key, "parent": "M_Parallax_Layer",
                          "textures": {"LayerTex": "BG_%s" % key},
                          "scalars": {"ParallaxFactor": 0.15, "FogDensity": 0.4}})
    return MATERIALS, instances


def main():
    ap = argparse.ArgumentParser(description="RedFront 1941 程序化美术/材质资产生成器")
    ap.add_argument("--quick", action="store_true", help="半分辨率快速生成")
    ap.add_argument("--only", default="", help="只生成指定类别，逗号分隔：environment,overlays,decals,vfx,characters,parallax,ui")
    args = ap.parse_args()

    global RES
    if args.quick:
        RES = dict(RES)
        RES["terrain"] = 1024
        RES["normal"] = 512
        RES["overlay"] = 512
        RES["decal"] = 256
        RES["decal_large"] = 512
        RES["vfx"] = 128
        RES["character"] = 96
        RES["parallax"] = (1024, 256)

    only = [s.strip() for s in args.only.split(",") if s.strip()] or \
        ["environment", "overlays", "decals", "vfx", "characters", "parallax", "ui"]

    ensure_dirs()
    t0 = time.time()
    log("=" * 70)
    log("RedFront 1941 程序化美术生成  %s" % ("快速模式" if args.quick else "完整模式"))
    log("输出：%s" % ART_ROOT)
    log("=" * 70)

    if "environment" in only:
        gen_environment(RES)
    if "overlays" in only:
        gen_overlays(RES)
    if "decals" in only:
        gen_decals(RES)
    if "vfx" in only:
        gen_vfx(RES)
    if "characters" in only:
        gen_characters(RES)
    if "parallax" in only:
        gen_parallax(RES)
    if "ui" in only:
        gen_ui(RES)

    mats, insts = build_material_defs()
    global MATERIALS
    MATERIALS = mats + [dict(i, type="instance") for i in insts]

    manifest = write_manifest(args.quick, only)
    total_bytes = sum(os.path.getsize(a["file"]) for a in ASSETS)
    log("-" * 70)
    log("完成：%d 张贴图 / %d 个材质与实例 / %d 张角色精灵表 / %d 个 VFX 序列帧" %
        (len(ASSETS), len(MATERIALS), len(SHEETS), len(VFX_SHEETS)))
    log("占用：%.1f MB   耗时：%.1fs" % (total_bytes / 1048576.0, time.time() - t0))
    log("=" * 70)


if __name__ == "__main__":
    main()
