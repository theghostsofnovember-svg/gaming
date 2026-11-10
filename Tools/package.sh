#!/usr/bin/env bash
# Builds dist/Nawat-<version>.zip: the plugin plus the one-click Windows setup script.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
version="$(jq -r .VersionName "$root/Plugins/Nawat/Nawat.uplugin")"
name="Nawat-$version"
stage="$root/dist/$name"

rm -rf "$root/dist"
mkdir -p "$stage"
cp -R "$root/Plugins/Nawat" "$stage/Nawat"
# Ship only modules the .uplugin lists; an unlisted source folder is unfinished work.
for dir in "$stage/Nawat/Source"/*/; do
	module="$(basename "$dir")"
	if ! jq -e --arg m "$module" '.Modules[] | select(.Name == $m)' "$stage/Nawat/Nawat.uplugin" > /dev/null; then
		rm -rf "$dir"
	fi
done
cp "$root/Tools/README.txt" "$stage/README.txt"
# cmd.exe mis-parses labels in batch files with LF line endings.
sed 's/\r$//; s/$/\r/' "$root/Tools/Setup.bat" > "$stage/Setup.bat"

(cd "$root/dist" && python3 -m zipfile -c "$name.zip" "$name")
echo "$root/dist/$name.zip"
