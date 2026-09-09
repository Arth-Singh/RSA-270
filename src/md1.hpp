#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rsa270 {

// RSA-MD1 recovered from Lotus Notes 2.1 _BSAFE.DLL (1991-11-16).
// MD1 uses the MD2 context/finalization shape, a proprietary S-box, and a
// different 18x48-byte mixing schedule.
class Md1 {
 public:
  Md1();

  void update(std::span<const std::uint8_t> input);
  std::array<std::uint8_t, 16> final();

  static std::array<std::uint8_t, 16> digest(
      std::span<const std::uint8_t> input);

 private:
  void update_byte(std::uint8_t value);

  std::array<std::uint8_t, 48> state_{};
  std::array<std::uint8_t, 16> checksum_{};
  std::uint8_t index_ = 0;
  std::uint8_t last_ = 0;
  bool finalized_ = false;
};

}  // namespace rsa270
