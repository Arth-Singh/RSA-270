#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using boost::multiprecision::cpp_int;

namespace {

constexpr std::array<std::uint8_t, 256> kPi = {
    0xd9, 0x78, 0xf9, 0xc4, 0x19, 0xdd, 0xb5, 0xed, 0x28, 0xe9, 0xfd,
    0x79, 0x4a, 0xa0, 0xd8, 0x9d, 0xc6, 0x7e, 0x37, 0x83, 0x2b, 0x76,
    0x53, 0x8e, 0x62, 0x4c, 0x64, 0x88, 0x44, 0x8b, 0xfb, 0xa2, 0x17,
    0x9a, 0x59, 0xf5, 0x87, 0xb3, 0x4f, 0x13, 0x61, 0x45, 0x6d, 0x8d,
    0x09, 0x81, 0x7d, 0x32, 0xbd, 0x8f, 0x40, 0xeb, 0x86, 0xb7, 0x7b,
    0x0b, 0xf0, 0x95, 0x21, 0x22, 0x5c, 0x6b, 0x4e, 0x82, 0x54, 0xd6,
    0x65, 0x93, 0xce, 0x60, 0xb2, 0x1c, 0x73, 0x56, 0xc0, 0x14, 0xa7,
    0x8c, 0xf1, 0xdc, 0x12, 0x75, 0xca, 0x1f, 0x3b, 0xbe, 0xe4, 0xd1,
    0x42, 0x3d, 0xd4, 0x30, 0xa3, 0x3c, 0xb6, 0x26, 0x6f, 0xbf, 0x0e,
    0xda, 0x46, 0x69, 0x07, 0x57, 0x27, 0xf2, 0x1d, 0x9b, 0xbc, 0x94,
    0x43, 0x03, 0xf8, 0x11, 0xc7, 0xf6, 0x90, 0xef, 0x3e, 0xe7, 0x06,
    0xc3, 0xd5, 0x2f, 0xc8, 0x66, 0x1e, 0xd7, 0x08, 0xe8, 0xea, 0xde,
    0x80, 0x52, 0xee, 0xf7, 0x84, 0xaa, 0x72, 0xac, 0x35, 0x4d, 0x6a,
    0x2a, 0x96, 0x1a, 0xd2, 0x71, 0x5a, 0x15, 0x49, 0x74, 0x4b, 0x9f,
    0xd0, 0x5e, 0x04, 0x18, 0xa4, 0xec, 0xc2, 0xe0, 0x41, 0x6e, 0x0f,
    0x51, 0xcb, 0xcc, 0x24, 0x91, 0xaf, 0x50, 0xa1, 0xf4, 0x70, 0x39,
    0x99, 0x7c, 0x3a, 0x85, 0x23, 0xb8, 0xb4, 0x7a, 0xfc, 0x02, 0x36,
    0x5b, 0x25, 0x55, 0x97, 0x31, 0x2d, 0x5d, 0xfa, 0x98, 0xe3, 0x8a,
    0x92, 0xae, 0x05, 0xdf, 0x29, 0x10, 0x67, 0x6c, 0xba, 0xc9, 0xd3,
    0x00, 0xe6, 0xcf, 0xe1, 0x9e, 0xa8, 0x2c, 0x63, 0x16, 0x01, 0x3f,
    0x58, 0xe2, 0x89, 0xa9, 0x0d, 0x38, 0x34, 0x1b, 0xab, 0x33, 0xff,
    0xb0, 0xbb, 0x48, 0x0c, 0x5f, 0xb9, 0xb1, 0xcd, 0x2e, 0xc5, 0xf3,
    0xdb, 0x47, 0xe5, 0xa5, 0x9c, 0x77, 0x0a, 0xa6, 0x20, 0x68, 0xfe,
    0x7f, 0xc1, 0xad,
};

std::uint16_t Rol(std::uint16_t x, unsigned n) {
  return static_cast<std::uint16_t>((x << n) | (x >> (16 - n)));
}

class Rc2 {
 public:
  explicit Rc2(std::span<const std::uint8_t, 8> key) {
    std::array<std::uint8_t, 128> bytes{};
    std::copy(key.begin(), key.end(), bytes.begin());
    for (int i = 8; i < 128; ++i) {
      bytes[i] = kPi[static_cast<std::uint8_t>(bytes[i - 1] + bytes[i - 8])];
    }
    bytes[120] = kPi[bytes[120]];
    for (int i = 119; i >= 0; --i) bytes[i] = kPi[bytes[i + 1] ^ bytes[i + 8]];
    for (int i = 0; i < 64; ++i) {
      key_[i] = static_cast<std::uint16_t>(bytes[2 * i] |
                                           (bytes[2 * i + 1] << 8));
    }
  }

  std::array<std::uint8_t, 8> encrypt(
      std::span<const std::uint8_t, 8> input) const {
    std::array<std::uint16_t, 4> r{};
    for (int i = 0; i < 4; ++i) {
      r[i] = static_cast<std::uint16_t>(input[2 * i] |
                                        (input[2 * i + 1] << 8));
    }
    int j = 0;
    const auto mix = [&](int rounds) {
      for (int round = 0; round < rounds; ++round) {
        for (int i = 0; i < 4; ++i) {
          const auto composite = static_cast<std::uint16_t>(
              (r[(i + 3) & 3] & r[(i + 2) & 3]) +
              (static_cast<std::uint16_t>(~r[(i + 3) & 3]) & r[(i + 1) & 3]));
          r[i] = Rol(static_cast<std::uint16_t>(r[i] + key_[j++] + composite),
                     std::array<unsigned, 4>{1, 2, 3, 5}[i]);
        }
      }
    };
    const auto mash = [&]() {
      for (int i = 0; i < 4; ++i) {
        r[i] = static_cast<std::uint16_t>(
            r[i] + key_[r[(i + 3) & 3] & 63]);
      }
    };
    mix(5);
    mash();
    mix(6);
    mash();
    mix(5);

    std::array<std::uint8_t, 8> output{};
    for (int i = 0; i < 4; ++i) {
      output[2 * i] = static_cast<std::uint8_t>(r[i]);
      output[2 * i + 1] = static_cast<std::uint8_t>(r[i] >> 8);
    }
    return output;
  }

 private:
  std::array<std::uint16_t, 64> key_{};
};

std::vector<std::uint8_t> DecimalBytes(std::string text) {
  cpp_int value = 0;
  for (const char c : text) {
    value *= 10;
    value += c - '0';
  }
  std::vector<std::uint8_t> result;
  while (value != 0) {
    result.push_back(static_cast<std::uint8_t>(value & 0xff));
    value >>= 8;
  }
  std::reverse(result.begin(), result.end());
  return result;
}

std::uint64_t Load64(std::span<const std::uint8_t> bytes) {
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) value = (value << 8) | bytes[i];
  return value;
}

struct Target {
  std::string label;
  std::string layout;
  int factor_offset;
  std::array<std::uint8_t, 8> bytes;
};

std::vector<std::uint8_t> GroupLayout(std::span<const std::uint8_t> input,
                                      int width, bool reverse_groups,
                                      bool reverse_inside) {
  std::vector<std::vector<std::uint8_t>> groups;
  int first = static_cast<int>(input.size()) % width;
  if (first == 0) first = width;
  for (int pos = 0; pos < static_cast<int>(input.size());) {
    const int count = pos == 0 ? first : width;
    groups.emplace_back(input.begin() + pos, input.begin() + pos + count);
    pos += count;
  }
  if (reverse_groups) std::reverse(groups.begin(), groups.end());
  std::vector<std::uint8_t> result;
  for (auto group : groups) {
    if (reverse_inside) std::reverse(group.begin(), group.end());
    result.insert(result.end(), group.begin(), group.end());
  }
  return result;
}

std::string Hex(std::span<const std::uint8_t> bytes) {
  std::ostringstream out;
  for (const auto x : bytes) {
    out << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<unsigned>(x);
  }
  return out.str();
}

}  // namespace

int main(int argc, char** argv) {
  const int stream_bytes = argc > 1 ? std::stoi(argv[1]) : 512;
  const std::string path = argc > 2 ? argv[2] : "data/solved_decimal_challenge.tsv";
  if (stream_bytes < 8 || stream_bytes % 8 != 0) {
    std::cerr << "stream length must be a positive multiple of 8\n";
    return 2;
  }

  std::ifstream input(path);
  std::string line;
  std::getline(input, line);
  std::vector<Target> targets;
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string label, modulus, factor;
    std::getline(row, label, '\t');
    std::getline(row, modulus, '\t');
    for (int which = 1; which <= 2 && std::getline(row, factor, '\t'); ++which) {
      const auto canonical = DecimalBytes(factor);
      for (int width = 1; width <= 4; ++width) {
        for (const bool reverse_groups : {false, true}) {
          for (const bool reverse_inside : {false, true}) {
            auto bytes = GroupLayout(canonical, width, reverse_groups,
                                     reverse_inside);
            const std::string layout = "w" + std::to_string(width) +
                (reverse_groups ? "-rg" : "-fg") +
                (reverse_inside ? "-ri" : "-fi");
            // Ignore edge bytes changed by forced top bits and prime search.
            for (int offset = 2; offset + 8 <= static_cast<int>(bytes.size()) - 4;
                 ++offset) {
              Target target{label + "/f" + std::to_string(which), layout,
                            offset, {}};
              std::copy_n(bytes.begin() + offset, 8, target.bytes.begin());
              targets.push_back(std::move(target));
            }
          }
        }
      }
    }
  }

  std::unordered_multimap<std::uint64_t, std::size_t> lookup;
  lookup.reserve(targets.size() * 2);
  for (std::size_t i = 0; i < targets.size(); ++i) {
    lookup.emplace(Load64(targets[i].bytes), i);
  }

  std::uint64_t matches = 0;
  for (std::uint32_t mask = 0; mask < 65536; ++mask) {
    std::array<std::uint8_t, 16> state{};
    for (int i = 0; i < 16; ++i) {
      state[i] = static_cast<std::uint8_t>(i ^ ((mask >> i) & 1));
    }
    const Rc2 rc2(std::span<const std::uint8_t, 8>(state.data(), 8));
    std::vector<std::uint8_t> stream;
    stream.reserve(stream_bytes);
    while (static_cast<int>(stream.size()) < stream_bytes) {
      for (int i = 12; i < 16; ++i) {
        if (++state[i] != 0) break;
      }
      const auto block = rc2.encrypt(
          std::span<const std::uint8_t, 8>(state.data() + 8, 8));
      stream.insert(stream.end(), block.begin(), block.end());
    }
    for (int offset = 0; offset + 8 <= stream_bytes; ++offset) {
      const auto range = lookup.equal_range(
          Load64(std::span<const std::uint8_t>(stream.data() + offset, 8)));
      for (auto it = range.first; it != range.second; ++it) {
        const auto& target = targets[it->second];
        std::cout << "MATCH mask=" << mask << " stream_offset=" << offset
                  << " target=" << target.label << " layout=" << target.layout
                  << " factor_offset=" << target.factor_offset
                  << " bytes=" << Hex(target.bytes) << '\n';
        ++matches;
      }
    }
  }
  std::cout << "states=65536 stream_bytes=" << stream_bytes
            << " targets=" << targets.size() << " matches=" << matches << '\n';
}
