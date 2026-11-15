pragma Singleton
import QtQuick
import Quickshell
import Quickshell.Io

// Live compositor state and output/mode geometry from dwl via `quattro-dwl-ipc`.
// Replaces compositor-specific IPC singletons across the shell without needing wlr-randr.
Singleton {
  id: root

  signal stateUpdated()
  signal outputNamesChanged()

  property int tagCount: 9
  property var layouts: ["[]=", "><>", "[]=", "[M]"]
  property var outputs: []
  property string focusedOutput: ""
  property var monitorModes: ({})
  property int restartDelayMs: 500
  property bool connected: false

  readonly property var focusedOutputState: outputState(focusedOutput)
  readonly property var activeWindow: {
    var o = focusedOutputState
    if (!o) {
      return {
        output: "",
        title: "",
        appId: "",
        fullscreen: false,
        floating: false,
        geometry: { x: 0, y: 0, width: 0, height: 0 },
      }
    }
    return {
      output: o.name || "",
      title: o.title || "",
      appId: o.appId || "",
      fullscreen: o.fullscreen === true,
      floating: o.floating === true,
      geometry: o.geometry || { x: 0, y: 0, width: 0, height: 0 },
    }
  }

  readonly property bool focusedFullscreen: !!(focusedOutputState && focusedOutputState.fullscreen)
  readonly property bool focusedFloatingLayout: {
    var o = focusedOutputState
    if (!o) return false
    return o.layoutSymbol === "><>" || Number(o.layoutIndex) === 1
  }

  function outputState(name) {
    var list = outputs || []
    if (!name) return list.length > 0 ? list[0] : null
    for (var i = 0; i < list.length; i++) {
      if (list[i] && list[i].name === name) return list[i]
    }
    return null
  }

  function isFullscreenOn(screenName) {
    var o = outputState(screenName)
    return !!(o && o.fullscreen)
  }

  function isFloatingLayoutOn(screenName) {
    var o = outputState(screenName)
    if (!o) return false
    return o.layoutSymbol === "><>" || Number(o.layoutIndex) === 1
  }

  function screenForName(name) {
    var screens = Quickshell.screens || []
    for (var i = 0; i < screens.length; i++) {
      if (screens[i] && screens[i].name === name) return screens[i]
    }
    return null
  }

  function focusedScreen() {
    var s = screenForName(focusedOutput)
    if (s) return s
    var screens = Quickshell.screens || []
    return screens.length > 0 ? screens[0] : null
  }

  function activeTagIndex(name) {
    var o = outputState(name)
    if (!o || !o.tags) return 1
    for (var i = 0; i < o.tags.length; i++) {
      if (o.tags[i].active) return o.tags[i].id
    }
    return 1
  }

  function viewTag(outputName, tagIndex1Based, toggle) {
    var idx = Math.max(1, Math.min(tagCount, Math.round(Number(tagIndex1Based) || 1)))
    var target = outputName || "focused"
    Quickshell.execDetached(["quattro-dwl-ipc", "view-tag", target, String(idx), toggle ? "1" : "0"])
  }

  function setTags(outputName, mask, toggle) {
    var target = outputName || "focused"
    Quickshell.execDetached(["quattro-dwl-ipc", "set-tags", target, String(mask >>> 0), toggle ? "1" : "0"])
  }

  function setLayout(outputName, index) {
    var target = outputName || "focused"
    Quickshell.execDetached(["quattro-dwl-ipc", "set-layout", target, String(Math.round(Number(index) || 0))])
  }

  function cycleLayout(outputName, delta) {
    var target = outputName || "focused"
    var step = delta === undefined ? 1 : delta
    Quickshell.execDetached(["quattro-dwl-ipc", "cycle-layout", target, String(step)])
  }

  function refreshMonitors() {
    if (!randrProc.running) randrProc.running = true
  }

  function applySnapshot(raw) {
    var line = String(raw || "").trim()
    if (!line) return
    try {
      var data = JSON.parse(line)
      if (!data || typeof data !== "object") return
      var prevNames = (root.outputs || []).map(function(o) { return o.name }).join(",")
      if (typeof data.tagCount === "number" && data.tagCount > 0) root.tagCount = data.tagCount
      if (Array.isArray(data.layouts) && data.layouts.length > 0) root.layouts = data.layouts
      if (Array.isArray(data.outputs)) root.outputs = data.outputs
      if (typeof data.focusedOutput === "string") root.focusedOutput = data.focusedOutput
      root.connected = true
      root.restartDelayMs = 500
      var nextNames = (root.outputs || []).map(function(o) { return o.name }).join(",")
      if (prevNames !== nextNames) {
        root.outputNamesChanged()
        root.refreshMonitors()
      }
      root.stateUpdated()
    } catch (e) {
      // Ignore partial or malformed lines
    }
  }

  Process {
    id: ipcWatcher
    command: ["quattro-dwl-ipc", "--watch"]
    running: true
    stdout: SplitParser {
      onRead: function(line) {
        root.applySnapshot(line)
      }
    }
    onExited: function() {
      root.connected = false
      restartTimer.interval = root.restartDelayMs
      root.restartDelayMs = Math.min(10000, root.restartDelayMs * 2)
      restartTimer.start()
    }
  }

  Timer {
    id: restartTimer
    repeat: false
    onTriggered: ipcWatcher.running = true
  }

  Process {
    id: randrProc
    command: ["quattro-dwl-ipc", "--monitors"]
    running: false
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        try {
          var arr = JSON.parse(text || "[]")
          if (!Array.isArray(arr)) return
          var map = {}
          for (var i = 0; i < arr.length; i++) {
            var item = arr[i]
            if (!item || !item.name) continue
            var cur = null
            var modes = Array.isArray(item.modes) ? item.modes : []
            for (var j = 0; j < modes.length; j++) {
              if (modes[j] && modes[j].current) {
                cur = modes[j]
                break
              }
            }
            if (!cur && modes.length > 0) cur = modes[0]
            map[item.name] = {
              name: item.name,
              description: item.description || "",
              make: item.make || "",
              model: item.model || "",
              enabled: item.enabled !== false,
              x: item.position ? Number(item.position.x || 0) : 0,
              y: item.position ? Number(item.position.y || 0) : 0,
              width: cur ? Number(cur.width || 0) : 0,
              height: cur ? Number(cur.height || 0) : 0,
              refresh: cur ? Number(cur.refresh || 0) : 0,
              scale: Number(item.scale || 1),
              transform: item.transform || "normal",
              modes: modes,
            }
          }
          root.monitorModes = map
        } catch (e) {
          // quattro-dwl-ipc --monitors unavailable
        }
      }
    }
  }

  Component.onCompleted: refreshMonitors()
}
