# Commands and retrieval method

Working directory: `/Users/arth_rogerthat/Downloads/RSA-270`.

Retrieval used Python 3 `urllib.request.urlopen` with `User-Agent: Mozilla/5.0` and a 25-second timeout for the three URLs in `manifest.json`; response bodies were saved exactly as received. The announcement used a 15-second timeout. SHA-256 values were computed with `hashlib.sha256(data).hexdigest()`. DiscMaster used three independent requests in a `ThreadPoolExecutor(max_workers=3)` and a 20-second timeout. Exact request URLs and complete errors are in `archive-query-manifest.json`.

The first text-extraction probe attempted `import pymupdf`, which failed with `ModuleNotFoundError`; no installation was performed. System Poppler was available.

Successful extraction and render commands:

```sh
pdftotext -layout results/generator-evidence-2026-09-09/dusse-kaliski.pdf results/generator-evidence-2026-09-09/dusse-kaliski.txt
pdftoppm -f 7 -l 7 -scale-to 1500 -png -singlefile results/generator-evidence-2026-09-09/dusse-kaliski.pdf results/generator-evidence-2026-09-09/dusse-kaliski-page236
pdftoppm -f 10 -l 11 -scale-to 1500 -png results/generator-evidence-2026-09-09/dusse-kaliski.pdf results/generator-evidence-2026-09-09/dusse-kaliski-page
rg -in 'dsp|56000|kaliski|dusse|manual|hardware|random' results/generator-evidence-2026-09-09/ftp-listing.txt
```

All Poppler commands exited 0 with no output. The final `rg` command exited 1 with no output, meaning no matching filenames in this listing.

Web-search queries included exact `"RSA DSP"` with `manual`, `random`, `software`, `noise`, `card`, or `prime`; `"RSA-DSP"`; `"CND6002A" cryptographic`; `"Dusse" "56000" pdf "random"`; `"RSA Data Security" "Jeff Thompson" DSP`; and archive-specific `site:archive.org`, `site:discmaster.textfiles.com`, and `site:csrc.nist.gov` searches. These were discovery aids. No search snippet was used as sole evidence for a generator implementation. Search-engine result bodies were not retained, so the exact query requests in the tool transcript are the record of those negative discovery attempts.

The direct Springer URL returned a redirect containing `error=cookies_not_supported` but the final payload was a genuine PDF, not an HTML error page. Its exact final URL, content type, hash, and length are in `manifest.json`.
