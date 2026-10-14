# Structural diff of two SPIR-V shaders whose debug names differ (current DOOM Eternal strips OpName):
# strip OpName/OpMemberName from both, decompile with spirv-cross, normalise _<id> identifiers, diff.
# Plain Python 3. Usage:
#   python shader_diff.py <old.spv> <new.spv> <tag> [--renderdoc <RenderDoc install folder>]
# spirv-cross is taken from <RenderDoc>/plugins/spirv; the folder can also come from DOOMFIX_RENDERDOC.
# Results go to analysis/shader_diff/ (not committed: it contains game shader code).
import sys, struct, subprocess, re, difflib, os

args = sys.argv[1:]
renderdoc = os.environ.get("DOOMFIX_RENDERDOC", "")
if "--renderdoc" in args:
    i = args.index("--renderdoc")
    renderdoc = args[i + 1] if i + 1 < len(args) else ""
    del args[i:i + 2]
if len(args) != 3:
    sys.exit("usage: shader_diff.py <old.spv> <new.spv> <tag> [--renderdoc <RenderDoc install folder>]")
SPV_CROSS = os.path.join(renderdoc, "plugins", "spirv", "spirv-cross.exe")
if not renderdoc or not os.path.isfile(SPV_CROSS):
    sys.exit("spirv-cross.exe not found. Pass --renderdoc <RenderDoc install folder> "
             "or set DOOMFIX_RENDERDOC (current value: %r)." % renderdoc)

def strip_names(src, dst):
    w = open(src, "rb").read()
    words = struct.unpack("<%dI" % (len(w) // 4), w)
    out = list(words[:5]); i = 5
    while i < len(words):
        op, n = words[i] & 0xffff, words[i] >> 16
        if op not in (5, 6):          # OpName, OpMemberName
            out.extend(words[i:i + n])
        i += n
    open(dst, "wb").write(struct.pack("<%dI" % len(out), *out))

def decompile(spv, glsl):
    subprocess.run([SPV_CROSS, spv, "--vulkan-semantics", "--output", glsl], check=True, capture_output=True)
    return open(glsl, encoding="utf-8").read()

def normalise(code):
    code = re.sub(r"\b_\d+(_\d+)*\b", "_N", code)
    code = re.sub(r"\b_m\d+\b", "_mN", code)
    return [l.rstrip() for l in code.split("\n") if l.strip()]

a, b, tag = args
outdir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "shader_diff")
os.makedirs(outdir, exist_ok=True)
sa, sb = os.path.join(outdir, tag + "_old_stripped.spv"), os.path.join(outdir, tag + "_new_stripped.spv")
strip_names(a, sa); strip_names(b, sb)
ga = normalise(decompile(sa, sa[:-4] + ".glsl")); gb = normalise(decompile(sb, sb[:-4] + ".glsl"))
d = list(difflib.unified_diff(ga, gb, "1.1/" + tag, "current/" + tag, n=2, lineterm=""))
open(os.path.join(outdir, tag + ".diff"), "w", encoding="utf-8").write("\n".join(d))
sm = difflib.SequenceMatcher(None, ga, gb, autojunk=False)
print("%s: old %d lines, new %d lines, similarity %.3f, diff lines %d" % (tag, len(ga), len(gb), sm.ratio(), sum(1 for l in d if l[:1] in "+-")))
