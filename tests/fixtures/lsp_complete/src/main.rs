struct Counter {
    count: u32,
}

impl Counter {
    fn new() -> Counter {
        Counter { count: 0 }
    }

    fn increment(&mut self) {
        self.count += 1;
    }

    fn total(&self) -> u32 {
        self.count
    }
}

fn main() {
    let mut counter = Counter::new();
    counter.increment();
    println!("{}", counter.total());

}

fn scale(value: u32, factor: u32) -> u32 { value * factor }
