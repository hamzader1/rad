# Performance investigation

Date: 2026-11-05

Scope: Raid's current macOS ARM64 code path and the shared editor/LSP code. This is a source-level investigation plus a release compile attempt. The current Mac typing benchmark did not start successfully, so the findings below distinguish measured behavior from code-path risks.

## Summary

There is no evidence that the primitive character insertion itself copies the whole file. `TB_Buffer` appends edits to a patch list. The larger costs are the work the editor repeats around those patches:

1. **Frame pacing sets a visible latency floor.** Raid processes input, rebuilds the UI, submits the frame, and presents it on the main thread. The Mac OpenGL backend requests swap interval 1. The previous 60 Hz release measurement was one presented frame, about 16.6 ms, from input to screen. Compiler optimization cannot remove that scheduling delay.
2. **Folding rescans the full document after every buffer version.** A normal keystroke increments the version. The text view then flattens the patched document and runs a byte scanner across it to rebuild fold ranges. This is the clearest avoidable per-edit O(file size) operation.
3. **rust-analyzer synchronization prepares full-file snapshots on the UI thread.** Changes are sent as full document text, with a 150 ms interval. Text materialization, JSON escaping, snapshot copying, and line-start indexing all happen in `rad_lsp_doc_send`; only pipe I/O is queued to worker threads. rust-analyzer itself is a separate process and can also compete for CPU.
4. **Patch compaction does full-file work after a 100 ms pause or 32 patches.** The UI thread materializes the document and hashes/submits it. Re-lexing is asynchronous, but the buffer remains marked busy until the new snapshot is ready, which keeps requesting frames.

These paths make the buffer representation, text view, frame loop, and LSP pipeline all relevant. For ordinary files the current implementation may still feel acceptable; cost grows with document size, typing pattern, active LSP, and number of visible views. The existing 3,000-line benchmark does not include rust-analyzer and does not establish current Mac latency.

## Findings

### 1. Main-thread frame and vsync pacing — confirmed baseline cost

`rad_frame` collects events, updates the UI, submits rendering, and ends the frame in one loop. On macOS, `wm_get_events` blocks in AppKit when the loop has no requested work, and the OpenGL context is configured with swap interval 1 before `flushBuffer` presents. That means a keypress is shown at the next present opportunity; release mode cannot make a 60 Hz display present more often than every 16.7 ms. At a higher refresh rate the interval is shorter, but the app still has a frame boundary and OS/compositor delay.

The old timing table records 16.6 ms input-to-present on a 60 Hz display in both debug and release. Release reduced UI-build CPU time from 4.1 ms avg / 4.7 ms p95 to 2.4 ms avg / 3.3 ms p95, but did not change the one-frame latency. This explains why an optimized build can still feel less immediate than an editor with a shorter input-to-present path.

Evidence: [frame loop](src/rad/rad_core.c#L6554), [Cocoa event wait](src/mac/window_manager/mac_window_manager.c#L1022), [Mac swap interval and present](src/render/opengl/mac/render_opengl_mac.c#L46), [historical measurements](docs/PROGRESS.md#L105).

### 2. Full-file fold scan on every edit — high-confidence editor cost

For editable text tabs, `rad_code_view_build` calls `rad_code_view_folds_update`. Its cache key is `buffer->version`. Every edit increments that version, so the view materializes the entire current text and calls `rad_fold_ranges_from_text` again. That scanner walks bytes through the whole file, skipping comments, strings, and literals while finding braces, comment runs, and `use` runs. The source comment explicitly describes this as once per buffer version.

This is separate from syntax highlighting: tokenization for code colors is restricted to the visible byte range. Folding is the full-file scan that runs on each text change. Large files or several editor views of the same file can pay for it repeatedly.

Evidence: [folding enabled for editable text views](src/rad/rad_views.c#L2180), [version invalidation and full text materialization](src/rad/rad_views.c#L1605), [full-file byte scan](src/rad/rad_views.c#L1415), [visible-range tokenization](src/rad/rad_views.c#L2292).

### 3. rust-analyzer full-text updates — UI serialization plus external CPU work

The LSP client sends a full-text `didChange`, rather than an incremental range edit. `rad_lsp_doc_send` synchronously:

- flattens the patched buffer into a contiguous string;
- JSON-escapes the entire file;
- formats the notification;
- copies the text into one of four retained version snapshots and scans it to build line starts.

The LSP writer/reader use background threads, so Raid does not wait synchronously for rust-analyzer to answer. However, the work that builds the full message and retained snapshot happens on the UI thread, and rust-analyzer's analysis consumes CPU in its own process. The 150 ms change timer is not a quiet-time debounce: the first observed edit starts the timer, and later edits do not reset it. Continuous typing can therefore send another full snapshot about every 150 ms.

This is a likely contributor when rust-analyzer is active, particularly for large files or a busy workspace. It is not the only cause: the folding scan and compaction paths run independently of the language server.

Evidence: [change interval](src/rad/rad_lsp.h#L39), [full-text serialization](src/rad/rad_lsp.c#L493), [snapshot copy and indexing](src/rad/rad_lsp.c#L195), [send cadence](src/rad/rad_lsp.c#L2460), [LSP worker threads](src/lsp/lsp.c#L383), [thread-safe outbox enqueue](src/lsp/lsp.c#L444).

### 4. Buffer compaction — full-file copy/hash on the UI thread

The editor buffer is a base blob plus an ordered edit log, not a rope or gap buffer. This avoids copying the entire file for a simple insertion in `tb_buffer_edit`, but `tb_buffer_tick` starts compaction when edits have paused for 100 ms or the patch count reaches 32. It rebuilds the patched memory map, copies the whole document into a new arena, then calls `c_submit_data`, which hashes the complete string before returning. `rad_documents_tick` calls this from the frame loop.

The new text artifact is lexed asynchronously. Until it is ready, patches remain pending; `tb_buffer_is_busy` returns true, and `rad_documents_tick` requests another frame. A large file can therefore cause a synchronous copy/hash hitch followed by extra active frames while re-lexing completes. Natural pauses longer than 100 ms can trigger this while someone is typing at a measured pace.

Evidence: [compaction thresholds](src/text_buffer/text_buffer.h#L60), [UI-thread compaction path](src/text_buffer/text_buffer.c#L177), [whole-text hash in `c_submit_data`](src/content/content.c#L159), [busy state](src/text_buffer/text_buffer.c#L208), [busy documents request frames](src/rad/rad_views.c#L296).

### 5. Rebuilding patched maps and visible lines — patch-count and view-count cost

Each text view first calls `tb_text_from_buffer`, then the code view constructs a `TXT_Patched` map again. After an edit it constructs that map again to update cursor positions, and the text view builds another map for its bottom-bar cursor data. `txt_patched_from_info_data_patches` applies every pending patch to memory, line, and token-point maps. Visible text is then copied line by line and tokenized each frame. Visible tokenization is bounded by the viewport, but the map work grows with pending patches and repeated/split views multiply it.

Evidence: [text view buffer read](src/rad/rad_views.c#L3183), [code view map construction](src/rad/rad_views.c#L1785), [post-edit map reconstruction](src/rad/rad_views.c#L2119), [bottom-bar map reconstruction](src/rad/rad_views.c#L3259), [patch application loop](src/text/text.c#L3035), [visible line copying](src/rad/rad_views.c#L2398).

### 6. Per-frame file checks scale with open documents

Every frame, `rad_documents_tick` loops through all live documents and calls `properties_from_file_path` (a `stat` on macOS), creates a file-stream key, and checks the text cache. A file watcher also exists, but this per-document polling still runs during each frame caused by typing. It is a secondary cost for a few files and can add up with many open documents.

Evidence: [document tick](src/rad/rad_views.c#L280), [macOS `stat`](src/mac/base/mac_base.c#L1440), [called each frame](src/rad/rad_core.c#L6903).

## What this does and does not say

- **Text buffer:** the individual edit appends a patch; it is not plainly doing an O(file size) replacement on every character. The costly behavior comes from repeatedly rebuilding maps, full-file fold scans, compaction, and LSP snapshots around that patch log.
- **Text editor:** the strongest direct typing-path issue is full-document fold-range regeneration per version. The editor also rebuilds its immediate-mode UI and view data when presenting a frame.
- **Architecture:** the single main-thread frame loop and vsynced present explain a nonzero latency floor even when CPU work is fast. The patch-based document and whole-file synchronization paths add size-dependent work.
- **rust-analyzer:** it can add CPU contention and the UI serializes full text for it, but protocol I/O is threaded and Raid does not synchronously wait for analysis results.
- **Release optimization:** macOS `build.sh release` uses `-O2 -DBUILD_DEBUG=0`. That improves instruction-level cost, but it cannot remove a present interval or turn full-file algorithms into incremental ones. `-g` debug symbols are not by themselves a runtime slowdown.
- **Syntax highlighting:** the interactive code-color lexer processes the visible text range. It is less suspicious than the full-file fold scan, although the base text artifact is lexed again when compaction produces a new snapshot.

## Measurements and limits

The existing timing entry is from a 3,000-line, 60 Hz benchmark. Its script opens `lines_3000.txt`, and LSP is off by default in script mode unless a script explicitly enables it. It measures aggregate UI-build time and input-to-present latency; it does not isolate fold scanning, compaction, LSP serialization, or OS/compositor latency. Treat it as a useful historical baseline, not a Mac or Rust+rust-analyzer result.

In this session, `build/raid` was a Mach-O ARM64 executable. A separate `-O2 -DBUILD_DEBUG=0` ARM64 executable compiled successfully. The perf script was attempted against both the existing binary and the separate optimized build. Both launches remained in startup and produced no `results.txt`/`DONE` record; the wrapper terminated them at 120 seconds and 15 seconds respectively. The app startup emitted Launch Services/XPC errors in this environment. These attempts do not provide Mac typing timings.

Per-function profiler macros are disabled by default (`PROFILE_TELEMETRY` and `PROFILE_SPALL` default to 0), and the Mac `build.sh` does not currently enable either one. The built-in script can report aggregate frame/UI-build latency, but there is no active per-function capture to identify which stage dominates on the user's machine.

## Recommended next work

1. Get the scripted perf app launch working on macOS, then benchmark the actual workload: `.rs` files at several sizes, with rust-analyzer on and off, natural pauses and continuous typing, and one versus multiple views of the same file. Record refresh rate and separate UI-build, whole-frame, and input-to-present percentiles.
2. Add stage timing around document ticking/compaction, `rad_code_view_folds_update`, patched-map construction, `rad_lsp_doc_send`, UI build, and present wait. Include text size, patch count, open view count, and LSP state in each sample.
3. Prioritize incremental fold-range updates and reuse of patched maps. Avoid flattening and scanning the whole file for ordinary edits.
4. Change LSP sync to incremental content changes and reset/debounce against the latest edit; keep document snapshot copying off the UI thread or retain a cheaper versioned representation.
5. Move compaction materialization/hash work off the UI thread, reconsider the 100 ms trigger, and stop forcing continuous frames when a background snapshot is simply waiting for lexing. Replace per-frame file stats with watcher-driven checks where possible.

The code identifies several real O(file size) paths, but a stage-level capture on the Mac is still needed to assign their share of the user's perceived delay.
