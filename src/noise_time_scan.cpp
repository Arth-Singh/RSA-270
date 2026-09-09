#include <openssl/md5.h>

#include <gmpxx.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Block = std::array<unsigned char, 16>;

const mpz_class kFactor1(
    "37975227936943673922808872755445627854565536638199", 10);
const mpz_class kFactor2(
    "40094690950920881030683735292761468389214899724061", 10);

struct Target {
  unsigned digit_bits;
  mpz_class factor;
  Block mask{};
  Block expected{};
  unsigned compared_bits = 0;
};

void Md5(const unsigned char* input, size_t size, Block* output) {
  MD5(input, size, output->data());
}

Block Add(const Block& left, const Block& right) {
  Block result{};
  unsigned carry = 0;
  for (int i = 15; i >= 0; --i) {
    const unsigned sum = left[i] + right[i] + carry;
    result[i] = static_cast<unsigned char>(sum);
    carry = sum >> 8;
  }
  return result;
}

Block Subtract(const Block& left, const Block& right) {
  Block result{};
  int borrow = 0;
  for (int i = 15; i >= 0; --i) {
    int difference = static_cast<int>(left[i]) - right[i] - borrow;
    if (difference < 0) {
      difference += 256;
      borrow = 1;
    } else {
      borrow = 0;
    }
    result[i] = static_cast<unsigned char>(difference);
  }
  return result;
}

Block Multiply(Block value, unsigned multiplier) {
  Block result{};
  unsigned carry = 0;
  for (int i = 15; i >= 0; --i) {
    const unsigned product = value[i] * multiplier + carry;
    result[i] = static_cast<unsigned char>(product);
    carry = product >> 8;
  }
  return result;
}

void Increment(Block* value) {
  for (int i = 15; i >= 0; --i) {
    if (++(*value)[i] != 0) break;
  }
}

std::vector<unsigned char> EncodeWord(uint64_t value, unsigned bytes,
                                      bool little_endian) {
  std::vector<unsigned char> output(bytes);
  for (unsigned i = 0; i < bytes; ++i) {
    const unsigned index = little_endian ? i : bytes - 1 - i;
    output[index] = static_cast<unsigned char>(value >> (8 * i));
  }
  return output;
}

std::pair<std::vector<unsigned char>, std::vector<unsigned char>> Samples(
    const std::string& mode) {
  if (mode == "byte01") return {{0}, {1}};
  if (mode == "byteff") return {{0}, {255}};
  if (mode == "ascii01") return {{'0'}, {'1'}};
  for (unsigned bytes : {2u, 3u, 4u}) {
    const std::string prefix = "word" + std::to_string(bytes * 8);
    if (mode == prefix + "le")
      return {EncodeWord(0, bytes, true), EncodeWord(1, bytes, true)};
    if (mode == prefix + "be")
      return {EncodeWord(0, bytes, false), EncodeWord(1, bytes, false)};
  }
  std::fprintf(stderr, "unknown sample mode: %s\n", mode.c_str());
  std::exit(2);
}

std::vector<unsigned char> EncodeTime(uint64_t timestamp,
                                      const std::string& mode) {
  if (mode == "time32le") return EncodeWord(timestamp, 4, true);
  if (mode == "time32be") return EncodeWord(timestamp, 4, false);
  if (mode == "time64le") return EncodeWord(timestamp, 8, true);
  if (mode == "time64be") return EncodeWord(timestamp, 8, false);
  std::fprintf(stderr, "unknown time mode: %s\n", mode.c_str());
  std::exit(2);
}

Target MakeTarget(unsigned digit_bits, const mpz_class& factor) {
  constexpr unsigned kPrimeBits = 165;
  const unsigned modulus_digits = (330 + digit_bits - 1) / digit_bits;
  const unsigned prime_digits = (modulus_digits + 1) / 2;
  const unsigned buffer_bits = prime_digits * digit_bits;
  const unsigned padding = buffer_bits - (kPrimeBits - 2);
  Target target{digit_bits, factor};
  const unsigned last = std::min(127u, buffer_bits - 33);
  for (unsigned stream_bit = padding; stream_bit <= last; ++stream_bit) {
    const unsigned integer_bit = buffer_bits - 1 - stream_bit;
    const unsigned byte = stream_bit / 8;
    const unsigned bit = 7 - stream_bit % 8;
    target.mask[byte] |= static_cast<unsigned char>(1u << bit);
    if (mpz_tstbit(factor.get_mpz_t(), integer_bit))
      target.expected[byte] |= static_cast<unsigned char>(1u << bit);
    ++target.compared_bits;
  }
  return target;
}

bool PartialMatch(const Block& digest, const Target& target) {
  for (size_t i = 0; i < digest.size(); ++i) {
    if ((digest[i] & target.mask[i]) != target.expected[i]) return false;
  }
  return true;
}

mpz_class ImportBytes(const unsigned char* bytes, size_t size) {
  mpz_class value;
  mpz_import(value.get_mpz_t(), size, 1, 1, 1, 0, bytes);
  return value;
}

bool SameRecoverableInterior(const mpz_class& raw, const mpz_class& factor) {
  constexpr unsigned kPrimeBits = 165;
  const unsigned width = kPrimeBits - 34;
  mpz_class raw_interior = raw >> 32;
  mpz_fdiv_r_2exp(raw_interior.get_mpz_t(), raw_interior.get_mpz_t(), width);
  mpz_class factor_interior = factor >> 32;
  mpz_fdiv_r_2exp(factor_interior.get_mpz_t(), factor_interior.get_mpz_t(),
                  width);
  return raw_interior == factor_interior;
}

bool FullMatch(Block state, unsigned digit_bits) {
  const unsigned modulus_digits = (330 + digit_bits - 1) / digit_bits;
  const unsigned prime_digits = (modulus_digits + 1) / 2;
  const size_t candidate_bytes = prime_digits * digit_bits / 8;
  const size_t required = 2 * candidate_bytes;
  std::vector<unsigned char> stream;
  while (stream.size() < required) {
    Block digest;
    Md5(state.data(), state.size(), &digest);
    stream.insert(stream.end(), digest.begin(), digest.end());
    Increment(&state);
  }
  const mpz_class raw1 = ImportBytes(stream.data(), candidate_bytes);
  const mpz_class raw2 = ImportBytes(stream.data() + candidate_bytes,
                                     candidate_bytes);
  return (SameRecoverableInterior(raw1, kFactor1) &&
          SameRecoverableInterior(raw2, kFactor2)) ||
         (SameRecoverableInterior(raw1, kFactor2) &&
          SameRecoverableInterior(raw2, kFactor1));
}

void PrintBlock(const Block& block) {
  for (unsigned char byte : block) std::printf("%02x", byte);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 6 || argc > 8) {
    std::fprintf(stderr,
        "usage: %s START END TIME_MODE SAMPLE_MODE THREADS [needed|full] [BIT]\n",
        argv[0]);
    return 2;
  }
  const uint64_t begin = std::strtoull(argv[1], nullptr, 0);
  const uint64_t end = std::strtoull(argv[2], nullptr, 0);
  const std::string time_mode = argv[3];
  const std::string sample_mode = argv[4];
  unsigned threads = std::strtoul(argv[5], nullptr, 0);
  const bool needed = argc < 7 || std::strcmp(argv[6], "full") != 0;
  const unsigned comparator_bit = argc >= 8 ? std::strtoul(argv[7], nullptr, 0) : 0;
  if (begin >= end || threads == 0) return 2;

  auto [zero_sample, one_sample] = Samples(sample_mode);
  if (comparator_bit) {
    std::fill(one_sample.begin(), one_sample.end(), 0);
    const unsigned bit = comparator_bit - 1;
    if (bit >= 8 * one_sample.size()) return 2;
    // Numeric word encodings place bit zero according to their suffix.
    const bool little = sample_mode.size() >= 2 &&
                        sample_mode.substr(sample_mode.size() - 2) == "le";
    const unsigned byte = little ? bit / 8 : one_sample.size() - 1 - bit / 8;
    one_sample[byte] = static_cast<unsigned char>(1u << (bit % 8));
  }
  Block zero_digest, one_digest;
  Md5(zero_sample.data(), zero_sample.size(), &zero_digest);
  Md5(one_sample.data(), one_sample.size(), &one_digest);
  const size_t time_bytes = EncodeTime(begin, time_mode).size();
  const unsigned remaining = needed ? 256 - std::min<size_t>(256, time_bytes) : 256;
  const unsigned calls = (remaining + zero_sample.size() - 1) / zero_sample.size();
  const Block base = Multiply(zero_digest, calls);
  const Block delta = Subtract(one_digest, zero_digest);
  std::vector<Block> noise_states(calls + 1);
  noise_states[0] = base;
  for (unsigned ones = 1; ones <= calls; ++ones)
    noise_states[ones] = Add(noise_states[ones - 1], delta);

  std::vector<Target> targets;
  for (unsigned digit_bits : {16u, 24u, 32u, 64u}) {
    targets.push_back(MakeTarget(digit_bits, kFactor1));
    targets.push_back(MakeTarget(digit_bits, kFactor2));
  }
  for (size_t i = 0; i < targets.size(); i += 2) {
    std::printf("layout=%u compared_bits=%u\n", targets[i].digit_bits,
                targets[i].compared_bits);
  }

  std::atomic<bool> found{false};
  std::atomic<uint64_t> tested{0};
  const auto started = std::chrono::steady_clock::now();
  std::vector<std::thread> workers;
  for (unsigned thread = 0; thread < threads; ++thread) {
    const uint64_t first = begin + (end - begin) * thread / threads;
    const uint64_t last = begin + (end - begin) * (thread + 1) / threads;
    workers.emplace_back([&, first, last]() {
      uint64_t local_tested = 0;
      for (uint64_t timestamp = first;
           timestamp < last && !found.load(std::memory_order_relaxed);
           ++timestamp) {
        const std::vector<unsigned char> encoded = EncodeTime(timestamp, time_mode);
        Block time_digest;
        Md5(encoded.data(), encoded.size(), &time_digest);
        for (unsigned ones = 0; ones <= calls; ++ones) {
          const Block state = Add(time_digest, noise_states[ones]);
          Block output;
          Md5(state.data(), state.size(), &output);
          ++local_tested;
          for (const Target& target : targets) {
            if (PartialMatch(output, target) &&
                FullMatch(state, target.digit_bits)) {
              if (!found.exchange(true)) {
                std::printf("MATCH timestamp=%llu ones=%u calls=%u layout=%u state=",
                    static_cast<unsigned long long>(timestamp), ones, calls,
                    target.digit_bits);
                PrintBlock(state);
                std::printf("\n");
              }
            }
          }
        }
      }
      tested.fetch_add(local_tested, std::memory_order_relaxed);
    });
  }
  for (std::thread& worker : workers) worker.join();
  const double seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started).count();
  std::printf("result=%s calls=%u tested=%llu elapsed=%.3fs rate=%.3f M/s\n",
      found ? "match" : "none", calls,
      static_cast<unsigned long long>(tested.load()), seconds,
      tested.load() / seconds / 1e6);
  return found ? 0 : 1;
}
