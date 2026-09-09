#include <gmpxx.h>
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
#include <utility>
#include <vector>

// Low-entropy state scanner for the *modeled* continuous RSAREF generator:
//
//   output_i = MD5(state_i)
//
// raw_r_random.c advances state with the unusual postincrement expression
//     if (state[15-i]++) break;
// rather than an ordinary big-endian increment.  Both literal "post" and
// likely-intended "pre" transitions are tested.  Exact 128-bit anchors come
// from rsaref_stream_reconstruct.py and are independent of equal-bit p/q order.
// A hit is checked against every deterministic anchor, then used to construct
// and verify both modeled RSA-270 factors.

namespace {

using Block = std::array<unsigned char, 16>;

enum class Transition { kPostincrement, kPreincrement };

struct Anchor {
  unsigned index;
  Block digest;
};

struct WidthModel {
  unsigned digit_bits;
  std::size_t target_offset;
  std::size_t target_block_bytes;
  std::vector<Anchor> anchors;
};

Block Hex(const char* input) {
  Block result{};
  if (std::strlen(input) != 32) std::abort();
  for (unsigned i = 0; i < result.size(); ++i) {
    unsigned value = 0;
    if (std::sscanf(input + 2 * i, "%2x", &value) != 1) std::abort();
    result[i] = static_cast<unsigned char>(value);
  }
  return result;
}

std::vector<WidthModel> Models() {
  return {
      {16, 1292, 56,
       {{6, Hex("34a7afe5eb24bc33197ddf8fe4c19811")},
        {13, Hex("7579673841602492cf7bae2c24b27021")},
        {15, Hex("1547ef43e50089228a4f116da694d9c4")},
        {25, Hex("261e6cfab566e3aab73bb44f6a303795")},
        {27, Hex("effdbcc195a1e5deec8a2fe2273657fb")},
        {34, Hex("1adcee7d7d9f02f0c4469c32a5307426")},
        {35, Hex("1b4ad657a864d5ab589201a6c9ad3f04")},
        {37, Hex("f162d3ae871a95e6a364c080b7f59939")},
        {39, Hex("8086d4687fa9845a21107b3afb857f7c")},
        {40, Hex("35e7eca277182aef8d3e8823108ed132")},
        {42, Hex("39a49136d61fa92197ac5fda19364064")},
        {50, Hex("248737a533a7d4c1ecf14e62b479bada")},
        {51, Hex("e55ccf23817256172773cd9f85afd061")},
        {53, Hex("a941801dfcf3b7cdc0bef9486e3b8e53")},
        {54, Hex("773d8567864932f6fbe58a9879b9bf81")},
        {62, Hex("053fb8fcb32c5ae43e828c5ca17cfc73")},
        {63, Hex("3df5f334c07af48e07706c091e9cff97")},
        {65, Hex("f13ccb7b34a2df26e3c1ee75f02aa4c9")},
        {66, Hex("3c1151f9ea6cdd12e337d679ce686a0c")},
        {68, Hex("616b4378f6bd991292cb2f21c10d06c5")},
        {69, Hex("e8e571a5e962b7e82dfd9fe7120f6d03")},
        {71, Hex("4b211e3bfdb54f680e5c04528aaa2042")},
        {72, Hex("8ae008faf48df6c913f5747d8608a5a4")}}},
      {24, 1314, 57,
       {{6, Hex("a7afe5eb24bc33197ddf8fe4c1981182")},
        {13, Hex("673841602492cf7bae2c24b27021853d")},
        {15, Hex("ef43e50089228a4f116da694d9c4cd85")},
        {25, Hex("e474398a261e6cfab566e3aab73bb44f")},
        {28, Hex("273657fb24c9bde1effca93482d90169")},
        {35, Hex("c4469c32a53074261b4ad657a864d5ab")},
        {37, Hex("fb1a4868708b1c03df4af162d3ae871a")},
        {38, Hex("95e6a364c080b7f5993906c2bff55ac7")},
        {40, Hex("845a21107b3afb857f7c35e7eca27718")},
        {43, Hex("a92197ac5fda19364064129dcbb19f5c")},
        {51, Hex("37a533a7d4c1ecf14e62b479badae55c")},
        {52, Hex("cf23817256172773cd9f85afd0612903")},
        {54, Hex("a941801dfcf3b7cdc0bef9486e3b8e53")},
        {55, Hex("773d8567864932f6fbe58a9879b9bf81")},
        {63, Hex("ee053fb8fcb32c5ae43e828c5ca17cfc")},
        {64, Hex("733df5f334c07af48e07706c091e9cff")},
        {66, Hex("27bcf13ccb7b34a2df26e3c1ee75f02a")},
        {67, Hex("a4c93c1151f9ea6cdd12e337d679ce68")},
        {69, Hex("1f41fdd7616b4378f6bd991292cb2f21")},
        {70, Hex("c10d06c5e8e571a5e962b7e82dfd9fe7")},
        {73, Hex("04528aaa20428ae008faf48df6c913f5")},
        {74, Hex("747d8608a5a48e2bfe41fae7a04683f2")}}},
      {32, 1328, 56,
       {{8, Hex("28ff76073161f57df3e24286ab153158")},
        {26, Hex("261e6cfab566e3aab73bb44f6a303795")},
        {28, Hex("effdbcc195a1e5deec8a2fe2273657fb")},
        {36, Hex("a53074261b4ad657a864d5ab589201a6")},
        {38, Hex("1c03df4af162d3ae871a95e6a364c080")},
        {41, Hex("7b3afb857f7c35e7eca277182aef8d3e")},
        {43, Hex("c8cb3dbe4d62c32a39a49136d61fa921")},
        {44, Hex("97ac5fda19364064129dcbb19f5c43f6")},
        {52, Hex("d4c1ecf14e62b479badae55ccf238172")},
        {55, Hex("fcf3b7cdc0bef9486e3b8e53773d8567")},
        {64, Hex("b8fcb32c5ae43e828c5ca17cfc733df5")},
        {65, Hex("f334c07af48e07706c091e9cff97e5c9")},
        {67, Hex("f13ccb7b34a2df26e3c1ee75f02aa4c9")},
        {68, Hex("3c1151f9ea6cdd12e337d679ce686a0c")},
        {70, Hex("616b4378f6bd991292cb2f21c10d06c5")},
        {71, Hex("e8e571a5e962b7e82dfd9fe7120f6d03")},
        {73, Hex("4b211e3bfdb54f680e5c04528aaa2042")},
        {74, Hex("8ae008faf48df6c913f5747d8608a5a4")}}},
  };
}

void Md5(const unsigned char* input, std::size_t size, Block* output) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  MD5(input, size, output->data());
#pragma clang diagnostic pop
}

void Step(Block* state, Transition transition) {
  if (transition == Transition::kPreincrement) {
    for (int i = 15; i >= 0; --i) {
      if (++(*state)[i] != 0) break;
    }
  } else {
    for (int i = 15; i >= 0; --i) {
      const unsigned char previous = (*state)[i]++;
      if (previous != 0) break;
    }
  }
}

Block Digest(const Block& state) {
  Block output;
  Md5(state.data(), state.size(), &output);
  return output;
}

bool Verify(Block state, Transition transition, const WidthModel& model) {
  std::size_t anchor = 0;
  for (unsigned index = 0; index <= model.anchors.back().index; ++index) {
    if (index == model.anchors[anchor].index) {
      if (Digest(state) != model.anchors[anchor].digest) return false;
      ++anchor;
      if (anchor == model.anchors.size()) return true;
    }
    Step(&state, transition);
  }
  return false;
}

std::vector<std::pair<std::string, Block>> InitialStates(
    const std::string& mode, std::uint32_t candidate, unsigned word_bytes) {
  if (word_bytes == 0 || word_bytes > 4) std::abort();
  std::vector<std::pair<std::string, Block>> result;
  for (const bool little : {false, true}) {
    std::array<unsigned char, 4> word{};
    for (unsigned i = 0; i < word_bytes; ++i) {
      const unsigned at = little ? i : word_bytes - 1 - i;
      word[at] = static_cast<unsigned char>(candidate >> (8 * i));
    }
    const std::string suffix = little ? "-le" : "-be";
    if (mode == "raw-word") {
      for (unsigned position = 0; position + word_bytes <= 16; ++position) {
        Block state{};
        std::copy_n(word.begin(), word_bytes, state.begin() + position);
        result.push_back({mode + suffix + "-at" + std::to_string(position), state});
      }
    } else if (mode == "raw-repeat-word") {
      Block state{};
      for (unsigned position = 0; position < 16; ++position)
        state[position] = word[position % word_bytes];
      result.push_back({mode + suffix, state});
    } else if (mode == "md5-word" || mode == "md5-word-times64") {
      Block state;
      Md5(word.data(), word_bytes, &state);
      if (mode == "md5-word-times64") {
        Block multiplied{};
        unsigned carry = 0;
        for (int i = 15; i >= 0; --i) {
          const unsigned product = 64u * state[i] + carry;
          multiplied[i] = static_cast<unsigned char>(product);
          carry = product >> 8;
        }
        state = multiplied;
      }
      result.push_back({mode + suffix, state});
    } else if (mode == "md5-repeat-buffer") {
      std::array<unsigned char, 256> buffer{};
      for (unsigned i = 0; i < buffer.size(); ++i)
        buffer[i] = word[i % word_bytes];
      Block state;
      Md5(buffer.data(), buffer.size(), &state);
      result.push_back({mode + suffix, state});
    } else if (mode == "ansi-rand" || mode == "ms-rand" ||
               mode == "borland-rand") {
      // Endianness does not affect these families; emit once.
      if (little) continue;
      std::array<unsigned char, 256> buffer{};
      std::uint32_t lcg = candidate;
      for (auto& byte : buffer) {
        if (mode == "ansi-rand") lcg = lcg * 1103515245u + 12345u;
        if (mode == "ms-rand") lcg = lcg * 214013u + 2531011u;
        if (mode == "borland-rand") lcg = lcg * 22695477u + 1u;
        byte = static_cast<unsigned char>((lcg >> 16) & 0x7fffu);
      }
      Block state;
      Md5(buffer.data(), buffer.size(), &state);
      result.push_back({mode, state});
    } else {
      std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
      std::exit(2);
    }
  }
  return result;
}

bool Probe(const Block& initial, Transition transition,
           const std::vector<WidthModel>& models, unsigned* matched_width) {
  Block state = initial;
  for (unsigned index = 0; index <= 8; ++index) {
    if (index == 6) {
      const Block digest = Digest(state);
      for (unsigned model_index : {0u, 1u}) {
        if (digest == models[model_index].anchors.front().digest &&
            Verify(initial, transition, models[model_index])) {
          *matched_width = models[model_index].digit_bits;
          return true;
        }
      }
    }
    if (index == 8) {
      const Block digest = Digest(state);
      if (digest == models[2].anchors.front().digest &&
          Verify(initial, transition, models[2])) {
        *matched_width = 32;
        return true;
      }
    }
    Step(&state, transition);
  }
  return false;
}

mpz_class Import(const unsigned char* data, std::size_t size) {
  mpz_class result;
  mpz_import(result.get_mpz_t(), size, 1, 1, 1, 0, data);
  return result;
}

bool Accepted(const mpz_class& value) {
  if (mpz_fdiv_ui(value.get_mpz_t(), 3) != 2) return false;
  for (unsigned divisor : {5u, 7u, 11u})
    if (mpz_divisible_ui_p(value.get_mpz_t(), divisor)) return false;
  mpz_class result;
  mpz_powm(result.get_mpz_t(), mpz_class(2).get_mpz_t(), value.get_mpz_t(),
           value.get_mpz_t());
  return result == 2;
}

mpz_class FindPrime(const mpz_class& raw, unsigned bits) {
  const mpz_class range = mpz_class(1) << (bits - 2);
  const mpz_class base = 3 * range;
  mpz_class candidate = base + raw % range;
  if (mpz_even_p(candidate.get_mpz_t())) ++candidate;
  const mpz_class upper = (mpz_class(1) << bits) - 2;
  while (!Accepted(candidate)) {
    if (candidate > upper) candidate -= range;
    candidate += 2;
  }
  return candidate;
}

void PrintBlock(const Block& block) {
  for (const auto byte : block) std::printf("%02x", byte);
}

bool BuildRsa270(Block state, Transition transition, const WidthModel& model) {
  const std::size_t end = model.target_offset + 2 * model.target_block_bytes;
  std::vector<unsigned char> stream;
  stream.reserve(end);
  while (stream.size() < end) {
    const Block digest = Digest(state);
    stream.insert(stream.end(), digest.begin(), digest.end());
    Step(&state, transition);
  }
  const mpz_class raw1 = Import(stream.data() + model.target_offset,
                                model.target_block_bytes);
  const mpz_class raw2 = Import(stream.data() + model.target_offset +
                                    model.target_block_bytes,
                                model.target_block_bytes);
  const mpz_class p = FindPrime(raw1, 448);
  const mpz_class q = FindPrime(raw2, 447);
  const mpz_class target(
      "233108530344407544527637656910680524145619812480305449042948611968495918245135782867888369318577116418213919268572658314913060672626911354027609793166341626693946596196427744273886601876896313468704059066746903123910748277606548649151920812699309766587514735456594993207",
      10);
  std::printf("RSA270 digit_bits=%u p=%s q=%s product_match=%s\n",
              model.digit_bits, p.get_str().c_str(), q.get_str().c_str(),
              p * q == target ? "yes" : "no");
  return p * q == target;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5 || argc > 6) {
    std::fprintf(stderr,
                 "usage: %s MODE BEGIN END THREADS [WORD_BYTES]\n"
                 "modes: raw-word raw-repeat-word md5-word md5-word-times64 "
                 "md5-repeat-buffer ansi-rand ms-rand borland-rand\n",
                 argv[0]);
    return 2;
  }
  const std::string mode = argv[1];
  const std::uint64_t begin = std::strtoull(argv[2], nullptr, 0);
  const std::uint64_t end = std::strtoull(argv[3], nullptr, 0);
  unsigned threads = std::strtoul(argv[4], nullptr, 0);
  const unsigned word_bytes = argc == 6 ? std::strtoul(argv[5], nullptr, 0) : 4;
  if (begin >= end || end > (std::uint64_t{1} << 32) || threads == 0 ||
      word_bytes == 0 || word_bytes > 4 ||
      (word_bytes < 4 && end > (std::uint64_t{1} << (8 * word_bytes)))) {
    std::fprintf(stderr, "invalid range/thread/word width\n");
    return 2;
  }

  const auto models = Models();
  std::atomic<bool> found{false};
  std::atomic<std::uint64_t> tested_variants{0};
  const auto started = std::chrono::steady_clock::now();
  std::vector<std::thread> workers;
  for (unsigned thread = 0; thread < threads; ++thread) {
    const std::uint64_t first = begin + (end - begin) * thread / threads;
    const std::uint64_t last = begin + (end - begin) * (thread + 1) / threads;
    workers.emplace_back([&, first, last]() {
      std::uint64_t local = 0;
      for (std::uint64_t candidate = first;
           candidate < last && !found.load(std::memory_order_relaxed);
           ++candidate) {
        const auto states = InitialStates(mode, static_cast<std::uint32_t>(candidate),
                                          word_bytes);
        for (const auto& named_state : states) {
          for (const auto transition : {Transition::kPostincrement,
                                        Transition::kPreincrement}) {
            ++local;
            unsigned width = 0;
            if (!Probe(named_state.second, transition, models, &width)) continue;
            found.store(true, std::memory_order_relaxed);
            std::printf("MATCH candidate=%llu family=%s transition=%s width=%u state=",
                        static_cast<unsigned long long>(candidate),
                        named_state.first.c_str(),
                        transition == Transition::kPostincrement ? "post" : "pre",
                        width);
            PrintBlock(named_state.second);
            std::printf("\n");
            const auto model = std::find_if(models.begin(), models.end(),
                [width](const WidthModel& item) { return item.digit_bits == width; });
            BuildRsa270(named_state.second, transition, *model);
          }
        }
      }
      tested_variants.fetch_add(local, std::memory_order_relaxed);
    });
  }
  for (auto& worker : workers) worker.join();
  const double seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started).count();
  std::printf("summary mode=%s candidates=%llu tested_state_transition_variants=%llu"
              " result=%s elapsed=%.3f rate_Mvariants_s=%.3f\n",
              mode.c_str(), static_cast<unsigned long long>(end - begin),
              static_cast<unsigned long long>(tested_variants.load()),
              found ? "match" : "none", seconds,
              tested_variants.load() / seconds / 1e6);
  return found ? 0 : 1;
}
