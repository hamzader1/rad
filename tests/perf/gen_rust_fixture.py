# Writes a Cargo crate whose src/main.rs has about <lines> lines of varied,
# compiling Rust (structs, enums, impls, matches, closures, doc comments), so
# semantic tokens, folds and symbols look like real code. For run_perf.sh.
# Usage: python3 gen_rust_fixture.py <lines> <out_dir>
import os, sys
def unit(i):
    return f'''/// A record kept by the registry number {i}.
/// It tracks a few counters and a name.
#[derive(Debug, Clone, Default)]
pub struct Record{i} {{
    pub name: String,
    pub count: u64,
    pub values: Vec<i64>,
    flags: Option<u32>,
}}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Kind{i} {{
    Alpha,
    Beta(u32),
    Gamma {{ x: i32, y: i32 }},
}}

impl Record{i} {{
    pub fn new(name: &str) -> Self {{
        Self {{ name: name.to_string(), count: 0, values: Vec::new(), flags: None }}
    }}

    pub fn push(&mut self, v: i64) -> usize {{
        self.values.push(v);
        self.count += 1;
        self.values.len()
    }}

    pub fn total(&self) -> i64 {{
        self.values.iter().copied().filter(|v| *v > 0).map(|v| v * 2).sum()
    }}

    pub fn classify(&self, k: Kind{i}) -> String {{
        // pick a label from the kind
        match k {{
            Kind{i}::Alpha => format!("alpha-{{}}", self.name),
            Kind{i}::Beta(n) if n > 10 => format!("big beta {{}}", n),
            Kind{i}::Beta(n) => format!("beta {{}}", n),
            Kind{i}::Gamma {{ x, y }} => {{
                let d = (x * x + y * y) as f64;
                format!("gamma {{:.2}}", d.sqrt())
            }}
        }}
    }}
}}

pub fn process_{i}(items: &[Record{i}]) -> HashMap<String, i64> {{
    let mut out = HashMap::new();
    for r in items.iter().filter(|r| r.count > 0) {{
        let entry = out.entry(r.name.clone()).or_insert(0);
        *entry += r.total();
        if let Some(f) = r.flags {{
            *entry += f as i64;
        }}
    }}
    out
}}

'''

def main():
    n_lines = int(sys.argv[1]); out_dir = sys.argv[2]
    os.makedirs(os.path.join(out_dir, 'src'), exist_ok=True)
    with open(os.path.join(out_dir, 'Cargo.toml'), 'w') as f:
        f.write('[package]\nname = "perf_fixture"\nversion = "0.1.0"\nedition = "2021"\n\n[dependencies]\n')
    parts = ['#![allow(dead_code)]\nuse std::collections::HashMap;\n\n']
    lines = 3; i = 0
    while lines < n_lines - 10:
        u = unit(i); parts.append(u); lines += u.count('\n'); i += 1
    parts.append('fn main() {\n    let mut r = Record0::new("a");\n    r.push(1);\n    println!("{}", r.total());\n}\n')
    with open(os.path.join(out_dir, 'src', 'main.rs'), 'w') as f:
        f.write(''.join(parts))
    print(lines, i)

main()
