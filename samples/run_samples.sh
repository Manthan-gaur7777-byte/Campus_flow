#!/bin/sh
cd "$(dirname "$0")/.." || exit 1
ROOT=$(pwd)
APP="$ROOT/a.out"
if [ ! -x "$APP" ]; then
    echo "Build the program first: make"
    exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
LIMIT=""
command -v timeout >/dev/null 2>&1 && LIMIT="timeout 30"

pass=0
fail=0

for name in full_demo custom_venue edge_cases; do
    in="$ROOT/samples/${name}_input.txt"
    expected="$ROOT/samples/${name}_expected_output.txt"
    actual="$TMP/${name}_actual.txt"
    (cd "$TMP" && $LIMIT "$APP" < "$in" > "$actual" 2>&1)
    rc=$?
    if [ "${UPDATE:-0}" = "1" ]; then
        cp "$actual" "$expected"
        echo "UPDATED  $name"
        continue
    fi
    if [ $rc -eq 0 ] && diff -u "$expected" "$actual" > "$TMP/${name}.diff"; then
        echo "PASS     $name"
        pass=$((pass + 1))
    else
        echo "FAIL     $name (exit code $rc)"
        head -40 "$TMP/${name}.diff"
        fail=$((fail + 1))
    fi
done

if [ "${UPDATE:-0}" = "1" ]; then
    exit 0
fi

out=$(printf '21\n' | (cd "$TMP" && $LIMIT "$APP") 2>&1)
rc=$?
if [ $rc -eq 1 ] && echo "$out" | grep -q "Input ended"; then
    echo "PASS     input ends inside the graph menu (exits cleanly, code 1)"
    pass=$((pass + 1))
else
    echo "FAIL     input ends inside the graph menu (exit code $rc)"
    fail=$((fail + 1))
fi

out=$(printf '' | (cd "$TMP" && $LIMIT "$APP") 2>&1)
rc=$?
if [ $rc -eq 0 ]; then
    echo "PASS     empty input at the main menu (exits cleanly)"
    pass=$((pass + 1))
else
    echo "FAIL     empty input at the main menu (exit code $rc)"
    fail=$((fail + 1))
fi

echo "Samples passed: $pass   failed: $fail"
[ $fail -eq 0 ]
