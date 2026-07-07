# Session archive generator

These scripts build the browseable + downloadable archive of the Claude Code sessions
that produced this port (published at `deploy/sessions/` and linked from the writeup).

The archive is generated from the raw Claude Code transcripts under
`~/.claude/projects/<project-dir>/`. Three sessions make up the FreeCAD effort:

| session | project dir | what it covers |
|---------|-------------|----------------|
| `18e25685-…` | `-home-magik6k-lcad-wasm` | feasibility + first port (1 workflow, 46 agents) |
| `17f09f89-…` | `-home-magik6k-lcad-wasm` | main port, continued (12 subagents, 14 workflows, 99 agents) |
| `832fa851-…` | `-home-magik6k-lcad-wasm-freecad-port` | short parallel side session |

(A fourth session, `b70028ac-…`, is the LibreCAD writeup — explicitly *not* FreeCAD — and is excluded.)

## Pipeline

Run in order (paths are hard-coded near the top of each script — adjust `PROJ`/`OUT`/`SC` for your machine):

1. **`extract_prompts.py <out.json>`** — pulls every genuine human prompt from the three
   sessions (skips tool results, slash-command noise, continuation summaries, sidechains),
   emitting them with session id + line number.
2. **`build_archive.py`** — renders each session's transcript to HTML (`deploy/sessions/s/`),
   renders every sub-agent and workflow-agent transcript (`deploy/sessions/a/`), and writes the
   **sanitized** `freecad-web-sessions.tar.gz` (raw JSONL, secrets scrubbed). Run it from a
   directory that does **not** contain a `struct.py` (the stdlib gets shadowed otherwise).
3. **`build_index.py`** — deduplicates the extracted prompts (collapses compaction replays and
   prefix-duplicates), splices in the two side-session prompts at the workspace-move point, and
   emits `prompts_frag.html` + `goals.json`.
4. **`build_overview.py`** — assembles `deploy/sessions/index.html` (stats, the full prompt list,
   session cards, download link) from the fragment + `_meta.json`.

`splice_post.py` (kept alongside in the original build) injects the "Who did what" + prompt-list
section into the writeup (`deploy/index.html`).

## Sanitization

`build_archive.py`'s `SECRET_RES` scrubs credential-shaped strings (Anthropic/OpenAI/GitHub/AWS
keys, bearer tokens, PEM private-key blocks) and the author's email from **both** the rendered
HTML and the tarball. A scan of these sessions found no real secrets — the port never handled any —
but the pass runs regardless. Re-verify after regenerating:

```
tar xzOf deploy/sessions/freecad-web-sessions.tar.gz | grep -cE 'sk-ant-[A-Za-z0-9]|ghp_[A-Za-z0-9]{20}|<your-email>'
# expect 0
```
