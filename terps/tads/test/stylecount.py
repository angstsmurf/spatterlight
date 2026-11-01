"""stylecount.py OUT.json GAME... -- per game, the distinct text-style combinations
its strings print (tag stack simulated within each string; colour left out)"""
import sys, os, re, json
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tadsscan.py')).read()
exec(src[:src.index('def main')])
TOK = re.compile(rb'<\s*(/?)\s*([A-Za-z][A-Za-z0-9]*)((?:[^<>"\']|"[^"]*"|\'[^\']*\')*)>|([^<]+)')
ATTR = re.compile(rb'([A-Za-z][-A-Za-z0-9]*)(?:\s*=\s*("[^"]*"|\'[^\']*\'|[^\s>]+))?')
FIX = {'TT', 'CODE', 'KBD', 'SAMP', 'PRE', 'LISTING', 'XMP'}
def family(face):
    f = face.lower()
    if 'input' in f: return 'input'
    if any(k in f for k in ('typewriter', 'courier', 'mono', 'fixed', 'console', 'lucida console')): return 'mono'
    if 'script' in f or 'comic' in f: return 'script'
    if any(k in f for k in ('sans', 'arial', 'verdana', 'helvetica', 'tahoma', 'trebuchet', 'geneva')): return 'sans'
    return 'serif'
def size_of(v, cur):
    v = v.strip()
    try:
        return max(1, min(7, cur + int(v))) if v[:1] in '+-' else max(1, min(7, int(v)))
    except ValueError: return cur
res = {}
for f in sys.argv[2:]:
    b = open(f, 'rb').read()
    if b.startswith(T2SIG): texts = t2_scan(b)[0]; kind = 'tads2'
    elif b.startswith(T3SIG): texts = t3_scan(b)[0]; kind = 't3'
    else: continue
    full = {}; char = set()
    for s in texts:
        if b'<' not in s: continue
        st = []                          # (tag, changes dict)
        def cur():
            k = dict(b=0, i=0, u=0, s=0, fam='', size=3, va=0, h=0, align='', ind=0)
            for t, ch in st:
                for kk, vv in ch.items():
                    k[kk] = k[kk] + vv if kk == 'ind' else vv
            return k
        for m in TOK.finditer(s):
            if m.group(4) is not None:
                if not m.group(4).strip() or not st: continue
                k = cur(); kk_ = tuple(sorted(k.items())); full[kk_] = full.get(kk_, 0) + 1
                char.add(tuple(sorted((x, y) for x, y in k.items() if x not in ('align', 'ind'))))
                continue
            close, n = m.group(1), m.group(2).decode('latin1').upper()
            if close:
                for j in range(len(st) - 1, -1, -1):
                    if st[j][0] == n: del st[j:]; break
                continue
            at = {a.group(1).decode('latin1').upper(): (a.group(2) or b'').strip(b'"\'').decode('latin1')
                  for a in ATTR.finditer(m.group(3))}
            c = cur(); ch = None
            if n in ('B', 'STRONG'): ch = {'b': 1}
            elif n in ('I', 'EM', 'CITE', 'VAR', 'DFN', 'ADDRESS'): ch = {'i': 1}
            elif n == 'U': ch = {'u': 1}
            elif n in ('S', 'STRIKE', 'DEL'): ch = {'s': 1}
            elif n in FIX: ch = {'fam': 'mono'}
            elif n == 'BIG': ch = {'size': min(7, c['size'] + 1)}
            elif n == 'SMALL': ch = {'size': max(1, c['size'] - 1)}
            elif n in ('SUP', 'SUB'): ch = {'va': 1 if n == 'SUP' else -1}
            elif n == 'FONT':
                ch = {}
                if 'SIZE' in at: ch['size'] = size_of(at['SIZE'], c['size'])
                if 'FACE' in at: ch['fam'] = family(at['FACE'])
            elif re.fullmatch(r'H[1-6]', n):
                ch = {'h': int(n[1]), 'b': 1}
                if at.get('ALIGN', '').lower() in ('center', 'right'): ch['align'] = at['ALIGN'].lower()
            elif n == 'CENTER': ch = {'align': 'center'}
            elif n in ('P', 'DIV') and at.get('ALIGN', '').lower() in ('center', 'right'):
                ch = {'align': at['ALIGN'].lower()}
            elif n in ('BLOCKQUOTE', 'BQ', 'UL', 'OL', 'DL'): ch = {'ind': 1}
            if ch is not None: st.append((n, ch))
    res[f] = {'kind': kind, 'full': len(full), 'char': len(char),
              'combos': sorted(str(dict(x)) for x in full),
              'counts': {str(dict(x)): n for x, n in full.items()}}
json.dump(res, open(sys.argv[1], 'w'))
