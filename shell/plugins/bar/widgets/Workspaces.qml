import QtQuick
import QtQuick.Layouts
import Quickshell
import qs.Commons
import qs.Ui

BarWidget {
  id: root
  moduleName: "quattro.workspaces"

  readonly property string monitorName: {
    var window = root.QsWindow.window
    var screen = window ? window.screen : null
    return screen && screen.name ? String(screen.name) : String(DwlState.focusedOutput || "")
  }

  readonly property var outputState: DwlState.outputState(monitorName)
  readonly property int maxTags: DwlState.tagCount > 0 ? DwlState.tagCount : 9

  function modeGlyph(symbol) {
    if (symbol === "><>") return "󰔶"
    if (symbol === "[M]") return "󰝥"
    return "󱓻"
  }

  function tagEntries() {
    var o = root.outputState
    var rawTags = o && Array.isArray(o.tags) ? o.tags : []
    var entries = []
    for (var i = 0; i < root.maxTags; i++) {
      var id = i + 1
      var t = i < rawTags.length ? rawTags[i] : null
      var active = !!(t && t.active)
      var urgent = !!(t && t.urgent)
      var clients = t ? Number(t.clients || 0) : 0
      var occupied = clients > 0
      if (id <= 5 || active || urgent || occupied) {
        entries.push({
          id: id,
          active: active,
          urgent: urgent,
          occupied: occupied,
          focused: !!(t && t.focused),
        })
      }
    }
    return entries
  }

  function activeTagId() {
    var o = root.outputState
    if (!o || !Array.isArray(o.tags)) return 1
    for (var i = 0; i < o.tags.length; i++) {
      if (o.tags[i].active) return o.tags[i].id
    }
    return 1
  }

  function focusTag(id, toggle) {
    DwlState.viewTag(root.monitorName, id, toggle === true)
  }

  function cycleLayout() {
    DwlState.cycleLayout(root.monitorName, 1)
  }

  // A wheel notch moves one entry along the row, and a touchpad adds up its
  // smaller deltas until they make a notch's worth.
  property real wheelAccumulator: 0

  function scrollWorkspaces(delta) {
    var wheel = Util.wheelSteps(root.wheelAccumulator, delta)
    root.wheelAccumulator = wheel.remainder
    if (wheel.steps === 0) return

    var entries = root.tagEntries()
    if (entries.length === 0) return
    var current = root.activeTagId()
    var index = -1
    for (var i = 0; i < entries.length; i++) {
      if (entries[i].id === current) {
        index = i
        break
      }
    }
    if (index === -1) return
    var target = entries[Util.clamp(index - wheel.steps, 0, entries.length - 1)]
    if (target && target.id !== current) root.focusTag(target.id, false)
  }

  ShellIpc {
    target: "quattro.workspaces"

    function refresh(): void { DwlState.refreshMonitors() }
  }

  readonly property real trailingGap: root.vertical ? 0 : Style.spaceReal(1.5)

  implicitWidth: grid.implicitWidth + trailingGap
  implicitHeight: grid.implicitHeight

  GridLayout {
    id: grid
    anchors.fill: parent
    anchors.rightMargin: root.trailingGap
    columns: root.vertical ? 1 : root.tagEntries().length
    columnSpacing: root.vertical ? 0 : Style.space(1)
    rowSpacing: root.vertical ? Style.space(2) : 0

    Repeater {
      model: root.tagEntries()

      WidgetButton {
        required property var modelData

        readonly property bool occupied: modelData.occupied
        readonly property bool shown: modelData.active
        readonly property string layoutSymbol: root.outputState ? String(root.outputState.layoutSymbol || "[]=") : "[]="

        bar: root.bar
        text: shown ? root.modeGlyph(layoutSymbol) : String(modelData.id)
        opacity: occupied || shown || modelData.urgent ? 1 : 0.5
        horizontalMargin: 6
        verticalPadding: 6
        fixedWidth: root.vertical ? root.barSize : Style.space(20)
        fixedHeight: root.barSize
        onPressed: function(mouseButton) {
          if (mouseButton === Qt.RightButton) root.focusTag(modelData.id, true)
          else if (shown) root.cycleLayout()
          else root.focusTag(modelData.id, false)
        }
        onWheelMoved: function(delta) { root.scrollWorkspaces(delta) }
      }
    }
  }
}
