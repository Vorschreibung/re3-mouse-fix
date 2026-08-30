#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$project_dir"

loader="${1:-/tmp/reframework-v1.5.9.1/dinput8.dll}"
if [[ ! -f "$loader" ]]; then
  echo "REFramework loader not found: $loader" >&2
  exit 1
fi

if [[ ! -f build/RE2MouseFix.dll ]]; then
  echo "Build output not found; run ./build-lmsvc.sh first." >&2
  exit 1
fi

mkdir -p package/reframework/plugins package/reframework/data package/licenses dist
cp "$loader" package/dinput8.dll
cp build/RE2MouseFix.dll package/reframework/plugins/RE2MouseFix.dll
cp reframework/data/RE2MouseFix.ini package/reframework/data/RE2MouseFix.ini
cp LICENSE.txt package/LICENSE-RE2MouseFix.txt
cp THIRD_PARTY_NOTICES.md package/THIRD_PARTY_NOTICES.md
cp README.md package/README-RE2MouseFix.md
cp third_party/licenses/REFramework-LICENSE.txt package/licenses/REFramework-LICENSE.txt
cp third_party/licenses/REFix-LICENSE.txt package/licenses/REFix-LICENSE.txt

(cd package && zip -9 -r ../dist/RE2MouseFix-v1.0.0-build11636119.zip .)
zip -9 -r dist/RE2MouseFix-v1.0.0-source.zip \
  src third_party reframework CMakeLists.txt build-lmsvc.sh build-msvc.cmd \
  package.sh README.md LICENSE.txt THIRD_PARTY_NOTICES.md
