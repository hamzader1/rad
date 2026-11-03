use std::io::Write;
use std::thread::sleep;
use std::time::Duration;

fn main() {
    println!("started");
    std::io::stdout().flush().unwrap();
    sleep(Duration::from_millis(1500));
    println!("tick");
    std::io::stdout().flush().unwrap();
    sleep(Duration::from_secs(60));
    println!("finished");
}
