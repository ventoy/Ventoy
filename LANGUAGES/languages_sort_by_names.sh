#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  sort_languages.sh [--case-sensitive] [--no-backup] [path/to/languages.json]

Behavior:
  - sort by .name (case-insensitive by default)
  - writes back to the same file
  - creates <file>.bak unless --no-backup
USAGE
}

CASE_INSENSITIVE=1
MAKE_BACKUP=1
FILE=""

while (( $# )); do
  case "$1" in
    --case-sensitive) CASE_INSENSITIVE=0; shift ;;
    --no-backup)      MAKE_BACKUP=0; shift ;;
    -h|--help)        usage; exit 0 ;;
    *)                FILE="${1}"; shift ;;
  esac
done
: "${FILE:=languages.json}"


if [[ -z "${FILE}" ]]; then
  if [[ $# -gt 0 ]]; then FILE="$1"; else FILE="languages.json"; fi
fi
[[ -f "$FILE" ]] || { echo "ERROR: File not found: $FILE" >&2; exit 1; }

TMP="$(mktemp "${FILE##*/}.XXXXXX")"
trap 'rm -f "$TMP"' EXIT

jq -e 'type=="array" and all(.[]; (type=="object") and (.name|type=="string"))' "$FILE" >/dev/null \
  || { echo "ERROR: Array must contain only objects with string .name" >&2; exit 1; }

if (( CASE_INSENSITIVE )); then
  jq 'def sk: (.name | ascii_downcase); sort_by(sk)' "$FILE" > "$TMP"
else
  jq 'def sk: (.name); sort_by(sk)' "$FILE" > "$TMP"
fi

if (( MAKE_BACKUP )); then
  cp -p "$FILE" "$FILE.bak"
fi

mv "$TMP" "$FILE"
trap - EXIT
echo "Sorted and wrote: $FILE"
if (( MAKE_BACKUP )); then
  echo "Backup created: $FILE.bak"
fi
