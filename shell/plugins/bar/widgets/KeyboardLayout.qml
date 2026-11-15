import QtQuick
import Quickshell
import Quickshell.Io
import qs.Ui
import qs.Commons
import "KeyboardLayoutModel.js" as KeyboardLayoutModel

BarWidget {
  id: root
  moduleName: "quattro.keyboard-layout"

  property string layoutFull: ""
  property string keyboardName: "seat0"
  property bool keyboardUnresolved: false
  property bool multipleLayouts: false
  property int layoutIndex: 0
  property int layoutCount: 1
  property var layoutNames: []
  property var layoutBriefs: ({})
  property bool layoutsRead: false
  property bool briefsRequested: false
  readonly property bool needsBriefs: layoutsRead && (multipleLayouts || multipleInputs)
  onNeedsBriefsChanged: loadBriefs()

  function loadBriefs() {
    if (!needsBriefs || briefsRequested) return
    briefsRequested = true
    briefsProc.running = true
  }

  readonly property string layoutLabel: KeyboardLayoutModel.shortLabel(layoutFull, layoutBriefs)
  readonly property var inputState: InputMethodState.state
  readonly property bool multipleInputs: (inputState.methods || []).length > 1
  readonly property string inputLabel: KeyboardLayoutModel.inputLabel(inputState, layoutLabel)

  function refresh() {
    if (!queryProc.running) queryProc.running = true
  }

  function cycleLayout() {
    if (multipleInputs) {
      InputMethodState.cycle()
    }
  }

  Component.onCompleted: refresh()

  // Read seat XKB layout configuration from environment, ~/.config/dwl/config.toml (or config.h), or localectl.
  Process {
    id: queryProc
    command: ["bash", "-c",
      "layouts=\"${XKB_DEFAULT_LAYOUT:-}\"; " +
      "if [[ -z $layouts && -r \"$HOME/.config/dwl/config.toml\" ]]; then " +
      "  layouts=$(sed -n 's/^[[:space:]]*xkb_layout[[:space:]]*=[[:space:]]*\"\\([^\"]*\\)\".*/\\1/p' \"$HOME/.config/dwl/config.toml\" | head -n1); " +
      "fi; " +
      "if [[ -z $layouts && -r \"$HOME/.config/dwl/config.h\" ]]; then " +
      "  layouts=$(sed -n 's/.*\\.layout[[:space:]]*=[[:space:]]*\"\\([^\"]*\\)\".*/\\1/p' \"$HOME/.config/dwl/config.h\" | head -n1); " +
      "fi; " +
      "if [[ -z $layouts ]] && command -v localectl >/dev/null 2>&1; then " +
      "  layouts=$(localectl status 2>/dev/null | sed -n 's/.*X11 Layout:[[:space:]]*//p' | head -n1); " +
      "fi; " +
      "printf '%s\\n' \"${layouts:-us}\""]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        var raw = String(text || "").trim()
        var list = raw ? raw.split(",").map(function(s) { return s.trim() }).filter(Boolean) : ["us"]
        if (list.length === 0) list = ["us"]
        root.layoutNames = list
        root.layoutCount = list.length
        root.multipleLayouts = list.length > 1
        root.layoutsRead = true
        root.keyboardUnresolved = false
        var active = list[Math.min(root.layoutIndex, list.length - 1)] || "us"
        root.layoutFull = active
      }
    }
  }

  Process {
    id: briefsProc
    command: ["xkbcli", "list", "--load-exotic"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: root.layoutBriefs = KeyboardLayoutModel.layoutBriefs(text)
    }
  }

  Connections {
    target: InputDevices
    function onChanged() { root.refresh() }
  }

  visible: inputLabel !== "" && KeyboardLayoutModel.showIndicator(layoutLabel, multipleLayouts, multipleInputs)
  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  WidgetButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: root.inputLabel
    fontSize: Style.font.caption
    horizontalMargin: 6
    tooltipText: KeyboardLayoutModel.inputTooltip(root.inputState, root.layoutFull, root.multipleLayouts)
    onPressed: function(mouseButton) {
      if (root.multipleInputs && mouseButton === Qt.LeftButton) {
        InputMethodState.cycle()
      } else if (root.multipleLayouts) {
        root.cycleLayout()
      }
    }
  }
}
