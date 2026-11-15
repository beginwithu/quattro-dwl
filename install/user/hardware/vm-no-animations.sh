# A VM usually renders on the CPU, where animations and transparency cost
# every frame, so start it without them and with opaque windows.
# quattro-toggle-animations brings them back. This runs before any dwl session
# exists, so place the flag directly.
if quattro-hw-vm; then
  echo "Detected a virtual machine. Turning off animations."
  mkdir -p "$HOME/.local/state/quattro/toggles/dwl"
  : >"$HOME/.local/state/quattro/toggles/dwl/no-animations"
fi
