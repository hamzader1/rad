#!/bin/bash
# Typing benchmarks on generated Rust crates (macOS/Linux). Not pass/fail:
# prints the MEASURE lines. Usage:
#   tests/perf/run_perf.sh [-e build/raid] [-s "3000 10000 50000"] [-l "off on"]
# For each size (lines) and LSP mode: human-paced typing (one character
# every 110 ms) and a burst (the same text 3 times at once), in a function
# body of a crate written by gen_rust_fixture.py ($TMPDIR/rad_perf/p<lines>).
# Script windows are not vsync-paced and do not take focus: input latency is
# app-side only (input injected -> its frame presented).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
exe="$here/../../build/raid"
sizes="3000 10000 50000"
modes="off on"
while getopts "e:s:l:" opt; do
  case $opt in
    e) exe="$OPTARG";;
    s) sizes="$OPTARG";;
    l) modes="$OPTARG";;
    *) exit 2;;
  esac
done
exe="$(cd "$(dirname "$exe")" && pwd)/$(basename "$exe")"
work="${TMPDIR:-/tmp}/rad_perf"
mkdir -p "$work"
for n in $sizes; do
  crate="$work/p$n"
  [ -f "$crate/src/main.rs" ] || python3 -I "$here/gen_rust_fixture.py" "$n" "$crate" > /dev/null
  for lsp in $modes; do
    name="p${n}_lsp_${lsp}"
    script="$work/$name.radtest"
    python3 -I - "$crate" "$lsp" "$script" <<'EOF'
import sys
crate, lsp, out = sys.argv[1], sys.argv[2], sys.argv[3]
txt = "let total_value = self.values.len() + 1; "
s = "#! timeout: 600\nlsp %s\nopen_folder %s\nwait_idle\nopen %s/src/main.rs\nwait_idle\n" % (lsp, crate, crate)
if lsp == "on":
    s += 'wait_until 30000 lsp_state == "ready"\nwait_until 300000 lsp_busy == 0\nwait_until 60000 semantic_count > 0\n'
s += "click code 30 9\nwait_idle\nmeasure_begin human\n"
for c in txt:
    s += 'text "%s"\nwait 110\n' % c.replace('"', '\\"')
s += "measure_end human\nmeasure_begin burst\n"
s += 'text "%s"\n' % (txt * 3)
s += "wait_idle\nmeasure_end burst\nkey ctrl+z 4\nquit\n"
open(out, "w").write(s)
EOF
    "$exe" "--script:$script" "--script_out:$work/$name.txt" "--logs:$work/logs_$name" > /dev/null 2>&1
    echo "== $name"
    grep -E "MEASURE|FAIL|ERROR" "$work/$name.txt"
  done
done
