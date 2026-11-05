#!/bin/bash
# Runs every tests/scripts/*.radtest with raid's script mode (macOS/Linux port
# of run_scripts.ps1). Usage: tests/run_scripts.sh [-e build/raid] [-f filter] [-t timeout_s]
# Each script runs on a fresh copy of tests/ in $TMPDIR/rad_script_tests.
# Prints OK/FAIL per script plus failing, MEASURE and PRINT lines; the exit
# code is the number of failed scripts. Scripts never take keyboard focus.
set -u
tests="$(cd "$(dirname "$0")" && pwd)"
exe="$tests/../build/raid"
filter="*"
timeout_s=120
while getopts "e:f:t:" opt; do
  case $opt in
    e) exe="$OPTARG";;
    f) filter="$OPTARG";;
    t) timeout_s="$OPTARG";;
    *) exit 2;;
  esac
done
exe="$(cd "$(dirname "$exe")" && pwd)/$(basename "$exe")"
work="${TMPDIR:-/tmp}/rad_script_tests"
rm -rf "$work"
mkdir -p "$work"
failed=0
for script in "$tests"/scripts/$filter.radtest; do
  [ -f "$script" ] || continue
  name="$(basename "$script" .radtest)"
  dir="$work/$name"
  mkdir -p "$dir"
  cp -R "$tests/fixtures" "$dir/"
  cp -R "$tests/scripts" "$dir/"
  # `#! mac: "<text>" => "<text>"` header lines: per-OS substitutions in the
  # copy (e.g. Windows font names -> fonts every Mac has)
  if [ "$(uname -s)" = "Darwin" ]; then
    while IFS= read -r sub; do
      if [[ "$sub" =~ ^#![[:space:]]*mac:[[:space:]]*\"(.*)\"[[:space:]]*=\>[[:space:]]*\"(.*)\"$ ]]; then
        from="${BASH_REMATCH[1]}" to="${BASH_REMATCH[2]}" python3 -I -c 'import os,sys; p=sys.argv[1]; s=open(p).read(); open(p,"w").write(s.replace("\""+os.environ["from"]+"\"", "\""+os.environ["to"]+"\""))' "$dir/scripts/$name.radtest"
      fi
    done < <(grep '^#!' "$script")
  fi
  results="$dir/results.txt"
  args=("--script:$dir/scripts/$name.radtest" "--script_out:$results" "--logs:$dir/logs")
  limit=$timeout_s
  while IFS= read -r head; do
    if [[ "$head" =~ ^#![[:space:]]*args:[[:space:]]*(.*)$ ]]; then
      extra="${BASH_REMATCH[1]//\{work\}/$work}"
      eval "args+=($extra)"
    fi
    if [[ "$head" =~ ^#![[:space:]]*timeout:[[:space:]]*([0-9]+) ]]; then
      limit="${BASH_REMATCH[1]}"
    fi
  done < <(head -n 5 "$script")
  start=$(date +%s)
  "$exe" "${args[@]}" > "$dir/stdout.txt" 2>&1 &
  pid=$!
  exited=1
  for ((i = 0; i < limit*10; i++)); do
    kill -0 $pid 2>/dev/null || break
    sleep 0.1
  done
  if kill -0 $pid 2>/dev/null; then
    kill -9 $pid 2>/dev/null
    exited=0
  fi
  wait $pid 2>/dev/null
  seconds=$(( $(date +%s) - start ))
  done_line="$(grep '^DONE ' "$results" 2>/dev/null | tail -n 1)"
  if [ $exited -eq 1 ] && [[ "$done_line" =~ ^DONE\ pass\ [0-9]+\ fail\ 0\ error\ 0$ ]]; then
    status="OK  "
  else
    status="FAIL"
    failed=$((failed+1))
  fi
  summary="${done_line#DONE }"
  [ -n "$done_line" ] || summary="no DONE line"
  [ $exited -eq 1 ] || summary="$summary, did not exit within $limit s (stopped)"
  echo "$status $name ($summary, $seconds s)"
  grep -E '^(FAIL|ERROR|MEASURE|PRINT) ' "$results" 2>/dev/null | sed 's/^/     /'
done
exit $failed
