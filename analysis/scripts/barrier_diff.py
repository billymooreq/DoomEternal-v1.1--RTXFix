# Diff of normalized barrier lists between two captures (EIDs stripped), plain Python.
import sys, difflib
def load(p):
    rows = []
    for l in open(p, encoding="utf-8"):
        eid, rest = l.strip().split(" ", 1)
        rows.append((int(eid), rest))
    return rows
a, b = load(sys.argv[1]), load(sys.argv[2])
sm = difflib.SequenceMatcher(None, [r[1] for r in a], [r[1] for r in b], autojunk=False)
for op, i1, i2, j1, j2 in sm.get_opcodes():
    if op == "equal":
        print("= %d barriers identical (A %d..%d, B %d..%d)" % (i2 - i1, a[i1][0], a[i2 - 1][0], b[j1][0], b[j2 - 1][0]))
        continue
    print("%s" % op.upper())
    for r in a[i1:i2]: print("  - A %6d %s" % r)
    for r in b[j1:j2]: print("  + B %6d %s" % r)
