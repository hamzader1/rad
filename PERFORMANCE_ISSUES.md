# Performance investigation

Date: 2026-11-05

> **Update 2026-11-06:** findings 2 (folds), 3 (sync, UI-thread parsing), 4 (token/symbol remapping) and 5 (compaction pause, busy frames) are fixed, plus a macOS GL submit stall this report did not cover. Before/after numbers and what is left: `docs/PROGRESS.md`, journal 2026-11-06. Re-measure with `tests/perf/run_perf.sh`.

Scope: Raid's current macOS ARM64 code path and the shared editor/LSP code. This is a source-level investigation plus a release compile attempt. The current Mac typing benchmark did not start successfully, so the findings below distinguish measured behavior from code-path risks.

## Summary

There is no evidence that the primitive character insertion itself copies the whole file. `TB_Buffer` appends edits to a patch list. The larger costs are the work the editor repeats around those patches, plus a newly confirmed LSP-on slowdown in the available Mac script runs:

1. **The LSP-on typing measurements are much slower than the paired LSP-off runs.** On the same 3,002-line Rust fixture, 140 ms-spaced typing measured 4.81 ms average / 6.17 ms p95 input latency without LSP and 14.06 / 23.62 ms with LSP. Fast scripted input increased the gap: p95 UI build was 2.37 ms without LSP and 21.68 ms with LSP. These are correlation measurements, not a stage-level attribution, but the size and repeatability make LSP-on behavior a top investigation target.
2. **Semantic tokens and document symbols are remapped across intervening edits after every buffer version.** Each token or symbol range maps multiple offsets; each offset mapping walks the change log once to validate it and again to transform it. The work therefore scales with both token/symbol count and edits since the server response. The default Rust navigation bar also maps all symbols while typing.
3. **Folding rescans the full document after every buffer version.** A normal keystroke increments the version. The text view then flattens the patched document and runs a byte scanner across it to rebuild fold ranges. This is the clearest avoidable per-edit O(file size) operation that also runs without LSP.
4. **rust-analyzer synchronization prepares full-file snapshots on the UI thread.** Changes are sent as full document text, with a 150 ms interval. Text materialization, JSON escaping, snapshot copying, and line-start indexing all happen in `rad_lsp_doc_send`; only pipe I/O is queued to worker threads. rust-analyzer itself is a separate process and can also compete for CPU. During the 150 ms delay, the client explicitly requests another frame, so the app continues rebuilding frames while waiting to send.
5. **Patch compaction does full-file work after a 100 ms pause or 32 patches.** The UI thread materializes the document and hashes/submits it. Re-lexing is asynchronous, but the buffer remains marked busy until the new snapshot is ready, which keeps requesting frames.

These paths make the buffer representation, text view, frame loop, and LSP pipeline all relevant. The available Mac runs show that Raid can feel responsive on a small fixture with LSP off, while LSP-on typing and larger Rust files can exceed a frame budget by a wide margin. The measurements below do not include a verified optimized-build comparison, so they do not establish release-build performance.

## Findings

### 1. Main-thread frame and vsync pacing — confirmed baseline cost

`rad_frame` collects events, updates the UI, submits rendering, and ends the frame in one loop. On macOS, `wm_get_events` blocks in AppKit when the loop has no requested work, and the OpenGL context is configured with swap interval 1 before `flushBuffer` presents. That means a keypress is shown at the next present opportunity; release mode cannot make a 60 Hz display present more often than every 16.7 ms. At a higher refresh rate the interval is shorter, but the app still has a frame boundary and OS/compositor delay.

The old Windows timing table records 16.6 ms input-to-present on a 60 Hz display in both debug and release. Release reduced UI-build CPU time from 4.1 ms avg / 4.7 ms p95 to 2.4 ms avg / 3.3 ms p95, but did not change the one-frame latency. On macOS, startup samples `NSScreen.mainScreen.maximumFramesPerSecond` (falling back to 60) and the OpenGL context requests swap interval 1. A 120 Hz display can therefore provide a present opportunity about every 8.3 ms; it does not guarantee a 120 FPS render if CPU/UI work exceeds that budget. The rate is sampled at startup, so moving the window to another display may leave the app using the original screen's rate.

Evidence: [frame loop](src/rad/rad_core.c#L6554), [Cocoa event wait](src/mac/window_manager/mac_window_manager.c#L1022), [Mac swap interval and present](src/render/opengl/mac/render_opengl_mac.c#L46), [historical measurements](docs/PROGRESS.md#L105).

### 2. Full-file fold scan on every edit — high-confidence editor cost

For editable text tabs, `rad_code_view_build` calls `rad_code_view_folds_update`. Its cache key is `buffer->version`. Every edit increments that version, so the view materializes the entire current text and calls `rad_fold_ranges_from_text` again. That scanner walks bytes through the whole file, skipping comments, strings, and literals while finding braces, comment runs, and `use` runs. The source comment explicitly describes this as once per buffer version.

This is separate from syntax highlighting: tokenization for code colors is restricted to the visible byte range. Folding is the full-file scan that runs on each text change. Large files or several editor views of the same file can pay for it repeatedly.

Evidence: [folding enabled for editable text views](src/rad/rad_views.c#L2180), [version invalidation and full text materialization](src/rad/rad_views.c#L1605), [full-file byte scan](src/rad/rad_views.c#L1415), [visible-range tokenization](src/rad/rad_views.c#L2292).

### 3. rust-analyzer full-text updates and forced frames — UI serialization plus external CPU work

The LSP client sends a full-text `didChange`, rather than an incremental range edit. `rad_lsp_doc_send` synchronously:

- flattens the patched buffer into a contiguous string;
- JSON-escapes the entire file;
- formats the notification;
- copies the text into one of four retained version snapshots and scans it to build line starts.

The LSP writer/reader use background threads, so Raid does not wait synchronously for rust-analyzer to answer. However, the work that builds the full message and retained snapshot happens on the UI thread, and rust-analyzer's analysis consumes CPU in its own process. The 150 ms change timer is not a quiet-time debounce: the first observed edit starts the timer, and later edits do not reset it. Continuous typing can therefore send another full snapshot about every 150 ms.

This is a likely contributor when rust-analyzer is active, particularly for large files or a busy workspace. While a changed document waits for the 150 ms send time, `rad_lsp_tick` calls `rad_request_frame()` again, so the UI keeps producing frames instead of sleeping until the send deadline. That repeats the editor's normal per-frame work during the wait and can consume more CPU on higher-refresh displays. The delay starts on the first observed edit and does not reset for every later keystroke, so it is not a quiet-time debounce.

The LSP reader and writer use background threads, but JSON parsing is not off the UI thread: `rad_lsp_server_take_messages` drains response bodies and calls `lsp_message_from_body` from the app tick. Semantic-token responses are then decoded into token arrays there. This can add response-time spikes for large responses; the current measurements do not isolate its cost.

Evidence: [change interval](src/rad/rad_lsp.h#L39), [full-text serialization](src/rad/rad_lsp.c#L493), [snapshot copy and indexing](src/rad/rad_lsp.c#L195), [send cadence and repeated frame request](src/rad/rad_lsp.c#L2460), [UI-thread response parsing](src/rad/rad_lsp.c#L787), [LSP tick on the main frame path](src/rad/rad_core.c#L6910), [semantic token decoding](src/rad/rad_lsp.c#L1531), [LSP worker threads](src/lsp/lsp.c#L383), [thread-safe outbox enqueue](src/lsp/lsp.c#L444).

### 4. Per-version semantic-token and symbol remapping — likely LSP-on typing cost

When a fresh semantic-token response is available, `rad_lsp_token_colors_from_document` carries every token range from the server's document version to the current buffer version. It is called while building the Rust code view. The default navigation bar similarly calls `rad_lsp_symbols_from_document` and maps every symbol's range and selection offset on each new buffer version. Both paths allocate a new mapped array on version changes.

`tb_buffer_map_offset_side` first walks every intervening change to validate the change log, then walks the same changes again to transform one offset. A semantic token maps two endpoints; a document symbol maps three offsets. The resulting cost is O((semantic token count + symbol count) × edits since the server version), with multiple passes over the edit history per item. If rust-analyzer is behind during a typing burst, the stale ranges can be carried across many edits (up to the bounded change-log window). The app can repeat this for each new version even though there has been no new LSP response. This is a strong source-level candidate for the large LSP-on UI-build numbers, but no per-function profile proves its exact share yet.

Evidence: [semantic ranges remapped per buffer version](src/rad/rad_lsp.c#L1612), [symbol ranges remapped per version](src/rad/rad_lsp.c#L1414), [offset mapping scans the intervening change log](src/text_buffer/text_buffer.c#L251), [semantic tokens requested for settled text](src/rad/rad_lsp.c#L2473), [semantic colors consumed in the code view](src/rad/rad_views.c#L2362), [navigation bar invokes the symbol mapper every frame](src/rad/rad_views.c#L836), [navigation bar enabled by default](src/rad/rad.mdesk#L1258).

### 5. Buffer compaction — full-file copy/hash on the UI thread

The editor buffer is a base blob plus an ordered edit log, not a rope or gap buffer. This avoids copying the entire file for a simple insertion in `tb_buffer_edit`, but `tb_buffer_tick` starts compaction when edits have paused for 100 ms or the patch count reaches 32. It rebuilds the patched memory map, copies the whole document into a new arena, then calls `c_submit_data`, which hashes the complete string before returning. `rad_documents_tick` calls this from the frame loop.

The new text artifact is lexed asynchronously. Until it is ready, patches remain pending; `tb_buffer_is_busy` returns true, and `rad_documents_tick` requests another frame. A large file can therefore cause a synchronous copy/hash hitch followed by extra active frames while re-lexing completes. Natural pauses longer than 100 ms can trigger this while someone is typing at a measured pace.

Evidence: [compaction thresholds](src/text_buffer/text_buffer.h#L60), [UI-thread compaction path](src/text_buffer/text_buffer.c#L177), [whole-text hash in `c_submit_data`](src/content/content.c#L159), [busy state](src/text_buffer/text_buffer.c#L208), [busy documents request frames](src/rad/rad_views.c#L296).

### 6. Rebuilding patched maps and visible lines — patch-count and view-count cost

Each text view first calls `tb_text_from_buffer`, then the code view constructs a `TXT_Patched` map again. After an edit it constructs that map again to update cursor positions, and the text view builds another map for its bottom-bar cursor data. `txt_patched_from_info_data_patches` applies every pending patch to memory, line, and token-point maps. Visible text is then copied line by line and tokenized each frame. Visible tokenization is bounded by the viewport, but the map work grows with pending patches and repeated/split views multiply it.

Evidence: [text view buffer read](src/rad/rad_views.c#L3183), [code view map construction](src/rad/rad_views.c#L1785), [post-edit map reconstruction](src/rad/rad_views.c#L2119), [bottom-bar map reconstruction](src/rad/rad_views.c#L3259), [patch application loop](src/text/text.c#L3035), [visible line copying](src/rad/rad_views.c#L2398).

### 7. Per-frame file checks scale with open documents

Every frame, `rad_documents_tick` loops through all live documents and calls `properties_from_file_path` (a `stat` on macOS), creates a file-stream key, and checks the text cache. A file watcher also exists, but this per-document polling still runs during each frame caused by typing. It is a secondary cost for a few files and can add up with many open documents.

Evidence: [document tick](src/rad/rad_views.c#L280), [macOS `stat`](src/mac/base/mac_base.c#L1440), [called each frame](src/rad/rad_core.c#L6903).

### 8. Rebuilding theme patterns and text runs — secondary per-frame overhead

Every window frame walks the theme tree, splits color tags, parses color values, and rebuilds the theme-pattern arrays. The theme normally changes rarely, so this work appears cacheable. Font glyph raster data is cached, but the string-to-run cache is allocated anew for each font style each frame; repeated UI labels and visible text can be hashed and laid out again. These paths run on every frame and may contribute to baseline UI-build time, but they are lower-confidence contributors than the measured LSP-on behavior and full-file fold scan.

Evidence: [theme traversal and reconstruction](src/rad/rad_core.c#L2190), [per-frame font-run cache reset](src/font_cache/font_cache.c#L563).

## What this does and does not say

- **Text buffer:** the individual edit appends a patch; it is not plainly doing an O(file size) replacement on every character. The costly behavior comes from repeatedly rebuilding maps, full-file fold scans, compaction, and LSP snapshots around that patch log.
- **Text editor:** the strongest direct typing-path issue is full-document fold-range regeneration per version. The editor also rebuilds its immediate-mode UI and view data when presenting a frame.
- **Architecture:** the single main-thread frame loop and vsynced present explain a nonzero latency floor even when CPU work is fast. The patch-based document and whole-file synchronization paths add size-dependent work.
- **rust-analyzer:** it can add CPU contention and the UI serializes full text for it, but protocol I/O is threaded and Raid does not synchronously wait for analysis results.
- **LSP range carrying:** semantic-token and document-symbol ranges are remapped against the edit log per buffer version; this is a further UI-thread cost not called out in the first report. The Mac script results show a strong LSP-on slowdown, but stage timings are needed to quantify how much comes from range carrying, JSON parsing, snapshots, and rust-analyzer CPU.
- **Release optimization:** macOS `build.sh release` uses `-O2 -DBUILD_DEBUG=0`. That improves instruction-level cost, but it cannot remove a present interval or turn full-file algorithms into incremental ones. `-g` debug symbols are not by themselves a runtime slowdown.
- **Syntax highlighting:** the interactive code-color lexer processes the visible text range. It is less suspicious than the full-file fold scan, although the base text artifact is lexed again when compaction produces a new snapshot.

## Measurements and limits

The old timing entry is from a Windows 3,000-line, 60 Hz benchmark. A Mac script run also uses a 3,002-line Rust fixture and explicitly enables LSP for the paired case. Completed outputs in the local `.perf_tmp` directory report:

| Mac scripted scenario | Frame avg / p95 | UI build avg / p95 | Input latency avg / p95 |
|---|---:|---:|---:|
| 3,002 lines, LSP off, 140 ms between keys | 4.15 / 6.00 ms | 0.64 / 0.73 ms | 4.81 / 6.17 ms |
| Same file, LSP on, 140 ms between keys | 8.19 / 16.73 ms | 1.41 / 2.48 ms | 14.06 / 23.62 ms |
| 3,002 lines, fast scripted text, LSP off | 5.55 / 7.63 ms | 1.94 / 2.37 ms | 5.60 / 7.64 ms |
| Same file, fast scripted text, LSP on | 17.40 / 27.50 ms | 12.36 / 21.68 ms | 17.60 / 27.53 ms |
| 5,000 lines, fast scripted text, LSP off | 6.13 / 8.91 ms | 2.54 / 2.71 ms | 6.19 / 8.95 ms |
| 10,000 lines, fast scripted text, LSP off | 8.36 / 10.85 ms | 4.47 / 4.87 ms | 8.45 / 10.86 ms |
| 20,000 lines, fast scripted text, LSP off | 11.82 / 14.33 ms | 8.30 / 8.91 ms | 12.06 / 14.68 ms |
| 5,002 lines, fast scripted text, LSP on | 24.08 / 40.46 ms | 19.67 / 35.27 ms | 24.48 / 40.50 ms |
| 10,002 lines, fast scripted text, LSP on | 43.88 / 74.88 ms | 38.69 / 68.07 ms | 44.60 / 75.00 ms |
| 5,002 lines, long fast-input stress case, LSP on | 91.37 / 199.44 ms | 71.49 / 173.18 ms | 92.70 / 199.78 ms |

These completed runs are important evidence: LSP-on p95 UI-build time grows from 21.68 ms at about 3k lines to 68.07 ms at about 10k lines, while LSP-off p95 UI-build time on the 3k fixture is 2.37 ms in the paired fast-input run. Even with LSP off, the 20k-line fast-input run has 8.91 ms p95 UI build, slightly above one 120 Hz frame budget. The long-input stress case shows severe stalls but is not representative of ordinary typing. The raw scripts and results are in `.perf_tmp/` (`nolsp_small_human`, `lsp_small_human`, `nolsp_small`, `lsp_small`, `type_5000`, `type_10000`, `type_20000`, `lsp_type`, `lsp_10k`, and `lsp_type_long`). Their outputs do not record compiler flags, so these numbers cannot safely be labeled as release-build measurements. They are scripted measurements, not direct physical-keyboard measurements, and they do not isolate per-function costs or the OS/compositor.

I rebuilt `build/raid` with `./build.sh raid release no_meta`; compilation succeeded. Launching the release binary with the no-LSP typing script stalled during startup and emitted Launch Services/XPC errors, so it did not produce a release timing record. The previous wrapper attempts had the same startup problem. The original `build/raid` binary was restored after this check. Therefore, a Mac release comparison remains unverified.

Per-function profiler macros are disabled by default (`PROFILE_TELEMETRY` and `PROFILE_SPALL` default to 0), and the Mac `build.sh` does not currently enable either one. The built-in script can report aggregate frame/UI-build latency, but there is no active per-function capture to identify which stage dominates on the user's machine.

## Recommended next work

1. Fix the macOS scripted-app startup path, then repeat the paired LSP-on/off cases on a verified release binary. Record build flags, refresh rate, fixture size, and input method with every result.
2. Add stage timings around semantic-token and symbol remapping, UI-thread LSP response parsing, fold updates, patched-map construction, full-text snapshots, compaction, UI build, and present wait. Include text size, token/symbol count, change-log length, patch count, view count, and LSP state.
3. Make semantic token/symbol range carrying incremental or batch-map ranges in one pass over the change log. Reuse mapped data where possible, and avoid remapping invisible tokens.
4. Prioritize incremental fold-range updates and reuse of patched maps; avoid flattening and scanning the whole file for ordinary edits.
5. Change LSP sync to incremental content changes and reset/debounce against the latest edit; keep snapshot serialization off the UI thread and replace repeated frame requests during the 150 ms delay with a deadline wake-up.
6. Move compaction materialization/hash work off the UI thread, reconsider the 100 ms trigger, and replace per-frame file stats with watcher-driven checks where possible. Cache theme patterns and consider persistent font-run caching after profiling.

The measurements establish that LSP-on typing can miss both 60 Hz and 120 Hz frame budgets on the tested files, even before compositor delay. They do not prove which LSP sub-path is dominant or establish release-build performance. A stage-level capture on a successfully launched optimized build is still needed to rank causes precisely.
