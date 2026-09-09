// Benchmark wrapper around the unchanged general production C++ digest API.
#include "md1.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<uint8_t> Decode(const std::string& text) {
  if (text.size() % 2) throw std::runtime_error("odd hex length");
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < text.size(); i += 2)
    bytes.push_back(static_cast<uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
  return bytes;
}

std::string Hex(std::span<const uint8_t> bytes) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (auto value : bytes) output << std::setw(2) << unsigned(value);
  return output.str();
}

void Postincrement(std::array<uint8_t, 16>& state) {
  for (int i = 15; i >= 0; --i) {
    const auto old = state[i];
    state[i] = static_cast<uint8_t>(old + 1);
    if (old != 0) break;
  }
}

uint64_t Splitmix64(uint64_t& state) {
  state += UINT64_C(0x9e3779b97f4a7c15);
  auto z = state;
  z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
  return z ^ (z >> 31);
}

std::array<uint8_t, 16> FillState(uint64_t& generator) {
  std::array<uint8_t, 16> state{};
  for (int half = 0; half < 2; ++half) {
    const auto word = Splitmix64(generator);
    for (int byte = 0; byte < 8; ++byte)
      state[8*half+byte] = static_cast<uint8_t>(word >> (8*byte));
  }
  return state;
}

void Benchmark(const std::string& mode, uint64_t iterations, uint64_t seed) {
  if (mode != "full-state" && mode != "postincrement")
    throw std::runtime_error("unknown workload");
  const bool full_state = mode == "full-state";
  auto generator = seed;
  std::array<uint8_t, 16> state{};
  if (!full_state) {
    state = FillState(generator);
    state[14] = state[15] = 0;
  }
  uint64_t fold = UINT64_C(0x6a09e667f3bcc909);
  for (uint64_t i = 0; i < iterations; ++i) {
    if (full_state) state = FillState(generator);
    const auto digest = rsa270::Md1::digest(state);
    for (auto value : digest) fold = std::rotl(fold, 5) ^ uint64_t(value);
    fold ^= i;
    if (!full_state) Postincrement(state);
  }
  std::cout << "{\"mode\":\"" << mode << "\",\"iterations\":" << iterations
            << ",\"seed\":" << seed << ",\"fold\":\"" << std::hex
            << std::setfill('0') << std::setw(16) << fold
            << "\",\"final_state\":\"" << Hex(state)
            << "\",\"rng_state\":\"" << std::setw(16) << generator << "\"}\n";
}

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "vectors") {
      std::string line;
      while (std::getline(std::cin, line))
        std::cout << Hex(rsa270::Md1::digest(Decode(line))) << '\n';
    } else if (argc == 2 && std::string(argv[1]) == "post-vectors") {
      std::string line;
      while (std::getline(std::cin, line)) {
        const auto bytes = Decode(line);
        if (bytes.size() != 16) throw std::runtime_error("need16 bytes");
        std::array<uint8_t, 16> state{};
        std::copy(bytes.begin(), bytes.end(), state.begin());
        Postincrement(state);
        std::cout << Hex(state) << '\n';
      }
    } else if (argc == 5 && std::string(argv[1]) == "bench") {
      Benchmark(argv[2], std::stoull(argv[3]), std::stoull(argv[4]));
    } else {
      throw std::runtime_error("usage: vectors | post-vectors | bench MODE COUNT SEED");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 2;
  }
}
