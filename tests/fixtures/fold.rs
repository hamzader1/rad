// Folding fixture.
// Second comment line.
use std::fmt;
use std::io;

struct Holder<'a> {
    name: &'a str,
}

fn main() {
    let c = '{';
    let s = "}";
    if true {
        println!("{}", c);
    } else {
        println!("{}", s);
    }
}
