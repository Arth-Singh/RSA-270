#include "md1.hpp"
#include <openssl/md5.h>
#include <gmpxx.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Conditional entropy-collapse experiment. R_RandomUpdate adds separately
// hashed samples, so binary samples leave only a Hamming-weight-dependent state.
// MD5 / big-endian addition / postfix increment matches raw_r_random.c's state
// operations. This scanner bypasses its 256-seed-byte output gate: families with
// samples*encoding_length < 256 require an assumed initialized state or changed
// threshold, not that source's complete seeding call sequence. The association
// with RSA DSP, and every candidate packing below, remain unproved.
// This program discovers matches to already known factors; it never factors N.
namespace {
using Bytes = std::vector<std::uint8_t>;
using Block = std::array<std::uint8_t, 16>;
using U128 = unsigned __int128;
enum class Digest { Md5, Md1 };
enum class Counter { Pre, Post };

std::string Name(Digest x) { return x == Digest::Md5 ? "md5" : "md1"; }
std::string Name(Counter x) { return x == Counter::Pre ? "pre" : "post"; }
std::string Endian(bool little) { return little ? "little" : "big"; }
Digest ParseDigest(const std::string& s) {
  if (s == "md5") return Digest::Md5;
  if (s == "md1") return Digest::Md1;
  throw std::runtime_error("digest must be md5 or md1");
}
Counter ParseCounter(const std::string& s) {
  if (s == "pre") return Counter::Pre;
  if (s == "post") return Counter::Post;
  throw std::runtime_error("counter must be pre or post");
}
bool ParseEndian(const std::string& s) {
  if (s == "big") return false;
  if (s == "little") return true;
  throw std::runtime_error("endian must be big or little");
}
Block Hash(Digest digest, std::span<const std::uint8_t> bytes) {
  if (digest == Digest::Md1) return rsa270::Md1::digest(bytes);
  Block output{};
  MD5(bytes.data(), bytes.size(), output.data());
  return output;
}
std::string Hex(std::span<const std::uint8_t> bytes) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (auto b : bytes) { result += digits[b >> 4]; result += digits[b & 15]; }
  return result;
}
Block ParseBlock(const std::string& hex) {
  if (hex.size() != 32) throw std::runtime_error("state must have 32 hex digits");
  Block result{};
  for (unsigned i = 0; i < 16; ++i) {
    const auto s = hex.substr(2*i, 2);
    std::size_t used = 0;
    result[i] = static_cast<std::uint8_t>(std::stoul(s, &used, 16));
    if (used != 2) throw std::runtime_error("invalid hex state");
  }
  return result;
}
U128 Load(const Block& b, bool little) {
  U128 n = 0;
  for (unsigned i = 0; i < 16; ++i) n = (n << 8) | b[little ? 15-i : i];
  return n;
}
Block Store(U128 n, bool little) {
  Block b{};
  for (unsigned i = 0; i < 16; ++i) { b[little ? i : 15-i] = n; n >>= 8; }
  return b;
}
void Increment(Block& state, bool little, Counter counter) {
  // Both transitions occur AFTER hashing the old state. "pre" and "post"
  // name the C increment expression, not output-versus-increment ordering.
  for (unsigned i = 0; i < 16; ++i) {
    auto& byte = state[little ? i : 15-i];
    if (counter == Counter::Post) { if (byte++) break; }
    else { if (++byte) break; }
  }
}
Bytes Stream(Digest digest, Block state, bool little, Counter counter,
             std::size_t size) {
  Bytes output; output.reserve(size);
  while (output.size() < size) {
    const Block block = Hash(digest, state);
    const auto take = std::min<std::size_t>(16, size-output.size());
    output.insert(output.end(), block.begin(), block.begin()+take);
    Increment(state, little, counter);
  }
  return output;
}

struct Encoding { std::string label; Bytes zero, one; };
std::vector<Encoding> Encodings() {
  std::vector<Encoding> output;
  for (unsigned bytes : {1u, 2u, 3u}) for (unsigned bit = 0; bit < 8*bytes; ++bit) {
    Encoding e{"b"+std::to_string(8*bytes)+"-mem-bit"+std::to_string(bit),
               Bytes(bytes), Bytes(bytes)};
    e.one[bit/8] = 1u << (bit%8); output.push_back(e);
  }
  output.push_back({"byte-00ff", {0}, {255}});
  output.push_back({"ascii-01", {'0'}, {'1'}});
  return output;
}
const std::vector<unsigned> kCounts =
    {34,50,64,83,86,100,128,150,165,166,256,270,330,512,768,1000,1024};
Block Initial(Digest d, const Encoding& e, unsigned n, unsigned weight,
              bool little) {
  if (weight > n) throw std::runtime_error("weight exceeds samples");
  const U128 h0 = Load(Hash(d, e.zero), little);
  const U128 h1 = Load(Hash(d, e.one), little);
  return Store(h0*(n-weight)+h1*weight, little);
}

bool Accepts(const mpz_class& n) {
  if (n <= 11 || mpz_even_p(n.get_mpz_t()) || mpz_fdiv_ui(n.get_mpz_t(), 3) != 2)
    return false;
  for (unsigned divisor : {5u,7u,11u})
    if (mpz_divisible_ui_p(n.get_mpz_t(), divisor)) return false;
  mpz_class r, two = 2;
  mpz_powm(r.get_mpz_t(), two.get_mpz_t(), n.get_mpz_t(), n.get_mpz_t());
  return r == 2;
}
struct Target {
  std::string label;
  mpz_class prime, previous, mask;
  std::size_t bits, modulus_bits;
};
Target MakeTarget(std::string label, const mpz_class& p, std::size_t modulus_bits) {
  if (!Accepts(p)) throw std::runtime_error("target fails old predicate: "+label);
  const auto bits = mpz_sizeinbase(p.get_mpz_t(), 2);
  const mpz_class low = mpz_class(3) << (bits-2);
  if (p < low) throw std::runtime_error("target lacks prefix 11: "+label);
  mpz_class previous = p-2;
  while (previous >= low && !Accepts(previous)) previous -= 2;
  // A target first in the high-quarter range needs a second, wraparound basin.
  // Refuse it instead of silently claiming incomplete coverage. No corpus
  // target has this boundary case.
  if (previous < low) throw std::runtime_error("wraparound target unsupported: "+label);
  const mpz_class differing = mpz_class(previous+1) ^ p;
  const auto varying = differing == 0 ? 0 : mpz_sizeinbase(differing.get_mpz_t(), 2);
  mpz_class mask = ((mpz_class(1) << (bits-2))-1);
  if (varying) mask &= ~((mpz_class(1) << varying)-1);
  return {std::move(label),p,previous,mask,bits,modulus_bits};
}
std::vector<Target> ReadTargets(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open "+path);
  std::string line; std::getline(input,line);
  std::vector<Target> targets;
  while (std::getline(input,line)) {
    if (line.empty()) continue;
    std::istringstream row(line);
    std::string label, ns, ps, qs;
    if (!std::getline(row,label,'\t') || !std::getline(row,ns,'\t') ||
        !std::getline(row,ps,'\t') || !std::getline(row,qs,'\t'))
      throw std::runtime_error("malformed target row");
    mpz_class n(ns,10),p(ps,10),q(qs,10);
    if (p*q != n) throw std::runtime_error("factor product mismatch: "+label);
    const auto b = mpz_sizeinbase(n.get_mpz_t(),2);
    targets.push_back(MakeTarget(label+"-f1",p,b));
    targets.push_back(MakeTarget(label+"-f2",q,b));
  }
  if (targets.empty()) throw std::runtime_error("empty target file");
  return targets;
}
Bytes Export(const mpz_class& n, std::size_t size, bool little) {
  Bytes minimal(size), output(size); std::size_t written = 0;
  if (n < 0 || mpz_sizeinbase(n.get_mpz_t(),2) > 8*size)
    throw std::runtime_error("integer does not fit buffer");
  mpz_export(minimal.data(), &written, little ? -1 : 1, 1, 1, 0, n.get_mpz_t());
  std::copy_n(minimal.begin(),written,little ? output.begin() : output.end()-written);
  return output;
}
mpz_class Import(std::span<const std::uint8_t> bytes, bool little) {
  mpz_class output;
  mpz_import(output.get_mpz_t(),bytes.size(),little ? -1 : 1,1,1,0,bytes.data());
  return output;
}
struct Lane { std::string label; std::vector<unsigned> permutation; };
std::vector<Lane> Lanes() {
  return {{"byte",{0}}, {"word-01",{0,1}}, {"word-10",{1,0}},
          {"24-012",{0,1,2}}, {"24-021",{0,2,1}}, {"24-102",{1,0,2}},
          {"24-120",{1,2,0}}, {"24-201",{2,0,1}}, {"24-210",{2,1,0}}};
}
Bytes Pack(std::span<const std::uint8_t> input, const Lane& lane, bool to_raw) {
  Bytes output(input.size());
  const auto width = lane.permutation.size();
  for (std::size_t base = 0; base < input.size(); base += width) {
    const auto available = std::min(width,input.size()-base);
    // Restrict the permutation to present byte positions. This is explicitly
    // a compact-tail hypothesis and is bijective, unlike the legacy fallback.
    std::size_t numeric = 0;
    for (auto raw : lane.permutation) if (raw < available) {
      if (to_raw) output[base+raw] = input[base+numeric];
      else output[base+numeric] = input[base+raw];
      ++numeric;
    }
  }
  return output;
}
std::vector<std::size_t> BufferSizes(const Target& t) {
  std::set<std::size_t> sizes{(t.bits+7)/8};
  for (unsigned digit_bits : {16u,24u,32u}) {
    const auto digits = (t.modulus_bits+digit_bits-1)/digit_bits;
    sizes.insert(((digits+1)/2)*(digit_bits/8));
  }
  return {sizes.begin(),sizes.end()};
}
std::uint64_t Fingerprint(const std::uint8_t* bytes) {
  std::uint64_t output; std::memcpy(&output,bytes,8); return output;
}
struct Pattern {
  std::size_t target,lane,size,anchor;
  bool little;
  Bytes expected,mask;
};
struct Patterns {
  std::vector<Pattern> entries;
  std::unordered_multimap<std::uint64_t,std::size_t> table;
  std::array<std::uint64_t,1024> prefix{};
};
Patterns BuildPatterns(const std::vector<Target>& targets,const std::vector<Lane>& lanes) {
  Patterns output;
  for (std::size_t t=0;t<targets.size();++t) for (bool little : {false,true})
    for (auto size : BufferSizes(targets[t])) for (std::size_t l=0;l<lanes.size();++l) {
      auto mask = Pack(Export(targets[t].mask,size,little),lanes[l],true);
      auto expected = Pack(Export(targets[t].prime & targets[t].mask,size,little),lanes[l],true);
      std::size_t anchor = size;
      // Prefer the old offset where it is provably fixed; otherwise choose
      // any fixed eight-byte run. Search uses anchor position to recover start.
      std::vector<std::size_t> choices{4};
      for (std::size_t i=0;i+8<=size;++i) if (i!=4) choices.push_back(i);
      for (auto i : choices) if (i+8<=size &&
          std::all_of(mask.begin()+i,mask.begin()+i+8,[](auto b){return b==255;})) {
        anchor=i; break;
      }
      if (anchor==size) throw std::runtime_error("no 64-bit interior anchor: "+targets[t].label);
      const auto fp=Fingerprint(expected.data()+anchor);
      output.table.emplace(fp,output.entries.size());
      output.prefix[(fp&65535)/64] |= std::uint64_t(1) << (fp&63);
      output.entries.push_back({t,l,size,anchor,little,std::move(expected),std::move(mask)});
    }
  return output;
}
mpz_class Candidate(const mpz_class& raw,std::size_t bits,unsigned forced_top) {
  mpz_class candidate;
  mpz_fdiv_r_2exp(candidate.get_mpz_t(),raw.get_mpz_t(),bits-forced_top);
  if (forced_top) candidate |= ((mpz_class(1)<<forced_top)-1) << (bits-forced_top);
  candidate |= 1;
  return candidate;
}
struct Event {
  std::size_t target,lane,size,start;
  bool little;
  int forced_top;  // -1 is an interior-pattern hit without an interval match.
  mpz_class delta;
  bool fresh;
};
struct Counts {
  std::uint64_t states=0,windows=0,anchors=0,patterns=0,intervals=0,fresh=0;
  Counts& operator+=(const Counts& x) {
    states+=x.states;windows+=x.windows;anchors+=x.anchors;patterns+=x.patterns;
    intervals+=x.intervals;fresh+=x.fresh;return *this;
  }
};
using Emit=std::function<void(const Event&)>;
Counts Search(const Bytes& stream,const Patterns& patterns,
              const std::vector<Target>& targets,const std::vector<Lane>& lanes,
              const Emit& emit) {
  Counts count;
  for (std::size_t at=0;at+8<=stream.size();++at) {
    ++count.windows;
    const auto fp=Fingerprint(stream.data()+at);
    if (!(patterns.prefix[(fp&65535)/64] & (std::uint64_t(1) << (fp&63)))) continue;
    const auto [first,last]=patterns.table.equal_range(fp);
    for (auto it=first;it!=last;++it) {
      const auto& p=patterns.entries[it->second];
      if (at<p.anchor) continue;
      const auto start=at-p.anchor;
      if (start+p.size>stream.size()) continue;
      ++count.anchors;
      bool matches=true;
      for (std::size_t i=0;i<p.size;++i)
        if ((stream[start+i]&p.mask[i])!=p.expected[i]) { matches=false;break; }
      if (!matches) continue;
      ++count.patterns;
      const auto raw=Import(Pack(std::span(stream.data()+start,p.size),lanes[p.lane],false),p.little);
      const auto& target=targets[p.target];
      bool interval=false;
      for (unsigned top : {0u,1u,2u}) {
        const auto candidate=Candidate(raw,target.bits,top);
        if (candidate<=target.previous || candidate>target.prime) continue;
        interval=true;++count.intervals;
        const bool fresh=candidate==target.prime;
        count.fresh+=fresh;
        emit({p.target,p.lane,p.size,start,p.little,static_cast<int>(top),
              target.prime-candidate,fresh});
      }
      if (!interval) emit({p.target,p.lane,p.size,start,p.little,-1,0,false});
    }
  }
  return count;
}

struct Work {
  Digest digest; std::size_t encoding; bool little; Counter counter;
  unsigned samples; std::uint64_t begin; U128 h0,delta;
};
std::vector<Work> WorkItems(const std::string& mode,const std::vector<Encoding>& encodings) {
  std::vector<Digest> digests;
  if (mode=="both") digests={Digest::Md5,Digest::Md1};
  else digests={ParseDigest(mode)};
  std::vector<Work> output; std::uint64_t begin=0;
  for (auto d:digests) for (std::size_t e=0;e<encodings.size();++e)
    for (bool little:{false,true}) {
      const auto h0=Load(Hash(d,encodings[e].zero),little);
      const auto delta=Load(Hash(d,encodings[e].one),little)-h0;
      for (auto counter:{Counter::Pre,Counter::Post}) for (auto n:kCounts) {
        output.push_back({d,e,little,counter,n,begin,h0,delta});begin+=n+1;
      }
    }
  return output;
}
Block Initial(const Work& work,unsigned weight) {
  return Store(work.h0*work.samples+work.delta*weight,work.little);
}
struct Options {
  std::string digest="both",targets="data/solved_decimal_challenge.tsv";
  std::uint64_t begin=0,end=std::numeric_limits<std::uint64_t>::max();
  unsigned threads=1;std::size_t stream_bytes=2048;
};
Counts Run(const Options& options,const std::vector<Target>& targets,bool print) {
  const auto encodings=Encodings();const auto lanes=Lanes();
  const auto patterns=BuildPatterns(targets,lanes);
  const auto work=WorkItems(options.digest,encodings);
  const auto total=work.back().begin+work.back().samples+1;
  const auto end=std::min(options.end,total);
  if (options.begin>=end) throw std::runtime_error("empty state range");
  if (print) std::cout << "config digest="<<options.digest<<" encodings=50 sample_counts=17"
      <<" arithmetic_orders=2 counters=2 configured_states="<<total
      <<" begin="<<options.begin<<" end="<<end<<" threads="<<options.threads
      <<" stream_bytes="<<options.stream_bytes<<" targets="<<targets.size()
      <<" patterns="<<patterns.entries.size()<<" starts=independent-complete-buffers-only"
      <<" seed_byte_gate=not-enforced"
      <<" candidate_modes=truncate,top1,top2;odd;upward-or-fresh\n";
  std::atomic<std::size_t> next{0};std::mutex mutex;Counts counts;
  const auto started=std::chrono::steady_clock::now();
  std::vector<std::thread> threads;
  for (unsigned thread=0;thread<options.threads;++thread) threads.emplace_back([&]{
    Counts local;
    while (true) {
      const auto index=next.fetch_add(1);
      if (index>=work.size()) break;
      const auto& w=work[index];
      const auto from=std::max(options.begin,w.begin);
      const auto until=std::min(end,w.begin+w.samples+1);
      for (auto id=from;id<until;++id) {
        const unsigned weight=id-w.begin;
        const auto initial=Initial(w,weight);
        const auto stream=Stream(w.digest,initial,w.little,w.counter,options.stream_bytes);
        ++local.states;
        local+=Search(stream,patterns,targets,lanes,[&](const Event& e){
          if (!print) return;
          std::lock_guard guard(mutex);
          std::cout<<(e.forced_top<0 ? "PATTERN_ONLY" : "CANDIDATE_INTERVAL")
            <<" state_id="<<id<<" digest="<<Name(w.digest)
            <<" encoding="<<encodings[w.encoding].label<<" samples="<<w.samples
            <<" weight="<<weight<<" arithmetic="<<Endian(w.little)
            <<" counter="<<Name(w.counter)<<" initial_state="<<Hex(initial)
            <<" target="<<targets[e.target].label<<" stream_start="<<e.start
            <<" buffer_bytes="<<e.size<<" lane="<<lanes[e.lane].label
            <<" integer_endian="<<Endian(e.little)<<" forced_top="<<e.forced_top
            <<" prime_delta="<<e.delta<<" fresh_candidate="<<(e.fresh?"yes":"no")
            <<" factorization=no\n";
        });
      }
    }
    std::lock_guard guard(mutex);counts+=local;
  });
  for (auto& thread:threads) thread.join();
  const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
  if (print) std::cout<<"summary states="<<counts.states<<" windows="<<counts.windows
    <<" fingerprint_hits="<<counts.anchors<<" interior_pattern_hits="<<counts.patterns
    <<" candidate_interval_hits="<<counts.intervals<<" fresh_candidate_hits="<<counts.fresh
    <<" elapsed_seconds="<<seconds<<" streams_per_second="<<counts.states/seconds
    <<" factorization=no\n";
  return counts;
}

void Require(bool good,const std::string& message) {
  if (!good) throw std::runtime_error("self-test: "+message);
}
void SelfTest() {
  for (const auto& [input,expected]:std::vector<std::pair<std::string,std::string>>{
      {"","7ae2a21d8793650044ae40277eafaa2e"},
      {"abc","74594a0a73b03503efe62e574dbaf816"},
      {"a","420965f2da150b44cbfadd695ab5bbe9"},
      {"1234567890","34dbffc3e4a9c0ad2d5559c99649c69e"}}) {
    const Bytes bytes(input.begin(),input.end());
    Require(Hex(Hash(Digest::Md1,bytes))==expected,"MD1 KAT "+input);
  }
  Require(Hex(Hash(Digest::Md5,Bytes{}))=="d41d8cd98f00b204e9800998ecf8427e","MD5 empty");
  Require(Hex(Hash(Digest::Md5,Bytes{'a','b','c'}))=="900150983cd24fb0d6963f7d28e17f72","MD5 abc");
  for (bool little:{false,true}) {
    Block zero{},ones;ones.fill(1);
    Increment(zero,little,Counter::Post);Require(zero==ones,"postfix zero carries through every byte");
    Block state{};state[little?0:15]=255;
    Increment(state,little,Counter::Pre);
    Block want{};want[little?1:14]=1;Require(state==want,"prefix 255 carry");
    state.fill(0);state[little?1:14]=7;
    Increment(state,little,Counter::Post);
    want.fill(0);want[little?0:15]=1;want[little?1:14]=8;
    Require(state==want,"postfix stops at first old nonzero byte");
  }
  // This composite passes the exact historical predicate and must be accepted
  // as an interval boundary rather than skipped by a modern primality test.
  Require(Accepts(mpz_class(8321)),"base-2 pseudoprime boundary");
  const auto lanes=Lanes();
  for (const auto& lane:lanes) for (unsigned n=1;n<=9;++n) {
    Bytes bytes(n);for(unsigned i=0;i<n;++i)bytes[i]=i+17;
    Require(Pack(Pack(bytes,lane,true),lane,false)==bytes,"packing inverse "+lane.label);
  }
  Require(Pack(Bytes{10,20},lanes[6],false)==Bytes({20,10}),"partial 24-120 has no duplicated byte");
  const auto encodings=Encodings();
  Require(encodings.size()==50 && kCounts.size()==17,"finite family size");
  auto works=WorkItems("both",encodings);
  Require(works.back().begin+works.back().samples+1==2081200,"configured state count");
  std::size_t controls=0,run_controls=0;
  for (auto digest:{Digest::Md5,Digest::Md1}) for(bool arithmetic:{false,true})
    for(auto counter:{Counter::Pre,Counter::Post}) {
      const auto initial=Initial(digest,encodings[0],34,9,arithmetic);
      const auto stream=Stream(digest,initial,arithmetic,counter,256);
      for (unsigned minimal:{21u,22u,23u}) {
        const unsigned bits=8*minimal-3;
        for (auto size:std::set<unsigned>{minimal,static_cast<unsigned>(((bits+15)/16)*2),
                                         static_cast<unsigned>(((bits+23)/24)*3),
                                         static_cast<unsigned>(((bits+31)/32)*4)})
          for (bool integer_little:{false,true}) for(std::size_t l=0;l<lanes.size();++l) {
            const std::size_t start=31;
            const auto raw=Import(Pack(std::span(stream.data()+start,size),lanes[l],false),integer_little);
            const auto candidate=Candidate(raw,bits,2);
            mpz_class p=candidate;
            while(!Accepts(p))p+=2;
            const std::vector<Target> targets{MakeTarget("planted",p,2*bits)};
            bool discovered=false;
            Search(stream,BuildPatterns(targets,lanes),targets,lanes,[&](const Event& e){
              if(e.start==start && e.size==size && e.lane==l && e.little==integer_little &&
                 e.forced_top==2 && e.delta==p-candidate) discovered=true;
            });
            Require(discovered,"seeded stream discovery "+Name(digest)+" "+lanes[l].label);
            ++controls;
            if(minimal==21 && size==21 && !integer_little && l==0) {
              Options o;o.digest=Name(digest);o.stream_bytes=256;
              const auto jobs=WorkItems(o.digest,encodings);
              for(const auto& w:jobs) if(w.encoding==0 && w.little==arithmetic &&
                  w.counter==counter && w.samples==34) {o.begin=w.begin+9;o.end=o.begin+1;break;}
              const auto found=Run(o,targets,false);
              Require(found.states==1 && found.intervals>0,"enumeration-to-discovery path");
              ++run_controls;
            }
          }
      }
    }
  // Fresh-candidate and pattern-only controls use the same masks and decoder.
  mpz_class p=(mpz_class(3)<<163)+123456789;
  p|=1;while(!Accepts(p))p+=2;
  const std::vector<Target> targets{MakeTarget("classification",p,330)};
  const auto patterns=BuildPatterns(targets,lanes);
  Bytes fresh(128,0);const auto encoded=Export(p,21,false);
  std::copy(encoded.begin(),encoded.end(),fresh.begin()+17);
  bool saw_fresh=false;
  Search(fresh,patterns,targets,lanes,[&](const Event& e){if(e.start==17 && e.fresh)saw_fresh=true;});
  Require(saw_fresh,"fresh candidate classification");
  mpz_class forged=p & targets[0].mask;
  if (Candidate(forged,targets[0].bits,2)>targets[0].previous) {
    const mpz_class different=mpz_class(targets[0].previous+1)^p;
    forged|=((mpz_class(1)<<mpz_sizeinbase(different.get_mpz_t(),2))-1);
  }
  Bytes only(128,0);const auto forged_bytes=Export(forged,21,false);
  std::copy(forged_bytes.begin(),forged_bytes.end(),only.begin()+17);
  bool saw_only=false;
  Search(only,patterns,targets,lanes,[&](const Event& e){if(e.start==17 && e.forced_top<0)saw_only=true;});
  Require(saw_only,"pattern-only classification");
  // No pattern may consume bytes beyond the declared stream boundary.
  Bytes truncated(encoded.begin(),encoded.end()-1);
  Require(Search(truncated,patterns,targets,lanes,[](const Event&){}).patterns==0,"stream boundary");
  std::cout<<"SELF_TEST PASSED seeded_packing_controls="<<controls
           <<" enumeration_controls="<<run_controls
           <<" digest_kats=6 counter_carries=6 classification_controls=2\n";
}

void Usage() {
  std::cerr<<"usage:\n"
    <<"  noise_hamming_scan self-test\n"
    <<"  noise_hamming_scan scan [--digest both|md5|md1] [--targets TSV]\n"
    <<"      [--begin N] [--end N] [--threads N] [--stream-bytes N]\n"
    <<"  noise_hamming_scan stream DIGEST ENCODING_INDEX SAMPLES WEIGHT big|little pre|post BYTES\n"
    <<"  noise_hamming_scan state-stream DIGEST STATE_HEX big|little pre|post BYTES\n"
    <<"  noise_hamming_scan counter STATE_HEX big|little pre|post\n"
    <<"pre/post names increment expressions; output always hashes the old state.\n"
    <<"scan exits 0=no full patterns, 3=pattern-only, 4=candidate interval; never factorization.\n";
}
} // namespace

int main(int argc,char** argv) {
  try {
    if(argc==2 && std::string(argv[1])=="self-test"){SelfTest();return 0;}
    if(argc==5 && std::string(argv[1])=="counter") {
      auto state=ParseBlock(argv[2]);Increment(state,ParseEndian(argv[3]),ParseCounter(argv[4]));
      std::cout<<Hex(state)<<'\n';return 0;
    }
    if(argc==7 && std::string(argv[1])=="state-stream") {
      const auto size=std::stoull(argv[6]);
      if(size>1048576)throw std::runtime_error("stream too large");
      std::cout<<Hex(Stream(ParseDigest(argv[2]),ParseBlock(argv[3]),ParseEndian(argv[4]),
                            ParseCounter(argv[5]),size))<<'\n';return 0;
    }
    if(argc==9 && std::string(argv[1])=="stream") {
      const auto encodings=Encodings();const auto index=std::stoull(argv[3]);
      if(index>=encodings.size())throw std::runtime_error("bad encoding index");
      const auto d=ParseDigest(argv[2]);const bool little=ParseEndian(argv[6]);
      const auto n=std::stoul(argv[4]),weight=std::stoul(argv[5]);
      const auto size=std::stoull(argv[8]);
      if(n>1000000 || weight>n || size>1048576)throw std::runtime_error("stream request exceeds limit");
      const auto state=Initial(d,encodings[index],n,weight,little);
      std::cout<<"initial_state="<<Hex(state)<<"\nstream="
               <<Hex(Stream(d,state,little,ParseCounter(argv[7]),size))<<'\n';return 0;
    }
    if(argc>=2 && std::string(argv[1])=="scan") {
      Options o;
      for(int i=2;i<argc;i+=2) {
        if(i+1>=argc)throw std::runtime_error("missing option value");
        const std::string key=argv[i],value=argv[i+1];
        if(key=="--digest")o.digest=value;
        else if(key=="--targets")o.targets=value;
        else if(key=="--begin")o.begin=std::stoull(value);
        else if(key=="--end")o.end=std::stoull(value);
        else if(key=="--threads")o.threads=std::stoul(value);
        else if(key=="--stream-bytes")o.stream_bytes=std::stoull(value);
        else throw std::runtime_error("unknown option "+key);
      }
      if(o.threads<1 || o.threads>256 || o.stream_bytes<128 || o.stream_bytes>1048576)
        throw std::runtime_error("threads must be 1..256 and stream bytes 128..1048576");
      const auto counts=Run(o,ReadTargets(o.targets),true);
      return counts.intervals?4:(counts.patterns?3:0);
    }
    Usage();return 2;
  } catch(const std::exception& e) {std::cerr<<"error: "<<e.what()<<'\n';return 2;}
}
