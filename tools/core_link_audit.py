#!/usr/bin/env python3
"""Audit the CPU *Core targets' declared link graph against their real #includes.

For every CPU component it reports
  * MISSING   -- a CGLib module whose header is included (public header or source) but that is
                 not in the target's transitive PUBLIC link closure (a link/include-path leak);
  * EXTRA     -- a declared direct link whose module is never included (candidate to drop).
Public headers and sources are reported separately: a missing dependency reached only from a
.cpp should be PRIVATE, one reached from a header must be PUBLIC.

Usage: python tools/core_link_audit.py   (run from anywhere; exits 1 on any MISSING)
"""
import re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE = (ROOT / "cmake" / "PhantomCoreLibs.cmake").read_text(encoding="utf-8")

# target -> (module name seen in includes, source dir)
TARGETS = {
    "MathCore": ("Math", "Math"), "GraphicsCore": ("Graphics", "Graphics"),
    "NumericsCore": ("Numerics", "Numerics/Numerics"), "SpaceCore": ("Space", "Space/Space"),
    "SceneCore": ("Scene", "Scene/Scene"), "VolumeCore": ("Volume", "Volume/Volume"),
    "FileCore": ("File", "File/File"), "GeometryNodeCore": ("GeometryNode", "GeometryNode/GeometryNode"),
    "AssetCore": ("AssetCore", "AssetCore/AssetCore"), "SceneRuntimeCore": ("SceneRuntime", "SceneRuntime/SceneRuntime"),
    "AnimationCore": ("Animation", "Animation/Animation"),
}
MODULE_OF_TARGET = {t: m for t, (m, _) in TARGETS.items()}
# Modules that live in a Core's directory tree but are not CPU cores (never allowed).
GPU_MODULES = {"Renderer", "VkAppBase", "VulkanGraphics", "UIWidgets", "GltfRenderer", "Gizmo", "Input", "Particles", "PostProcess"}
# Util is header-only and travels with the repo include root.
HEADER_ONLY = {"Util"}

links = {}
for t in TARGETS:
    m = re.search(r"target_link_libraries\(%s\s+PUBLIC\s+([^)]*)\)" % t, CMAKE)
    links[t] = [x for x in (m.group(1).split() if m else []) if x in TARGETS]

def closure(t, seen=None):
    seen = seen if seen is not None else set()
    for d in links[t]:
        if d not in seen:
            seen.add(d); closure(d, seen)
    return seen

inc_re = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
def module_of_include(inc, own_dir):
    parts = Path(inc).parts
    if parts[0] == "CGLib" and len(parts) > 1:
        return parts[1]
    # relative include: resolve against the including file's directory
    return None

def scan(path, rel_dir):
    out = set()
    for inc in inc_re.findall(path.read_text(encoding="utf-8", errors="ignore")):
        if inc.startswith("CGLib/"):
            out.add(inc.split("/")[1])
        elif "../" in inc:
            resolved = (path.parent / inc).resolve()
            try:
                rel = resolved.relative_to(ROOT)
            except ValueError:
                continue
            out.add(rel.parts[0])
    return out

rc = 0
mod_to_target = {m: t for t, m in MODULE_OF_TARGET.items()}
for t, (mod, d) in TARGETS.items():
    src_dir = ROOT / d
    pub, priv = set(), set()
    for f in src_dir.glob("*"):
        if f.suffix in (".h", ".hpp", ".inl"):
            if f.name == "pch.h" or f.name.endswith("Presenter.h"):
                continue
            pub |= scan(f, d)
        elif f.suffix == ".cpp":
            priv |= scan(f, d)
    own = {mod, "ThirdParty"} | HEADER_ONLY
    have = {MODULE_OF_TARGET[x] for x in closure(t)}
    miss_pub = sorted(m for m in pub - own - have if m in mod_to_target or m in GPU_MODULES)
    miss_priv = sorted(m for m in priv - pub - own - have if m in mod_to_target or m in GPU_MODULES)
    direct = {MODULE_OF_TARGET[x] for x in links[t]}
    extra = sorted(direct - (pub | priv))
    print(f"{t:18} links={sorted(direct)}")
    if miss_pub: print(f"   MISSING (public header, needs PUBLIC): {miss_pub}"); rc = 1
    if miss_priv: print(f"   MISSING (source only, needs PRIVATE) : {miss_priv}"); rc = 1
    if extra: print(f"   EXTRA direct link never included     : {extra}")
sys.exit(rc)
