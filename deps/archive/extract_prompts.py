import json, re, sys

SESSIONS = [
    ("18e25685", "./-home-magik6k-lcad-wasm/18e25685-98b5-4959-935f-943b1af20789.jsonl"),
    ("17f09f89", "./-home-magik6k-lcad-wasm/17f09f89-ce7c-4f49-aa6a-cf0521026805.jsonl"),
    ("832fa851", "./-home-magik6k-lcad-wasm-freecad-port/832fa851-b88e-4605-884a-ab3517ceb9f6.jsonl"),
]

# commands whose args are meaningful human intent
MEANINGFUL_CMDS = {"goal","effort","loop"}

def get_text(m):
    c = m.get("content")
    if isinstance(c, str): return c
    if isinstance(c, list):
        return "".join(b.get("text","") for b in c if isinstance(b,dict) and b.get("type")=="text")
    return ""

def strip_reminders(t):
    t = re.sub(r"<system-reminder>.*?</system-reminder>", "", t, flags=re.S)
    return t

def classify(text):
    s = text.strip()
    if not s: return None
    # command wrappers
    if s.startswith("<command-name>"):
        name = re.search(r"<command-name>\s*/?([\w-]+)", s)
        args = re.search(r"<command-args>(.*?)</command-args>", s, flags=re.S)
        cmd = name.group(1) if name else "?"
        a = (args.group(1).strip() if args else "")
        if cmd in MEANINGFUL_CMDS:
            return ("cmd", cmd, a)
        return None  # noise command
    if s.startswith("<local-command-"): return None
    if s.startswith("<bash-") : return None
    if "This session is being continued from a previous conversation" in s[:120]: return None
    if s.startswith("Caveat:"): return None
    if s.startswith("[Request interrupted"): return None
    # after stripping reminders, is there anything?
    s2 = strip_reminders(s).strip()
    if not s2: return None
    # skip pure tool-result echoes / api error notes
    if s2.startswith("API Error") or s2.startswith("<task-notification>"): return None
    return ("text", None, s2)

rows = []
for sid, path in SESSIONS:
    try:
        fh = open(path)
    except FileNotFoundError:
        continue
    for ln, line in enumerate(fh):
        try: o = json.loads(line)
        except: continue
        if o.get("type") != "user": continue
        if o.get("isSidechain"): continue
        if o.get("isMeta"): continue
        m = o.get("message")
        if not isinstance(m, dict) or m.get("role")!="user": continue
        # skip tool_result-only
        c = m.get("content")
        if isinstance(c, list) and all(isinstance(b,dict) and b.get("type")=="tool_result" for b in c): 
            continue
        t = get_text(m)
        r = classify(t)
        if not r: continue
        kind, cmd, body = r
        rows.append({"sid":sid,"line":ln,"ts":o.get("timestamp"),"kind":kind,"cmd":cmd,"text":body})
    fh.close()

json.dump(rows, open(sys.argv[1],"w"), indent=1)
print(f"extracted {len(rows)} human prompts")
for r in rows:
    tag = f"[{r['sid']}] " + (f"/{r['cmd']} " if r['kind']=='cmd' else "")
    print(f"  {tag}{len(r['text'])}c :: {r['text'][:100].replace(chr(10),' ')}")
