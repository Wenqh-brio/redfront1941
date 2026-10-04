/* =============================================================================
 *  RedFront 1941 — 渲染层（Canvas 2D）
 *  只读取 Game 的状态，不修改任何逻辑；正交俯视 + 分层阴影，追求高清手绘质感。
 * ===========================================================================*/
(function (global) {
  'use strict';
  var RF = global.RF = global.RF || {};
  var U = RF.util;

  function drawGround(ctx, g, W, H, zoom, cx, cy) {
    var pal = g.map.palette;
    ctx.fillStyle = pal.ground;
    ctx.fillRect(0, 0, W, H);

    // 地面色斑（世界坐标 → 屏幕）
    for (var i = 0; i < g.map.decals.length; i++) {
      var d = g.map.decals[i];
      var sx = (d.x - cx) * zoom + W / 2, sy = (d.y - cy) * zoom + H / 2;
      if (sx < -160 || sy < -160 || sx > W + 160 || sy > H + 160) continue;
      if (d.kind === 'patch') {
        ctx.globalAlpha = d.a || 0.08;
        ctx.fillStyle = pal.patch;
        ctx.beginPath(); ctx.ellipse(sx, sy, d.r * zoom, d.r * zoom * 0.7, 0, 0, Math.PI * 2); ctx.fill();
        ctx.globalAlpha = 1;
      } else if (d.kind === 'crater') {
        ctx.fillStyle = 'rgba(20,16,12,0.55)';
        ctx.beginPath(); ctx.ellipse(sx, sy, d.r * zoom, d.r * zoom * 0.78, 0, 0, Math.PI * 2); ctx.fill();
        ctx.strokeStyle = 'rgba(120,105,80,0.35)'; ctx.lineWidth = 2 * zoom;
        ctx.beginPath(); ctx.ellipse(sx, sy, d.r * zoom, d.r * zoom * 0.78, 0, 0, Math.PI * 2); ctx.stroke();
      } else {
        ctx.fillStyle = 'rgba(90,86,78,0.30)';
        ctx.fillRect(sx - d.r * zoom, sy - d.r * zoom * 0.5, d.r * 2 * zoom, d.r * zoom);
      }
    }
  }

  function drawObjectiveZone(ctx, g, W, H, zoom, cx, cy) {
    var z = g.map.objectiveZone;
    var sx = (z.x - cx) * zoom + W / 2, sy = (z.y - cy) * zoom + H / 2;
    ctx.strokeStyle = g.mode === 'endless' ? 'rgba(120,200,140,0.5)' : 'rgba(230,190,90,0.55)';
    ctx.lineWidth = 2;
    ctx.setLineDash([10, 8]);
    ctx.strokeRect(sx, sy, z.w * zoom, z.h * zoom);
    ctx.setLineDash([]);
    ctx.fillStyle = 'rgba(230,190,90,0.07)';
    ctx.fillRect(sx, sy, z.w * zoom, z.h * zoom);
  }

  function drawObstacles(ctx, g, W, H, zoom, cx, cy) {
    var obs = g.map.obstacles;
    for (var i = 0; i < obs.length; i++) {
      var o = obs[i];
      var sx = (o.x - cx) * zoom + W / 2, sy = (o.y - cy) * zoom + H / 2;
      var sw = o.w * zoom, sh = o.h * zoom;
      if (sx + sw < -80 || sy + sh < -80 || sx > W + 80 || sy > H + 80) continue;

      if (o.kind === 'tree') {
        ctx.fillStyle = 'rgba(0,0,0,0.28)';
        ctx.beginPath(); ctx.ellipse(sx + sw / 2 + 6, sy + sh / 2 + 8, sw / 2, sh / 2.4, 0, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = g.map.palette.accent;
        ctx.beginPath(); ctx.arc(sx + sw / 2, sy + sh / 2, sw / 2, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = 'rgba(255,255,255,0.05)';
        ctx.beginPath(); ctx.arc(sx + sw / 2 - sw * 0.12, sy + sh / 2 - sh * 0.12, sw / 3.2, 0, Math.PI * 2); ctx.fill();
      } else if (o.kind === 'sandbag') {
        ctx.fillStyle = '#7a6c4c';
        ctx.fillRect(sx, sy, sw, sh);
        ctx.fillStyle = 'rgba(0,0,0,0.25)';
        for (var k = 0; k < sw; k += 14 * zoom) ctx.fillRect(sx + k, sy, 1.5 * zoom, sh);
        ctx.fillStyle = 'rgba(255,240,200,0.08)';
        ctx.fillRect(sx, sy, sw, 2 * zoom);
      } else if (o.kind === 'building') {
        ctx.fillStyle = 'rgba(0,0,0,0.35)';
        ctx.fillRect(sx + 8, sy + 10, sw, sh);
        ctx.fillStyle = o.destroyed ? '#5a5450' : '#6a625a';
        ctx.fillRect(sx, sy, sw, sh);
        ctx.fillStyle = 'rgba(255,255,255,0.06)';
        ctx.fillRect(sx, sy, sw, 5 * zoom);
        ctx.strokeStyle = 'rgba(0,0,0,0.5)'; ctx.lineWidth = 1.5;
        ctx.strokeRect(sx, sy, sw, sh);
        // 窗户
        ctx.fillStyle = 'rgba(20,22,26,0.8)';
        for (var wx = sx + 12 * zoom; wx < sx + sw - 14 * zoom; wx += 34 * zoom) {
          for (var wy = sy + 12 * zoom; wy < sy + sh - 14 * zoom; wy += 30 * zoom) {
            ctx.fillRect(wx, wy, 12 * zoom, 14 * zoom);
          }
        }
        if (o.destroyed) {
          ctx.fillStyle = 'rgba(30,25,20,0.5)';
          ctx.fillRect(sx, sy, sw, sh);
        }
      } else if (o.kind === 'hedge') {
        ctx.fillStyle = '#3f4a2c';
        ctx.fillRect(sx, sy, sw, sh);
        ctx.fillStyle = 'rgba(255,255,255,0.05)';
        ctx.fillRect(sx, sy, sw, 3 * zoom);
      } else {
        ctx.fillStyle = 'rgba(0,0,0,0.3)';
        ctx.fillRect(sx + 5, sy + 7, sw, sh);
        ctx.fillStyle = '#5d564c';
        ctx.fillRect(sx, sy, sw, sh);
        ctx.strokeStyle = 'rgba(0,0,0,0.45)'; ctx.lineWidth = 1.2;
        ctx.strokeRect(sx, sy, sw, sh);
      }
    }
  }

  function drawUnit(ctx, x, y, r, body, head, facing, opts) {
    opts = opts || {};
    ctx.save();
    // 阴影
    ctx.fillStyle = 'rgba(0,0,0,0.32)';
    ctx.beginPath(); ctx.ellipse(x + 4, y + 6, r * 1.05, r * 0.85, 0, 0, Math.PI * 2); ctx.fill();
    // 身体
    ctx.fillStyle = body;
    ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill();
    // 头盔高光
    ctx.fillStyle = head;
    ctx.beginPath(); ctx.arc(x, y, r * 0.62, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = 'rgba(255,255,255,0.18)';
    ctx.beginPath(); ctx.arc(x - r * 0.2, y - r * 0.22, r * 0.34, 0, Math.PI * 2); ctx.fill();
    // 枪
    if (!opts.noWeapon) {
      ctx.strokeStyle = opts.weaponColor || '#20211d';
      ctx.lineWidth = Math.max(2, r * 0.32);
      ctx.beginPath();
      ctx.moveTo(x + Math.cos(facing) * r * 0.6, y + Math.sin(facing) * r * 0.6);
      ctx.lineTo(x + Math.cos(facing) * (r + 9), y + Math.sin(facing) * (r + 9));
      ctx.stroke();
    }
    ctx.restore();
  }

  function drawHealthBar(ctx, x, y, w, ratio, color) {
    ctx.fillStyle = 'rgba(0,0,0,0.6)';
    ctx.fillRect(x - w / 2, y, w, 4);
    ctx.fillStyle = color;
    ctx.fillRect(x - w / 2, y, w * U.clamp(ratio, 0, 1), 4);
  }

  function drawSmoke(ctx, p, W, H, zoom, cx, cy) {
    var a = U.clamp(p.t / p.maxT, 0, 1);
    var r = (p.r || 40) * zoom;
    var sx = (p.x - cx) * zoom + W / 2, sy = (p.y - cy) * zoom + H / 2;
    for (var k = 0; k < 6; k++) {
      var ang = k * 1.05, rr = r * (0.35 + (k % 3) * 0.22);
      ctx.globalAlpha = 0.16 * a;
      ctx.fillStyle = '#d8d4c8';
      ctx.beginPath();
      ctx.arc(sx + Math.cos(ang) * r * 0.34, sy + Math.sin(ang) * r * 0.34, rr * 0.62, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  }

  RF.renderWorld = function (ctx, g, W, H, mouse) {
    var zoom = 1.0;
    var cx = g.camera.x, cy = g.camera.y;
    drawGround(ctx, g, W, H, zoom, cx, cy);
    drawObjectiveZone(ctx, g, W, H, zoom, cx, cy);
    drawObstacles(ctx, g, W, H, zoom, cx, cy);

    var i, p, sx, sy;

    // 支援待弹着标记
    var warn = g.pendingWarnings();
    for (i = 0; i < warn.length; i++) {
      var w = warn[i];
      sx = (w.x - cx) * zoom + W / 2; sy = (w.y - cy) * zoom + H / 2;
      var pulse = 0.5 + 0.5 * Math.sin(g.time * 6);
      ctx.strokeStyle = w.enemy ? 'rgba(255,80,70,' + (0.4 + pulse * 0.5) + ')' : 'rgba(255,210,90,' + (0.4 + pulse * 0.5) + ')';
      ctx.lineWidth = 2;
      ctx.beginPath(); ctx.arc(sx, sy, 16 + pulse * 8, 0, Math.PI * 2); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(sx - 22, sy); ctx.lineTo(sx + 22, sy); ctx.moveTo(sx, sy - 22); ctx.lineTo(sx, sy + 22); ctx.stroke();
      ctx.fillStyle = w.enemy ? '#ff6b5e' : '#ffd25a';
      ctx.font = '12px "Segoe UI", system-ui, sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText((w.enemy ? '敌方火力 ' : '') + w.t.toFixed(1) + 's', sx, sy - 26);
    }

    // 整备阶段补给点
    for (i = 0; i < g.pickups.length; i++) {
      var it = g.pickups[i];
      sx = (it.x - cx) * zoom + W / 2; sy = (it.y - cy) * zoom + H / 2;
      ctx.fillStyle = it.type === 'water' ? '#5fa8d3' : it.type === 'food' ? '#c9a227' : it.type === 'med' ? '#c0455a' : '#9aa06b';
      ctx.fillRect(sx - 8, sy - 8, 16, 16);
      ctx.strokeStyle = 'rgba(0,0,0,0.5)'; ctx.lineWidth = 1.5; ctx.strokeRect(sx - 8, sy - 8, 16, 16);
    }

    // 手榴弹
    for (i = 0; i < g.grenades.length; i++) {
      var gr = g.grenades[i];
      sx = (gr.x - cx) * zoom + W / 2; sy = (gr.y - cy) * zoom + H / 2;
      ctx.fillStyle = '#3f5233';
      ctx.beginPath(); ctx.arc(sx, sy, 5, 0, Math.PI * 2); ctx.fill();
      ctx.strokeStyle = 'rgba(255,140,60,0.9)'; ctx.lineWidth = 1.5;
      ctx.beginPath(); ctx.arc(sx, sy, 5 + gr.fuse * 3, 0, Math.PI * 2); ctx.stroke();
    }

    // 敌人
    for (i = 0; i < g.enemies.length; i++) {
      var e = g.enemies[i]; if (!e.alive) continue;
      sx = (e.x - cx) * zoom + W / 2; sy = (e.y - cy) * zoom + H / 2;
      if (sx < -60 || sy < -60 || sx > W + 60 || sy > H + 60) continue;
      var body = e.role === 'elite' ? '#5b5f4a' : (e.role === 'support' ? '#5a5148' : '#63665a');
      if (e.state === 'flee') body = '#7a7160';
      drawUnit(ctx, sx, sy, e.r, body, '#3c3f36', e.facing, { weaponColor: '#181915' });
      if (e.hp < e.maxHp) drawHealthBar(ctx, sx, sy - e.r - 9, 22, e.hp / e.maxHp, '#d2543f');
      if (e.suppression > 1.5) {
        ctx.fillStyle = 'rgba(255,120,60,0.85)';
        ctx.font = '11px sans-serif'; ctx.textAlign = 'center';
        ctx.fillText('压制', sx, sy - e.r - 13);
      }
      if (e.marks > 0) {
        ctx.strokeStyle = 'rgba(255,90,80,0.9)'; ctx.lineWidth = 1.4;
        ctx.beginPath(); ctx.arc(sx, sy, e.r + 6, 0, Math.PI * 2); ctx.stroke();
      }
    }

    // 载具
    for (i = 0; i < g.vehicles.length; i++) {
      var v = g.vehicles[i]; if (!v.alive) continue;
      sx = (v.x - cx) * zoom + W / 2; sy = (v.y - cy) * zoom + H / 2;
      ctx.save();
      ctx.translate(sx, sy); ctx.rotate(v.facing);
      ctx.fillStyle = 'rgba(0,0,0,0.35)';
      ctx.fillRect(-v.r + 5, -v.r * 0.8 + 6, v.r * 2, v.r * 1.6);
      ctx.fillStyle = '#4b4a41';
      ctx.fillRect(-v.r, -v.r * 0.8, v.r * 2, v.r * 1.6);
      ctx.fillStyle = '#5a584d';
      ctx.fillRect(-v.r * 0.3, -v.r * 0.5, v.r * 1.1, v.r);
      ctx.strokeStyle = 'rgba(0,0,0,0.55)'; ctx.lineWidth = 1.6;
      ctx.strokeRect(-v.r, -v.r * 0.8, v.r * 2, v.r * 1.6);
      ctx.strokeStyle = '#2a2b26'; ctx.lineWidth = 3;
      ctx.beginPath(); ctx.moveTo(v.r * 0.6, 0); ctx.lineTo(v.r * 1.5, 0); ctx.stroke();
      ctx.restore();
      drawHealthBar(ctx, sx, sy - v.r - 10, 40, v.hp / v.maxHp, '#e0b13c');
      ctx.fillStyle = 'rgba(240,230,210,0.85)'; ctx.font = '11px sans-serif'; ctx.textAlign = 'center';
      ctx.fillText(v.name, sx, sy - v.r - 14);
    }

    // 友军
    for (i = 0; i < g.allies.length; i++) {
      var a = g.allies[i]; if (!a.alive) continue;
      sx = (a.x - cx) * zoom + W / 2; sy = (a.y - cy) * zoom + H / 2;
      if (a.kind === 'ally_tank') {
        ctx.save(); ctx.translate(sx, sy); ctx.rotate(a.facing);
        ctx.fillStyle = '#3f5140'; ctx.fillRect(-a.r, -a.r * 0.8, a.r * 2, a.r * 1.6);
        ctx.fillStyle = '#4b5f4a'; ctx.fillRect(-a.r * 0.3, -a.r * 0.5, a.r * 1.1, a.r);
        ctx.strokeStyle = '#e8e2d0'; ctx.lineWidth = 1.4; ctx.strokeRect(-a.r, -a.r * 0.8, a.r * 2, a.r * 1.6);
        ctx.restore();
        drawHealthBar(ctx, sx, sy - a.r - 10, 38, a.hp / a.maxHp, '#79b46a');
      } else {
        drawUnit(ctx, sx, sy, a.r, '#3f5d43', '#2f4733', a.facing, { weaponColor: '#1a1b18' });
      }
    }

    // 玩家
    if (g.player.alive) {
      var px = (g.player.x - cx) * zoom + W / 2, py = (g.player.y - cy) * zoom + H / 2;
      if (g.player.binocular > 0) {
        ctx.strokeStyle = 'rgba(255,220,120,0.35)'; ctx.lineWidth = 1.2;
        ctx.beginPath(); ctx.arc(px, py, 90 + Math.sin(g.time * 3) * 6, 0, Math.PI * 2); ctx.stroke();
      }
      drawUnit(ctx, px, py, g.player.r + 1, '#8a2f2a', '#c9b28a', g.player.facing, { weaponColor: '#15160f' });
      // 体力环
      ctx.strokeStyle = 'rgba(120,220,255,0.85)'; ctx.lineWidth = 2;
      ctx.beginPath(); ctx.arc(px, py, g.player.r + 7, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * (g.player.stamina / 100)); ctx.stroke();
      if (g.player.actionTimer > 0) {
        ctx.fillStyle = 'rgba(0,0,0,0.6)'; ctx.fillRect(px - 34, py - 30, 68, 12);
        ctx.fillStyle = '#7fd18b';
        ctx.fillRect(px - 33, py - 29, 66 * (1 - g.player.actionTimer / 3), 10);
      }
    }

    // 子弹
    for (i = 0; i < g.projectiles.length; i++) {
      p = g.projectiles[i];
      sx = (p.x - cx) * zoom + W / 2; sy = (p.y - cy) * zoom + H / 2;
      var tx = sx - p.vx * 0.012 * zoom, ty = sy - p.vy * 0.012 * zoom;
      if (p.flame) {
        ctx.strokeStyle = 'rgba(255,150,50,0.6)'; ctx.lineWidth = 9;
      } else if (p.owner === 'player') {
        ctx.strokeStyle = 'rgba(255,236,170,0.9)'; ctx.lineWidth = p.heavy ? 3 : 1.5;
      } else if (p.owner === 'ally') {
        ctx.strokeStyle = 'rgba(180,235,255,0.85)'; ctx.lineWidth = 1.4;
      } else {
        ctx.strokeStyle = 'rgba(255,130,110,0.85)'; ctx.lineWidth = 1.4;
      }
      ctx.beginPath(); ctx.moveTo(tx, ty); ctx.lineTo(sx, sy); ctx.stroke();
    }

    // 粒子
    for (i = 0; i < g.particles.length; i++) {
      p = g.particles[i];
      sx = (p.x - cx) * zoom + W / 2; sy = (p.y - cy) * zoom + H / 2;
      if (p.kind === 'boom') {
        var t = 1 - p.t / p.maxT;
        ctx.globalAlpha = Math.max(0, 1 - t);
        var grad = ctx.createRadialGradient(sx, sy, 0, sx, sy, p.r);
        grad.addColorStop(0, 'rgba(255,240,180,0.95)');
        grad.addColorStop(0.45, 'rgba(255,140,40,0.75)');
        grad.addColorStop(1, 'rgba(60,40,30,0)');
        ctx.fillStyle = grad;
        ctx.beginPath(); ctx.arc(sx, sy, p.r, 0, Math.PI * 2); ctx.fill();
        ctx.globalAlpha = 1;
      } else if (p.kind === 'muzzle') {
        ctx.globalAlpha = p.t / p.maxT;
        ctx.fillStyle = 'rgba(255,230,150,0.95)';
        ctx.beginPath(); ctx.arc(sx, sy, 7 * (p.s || 1), 0, Math.PI * 2); ctx.fill();
        ctx.globalAlpha = 1;
      } else if (p.kind === 'blood') {
        ctx.globalAlpha = p.t / p.maxT * 0.85;
        ctx.fillStyle = '#7d2018';
        ctx.beginPath(); ctx.arc(sx, sy, p.r, 0, Math.PI * 2); ctx.fill();
        ctx.globalAlpha = 1;
      } else if (p.kind === 'spark') {
        ctx.globalAlpha = p.t / p.maxT;
        ctx.fillStyle = '#ffd88a';
        ctx.beginPath(); ctx.arc(sx, sy, 4, 0, Math.PI * 2); ctx.fill();
        ctx.globalAlpha = 1;
      } else if (p.kind === 'smoke') {
        drawSmoke(ctx, p, W, H, zoom, cx, cy);
      } else if (p.kind === 'fire') {
        for (var f = 0; f < 5; f++) {
          ctx.globalAlpha = 0.35 * (p.t / p.maxT);
          ctx.fillStyle = f % 2 ? '#ff8a2b' : '#ffd24a';
          ctx.beginPath();
          ctx.arc(sx + (Math.random() - 0.5) * p.r * 0.6, sy + (Math.random() - 0.5) * p.r * 0.6, 6 + Math.random() * 8, 0, Math.PI * 2);
          ctx.fill();
        }
        ctx.globalAlpha = 1;
      }
    }

    // 玩家标记（侦察/狙击）
    for (i = 0; i < g.player.marks.length; i++) {
      var m = g.player.marks[i];
      sx = (m.x - cx) * zoom + W / 2; sy = (m.y - cy) * zoom + H / 2;
      ctx.strokeStyle = 'rgba(120,200,255,0.8)'; ctx.lineWidth = 1.4;
      ctx.beginPath(); ctx.arc(sx, sy, 12, 0, Math.PI * 2); ctx.stroke();
    }

    // 瞄准十字线
    if (mouse && g.player.alive) {
      ctx.strokeStyle = 'rgba(255,255,255,0.55)'; ctx.lineWidth = 1;
      var spread = 8 + (g.player.suppressed * 4) + (g.player.recoil * 6);
      ctx.beginPath();
      ctx.moveTo(mouse.x - spread - 6, mouse.y); ctx.lineTo(mouse.x - spread, mouse.y);
      ctx.moveTo(mouse.x + spread, mouse.y); ctx.lineTo(mouse.x + spread + 6, mouse.y);
      ctx.moveTo(mouse.x, mouse.y - spread - 6); ctx.lineTo(mouse.x, mouse.y - spread);
      ctx.moveTo(mouse.x, mouse.y + spread); ctx.lineTo(mouse.x, mouse.y + spread + 6);
      ctx.stroke();
    }

    // 战场雾气（低能见度天气）
    var w = g.level && g.level.weather;
    var vis = (g.level && g.level.visibility_m) || 600;
    if (w === 'fog' || w === 'snow' || w === 'storm' || vis < 320) {
      var vig = ctx.createRadialGradient(W / 2, H / 2, Math.min(W, H) * 0.25, W / 2, H / 2, Math.max(W, H) * 0.62);
      vig.addColorStop(0, 'rgba(0,0,0,0)');
      vig.addColorStop(1, w === 'snow' ? 'rgba(200,205,215,0.55)' : 'rgba(40,45,50,0.72)');
      ctx.fillStyle = vig;
      ctx.fillRect(0, 0, W, H);
    }
  };

  RF.renderMinimap = function (ctx, g, W, H) {
    var sc = Math.min(W / g.map.w, H / g.map.h);
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = 'rgba(18,20,16,0.85)';
    ctx.fillRect(0, 0, W, H);
    ctx.fillStyle = 'rgba(120,120,100,0.35)';
    for (var i = 0; i < g.map.obstacles.length; i++) {
      var o = g.map.obstacles[i];
      if (o.kind === 'building' || o.kind === 'hedge') ctx.fillRect(o.x * sc, o.y * sc, Math.max(1, o.w * sc), Math.max(1, o.h * sc));
    }
    ctx.fillStyle = 'rgba(214,60,50,0.95)';
    for (i = 0; i < g.enemies.length; i++) {
      var e = g.enemies[i]; if (!e.alive) continue;
      ctx.fillRect(e.x * sc - 1.5, e.y * sc - 1.5, 3, 3);
    }
    ctx.fillStyle = 'rgba(230,180,60,0.95)';
    for (i = 0; i < g.vehicles.length; i++) {
      var v = g.vehicles[i]; if (!v.alive) continue;
      ctx.fillRect(v.x * sc - 2.5, v.y * sc - 2.5, 5, 5);
    }
    ctx.fillStyle = 'rgba(110,220,140,0.95)';
    ctx.beginPath(); ctx.arc(g.player.x * sc, g.player.y * sc, 3.2, 0, Math.PI * 2); ctx.fill();
    var z = g.map.objectiveZone;
    ctx.strokeStyle = 'rgba(230,190,90,0.8)'; ctx.lineWidth = 1;
    ctx.strokeRect(z.x * sc, z.y * sc, z.w * sc, z.h * sc);
  };

})(typeof window !== 'undefined' ? window : globalThis);
