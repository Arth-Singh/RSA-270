#include <gmpxx.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Sequence {
  std::string label;
  mpz_class bits;
  size_t width;
};

std::vector<std::string> SplitTabs(const std::string& line) {
  std::vector<std::string> fields;
  std::stringstream input(line);
  for (std::string field; std::getline(input, field, '\t');) {
    fields.push_back(std::move(field));
  }
  return fields;
}

std::vector<Sequence> ReadSequences(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open " + path);
  std::vector<Sequence> sequences;
  std::string line;
  std::getline(input, line);
  while (std::getline(input, line)) {
    const std::vector<std::string> fields = SplitTabs(line);
    if (fields.size() != 4) throw std::runtime_error("bad TSV row: " + line);
    for (size_t column : {size_t{2}, size_t{3}}) {
      mpz_class prime(fields[column], 10);
      const size_t prime_bits = mpz_sizeinbase(prime.get_mpz_t(), 2);
      if (prime_bits <= 34) throw std::runtime_error("factor too short");

      // RSAREF-style candidate construction fixes the top two bits. Prime
      // search changes the low end. Keep only recoverable random interior.
      const size_t width = prime_bits - 34;
      mpz_class interior = prime >> 32;
      mpz_fdiv_r_2exp(interior.get_mpz_t(), interior.get_mpz_t(), width);
      sequences.push_back({fields[0], std::move(interior), width});
    }
  }
  return sequences;
}

struct Packed {
  mpz_class bits;
  std::vector<mpz_class> masks;
  std::vector<size_t> samples;
  std::vector<std::pair<size_t, size_t>> slots;
};

Packed MakeLayout(const std::vector<Sequence>& sequences, size_t max_lag) {
  Packed packed;
  packed.masks.resize(max_lag + 1);
  packed.samples.assign(max_lag + 1, 0);
  size_t offset = 0;
  for (const Sequence& sequence : sequences) {
    packed.bits |= sequence.bits << offset;
    packed.slots.emplace_back(offset, sequence.width);
    for (size_t lag = 1; lag <= max_lag; ++lag) {
      if (sequence.width <= lag) continue;
      const size_t count = sequence.width - lag;
      mpz_class run = (mpz_class{1} << count) - 1;
      packed.masks[lag] |= run << offset;
      packed.samples[lag] += count;
    }
    offset += sequence.width + max_lag;
  }
  return packed;
}

double ZScore(const mpz_class& packed_bits, const Packed& layout, size_t lag) {
  mpz_class differing = (packed_bits ^ (packed_bits >> lag)) & layout.masks[lag];
  const size_t differences = mpz_popcount(differing.get_mpz_t());
  const double n = static_cast<double>(layout.samples[lag]);
  return (n - 2.0 * differences) / std::sqrt(n);
}

mpz_class RandomPacked(const Packed& layout, gmp_randstate_t random) {
  mpz_class result;
  for (const auto& [offset, width] : layout.slots) {
    mpz_class value;
    mpz_urandomb(value.get_mpz_t(), random, width);
    result |= value << offset;
  }
  return result;
}

void PrintObserved(const std::vector<Sequence>& sequences, size_t max_lag,
                   size_t min_samples) {
  const Packed layout = MakeLayout(sequences, max_lag);
  std::vector<std::pair<double, size_t>> ranked;
  for (size_t lag = 1; lag <= max_lag; ++lag) {
    if (layout.samples[lag] >= min_samples) {
      ranked.emplace_back(std::abs(ZScore(layout.bits, layout, lag)), lag);
    }
  }
  std::sort(ranked.rbegin(), ranked.rend());
  std::cout << "observed sequences=" << sequences.size()
            << " lag64_z=" << ZScore(layout.bits, layout, 64)
            << " lag64_n=" << layout.samples[64] << " top=";
  for (size_t i = 0; i < std::min<size_t>(10, ranked.size()); ++i) {
    const size_t lag = ranked[i].second;
    std::cout << (i ? "," : "") << lag << ":"
              << ZScore(layout.bits, layout, lag);
  }
  std::cout << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  const std::string path = argc >= 2 ? argv[1] : "data/solved_decimal_challenge.tsv";
  const size_t trials = argc >= 3 ? std::strtoull(argv[2], nullptr, 10) : 20000;
  constexpr size_t kMaxLag = 256;
  constexpr size_t kMinSamples = 1000;

  std::vector<Sequence> sequences = ReadSequences(path);
  std::cout << std::fixed << std::setprecision(6);
  PrintObserved(sequences, kMaxLag, kMinSamples);
  PrintObserved(std::vector<Sequence>(sequences.begin(), sequences.begin() + 16),
                kMaxLag, kMinSamples);
  PrintObserved(std::vector<Sequence>(sequences.begin() + 16, sequences.end()),
                kMaxLag, kMinSamples);

  const Packed layout = MakeLayout(sequences, kMaxLag);
  const double observed64 = std::abs(ZScore(layout.bits, layout, 64));
  double observed_max = 0.0;
  for (size_t lag = 1; lag <= kMaxLag; ++lag) {
    if (layout.samples[lag] >= kMinSamples) {
      observed_max = std::max(observed_max,
                              std::abs(ZScore(layout.bits, layout, lag)));
    }
  }

  gmp_randstate_t random;
  gmp_randinit_mt(random);
  gmp_randseed_ui(random, 0x2702026u);
  size_t exceed64 = 0;
  size_t exceed_max = 0;
  std::vector<double> maxima;
  maxima.reserve(trials);
  for (size_t trial = 0; trial < trials; ++trial) {
    const mpz_class sample = RandomPacked(layout, random);
    if (std::abs(ZScore(sample, layout, 64)) >= observed64) ++exceed64;
    double maximum = 0.0;
    for (size_t lag = 1; lag <= kMaxLag; ++lag) {
      if (layout.samples[lag] >= kMinSamples) {
        maximum = std::max(maximum, std::abs(ZScore(sample, layout, lag)));
      }
    }
    if (maximum >= observed_max) ++exceed_max;
    maxima.push_back(maximum);
  }
  std::sort(maxima.begin(), maxima.end());
  auto quantile = [&](double probability) {
    const size_t index = std::min(trials - 1,
        static_cast<size_t>(probability * static_cast<double>(trials)));
    return maxima[index];
  };
  std::cout << "null trials=" << trials
            << " fixed64_p=" << static_cast<double>(exceed64 + 1) / (trials + 1)
            << " familywise_p=" << static_cast<double>(exceed_max + 1) / (trials + 1)
            << " observed_max=" << observed_max
            << " max_q50=" << quantile(0.50)
            << " max_q95=" << quantile(0.95)
            << " max_q99=" << quantile(0.99) << "\n";
  gmp_randclear(random);
  return 0;
}
