#include "md1.hpp"

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

// Hypothesis test, not recovered DLL behavior:
//
// Replace MD5 by recovered RSA-MD1 in the RSAREF/BSAFE-1/2 additive PRNG:
//   state <- state + MD1(seed_block) mod 2^128
//   output <- MD1(state); state <- state + 1 mod 2^128
//
// When every comparator sample is passed as a separate seed block, order is
// lost.  For n binary samples only n+1 states remain, indexed by Hamming
// weight.  This scanner tests those states against every solved decimal RSA
// challenge factor.  It also tests the opposite integer byte order as an
// explicit porting variant; RSAREF itself uses big-endian state arithmetic.

namespace {

using Block = std::array<std::uint8_t, 16>;
using U128 = unsigned __int128;

U128 LoadInteger(const Block& bytes, bool little) {
  U128 value = 0;
  if (little) {
    for (int i = 15; i >= 0; --i) value = (value << 8) | bytes[i];
  } else {
    for (std::uint8_t byte : bytes) value = (value << 8) | byte;
  }
  return value;
}

Block StoreInteger(U128 value, bool little) {
  Block result{};
  for (int i = 0; i < 16; ++i) {
    const int at = little ? i : 15 - i;
    result[at] = static_cast<std::uint8_t>(value);
    value >>= 8;
  }
  return result;
}

struct Encoding {
  std::string label;
  std::vector<std::uint8_t> zero;
  std::vector<std::uint8_t> one;
};

std::vector<Encoding> Encodings() {
  std::vector<Encoding> result;
  // Test every possible one-bit comparator position in an 8-, 16-, or
  // 24-bit memory/register image.  This includes ordinary numeric 0/1 in
  // either endian without giving endian aliases misleading duplicate names.
  for (unsigned bytes : {1u, 2u, 3u}) {
    for (unsigned bit = 0; bit < 8 * bytes; ++bit) {
      Encoding encoding;
      encoding.label = "b" + std::to_string(8 * bytes) + "-mem-bit" +
                       std::to_string(bit);
      encoding.zero.assign(bytes, 0);
      encoding.one.assign(bytes, 0);
      encoding.one[bit / 8] = static_cast<std::uint8_t>(1u << (bit % 8));
      result.push_back(std::move(encoding));
    }
  }
  result.push_back({"byte-00ff", {0x00}, {0xff}});
  result.push_back({"ascii-01", {'0'}, {'1'}});
  return result;
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
  std::vector<std::uint8_t> result(size);
  std::vector<std::uint8_t> minimal(size);
  std::size_t written = 0;
  mpz_export(minimal.data(), &written, little ? -1 : 1, 1, 1, 0,
             value.get_mpz_t());
  if (little) {
    std::copy_n(minimal.begin(), written, result.begin());
  } else {
    std::copy_n(minimal.begin(), written, result.end() - written);
  }
  return result;
}

mpz_class ImportBytes(const std::vector<std::uint8_t>& bytes, bool little) {
  mpz_class result;
  mpz_import(result.get_mpz_t(), bytes.size(), little ? -1 : 1, 1, 1, 0,
             bytes.data());
  return result;
}

struct Lane {
  std::string label;
  std::vector<unsigned> permutation;  // numeric[i] = raw[permutation[i]]
};

std::vector<Lane> Lanes() {
  return {{"byte", {0}},       {"word-01", {0, 1}},
          {"word-10", {1, 0}}, {"24-012", {0, 1, 2}},
          {"24-021", {0, 2, 1}}, {"24-102", {1, 0, 2}},
          {"24-120", {1, 2, 0}}, {"24-201", {2, 0, 1}},
          {"24-210", {2, 1, 0}}};
}

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
  std::uint64_t result;
  std::memcpy(&result, data, sizeof(result));
  return result;
}

struct Pattern {
  std::size_t target;
  std::size_t lane;
  bool little;
  std::uint64_t fingerprint;
};

bool InFirstPrimeInterval(const mpz_class& candidate, const Target& target,
                          mpz_class* delta) {
  if (candidate > target.prime || candidate <= target.previous ||
      mpz_even_p(candidate.get_mpz_t())) return false;
  *delta = target.prime - candidate;
  return true;
}

std::string Hex(const Block& block) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  result.reserve(32);
  for (std::uint8_t x : block) {
    result.push_back(kHex[x >> 4]);
    result.push_back(kHex[x & 15]);
  }
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  const std::size_t stream_bytes = argc >= 2 ? std::stoull(argv[1]) : 2048;
  const std::string target_path =
      argc >= 3 ? argv[2] : "data/solved_decimal_challenge.tsv";
  if (stream_bytes < 128) {
    std::cerr << "stream length must be at least 128\n";
    return 2;
  }

  const auto targets = ReadTargets(target_path);
  const auto lanes = Lanes();
  const auto encodings = Encodings();
  // Plausible sample-call counts: recovered DLL thresholds, RSAREF threshold,
  // RSA-100 factor/modulus bit lengths, challenge name, and conservative large
  // pools used in contemporary guidance.  This is deliberately finite and is
  // reported as model coverage, not claimed as recovered behavior.
  const std::vector<unsigned> sample_counts =
      {34, 50, 64, 83, 86, 100, 128, 150, 165, 166,
       256, 270, 330, 512, 768, 1000, 1024};

  constexpr std::size_t kAnchor = 4;
  std::multimap<std::uint64_t, Pattern> patterns;
  for (std::size_t t = 0; t < targets.size(); ++t) {
    for (bool little : {false, true}) {
      const auto numeric = ExportBytes(targets[t].prime, targets[t].bytes,
                                       little);
      for (std::size_t l = 0; l < lanes.size(); ++l) {
        const auto raw = RawForNumeric(numeric, lanes[l]);
        patterns.emplace(Load64(raw.data() + kAnchor),
                         Pattern{t, l, little,
                                 Load64(raw.data() + kAnchor)});
      }
    }
  }

  std::size_t states_tested = 0;
  std::size_t anchor_hits = 0;
  std::size_t full_matches = 0;
  std::set<std::string> printed;
  std::vector<std::uint8_t> stream;
  stream.reserve(stream_bytes + 64);

  for (const Encoding& encoding : encodings) {
    const Block h0 = rsa270::Md1::digest(encoding.zero);
    const Block h1 = rsa270::Md1::digest(encoding.one);
    for (bool arithmetic_little : {false, true}) {
      const U128 zero = LoadInteger(h0, arithmetic_little);
      const U128 delta = LoadInteger(h1, arithmetic_little) - zero;
      for (unsigned samples : sample_counts) {
        U128 initial = zero * samples;
        for (unsigned weight = 0; weight <= samples; ++weight) {
          ++states_tested;
          U128 state = initial + delta * weight;
          stream.clear();
          while (stream.size() < stream_bytes + 64) {
            const Block state_bytes = StoreInteger(state, arithmetic_little);
            const Block output = rsa270::Md1::digest(state_bytes);
            stream.insert(stream.end(), output.begin(), output.end());
            ++state;
          }
          for (std::size_t start = 0; start < stream_bytes; ++start) {
            const std::uint64_t fingerprint =
                Load64(stream.data() + start + kAnchor);
            const auto range = patterns.equal_range(fingerprint);
            for (auto found = range.first; found != range.second; ++found) {
              const Pattern& pattern = found->second;
              const Target& target = targets[pattern.target];
              if (start + target.bytes > stream.size()) continue;
              ++anchor_hits;
              const std::vector<std::uint8_t> raw(
                  stream.begin() + start,
                  stream.begin() + start + target.bytes);
              const auto numeric = NumericFromRaw(raw, lanes[pattern.lane]);
              const mpz_class random = ImportBytes(numeric, pattern.little);
              for (unsigned forced_top : {0u, 1u, 2u}) {
                mpz_class candidate = random;
                if (forced_top != 0) {
                  const std::size_t retained = target.bits - forced_top;
                  mpz_fdiv_r_2exp(candidate.get_mpz_t(),
                                  candidate.get_mpz_t(), retained);
                  candidate |= ((mpz_class{1} << forced_top) - 1) << retained;
                } else {
                  mpz_fdiv_r_2exp(candidate.get_mpz_t(),
                                  candidate.get_mpz_t(), target.bits);
                }
                candidate |= 1;
                mpz_class prime_delta;
                if (!InFirstPrimeInterval(candidate, target, &prime_delta))
                  continue;
                std::ostringstream identity;
                identity << target.label << ':' << encoding.label << ':'
                         << arithmetic_little << ':' << samples << ':'
                         << weight << ':' << start << ':' << pattern.lane << ':'
                         << pattern.little << ':' << forced_top << ':'
                         << prime_delta;
                if (!printed.insert(identity.str()).second) continue;
                ++full_matches;
                std::cout << "MATCH target=" << target.label
                          << " encoding=" << encoding.label
                          << " arithmetic="
                          << (arithmetic_little ? "little-port" :
                                                  "big-rsaref")
                          << " samples=" << samples << " weight=" << weight
                          << " initial_state="
                          << Hex(StoreInteger(initial + delta * weight,
                                              arithmetic_little))
                          << " stream_offset=" << start
                          << " integer_endian="
                          << (pattern.little ? "little" : "big")
                          << " lane=" << lanes[pattern.lane].label
                          << " forced_top=" << forced_top
                          << " prime_delta=" << prime_delta << '\n';
              }
            }
          }
        }
      }
    }
    std::cout << "completed encoding=" << encoding.label << '\n';
  }

  // Independent-execution KATs for recovered RSA-MD1 implementation.
  const Block want_empty = {0x7a,0xe2,0xa2,0x1d,0x87,0x93,0x65,0x00,
                            0x44,0xae,0x40,0x27,0x7e,0xaf,0xaa,0x2e};
  const Block want_abc = {0x74,0x59,0x4a,0x0a,0x73,0xb0,0x35,0x03,
                          0xef,0xe6,0x2e,0x57,0x4d,0xba,0xf8,0x16};
  const std::array<std::uint8_t, 3> abc = {'a','b','c'};
  if (rsa270::Md1::digest(std::span<const std::uint8_t>{}) != want_empty ||
      rsa270::Md1::digest(abc) != want_abc) {
    std::cerr << "recovered MD1 known-answer test failed\n";
    return 3;
  }

  std::cout << "summary model=RSAREF-additive-with-MD1"
            << " targets=" << targets.size()
            << " encodings=" << encodings.size()
            << " sample_counts=" << sample_counts.size()
            << " arithmetic_orders=2 stream_bytes=" << stream_bytes
            << " states=" << states_tested
            << " anchor_hits=" << anchor_hits
            << " full_matches=" << full_matches << '\n';
  return full_matches == 0 ? 0 : 1;
}
