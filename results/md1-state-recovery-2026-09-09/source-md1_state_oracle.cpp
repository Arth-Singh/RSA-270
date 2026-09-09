// Production digest oracle for public, synthetic state-recovery fixtures.
#include "../src/md1.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<std::uint8_t> ParseHex(const std::string& input) {
  if (input.size() % 2) throw std::runtime_error("odd hex length");
  std::vector<std::uint8_t> bytes;
  for (std::size_t at = 0; at < input.size(); at += 2) {
    const auto part = input.substr(at, 2);
    std::size_t used = 0;
    const auto value = std::stoul(part, &used, 16);
    if (used != 2) throw std::runtime_error("invalid hexadecimal byte");
    bytes.push_back(static_cast<std::uint8_t>(value));
  }
  return bytes;
}

template <class Bytes> std::string Hex(const Bytes& bytes) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (auto byte : bytes) output << std::setw(2) << static_cast<unsigned>(byte);
  return output.str();
}

int main(int argc, char** argv) {
  try {
    if (argc != 3) throw std::runtime_error("usage: md1_state_oracle digest|pair HEX");
    auto bytes = ParseHex(argv[2]);
    const auto first = rsa270::Md1::digest(bytes);
    if (std::string(argv[1]) == "digest") {
      std::cout << Hex(first) << '\n';
      return 0;
    }
    if (std::string(argv[1]) != "pair" || bytes.size() != 16)
      throw std::runtime_error("pair requires exactly sixteen state bytes");
    // Literal preserved RSAREF postfix transition, after hashing the old state.
    for (int at = 15; at >= 0; --at)
      if (bytes[at]++) break;
    std::cout << "{\"d0\":\"" << Hex(first) << "\",\"d1\":\""
              << Hex(rsa270::Md1::digest(bytes)) << "\",\"next_state\":\""
              << Hex(bytes) << "\"}\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
}
