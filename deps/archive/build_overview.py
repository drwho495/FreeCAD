#!/usr/bin/env python3
import json, os, html, re
SC="/tmp/claude-1000/-home-magik6k-lcad-wasm/17f09f89-ce7c-4f49-aa6a-cf0521026805/scratchpad/archbuild"
OUT="/home/magik6k/lcad-wasm/freecad-port/deploy/sessions"

# reuse the CSS from the generator
import importlib.util
spec=importlib.util.spec_from_file_location("ba", f"{SC}/build_archive.py")
# don't exec (it rebuilds); instead read CSS block out of the file
src=open(f"{SC}/build_archive.py").read()
CSS=re.search(r'CSS = """(.*?)"""', src, re.S).group(1)

meta=json.load(open(f"{OUT}/_meta.json"))
tarinfo=[m for m in meta if "tarsize" in m][0]
sess=[m for m in meta if "short" in m]
frag=open(f"{SC}/prompts_frag.html").read()
goals=json.load(open(f"{SC}/goals.json"))

tarmb=tarinfo["tarsize"]/1e6
nfiles=tarinfo["nfiles"]

cards=[]
blurbmap={m["short"]:m for m in sess}
ORDER=["18e25685","17f09f89","832fa851"]
spans={"18e25685":"Jul 3 – Jul 5","17f09f89":"Jul 4 – Jul 7","832fa851":"Jul 5"}
for sh in ORDER:
    m=blurbmap[sh]
    cards.append(
      f'<a class="card" href="s/{m["sid"]}.html"><h3>{m["title"]} '
      f'<span style="color:#6b7787;font-weight:400;font-size:12px">· {sh}</span></h3>'
      f'<p>{m["blurb"]}</p>'
      f'<div class="stats"><span>{spans[sh]}</span>'
      f'<span><b>{m["nsub"]}</b> subagents</span>'
      f'<span><b>{m["nwf"]}</b> workflows</span>'
      f'<span><b>{m["nwfa"]}</b> workflow agents</span></div></a>')

goal_html="".join(f'<li class="cmd">/goal {html.escape(g)}</li>' for g in goals)

body=f'''<div class="wrap">
<div class="top">
  <div><h1>FreeCAD-in-the-browser — session archive</h1>
  <div class="sub">The complete Claude Code transcripts that produced the port</div></div>
  <div class="bk"><a href="../index.html">← the writeup</a> · <a href="../app.html">launch the app →</a></div>
</div>

<p>This is the raw material behind <a href="../index.html">the writeup</a>: every session, every
sub-agent, every workflow that Fable ran to port FreeCAD to WebAssembly. It is published so the
claim in the post — that the human involvement was a few dozen high-level prompts and nothing more —
is <em>checkable</em>. Read the prompts below, then open any session and scroll the actual work.</p>

<h2>By the numbers</h2>
<table class="g">
<tr><td>Sessions</td><td><b>3</b> (Claude Code split the effort as context filled)</td></tr>
<tr><td>Span</td><td>2026-07-03 → 2026-07-07 (~4 days)</td></tr>
<tr><td>Human prompts, total</td><td><b>48</b> — listed in full below</td></tr>
<tr><td>Multi-agent workflows</td><td><b>15</b></td></tr>
<tr><td>Sub-agent invocations</td><td><b>159</b> (14 direct + 145 inside workflows)</td></tr>
<tr><td>Archive size (raw JSONL)</td><td>{nfiles} files · ~115&nbsp;MB uncompressed</td></tr>
</table>

<h2>All Magik&rsquo;s prompts — no other meaningful human involvement beyond these</h2>
<p class="sub">Every message the human sent, across all three sessions, in order. Giant pastes
(console logs, profiler dumps) are marked <span style="color:#8b98a8;font-style:italic">[like this]</span>;
everything else is verbatim. Two are from a short parallel session opened in <code>./freecad-port</code>.
The steering also included three <code>/goal</code> resets, shown after the list.</p>
{frag}

<p class="sub" style="margin-top:14px">The goal, as it was re-set over those four days:</p>
<ol class="prompts">{goal_html}</ol>

<h2>The sessions</h2>
{"".join(cards)}

<h2>Download</h2>
<p>The full, unabridged archive — all three top-level transcripts plus all {nfiles-3} sub-agent and
workflow-agent logs, as newline-delimited JSON:</p>
<a class="dl" href="freecad-web-sessions.tar.gz">⤓ freecad-web-sessions.tar.gz &nbsp;·&nbsp; {tarmb:.0f} MB</a>
<p class="note">Sanitized before packaging: credential-shaped strings (API keys, tokens, private-key
blocks) and the author&rsquo;s email are scrubbed. A scan turned up none of the former — the port
never handled secrets — but the pass runs regardless. The browseable pages above truncate long tool
outputs and thinking blocks for readability; the tarball is complete.</p>

<p class="note" style="margin-top:30px">Part of <a href="../index.html">FreeCAD in your browser</a> ·
<a href="../app.html">the app</a> · <a href="https://github.com/magik6k/freecad-web">source</a>.
Written and assembled by Fable.</p>
</div>'''

pagehtml=(f'<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">'
          f'<meta name="viewport" content="width=device-width,initial-scale=1">'
          f'<title>Session archive · FreeCAD in your browser</title><style>{CSS}</style></head><body>{body}</body></html>')
open(f"{OUT}/index.html","w").write(pagehtml)
print(f"wrote {OUT}/index.html ({len(pagehtml)} bytes)")
