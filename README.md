# quattro-dwl

Minimal [dwl](https://codeberg.org/dwl/dwl) desktop configuration and Quickshell environment adapted from Quattro to run on a standard Arch Linux install without custom ISO tooling, bloatware apps, or AI/agent integrations.

## Directory layout

- `config/` - user dotfiles (`dwl/config.toml`, `xdg-desktop-portal`, `xdg-desktop-portal-wlr`, `foot`, `alacritty`, `kitty`, `ghostty`, `btop`, `tmux`, `starship.toml`, etc.)
- `default/` - runtime defaults (bash setup, dwl IPC protocols, theme templates, systemd user units, fonts)
- `dwl/` - bundled `dwl` Wayland compositor C source (runtime TOML loader `toml_config.h`, `dwl-ipc-unstable-v2`, `SIGUSR1` live reload, floating rule sizing) built into `~/.local/share/quattro/libexec/dwl` (`~/.local/bin/dwl` & `/usr/local/bin/dwl`)
- `src/` - C source for `quattro-dwl-ipc` (`dwl-ipc-unstable-v2` bridge)
- `bin/` - `quattro-*` helper scripts
- `shell/` - Quickshell configuration (bar, dock, launcher, panels, notifications, lockscreen)
- `themes/` - 23 color schemes (`Tokyo Night`, `Catppuccin`, `Gruvbox`, `Kanagawa`, `Nord`, `Rose Pine`, etc.)
- `applications/` - core `.desktop` entries and icons
- `etc/` - system config files (`fastfetch`, `mpv`, `xkb`, `profile.d`, etc.)
- `packages/` - core Arch package list ([arch-core.packages](packages/arch-core.packages)) and [package-map.md](packages/package-map.md)
- `AGENTS.md` - guide for working on `quattro-shell` (Quickshell architecture, IPC, theming, QML conventions)
- `install.sh` - single-command installer (deploys runtime, configs, theme, and system integration)
- `verify.sh` - checks script syntax, C builds (`dwl` and `quattro-dwl-ipc`), JSON configs, pacman package names, and sandbox install

## Install

Check script syntax, build `quattro-dwl-ipc`, and verify package names without touching the system:

```bash
./verify.sh
```

Run the installer (installs lean core Arch `[core]`/`[extra]` packages, runtime, user configs, initial `Tokyo Night` theme, and system integration):

```bash
./install.sh
```

Existing user files in `~/.config` and `~/.bashrc` are overwritten directly without creating backups.

## Starting the session

Log in via SDDM (choose **Quattro (dwl)**), or start it from a TTY:

```bash
quattro-dwl-session
```

## Notes for standard Arch

- `dwl` is configured at runtime via `~/.config/dwl/config.toml` (`[appearance]`, `[keyboard]`, `[trackpad]`, `[window_rules]`, `[bindings]`, `[[monitors]]`) and reloads live on `Super + Shift + R` (`pkill -USR1 dwl`) without recompiling. Switching themes runs `quattro-theme-set-dwl` to update `rootcolor`, `bordercolor`, `focuscolor`, and `urgentcolor` in `~/.config/dwl/config.toml` and signal `dwl` live.
- `QUATTRO_PATH` resolves `~/.local/share/quattro` before `/usr/share/quattro`, so user-only installs work without root.
- Custom distro packages (`nvim`, `mise-bin`, `ttf-jetbrains-mono-nerd-basic`, `omarchy-power-profiles-daemon`, `vi`) are mapped to Arch repo names; see [packages/package-map.md](packages/package-map.md).
- Custom tools (`folio`, `omasnap`, `omacalc`, `omawrite`, `cliamp`, `qtengine`, `omarchy-screensaver`, share picker) fall back to standard Arch tools or built-in Quattro components (`nautilus`, `grim`+`slurp`, `qalculate-gtk`/`kcalc`, `libreoffice`, `cmus`/`ncmpcpp`, `qt6ct`, `quattro.screensaver`, `quattro-share-picker`).
- Wayland screensharing uses `xdg-desktop-portal-wlr` (`~/.config/xdg-desktop-portal-wlr/config`) with `chooser_type=dmenu` and `chooser_cmd=quattro-share-picker --dmenu` to present a visual Monitors/Windows chooser, with portal services restarted once `WAYLAND_DISPLAY` is imported at session start (`bin/quattro-launch-shell`) and after audio restarts (`bin/quattro-restart-audio`).
- Bootloader and ISO-specific configs (`limine`, `mkinitcpio`, `snapper`) are left out so existing Arch installs are untouched.
- Existing terminal configs (`kitty.conf` defaults) and browser flag files (`chrome-flags.conf`, `chromium-flags.conf`) are self-contained under `$HOME`.
