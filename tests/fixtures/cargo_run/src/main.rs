fn add(a: i32, b: i32) -> i32 {
    a + b
}

fn main() {
    println!("hello from cargo_run");
    eprintln!("a line on stderr");
    println!("2 + 3 = {}", add(2, 3));
    std::process::exit(3);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn adds() {
        assert_eq!(add(2, 3), 5);
    }

    #[test]
    fn fails() {
        assert_eq!(add(2, 2), 5);
    }
}
