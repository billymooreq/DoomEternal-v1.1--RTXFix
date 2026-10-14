# Changes of one member of one struct variable in a Debug Pixel trace, with the instructions that produced
# them. Usage: trace_accum.py <trace> <structVar> <member>[,<member>...]
import re, sys
path, var, members = sys.argv[1], sys.argv[2], sys.argv[3].split(",")
last = {}
recent = []
step = 0
for line in open(path, encoding="utf-8"):
    m = re.match(r"\[\s*(\d+)\] inst\s+\d+ line\s+\d+ \| (.*)", line)
    if m:
        step = int(m.group(1)); recent = (recent + [m.group(2).strip()])[-6:]; continue
    s = line.strip()
    if not s.startswith(var + ":;"): continue
    for mem in members:
        mm = re.search(r";\s+%s \[[^\]]*\] = ([^;]*)" % re.escape(mem), s)
        if mm and last.get(mem) != mm.group(1).strip():
            last[mem] = mm.group(1).strip()
            print("step %5d %s.%s = %s    <= %s" % (step, var, mem, last[mem], " / ".join(recent[-3:])[:200]))
