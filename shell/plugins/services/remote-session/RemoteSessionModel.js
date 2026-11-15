// Check whether a compositor event string indicates a screencast/capture transition.
function isCaptureEvent(name) {
  return String(name || "") === "screencast"
}

// Parses the probe output: one "<pid> [peer]" line per running gliff-server,
// where peer is the ssh client address when the server was spawned over ssh.
function stateFromOutput(text) {
  var lines = String(text || "").split("\n")
  var active = false
  var peers = []
  for (var i = 0; i < lines.length; i++) {
    var parts = lines[i].trim().split(/\s+/)
    if (parts[0] === "") continue
    active = true
    var peer = parts[1] || ""
    if (peer !== "" && peers.indexOf(peer) === -1) peers.push(peer)
  }
  return { active: active, peers: peers }
}

if (typeof module !== "undefined") {
  module.exports = {
    isCaptureEvent: isCaptureEvent,
    stateFromOutput: stateFromOutput
  }
}
