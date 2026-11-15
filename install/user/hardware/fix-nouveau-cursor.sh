# Disable hardware cursors when the nouveau driver is in use.
#
# The nouveau DRM driver does not display the hardware cursor plane on many
# older NVIDIA GPUs, leaving the mouse pointer invisible under wlroots/dwl.
# Skip the fix when the proprietary driver was configured: supported GPUs can
# still use nouveau during installation before switching drivers on reboot.
nvidia_config="${QUATTRO_NVIDIA_MODPROBE_CONFIG:-/etc/modprobe.d/nvidia.conf}"

if [[ ! -f $nvidia_config ]] &&
  quattro-cmd-present lspci &&
  LC_ALL=C lspci -k | grep -qi 'Kernel driver in use: nouveau'; then
  env_dropin="$HOME/.config/uwsm/env.d/90-nouveau-cursor.conf"
  if [[ ! -f $env_dropin ]]; then
    echo "Detected nouveau driver. Forcing software cursors so the mouse pointer stays visible."
    mkdir -p "$(dirname "$env_dropin")"
    printf 'export WLR_NO_HARDWARE_CURSORS=1\n' >"$env_dropin"
  fi
fi
