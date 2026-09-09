# RSA DSP primary-source recovery, 2026-09-09

## Result

No challenge-specific firmware, driver, manual, candidate-generation loop, seed-conditioning algorithm, or usable generator state was recovered. The search does not establish incremental search versus candidate resampling, and it provides no evidence of a low-entropy seed.

The missing Dusse/Kaliski paper has been recovered as a valid 15-page PDF from Springer, text extracted, and the relevant pages visually checked. This restores a primary artifact that was previously absent from the handoff.

## Primary-source observations

Source: S. R. Dusse and B. S. Kaliski Jr., *A Cryptographic Library for the Motorola DSP56000*, EUROCRYPT '90 proceedings, 1991, pp. 230-244. [Publisher PDF](https://link.springer.com/content/pdf/10.1007/3-540-46877-3_21.pdf).

- Section 3.3, printed p. 236: the authors use signed 24-bit digits, with each digit in [-2^23, 2^23-1]. They explicitly distinguish this representation from unsigned 24-bit limbs.
- Section 5.3, printed p. 239: the PC memory interface can transfer each DSP word in low-medium-high byte order or transfer its low byte only. The PC-programmable address generator determines which byte is addressed.
- Section 5.4, printed p. 240: a CND6002A noise diode, amplification, and TTL-output comparator provide bits to the DSP and status registers. Its stated purpose is cryptographic key randomness, with isolation intended to reduce predictable coupling.
- Section 6, printed p. 240 footnote: the implementation described had RSA-MD1; RSA-MD2 was planned.

These are observations about the authors' described board/library, not confirmation of the challenge's exact firmware. None of the paper's sections describes its prime-selection loop or entropy conditioning. The signed-digit and interface details do not determine the byte order of the RNG-to-candidate path: conversion could occur before arithmetic, and transfer modes could serve unrelated data.

The [preserved RSA challenge-list auto-email](https://www.ontko.com/pub/rayo/primes/rsa_fact.html) was also reacquired. Its generation paragraph says approximately equal-size random primes, residue 2 modulo 3, probabilistic primality testing, disposal of the factors, and about 30 minutes on RSA DSP. It gives no candidate acceptance or reseeding algorithm. The target RSA-270 text matches CONTEXT.md exactly.

The [March 1991 announcement mirror](https://decnotes.datacellar.net/showtopic.php?conf=1269&topic=1073752158&userno=4629) was reacquired. Its Miller-Rabin passage concerns checking submitted factorizations; it must not be treated as the generator's exact prime-testing routine.

## Archive checks and limits

The 166,295-byte [ftp.rsa.com filename listing](https://archive.org/download/ftp.rsa.com/2014.05.ftp.rsa.com.tar.txt) was reacquired. A case-insensitive filename search for `dsp|56000|kaliski|dusse|manual|hardware|random` returned no lines (rg exit 1). This is only a filename-level result for that preserved FTP archive; it does not inspect nested archive content or establish absence from other archives.

Three direct DiscMaster content queries were attempted: the exact phrase `"RSA DSP"`, `CND6002A`, and `56000` restricted to FTP item 17453. All timed out after a 20-second request timeout. The web tool also failed to open the first query. These are unavailable searches, not zero-result searches. Request URLs and errors are retained in `archive-query-manifest.json`.

Indexed searches for exact board names, the diode part, the paper, and its acknowledged implementer did not supply an additional generation artifact. Modern DSP products, unrelated accelerator manuals, and later BSAFE documents were not used to infer challenge behavior. No people were contacted and no cloud jobs were launched.

## Limited next step

No new seed sweep is justified by this recovery alone. First finish the matched predecessor-gap experiment already proposed in CONTEXT.md. If the continuous incremental model remains worth pursuing, a small local sensitivity audit can compare canonical signed 24-bit representation and per-word transfer orders against the current unsigned/big-endian reconstruction. Such an audit would be a conditional layout test, not a source-exact reconstruction, and should not automatically trigger an expanded exhaustive scan.

## Artifacts and verification

- `dusse-kaliski.pdf`: valid PDF, 15 pages; SHA-256 `ef14dcd3021d2817b83f8783e4072a398db70f28413862f30baff674801f966c`.
- `dusse-kaliski.txt`: complete Poppler text extraction; OCR errors remain, so the cited pages were checked visually.
- `dusse-kaliski-page236.png`, `dusse-kaliski-page-10.png`, and `dusse-kaliski-page-11.png`: rendered printed pages 236, 239, and 240 used for visual verification.
- `manifest.json`, `announcement-manifest.json`: source URLs, response metadata, byte counts, and hashes.
- `archive-query-manifest.json`: complete direct-query failure records.
- `COMMANDS.md`: retrieval and inspection commands, with search limits.
- `verification.json`: independently computed local artifact facts and target comparison.

No factorization result is claimed.
