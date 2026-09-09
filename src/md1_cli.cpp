#include "md1.hpp"

#include <iomanip>
#include <iostream>
#include <span>
#include <string>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: md1_cli STRING\n";
    return 2;
  }

  const std::string input(argv[1]);
  const auto bytes = std::as_bytes(std::span(input));
  const auto digest = rsa270::Md1::digest(
      {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
  for (const auto byte : digest) {
    std::cout << std::hex << std::setfill('0') << std::setw(2)
              << static_cast<unsigned>(byte);
  }
  std::cout << '\n';
}
