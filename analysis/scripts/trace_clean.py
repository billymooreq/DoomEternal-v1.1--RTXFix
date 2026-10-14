# Produce a compact version of a Debug Pixel trace: drop anonymous changes and collapse scope-exit dumps.
import sys, re
src, dst = sys.argv[1], sys.argv[2]
out = open(dst, "w", encoding="utf-8")
pend = []
def flush():
    named = [p for p in pend if not p.startswith("[Unknown") and not p.startswith(" [Unknown")]
    if len(named) > 12:
        out.write("        (%d variable updates on scope exit, omitted)\n" % len(named))
    else:
        for p in named: out.write("        => " + p + "\n")
    pend.clear()
hdr = True
for line in open(src, encoding="utf-8"):
    if hdr:
        if line.startswith("## steps"): hdr = False
        continue
    if line.startswith("      "):
        s = line.strip()
        if s.startswith("[Unknown") or s.startswith("_604:") or s.startswith("_886:") or s.startswith("_697:") or s.startswith("_887:"):
            if s[0] == "_" and ":" in s.split(" ")[0]:
                pend.append(s[:300] + (" ..." if len(s) > 300 else ""))
            continue
        pend.append(s)
    elif line.startswith("["):
        flush(); out.write(line)
    else:
        flush(); out.write(line)
flush()
