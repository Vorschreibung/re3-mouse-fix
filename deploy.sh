#!/usr/bin/env bash
set -euo pipefail

# Steam app 952060 is Resident Evil 3 Remake.
readonly app_id=952060
# The public DX12 build uses the RE3/TDB70 REFramework loader.
readonly reframework_url="${RE3_REFRAMEWORK_URL:-https://github.com/praydog/REFramework/releases/download/v1.5.9.1/RE3.zip}"
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
steamapps_dir="${RE3_STEAMAPPS_DIR:-${HOME}/.local/share/Steam/steamapps}"
manifest_path="${steamapps_dir}/appmanifest_${app_id}.acf"

temporary_directory=''
cleanup() {
   if [[ -n "${temporary_directory}" ]]; then
      rm -rf -- "${temporary_directory}"
   fi
}
trap cleanup EXIT

is_reframework_loader() {
   [[ -f "$1" ]] && LC_ALL=C grep -aFq 'REFramework' "$1"
}

if [[ ! -f "${manifest_path}" ]]; then
   printf 'Steam manifest not found: %s\n' "${manifest_path}" >&2
   printf 'Set RE3_STEAMAPPS_DIR to the Steam library steamapps directory.\n' >&2
   exit 1
fi

install_directory="$(awk -F '"' '$2 == "installdir" { print $4; exit }' "${manifest_path}")"
if [[ -z "${install_directory}" ]]; then
   printf 'Could not read installdir from %s\n' "${manifest_path}" >&2
   exit 1
fi

game_dir="${RE3_GAME_DIR:-${steamapps_dir}/common/${install_directory}}"
proton_prefix="${RE3_PROTON_PREFIX:-${steamapps_dir}/compatdata/${app_id}/pfx}"
game_executable="${game_dir}/re3.exe"
plugin_source="${project_dir}/build/RE3MouseFix.dll"
plugin_destination="${game_dir}/reframework/plugins/RE3MouseFix.dll"
configuration_source="${project_dir}/reframework/data/RE3MouseFix.ini"
configuration_destination="${game_dir}/reframework/data/RE3MouseFix.ini"
loader_destination="${game_dir}/dinput8.dll"

if [[ ! -f "${game_executable}" ]]; then
   printf 'Game executable not found: %s\n' "${game_executable}" >&2
   exit 1
fi

if [[ ! -f "${proton_prefix}/system.reg" || ! -d "${proton_prefix}/drive_c" ]]; then
   printf 'Proton prefix not initialized: %s\n' "${proton_prefix}" >&2
   printf 'Start the game through Steam once, then deploy again.\n' >&2
   exit 1
fi

printf 'Building RE3MouseFix for x64...\n'
"${project_dir}/build.sh"

if [[ ! -f "${plugin_source}" ]]; then
   printf 'Build did not produce %s\n' "${plugin_source}" >&2
   exit 1
fi

install -D -m 0644 "${plugin_source}" "${plugin_destination}"
install -D -m 0644 "${configuration_source}" "${configuration_destination}"
printf 'Deployed: %s\n' "${plugin_destination}"
printf 'Deployed: %s\n' "${configuration_destination}"
printf 'Proton prefix: %s\n' "${proton_prefix}"

if is_reframework_loader "${loader_destination}"; then
   printf 'REFramework already installed: %s\n' "${loader_destination}"
else
   for required_command in curl unzip; do
      if ! command -v "${required_command}" >/dev/null 2>&1; then
         printf 'Required command not found: %s\n' "${required_command}" >&2
         exit 1
      fi
   done

   if [[ -e "${loader_destination}" ]]; then
      printf 'Refusing to replace non-REFramework DLL: %s\n' "${loader_destination}" >&2
      exit 1
   fi

   temporary_directory="$(mktemp -d "${TMPDIR:-/tmp}/re3-reframework.XXXXXX")"
   loader_archive="${temporary_directory}/RE3.zip"
   loader_extract_directory="${temporary_directory}/extracted"
   loader_source="${loader_extract_directory}/dinput8.dll"

   printf 'Downloading REFramework for Resident Evil 3...\n'
   curl --fail --location --retry 3 --silent --show-error \
      --output "${loader_archive}" "${reframework_url}"
   mkdir -p "${loader_extract_directory}"
   unzip -q "${loader_archive}" dinput8.dll -d "${loader_extract_directory}"

   if ! is_reframework_loader "${loader_source}"; then
      printf 'Downloaded archive does not contain a valid REFramework loader.\n' >&2
      exit 1
   fi

   install -m 0644 "${loader_source}" "${loader_destination}"
   printf 'Installed REFramework: %s\n' "${loader_destination}"
fi

printf 'Steam launch option: WINEDLLOVERRIDES="dinput8.dll=n,b" %%command%%\n'
