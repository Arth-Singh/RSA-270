# Sources, provenance, and artifact recovery

These links were investigated in the September 3 session. Their availability and external claims were not freshly verified during the September 9 handoff. Local files and exact arithmetic were checked as described in results/handoff-2026-09-09/.

## Challenge provenance

- Preserved RSA challenge-list auto-email / historical mirror:
  https://www.ontko.com/pub/rayo/primes/rsa_fact.html
  It states random approximately equal-length primes, both 2 modulo 3, probabilistic primality testing, generation in about 30 minutes on RSA DSP, and disposal of the factors.
- Original 1991 Kaliski announcement mirror:
  https://decnotes.datacellar.net/showtopic.php?conf=1269&topic=1073752158&userno=4629
  Reported to omit the needed generator internals.
- Later BSAFE-generated challenge-list FAQ, different provenance:
  https://web.archive.org/web/20130921043454/http://www.emc.com/emc-plus/rsa-labs/historical/the-rsa-factoring-challenge-faq.htm
  Do not transfer its BSAFE 4.0 / ComScire seeding description to the original 1991 decimal list.

## RSA DSP hardware paper

S. R. Dusse and B. S. Kaliski Jr., "A Cryptographic Library for the Motorola DSP56000", EUROCRYPT '90 proceedings, published 1991, pp. 230–244.

https://link.springer.com/content/pdf/10.1007/3-540-46877-3_21.pdf

Previously extracted text: /tmp/dusse-kaliski.txt, now absent. The valid
publisher PDF was reacquired during the September 9 compute continuation and
is preserved locally as
`results/generator-evidence-2026-09-09/dusse-kaliski.pdf`, SHA-256
`ef14dcd3021d2817b83f8783e4072a398db70f28413862f30baff674801f966c`.
The directory includes full extraction, inspected page renders, retrieval
commands, manifests, and `REPORT.md` with exact source limitations. Signed
24-bit arithmetic and configurable host transfers are confirmed, but neither
specifies RNG-to-candidate packing.

Relevant portions discuss the DSP, PC memory/host/status interfaces, the CND6002A noise circuit, and RSA-MD1. They do not supply challenge-specific key-generation firmware, seed acquisition, or sampling cadence.

## Lotus Notes BSAFE and LIB binaries

_BSAFE.DLL inside the 1991-11-16 Lotus Notes distribution:

https://discmaster.textfiles.com/file/29624/ibm0060-0069/ibm0061.tar/ibm0061/NOTESDW1.ZIP/DOSPROG1.CMP/_BSAFE.DLL

Recorded full SHA-256, also preserved in src/bsafe1_rng_scan.cpp:

```text
5fd7711ad68e1af8a1d832b4a95d35bc414e1f663eefded3f140a0aaf209eb23
```

Companion _LIB.DLL:

https://discmaster.textfiles.com/file/29624/ibm0060-0069/ibm0061.tar/ibm0061/NOTESDW1.ZIP/DOSPROG1.CMP/_LIB.DLL

The available old summary recorded only a truncated _LIB SHA-256 beginning 917321 and ending eb1. That is not enough to verify a download; obtain a full new hash if reacquired.

Missing prior local artifacts:

- /tmp/rsa270_bsafe_19911116.dll
- /tmp/rsa270__LIB.DLL
- /tmp/bsafe_lib_random.py
- Emulator/disassembly scripts that were not copied into src/
- Contents formerly under /tmp/rsa270-unicorn.dO6zGZ/

The source recovered from these binaries survives in src/. Do not claim the original files or emulator logs are present.

## RSAREF source

Plan 9 source browser:

https://9p.io/sources/contrib/btdn/src/pgp/rsaref/source/

Files downloaded into references/rsaref/r_random.c, prime.c, and r_keygen.c are HTML-wrapped source. Use raw_ versions for code inspection or compilation.

Raw gzipped mirror:

https://ftp.arnes.si/pub/packages3/crypto-tools/ftp.funet.fi-crypto/cryptography/rpem/ripem/rsaref/source/

The directory serves names such as r_random.c.gz, r_keygen.c.gz, prime.c.gz, prime-old.c.gz, and nn.h.gz. They were decompressed into references/rsaref/raw_*.

Stock version archives:

https://sourceforge.net/projects/rsaref/files/

Previously downloaded rsaref-1.0-1993.tar.gz as /tmp/rsaref-1.0.tar.gz; now absent. The package's r_random.c independently contained the unusual postfix increment. The older prime file includes Mark Henderson's April 1993 optional faster Fermat exponentiation modification.

Known distinctions:

- Published files carry 1991–1992 copyright notices, but source timestamps alone do not prove use for the March 1991 challenge.
- A reported release date is March 2, 1992.
- A reported MD5 source comment says created 2/17/90 and revised 1/91. This deserved verification; do not infer that the challenge necessarily used MD1 just because MD5's RFC appeared in 1992.
- The raw-random postfix increment must be modeled literally or explicitly marked as an alternative.

## Preserved ftp.rsa.com archive

DiscMaster directory:

https://discmaster.textfiles.com/browse/17453/2014.05.ftp.rsa.com.tar/ftp.rsa.com/pub

Internet Archive item:

https://archive.org/details/ftp.rsa.com

Archive filename listing:

https://archive.org/download/ftp.rsa.com/2014.05.ftp.rsa.com.tar.txt

The full tar was reported as approximately 6.864 GB. The session searched filenames and selected documents; it did not establish that every byte of the full archive had been analyzed.

### generation.ps

https://discmaster.textfiles.com/file/17453/2014.05.ftp.rsa.com.tar/ftp.rsa.com/pub/ps/generation.ps

Robert D. Silverman, "FAST GENERATION OF RANDOM, STRONG RSA PRIMES", internal date May 17, 1997; nine pages; DSC print time July 21, 1997 08:45:30.

This is an X9.31 strong-prime proposal with CRT construction, random 101-bit auxiliary starts, and 25 Miller–Rabin iterations. It is not a description of 1991 challenge generation. Its RNG discussion recommends auditing the algorithm/seed and sufficient entropy, without identifying the challenge RNG.

Recorded original SHA-256:

```text
d5f1873c82a6d93d3d0179d417d04713ab35095e456646d1e85a8bf43a780089
```

### primes.ps

https://discmaster.textfiles.com/file/17453/2014.05.ftp.rsa.com.tar/ftp.rsa.com/pub/ps/primes.ps

Robert D. Silverman, "THE REQUIREMENT FOR STRONG PRIMES IN RSA ENCRYPTION"; seven pages; DSC print time July 21, 1997 08:40:52. Discusses whether strong primes are needed; no relevant generator implementation.

Recorded original SHA-256:

```text
246fe3089f9c38431ccb39f4e5a7133a5b0de82185056ccfad60b8af3e1d1b5b
```

Both files required removing PJL and decoding Adobe TBCP quotation: an escape byte 01 followed by xx decodes to xx XOR 40. The old /tmp/decode_tbcp.py and /tmp/rsa270_generation.* and /tmp/rsa270_primes.* decoded PS/PDF/text files are absent.

### Bulletin 8 and neighbors

https://discmaster.textfiles.com/file/17453/2014.05.ftp.rsa.com.tar/ftp.rsa.com/pub/ps/bulletn8.ps

Robert Baldwin's 1998 analysis of BSAFE 3.x PRNGs discusses backward compatibility and older order-independent seed accumulation. It does not connect the recovered Lotus byte RNG or the challenge's DSP generator to an exact seed procedure.

Other inspected neighbors included 201.ps (Koc, high-speed RSA, 1994), sp2 (strong primes, 1998), and tr801 (hardware, 1996). No challenge firmware/driver was found.

## FactorLab and other adjacent artifacts

Waldemar Horwat's FactorLab, MacHack 1989 collection, copyright 1987/1989:

https://discmaster.textfiles.com/file/14142/MacHack%201995.toast/Presentations/Presentations%20%E2%80%9989/Papers%20%E2%80%9989/Horwat%20Papers/FactorLab

Reported code uses find-rsa-prime(start), with p=5+6*start and increments of 6 until prime?. make-rsa takes two starting values. A global Miller–Rabin count is 32; bases use (1+ (random (n-1))). A comment calls start a true random number seed.

This is only a contemporary comparison. No RSADSI/Kaliski/DSP connection or exact RNG implementation was established before interruption. The requested neighbor/source hunt was unfinished.

Dr. Dobb's, Burt Kaliski, "Multiple-Precision Arithmetic in C", 1992:

https://discmaster.textfiles.com/file/41789/DDJ_CD6.iso/data/1992/9208/9208c/9208c.htm

Provides RSAREF arithmetic context, not the missing 1991 RNG.

1991 NIST DSS material:

https://discmaster.textfiles.com/file/16623/SharewareTiger.cdr/x13/dss9115.zip/DSS.TXT

Generic generation/noise/ANSI X9.17 guidance, not challenge code.

The surviving Netscape 1.0 Windows distribution was copied from /private/tmp/rsa270_netscape1.kmp5T9 to references/recovered-temp/netscape-1.0/. It contains README, LICENSE, NETSCAPE.EXE, and installer files. Its original download URL was not preserved in available context. No result linking it to the challenge generator is known. Preserve its license; its presence does not make it a project-authored component.

## RNG cryptanalysis reference

John Kelsey, Bruce Schneier, David Wagner, and Chris Hall, "Cryptanalytic Attacks on Pseudorandom Number Generators":

https://www.schneier.com/wp-content/uploads/2017/10/paper-prngs.pdf

Previously /tmp/prngs.pdf and /tmp/prngs.txt; now absent.

The RSAREF section discusses MD5/addition, order-independent seed accumulation, state-compromise extension, and chosen-input/timing attacks. Those attacks typically require state compromise or an interactive oracle unavailable from fixed public RSA moduli. It does not provide a practical full-state recovery from our small fixed set of modeled output blocks.

The former structure agent cited Sasaki and Aoki's EUROCRYPT 2009 MD5 preimage work (~2^123.4) as a reason direct full-MD5 inversion is impractical. The exact paper link was not retained; re-source before making a current cryptanalytic-bound claim.

On September 9, Søren S. Thomsen's primary paper, [An improved preimage attack
on MD2](https://eprint.iacr.org/2008/089.pdf), and the MD2 reference implementation
in [RFC 1319](https://www.rfc-editor.org/rfc/rfc1319.txt) were checked against the
preserved MD1 code. The journal record is [Cryptanalysis of
MD2](https://orbit.dtu.dk/en/publications/cryptanalysis-of-md2/), DOI
10.1007/s00145-009-9054-1; its full text was not obtained. The exact-length,
checksum, IV, projection, and memory barriers are recorded in
`results/primitive-analysis-2026-09-09/literature/MD1_MD2_APPLICABILITY.md`.
This work does not transfer the published MD2 attack bound to MD1. Downloaded
paper, extracted text, and inspected renderings remain local, with URLs and
hashes in the provenance file.

## Construction-family paper found during the search

Anthony Overmars and Sitalakshmi Venkatraman, "Forensic Construction-Family Signatures in Solved RSA Challenge Moduli: High-Bit Conditioning, Residue Constraints, and Factor-Balance Patterns", Information 2026, 17(9), 847; page reported publication August 31, 2026.

https://www.mdpi.com/2078-2489/17/9/847

DOI: https://doi.org/10.3390/info17090847

Supplement: https://www.mdpi.com/article/10.3390/info17090847/s1

The fetched page reports 22 formal solved challenge moduli / 44 factors, split into 18 original decimal-labelled pairs and four later bit-labelled pairs. It emphasizes high-bit conditioning as a broad construction-family property, not exact implementation attribution, seed recovery, or weaker factoring resistance. It also notes small sample/selection limitations and that the independent rerun reused archived control pools.

The original PDF download attempt returned HTML, not a valid PDF. The old /tmp/rsa_forensics.pdf name must not be treated as a successfully acquired PDF. The supplemental temporary directories were empty when inspected for handoff.

## Search methods that already worked or failed

- DiscMaster search endpoint shape:
  /search?q=...&qfields=t|name&mode=deep&itemid=17453&limit=...&outputAs=json
  Exact field behavior and quoting should be checked against actual results.
- Exact RSA DSP queries produced many irrelevant modern DSP hits. Global source-string searches and archive-specific filename searches were more targeted.
- Wayback /cdx/search/cdx returned 503 in the prior session.
- Wayback /web/timemap/json accepted a url parameter and trailing-prefix wildcards, for example url=www.rsa.com/rsalabs/*.
- A wildcard in the middle of a URL was not useful in the tested endpoint.
- Old RSA website prefixes mostly yielded later challenge/product pages, not the early board firmware.
- Full ftp filename audit plus selected document inspection found no usable generator artifact.

These are historical observations, not guarantees of current endpoint availability.
