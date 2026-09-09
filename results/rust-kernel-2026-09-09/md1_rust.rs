#![forbid(unsafe_code)]
// Safe, general-input translation of the preserved production Md1 context.
// No fixed-length specialization and no factoring or seed-search functionality.
use std::env;
use std::io::{self, BufRead};

include!("sbox.rs");

struct Md1 {
    state: [u8; 48],
    checksum: [u8; 16],
    index: u8,
    last: u8,
    finalized: bool,
}

impl Md1 {
    fn new() -> Self {
        Self { state: [0; 48], checksum: [0; 16], index: 0,
               last: 0, finalized: false }
    }

    fn update_byte(&mut self, value: u8) {
        let index = self.index as usize;
        self.state[index + 16] = value;
        self.state[index + 32] = self.state[index] ^ value;
        self.checksum[index] ^= SBOX[(value ^ self.last) as usize];
        self.last = self.checksum[index];
        self.index = (self.index + 1) & 0x0f;
        if self.index != 0 { return; }

        let mut t = 0u8;
        for _ in 0..18 {
            for position in 0..48 {
                let countdown = (48 - position) as u8;
                t = self.state[position] ^ SBOX[t.wrapping_add(countdown) as usize];
                self.state[position] = t;
            }
        }
    }

    fn update(&mut self, input: &[u8]) {
        assert!(!self.finalized, "MD1 context already finalized");
        for &value in input { self.update_byte(value); }
    }

    fn finalize(&mut self) -> [u8; 16] {
        assert!(!self.finalized, "MD1 context already finalized");
        let padding = 16 - self.index;
        for _ in 0..padding { self.update_byte(padding); }
        for i in 0..16 { self.update_byte(self.checksum[i]); }
        self.finalized = true;
        let mut result = [0u8; 16];
        result.copy_from_slice(&self.state[..16]);
        result
    }

    fn digest(input: &[u8]) -> [u8; 16] {
        let mut context = Self::new();
        context.update(input);
        context.finalize()
    }
}

fn decode(text: &str) -> Vec<u8> {
    assert!(text.len() % 2 == 0, "odd hex length");
    (0..text.len()).step_by(2)
        .map(|i| u8::from_str_radix(&text[i..i+2], 16).expect("invalid hex"))
        .collect()
}

fn hex(input: &[u8]) -> String {
    input.iter().map(|value| format!("{value:02x}")).collect()
}

fn postincrement(state: &mut [u8; 16]) {
    for i in (0..16).rev() {
        let old = state[i];
        state[i] = old.wrapping_add(1);
        if old != 0 { break; }
    }
}

fn splitmix64(state: &mut u64) -> u64 {
    *state = state.wrapping_add(0x9e3779b97f4a7c15);
    let mut z = *state;
    z = (z ^ (z >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94d049bb133111eb);
    z ^ (z >> 31)
}

fn fill_state(generator: &mut u64) -> [u8; 16] {
    let mut state = [0u8; 16];
    state[..8].copy_from_slice(&splitmix64(generator).to_le_bytes());
    state[8..].copy_from_slice(&splitmix64(generator).to_le_bytes());
    state
}

fn benchmark(mode: &str, iterations: u64, seed: u64) {
    assert!(mode == "full-state" || mode == "postincrement");
    let full_state = mode == "full-state";
    let mut generator = seed;
    let mut state = [0u8; 16];
    if !full_state {
        state = fill_state(&mut generator);
        state[14] = 0;
        state[15] = 0;
    }
    let mut fold = 0x6a09e667f3bcc909u64;
    for i in 0..iterations {
        if full_state { state = fill_state(&mut generator); }
        let digest = Md1::digest(&state);
        for value in digest { fold = fold.rotate_left(5) ^ value as u64; }
        fold ^= i;
        if !full_state { postincrement(&mut state); }
    }
    println!("{{\"mode\":\"{}\",\"iterations\":{},\"seed\":{},\"fold\":\"{:016x}\",\"final_state\":\"{}\",\"rng_state\":\"{:016x}\"}}",
             mode, iterations, seed, fold, hex(&state), generator);
}

fn main() {
    let args: Vec<String> = env::args().collect();
    match args.get(1).map(String::as_str) {
        Some("vectors") => {
            for line in io::stdin().lock().lines() {
                println!("{}", hex(&Md1::digest(&decode(&line.unwrap()))));
            }
        }
        Some("post-vectors") => {
            for line in io::stdin().lock().lines() {
                let bytes = decode(&line.unwrap());
                let mut state: [u8; 16] = bytes.try_into().expect("need16 bytes");
                postincrement(&mut state);
                println!("{}", hex(&state));
            }
        }
        Some("bench") if args.len() == 5 => benchmark(
            &args[2], args[3].parse().unwrap(), args[4].parse().unwrap()),
        _ => panic!("usage: vectors | post-vectors | bench MODE COUNT SEED"),
    }
}
