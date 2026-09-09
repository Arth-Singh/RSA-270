#include "md1.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Binary-safe adapter to the preserved production implementation.
int main() {
  std::string hex;
  while (std::getline(std::cin, hex)) {
    if (hex.size() % 2) throw std::runtime_error("odd hex length");
    std::vector<std::uint8_t> input;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
      const std::string pair = hex.substr(i, 2);
      std::size_t used = 0;
      const auto value = std::stoul(pair, &used, 16);
      if (used != 2) throw std::runtime_error("invalid hex");
      input.push_back(static_cast<std::uint8_t>(value));
    }
    for (auto byte : rsa270::Md1::digest(input)) {
      std::cout << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<unsigned>(byte);
    }
    std::cout << '\n';
  }
}
