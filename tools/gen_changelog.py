#!/usr/bin/env python3
"""Generates CHANGELOG.en.md and the JS array in src/web_ui.h from changelog.json.
Usage: python3 tools/gen_changelog.py  (in the project folder)"""
import json, re
c = json.load(open('changelog.json', encoding='utf-8'))
out = ['# Changelog\n']
for e in c:
    out.append(f"## {e['v']}")
    out += [f'- {x}' for x in e['en']]
    out.append('')
open('CHANGELOG.en.md', 'w', encoding='utf-8').write('\n'.join(out))
p = 'src/web_ui.h'
s = open(p, encoding='utf-8').read()
js = json.dumps(c, ensure_ascii=False)
s, n = re.subn(r'const CHANGELOG=\[.*?\];\n', lambda m: 'const CHANGELOG=' + js + ';\n', s, count=1, flags=re.S)
assert n == 1, 'CHANGELOG not found'
open(p, 'w', encoding='utf-8').write(s)
print('Changelog generated:', len(c), 'versions')
