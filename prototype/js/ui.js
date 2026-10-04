/* =============================================================================
 *  RedFront 1941 — 原型 UI 层（DOM 屏幕 / 关卡选择 / 装备负重 / HUD）
 * ===========================================================================*/
(function () {
  'use strict';
  var RF = window.RF;
  var D = window.RF_DATA;

  var $ = function (id) { return document.getElementById(id); };
  var state = {
    screen: 'menu', campaign: 'all', level: null, loadout: null,
    difficulty: 'regular', mode: 'campaign', faction: 'soviet',
    game: null, running: false, paused: false, armedSupport: null,
    tab: 'primary', lastT: 0, keys: {}, mouse: { x: 0, y: 0 }, test: false
  };
  window.RF_STATE = state;

  /* ---------------------------- 数据可用性检查 ---------------------------- */
  if (!D || !D.levels || !D.levels.length) {
    document.body.innerHTML =
      '<div style="padding:40px;font:16px/1.7 system-ui;color:#e8e3d6;background:#14150f;min-height:100vh">' +
      '<h1 style="color:#d8b45a">数据未生成</h1>' +
      '<p>原型需要 <code>js/data.generated.js</code>。请在项目根目录运行：</p>' +
      '<pre style="background:#1e201a;padding:16px;border-left:3px solid #d8b45a">node tools/build-data.mjs</pre>' +
      '<p>然后刷新本页。若关卡数据（data/levels/*.json）尚未就绪，原型仍可运行，但关卡列表为空。</p></div>';
    return;
  }

  var CAMPAIGNS = D.campaigns || [];
  var LEVELS = D.levels.slice().sort(function (a, b) { return a.index - b.index; });
  var DIFFS = Object.keys(D.difficulty || {});

  function campaignOf(id) {
    for (var i = 0; i < CAMPAIGNS.length; i++) if (CAMPAIGNS[i].id === id) return CAMPAIGNS[i];
    return null;
  }
  function levelById(id) {
    for (var i = 0; i < LEVELS.length; i++) if (LEVELS[i].id === id) return LEVELS[i];
    return null;
  }
  function esc(s) { return String(s == null ? '' : s).replace(/[&<>"]/g, function (c) { return ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]; }); }

  /* ------------------------------- 屏幕切换 ------------------------------- */
  function show(name) {
    state.screen = name;
    var screens = document.querySelectorAll('.screen');
    for (var i = 0; i < screens.length; i++) screens[i].classList.toggle('active', screens[i].id === 'screen-' + name);
    if (name !== 'game') stopLoop();
  }
  document.addEventListener('click', function (e) {
    var t = e.target.closest ? e.target.closest('[data-go]') : null;
    if (!t) return;
    var go = t.getAttribute('data-go');
    // 注意：各分支自行负责显示目标界面，不要在这里统一 show(go)，
    // 否则 'endless' 会覆盖 startEndlessSetup() 已经切到的 loadout 屏幕。
    if (go === 'levels') { renderLevels(); show('levels'); return; }
    if (go === 'endless') { startEndlessSetup(); return; }
    if (go === 'about') { renderAbout(); show('about'); return; }
    show(go);
  });

  /* ------------------------------- 主菜单元数据 ------------------------------- */
  function renderMenuMeta() {
    $('lvlCount').textContent = LEVELS.length;
    $('dataMeta').textContent = '数据版本 ' + (D.meta.generatedAt || '—') +
      ' · 关卡 ' + LEVELS.length + ' · 武器 ' + D.meta.weapons + ' · 专长 ' + D.meta.classes +
      ' · 支援 ' + D.meta.callIns + ' · 原型 v' + RF.VERSION;
  }

  function renderAbout() {
    var ul = $('campaignSummary');
    ul.innerHTML = CAMPAIGNS.map(function (c) {
      var n = LEVELS.filter(function (l) { return l.campaign_id === c.id; }).length;
      return '<li><b>' + esc(c.name_zh) + '</b>（' + n + ' 关，' + esc(c.start_date) + ' → ' + esc(c.end_date) + '）<br><span class="dim">' + esc(c.tone) + '</span></li>';
    }).join('');
  }

  /* ------------------------------- 关卡选择 ------------------------------- */
  function renderLevels() {
    var tabs = $('campaignTabs');
    var all = [{ id: 'all', name_zh: '全部战役' }].concat(CAMPAIGNS);
    tabs.innerHTML = all.map(function (c) {
      return '<button class="tab' + (state.campaign === c.id ? ' on' : '') + '" data-camp="' + c.id + '">' + esc(c.name_zh) + '</button>';
    }).join('');
    tabs.querySelectorAll('[data-camp]').forEach(function (b) {
      b.onclick = function () { state.campaign = b.getAttribute('data-camp'); renderLevels(); };
    });

    var list = LEVELS.filter(function (l) { return state.campaign === 'all' || l.campaign_id === state.campaign; });
    $('levelList').innerHTML = list.map(function (l) {
      var camp = campaignOf(l.campaign_id);
      return '<div class="level-card" data-level="' + l.id + '">' +
        '<div class="lc-top"><span class="lc-index">' + String(l.index).padStart(2, '0') + '</span>' +
        '<span class="lc-name">' + esc(l.name_zh) + '</span>' +
        '<span class="lc-diff">难度 ' + l.difficulty + '</span></div>' +
        '<div class="lc-meta"><span>' + esc(l.date) + '</span><span>' + esc(l.location_zh) + '</span></div>' +
        '<div class="lc-tags"><i>' + esc(missionTypeZh(l.mission_type)) + '</i><i>' + esc(weatherZh(l.weather)) + '</i><i>' + esc(terrainZh(l.terrain)) + '</i>' +
        (camp ? '<i class="camp">' + esc(camp.name_zh.split('—')[0]) + '</i>' : '') + '</div>' +
        '</div>';
    }).join('') || '<p class="hint">该战役暂无关卡数据。</p>';

    $('levelList').querySelectorAll('[data-level]').forEach(function (el) {
      el.onclick = function () {
        var l = levelById(el.getAttribute('data-level'));
        state.level = l; state.mode = 'campaign'; state.faction = l.faction;
        renderLevelDetail(l);
        renderBriefing(l);
      };
    });
  }

  function renderLevelDetail(l) {
    var camp = campaignOf(l.campaign_id);
    var enemies = Object.keys(l.enemy_composition || {}).map(function (k) {
      var e = findEnemy(k);
      return '<li>' + esc(e ? e.name_zh : k) + ' ×' + l.enemy_composition[k] + (e && e.note ? ' <span class="dim">' + esc(e.note) + '</span>' : '') + '</li>';
    }).join('');
    var sup = (l.available_support || []).map(function (s) { return '<li>' + esc(callInName(s)) + '</li>'; }).join('') || '<li class="dim">无</li>';
    $('levelDetail').innerHTML =
      '<h3>' + esc(l.name_zh) + ' <span class="dim">' + esc(l.name_en) + '</span></h3>' +
      '<p class="dim">' + esc(l.date) + ' · ' + esc(l.location_zh) + ' · ' + esc(l.coordinates) + '</p>' +
      '<p class="role">' + esc(l.player_role_zh) + '</p>' +
      '<h4>历史背景</h4><p class="history">' + esc(l.historical_note_zh) + '</p>' +
      '<h4>敌军</h4><ul class="tight">' + enemies + '</ul>' +
      '<p class="dim">' + esc(l.enemy_tactics_zh) + '</p>' +
      '<h4>友军</h4><p class="dim">' + esc(l.friendly_forces_zh) + '</p>' +
      '<h4>可用支援</h4><ul class="tight">' + sup + '</ul>' +
      '<div class="br-row"><button class="btn primary" id="detailGo">查看简报 → 装备</button></div>';
    var go = $('detailGo');
    if (go) go.onclick = function () { renderBriefing(l); };
  }

  function renderBriefing(l) {
    state.level = l; state.faction = l.faction;
    $('briefTitle').textContent = '作战简报 · ' + l.name_zh;
    $('briefMain').innerHTML =
      '<p class="role">' + esc(l.player_role_zh) + '</p>' +
      '<h4>任务目标</h4><ul class="tight">' + (l.objectives || []).map(function (o) {
        return '<li>' + (o.type === 'primary' ? '<b class="primary-tag">主</b>' : '<b class="secondary-tag">次</b>') + ' ' + esc(o.text_zh) +
          (o.time_limit_s ? ' <span class="dim">限时 ' + Math.round(o.time_limit_s / 60) + ' 分钟</span>' : '') + '</li>';
      }).join('') + '</ul>' +
      '<h4>历史背景</h4><p class="history">' + esc(l.historical_note_zh) + '</p>' +
      '<h4>史实与戏剧化处理</h4><ul class="tight"><li>可考证：' + esc((l.historical_accuracy.verified_events || []).join('；')) + '</li>' +
      '<li>戏剧化：' + esc((l.historical_accuracy.dramatized || []).join('；')) + '</li></ul>' +
      '<h4>设计意图</h4><p class="dim">' + esc(l.design_note_zh) + '</p>' +
      '<p class="dim">配乐情绪：' + esc(l.music_mood) + '</p>';
    $('briefSide').innerHTML =
      '<h4>战场条件</h4>' +
      '<table class="kv">' +
      '<tr><td>日期</td><td>' + esc(l.date) + '</td></tr>' +
      '<tr><td>地点</td><td>' + esc(l.location_zh) + '</td></tr>' +
      '<tr><td>地形</td><td>' + esc(terrainZh(l.terrain)) + '</td></tr>' +
      '<tr><td>天气</td><td>' + esc(weatherZh(l.weather)) + '（能见度 ' + l.visibility_m + 'm）</td></tr>' +
      '<tr><td>时间</td><td>' + esc(todZh(l.time_of_day)) + '</td></tr>' +
      '<tr><td>难度</td><td>' + l.difficulty + ' / 10</td></tr>' +
      '<tr><td>建议时长</td><td>' + Math.round(l.par_time_s / 60) + ' 分钟</td></tr>' +
      '<tr><td>负重预算</td><td>' + l.weight_budget_kg + ' kg</td></tr>' +
      '<tr><td>补给点</td><td>' + l.resupply_points + '</td></tr>' +
      '<tr><td>奖励</td><td>' + l.unlock_reward.xp + ' XP' +
      (l.unlock_reward.weapon ? ' · ' + esc(weaponName(l.unlock_reward.weapon)) : '') +
      (l.unlock_reward.class ? ' · ' + esc(className(l.unlock_reward.class)) : '') + '</td></tr>' +
      '</table>' +
      '<h4>推荐专长</h4><p>' + (l.player_class_recommended || []).map(function (c) { return '<span class="chip">' + esc(className(c)) + '</span>'; }).join('') + '</p>' +
      '<h4>敌军构成</h4><ul class="tight">' + Object.keys(l.enemy_composition || {}).map(function (k) {
        var e = findEnemy(k); return '<li>' + esc(e ? e.name_zh : k) + ' ×' + l.enemy_composition[k] + '</li>';
      }).join('') + '</ul>' +
      '<div class="br-row"><button class="btn primary big" id="briefGo">进入装备 →</button></div>';
    $('briefGo').onclick = function () { openLoadout(l); };
    show('briefing');
  }

  function findEnemy(id) {
    var cats = ['infantry', 'vehicles', 'air'];
    for (var i = 0; i < cats.length; i++) {
      var arr = D.enemies[cats[i]] || [];
      for (var j = 0; j < arr.length; j++) if (arr[j].id === id) return arr[j];
    }
    return null;
  }
  function weaponName(id) { return D.weapons[id] ? D.weapons[id].name_zh : id; }
  function className(id) { return D.classes[id] ? D.classes[id].name_zh : id; }
  function callInName(id) {
    var arr = D.support.call_ins || [];
    for (var i = 0; i < arr.length; i++) if (arr[i].id === id) return arr[i].name_zh;
    return id;
  }
  var MISSION_ZH = { defense: '防守', delay: '迟滞', breakthrough: '突破', assault: '强攻', urban_clearing: '巷战清剿', river_crossing: '渡河', armored: '装甲战', ambush: '伏击', relief: '解围/会师', sabotage: '破袭', siege: '围城', amphibious: '两栖登陆' };
  var WEATHER_ZH = { clear: '晴', rain: '雨', snow: '雪', fog: '雾', mud: '泥泞', storm: '暴风雪' };
  var TERRAIN_ZH = { city: '城市', forest: '森林', field: '旷野', village: '村落', river: '河流', rail: '铁路', industrial: '工业区', fortress: '要塞', trench: '战壕' };
  var TOD_ZH = { dawn: '拂晓', day: '白昼', dusk: '黄昏', night: '夜间' };
  function missionTypeZh(v) { return MISSION_ZH[v] || v; }
  function weatherZh(v) { return WEATHER_ZH[v] || v; }
  function terrainZh(v) { return TERRAIN_ZH[v] || v; }
  function todZh(v) { return TOD_ZH[v] || v; }

  /* ------------------------------- 装备与负重 ------------------------------- */
  function eqByCategory(cat, faction) {
    var out = [];
    Object.keys(D.equipment).forEach(function (k) {
      var it = D.equipment[k];
      if (it.category !== cat) return;
      if (it.faction !== 'both' && it.faction !== faction && it.faction !== 'captured') return;
      out.push(it);
    });
    return out;
  }
  function weaponList(faction, cat) {
    var out = [];
    Object.keys(D.weapons).forEach(function (k) {
      var w = D.weapons[k];
      if (w.faction !== faction) return;
      if (cat && w.category !== cat) return;
      out.push(w);
    });
    return out;
  }
  function capacityFor(faction) {
    var ws = D.classes.weight_system || D.classes;
    return faction === 'us' ? (ws.us_capacity_kg || 26) : (ws.base_capacity_kg || 24);
  }

  function openLoadout(level) {
    state.level = level || state.level;
    state.mode = 'campaign';
    var faction = level ? level.faction : state.faction;
    state.faction = faction;
    // 默认推荐配置
    state.loadout = RF.autoLoadout(D, faction, (level && level.player_class_recommended && level.player_class_recommended[0]) || 'rifleman',
      level ? level.weight_budget_kg : capacityFor(faction));
    state.loadout.capacityKg = level ? level.weight_budget_kg : capacityFor(faction);
    $('loadoutTitle').textContent = level ? ('装备与负重 · ' + level.name_zh) : ('装备与负重 · 无限模式');
    renderDifficulty();
    renderLoadout();
    show('loadout');
  }

  function renderDifficulty() {
    $('difficultyPicker').innerHTML = DIFFS.map(function (d) {
      var dd = D.difficulty[d];
      return '<button class="diff' + (state.difficulty === d ? ' on' : '') + '" data-diff="' + d + '" title="' + esc(dd.desc) + '">' + esc(diffName(d)) + '</button>';
    }).join('');
    $('difficultyPicker').querySelectorAll('[data-diff]').forEach(function (b) {
      b.onclick = function () { state.difficulty = b.getAttribute('data-diff'); renderDifficulty(); };
    });
  }
  var DIFF_ZH = { recruit: '新兵', regular: '标准', veteran: '老兵', historical: '史实' };
  function diffName(d) { return DIFF_ZH[d] || d; }

  function renderLoadout() {
    var lo = state.loadout, faction = state.faction;
    // 专长
    $('classRow').innerHTML = Object.keys(D.classes).filter(function (k) { return typeof D.classes[k] === 'object' && D.classes[k].id; }).map(function (k) {
      var c = D.classes[k];
      var ok = !c.factions || c.factions.indexOf(faction) >= 0;
      return '<button class="cls' + (lo.classId === c.id ? ' on' : '') + (ok ? '' : ' disabled') + '" data-cls="' + c.id + '">' +
        '<b>' + esc(c.name_zh) + '</b><span>' + c.passives.map(function (p) { return esc(p.name_zh); }).join(' / ') + '</span>' +
        '<i>主动：' + esc(c.active.name_zh) + '</i></button>';
    }).join('');
    $('classRow').querySelectorAll('[data-cls]').forEach(function (b) {
      b.onclick = function () {
        if (b.classList.contains('disabled')) return;
        lo.classId = b.getAttribute('data-cls'); renderLoadout();
      };
    });

    // 分栏
    var tabs = [
      { id: 'primary', zh: '主武器' }, { id: 'secondary', zh: '副武器' },
      { id: 'grenade', zh: '手榴弹' }, { id: 'food', zh: '食物' }, { id: 'water', zh: '饮水' },
      { id: 'gear', zh: '装备/医疗/护甲' }, { id: 'support', zh: '支援与电台' }
    ];
    $('loadoutTabs').innerHTML = tabs.map(function (t) {
      return '<button class="tab' + (state.tab === t.id ? ' on' : '') + '" data-tab="' + t.id + '">' + t.zh + '</button>';
    }).join('');
    $('loadoutTabs').querySelectorAll('[data-tab]').forEach(function (b) {
      b.onclick = function () { state.tab = b.getAttribute('data-tab'); renderLoadout(); };
    });

    var html = '';
    var slots = D.classes[lo.classId] ? D.classes[lo.classId].slots : { grenade: 2, supply: 4 };
    if (state.tab === 'primary' || state.tab === 'secondary') {
      var cats = state.tab === 'primary'
        ? ['rifle_bolt', 'rifle_semi', 'smg', 'lmg', 'sniper', 'at_rifle', 'at_rocket', 'assault_rifle', 'shotgun', 'flamethrower', 'carbine', 'demolition', 'hmg_emplacement']
        : ['sidearm', 'carbine', 'smg'];
      weaponList(faction, null).filter(function (w) { return cats.indexOf(w.category) >= 0; }).forEach(function (w) {
        var sel = (state.tab === 'primary' ? lo.primary : lo.secondary) === w.id;
        html += itemCard(w.id, w.name_zh, w.weight_kg, [
          '口径 ' + w.caliber, '伤害 ' + w.damage, '射速 ' + w.rpm + ' rpm', '弹匣 ' + w.magazine,
          '换弹 ' + w.reload_s + 's', '有效 ' + w.effective_range_m + 'm', '穿深 ' + w.penetration_mm + 'mm'
        ], w.realism, sel, state.tab, w.unlock_level);
      });
      // 缴获武器（高解锁等级时可用）
      weaponList('captured', null).forEach(function (w) {
        var sel = (state.tab === 'primary' ? lo.primary : lo.secondary) === w.id;
        html += itemCard(w.id, w.name_zh + '（缴获）', w.weight_kg, [
          '口径 ' + w.caliber, '伤害 ' + w.damage, '弹匣 ' + w.magazine, '弹药稀缺'
        ], w.realism, sel, state.tab, w.unlock_level);
      });
    } else if (state.tab === 'grenade') {
      eqByCategory('grenade', faction).forEach(function (it) {
        var n = (lo.grenades[it.id] || 0);
        html += itemCard(it.id, it.name_zh, it.weight_kg, [
          '伤害 ' + (it.effect.damage || 0), '半径 ' + (it.effect.radius_m || 0) + 'm',
          '引信 ' + (it.effect.fuse_s || 0) + 's', it.effect.penetration_mm ? ('穿深 ' + it.effect.penetration_mm + 'mm') : '',
          it.effect.smoke_duration_s ? ('烟幕 ' + it.effect.smoke_duration_s + 's') : ''
        ].filter(Boolean), it.realism, n > 0, 'grenade', 1, n);
      });
    } else if (state.tab === 'food' || state.tab === 'water') {
      eqByCategory(state.tab === 'food' ? 'food' : 'water', faction).forEach(function (it) {
        var n = (lo.supplies[it.id] || 0);
        html += itemCard(it.id, it.name_zh, it.weight_kg, [
          it.effect.calories ? ('热量 ' + it.effect.calories) : ('补水 ' + it.effect.hydration),
          '耗时 ' + (it.effect.eat_time_s || it.effect.drink_time_s) + 's',
          it.effect.morale ? ('士气 +' + it.effect.morale) : '', it.effect.debuff ? '副作用：' + it.effect.debuff : ''
        ].filter(Boolean), it.realism, n > 0, 'supply', 1, n);
      });
    } else if (state.tab === 'gear') {
      ['medical', 'tool', 'armor', 'ammo'].forEach(function (cat) {
        eqByCategory(cat, faction).forEach(function (it) {
          var n = (lo.gear[it.id] || 0);
          html += itemCard(it.id, it.name_zh, it.weight_kg, Object.keys(it.effect).map(function (k) { return k + ': ' + it.effect[k]; }),
            it.realism, n > 0, 'gear', 1, n);
        });
      });
    } else {
      html += '<div class="item card-note"><b>无线电台（必需）</b><p>没有电台就无法呼叫任何支援。电台被摧毁后 60 秒内无法呼叫。</p>' +
        '<button class="btn ' + (lo.radio ? 'primary' : '') + '" id="radioToggle">' + (lo.radio ? '已携带（' + (D.support.comms.radio_weight_kg || 2.4) + ' kg）' : '携带电台') + '</button></div>';
      var f = faction === 'us' ? 'us' : 'soviet';
      (D.support.call_ins || []).filter(function (s) { return s.faction === f || s.faction === 'both'; }).forEach(function (s) {
        var avail = !state.level || (state.level.available_support || []).indexOf(s.id) >= 0;
        html += '<div class="item' + (avail ? '' : ' off') + '"><b>' + esc(s.name_zh) + '</b>' +
          '<p class="dim">' + esc(s.realism) + '</p>' +
          '<p>指挥点 <b>' + s.cp_cost + '</b> · 弹着延迟 <b>' + s.delay_s + 's</b> · 冷却 ' + s.cooldown_s + 's' +
          (s.radius_m ? ' · 半径 ' + s.radius_m + 'm' : '') + (s.rounds ? ' · ' + s.rounds + ' 发' : '') + '</p>' +
          '<p class="' + (avail ? 'ok' : 'warn') + '">' + (avail ? '本关可用' : '本关不可用（无授权/无观察条件）') + '</p></div>';
      });
      setTimeout(function () {
        var r = $('radioToggle');
        if (r) r.onclick = function () { lo.radio = !lo.radio; renderLoadout(); };
      }, 0);
    }
    $('itemList').innerHTML = html;

    $('itemList').querySelectorAll('[data-item]').forEach(function (el) {
      el.onclick = function (ev) {
        var id = el.getAttribute('data-item'), bk = el.getAttribute('data-bucket');
        if (ev.target.getAttribute && ev.target.getAttribute('data-delta')) {
          var d = parseInt(ev.target.getAttribute('data-delta'), 10);
          adjust(bk, id, d, slots);
          // 拦掉冒泡带来的默认切换
          ev.stopPropagation();
          renderLoadout(); return;
        }
        if (bk === 'primary') lo.primary = (lo.primary === id ? null : id);
        else if (bk === 'secondary') lo.secondary = (lo.secondary === id ? null : id);
        else adjust(bk, id, (bk === 'grenade' || bk === 'supply') ? 1 : 1, slots);
        renderLoadout();
      };
    });

    renderWeight();
  }

  function adjust(bucket, id, delta, slots) {
    var lo = state.loadout;
    var map = bucket === 'grenade' ? lo.grenades : bucket === 'supply' ? lo.supplies : lo.gear;
    var cur = map[id] || 0;
    var maxItems = bucket === 'grenade' ? 2 : (bucket === 'supply' ? (slots.supply || 4) : 3);
    if (delta > 0) {
      var total = Object.keys(map).reduce(function (a, k) { return a + map[k]; }, 0);
      if (cur >= maxItems) { flashHint('该物品已达上限（' + maxItems + '）'); return; }
      if (total >= (bucket === 'grenade' ? (slots.grenade || 2) * 2 : (bucket === 'supply' ? (slots.supply || 4) : 5))) {
        flashHint('槽位已满，请先卸下其它装备'); return;
      }
      map[id] = cur + 1;
    } else {
      if (cur <= 1) delete map[id]; else map[id] = cur - 1;
    }
  }

  var hintTimer = null;
  function flashHint(msg) {
    var el = $('loadoutHint');
    el.textContent = msg;
    el.classList.add('warn');
    clearTimeout(hintTimer);
    hintTimer = setTimeout(function () { el.textContent = ''; el.classList.remove('warn'); }, 2500);
  }

  function itemCard(id, name, kg, facts, realism, selected, bucket, level, count) {
    var cnt = count == null ? 0 : count;
    return '<div class="item' + (selected ? ' on' : '') + '" data-item="' + id + '" data-bucket="' + bucket + '">' +
      '<div class="it-head"><b>' + esc(name) + '</b><span class="kg">' + kg + ' kg</span></div>' +
      '<div class="it-facts">' + facts.filter(Boolean).map(function (f) { return '<i>' + esc(f) + '</i>'; }).join('') + '</div>' +
      (realism ? '<p class="dim">' + esc(realism) + '</p>' : '') +
      (bucket === 'grenade' || bucket === 'supply' || bucket === 'gear'
        ? '<div class="it-count"><button data-delta="-1">−</button><span>' + cnt + '</span><button data-delta="1">＋</button></div>' : '') +
      '</div>';
  }

  function renderWeight() {
    var lo = state.loadout;
    var c = RF.computeLoadout(lo, D);
    $('weightTotal').textContent = c.totalKg.toFixed(1) + ' kg';
    var ratio = c.totalKg / lo.capacityKg;
    $('weightFill').style.width = Math.min(100, ratio * 100) + '%';
    $('weightFill').className = ratio > 1 ? 'over' : (ratio > 0.85 ? 'heavy' : 'ok');
    $('weightBand').textContent = c.band.label + '（×' + c.band.speedMult.toFixed(2) + ' 移速，体力消耗 ×' + c.band.staminaDrain.toFixed(2) + '）';
    $('weightMeta').innerHTML = '上限 ' + lo.capacityKg + ' kg · 占比 ' + Math.round(ratio * 100) + '%' +
      (ratio > 1 ? ' <b class="warn">已超载：无法冲刺，起身更慢</b>' : '');
    $('weightLines').innerHTML = c.lines.map(function (l) {
      return '<li>' + esc(l.label) + ' <span>' + l.kg.toFixed(1) + ' kg</span></li>';
    }).join('');
    var cls = D.classes[lo.classId];
    $('selCard').innerHTML =
      '<h4>' + esc(cls.name_zh) + '</h4>' +
      '<p class="dim">槽位：手榴弹 ' + cls.slots.grenade + ' · 补给 ' + cls.slots.supply + '</p>' +
      '<ul class="tight">' + cls.passives.map(function (p) { return '<li><b>' + esc(p.name_zh) + '</b>：' + esc(p.effect) + '</li>'; }).join('') + '</ul>' +
      '<p class="dim">主动技能 <b>' + esc(cls.active.name_zh) + '</b>（' + cls.active.cooldown_s + 's）：' + esc(cls.active.effect) + '</p>' +
      '<p class="dim">主武器：' + esc(lo.primary ? D.weapons[lo.primary].name_zh : '无') +
      ' · 副武器：' + esc(lo.secondary ? D.weapons[lo.secondary].name_zh : '无') + '</p>';
  }

  $('autoBtn').onclick = function () {
    var l = state.level;
    state.loadout = RF.autoLoadout(D, state.faction,
      (l && l.player_class_recommended && l.player_class_recommended[0]) || state.loadout.classId,
      l ? l.weight_budget_kg : capacityFor(state.faction));
    state.loadout.capacityKg = l ? l.weight_budget_kg : capacityFor(state.faction);
    renderLoadout();
  };
  $('loadoutBack').onclick = function () { if (state.mode === 'endless') { show('menu'); } else renderBriefing(state.level); };
  $('deployBtn').onclick = function () { startGame(state.level, state.mode); };

  /* ------------------------------- 无限模式 ------------------------------- */
  function startEndlessSetup() {
    state.mode = 'endless';
    state.level = null;
    state.faction = 'soviet';
    state.loadout = RF.autoLoadout(D, 'soviet', 'rifleman', capacityFor('soviet'));
    state.loadout.capacityKg = capacityFor('soviet');
    $('loadoutTitle').textContent = '装备与负重 · 无限模式（永不停歇的战线）';
    renderDifficulty();
    var old = $('classRow');
    old.insertAdjacentHTML('beforebegin',
      '<div class="faction-pick" id="factionPick">' +
      '<button data-fac="soviet" class="on">苏军（24 kg 上限）</button>' +
      '<button data-fac="us">美军（26 kg 上限）</button>' +
      '</div>');
    document.querySelectorAll('#factionPick button').forEach(function (b) {
      b.onclick = function () {
        state.faction = b.getAttribute('data-fac');
        document.querySelectorAll('#factionPick button').forEach(function (x) { x.classList.remove('on'); });
        b.classList.add('on');
        state.loadout = RF.autoLoadout(D, state.faction, state.loadout.classId, capacityFor(state.faction));
        state.loadout.capacityKg = capacityFor(state.faction);
        renderLoadout();
      };
    });
    renderLoadout();
    show('loadout');
  }

  /* ------------------------------- 开始游戏 ------------------------------- */
  var canvas, ctx, mini, miniCtx, loopId = null, lastFrame = 0;

  function startGame(level, mode) {
    canvas = $('canvas'); ctx = canvas.getContext('2d');
    mini = $('minimap'); miniCtx = mini.getContext('2d');
    bindCanvasInput();
    resizeCanvas();
    state.game = new RF.Game(D, {
      level: level, loadout: state.loadout, mode: mode, difficulty: state.difficulty
    });
    RF.onLog = function (msg) { pushLog(msg); };
    state.paused = false; state.armedSupport = null;
    $('pauseOverlay').hidden = true;
    buildSupportPanel();
    pushLog('任务开始：' + (level ? level.name_zh : '无限模式'));
    show('game');
    startLoop();
  }

  function resizeCanvas() {
    var stage = canvas.parentElement;
    canvas.width = stage.clientWidth || 1280;
    canvas.height = stage.clientHeight || 720;
  }
  window.addEventListener('resize', function () { if (state.screen === 'game') resizeCanvas(); });

  function buildSupportPanel() {
    var g = state.game;
    var fac = state.faction === 'us' ? 'us' : 'soviet';
    var list = (D.support.call_ins || []).filter(function (s) {
      if (s.faction !== fac && s.faction !== 'both') return false;
      if (!g.level) return true;
      return (g.level.available_support || []).indexOf(s.id) >= 0;
    });
    $('supportPanel').innerHTML = list.map(function (s) {
      var keyTxt = s.type === 'armor' || s.type === 'armor_siege' ? 'T' : (s.type === 'air' ? 'V' : 'Q');
      return '<button class="sup" data-sup="' + s.id + '">' +
        '<span class="sup-name">' + esc(s.name_zh) + '</span>' +
        '<span class="sup-cost">' + s.cp_cost + ' CP</span>' +
        '<span class="sup-meta">' + s.delay_s + 's 弹着 · 冷却 ' + s.cooldown_s + 's</span>' +
        '<span class="sup-key">' + keyTxt + '</span>' +
        '</button>';
    }).join('') || '<p class="hint">本关无可用支援（未携带电台或不具备条件）</p>';
    $('supportPanel').querySelectorAll('[data-sup]').forEach(function (b) {
      b.onclick = function () { armSupport(b.getAttribute('data-sup')); };
    });
  }

  function armSupport(id) {
    var g = state.game;
    var chk = g.canCall(id);
    if (!chk.ok) { pushLog('无法呼叫：' + chk.why); return; }
    state.armedSupport = id;
    pushLog('已选择支援：' + chk.def.name_zh + ' — 用左键点击目标位置（右键取消）');
  }

  /* ------------------------------- 输入 ------------------------------- */
  var KEYMAP = {
    KeyW: 'up', ArrowUp: 'up', KeyS: 'down', ArrowDown: 'down',
    KeyA: 'left', ArrowLeft: 'left', KeyD: 'right', ArrowRight: 'right'
  };
  window.addEventListener('keydown', function (e) {
    if (state.screen !== 'game') {
      if (e.code === 'Escape') show('menu');
      return;
    }
    state.keys[e.code] = true;
    if (e.code === 'Escape') { togglePause(); return; }
    var g = state.game; if (!g) return;
    if (e.code === 'KeyR') g.startReload();
    if (e.code === 'Digit1') { if (g.computed.primary) { g.player.currentSlot = 'primary'; pushLog('主武器'); } }
    if (e.code === 'Digit2') { if (g.computed.secondary) { g.player.currentSlot = 'secondary'; pushLog('副武器'); } }
    if (e.code === 'KeyF') g.useSupply('food');
    if (e.code === 'KeyE') g.useSupply('water');
    if (e.code === 'KeyH') g.useMedical();
    if (e.code === 'KeyQ' || e.code === 'KeyT' || e.code === 'KeyV') {
      var list = (D.support.call_ins || []).filter(function (s) {
        if (s.faction !== (state.faction === 'us' ? 'us' : 'soviet') && s.faction !== 'both') return false;
        if (g.level) return (g.level.available_support || []).indexOf(s.id) >= 0;
        return true;
      });
      var want = e.code === 'KeyT' ? ['armor', 'armor_siege'] : (e.code === 'KeyV' ? ['air'] : ['indirect', 'area_saturation', 'obscurant']);
      var pick = list.filter(function (s) { return want.indexOf(s.type) >= 0; })[0];
      if (pick) armSupport(pick.id); else pushLog('本关没有该类支援');
    }
    if (e.code === 'Space' && g.phase === 'resupply') { g.resupplyTimer = Math.min(g.resupplyTimer, 0.05); }
    if (e.code === 'Space' && (g.phase === 'won' || g.phase === 'lost')) showResult();
  });
  window.addEventListener('keyup', function (e) { state.keys[e.code] = false; });

  // 画布输入在首次进入关卡后才接线（canvas 到那时才被赋值，避免脚本加载期空引用）
  var canvasBound = false;
  function bindCanvasInput() {
    if (canvasBound || !canvas) return;
    canvasBound = true;
    canvas.addEventListener('mousemove', function (e) {
      var r = canvas.getBoundingClientRect();
      state.mouse.x = e.clientX - r.left;
      state.mouse.y = e.clientY - r.top;
    });
    canvas.addEventListener('contextmenu', function (e) { e.preventDefault(); state.armedSupport = null; });
    canvas.addEventListener('mousedown', function (e) {
      var g = state.game; if (!g) return;
      if (e.button === 2) { state.armedSupport = null; return; }
      if (state.armedSupport) {
        var w = screenToWorld(state.mouse.x, state.mouse.y);
        g.callSupport(state.armedSupport, w.x, w.y);
        state.armedSupport = null;
        return;
      }
      state.keys.mouseDown = true;
      state.fireEdge = true;
    });
    // 触摸设备：拖动即瞄准，短按射击
    canvas.addEventListener('touchstart', function (e) {
      var r = canvas.getBoundingClientRect(), t = e.touches[0];
      if (!t) return;
      state.mouse.x = t.clientX - r.left; state.mouse.y = t.clientY - r.top;
      state.keys.mouseDown = true; state.fireEdge = true;
    }, { passive: true });
    canvas.addEventListener('touchmove', function (e) {
      var r = canvas.getBoundingClientRect(), t = e.touches[0];
      if (!t) return;
      state.mouse.x = t.clientX - r.left; state.mouse.y = t.clientY - r.top;
    }, { passive: true });
    canvas.addEventListener('touchend', function () { state.keys.mouseDown = false; });
  }
  window.addEventListener('blur', function () {
    for (var k in state.keys) state.keys[k] = false;
    state.keys.mouseDown = false;
  });
  window.addEventListener('mouseup', function () { state.keys.mouseDown = false; });

  function screenToWorld(sx, sy) {
    var g = state.game;
    return { x: (sx - canvas.width / 2) + g.camera.x, y: (sy - canvas.height / 2) + g.camera.y };
  }

  function buildInput() {
    var k = state.keys;
    var moveX = (k.KeyD || k.ArrowRight ? 1 : 0) - (k.KeyA || k.ArrowLeft ? 1 : 0);
    var moveY = (k.KeyS || k.ArrowDown ? 1 : 0) - (k.KeyW || k.ArrowUp ? 1 : 0);
    return {
      moveX: moveX, moveY: moveY,
      sprint: !!k.ShiftLeft || !!k.ShiftRight,
      crouch: !!k.ControlLeft || !!k.ControlRight,
      binocular: !!k.KeyB,
      fire: !!k.mouseDown || !!k.KeyF1,
      firePressed: true,
      cookGrenade: !!k.KeyG,
      aimWorld: screenToWorld(state.mouse.x, state.mouse.y)
    };
  }

  /* ------------------------------- 主循环 ------------------------------- */
  function startLoop() {
    if (loopId) return;
    state.running = true; lastFrame = performance.now();
    loopId = requestAnimationFrame(frame);
  }
  function stopLoop() {
    if (loopId) cancelAnimationFrame(loopId);
    loopId = null; state.running = false;
  }
  function togglePause() {
    state.paused = !state.paused;
    $('pauseOverlay').hidden = !state.paused;
    if (state.paused) {
      var p = state.game.player;
      $('pauseStats').textContent = '击杀 ' + state.game.kills + ' · 命中率 ' +
        (p.stats.shots ? Math.round(p.stats.hits / p.stats.shots * 100) : 0) + '% · 承受伤害 ' +
        Math.round(p.stats.damageTaken) + ' · 支援呼叫 ' + p.stats.supportCalls;
    }
  }
  $('resumeBtn').onclick = togglePause;
  $('restartBtn').onclick = function () { startGame(state.level, state.mode); };
  $('toLevelsBtn').onclick = function () { renderLevels(); show('levels'); };

  function frame(now) {
    var dt = Math.min(0.05, (now - lastFrame) / 1000);
    lastFrame = now;
    var g = state.game;
    if (g && !state.paused) g.step(dt, buildInput());
    if (g) {
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      RF.renderWorld(ctx, g, canvas.width, canvas.height, state.mouse);
      if (state.armedSupport) drawTargetingHint();
      if (mini) RF.renderMinimap(miniCtx, g, mini.width, mini.height);
      updateHud();
      if ((g.phase === 'won' || g.phase === 'lost') && !state.resultShown) {
        state.resultShown = true;
        setTimeout(showResult, 900);
      }
    }
    loopId = requestAnimationFrame(frame);
  }

  function drawTargetingHint() {
    var d = state.game.supportDef(state.armedSupport);
    if (!d) return;
    var w = screenToWorld(state.mouse.x, state.mouse.y);
    var sx = state.mouse.x, sy = state.mouse.y;
    var r = (d.radius_m || 20);
    var pulse = 0.5 + 0.5 * Math.sin(performance.now() / 180);
    var near = RF.util.dist(w.x, w.y, state.game.player.x, state.game.player.y);
    ctx.save();
    ctx.strokeStyle = near < r * 1.2 ? 'rgba(255,70,60,' + (0.5 + pulse * 0.5) + ')' : 'rgba(255,210,90,' + (0.5 + pulse * 0.5) + ')';
    ctx.lineWidth = 2;
    ctx.beginPath(); ctx.arc(sx, sy, r, 0, Math.PI * 2); ctx.stroke();
    ctx.beginPath(); ctx.arc(sx, sy, r * 2.4, 0, Math.PI * 2); ctx.stroke();
    ctx.fillStyle = '#ffe9a8';
    ctx.font = '13px system-ui'; ctx.textAlign = 'center';
    ctx.fillText(d.name_zh + '（' + d.delay_s + 's 弹着）', sx, sy - r - 10);
    if (near < r * 1.2) ctx.fillText('⚠ 危险距离：会误伤', sx, sy + r + 20);
    ctx.restore();
  }

  /* ------------------------------- HUD ------------------------------- */
  var hudRefs = {};
  function r(id) { return hudRefs[id] || (hudRefs[id] = $(id)); }
  function setBar(id, valId, v) {
    r(id).style.width = Math.max(0, Math.min(100, v)) + '%';
    r(valId).textContent = Math.round(v);
  }
  function updateHud() {
    var g = state.game, p = g.player;
    setBar('hpBar', 'hpVal', p.hp);
    setBar('stBar', 'stVal', p.stamina);
    setBar('foBar', 'foVal', p.food);
    setBar('waBar', 'waVal', p.water);
    setBar('wmBar', 'wmVal', p.warmth);
    r('tagWeight').textContent = '负重 ' + g.computed.totalKg.toFixed(1) + ' / ' + g.loadout.capacityKg + ' kg';
    r('tagBand').textContent = p.band.label;
    r('tagBand').className = p.band.id;
    toggle('tagCold', !!p.cold); toggle('tagHunger', !!p.starving);
    toggle('tagThirst', !!p.dehydrated); toggle('tagBleed', !!p.bleeding);

    r('cpVal').textContent = Math.floor(g.cp) + ' / ' + g.cpMax;
    r('cpBar').style.width = (g.cp / g.cpMax * 100) + '%';
    r('radioState').textContent = g.radioBroken > 0 ? ('电台受损 ' + Math.ceil(g.radioBroken) + 's') : (g.loadout.radio ? '无线电台正常' : '未携带电台');

    var w = g.currentWeapon();
    r('wpnName').textContent = w ? (w.name + (g.player.reloading > 0 ? '（换弹中 ' + p.reloading.toFixed(1) + 's）' : '')) : '赤手';
    r('wpnMag').textContent = w ? g.currentMag() : 0;
    r('wpnRes').textContent = w ? (p.ammo[w.ammo] || 0) : 0;
    var dots = '';
    if (w) for (var i = 0; i < Math.min(w.mag, 40); i++) dots += '<i class="' + (i < g.currentMag() ? 'on' : '') + '"></i>';
    r('wpnMagDots').innerHTML = dots;
    r('wpnMeta').textContent = w ? (w.cat + ' · ' + w.pen + 'mm 穿深 · 后坐 ' + Math.round(p.recoil * 100) + '%' + (p.band.id === 'overloaded' ? ' · 超载' : '')) : '';
    var th = Object.keys(p.grenades).filter(function (k) { return p.grenades[k] > 0; })
      .map(function (k) { return esc(D.equipment[k].name_zh) + ' ×' + p.grenades[k]; }).join(' · ');
    var sup = Object.keys(p.supplies).filter(function (k) { return p.supplies[k] > 0; })
      .map(function (k) { return esc(D.equipment[k].name_zh) + ' ×' + p.supplies[k]; }).join(' · ');
    r('throwables').innerHTML = (th ? '<div>投掷物：' + th + '</div>' : '') + (sup ? '<div>补给：' + sup + '</div>' : '') +
      (p.actionTimer > 0 ? '<div class="acting">' + esc(p.actionLabel || '执行中') + ' ' + p.actionTimer.toFixed(1) + 's</div>' : '');

    var objs = g.objectives.map(function (o) {
      var prog = '';
      if (o.kind === 'eliminate') prog = o.current + '/' + o.target;
      else if (o.kind === 'hold' || o.kind === 'survive') prog = Math.round(o.current) + '/' + Math.round(o.target) + 's';
      else if (o.kind === 'reach') prog = o.done ? '已抵达' : '未抵达';
      else prog = o.current + '/' + o.target;
      return '<div class="obj' + (o.done ? ' done' : '') + (o.required ? ' req' : '') + '">' +
        (o.done ? '✔ ' : '▸ ') + esc(o.text_zh) + ' <span>' + prog + '</span></div>';
    }).join('');
    r('objectives').innerHTML = objs;

    var wave = g.mode === 'endless'
      ? ('无限模式 第 ' + g.wave + ' 波' + (g.phase === 'resupply' ? ' · 整备 ' + Math.ceil(g.resupplyTimer) + 's' : ''))
      : ('第 ' + g.wave + ' / ' + g.wavesTotal + ' 波');
    r('waveBadge').textContent = wave + ' · 击杀 ' + g.kills + ' · 时间 ' + Math.floor(g.time / 60) + ':' + String(Math.floor(g.time % 60)).padStart(2, '0');

    // 支援按钮状态
    $('supportPanel').querySelectorAll('[data-sup]').forEach(function (b) {
      var id = b.getAttribute('data-sup');
      var chk = g.canCall(id);
      b.classList.toggle('ready', chk.ok);
      b.classList.toggle('armed', state.armedSupport === id);
      var cd = g.cooldowns[id] || 0;
      b.classList.toggle('cooling', cd > 0);
      b.title = chk.ok ? '点击后在地图上选择目标' : chk.why;
    });
  }
  function toggle(id, on) { var el = r(id); if (on) el.removeAttribute('hidden'); else el.setAttribute('hidden', ''); }

  var logLines = [];
  function pushLog(msg) {
    if (state.test) return;
    logLines.push(msg);
    if (logLines.length > 6) logLines.shift();
    var box = $('logBox');
    if (box) box.innerHTML = logLines.map(function (l) { return '<div>· ' + esc(l) + '</div>'; }).join('');
  }

  /* ------------------------------- 结算 ------------------------------- */
  function showResult() {
    var g = state.game; if (!g) return;
    var won = g.phase === 'won';
    $('resultTitle').textContent = won ? '✔ 作战成功' : '✖ 作战失败';
    $('resultTitle').className = won ? 'win' : 'lose';
    $('resultSub').textContent = (state.level ? state.level.name_zh : '无限模式') + ' · ' + diffName(state.difficulty) +
      ' · 用时 ' + Math.floor(g.time / 60) + ' 分 ' + Math.floor(g.time % 60) + ' 秒';
    var p = g.player;
    var acc = p.stats.shots ? Math.round(p.stats.hits / p.stats.shots * 100) : 0;
    var rows = [
      ['击杀', g.kills], ['射击', p.stats.shots], ['命中率', acc + '%'],
      ['投掷手榴弹', p.stats.grenadesThrown], ['支援呼叫', p.stats.supportCalls],
      ['承受伤害', Math.round(p.stats.damageTaken)], ['剩余生命', Math.round(p.hp)],
      ['剩余负重', g.computed.totalKg.toFixed(1) + ' kg'], ['饱食度', Math.round(p.food)],
      ['饮水', Math.round(p.water)], ['体温', Math.round(p.warmth)]
    ];
    $('resultStats').innerHTML = rows.map(function (x) { return '<div><span>' + x[0] + '</span><b>' + x[1] + '</b></div>'; }).join('');
    var history = state.level ? state.level.historical_note_zh : '无限模式没有终点：每一波敌军都比上一波更接近 1945 年的柏林。';
    $('resultHistory').innerHTML = '<h4>' + (won ? '史实注记' : '失败原因分析') + '</h4><p class="history">' + esc(history) + '</p>' +
      '<p class="dim">' + (won
        ? '提示：尝试提高难度或减少负重（轻装档位速度 +8%），体验史实档的单命与无标记规则。'
        : '提示：检查你的负重档位、是否及时进食饮水，以及是否用烟幕/炮击处理了机枪火力点。') + '</p>';
    var idx = state.level ? state.level.index : 0;
    var nextLvl = null;
    for (var i = 0; i < LEVELS.length; i++) if (LEVELS[i].index === idx + 1) nextLvl = LEVELS[i];
    $('nextLevelBtn').style.display = (won && nextLvl) ? '' : 'none';
    $('nextLevelBtn').onclick = function () { if (nextLvl) { state.level = nextLvl; openLoadout(nextLvl); } };
    $('retryBtn').onclick = function () { state.resultShown = false; startGame(state.level, state.mode); };
    $('resultLevelsBtn').onclick = function () { state.resultShown = false; renderLevels(); show('levels'); };
    state.resultShown = true;
    stopLoop();
    show('result');
  }

  /* ------------------------------- 启动 ------------------------------- */
  renderMenuMeta();
  renderLevels();
  show('menu');
})();
