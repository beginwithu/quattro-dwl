# Package mapping

Custom Omarchy packages and how they are handled on standard Arch Linux:

| Upstream | Arch Linux handling | Notes |
|---|---|---|
| `nvim` | `neovim` (`extra`) | Package rename |
| `vi` | `vim` (`extra`) | Package rename |
| `mise-bin` | `mise` (`extra`) | Package rename |
| `ttf-jetbrains-mono-nerd-basic` | `ttf-jetbrains-mono-nerd` (`extra`) | Package rename |
| `omarchy-power-profiles-daemon` | `power-profiles-daemon` (`extra`) | Package rename |
| `folio` | `nautilus` (`extra`) | `quattro-launch-files` launches `nautilus` (with fallbacks) and syncs GTK4/Libadwaita theme colors |
| `omasnap` | `grim` + `slurp` + `wl-clipboard` + `satty` (`extra`) | `quattro-capture-screenshot` falls back to `quattro-capture-region` |
| `omacalc` | Optional (`qalculate-gtk` / `kcalc` / Python REPL) | Bound if installed, otherwise opens Python REPL |
| `omawrite` | Removed (optional) | Not installed by default |
| `cliamp` | Removed (optional) | Not installed by default |
| `herdr` | Removed (uses `tmux` from `extra`) | `SUPER + ALT + RETURN` opens `tmux` |
| `qtengine` | `qt6ct` (`extra`) | `default/uwsm/env.d/10-quattro` sets `QT_QPA_PLATFORMTHEME` dynamically |
| `share-picker` | Built-in `quattro-share-picker` (`quickshell` + `quattro-dwl-ipc`) | Provides live screen/window/workspace previews (`quattro.share-picker` overlay) |
| Idle / Lock daemons | Built-in `quattro-idle` + `quattro.lock` (`quickshell`) | `ExtSessionLockV1` lockscreen + Wayland idle service |
| Nightlight / Color picker | `wlsunset` (`extra`) + `quattro-color-picker` (`grim` + `slurp` + `imagemagick`) | Nightlight & color picking on `wlroots`/`dwl` |
| `omarchy-screensaver` | Built-in `quattro.screensaver` (`quickshell`) | `bin/quattro-launch-screensaver` launches the built-in animated ASCII branding screensaver (`quattro.screensaver`) |
| `owe` | Optional | Quickshell `BackgroundService` handles still images directly without `owe` |
| `omarchy-nvim` | `neovim` (`extra`) | User Neovim config / LazyVim works independently |
| `ttf-ia-writer` | Optional (`noto-fonts` / `adwaita-fonts` included) | Available manually or via AUR if desired |
| `limine` / `linux-omarchy` / `snapper` | Excluded | ISO/bootloader/kernel-specific packages are not forced onto an existing Arch Linux system |
