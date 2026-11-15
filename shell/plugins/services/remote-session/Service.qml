import QtQuick
import Quickshell
import Quickshell.Io
import "RemoteSessionModel.js" as RemoteSessionModel

// Watches for remote screen-capture sessions (such as gliff-server / wayvnc),
// so the RemoteSession bar indicator reads `active` and `peers`.
Item {
  id: root

  // Injected by quattro-shell (the first-party service loader).
  property var shell: null

  property bool stateLoaded: false
  property bool active: false
  property var peers: []

  property bool refreshPending: false

  function refresh() {
    if (statusProbe.running) {
      root.refreshPending = true
      return
    }
    root.refreshPending = false
    statusProbe.running = true
  }

  function applyProbe(text) {
    var state = RemoteSessionModel.stateFromOutput(text)
    root.peers = state.peers
    root.active = state.active
    root.stateLoaded = true
  }

  Component.onCompleted: refresh()

  // Poll periodically for active remote capture sessions.
  Timer {
    interval: root.active ? 2000 : 5000
    repeat: true
    running: true
    onTriggered: root.refresh()
  }

  Process {
    id: statusProbe
    command: ["bash", Quickshell.env("QUATTRO_PATH") + "/shell/plugins/services/remote-session/probe.sh"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: root.applyProbe(text)
    }
    onExited: function() {
      if (root.refreshPending) Qt.callLater(root.refresh)
    }
  }
}
