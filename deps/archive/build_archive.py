#!/usr/bin/env python3
"""Build a browseable + downloadable archive of the FreeCAD-wasm Claude Code sessions."""
import json, os, re, glob, html, tarfile, sys, io

PROJ = "/home/magik6k/.claude/projects"
OUT  = "/home/magik6k/lcad-wasm/freecad-port/deploy/sessions"

SESSIONS = [
    dict(sid="18e25685-98b5-4959-935f-943b1af20789",
         short="18e25685", title="Feasibility &amp; first port",
         blurb="The kickoff: assess whether FreeCAD can go to wasm at all, clone it, stand up the toolchain, and fight the 3D viewport to first pixels.",
         base=f"{PROJ}/-home-magik6k-lcad-wasm/18e25685-98b5-4959-935f-943b1af20789"),
    dict(sid="17f09f89-ce7c-4f49-aa6a-cf0521026805",
         short="17f09f89", title="Main port (continued)",
         blurb="The long haul: PySide6, the workspace relocation, the parity super-workflow, FEM Stage 1 &amp; 2, the render-perf pass, and the deploy.",
         base=f"{PROJ}/-home-magik6k-lcad-wasm/17f09f89-ce7c-4f49-aa6a-cf0521026805"),
    dict(sid="832fa851-b88e-4605-884a-ab3517ceb9f6",
         short="832fa851", title="Side session (freecad-port)",
         blurb="A short parallel session opened in ./freecad-port to work out how to resume the main effort after a sandbox hiccup.",
         base=f"{PROJ}/-home-magik6k-lcad-wasm-freecad-port/832fa851-b88e-4605-884a-ab3517ceb9f6"),
]

# ---------- sanitize ----------
SECRET_RES = [
    (re.compile(r'sk-ant-[A-Za-z0-9_-]*'),      '[REDACTED-ANTHROPIC-KEY]'),
    (re.compile(r'sk-[A-Za-z0-9]{40,}'),        '[REDACTED-KEY]'),
    (re.compile(r'gh[pousr]_[A-Za-z0-9]{20,}'), '[REDACTED-GH-TOKEN]'),
    (re.compile(r'AKIA[0-9A-Z]{16}'),           '[REDACTED-AWS-KEY]'),
    (re.compile(r'Bearer [A-Za-z0-9._-]{20,}'), 'Bearer [REDACTED]'),
    (re.compile(r'magik6k@gmail(\\?\.com)?'),   '[redacted-email]'),
    (re.compile(r'-----BEGIN[^-]*PRIVATE KEY-----.*?-----END[^-]*PRIVATE KEY-----', re.S), '[REDACTED-PRIVATE-KEY]'),
]
def sanitize(s):
    if not s: return s
    for rx, repl in SECRET_RES:
        s = rx.sub(repl, s)
    return s

# ---------- helpers ----------
def esc(s): return html.escape(s or "")

def trunc(s, n):
    s = s or ""
    if len(s) <= n: return s, False
    return s[:n], True

def block_text(content):
    """Extract concatenated text from a message.content (str or list)."""
    if isinstance(content, str): return content
    if isinstance(content, list):
        out=[]
        for b in content:
            if isinstance(b, dict) and b.get("type")=="text":
                out.append(b.get("text",""))
        return "".join(out)
    return ""

def preview_input(inp, n=240):
    try:
        s = json.dumps(inp, ensure_ascii=False)
    except Exception:
        s = str(inp)
    return trunc(s, n)

def tool_result_text(c):
    if isinstance(c, str): return c
    if isinstance(c, list):
        out=[]
        for b in c:
            if isinstance(b, dict):
                if b.get("type")=="text": out.append(b.get("text",""))
                elif b.get("type")=="image": out.append("[image]")
            else: out.append(str(b))
        return "\n".join(out)
    return str(c)

# ---------- render one message stream to HTML ----------
def render_stream(path, tool_cap=2200, think_cap=1800, max_msgs=None, agent_index=None):
    """agent_index: optional dict mapping to link tool-uses; here we just render."""
    parts=[]
    n=0
    with open(path) as fh:
        for line in fh:
            try: o=json.loads(line)
            except: continue
            typ=o.get("type")
            m=o.get("message")
            if typ=="user":
                if o.get("isMeta"):
                    # show slash commands as system chips, skip caveats
                    t=block_text(m.get("content")) if isinstance(m,dict) else ""
                    continue
                if not isinstance(m,dict): continue
                content=m.get("content")
                # tool_result?
                if isinstance(content,list) and any(isinstance(b,dict) and b.get("type")=="tool_result" for b in content):
                    for b in content:
                        if isinstance(b,dict) and b.get("type")=="tool_result":
                            txt=sanitize(tool_result_text(b.get("content")))
                            txt,cut=trunc(txt,tool_cap)
                            err=' err' if b.get("is_error") else ''
                            parts.append(f'<div class="msg tool{err}"><div class="rl">tool result</div><pre>{esc(txt)}{"  …[truncated]" if cut else ""}</pre></div>')
                    n+=1
                    if max_msgs and n>=max_msgs: break
                    continue
                # command wrapper?
                raw=block_text(content)
                s=raw.strip()
                if s.startswith("<command-name>"):
                    cmd=re.search(r"<command-name>\s*(/?[\w-]+)",s)
                    args=re.search(r"<command-args>(.*?)</command-args>",s,re.S)
                    a=(args.group(1).strip() if args else "")
                    label=esc(cmd.group(1) if cmd else "/cmd")+((" "+esc(a)) if a else "")
                    parts.append(f'<div class="msg cmd"><div class="rl">command</div><div class="bd">{label}</div></div>')
                    continue
                if s.startswith("<local-command-") or s.startswith("<bash-"):
                    continue
                # real human message (strip system-reminders)
                s=re.sub(r"<system-reminder>.*?</system-reminder>","",s,flags=re.S).strip()
                if not s: continue
                if "This session is being continued from a previous conversation" in s[:120]:
                    s2,cut=trunc(s, 900)
                    parts.append(f'<div class="msg cont"><div class="rl">↻ continued session — compaction summary</div><pre>{esc(sanitize(s2))}{"  …" if cut else ""}</pre></div>')
                    n+=1; continue
                s=sanitize(s)
                disp,cut=trunc(s, 6000)
                parts.append(f'<div class="msg human"><div class="rl">▸ Magik</div><div class="bd">{esc(disp)}{"  …" if cut else ""}</div></div>')
                n+=1
                if max_msgs and n>=max_msgs: break
            elif typ=="assistant":
                if not isinstance(m,dict): continue
                content=m.get("content")
                if not isinstance(content,list):
                    content=[{"type":"text","text":str(content)}]
                seg=[]
                for b in content:
                    if not isinstance(b,dict): continue
                    bt=b.get("type")
                    if bt=="text" and b.get("text","").strip():
                        seg.append(f'<div class="bd">{esc(sanitize(b["text"]))}</div>')
                    elif bt=="thinking" and b.get("thinking","").strip():
                        tk,cut=trunc(b["thinking"],think_cap)
                        seg.append(f'<details class="think"><summary>thinking</summary><pre>{esc(sanitize(tk))}{"  …" if cut else ""}</pre></details>')
                    elif bt=="tool_use":
                        name=esc(b.get("name","tool"))
                        pv,cut=preview_input(b.get("input"))
                        seg.append(f'<div class="tooluse"><span class="tn">⚙ {name}</span> <code>{esc(sanitize(pv))}{"…" if cut else ""}</code></div>')
                if seg:
                    parts.append(f'<div class="msg ai"><div class="rl">Fable</div>'+"".join(seg)+'</div>')
                    n+=1
                    if max_msgs and n>=max_msgs: break
    return "\n".join(parts), n

# ---------- page shell ----------
CSS = """
:root{--bg:#0e1116;--pan:#161b22;--pan2:#1c232c;--fg:#d7dde5;--mut:#8b98a8;}
*{box-sizing:border-box}
body{margin:0;background:#0e1116;color:#d7dde5;font:14px/1.6 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
a{color:#6cb6ff;text-decoration:none}a:hover{text-decoration:underline}
.wrap{max-width:920px;margin:0 auto;padding:26px 20px 80px}
.top{display:flex;justify-content:space-between;align-items:baseline;border-bottom:1px solid #263040;padding-bottom:12px;margin-bottom:22px;flex-wrap:wrap;gap:8px}
.top .bk{font-size:12.5px;color:#8b98a8}
h1{font-size:23px;margin:0 0 4px;font-weight:650;letter-spacing:-.2px}
h2{font-size:16px;margin:30px 0 10px;color:#cbd5e1;border-bottom:1px solid #212a35;padding-bottom:5px}
.sub{color:#8b98a8;font-size:13px;margin:0 0 20px}
.msg{margin:0 0 12px;padding:9px 13px;border-radius:8px;background:#161b22;border:1px solid #212a35}
.msg .rl{font-size:11px;letter-spacing:.4px;text-transform:uppercase;color:#7d8a9a;margin-bottom:5px;font-weight:600}
.msg.human{background:#12243a;border-color:#1d3a5c}
.msg.human .rl{color:#6cb6ff}
.msg.human .bd{white-space:pre-wrap;font-size:14.5px;color:#e8eef6}
.msg.ai{background:#141a21}
.msg.ai .rl{color:#9ae6b4}
.msg.ai .bd{white-space:pre-wrap;margin:2px 0}
.msg.cont{background:#1a1622;border-color:#3a2d4d}
.msg.cont .rl{color:#c4a3f0}
.msg.cmd{padding:5px 12px;background:#12181f;border-color:#222c37}
.msg.cmd .rl{display:inline;margin-right:8px}
.msg.cmd .bd{display:inline;font-family:ui-monospace,Menlo,monospace;color:#e0b060;font-size:13px}
.msg.tool{background:#10151b;border-color:#1c242e}
.msg.tool.err{border-color:#5c2b2b;background:#1c1414}
.msg.tool .rl{color:#6b7787}
.msg pre{margin:0;white-space:pre-wrap;word-break:break-word;font:12px/1.5 ui-monospace,Menlo,monospace;color:#aeb9c7;max-height:340px;overflow:auto}
.tooluse{font-family:ui-monospace,Menlo,monospace;font-size:12.5px;margin:3px 0;color:#7fd1c9}
.tooluse .tn{color:#e0b060}
.tooluse code{background:#0d1218;padding:1px 5px;border-radius:4px;color:#95a3b3;font-size:11.5px}
details.think{margin:4px 0}
details.think summary{cursor:pointer;color:#6b7787;font-size:11.5px;letter-spacing:.3px}
details.think pre{margin:5px 0 0;color:#7a8798;font-style:italic;background:#0d1218;padding:8px 10px;border-radius:6px}
.card{display:block;padding:15px 17px;margin:0 0 14px;background:#161b22;border:1px solid #222c37;border-radius:10px}
.card:hover{border-color:#345;text-decoration:none;background:#1a212a}
.card h3{margin:0 0 5px;font-size:16px;color:#e8eef6}
.card p{margin:0 0 8px;color:#9aa7b6;font-size:13px}
.stats{display:flex;gap:16px;flex-wrap:wrap;font-size:12px;color:#7d8a9a}
.stats b{color:#cbd5e1;font-weight:650}
table.g{width:100%;border-collapse:collapse;font-size:13px;margin:6px 0 18px}
table.g td{border:1px solid #222c37;padding:7px 11px}
table.g td:first-child{color:#8b98a8;width:42%}
.dl{display:inline-block;margin:6px 0;padding:9px 18px;background:#1f6feb;color:#fff;border-radius:7px;font-weight:600;font-size:13.5px}
.dl:hover{background:#388bfd;text-decoration:none}
.note{font-size:12px;color:#7d8a9a;font-style:italic}
ol.prompts{padding-left:0;list-style:none;counter-reset:p;margin:10px 0}
ol.prompts li{counter-increment:p;position:relative;padding:9px 12px 9px 46px;margin:0 0 7px;background:#12243a;border:1px solid #1d3a5c;border-radius:8px;white-space:pre-wrap;color:#e8eef6;font-size:13.5px}
ol.prompts li::before{content:counter(p);position:absolute;left:12px;top:9px;color:#6cb6ff;font-family:ui-monospace,monospace;font-size:12px;font-weight:700}
ol.prompts li .mk{color:#8b98a8;font-style:italic}
ol.prompts li.cmd{background:#12181f;border-color:#222c37;color:#e0b060;font-family:ui-monospace,Menlo,monospace;font-size:12.5px}
ol.prompts li.aside{background:#151b13;border-color:#2c3a20}
.agrp{margin:0 0 14px}
.agrp .wl{font-size:12px;color:#7d8a9a;margin:0 0 4px;font-family:ui-monospace,monospace}
.agrp a{display:block;padding:6px 11px;margin:0 0 5px;background:#12181f;border:1px solid #202a34;border-radius:6px;font-size:12.5px;color:#c4cdd8}
.agrp a:hover{border-color:#345;text-decoration:none}
.agrp a .k{color:#e0b060;font-family:ui-monospace,monospace}
"""

def page(title, body, css=True):
    return (f'<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">'
            f'<meta name="viewport" content="width=device-width,initial-scale=1">'
            f'<title>{title}</title><style>{CSS}</style></head><body>{body}</body></html>')

os.makedirs(OUT, exist_ok=True)
os.makedirs(f"{OUT}/s", exist_ok=True)
os.makedirs(f"{OUT}/a", exist_ok=True)

# ---------- gather agent files ----------
def agent_files(base):
    subs = sorted(glob.glob(f"{base}/subagents/agent-*.jsonl"))
    wfs  = sorted(glob.glob(f"{base}/subagents/workflows/*/agent-*.jsonl"))
    return subs, wfs

def first_human(path):
    with open(path) as fh:
        for line in fh:
            try:o=json.loads(line)
            except:continue
            if o.get("type")=="user" and not o.get("isMeta"):
                m=o.get("message")
                if isinstance(m,dict):
                    t=block_text(m.get("content")).strip()
                    if t and not t.startswith("<"):
                        return sanitize(re.sub(r"\s+"," ",t))[:150]
    return "(no prompt)"

def agent_html_name(p):
    return "a_"+os.path.basename(p).replace(".jsonl","").replace("agent-","")+".html"

print("Rendering sessions…")
session_meta=[]
for S in SESSIONS:
    base=S["base"]; sid=S["sid"]
    subs, wfs = agent_files(base)
    # render main transcript
    cap = 2200 if S["short"]!="17f09f89" else 1600
    tmax = None
    body_stream,_ = render_stream(base+".jsonl", tool_cap=cap, think_cap=1500, max_msgs=tmax)
    # agent index grouped by workflow
    wf_groups={}
    for p in wfs:
        wid=re.search(r'wf_[a-z0-9]+',p).group(0)
        wf_groups.setdefault(wid,[]).append(p)
    aidx=[]
    if subs:
        aidx.append('<div class="agrp"><div class="wl">direct subagents ('+str(len(subs))+')</div>')
        for p in subs:
            aidx.append(f'<a href="../a/{agent_html_name(p)}"><span class="k">agent</span> {esc(first_human(p))}</a>')
        aidx.append('</div>')
    for wid in sorted(wf_groups):
        g=wf_groups[wid]
        aidx.append(f'<div class="agrp"><div class="wl">▸ workflow {wid} — {len(g)} agents</div>')
        for p in g:
            aidx.append(f'<a href="../a/{agent_html_name(p)}"><span class="k">{wid}</span> · {esc(first_human(p))}</a>')
        aidx.append('</div>')
    # render each agent page
    for p in subs+wfs:
        ab,_ = render_stream(p, tool_cap=500, think_cap=700, max_msgs=500)
        wid=""
        mm=re.search(r'wf_[a-z0-9]+',p)
        if mm: wid=f' · workflow {mm.group(0)}'
        ah=(f'<div class="wrap"><div class="top"><div><h1>subagent transcript</h1>'
            f'<div class="sub">session {S["short"]}{wid}</div></div>'
            f'<div class="bk"><a href="../s/{sid}.html">← {S["short"]}</a> · <a href="../index.html">archive</a></div></div>'
            f'<div class="msg cont"><div class="rl">delegated task</div><div class="bd">{esc(first_human(p))}</div></div>'
            f'{ab}</div>')
        open(f"{OUT}/a/{agent_html_name(p)}","w").write(page(f"subagent · {S['short']}", ah))
    session_meta.append(dict(S=S, nsub=len(subs), nwf=len(wf_groups), nwfa=len(wfs)))
    # session page
    sbody=(f'<div class="wrap"><div class="top"><div><h1>{S["title"]}</h1>'
           f'<div class="sub">session {sid}</div></div>'
           f'<div class="bk"><a href="../index.html">← archive</a> · <a href="../../index.html">the writeup</a></div></div>'
           f'<p class="sub">{S["blurb"]}</p>'
           f'<div class="stats"><span><b>{len(subs)}</b> direct subagents</span>'
           f'<span><b>{len(wf_groups)}</b> workflows</span>'
           f'<span><b>{len(wfs)}</b> workflow agents</span></div>'
           f'<h2>Transcript</h2>{body_stream}'
           f'<h2>Subagents &amp; workflows</h2>{"".join(aidx) if aidx else "<p class=note>none</p>"}'
           f'</div>')
    open(f"{OUT}/s/{sid}.html","w").write(page(f"{S['title']} · session archive", sbody))
    print(f"  {S['short']}: main + {len(subs)+len(wfs)} agent pages")

# ---------- build sanitized tarball of raw jsonl ----------
print("Building sanitized tarball…")
tarpath=f"{OUT}/freecad-web-sessions.tar.gz"
nfiles=0
with tarfile.open(tarpath,"w:gz") as tar:
    for S in SESSIONS:
        base=S["base"]; sid=S["sid"]
        files=[base+".jsonl"]+glob.glob(f"{base}/**/*.jsonl",recursive=True)
        for f in files:
            rel=os.path.relpath(f, os.path.dirname(base))
            arc=f"freecad-web-sessions/{S['short']}/{rel}"
            data=io.BytesIO()
            with open(f,"rb") as src:
                raw=src.read().decode("utf-8","replace")
            raw=sanitize(raw)
            b=raw.encode("utf-8")
            ti=tarfile.TarInfo(arc); ti.size=len(b)
            tar.addfile(ti, io.BytesIO(b))
            nfiles+=1
tsize=os.path.getsize(tarpath)
print(f"  tarball: {nfiles} files, {tsize/1e6:.1f} MB -> {tarpath}")

# stash meta for index build
json.dump([dict(short=m["S"]["short"], sid=m["S"]["sid"], title=m["S"]["title"],
                blurb=m["S"]["blurb"], nsub=m["nsub"], nwf=m["nwf"], nwfa=m["nwfa"])
           for m in session_meta] + [dict(tarsize=tsize, nfiles=nfiles)],
          open(f"{OUT}/_meta.json","w"))
print("done stage 1")
