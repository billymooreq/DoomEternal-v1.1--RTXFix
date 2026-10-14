# Summarise a Debug Pixel trace: value changes of selected variables / struct members, with step numbers.
import re, sys
path = sys.argv[1]
keys = sys.argv[2].split(",") if len(sys.argv) > 2 else ["output_lighting", "ambientSH0", "ambientSH1", "ambientSH2", "ambientOcclusion", "envBRDF", "envprobes_lighting", "albedo", "normal", "emissive"]
scal = ["indirectDiffuse", "ambient", "numLights", "numClusterLights", "numProbes", "numClusterProbes", "clusterOffset_1", "clusterLightHash", "clusterProbeHash", "shadow", "light_color_1", "light_color_final_1", "NdotL_2", "lightAttenuation_1", "spec", "specEnvProbe", "param_18", "lightId", "lightIdx", "modelLightingFlags", "linearDepth", "slice", "clusterZ", "out_FragColor0", "shc0", "shc1", "shc2", "shc3"]
last = {}
step = 0
for line in open(path, encoding="utf-8"):
    m = re.match(r"\[\s*(\d+)\]", line)
    if m:
        step = int(m.group(1)); continue
    if not line.startswith("      "): continue
    for k in keys:
        for mm in re.finditer(r"(?:^|;)\s+%s \[[^\]]*\] = ([^;]*)" % re.escape(k), line):
            v = mm.group(1).strip()
            if last.get(k) != v:
                print("step %5d  inputs.%s = %s" % (step, k, v)); last[k] = v
    for k in scal:
        mm = re.match(r"\s+%s \[[^\]]*\] = ([^;]*)$" % re.escape(k), line.rstrip("\n"))
        if mm:
            v = mm.group(1).strip()
            print("step %5d  %s = %s" % (step, k, v))
