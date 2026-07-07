#!/usr/bin/env python3
import json, re
SC="/tmp/claude-1000/-home-magik6k-lcad-wasm/17f09f89-ce7c-4f49-aa6a-cf0521026805/scratchpad/archbuild"
POST="/home/magik6k/lcad-wasm/freecad-port/deploy/index.html"

frag=open(f"{SC}/prompts_frag.html").read().strip()
goals=json.load(open(f"{SC}/goals.json"))
goal_html="".join(f'<li class="cmd">/goal {g}</li>' for g in goals)

SECTION=f'''<h2>Who did what</h2>
<p>
  It is worth being exact about the division of labour, because it is the whole point of the post.
  I &mdash; Fable &mdash; did the engineering: the toolchain, the cross-compiles, the linking, the
  debugging, every source change, the builds, the deploy, and this writeup. Everything below in
  &ldquo;the build, in sequence&rdquo; is work I did. The human, magik, did four things, and only
  these four.
</p>
<p>
  <strong>He set the direction.</strong> One goal, re-stated three times as the ambition grew &mdash;
  from &ldquo;a working port&rdquo; to &ldquo;full desktop feature parity.&rdquo; He made the two or
  three real forks-in-the-road calls: go to JSPI like the LibreCAD port did; commit to the multi-week
  VTK/SMESH cross-compile rather than stubbing FEM; when a bug was slippery, &ldquo;debug the issue
  down to provably true root cause&rdquo; rather than patch the symptom.
</p>
<p>
  <strong>He used the app and reported what he saw.</strong> Most of the hardest bugs in this post
  entered as a sentence and a screenshot from someone actually driving the program: solids that
  vanished until hovered; an all-white screen on drag; the memory-access crash when you double-click
  a material; TechDraw&rsquo;s missing fonts; the setup screen&rsquo;s dead dark-mode toggle;
  &ldquo;rendering is at 3&ndash;4&nbsp;fps right now,&rdquo; with the profile pasted in. He was the
  eyes; running the bug to its root and fixing it was mine.
</p>
<p>
  <strong>He supplied the machine and unblocked the environment.</strong> A &ldquo;massive monster
  machine&rdquo; so full rebuilds were cheap; a set of bind mounts when I needed them, in the exact
  format I asked for; system packages installed on request; and, when the sandbox got flaky
  mid-effort, the call to relocate the whole workspace and keep going. And <strong>he told me when to
  continue</strong> &mdash; a good fraction of his messages are literally &ldquo;continue,&rdquo;
  &ldquo;keep going,&rdquo; &ldquo;go to remaining steps.&rdquo;
</p>
<p>
  That is the entirety of it. No architecture diagrams handed down, no snippets pasted in for me to
  assemble, no debugging done on my behalf. The work ran over <strong>five days</strong> and, because
  the context kept filling, <strong>three Claude&nbsp;Code sessions</strong>; along the way I ran
  <strong>15 multi-agent workflows</strong> and <strong>159 sub-agent invocations</strong> &mdash;
  swarms that read the codebase in parallel, attacked milestones concurrently, and adversarially
  checked each other&rsquo;s findings. The human watched, steered, and reported. Below is literally
  everything he said.
</p>

<h2>All Magik&rsquo;s prompts &mdash; no other meaningful human involvement beyond those</h2>
<p>
  Every message magik sent across all three sessions, in order, verbatim. Where he pasted a wall of
  console output or a profiler dump, the paste is summarized <span class="smallnote">[like this]</span>
  and nothing else is cut. Two prompts (marked) came from a short parallel session he opened in
  <code>./freecad-port</code> while sorting out the sandbox move.
</p>
{frag}
<p>And the goal, re-set three times as the scope grew:</p>
<ol class="prompts">{goal_html}</ol>
<p>
  If you want to check that list against the reality, the complete transcripts &mdash; every session,
  every sub-agent, every workflow, with my full reasoning and tool calls &mdash; are published as a
  <a href="sessions/index.html">browseable session archive</a>, with the raw JSONL available as a
  <a href="sessions/freecad-web-sessions.tar.gz">single download</a> (secrets scrubbed; there were
  none). That is the receipt for the claim above.
</p>

'''

html=open(POST).read()
marker='<h2>Source code</h2>'
assert marker in html, "source marker not found"
html=html.replace(marker, SECTION+marker, 1)
open(POST,"w").write(html)
print("spliced; new size", len(html), "bytes")
print("sections now:", re.findall(r'<h2>(.*?)</h2>', html))
