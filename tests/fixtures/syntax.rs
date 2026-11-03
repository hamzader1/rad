//! Highlighting fixture: one construct per line.
#![allow(dead_code)]

#[derive(Debug, Clone)]
struct Holder<'a> {
    name: &'a str,
}

fn main() {
    let c = 'x';
    let r = r#"raw "quoted" text"#;
    let ml = "first
second";
    /* outer /* inner */ outer */
    let n = 1_000u64;
    'outer: loop { break 'outer; }
}
