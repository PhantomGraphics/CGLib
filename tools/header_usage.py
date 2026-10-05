#!/usr/bin/env python3
"""Public-header classification and consumer usage table (PLAN_cglib_refactoring Phase 0).

Usage: python tools/header_usage.py [--md] [--list <Module>]

Scans every CGLib header and every #include in CGLib, Phantom (Physics, PointCloud, RayTracer,
...) and CGApp, resolving "CGLib/..." includes and relative includes to real files. Each header is
classified by who includes it:

  public     included by a consumer outside CGLib (Phantom modules or CGApp)
  cross      not used outside CGLib, but included by another CGLib module
  internal   included only inside its own module
  unused     included nowhere (may be an entry point, generated, or dead; verify before removing)
  test       lives in a *Test directory (never public)

Heuristic only: includes through other include-path forms (e.g. `#include "Foo.h"` found via a
target's include dirs) are resolved by file-name match inside the including module and then
CGLib-wide when the name is unique. Treat `unused` as a candidate list, not a verdict.
"""
import collections
import os
import re
import sys

CGLIB = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PHANTOM = os.path.dirname(CGLIB)
REPO = os.path.dirname(PHANTOM)
SKIP_DIRS = {'build', '.git', 'x64', 'bin', 'obj', 'packages', 'node_modules', '_cglib_headers', 'out'}
HDR_EXT = ('.h', '.hpp', '.inl', '.hxx')
SRC_EXT = HDR_EXT + ('.cpp', '.cc', '.cxx', '.mm', '.cs')
INC = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
CGLIB_SKIP = {'ThirdParty', 'build', 'docs', 'cmake', 'tools', 'tests', 'examples'}


def walk(root, skip_third_party=True):
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in SKIP_DIRS and not d.startswith('.')
                 and not (skip_third_party and d == 'ThirdParty')]
        for f in fn:
            yield os.path.join(dp, f)


def mod_of(path):
    rel = os.path.relpath(path, CGLIB).replace(os.sep, '/')
    return rel.split('/')[0]


def is_test_path(path):
    rel = os.path.relpath(path, CGLIB).replace(os.sep, '/')
    return any(p.endswith('Test') for p in rel.split('/')[:-1])


# ---- collect CGLib headers -------------------------------------------------
headers = {}
by_name = collections.defaultdict(list)
for m in sorted(os.listdir(CGLIB)):
    d = os.path.join(CGLIB, m)
    if not os.path.isdir(d) or m in CGLIB_SKIP or m.startswith('.'):
        continue
    for p in walk(d):
        if p.endswith(HDR_EXT):
            headers[os.path.normpath(p)] = m
            by_name[os.path.basename(p)].append(os.path.normpath(p))


def resolve(src, kind, target):
    t = target.replace('\\', '/')
    if t.startswith('Phantom/CGLib/'):
        t = t[len('Phantom/'):]
    if t.startswith('CGLib/'):
        p = os.path.normpath(os.path.join(CGLIB, t[len('CGLib/'):]))
        return p if p in headers else None
    if kind == '"':
        p = os.path.normpath(os.path.join(os.path.dirname(src), t))
        if p in headers:
            return p
    base = os.path.basename(t)
    c = by_name.get(base)
    if c:
        if len(c) == 1 and ('/' not in t or t.startswith('..')):
            return c[0]
        # prefer a same-module match
        sm = [x for x in c if src.startswith(os.path.join(CGLIB, mod_of(src)) + os.sep)
              and mod_of(x) == mod_of(src)] if src.startswith(CGLIB) else []
        if len(sm) == 1:
            return sm[0]
    return None


# consumer roots: label -> directory
consumers = {}
for name in sorted(os.listdir(PHANTOM)):
    d = os.path.join(PHANTOM, name)
    if name not in ('Physics', 'PointCloud', 'RayTracer') or not os.path.isdir(d):
        continue
    consumers['Phantom/' + name] = d
CGAPP = os.path.join(REPO, 'CGApp')
if os.path.isdir(CGAPP):
    for name in sorted(os.listdir(CGAPP)):
        d = os.path.join(CGAPP, name)
        if os.path.isdir(d) and name not in SKIP_DIRS and not name.startswith('.'):
            consumers['CGApp/' + name] = d

used_by_ext = collections.defaultdict(collections.Counter)    # header -> consumer label -> count
used_by_mod = collections.defaultdict(set)                     # header -> CGLib modules (other than own)
used_by_own = collections.defaultdict(int)                     # header -> count within own module


def scan(path, label, own_mod=None):
    try:
        with open(path, encoding='utf-8', errors='replace') as fh:
            for ln in fh:
                mm = INC.match(ln)
                if not mm:
                    continue
                h = resolve(path, *mm.groups())
                if not h:
                    continue
                if label is None:
                    if headers[h] == own_mod:
                        used_by_own[h] += 1
                    else:
                        used_by_mod[h].add(own_mod)
                else:
                    used_by_ext[h][label] += 1
    except OSError:
        pass


for p in headers:
    pass
for p in walk(CGLIB):
    if p.endswith(SRC_EXT) and mod_of(p) not in CGLIB_SKIP:
        scan(p, None, mod_of(p))
for label, d in consumers.items():
    for p in walk(d):
        if p.endswith(SRC_EXT):
            scan(p, label)

# a header included only by itself (self include) or by tests of its own module counts as internal
rows = collections.defaultdict(lambda: collections.Counter())
members = collections.defaultdict(lambda: collections.defaultdict(list))
for h, m in headers.items():
    if is_test_path(h):
        cls = 'test'
    elif used_by_ext.get(h):
        cls = 'public'
    elif used_by_mod.get(h):
        cls = 'cross'
    elif used_by_own.get(h):
        cls = 'internal'
    else:
        cls = 'unused'
    rows[m][cls] += 1
    members[m][cls].append(os.path.relpath(h, CGLIB).replace(os.sep, '/'))

order = ['public', 'cross', 'internal', 'unused', 'test']
if '--list' in sys.argv:
    mod = sys.argv[sys.argv.index('--list') + 1]
    for cls in order:
        for h in sorted(members[mod][cls]):
            who = ','.join(sorted(used_by_ext[os.path.join(CGLIB, h)])) if cls == 'public' else ''
            print(f'{cls:9} {h} {who}')
    sys.exit(0)

print('## Header classification (counts)\n')
print('| Module | public | cross | internal | unused | test | total |')
print('|---|---|---|---|---|---|---|')
tot = collections.Counter()
for m in sorted(rows):
    c = rows[m]
    tot.update(c)
    print(f'| {m} | ' + ' | '.join(str(c[k]) for k in order) + f' | {sum(c.values())} |')
print('| **total** | ' + ' | '.join(str(tot[k]) for k in order) + f' | {sum(tot.values())} |')

print('\n## Consumer usage (distinct CGLib headers included / total include directives)\n')
labels = sorted(consumers)
mods = sorted(rows)
print('| CGLib module | ' + ' | '.join(labels) + ' |')
print('|---|' + '---|' * len(labels))
for m in mods:
    cells = []
    for lb in labels:
        hs = [h for h in headers if headers[h] == m and used_by_ext.get(h) and lb in used_by_ext[h]]
        n = sum(used_by_ext[h][lb] for h in hs)
        cells.append(f'{len(hs)} / {n}' if hs else '-')
    if any(c != '-' for c in cells):
        print(f'| {m} | ' + ' | '.join(cells) + ' |')

print('\n## Most-used headers by consumers (top 25)\n')
tops = sorted(((sum(c.values()), h) for h, c in used_by_ext.items()), reverse=True)[:25]
for n, h in tops:
    print(f'- {os.path.relpath(h, CGLIB).replace(os.sep, "/")}: {n} (' +
          ', '.join(f'{k}:{v}' for k, v in sorted(used_by_ext[h].items())) + ')')
