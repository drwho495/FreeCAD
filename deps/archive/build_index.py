#!/usr/bin/env python3
import json, re, html, os
SC="/tmp/claude-1000/-home-magik6k-lcad-wasm/17f09f89-ce7c-4f49-aa6a-cf0521026805/scratchpad"
OUT="/home/magik6k/lcad-wasm/freecad-port/deploy/sessions"
def esc(s): return html.escape(s)
def clean(s): return re.sub(r'\s+',' ',s).strip()

d=json.load(open(f"{SC}/uniq.json"))
prose={r['line']:clean(r['text']) for r in d['prose']}

# hand-authored display for prompts that had giant pastes appended.
# key = line number ; value = display string (human prose + bracketed marker for the paste)
OVERRIDE={
 2939: "progress? Somehow artifacts got smaller — <span class=mk>[+ pasted du output showing ../freecad-artifacts shrinking 4.5G → 3.6G]</span>",
 10917:"Similar to before, still see faces/outlines from context menu, but no longer see lines/outlines on just mouse over. Console: We have lots of errors, the readPixels one may be interesting? Other than that also pretty long promise chains: <span class=mk>[+ ~3,800 lines of WebGL console errors &amp; wasm stack traces pasted]</span>",
 4808: "Keep going through next stages, as previously do a workflow researching what's in play. We also have some gaps in the UI, maybe missing some Qt assets/setup — see @…scrot.png — context menus are very unstyled, no ui elements do anything on mouse-over, right tasks panel has no background, font is weirdly big etc. Also could try to improve perf a little bit, would get better profile trace but there are no wasm debug symbols it seems: <span class=mk>[+ profile trace pasted]</span>",
 6440: "Bottom-up perf in frame render doesn't do much sense to me, but this is what the profiler says — <span class=mk>[+ profile table: ffVertex / bufferData / Minor GC / texSubImage2D hot spots]</span>",
 11650:"What would it take to fix those: <span class=mk>[+ console errors — Cannot find icon: MassPropertiesIcon; No module named 'Tux_rc'; No module named 'urllib.request'; and missing workbench SVG icons for Assembly / BIM / CAM / Draft / Mesh / PartDesign / Points / Spreadsheet / Surface…]</span>",
 13874:"Seems FEM works correctly with the example showing correct deflection; In FEM example double clicking on material crashes: <span class=mk>[memory access out of bounds in QWidgetWindow::focusObject() → PythonConsole::printPrompt]</span> — in other example getting missing font/resource errors: <span class=mk>[TechDraw failed to load font osifont-lgpl3fe.ttf and 3 more]</span> — Also on the setup screen dark mode selection doesn't do anything. Any resources we should think about bundling / hot loading?",
 14202:"Think if we can win some rendering performance, bottom-up profile of a few seconds <span class=mk>[+ profile table]</span> — Rendering is at 3-4 fps right now",
}

# order the prose prompts; insert the two 832fa851 asides at the sandbox-move point (after line 2694 'Sandbox crashed')
order={"18e25685":0,"17f09f89":1,"832fa851":2}
rows=sorted(d['prose'], key=lambda r:(order[r['sid']], r['line']))

# build final ordered list of (kind, html) — kind in {human, aside}
side=[r for r in rows if r['sid']=='832fa851']
main=[r for r in rows if r['sid']!='832fa851']

items=[]
inserted_side=False
for r in main:
    line=r['line']
    disp = OVERRIDE.get(line, esc(prose[line]))
    if line not in OVERRIDE:
        disp = esc(prose[line])
    items.append(("human", disp))
    # after the 'Sandbox crashed' prompt, splice in the parallel-session asides
    if not inserted_side and prose[line].startswith("Sandbox crashed"):
        for sr in side:
            asd = esc(clean(sr['text']))
            items.append(("aside", '<span class=mk>[parallel ./freecad-port session]</span> '+asd))
        inserted_side=True
if not inserted_side:
    for sr in side:
        items.append(("aside", '<span class=mk>[parallel ./freecad-port session]</span> '+esc(clean(sr['text']))))

# collapse exact prefix-duplicates (e.g. a resend with an appended clause): keep the longer
def plain(hh): return re.sub('<[^>]+>','',hh)
dedup=[]
for it in items:
    if dedup:
        a=plain(dedup[-1][1]); b=plain(it[1])
        if b.startswith(a) and len(b)>len(a):   # current extends previous -> replace
            dedup[-1]=it; continue
        if a.startswith(b) and len(a)>len(b):    # current is a shorter prefix -> skip
            continue
    dedup.append(it)
items=dedup
print(f"total display items: {len(items)}")

# also note the steering commands (goal/effort) separately
goals=[c['text'] for c in d['cmds'] if c['cmd']=='goal']

def render_list(dark=True):
    li=[]
    for kind,hh in items:
        cls="aside" if kind=="aside" else ""
        li.append(f'<li class="{cls}">{hh}</li>')
    return "\n".join(li)

# emit fragment used by both index + post
frag = "<ol class=prompts>\n"+render_list()+"\n</ol>"
open(f"{SC}/archbuild/prompts_frag.html","w").write(frag)
open(f"{SC}/archbuild/goals.json","w").write(json.dumps(goals))
print("goals:", goals)
print("wrote prompts_frag.html")
