#include <gmpxx.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Exact random-byte core recovered from the 1991-11-16 Lotus Notes BSAFE DLL
// (SHA-256 5fd7711ad68e1af8a1d832b4a95d35bc414e1f663eefded3f140a0aaf209eb23).
//
// Seed update at file 0x1e76 XORs one byte into a rotating 16-byte state.
// Random-byte at 0x1ebb increments state[12..15] little endian, makes an RC2
// 64-bit key schedule from state[0..7], encrypts state[8..15], and emits the
// resulting eight bytes.  RC2 setup/encrypt are file 0xcc1c/0xccb4; PITABLE is
// at 0x2863.  Key length and effective key size are both 64 bits.

namespace {

constexpr std::array<std::uint8_t, 256> kPiTable = {
    0xd9, 0x78, 0xf9, 0xc4, 0x19, 0xdd, 0xb5, 0xed, 0x28, 0xe9, 0xfd, 0x79, 0x4a, 0xa0, 0xd8, 0x9d,
    0xc6, 0x7e, 0x37, 0x83, 0x2b, 0x76, 0x53, 0x8e, 0x62, 0x4c, 0x64, 0x88, 0x44, 0x8b, 0xfb, 0xa2,
    0x17, 0x9a, 0x59, 0xf5, 0x87, 0xb3, 0x4f, 0x13, 0x61, 0x45, 0x6d, 0x8d, 0x09, 0x81, 0x7d, 0x32,
    0xbd, 0x8f, 0x40, 0xeb, 0x86, 0xb7, 0x7b, 0x0b, 0xf0, 0x95, 0x21, 0x22, 0x5c, 0x6b, 0x4e, 0x82,
    0x54, 0xd6, 0x65, 0x93, 0xce, 0x60, 0xb2, 0x1c, 0x73, 0x56, 0xc0, 0x14, 0xa7, 0x8c, 0xf1, 0xdc,
    0x12, 0x75, 0xca, 0x1f, 0x3b, 0xbe, 0xe4, 0xd1, 0x42, 0x3d, 0xd4, 0x30, 0xa3, 0x3c, 0xb6, 0x26,
    0x6f, 0xbf, 0x0e, 0xda, 0x46, 0x69, 0x07, 0x57, 0x27, 0xf2, 0x1d, 0x9b, 0xbc, 0x94, 0x43, 0x03,
    0xf8, 0x11, 0xc7, 0xf6, 0x90, 0xef, 0x3e, 0xe7, 0x06, 0xc3, 0xd5, 0x2f, 0xc8, 0x66, 0x1e, 0xd7,
    0x08, 0xe8, 0xea, 0xde, 0x80, 0x52, 0xee, 0xf7, 0x84, 0xaa, 0x72, 0xac, 0x35, 0x4d, 0x6a, 0x2a,
    0x96, 0x1a, 0xd2, 0x71, 0x5a, 0x15, 0x49, 0x74, 0x4b, 0x9f, 0xd0, 0x5e, 0x04, 0x18, 0xa4, 0xec,
    0xc2, 0xe0, 0x41, 0x6e, 0x0f, 0x51, 0xcb, 0xcc, 0x24, 0x91, 0xaf, 0x50, 0xa1, 0xf4, 0x70, 0x39,
    0x99, 0x7c, 0x3a, 0x85, 0x23, 0xb8, 0xb4, 0x7a, 0xfc, 0x02, 0x36, 0x5b, 0x25, 0x55, 0x97, 0x31,
    0x2d, 0x5d, 0xfa, 0x98, 0xe3, 0x8a, 0x92, 0xae, 0x05, 0xdf, 0x29, 0x10, 0x67, 0x6c, 0xba, 0xc9,
    0xd3, 0x00, 0xe6, 0xcf, 0xe1, 0x9e, 0xa8, 0x2c, 0x63, 0x16, 0x01, 0x3f, 0x58, 0xe2, 0x89, 0xa9,
    0x0d, 0x38, 0x34, 0x1b, 0xab, 0x33, 0xff, 0xb0, 0xbb, 0x48, 0x0c, 0x5f, 0xb9, 0xb1, 0xcd, 0x2e,
    0xc5, 0xf3, 0xdb, 0x47, 0xe5, 0xa5, 0x9c, 0x77, 0x0a, 0xa6, 0x20, 0x68, 0xfe, 0x7f, 0xc1, 0xad,
};

std::uint16_t Rotl16(std::uint16_t value, unsigned count) {
  return static_cast<std::uint16_t>((value << count) |
                                    (value >> (16 - count)));
}

std::array<std::uint16_t, 64> Rc2Key64(
    const std::array<std::uint8_t, 8>& key) {
  std::array<std::uint8_t, 128> bytes{};
  std::copy(key.begin(), key.end(), bytes.begin());
  for (std::size_t i = 8; i < bytes.size(); ++i) {
    bytes[i] = kPiTable[static_cast<std::uint8_t>(bytes[i - 1] + bytes[i - 8])];
  }
  // T1=64, T8=8, TM=0xff in the generic RC2 key-reduction algorithm.
  bytes[120] = kPiTable[bytes[120]];
  for (int i = 119; i >= 0; --i) {
    bytes[i] = kPiTable[bytes[i + 1] ^ bytes[i + 8]];
  }
  std::array<std::uint16_t, 64> words{};
  for (std::size_t i = 0; i < words.size(); ++i) {
    words[i] = static_cast<std::uint16_t>(bytes[2 * i]) |
               static_cast<std::uint16_t>(bytes[2 * i + 1] << 8);
  }
  return words;
}

std::array<std::uint8_t, 8> Rc2Encrypt(
    const std::array<std::uint8_t, 8>& input,
    const std::array<std::uint16_t, 64>& key) {
  std::uint16_t r0 = input[0] | static_cast<std::uint16_t>(input[1] << 8);
  std::uint16_t r1 = input[2] | static_cast<std::uint16_t>(input[3] << 8);
  std::uint16_t r2 = input[4] | static_cast<std::uint16_t>(input[5] << 8);
  std::uint16_t r3 = input[6] | static_cast<std::uint16_t>(input[7] << 8);
  std::size_t j = 0;
  for (unsigned round = 0; round < 16; ++round) {
    r0 = Rotl16(static_cast<std::uint16_t>(r0 + key[j++] +
        (r3 & r2) + (static_cast<std::uint16_t>(~r3) & r1)), 1);
    r1 = Rotl16(static_cast<std::uint16_t>(r1 + key[j++] +
        (r0 & r3) + (static_cast<std::uint16_t>(~r0) & r2)), 2);
    r2 = Rotl16(static_cast<std::uint16_t>(r2 + key[j++] +
        (r1 & r0) + (static_cast<std::uint16_t>(~r1) & r3)), 3);
    r3 = Rotl16(static_cast<std::uint16_t>(r3 + key[j++] +
        (r2 & r1) + (static_cast<std::uint16_t>(~r2) & r0)), 5);
    if (round == 4 || round == 10) {
      r0 = static_cast<std::uint16_t>(r0 + key[r3 & 63]);
      r1 = static_cast<std::uint16_t>(r1 + key[r0 & 63]);
      r2 = static_cast<std::uint16_t>(r2 + key[r1 & 63]);
      r3 = static_cast<std::uint16_t>(r3 + key[r2 & 63]);
    }
  }
  return {static_cast<std::uint8_t>(r0), static_cast<std::uint8_t>(r0 >> 8),
          static_cast<std::uint8_t>(r1), static_cast<std::uint8_t>(r1 >> 8),
          static_cast<std::uint8_t>(r2), static_cast<std::uint8_t>(r2 >> 8),
          static_cast<std::uint8_t>(r3), static_cast<std::uint8_t>(r3 >> 8)};
}

std::vector<std::uint8_t> StateStream(std::array<std::uint8_t, 16> state,
                                      std::size_t count) {
  std::array<std::uint8_t, 8> key_bytes{};
  std::copy_n(state.begin(), 8, key_bytes.begin());
  const auto key = Rc2Key64(key_bytes);
  std::vector<std::uint8_t> result;
  result.reserve(count);
  while (result.size() < count) {
    for (unsigned i = 12; i < 16; ++i) {
      if (++state[i] != 0) break;
    }
    std::array<std::uint8_t, 8> block{};
    std::copy_n(state.begin() + 8, 8, block.begin());
    const auto encrypted = Rc2Encrypt(block, key);
    const std::size_t take = std::min(encrypted.size(), count - result.size());
    result.insert(result.end(), encrypted.begin(), encrypted.begin() + take);
  }
  return result;
}

std::vector<std::uint8_t> ParityStream(std::uint16_t parity,
                                       std::uint8_t injected_value,
                                       std::size_t count) {
  std::array<std::uint8_t, 16> state{};
  for (unsigned i = 0; i < state.size(); ++i) {
    state[i] = static_cast<std::uint8_t>(i);
    if ((parity >> i) & 1) state[i] ^= injected_value;
  }
  return StateStream(state, count);
}

struct Target {
  std::string label;
  mpz_class prime;
  mpz_class previous;
  std::size_t bits;
  std::size_t bytes;
};

Target MakeTarget(std::string label, const std::string& decimal) {
  Target target{std::move(label), mpz_class(decimal, 10), 0, 0, 0};
  mpz_prevprime(target.previous.get_mpz_t(), target.prime.get_mpz_t());
  target.bits = mpz_sizeinbase(target.prime.get_mpz_t(), 2);
  target.bytes = (target.bits + 7) / 8;
  return target;
}

std::vector<Target> ReadTargets(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open target file: " + path);
  std::string line;
  std::getline(input, line);
  std::vector<Target> result;
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string label, modulus, factor;
    std::getline(row, label, '\t');
    std::getline(row, modulus, '\t');
    for (unsigned which = 1;
         which <= 2 && std::getline(row, factor, '\t'); ++which) {
      result.push_back(MakeTarget(label + "-f" + std::to_string(which),
                                  factor));
    }
  }
  return result;
}

std::vector<std::uint8_t> ExportBytes(const mpz_class& value,
                                      std::size_t size, bool little) {
  std::vector<std::uint8_t> bytes(size);
  std::size_t written = 0;
  std::vector<std::uint8_t> minimal(size);
  mpz_export(minimal.data(), &written, little ? -1 : 1, 1, 1, 0,
             value.get_mpz_t());
  if (little) {
    std::copy_n(minimal.begin(), written, bytes.begin());
  } else {
    std::copy_n(minimal.begin(), written, bytes.end() - written);
  }
  return bytes;
}

mpz_class ImportBytes(const std::vector<std::uint8_t>& bytes, bool little) {
  mpz_class value;
  mpz_import(value.get_mpz_t(), bytes.size(), little ? -1 : 1, 1, 1, 0,
             bytes.data());
  return value;
}

struct Lane {
  std::string label;
  std::vector<unsigned> permutation;  // numeric[i] = raw[permutation[i]]
};

std::vector<std::uint8_t> RawForNumeric(
    const std::vector<std::uint8_t>& numeric, const Lane& lane) {
  std::vector<std::uint8_t> raw(numeric.size());
  const std::size_t width = lane.permutation.size();
  for (std::size_t base = 0; base < numeric.size(); base += width) {
    const std::size_t available = std::min(width, numeric.size() - base);
    for (std::size_t i = 0; i < available; ++i) {
      const unsigned source = lane.permutation[i];
      if (source < available) raw[base + source] = numeric[base + i];
      else raw[base + i] = numeric[base + i];
    }
  }
  return raw;
}

std::vector<std::uint8_t> NumericFromRaw(
    const std::vector<std::uint8_t>& raw, const Lane& lane) {
  std::vector<std::uint8_t> numeric(raw.size());
  const std::size_t width = lane.permutation.size();
  for (std::size_t base = 0; base < raw.size(); base += width) {
    const std::size_t available = std::min(width, raw.size() - base);
    for (std::size_t i = 0; i < available; ++i) {
      const unsigned source = lane.permutation[i];
      numeric[base + i] = source < available ? raw[base + source]
                                             : raw[base + i];
    }
  }
  return numeric;
}

std::uint64_t Load64(const std::uint8_t* data) {
  std::uint64_t value;
  std::memcpy(&value, data, sizeof(value));
  return value;
}

struct Pattern {
  std::size_t target;
  std::size_t lane;
  bool little;
  std::size_t anchor;
  std::uint64_t fingerprint;
};

bool InFirstPrimeInterval(const mpz_class& candidate, const Target& target,
                          mpz_class* delta) {
  if (candidate > target.prime || candidate <= target.previous ||
      mpz_even_p(candidate.get_mpz_t())) return false;
  *delta = target.prime - candidate;
  return true;
}

std::string HexState(std::uint16_t parity, std::uint8_t injected) {
  std::ostringstream out;
  out << std::hex;
  for (unsigned i = 0; i < 16; ++i) {
    const unsigned value = i ^ (((parity >> i) & 1) ? injected : 0);
    if (value < 16) out << '0';
    out << value;
  }
  return out.str();
}

using Contributions =
    std::array<std::array<std::uint8_t, 16>, 65536>;

std::pair<Contributions, Contributions> RandomConstantContributions() {
  Contributions from_a{};
  Contributions from_b{};
  for (unsigned input = 0; input < 65536; ++input) {
    std::uint16_t a = 0;
    std::uint16_t b = 0;
    unsigned required = 100;
    unsigned cursor = 0;
    for (unsigned call = 0; call < 150; ++call) {
      if (required != 0) --required;
      a ^= static_cast<std::uint16_t>(input);
      a = static_cast<std::uint16_t>(
          (static_cast<std::uint32_t>(a) * 0xaf63u) >> 11);
      b = static_cast<std::uint16_t>((b >> 1) | (b << 1));
      b ^= static_cast<std::uint16_t>(input);
      from_a[input][cursor] ^= static_cast<std::uint8_t>(a);
      from_b[input][cursor] ^= static_cast<std::uint8_t>(b);
      cursor = (cursor + 1) % (required == 0 ? 12 : 16);
    }
  }
  return {std::move(from_a), std::move(from_b)};
}

std::array<std::uint8_t, 16> DirectConstantSeed(std::uint16_t a_input,
                                                std::uint16_t b_input) {
  std::array<std::uint8_t, 16> seed{};
  for (unsigned i = 0; i < seed.size(); ++i)
    seed[i] = static_cast<std::uint8_t>(i);
  std::uint16_t a = 0;
  std::uint16_t b = 0;
  unsigned required = 100;
  unsigned cursor = 0;
  for (unsigned call = 0; call < 150; ++call) {
    if (required != 0) --required;
    a ^= a_input;
    a = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(a) * 0xaf63u) >> 11);
    b = static_cast<std::uint16_t>((b >> 1) | (b << 1));
    b ^= b_input;
    seed[cursor] ^= static_cast<std::uint8_t>(a ^ b);
    cursor = (cursor + 1) % (required == 0 ? 12 : 16);
  }
  return seed;
}

int RunConstantTimeScan(int argc, char** argv) {
  if (argc < 6 || argc > 7) {
    std::cerr << "usage: " << argv[0]
              << " time-constant JULIAN_FIRST JULIAN_LAST"
                 " HUNDREDTH_STEP CANDIDATE_STARTS [TARGET_TSV]\n";
    return 2;
  }
  const unsigned julian_first = std::stoul(argv[2]);
  const unsigned julian_last = std::stoul(argv[3]);
  const unsigned hundredth_step = std::stoul(argv[4]);
  const std::size_t candidate_starts = std::stoull(argv[5]);
  const std::string target_path = argc >= 7
      ? argv[6] : "data/solved_decimal_challenge.tsv";
  if (julian_first >= julian_last || hundredth_step == 0 ||
      candidate_starts == 0) return 2;

  const auto targets = ReadTargets(target_path);
  const std::vector<Lane> lanes = {
      {"byte", {0}}, {"word-be", {0, 1}}, {"word-le", {1, 0}},
      {"24-012", {0, 1, 2}}, {"24-021", {0, 2, 1}},
      {"24-102", {1, 0, 2}}, {"24-120", {1, 2, 0}},
      {"24-201", {2, 0, 1}}, {"24-210", {2, 1, 0}},
  };
  constexpr std::size_t kAnchor = 4;
  std::multimap<std::uint64_t, Pattern> patterns;
  for (std::size_t t = 0; t < targets.size(); ++t) {
    for (bool little : {false, true}) {
      const auto numeric = ExportBytes(targets[t].prime, targets[t].bytes,
                                       little);
      for (std::size_t l = 0; l < lanes.size(); ++l) {
        const auto raw = RawForNumeric(numeric, lanes[l]);
        patterns.emplace(Load64(raw.data() + kAnchor),
                         Pattern{t, l, little, kAnchor,
                                 Load64(raw.data() + kAnchor)});
      }
    }
  }

  // `_LIB.243` RANDOM, independently recovered from companion _LIB.DLL:
  // A ^= low16(Innards[0]/7); A = low16((uint32(A)*0xaf63)>>11)
  // B = low16((B>>1)|(B<<1)); B ^= xor(TIMEDATE words); return A^B.
  // Package initialization starts A=B=0 and calls it 150 times.  With one
  // constant TIMEDATE, A and B contributions XOR-fold independently into the
  // exact BSAFE seed state, so 2*65536 precomputations replace 150 operations
  // per timestamp.
  auto [from_a, from_b] = RandomConstantContributions();
  for (const auto [a_input, b_input] :
       {std::pair<std::uint16_t, std::uint16_t>{0, 0},
        {1, 0xffff}, {0x1234, 0xabcd}, {0xffff, 1}}) {
    std::array<std::uint8_t, 16> split{};
    for (unsigned i = 0; i < split.size(); ++i) {
      split[i] = static_cast<std::uint8_t>(
          i ^ from_a[a_input][i] ^ from_b[b_input][i]);
    }
    if (split != DirectConstantSeed(a_input, b_input)) {
      std::cerr << "split `_LIB.243` seed precomputation KAT failed\n";
      return 3;
    }
  }
  std::size_t states = 0;
  std::size_t anchor_hits = 0;
  std::size_t full_matches = 0;
  for (unsigned julian = julian_first; julian < julian_last; ++julian) {
    for (unsigned hundredths = 0; hundredths < 8640000;
         hundredths += hundredth_step) {
      const std::uint16_t a_input =
          static_cast<std::uint16_t>(hundredths / 7);
      // OSCurrentTIMEDATE is four little-endian words. RANDOM explicitly
      // clears byte 7, erasing timezone/DST while retaining 24-bit Julian day.
      const std::uint16_t b_input = static_cast<std::uint16_t>(
          (hundredths & 0xffffu) ^ (hundredths >> 16) ^
          (julian & 0xffffu) ^ ((julian >> 16) & 0xffu));
      std::array<std::uint8_t, 16> seed{};
      for (unsigned i = 0; i < seed.size(); ++i) {
        seed[i] = static_cast<std::uint8_t>(
            i ^ from_a[a_input][i] ^ from_b[b_input][i]);
      }
      ++states;
      auto stream = StateStream(seed, candidate_starts + kAnchor + 8);
      for (std::size_t start = 0; start < candidate_starts; ++start) {
        const auto range = patterns.equal_range(
            Load64(stream.data() + start + kAnchor));
        for (auto found = range.first; found != range.second; ++found) {
          ++anchor_hits;
          const Pattern& pattern = found->second;
          const Target& target = targets[pattern.target];
          if (start + target.bytes > stream.size())
            stream = StateStream(seed, start + target.bytes);
          const std::vector<std::uint8_t> raw(
              stream.begin() + start,
              stream.begin() + start + target.bytes);
          const auto numeric = NumericFromRaw(raw, lanes[pattern.lane]);
          const mpz_class random = ImportBytes(numeric, pattern.little);
          for (unsigned forced_top : {0u, 1u, 2u}) {
            mpz_class candidate = random;
            if (forced_top != 0) {
              const std::size_t retained = target.bits - forced_top;
              mpz_fdiv_r_2exp(candidate.get_mpz_t(), candidate.get_mpz_t(),
                              retained);
              candidate |= ((mpz_class{1} << forced_top) - 1) << retained;
            } else {
              mpz_fdiv_r_2exp(candidate.get_mpz_t(), candidate.get_mpz_t(),
                              target.bits);
            }
            candidate |= 1;
            mpz_class delta;
            if (!InFirstPrimeInterval(candidate, target, &delta)) continue;
            ++full_matches;
            std::cout << "MATCH model=constant-time-direct-stream"
                      << " julian=" << julian
                      << " hundredths=" << hundredths
                      << " target=" << target.label
                      << " stream_offset=" << start
                      << " integer_endian="
                      << (pattern.little ? "little" : "big")
                      << " lane=" << lanes[pattern.lane].label
                      << " forced_top=" << forced_top
                      << " prime_delta=" << delta << '\n';
          }
        }
      }
    }
    std::cout << "completed julian=" << julian << '\n';
  }
  std::cout << "summary model=constant-time-direct-stream-speculative"
            << " julian_first=" << julian_first
            << " julian_last=" << julian_last
            << " hundredth_step=" << hundredth_step
            << " candidate_starts=" << candidate_starts
            << " targets=" << targets.size()
            << " states=" << states
            << " anchor_hits=" << anchor_hits
            << " full_matches=" << full_matches << '\n';
  return full_matches == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "time-constant")
    return RunConstantTimeScan(argc, argv);
  const std::size_t stream_bytes = argc >= 2 ? std::stoull(argv[1]) : 2048;
  const std::string target_path =
      argc >= 3 ? argv[2] : "data/solved_decimal_challenge.tsv";
  if (stream_bytes < 128) {
    std::cerr << "stream length must be at least 128\n";
    return 2;
  }

  const std::vector<Target> targets = ReadTargets(target_path);
  const std::vector<Lane> lanes = {
      {"byte", {0}}, {"word-be", {0, 1}}, {"word-le", {1, 0}},
      {"24-012", {0, 1, 2}}, {"24-021", {0, 2, 1}},
      {"24-102", {1, 0, 2}}, {"24-120", {1, 2, 0}},
      {"24-201", {2, 0, 1}}, {"24-210", {2, 1, 0}},
  };

  std::multimap<std::uint64_t, Pattern> patterns;
  for (std::size_t t = 0; t < targets.size(); ++t) {
    for (bool little : {false, true}) {
      const auto numeric = ExportBytes(targets[t].prime, targets[t].bytes,
                                       little);
      for (std::size_t l = 0; l < lanes.size(); ++l) {
        const auto raw = RawForNumeric(numeric, lanes[l]);
        const std::size_t anchor = 4;  // Avoid forced top and searched low end.
        Pattern pattern{t, l, little, anchor, Load64(raw.data() + anchor)};
        patterns.emplace(pattern.fingerprint, pattern);
      }
    }
  }

  std::size_t anchor_hits = 0;
  std::size_t full_matches = 0;
  std::set<std::string> printed;
  for (std::uint8_t injected : {std::uint8_t{0x01}, std::uint8_t{0x80},
                                std::uint8_t{0xff}}) {
    for (std::uint32_t parity = 0; parity < 65536; ++parity) {
      const auto stream = ParityStream(static_cast<std::uint16_t>(parity),
                                       injected, stream_bytes + 64);
      for (std::size_t offset = 0; offset < stream_bytes; ++offset) {
        const std::uint64_t fingerprint = Load64(stream.data() + offset + 4);
        const auto range = patterns.equal_range(fingerprint);
        for (auto found = range.first; found != range.second; ++found) {
          const Pattern& pattern = found->second;
          const Target& target = targets[pattern.target];
          if (offset + target.bytes > stream.size()) continue;
          ++anchor_hits;
          std::vector<std::uint8_t> raw(stream.begin() + offset,
                                        stream.begin() + offset + target.bytes);
          const auto numeric = NumericFromRaw(raw, lanes[pattern.lane]);
          const mpz_class random = ImportBytes(numeric, pattern.little);
          for (unsigned forced_top : {0u, 1u, 2u}) {
            mpz_class candidate = random;
            if (forced_top != 0) {
              const std::size_t random_bits = target.bits - forced_top;
              mpz_fdiv_r_2exp(candidate.get_mpz_t(), candidate.get_mpz_t(),
                              random_bits);
              candidate |= ((mpz_class{1} << forced_top) - 1) << random_bits;
            } else {
              mpz_fdiv_r_2exp(candidate.get_mpz_t(), candidate.get_mpz_t(),
                              target.bits);
            }
            candidate |= 1;
            mpz_class delta;
            if (!InFirstPrimeInterval(candidate, target, &delta)) continue;
            std::ostringstream key;
            key << target.label << ':' << unsigned(injected) << ':' << parity
                << ':' << offset << ':' << pattern.lane << ':'
                << pattern.little << ':' << forced_top << ':' << delta;
            if (!printed.insert(key.str()).second) continue;
            ++full_matches;
            std::cout << "MATCH target=" << target.label
                      << " xor=0x" << std::hex << unsigned(injected)
                      << " parity=0x" << parity << std::dec
                      << " state=" << HexState(parity, injected)
                      << " offset=" << offset
                      << " endian=" << (pattern.little ? "little" : "big")
                      << " lane=" << lanes[pattern.lane].label
                      << " forced_top=" << forced_top
                      << " prime_delta=" << delta << "\n";
          }
        }
      }
    }
    std::cout << "completed xor=0x" << std::hex << unsigned(injected)
              << std::dec << " parity_states=65536 stream_bytes="
              << stream_bytes << "\n";
  }

  // Exact DLL prime-candidate skeleton, recovered past MAKEKEYPAIR in 0x6382:
  // 0x6960 calls random-byte at 0x1ebb once into the low byte of every
  // little-endian 16-bit limb (relocation at file 0x6a48).  It then forces the
  // top two requested bits, clears bit zero, and searches upward.  A factor
  // interval is statically impossible if any unaffected interior limb has a
  // nonzero high byte.  This test is independent of seed and stream offset.
  for (const auto& target : targets) {
    bool possible = false;
    const mpz_class gap = target.prime - target.previous;
    // Raw candidate has bit zero cleared; target prime is odd, hence delta is
    // odd.  It must lie strictly above the preceding prime.
    for (mpz_class delta = 1; delta < gap; delta += 2) {
      const mpz_class candidate = target.prime - delta;
      const std::size_t limbs = (target.bits + 15) / 16;
      bool valid = true;
      for (std::size_t i = 1; i + 1 < limbs; ++i) {
        const mpz_class shifted = candidate >> (16 * i);
        const unsigned long limb = mpz_fdiv_ui(shifted.get_mpz_t(), 1u << 16);
        if (limb > 0xff) { valid = false; break; }
      }
      if (valid) { possible = true; break; }
    }
    std::cout << "dll-radix16 target=" << target.label
              << " result=" << (possible ? "survivor" : "impossible")
              << " previous_prime_gap=" << gap << "\n";
  }

  // Known-answer check against independent execution of the recovered x86.
  const std::array<std::uint8_t, 8> test_key = {0,1,2,3,4,5,6,7};
  const std::array<std::uint8_t, 8> test_plain = {8,9,10,11,12,13,14,15};
  const std::array<std::uint8_t, 8> test_want =
      {0x9f,0x0f,0x9b,0x51,0xd9,0xf3,0x19,0x38};
  if (Rc2Encrypt(test_plain, Rc2Key64(test_key)) != test_want) {
    std::cerr << "RC2 recovered-code known-answer test failed\n";
    return 3;
  }
  std::cout << "summary anchor_hits=" << anchor_hits
            << " full_matches=" << full_matches << "\n";
  return full_matches == 0 ? 0 : 1;
}
