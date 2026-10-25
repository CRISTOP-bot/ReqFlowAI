#!/usr/bin/env bash
set -euo pipefail

binary=${1:?usage: build-deb.sh <binary> <version> <output>}
version=${2:?usage: build-deb.sh <binary> <version> <output>}
output=${3:?usage: build-deb.sh <binary> <version> <output>}

if [[ ! -x "$binary" ]]; then
  echo "Executable not found or not executable: $binary" >&2
  exit 1
fi

arch=$(dpkg --print-architecture)
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
pkg="$root/reqflow-ai"
install -D -m 0755 "$binary" "$pkg/usr/bin/reqflow-ai"
install -D -m 0644 "$(dirname "$0")/reqflow-ai.desktop" "$pkg/usr/share/applications/reqflow-ai.desktop"
mkdir -p "$pkg/DEBIAN"

depends=()
while IFS= read -r library; do
  [[ -n "$library" ]] || continue
  realpath=$(readlink -f "$library")
  owner=$(dpkg-query -S "$realpath" 2>/dev/null | head -n1 | cut -d: -f1 || true)
  if [[ -z "$owner" ]]; then
    echo "Could not find Debian package owning runtime library: $realpath" >&2
    exit 1
  fi
  depends+=("$owner")
done < <(ldd "$pkg/usr/bin/reqflow-ai" | awk '$2 == "=>" {print $3} $1 ~ /^\// {print $1}')
if ldd "$pkg/usr/bin/reqflow-ai" | grep -q 'not found'; then
  echo 'Executable has unresolved shared-library dependencies' >&2
  exit 1
fi
mapfile -t depends < <(printf '%s\n' "${depends[@]}" | sort -u)
shlibs=$(IFS=,; echo "${depends[*]}")
if [[ -z "$shlibs" ]]; then
  echo 'Could not determine shared-library dependencies for the executable' >&2
  exit 1
fi
cat > "$pkg/DEBIAN/control" <<CONTROL
Package: reqflow-ai
Version: ${version}
Section: devel
Priority: optional
Architecture: ${arch}
Depends: ${shlibs}, libqt6sql6-sqlite
Maintainer: ReqFlow AI contributors
Description: Native desktop HTTP API client
 ReqFlow AI is a local-first Qt desktop application for creating and sending
 HTTP requests and managing API workflows.
CONTROL
mkdir -p "$(dirname "$output")"
dpkg-deb --root-owner-group --build "$pkg" "$output"
