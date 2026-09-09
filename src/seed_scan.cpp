#include <openssl/md5.h>

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
#include <vector>

namespace {

using Block = std::array<unsigned char, 16>;

constexpr Block kDefaultTargetSmallFirst = {
    0x56, 0x23, 0xf9, 0xae, 0x32, 0xfb, 0x14, 0xa8,
    0x9c, 0x90, 0x85, 0x24, 0xe9, 0x91, 0xfb, 0x7c,
};
constexpr Block kDefaultTargetLargeFirst = {
    0xfc, 0xd0, 0x70, 0x7b, 0x67, 0x5d, 0x3a, 0x95,
    0x68, 0x4c, 0xaa, 0xfe, 0xc8, 0x3b, 0x1e, 0xd6,
};

enum class Mode {
  kRaw32,
  kMd5Word,
  kMd5WordTimes64,
  kMd5RepeatedWordBuffer,
  kAnsiRandBuffer,
  kMicrosoftRandBuffer,
  kBorlandRandBuffer,
};

struct Config {
  Mode mode;
  uint64_t begin;
  uint64_t end;
  unsigned threads;
  uint64_t counter_offset;
  bool little_endian;
  bool postincrement_counter;
  std::vector<Block> targets;
};

void PutWord(unsigned char* out, uint32_t value, bool little_endian) {
  for (unsigned i = 0; i < 4; ++i) {
    const unsigned shift = little_endian ? 8 * i : 8 * (3 - i);
    out[i] = static_cast<unsigned char>(value >> shift);
  }
}

void AddBigEndian(Block* value, uint64_t increment) {
  unsigned carry = 0;
  for (int i = 15; i >= 0; --i) {
    const unsigned add = static_cast<unsigned>(increment & 0xffu);
    increment >>= 8;
    const unsigned sum = (*value)[i] + add + carry;
    (*value)[i] = static_cast<unsigned char>(sum);
    carry = sum >> 8;
  }
}

// RSAREF 1.0/2.0 source uses postfix increment in its carry test:
//   if (state[15-i]++) break;
// This transition is conjugate to ordinary addition after subtracting one
// independently from each byte.  Support both source-exact and intended
// preincrement-counter interpretations.
void AdvanceCounter(Block* value, uint64_t increment, bool postincrement) {
  if (!postincrement) {
    AddBigEndian(value, increment);
    return;
  }
  for (auto& byte : *value) --byte;
  AddBigEndian(value, increment);
  for (auto& byte : *value) ++byte;
}

void MultiplyBigEndian(Block* value, unsigned multiplier) {
  unsigned carry = 0;
  for (int i = 15; i >= 0; --i) {
    const unsigned product = (*value)[i] * multiplier + carry;
    (*value)[i] = static_cast<unsigned char>(product);
    carry = product >> 8;
  }
}

void Md5(const unsigned char* input, size_t size, Block* output) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  MD5(input, size, output->data());
#pragma clang diagnostic pop
}

uint32_t NextRand(uint32_t* state, Mode mode) {
  switch (mode) {
    case Mode::kAnsiRandBuffer:
      *state = *state * 1103515245u + 12345u;
      break;
    case Mode::kMicrosoftRandBuffer:
      *state = *state * 214013u + 2531011u;
      break;
    case Mode::kBorlandRandBuffer:
      *state = *state * 22695477u + 1u;
      break;
    default:
      std::abort();
  }
  return (*state >> 16) & 0x7fffu;
}

Block InitialState(uint32_t candidate, const Config& config) {
  Block state{};
  unsigned char word[4];
  PutWord(word, candidate, config.little_endian);

  switch (config.mode) {
    case Mode::kRaw32:
      std::copy(word, word + 4, state.end() - 4);
      return state;
    case Mode::kMd5Word:
      Md5(word, sizeof(word), &state);
      return state;
    case Mode::kMd5WordTimes64:
      Md5(word, sizeof(word), &state);
      MultiplyBigEndian(&state, 64);
      return state;
    case Mode::kMd5RepeatedWordBuffer: {
      unsigned char buffer[256];
      for (size_t i = 0; i < sizeof(buffer); i += sizeof(word)) {
        std::memcpy(buffer + i, word, sizeof(word));
      }
      Md5(buffer, sizeof(buffer), &state);
      return state;
    }
    case Mode::kAnsiRandBuffer:
    case Mode::kMicrosoftRandBuffer:
    case Mode::kBorlandRandBuffer: {
      unsigned char buffer[256];
      uint32_t random_state = candidate;
      for (unsigned char& byte : buffer) {
        byte = static_cast<unsigned char>(NextRand(&random_state, config.mode));
      }
      Md5(buffer, sizeof(buffer), &state);
      return state;
    }
  }
  std::abort();
}

bool Matches(const Block& digest, const Config& config) {
  return std::find(config.targets.begin(), config.targets.end(), digest) !=
         config.targets.end();
}

Block ParseBlock(const char* text) {
  if (std::strlen(text) != 32) {
    std::fprintf(stderr, "target must contain exactly 32 hex digits: %s\n", text);
    std::exit(2);
  }
  Block result{};
  for (unsigned i = 0; i < result.size(); ++i) {
    unsigned value = 0;
    if (std::sscanf(text + 2 * i, "%2x", &value) != 1) {
      std::fprintf(stderr, "invalid target hex: %s\n", text);
      std::exit(2);
    }
    result[i] = static_cast<unsigned char>(value);
  }
  return result;
}

void PrintBlock(const Block& block) {
  for (unsigned char byte : block) std::printf("%02x", byte);
}

Mode ParseMode(const std::string& mode) {
  if (mode == "raw32") return Mode::kRaw32;
  if (mode == "md5-word") return Mode::kMd5Word;
  if (mode == "md5-word-times64") return Mode::kMd5WordTimes64;
  if (mode == "md5-repeat-word") return Mode::kMd5RepeatedWordBuffer;
  if (mode == "ansi-rand") return Mode::kAnsiRandBuffer;
  if (mode == "ms-rand") return Mode::kMicrosoftRandBuffer;
  if (mode == "borland-rand") return Mode::kBorlandRandBuffer;
  std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
  std::exit(2);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr,
                 "usage: %s MODE BEGIN END [THREADS] [OFFSET] "
                 "[le|be|post-le|post-be] "
                 "[TARGET_HEX ...]\n",
                 argv[0]);
    return 2;
  }

  Config config{
      .mode = ParseMode(argv[1]),
      .begin = std::strtoull(argv[2], nullptr, 0),
      .end = std::strtoull(argv[3], nullptr, 0),
      .threads = argc >= 5 ? static_cast<unsigned>(std::strtoul(argv[4], nullptr, 0))
                           : std::thread::hardware_concurrency(),
      .counter_offset = argc >= 6 ? std::strtoull(argv[5], nullptr, 0) : 77,
      .little_endian = argc < 7 || std::strstr(argv[6], "be") == nullptr,
      .postincrement_counter = argc >= 7 &&
                               std::strstr(argv[6], "post") != nullptr,
      .targets = {},
  };
  for (int i = 7; i < argc; ++i) config.targets.push_back(ParseBlock(argv[i]));
  if (config.targets.empty()) {
    config.targets = {kDefaultTargetSmallFirst, kDefaultTargetLargeFirst};
  }
  if (config.threads == 0) config.threads = 1;
  if (config.end <= config.begin || config.end > (uint64_t{1} << 32)) {
    std::fprintf(stderr, "range must satisfy 0 <= BEGIN < END <= 2^32\n");
    return 2;
  }

  std::atomic<bool> found{false};
  std::atomic<uint64_t> match{0};
  const auto started = std::chrono::steady_clock::now();
  std::vector<std::thread> workers;
  for (unsigned thread = 0; thread < config.threads; ++thread) {
    const uint64_t span = config.end - config.begin;
    const uint64_t first = config.begin + span * thread / config.threads;
    const uint64_t last = config.begin + span * (thread + 1) / config.threads;
    workers.emplace_back([&, first, last]() {
      for (uint64_t value = first; value < last && !found.load(std::memory_order_relaxed);
           ++value) {
        Block state = InitialState(static_cast<uint32_t>(value), config);
        AdvanceCounter(&state, config.counter_offset,
                       config.postincrement_counter);
        Block digest;
        Md5(state.data(), state.size(), &digest);
        if (Matches(digest, config)) {
          match.store(value, std::memory_order_relaxed);
          found.store(true, std::memory_order_relaxed);
          std::printf("match candidate=%llu state=",
                      static_cast<unsigned long long>(value));
          PrintBlock(state);
          std::printf(" digest=");
          PrintBlock(digest);
          std::printf("\n");
        }
      }
    });
  }
  for (std::thread& worker : workers) worker.join();

  const double seconds = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - started)
                             .count();
  const double tested = found ? static_cast<double>(match - config.begin + 1)
                              : static_cast<double>(config.end - config.begin);
  std::printf("result=%s elapsed=%.3fs nominal_rate=%.3f M/s\n",
              found ? "match" : "none", seconds, tested / seconds / 1e6);
  return found ? 0 : 1;
}
