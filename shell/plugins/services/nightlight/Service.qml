import QtQuick
import Quickshell.Io
import "NightlightModel.js" as NightlightModel
import qs.Commons

Item {
  id: root

  // Injected by quattro-shell (the first-party service loader).
  property var shell: null

  // Keep in sync with bin/quattro-toggle-nightlight, which sets the same
  // temperatures for callers outside the shell (keybindings, menu, ssh).
  readonly property int nightTemperature: 4000
  readonly property int dayTemperature: 6500

  property bool stateLoaded: false
  property var temperature: null
  readonly property bool enabled: stateLoaded && NightlightModel.isNightlight(temperature)

  property bool hasPendingTemperature: false
  property int pendingTemperature: 0

  function refresh() {
    if (!statusProbe.running) statusProbe.running = true
  }

  function setNightlight(value) {
    applyTemperature(value ? nightTemperature : dayTemperature)
  }

  function toggle() {
    setNightlight(!enabled)
  }

  function applyTemperature(temp) {
    root.temperature = temp
    root.stateLoaded = true

    if (applyProcess.running) {
      root.pendingTemperature = temp
      root.hasPendingTemperature = true
      return
    }

    runApply(temp)
  }

  function runApply(temp) {
    var targetTemp = Math.round(Number(temp) || dayTemperature)
    var night = targetTemp < NightlightModel.IDENTITY_TEMPERATURE
    var stateFile = "$HOME/.local/state/quattro/nightlight.on"
    var script = night
      ? "mkdir -p \"$HOME/.local/state/quattro\" && printf '%s\\n' " + targetTemp + " >\"" + stateFile + "\" && pkill -x wlsunset 2>/dev/null || true; nohup wlsunset -t " + targetTemp + " -T " + (targetTemp + 1) + " -S 00:00 -s 00:01 -d 0 >/dev/null 2>&1 &"
      : "rm -f \"" + stateFile + "\" && pkill -x wlsunset 2>/dev/null || true"
    applyProcess.command = ["bash", "-c", script]
    applyProcess.running = true
  }

  Process {
    id: statusProbe
    command: ["bash", "-c", "if pgrep -x wlsunset >/dev/null 2>&1 || [[ -f \"$HOME/.local/state/quattro/nightlight.on\" ]]; then cat \"$HOME/.local/state/quattro/nightlight.on\" 2>/dev/null || echo " + nightTemperature + "; else echo " + dayTemperature + "; fi"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        root.temperature = NightlightModel.temperatureFromOutput(text)
        root.stateLoaded = true
      }
    }
    onExited: function(exitCode) {
      if (exitCode !== 0) {
        root.temperature = dayTemperature
        root.stateLoaded = true
      }
    }
  }

  Process {
    id: applyProcess
    onExited: function() {
      if (root.hasPendingTemperature) {
        root.hasPendingTemperature = false
        root.runApply(root.pendingTemperature)
        return
      }

      root.refresh()
    }
  }

  Component.onCompleted: refresh()

  ShellIpc {
    target: "nightlight"

    function status(): string {
      return JSON.stringify({ enabled: root.enabled, temperature: root.temperature })
    }

    function refresh(): void {
      root.refresh()
    }

    function enable(): string {
      root.setNightlight(true)
      return "enabled"
    }

    function disable(): string {
      root.setNightlight(false)
      return "disabled"
    }

    function toggle(): string {
      var enabling = !root.enabled
      root.setNightlight(enabling)
      return enabling ? "enabled" : "disabled"
    }
  }
}
