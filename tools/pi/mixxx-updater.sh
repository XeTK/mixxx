#!/usr/bin/env bash
#
# mixxx-updater.sh
#
# Polls the private Gitea instance for the latest Mixxx release, compares it
# against the last-installed release tag, and if a newer build exists:
#   1. shows a desktop notification (via notify-send / mako),
#   2. downloads the arm64 .deb from the release assets,
#   3. installs it with dpkg (requires passwordless sudo),
#   4. restarts Mixxx if it is running.
#
# Designed to run on the Raspberry Pi 5 (arm64) that boots Mixxx fullscreen.
#
# Configuration (environment variables, read from updater.env):
#   GITEA_URL       - base URL of the Gitea instance (default: https://gitea.xetk.co.uk)
#   GITEA_OWNER     - repo owner (default: xetk)
#   GITEA_REPO      - repo name (default: mixxx)
#   GITEA_TOKEN     - personal access token with read:release scope (required)
#   MIXXX_BIN       - path to the mixxx binary (default: /usr/bin/mixxx)
#
# The "latest" release is determined by Gitea's release ordering (created
# date). We track the last-installed release tag in a state file, so a new
# release (even with an identical-looking tag) triggers an update. This is
# robust for this repo's date-based beta tags (e.g. accessibility-beta-2026-07-07).
#
# Exit codes:
#   0 - up to date, or update applied
#   1 - configuration error (missing token)
#   2 - network/API error
#   3 - download/install error

set -euo pipefail

GITEA_URL="${GITEA_URL:-https://gitea.xetk.co.uk}"
GITEA_OWNER="${GITEA_OWNER:-xetk}"
GITEA_REPO="${GITEA_REPO:-mixxx}"
GITEA_TOKEN="${GITEA_TOKEN:-}"
MIXXX_BIN="${MIXXX_BIN:-/usr/bin/mixxx}"

STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/mixxx-updater"
STATE_FILE="$STATE_DIR/installed-release"

log() { echo "[mixxx-updater] $*"; }
die() { log "ERROR: $*"; exit "${2:-1}"; }

notify() {
    local title="$1" body="$2" urgency="${3:-normal}"
    if command -v notify-send >/dev/null 2>&1; then
        notify-send -u "$urgency" -a "Mixxx Updater" "$title" "$body" || true
    fi
    log "$title: $body"
}

# --- main ------------------------------------------------------------------

[ -n "$GITEA_TOKEN" ] || die "GITEA_TOKEN is not set" 1

mkdir -p "$STATE_DIR"

log "Checking for updates from $GITEA_URL/$GITEA_OWNER/$GITEA_REPO ..."

# Fetch the latest release.
API_URL="$GITEA_URL/api/v1/repos/$GITEA_OWNER/$GITEA_REPO/releases/latest"
HTTP_CODE="$(curl -sSL -o /tmp/mixxx-updater-response.json -w '%{http_code}' -H "Authorization: token $GITEA_TOKEN" "$API_URL" 2>/dev/null || echo 000)"
if [ "$HTTP_CODE" = "404" ]; then
    # No releases published yet - nothing to update.
    log "No releases published yet; nothing to update."
    exit 0
fi
if [ "$HTTP_CODE" != "200" ]; then
    die "Failed to query Gitea API (HTTP $HTTP_CODE, network/auth error)" 2
fi
RESPONSE="$(cat /tmp/mixxx-updater-response.json)"

LATEST_TAG="$(printf '%s' "$RESPONSE" | grep -oE '"tag_name":"[^"]*"' | head -n1 | sed 's/.*:"//;s/"//')"
if [ -z "$LATEST_TAG" ]; then
    die "Could not parse latest release tag from API response" 2
fi

# The tag we last installed (from state file), or empty if never installed.
INSTALLED_TAG="$(cat "$STATE_FILE" 2>/dev/null || true)"

log "Latest release: $LATEST_TAG"
log "Last installed: ${INSTALLED_TAG:-<none>}"

if [ -n "$INSTALLED_TAG" ] && [ "$INSTALLED_TAG" = "$LATEST_TAG" ]; then
    log "Already up to date ($LATEST_TAG)."
    exit 0
fi

# Find the arm64 .deb asset. The CPack-generated filename uses the arch
# triple "aarch64" (e.g. mixxx-...-aarch64.deb), so match both "arm64" and
# "aarch64".
DEB_URL="$(printf '%s' "$RESPONSE" | grep -oE '"browser_download_url":"[^"]*(arm64|aarch64)[^"]*\.deb"' | head -n1 | sed 's/.*:"//;s/"//')"
if [ -z "$DEB_URL" ]; then
    notify "Mixxx update available" "Release $LATEST_TAG is available but no arm64 .deb was found on it." "critical"
    die "No arm64 .deb asset found on release $LATEST_TAG" 3
fi

log "Found arm64 .deb: $DEB_URL"
notify "Mixxx update available" "Version $LATEST_TAG is available. Downloading and installing..." "normal"

TMP_DEB="$(mktemp --suffix=.deb)"
trap 'rm -f "$TMP_DEB"' EXIT

log "Downloading $DEB_URL ..."
curl -fsSL -H "Authorization: token $GITEA_TOKEN" -o "$TMP_DEB" "$DEB_URL" || die "Download failed" 3

log "Installing $TMP_DEB ..."
sudo dpkg -i "$TMP_DEB" || {
    log "dpkg failed; attempting to fix broken dependencies..."
    sudo apt-get -f install -y || die "dpkg install failed" 3
}

printf '%s' "$LATEST_TAG" > "$STATE_FILE"
log "Installed $LATEST_TAG successfully."

notify "Mixxx updated" "Version $LATEST_TAG installed successfully." "normal"

# Restart Mixxx if it is currently running.
if pgrep -x mixxx >/dev/null 2>&1; then
    log "Restarting Mixxx..."
    pkill -x mixxx || true
    sleep 1
    if command -v mixxx >/dev/null 2>&1; then
        nohup mixxx --full-screen >/dev/null 2>&1 &
    fi
fi

exit 0
