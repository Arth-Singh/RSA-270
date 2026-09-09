# External status check, 2026-09-09

Performed by web search and page fetches on 2026-09-09 (KST afternoon). No local computation.

## RSA-270

Not factored. FactorDB returns status C (composite, no known factors) for the exact 270-digit value in data/target.json. The Wikipedia "RSA numbers" article (revision of 2026-09-06) states that RSA-270 has not been factored; the "RSA Factoring Challenge" table leaves its "factored on" and "factored by" cells empty. Searches for "RSA-270 factored" returned only RSA-260 coverage and a joke page. Not checkable: mersenneforum.org (HTTP 403) and the NMBRTHRY September 2026 archive (index page only), so those two negatives are unverified.

## RSA-260

Factored on 2026-09-03 by Eric Lu (@penlume on X, post 2095372672356212876, 04:45:49 UTC), who published one 130-digit factor. The cofactor and the full product were re-verified locally during this check (both factors 431 bits, Miller-Rabin passes, product equals the RSA-260 value). FactorDB now lists RSA-260 as fully factored. Method, hardware, and running time were not disclosed as of this date; there is no write-up, and the "pencil and paper" story circulating on social media originated as a coworker's joke.

Third-party effort estimates for RSA-260, none confirmed by Lu:

| Source | Estimate |
| --- | --- |
| Emmanuel Thomé, quoted by Scientific American | about three times RSA-250, so roughly 8,000 Xeon Gold 6130 core-years |
| Steve Weis, X thread of 2026-09-03 | 3,400 to 4,100 core-years with stock CADO-NFS on modern cores; about 25 GPU-years with a GPU port; his own run on up to 1,024 GPUs reached linear algebra in about two weeks |
| lilting.ch | about 7,000 core-years from L[1/3] scaling |

## Published reference points

| Number | Bits | Effort | Reference CPU | Source |
| --- | ---: | --- | --- | --- |
| RSA-240 | 795 | 800 core-years sieving + 100 matrix | Xeon Gold 6130, 2.1 GHz | caramba.loria.fr/dlp240-rsa240.txt |
| RSA-250 | 829 | 2,450 core-years sieving + 250 matrix | Xeon Gold 6130, 2.1 GHz | caramba.loria.fr/rsa250.txt |

## Derived estimate for 895 bits

L[1/3, (64/9)^(1/3)] ratios: L(829)/L(795) = 2.71 (observed 3.0), L(895)/L(829) = 6.49. Anchored on RSA-250 that gives about 17,500 Xeon Gold 6130 core-years; absorbing the observed exponent gives about 21,000; chaining Thomé's "three times per 33 bits" twice gives about 24,000; anchoring on RSA-240 gives about 16,000. Working range: 16,000 to 24,000 Xeon Gold 6130 core-years, roughly 6,000 to 12,000 core-years on 2026 cores, or 60 to 75 GPU-years if Weis's GPU figure for RSA-260 holds. The uncertainty is at least plus or minus 30 to 50 percent.

## Sources

- https://factordb.com/index.php?query=233108530344407544527637656910680524145619812480305449042948611968495918245135782867888369318577116418213919268572658314913060672626911354027609793166341626693946596196427744273886601876896313468704059066746903123910748277606548649151920812699309766587514735456594993207
- https://en.wikipedia.org/wiki/RSA_numbers
- https://en.wikipedia.org/wiki/RSA_Factoring_Challenge
- https://x.com/penlume/status/2095372672356212876
- https://x.com/sweis/status/2095570647036682673
- https://lilting.ch/en/articles/rsa-260-factored-how-computed
- https://www.johndcook.com/blog/2026/09/03/new-rsa-number-factored/
- https://www.scientificamerican.com/article/whats-the-tech-behind-the-record-breaking-rsa-260-crack/
- https://news.ycombinator.com/item?id=49546284
- https://caramba.loria.fr/rsa250.txt
- https://caramba.loria.fr/dlp240-rsa240.txt
- https://eprint.iacr.org/2020/697
- https://members.loria.fr/PZimmermann/records/factor.html
