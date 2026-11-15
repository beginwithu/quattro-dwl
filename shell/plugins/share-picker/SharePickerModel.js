.pragma library

function normalizeAddress(addr) {
  var s = String(addr || "").trim().toLowerCase()
  if (s.indexOf("0x") === 0) s = s.slice(2)
  return s
}

function findScreenByName(screens, name) {
  var target = String(name || "")
  if (!screens || !target) return null
  for (var i = 0; i < screens.length; i++) {
    if (String(screens[i].name || "") === target) return screens[i]
  }
  return null
}

function findToplevel(toplevels, win) {
  if (!toplevels || !win) return null
  var title = String(win.title || "")
  var titlePrefix = title.slice(-3) === "..." ? title.slice(0, -3) : ""
  var appClass = String(win.appClass || "").toLowerCase()
  for (var j = 0; j < toplevels.length; j++) {
    var t = toplevels[j]
    var tTitle = String(t.title || "")
    var tClass = String(t.appId || "").toLowerCase()
    var titleMatch = tTitle === title || (titlePrefix.length >= 4 && tTitle.indexOf(titlePrefix) === 0)
    if (titleMatch && (!appClass || !tClass || tClass === appClass)) return t
  }
  for (var k = 0; k < toplevels.length; k++) {
    var tk = toplevels[k]
    var tkClass = String(tk.appId || "").toLowerCase()
    if (appClass && tkClass === appClass) return tk
  }
  return null
}

function buildMonitorEntries(rawOutputs, screens, dwlOutputs, dmenuMode) {
  var out = []
  var list = Array.isArray(rawOutputs) ? rawOutputs.slice() : []
  if (!dmenuMode && list.length === 0 && dwlOutputs) {
    for (var m = 0; m < dwlOutputs.length; m++) {
      var dm = dwlOutputs[m]
      var og = dm.outputGeometry || {}
      list.push({
        name: String(dm.name || ""),
        description: String(dm.name || ""),
        x: Number(og.x || 0),
        y: Number(og.y || 0),
        width: Number(og.width || 0),
        height: Number(og.height || 0)
      })
    }
  }
  if (!dmenuMode && list.length === 0 && screens) {
    for (var s = 0; s < screens.length; s++) {
      var sc = screens[s]
      list.push({
        name: String(sc.name || ""),
        description: String(sc.name || ""),
        x: Number(sc.x || 0),
        y: Number(sc.y || 0),
        width: Number(sc.width || 0),
        height: Number(sc.height || 0)
      })
    }
  }
  for (var i = 0; i < list.length; i++) {
    var o = list[i]
    if (!o || !o.name) continue
    var screen = findScreenByName(screens, o.name)
    out.push({
      name: String(o.name),
      description: String(o.description || o.name),
      x: Number(o.x || 0),
      y: Number(o.y || 0),
      width: Number(o.width || (screen ? screen.width : 0)),
      height: Number(o.height || (screen ? screen.height : 0)),
      rawLine: String(o.rawLine || ""),
      screen: screen
    })
  }
  return out
}

function buildWindowEntries(rawWindows, toplevels, dmenuMode) {
  var out = []
  var list = Array.isArray(rawWindows) ? rawWindows : []
  for (var i = 0; i < list.length; i++) {
    var w = list[i]
    if (!w || !w.id) continue
    var tl = findToplevel(toplevels, w)
    out.push({
      id: String(w.id),
      appClass: String(w.appClass || (tl ? tl.appId : "") || "window"),
      title: String(w.title || (tl ? tl.title : "") || w.appClass || w.id),
      address: String(w.address || ""),
      rawLine: String(w.rawLine || ""),
      captureSource: tl || null
    })
  }
  if (!dmenuMode && list.length === 0 && toplevels) {
    for (var j = 0; j < toplevels.length; j++) {
      var t = toplevels[j]
      if (!t || (!t.appId && !t.title)) continue
      out.push({
        id: String(j + 1),
        appClass: String(t.appId || "window"),
        title: String(t.title || t.appId || "window"),
        address: "",
        captureSource: t
      })
    }
  }
  return out
}

function filterWindows(windows, query) {
  var q = String(query || "").trim().toLowerCase()
  if (!q) return windows || []
  var out = []
  for (var i = 0; i < (windows || []).length; i++) {
    var w = windows[i]
    var hay = (w.appClass + " " + w.title).toLowerCase()
    if (hay.indexOf(q) !== -1) out.push(w)
  }
  return out
}

function buildWorkspaceEntries(rawWorkspaces, dwlOutputs, screens, toplevels) {
  var out = []
  var list = Array.isArray(rawWorkspaces) ? rawWorkspaces : []
  if (list.length === 0 && dwlOutputs) {
    for (var oi = 0; oi < dwlOutputs.length; oi++) {
      var o = dwlOutputs[oi]
      var tags = Array.isArray(o.tags) ? o.tags : []
      for (var ti = 0; ti < tags.length; ti++) {
        var tg = tags[ti]
        if (!tg.active && Number(tg.clients || 0) === 0) continue
        list.push({
          id: String(tg.id),
          name: o.name + " : tag " + tg.id,
          monitor: o.name,
          active: !!tg.active,
          clients: Number(tg.clients || 0)
        })
      }
    }
  }
  for (var i = 0; i < list.length; i++) {
    var ws = list[i]
    if (!ws) continue
    var monitorName = String(ws.monitor || "")
    var screen = findScreenByName(screens, monitorName)
    var isActiveOnScreen = !!ws.active
    var primaryCapture = isActiveOnScreen && screen ? screen : null
    out.push({
      id: String(ws.id || ""),
      name: String(ws.name || ws.id || ""),
      monitorName: monitorName,
      active: isActiveOnScreen,
      windowCount: Number(ws.clients || 0),
      windowSummary: "",
      captureSource: primaryCapture
    })
  }
  return out
}

function formatSelection(target, remember) {
  var flags = remember ? "r" : ""
  return "[SELECTION]" + flags + "/" + target
}
