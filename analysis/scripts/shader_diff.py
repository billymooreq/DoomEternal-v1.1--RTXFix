# Structural diff of two SPIR-V shaders whose debug names differ (current DOOM Eternal strips OpName):
# strip OpName/OpMemberName from both, decompile with spirv-cross, normalise _<id> identifiers, diff.
import sys, struct, subprocess, re, difflib, os
SPV_CROSS = r"C:\Program Files\RenderDoc\plugins\spirv\spirv-cross.exe"

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

a, b, tag = sys.argv[1], sys.argv[2], sys.argv[3]
outdir = os.path.dirname(os.path.abspath(sys.argv[0])) + r"\..\shader_diff"
sa, sb = os.path.join(outdir, tag + "_old_stripped.spv"), os.path.join(outdir, tag + "_new_stripped.spv")
strip_names(a, sa); strip_names(b, sb)
ga = normalise(decompile(sa, sa[:-4] + ".glsl")); gb = normalise(decompile(sb, sb[:-4] + ".glsl"))
d = list(difflib.unified_diff(ga, gb, "1.1/" + tag, "current/" + tag, n=2, lineterm=""))
open(os.path.join(outdir, tag + ".diff"), "w", encoding="utf-8").write("\n".join(d))
sm = difflib.SequenceMatcher(None, ga, gb, autojunk=False)
print("%s: old %d lines, new %d lines, similarity %.3f, diff lines %d" % (tag, len(ga), len(gb), sm.ratio(), sum(1 for l in d if l[:1] in "+-")))
