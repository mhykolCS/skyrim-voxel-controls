#!/usr/bin/env bash
set -euo pipefail
steam_root="${STEAM_ROOT:-$HOME/.local/share/Steam}"
game_dir="$steam_root/steamapps/common/Skyrim Special Edition"
proton_dir="${PROTON_ROOT:-$steam_root/steamapps/common/Proton - Experimental}"
[[ -f "$game_dir/skse64_loader.exe" ]] || { echo 'Install official SKSE first.' >&2; exit 1; }
[[ -f "$proton_dir/proton" ]] || { echo 'Set PROTON_ROOT to an installed Proton directory.' >&2; exit 1; }
pgrep -x steam >/dev/null || { echo 'Start Steam and sign in before launching Skyrim.' >&2; exit 1; }
if command -v hyprctl >/dev/null && [[ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]]; then
    # Skyrim calculates its viewport at startup and cannot follow a tiled resize.
    # This temporary rule applies only to Skyrim and is recreated after a reload.
    hyprctl eval 'voxelSkyrimDisplay = voxelSkyrimDisplay or hl.window_rule({name="voxel-skyrim-launch",match={class="^steam_app_489830$"},float=true,fullscreen=true,monitor="DP-2"})' >/dev/null
fi
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$steam_root"
export STEAM_COMPAT_DATA_PATH="$steam_root/steamapps/compatdata/489830"
export STEAM_COMPAT_APP_ID=489830
export SteamAppId=489830
export SteamGameId=489830
cd "$game_dir"
exec "$proton_dir/proton" waitforexitandrun "$game_dir/skse64_loader.exe"
