# First prompt for Claude Code

Paste everything inside the block below as your first message. Make sure `CLAUDE.md`
is already in the repo root (the fork of raddebugger) before you start.

```
Read CLAUDE.md in the repo root completely. It is the project brief and the rules
we work by. Follow it.

We are starting milestone 1 only: build and read. Do NOT modify any source file in
this session. You may create notes under docs/notes/.

Tasks, in this order:

1. Confirm the build setup. Check what is installed on this machine (MSVC build
   tools, Windows SDK, or Clang). Install what is missing, checking current official
   docs rather than relying on memory. Then build with build.bat (debug mode) and
   tell me whether it worked.

2. Map the codebase. Read the layers in this order: base, ui, draw, render,
   font_provider and font_cache, os/core, os/gfx, text, and the code-view part of
   raddbg (RD_). For each one, give me a short mechanistic explanation: what it owns,
   what it depends on, and the key data structures. Use ASCII diagrams.

3. Explain one full frame. Trace how a frame goes from the OS event loop to pixels:
   events -> UI box tree built -> layout -> interaction state -> draw commands ->
   render. Name the functions and files involved so I can follow along in the source.

4. Explain how the read-only code view works. How does file content reach the view
   (file_stream, content cache, text layer, async lexing)? Where exactly does
   "read-only" come from, and which assumptions about immutability would an editable
   buffer break?

5. Answer the feasibility spike questions from CLAUDE.md section 7 using evidence from
   the code (cite file and line). If you cannot determine an answer, say so.

6. Save your findings as markdown files in docs/notes/ (one file per topic).

7. Finish with a proposal for milestone 2 (editor view without a debugger): which files
   you expect to touch, the risks, and the smallest first step. Then stop and wait for me.

Rules for this session: explain the mechanism, not just the result. Mark every claim
you did not verify in the code as unverified. If something in CLAUDE.md looks wrong
after reading the code, tell me directly.
```
