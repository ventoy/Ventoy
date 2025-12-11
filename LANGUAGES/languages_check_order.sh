#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  check_languages_order.sh [--case-sensitive] [--check-utf8] [path/to/languages.json]

Checks:
  - top-level is an array
  - every element is an object with string .name
  - sorted by .name (case-insensitive by default)

Exit codes:
  0 = OK (sorted & valid)
  1 = Not sorted or validation failed
USAGE
}

CASE_INSENSITIVE=1
CHECK_UTF8=0
FILE=""

while (( $# )); do
  case "$1" in
    --case-sensitive) CASE_INSENSITIVE=0; shift ;;
    --check-utf8)     CHECK_UTF-8=1; shift ;;
    -h|--help)        usage; exit 0 ;;
    *)                FILE="${1}"; shift ;;
  esac
done
: "${FILE:=languages.json}"

if [[ ! -f "$FILE" ]]; then
  echo "ERROR: File not found: $FILE" >&2
  exit 1
fi

if (( CHECK_UTF8 )); then
  if ! iconv -f UTF-8 -t UTF-8 "$FILE" -o /dev/null 2>/dev/null; then
    echo "ERROR: $FILE is not valid UTF-8" >&2
    exit 1
  fi
fi

jq -e 'type=="array"' "$FILE" >/dev/null || {
  echo "ERROR: Top-level JSON must be an array" >&2
  exit 1
}

if ! jq -e 'all(.[]; (type=="object") and (has("name")) and ((.name|type)=="string"))' "$FILE" >/dev/null; then
  echo "ERROR: Some elements are not objects with string .name:" >&2
  jq -r '
    to_entries
    | map(select(.value|type!="object" or (has("name")|not) or ((.value.name|type)!="string")))
    | .[:10][] | "index=\(.key) kind=\(.value|type) sample=\((.value.name // "<missing>")|tostring)"
  ' "$FILE" 2>/dev/null || true
  exit 1
fi

# sort-key
if (( CASE_INSENSITIVE )); then
  JQ_SORTKEY='def sk: (.name | ascii_downcase);'
else
  JQ_SORTKEY='def sk: (.name);'
fi

IS_SORTED=$(
  jq -e "
    $JQ_SORTKEY
    ( map(sk) as \$k | \$k == (\$k|sort) )
  " "$FILE" 2>/dev/null || echo false
)

if [[ "$IS_SORTED" != "true" ]]; then
  echo "NOT SORTED: languages.json is not in alphabetical order by .name" >&2
  # print first mismatch safely
  jq -r "
    $JQ_SORTKEY
    ( map(.name) ) as \$n
    | ( map(sk) )  as \$k
    | (\$k|sort)   as \$s
    | ( [range(0; (\$k|length))] 
        | map(select(\$k[.] != \$s[.])) 
        | .[0] ) as \$i
    | if \$i == null then
        \"(Could not compute mismatch index)\"
      else
        \"First mismatch at index \(\$i): actual=\\\"\(\$n[\$i])\\\" vs expected(order key)=\\\"\(\$s[\$i])\\\"\" 
      end
  " "$FILE"
  exit 1
fi

echo "OK: languages.json is sorted by .name"
