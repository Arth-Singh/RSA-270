#include <gmpxx.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

struct Candidate {
  double saving;
  unsigned degree;
  unsigned multiplier;
  unsigned leading_probe;
  mpz_class base;
  std::vector<mpz_class> coefficients;
};

struct WorseFirst {
  bool operator()(const Candidate& left, const Candidate& right) const {
    return left.saving > right.saving;
  }
};

size_t BitLength(const mpz_class& value) {
  if (value == 0) return 0;
  mpz_class magnitude = value >= 0 ? value : -value;
  return mpz_sizeinbase(magnitude.get_mpz_t(), 2);
}

std::vector<mpz_class> BalancedExpansion(mpz_class value,
                                         const mpz_class& base) {
  std::vector<mpz_class> coefficients;
  while (value != 0) {
    mpz_class quotient, remainder;
    mpz_fdiv_qr(quotient.get_mpz_t(), remainder.get_mpz_t(),
                value.get_mpz_t(), base.get_mpz_t());
    if (2 * remainder > base) {
      remainder -= base;
      ++quotient;
    }
    coefficients.push_back(std::move(remainder));
    value = std::move(quotient);
  }
  return coefficients;
}

void Consider(std::priority_queue<Candidate, std::vector<Candidate>, WorseFirst>* best,
              Candidate candidate, size_t keep) {
  if (best->size() < keep) {
    best->push(std::move(candidate));
  } else if (candidate.saving > best->top().saving) {
    best->pop();
    best->push(std::move(candidate));
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 5) {
    std::cerr << "usage: " << argv[0]
              << " N [MAX_LEADING=100000] [MAX_MULTIPLIER=10] [KEEP=20]\n";
    return 2;
  }
  const mpz_class n(argv[1], 10);
  const unsigned max_leading = argc >= 3 ? std::strtoul(argv[2], nullptr, 10) : 100000;
  const unsigned max_multiplier = argc >= 4 ? std::strtoul(argv[3], nullptr, 10) : 10;
  const size_t keep = argc >= 5 ? std::strtoull(argv[4], nullptr, 10) : 20;
  std::priority_queue<Candidate, std::vector<Candidate>, WorseFirst> best;

  for (unsigned multiplier = 1; multiplier <= max_multiplier; ++multiplier) {
    const mpz_class target = n * multiplier;
    for (unsigned degree = 3; degree <= 12; ++degree) {
      mpz_class previous_base = -1;
      for (unsigned leading = 1; leading <= max_leading; ++leading) {
        mpz_class scaled = target / leading;
        mpz_class base;
        mpz_root(base.get_mpz_t(), scaled.get_mpz_t(), degree);
        for (unsigned add = 0; add <= 1; ++add) {
          mpz_class probe = base + add;
          if (probe <= 2 || (add == 0 && probe == previous_base)) continue;
          std::vector<mpz_class> coefficients = BalancedExpansion(target, probe);
          if (coefficients.size() != degree + 1) continue;
          size_t max_lower_bits = 0;
          for (size_t i = 0; i < degree; ++i)
            max_lower_bits = std::max(max_lower_bits, BitLength(coefficients[i]));
          const size_t base_bits = BitLength(probe);
          const double saving = static_cast<double>(base_bits) - max_lower_bits;
          Consider(&best, {saving, degree, multiplier, leading, std::move(probe),
                           std::move(coefficients)}, keep);
        }
        previous_base = std::move(base);
      }
    }
  }

  std::vector<Candidate> ordered;
  while (!best.empty()) {
    ordered.push_back(best.top());
    best.pop();
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const Candidate& a, const Candidate& b) {
              return a.saving > b.saving;
            });
  for (const Candidate& candidate : ordered) {
    std::cout << "saving_bits=" << candidate.saving
              << " degree=" << candidate.degree
              << " multiplier=" << candidate.multiplier
              << " leading_probe=" << candidate.leading_probe
              << " base=" << candidate.base << " coeffs=";
    for (size_t i = 0; i < candidate.coefficients.size(); ++i)
      std::cout << (i ? "," : "") << candidate.coefficients[i];
    std::cout << "\n";
  }
  return 0;
}
