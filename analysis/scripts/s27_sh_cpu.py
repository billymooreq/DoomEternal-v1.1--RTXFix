# CPU re-implementation of the vertex-shader ambient lookup (octree atlas -> SH atlas) for the weapon
# vertices, using the GPU post-VS viewVector and the camera origin. Independent of the shader debugger.
# Env: DOOMFIX_DRAW, DOOMFIX_TARGET, DOOMFIX_PIXEL, DOOMFIX_CAM="x,y,z" (camera / localvieworigin).
import sys, os, struct, math
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *

SUB = rd.Subresource(0, 0, 0)
DRAW = int(os.environ["DOOMFIX_DRAW"]); TARGET = int(os.environ["DOOMFIX_TARGET"])
X, Y = [int(v) for v in os.environ.get("DOOMFIX_PIXEL", "1437,917").split(",")]
CAM = [float(v) for v in os.environ["DOOMFIX_CAM"].split(",")]


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def half(h):
    return struct.unpack("<e", struct.pack("<H", h))[0]


class Atlas:
    def __init__(self, data, w, h, d, fmt):
        self.data, self.w, self.h, self.d, self.fmt = data, w, h, d, fmt

    def texel(self, x, y, z):
        x = min(max(x, 0), self.w - 1); y = min(max(y, 0), self.h - 1); z = min(max(z, 0), self.d - 1)
        i = (z * self.h + y) * self.w + x
        if self.fmt == "rgba8":
            return [f32(c / 255.0) for c in self.data[i * 4:i * 4 + 4]]
        return [half(v) for v in struct.unpack_from("<4H", self.data, i * 8)]

    def nearest(self, tc):
        return self.texel(int(math.floor(tc[0] * self.w)), int(math.floor(tc[1] * self.h)), int(math.floor(tc[2] * self.d)))

    def trilinear(self, tc):
        p = [tc[0] * self.w - 0.5, tc[1] * self.h - 0.5, tc[2] * self.d - 0.5]
        b = [int(math.floor(v)) for v in p]
        f = [v - math.floor(v) for v in p]
        acc = [0.0] * 4
        for dz in (0, 1):
            for dy in (0, 1):
                for dx in (0, 1):
                    wgt = (f[0] if dx else 1 - f[0]) * (f[1] if dy else 1 - f[1]) * (f[2] if dz else 1 - f[2])
                    t = self.texel(b[0] + dx, b[1] + dy, b[2] + dz)
                    acc = [a + wgt * c for a, c in zip(acc, t)]
        return acc


def lookup(octree, shatlas, world):
    """Mirror of the VS code (spirv-cross vert_15508.glsl, lines ~640-685). Returns (leaf, key, SH0..2)."""
    p = [min(max((c + 4096.0) / 8192.0, 0.0), 1.0) for c in world]
    octant = [0, 0, 0]
    s = [0.0] * 4
    for _ in range(13):
        quad = [0.5 + math.floor(2.0 * v) for v in p]
        tc = [(octant[i] + quad[i]) / [octree.w, octree.h, octree.d][i] for i in range(3)]
        s = octree.nearest(tc)
        p = [2.0 * v - math.floor(2.0 * v) for v in p]
        octant = [int(f32(s[2] * 255.0)), int(f32(s[1] * 255.0)), int(f32(s[0] * 255.0))]
        if s[3] > 0: break
    key = (int(f32(s[3] * 255.0)) << 24) | (int(f32(s[2] * 255.0)) << 16) | (int(f32(s[1] * 255.0)) << 8) | int(f32(s[0] * 255.0))
    k = [(key >> 19) & 4095, (key >> 7) & 4095, key & 127]
    q = [2.0 * (v * 0.5 + 0.25) for v in p]
    tc = [(k[0] + q[0]) / shatlas.w, (k[1] + q[1]) / shatlas.h, (k[2] + q[2]) / shatlas.d]
    r = shatlas.trilinear(tc)
    g = shatlas.trilinear([tc[0], tc[1], tc[2] + 2.0 / shatlas.d])
    b = shatlas.trilinear([tc[0], tc[1], tc[2] + 4.0 / shatlas.d])
    sh0 = (r[0], g[0], b[0], r[1])
    return s, key, k, sh0, (r, g, b)


def body(ctrl, log):
    names = res_names(ctrl); texs = tex_map(ctrl)
    acts = {a.eventId: a for a, p in flatten(ctrl.GetRootActions())}
    tgt = [r for r in names if int(r) == TARGET][0]
    prim = [m.primitiveID for m in ctrl.PixelHistory(tgt, X, Y, SUB, rd.CompType.Typeless) if m.eventId == DRAW and m.Passed()][0]
    ctrl.SetFrameEvent(DRAW, True)
    st = ctrl.GetPipelineState()
    refl = st.GetShaderReflection(rd.ShaderStage.Vertex)
    atl = {}
    for u in st.GetReadOnlyResources(rd.ShaderStage.Vertex, True):
        t = texs.get(u.descriptor.resource)
        if t and t.type == rd.TextureType.Texture3D:
            kind = "rgba8" if t.format.Name().startswith("R8G8B8A8") else "rgba16f"
            atl[kind] = Atlas(ctrl.GetTextureData(u.descriptor.resource, SUB), t.width, t.height, t.depth, kind)
    octree, shatlas = atl["rgba8"], atl["rgba16f"]
    sig = [(s.varName, s.regIndex, s.regChannelMask, s.compCount) for s in refl.outputSignature]
    log("camera origin used: %s" % CAM)
    a = acts[DRAW]
    ib = st.GetIBuffer(); stride = ib.byteStride or 2
    idxs = struct.unpack({2: "<3H", 4: "<3I"}[stride], ctrl.GetBufferData(ib.resourceId, ib.byteOffset + (a.indexOffset + prim * 3) * stride, 3 * stride))
    mf = ctrl.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
    off = 0; lay = []
    for nm, reg, mask, c in sig:
        lay.append((nm, reg, off, c)); off += 4 * c
    # viewVector = scalar outputs at location 18 components 1..3 (vofi_Scalar4..6 in 1.1)
    vv_slots = [x for x in lay if x[1] == 18 and x[3] == 1][1:4]
    log("viewVector taken from outputs: %s" % [x[0] for x in vv_slots])
    for k, vi in enumerate(idxs):
        isz = mf.indexByteStride or 4
        slot = struct.unpack("<I" if isz == 4 else "<H", ctrl.GetBufferData(mf.indexResourceId, mf.indexByteOffset + (prim * 3 + k) * isz, isz))[0]
        raw = ctrl.GetBufferData(mf.vertexResourceId, mf.vertexByteOffset + slot * mf.vertexByteStride, mf.vertexByteStride)
        vv = [struct.unpack_from("<f", raw, x[2])[0] for x in vv_slots]
        world = [CAM[i] + vv[i] for i in range(3)]
        leaf, key, kk, sh0, rgb = lookup(octree, shatlas, world)
        log("vertex %d: world=%s leaf=%s key=0x%08x SHkey=%s SH0(CPU)=%s" % (
            vi, [round(v, 3) for v in world], [round(v, 4) for v in leaf], key, kk, [round(v, 5) for v in sh0]))
    # sensitivity: grid around the first vertex, and the camera itself
    raw = None
    log("\nsensitivity around the weapon (step 0.25 units, +-1 unit):")
    base = [CAM[i] + v for i, v in enumerate(vv)]
    zero = nonzero = 0; keys = {}
    for dx in [i * 0.25 for i in range(-4, 5)]:
        for dy in [i * 0.25 for i in range(-4, 5)]:
            for dz in [i * 0.25 for i in range(-4, 5)]:
                _, key, kk, sh0, _ = lookup(octree, shatlas, [base[0] + dx, base[1] + dy, base[2] + dz])
                keys[tuple(kk)] = keys.get(tuple(kk), 0) + 1
                if any(abs(v) > 1e-6 for v in sh0): nonzero += 1
                else: zero += 1
    log("  grid points with zero SH: %d, non-zero: %d; SH keys hit: %s" % (zero, nonzero, sorted(keys.items(), key=lambda kv: -kv[1])[:8]))
    for label, pt in [("camera", CAM), ("1 unit forward/up", [CAM[0], CAM[1] + 1, CAM[2] + 1]), ("4 units +x", [CAM[0] + 4, CAM[1], CAM[2]])]:
        _, key, kk, sh0, _ = lookup(octree, shatlas, pt)
        log("  %-18s %s -> SHkey %s SH0 %s" % (label, [round(v, 2) for v in pt], kk, [round(v, 5) for v in sh0]))

run("s27_sh_cpu.log", body)
