// Direct matched Monte Carlo for the conditional old-RSAREF gap model.
// This tests a statistical construction model, not historical attribution.
#include <gmpxx.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct Counters {
  uint64_t candidates = 0, fermat = 0, wraps = 0;
};

// The caller visits odd candidates. The rejection order is optimized without
// changing raw_prime-old.c's predicate: small primes 3,5,7,11; 2^n mod n == 2;
// and gcd(n-1, 3) == 1. In particular, this is not a modern primality test.
bool accepts(const mpz_class& n, Counters& counts) {
  ++counts.candidates;
  if (mpz_even_p(n.get_mpz_t()) || n < 3 ||
      mpz_fdiv_ui(n.get_mpz_t(), 3) != 2 ||
      mpz_divisible_ui_p(n.get_mpz_t(), 5) ||
      mpz_divisible_ui_p(n.get_mpz_t(), 7) ||
      mpz_divisible_ui_p(n.get_mpz_t(), 11)) return false;
  ++counts.fermat;
  mpz_class residue, two = 2;
  mpz_powm(residue.get_mpz_t(), two.get_mpz_t(), n.get_mpz_t(), n.get_mpz_t());
  return residue == 2;
}

struct Range {
  unsigned bits;
  mpz_class width, low, high;
  explicit Range(unsigned b) : bits(b), width(mpz_class(1) << (b-2)),
      low(3 * width), high(4 * width - 1) {
    if (b < 5) throw std::runtime_error("bit length must be at least 5");
  }
};

mpz_class increment(const mpz_class& x, const Range& range, Counters& counts) {
  if (x == range.high) {
    ++counts.wraps;
    return range.low + 1;
  }
  return x + 2;
}

mpz_class decrement(const mpz_class& x, const Range& range, Counters& counts) {
  if (x == range.low + 1) {
    ++counts.wraps;
    return range.high;
  }
  return x - 2;
}

mpz_class predecessor(const mpz_class& prime, const Range& range,
                      Counters& counts) {
  mpz_class candidate = decrement(prime, range, counts);
  while (!accepts(candidate, counts))
    candidate = decrement(candidate, range, counts);
  return candidate;
}

mpz_class gap(const mpz_class& prime, const mpz_class& previous,
              const Range& range) {
  mpz_class answer = prime - previous;
  if (answer <= 0) answer += range.width;
  return answer;
}

mpz_class upward(mpz_class candidate, const Range& range, Counters& counts) {
  while (!accepts(candidate, counts))
    candidate = increment(candidate, range, counts);
  return candidate;
}

// mt19937_64's output sequence is standardized. Importing explicitly ordered
// words and masking bits avoids implementation-dependent integer distributions.
mpz_class raw_residue(std::mt19937_64& random, unsigned bits) {
  std::vector<uint64_t> words((bits + 63) / 64);
  for (auto& word : words) word = random();
  mpz_class value;
  mpz_import(value.get_mpz_t(), words.size(), -1, sizeof(uint64_t), 0, 0,
             words.data());
  mpz_fdiv_r_2exp(value.get_mpz_t(), value.get_mpz_t(), bits);
  return value;
}

mpz_class odd_candidate(std::mt19937_64& random, const Range& range) {
  mpz_class candidate = range.low + raw_residue(random, range.bits - 2);
  mpz_setbit(candidate.get_mpz_t(), 0);
  return candidate;
}

mpz_class draw(std::mt19937_64& random, const Range& range, bool scan,
               Counters& counts) {
  // Every odd candidate has exactly two raw preimages. Rejection sampling here
  // therefore gives equal mass to every predicate-accepted value in the range.
  // Sampling a random start and taking its successor would instead weight gaps.
  if (scan) return upward(odd_candidate(random, range), range, counts);
  for (;;) {
    mpz_class candidate = odd_candidate(random, range);
    if (accepts(candidate, counts)) return candidate;
  }
}

uint64_t splitmix64(uint64_t x) {
  x += UINT64_C(0x9e3779b97f4a7c15);
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  return x ^ (x >> 31);
}

struct Factor {
  std::string label;
  unsigned index;
  mpz_class value;
  Range range;
  Factor(std::string label_, unsigned index_, mpz_class value_)
      : label(std::move(label_)), index(index_), value(std::move(value_)),
        range(mpz_sizeinbase(value.get_mpz_t(), 2)) {}
};

std::vector<Factor> load_factors(const std::string& input) {
  std::ifstream file(input);
  if (!file) throw std::runtime_error("cannot open dataset: " + input);
  std::string line;
  std::getline(file, line);
  if (line != "label\tmodulus\tfactor1\tfactor2")
    throw std::runtime_error("unexpected dataset header");
  std::vector<Factor> result;
  unsigned rows = 0;
  while (std::getline(file, line)) {
    if (line.empty()) continue;
    std::istringstream stream(line);
    std::string label, n_text, p_text, q_text;
    stream >> label >> n_text >> p_text >> q_text;
    mpz_class n(n_text), p(p_text), q(q_text);
    if (p*q != n) throw std::runtime_error("incorrect product: " + label);
    const unsigned b = mpz_sizeinbase(n.get_mpz_t(), 2);
    std::vector<unsigned> actual = {
        static_cast<unsigned>(mpz_sizeinbase(p.get_mpz_t(), 2)),
        static_cast<unsigned>(mpz_sizeinbase(q.get_mpz_t(), 2))};
    std::sort(actual.begin(), actual.end());
    if (actual != std::vector<unsigned>{b/2, (b+1)/2})
      throw std::runtime_error("incorrect balanced bit allocation: " + label);
    Range first(actual[0]), second(actual[1]);
    mpz_class minimum = first.low * second.low;
    mpz_class maximum = first.high * second.high;
    // For all 17 actual labels, every pair in its high-quarter ranges has the
    // observed decimal length. Thus label conditioning introduces no rejection
    // and no hidden change of weights. Fail loudly if a different corpus does.
    if (minimum.get_str().size() != n_text.size() ||
        maximum.get_str().size() != n_text.size() ||
        label != "RSA-" + std::to_string(n_text.size()))
      throw std::runtime_error("decimal label needs explicit conditioning: " + label);
    result.emplace_back(label, 1, p);
    result.emplace_back(label, 2, q);
    ++rows;
  }
  if (rows != 17) throw std::runtime_error("expected 17 rows / 34 factors");
  return result;
}

double normalized_gap(const mpz_class& prime, const mpz_class& distance) {
  // 2 log(p) is a fixed descriptive normalization, not an assumed exact mean.
  // The Monte Carlo calibration uses exactly this statistic on every draw.
  return distance.get_d() / (2 * std::log(prime.get_d()));
}

void self_test() {
  Counters counts;
  if (!accepts(mpz_class(8321), counts) ||
      mpz_probab_prime_p(mpz_class(8321).get_mpz_t(), 25) != 0)
    throw std::runtime_error("base-2 pseudoprime 8321 must be accepted");
  if (accepts(mpz_class(341), counts) || accepts(mpz_class(13), counts) ||
      !accepts(mpz_class(17), counts))
    throw std::runtime_error("small-factor or e=3 predicate regression");
  uint64_t raw_checks = 0, accepted_checks = 0, endpoint_checks = 0;
  for (unsigned bits = 6; bits <= 13; ++bits) {
    const Range range(bits);
    const auto width = range.width.get_ui(), low = range.low.get_ui();
    std::vector<uint64_t> starts(width, 0), rejection_preimages(width, 0);
    for (uint64_t residue = 0; residue < width; ++residue) {
      mpz_class candidate(static_cast<unsigned long>(low + residue));
      mpz_setbit(candidate.get_mpz_t(), 0);
      const mpz_class final = upward(candidate, range, counts);
      ++starts[mpz_class(final - range.low).get_ui()];
      if (accepts(candidate, counts))
        ++rejection_preimages[mpz_class(candidate - range.low).get_ui()];
      ++raw_checks;
    }
    for (uint64_t offset = 1; offset < width; offset += 2) {
      mpz_class prime(static_cast<unsigned long>(low + offset));
      if (!accepts(prime, counts)) continue;
      const auto previous = predecessor(prime, range, counts);
      if (starts[offset] != gap(prime, previous, range).get_ui() ||
          rejection_preimages[offset] != 2)
        throw std::runtime_error("exact basin or rejection-weight mismatch");
      ++accepted_checks;
      if (previous >= prime) ++endpoint_checks;
    }
  }
  if (endpoint_checks != 8) throw std::runtime_error("missing cyclic endpoint test");
  std::cout << "self_test=PASSED raw_preimages=" << raw_checks
            << " accepted_basins=" << accepted_checks
            << " cyclic_endpoints=" << endpoint_checks
            << " accepted_composite=8321\n";
}

struct Sample {
  double statistic = 0, log_statistic = 0, seconds = 0;
  Counters counts;
  std::string details;
};

int main(int argc, char** argv) {
  try {
    std::string input = "data/solved_decimal_challenge.tsv";
    std::string prefix = "results/gap-study-20260909/pilot";
    uint64_t seed = 20260909, first = 0, replicates = 32;
    unsigned threads = 1;
    bool details = false;
    for (int i = 1; i < argc; ++i) {
      const std::string argument = argv[i];
      if (argument == "--self-test") { self_test(); return 0; }
      if (argument == "--save-factors") { details = true; continue; }
      if (i+1 >= argc) throw std::runtime_error("missing value for " + argument);
      const std::string value = argv[++i];
      if (argument == "--input") input = value;
      else if (argument == "--output-prefix") prefix = value;
      else if (argument == "--seed") seed = std::stoull(value);
      else if (argument == "--first-replicate") first = std::stoull(value);
      else if (argument == "--replicates") replicates = std::stoull(value);
      else if (argument == "--threads") threads = std::stoul(value);
      else throw std::runtime_error("unknown option: " + argument);
    }
    if (!replicates || !threads || first > (UINT64_MAX/2)-replicates)
      throw std::runtime_error("invalid replicate interval or thread count");
    const auto started = std::chrono::steady_clock::now();
    const auto factors = load_factors(input);
    std::ofstream observed(prefix + ".observed.tsv");
    std::ofstream output(prefix + ".samples.tsv");
    std::ofstream factor_output;
    if (!observed || !output) throw std::runtime_error("cannot open output prefix");
    if (details) {
      factor_output.open(prefix + ".factors.tsv");
      if (!factor_output) throw std::runtime_error("cannot open detail output");
      factor_output << "seed\treplicate\tmodel\tlabel\tfactor\tbits\tvalue\tpredecessor\tgap\n";
    }
    observed << "label\tfactor\tbits\tvalue\tpredecessor\tgap\tnormalized_gap"
                "\tvalue_gmp_probable_prime\tpredecessor_gmp_probable_prime\n";
    observed << std::setprecision(17);
    double observed_statistic = 0, observed_log = 0;
    for (const auto& factor : factors) {
      Counters counts;
      if (factor.value < factor.range.low || factor.value > factor.range.high ||
          !accepts(factor.value, counts))
        throw std::runtime_error("factor does not fit model: " + factor.label);
      const auto previous = predecessor(factor.value, factor.range, counts);
      const auto distance = gap(factor.value, previous, factor.range);
      const auto normalized = normalized_gap(factor.value, distance);
      observed_statistic += normalized;
      observed_log += std::log(normalized);
      observed << factor.label << '\t' << factor.index << '\t' << factor.range.bits
               << '\t' << factor.value << '\t' << previous << '\t' << distance
               << '\t' << normalized << '\t'
               << mpz_probab_prime_p(factor.value.get_mpz_t(), 25) << '\t'
               << mpz_probab_prime_p(previous.get_mpz_t(), 25) << '\n';
    }
    observed.close();
    std::cout << std::setprecision(17)
              << "model=old-RSAREF exact_predicate=yes exponent=3 factors=34"
                 " decimal_labels=automatically_satisfied cyclic_endpoints=yes\n"
              << "observed normalized_gap_sum=" << observed_statistic
              << " log_normalized_gap_sum=" << observed_log << '\n'
              << "run seed=" << seed << " first_replicate=" << first
              << " replicates_per_model=" << replicates << " threads=" << threads
              << " random=std-mt19937_64 seed_derivation=splitmix64(seed+2*replicate+model)"
                 " model_index_uniform=0 model_index_scan=1\n" << std::flush;
    const uint64_t job_count = 2 * replicates;
    std::vector<Sample> samples(job_count);
    std::atomic<uint64_t> next(0), done(0);
    std::vector<std::thread> workers;
    for (unsigned thread = 0; thread < std::min<uint64_t>(threads, job_count); ++thread) {
      workers.emplace_back([&]() {
        for (;;) {
          const uint64_t job = next.fetch_add(1);
          if (job >= job_count) break;
          const auto begin = std::chrono::steady_clock::now();
          const uint64_t replicate = first + job/2;
          const bool scan = job % 2;
          const char* model = scan ? "upward_scan" : "uniform_accepted";
          std::mt19937_64 random(splitmix64(seed + 2*replicate + (scan ? 1 : 0)));
          Sample& sample = samples[job];
          std::ostringstream detail;
          for (const auto& factor : factors) {
            const auto prime = draw(random, factor.range, scan, sample.counts);
            const auto previous = predecessor(prime, factor.range, sample.counts);
            const auto distance = gap(prime, previous, factor.range);
            const auto normalized = normalized_gap(prime, distance);
            sample.statistic += normalized;
            sample.log_statistic += std::log(normalized);
            if (details) detail << seed << '\t' << replicate << '\t' << model
                << '\t' << factor.label << '\t' << factor.index << '\t'
                << factor.range.bits << '\t' << prime << '\t' << previous
                << '\t' << distance << '\n';
          }
          sample.details = detail.str();
          sample.seconds = std::chrono::duration<double>(
              std::chrono::steady_clock::now() - begin).count();
          done.fetch_add(1);
        }
      });
    }
    for (auto& worker : workers) worker.join();
    output << "seed\treplicate\tmodel\tnormalized_gap_sum\tlog_normalized_gap_sum"
              "\tcandidates\tfermat_tests\twraps\tjob_seconds\n" << std::setprecision(17);
    uint64_t lower[2] = {0, 0};
    double totals[2] = {0, 0};
    for (uint64_t job = 0; job < job_count; ++job) {
      const auto& sample = samples[job];
      const unsigned model = job % 2;
      lower[model] += sample.statistic <= observed_statistic;
      totals[model] += sample.statistic;
      output << seed << '\t' << first+job/2 << '\t'
             << (model ? "upward_scan" : "uniform_accepted") << '\t'
             << sample.statistic << '\t' << sample.log_statistic << '\t'
             << sample.counts.candidates << '\t' << sample.counts.fermat << '\t'
             << sample.counts.wraps << '\t' << sample.seconds << '\n';
      if (details) factor_output << sample.details;
    }
    for (unsigned model = 0; model < 2; ++model)
      std::cout << "summary model=" << (model ? "upward_scan" : "uniform_accepted")
                << " n=" << replicates << " mean=" << totals[model]/replicates
                << " lower_tail_count=" << lower[model]
                << " monte_carlo_plus_one_p=" << (lower[model]+1.0)/(replicates+1.0)
                << '\n';
    const auto seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now()-started).count();
    std::cout << "complete jobs=" << done.load() << " elapsed_seconds=" << seconds
              << " output_prefix=" << prefix << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 2;
  }
}
