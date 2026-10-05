#!/usr/bin/env python3
"""Module dependency table from #include directives (PLAN_cglib_refactoring Phase 0).

Usage: python tools/module_deps.py [--external]
Prints, per top-level CGLib module, which other CGLib modules its non-test sources include
(header-level vs. source-only), and with --external which third-party headers it includes.
Static text analysis only; ThirdParty/ and build/ trees are skipped, as are test directories.
"""
import collections
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP = {'build', 'ThirdParty', 'docs', 'cmake', 'examples', 'tools', 'tests'}
MODULES = sorted(d for d in os.listdir(ROOT)
                 if os.path.isdir(os.path.join(ROOT, d)) and d not in SKIP and not d.startswith('.'))
INC = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
EXT = ('vulkan', 'GLFW', 'imgui', 'glm', 'Eigen', 'nlohmann', 'stb', 'cgltf', 'pugixml')


def module_of(path):
    return os.path.relpath(path, ROOT).replace(os.sep, '/').split('/')[0]


def resolve(src_file, kind, target):
    """CGLib module an include points to, or None."""
    t = target.replace('\\', '/')
    if t.startswith('CGLib/'):
        top = t.split('/')[1]
        return top if top in MODULES else None
    if kind == '"':
        p = os.path.normpath(os.path.join(os.path.dirname(src_file), t))
        if p.startswith(ROOT) and os.path.exists(p):
            return module_of(p)
    return None


def is_test(path):
    rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
    return any(part.endswith('Test') for part in rel.split('/')[:-1])


hdr = collections.defaultdict(collections.Counter)
src = collections.defaultdict(collections.Counter)
ext = collections.defaultdict(collections.Counter)
for m in MODULES:
    for dp, dn, fn in os.walk(os.path.join(ROOT, m)):
        dn[:] = [d for d in dn if d not in ('build', 'ThirdParty') and not d.startswith('.')]
        for f in fn:
            ex = os.path.splitext(f)[1]
            path = os.path.join(dp, f)
            if ex not in ('.h', '.hpp', '.inl', '.cpp') or is_test(path):
                continue
            kindname = 'source' if ex == '.cpp' else 'header'
            with open(path, encoding='utf-8', errors='replace') as fh:
                for ln in fh:
                    mm = INC.match(ln)
                    if not mm:
                        continue
                    kind, target = mm.groups()
                    dep = resolve(path, kind, target)
                    if dep and dep != m:
                        (src if ex == '.cpp' else hdr)[m][dep] += 1
                    for e in EXT:
                        if e.lower() in target.lower():
                            ext[m][(e, kindname)] += 1

print('Module | header deps (include count) | source-only deps')
for m in MODULES:
    h = dict(hdr[m])
    s = {k: v for k, v in src[m].items() if k not in h}
    hs = ', '.join(f'{k}({v})' for k, v in sorted(h.items())) or '-'
    ss = ', '.join(f'{k}({v})' for k, v in sorted(s.items())) or '-'
    print(f'{m} | {hs} | {ss}')
if '--external' in sys.argv:
    print('\nThird-party includes by module')
    for m in MODULES:
        if ext[m]:
            print(f'{m}: ' + ', '.join(f'{k[0]}[{k[1]}]x{v}' for k, v in sorted(ext[m].items())))
