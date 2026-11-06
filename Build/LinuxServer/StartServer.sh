#!/bin/bash
set -euo pipefail

# Edgegap's port mapping is named "gameport". Local Docker uses the same default.
port="${ARBITRIUM_PORT_GAMEPORT_INTERNAL:-${VANTHEX_PORT:-7777}}"
if [[ ! "$port" =~ ^[0-9]{1,5}$ ]] || (( 10#$port < 1 || 10#$port > 65535 )); then
    echo "Invalid game UDP port: $port" >&2
    exit 64
fi
cd "$(dirname "$(readlink -f "$0")")"
binary=./Vanthex/Binaries/Linux/VanthexServer
if [[ ! -x "$binary" ]]; then
    echo "Missing executable Linux dedicated server: $binary" >&2
    exit 66
fi
# Exec the ELF directly so SIGTERM reaches Unreal and its exit status is preserved.
# Do not echo deployment environment variables (they may contain credentials).
exec "$binary" /Game/Vanthex/Maps/GridV4 "-port=$port" \
    -unattended -nosound -stdout -FullStdOutLogOutput "$@"
