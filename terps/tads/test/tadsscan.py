#!/usr/bin/env python3
"""Scan TADS 2/3 game files (bare or inside archives) for HTML TADS usage.
Usage: tadsscan.py OUT.jsonl FILE...   (one JSON object per game file found)"""
import sys, os, re, io, json, zipfile, hashlib, gzip, subprocess, struct

T2SIG = b"TADS2 bin\n\r\x1a"; T2RSC = b"TADS2 rsc\n\r\x1a"; T3SIG = b"T3-image\r\n\x1a"
TAGS = ("A ABOUTBOX ADDRESS AREA B BANNER BASEFONT BIG BLOCKQUOTE BODY BQ BR CAPTION CENTER "
        "CITE CODE CREDIT DD DFN DIR DIV DL DT EM FONT H1 H2 H3 H4 H5 H6 HILITEOFF HILITEON HR "
        "I IMG KBD LH LI LISTING LOG MAP MENU NOBR NOLOG OL P PRE Q S SAMP SMALL SOUND STRIKE "
        "STRONG SUB SUP TAB TABLE TD TH TITLE TR TT U UL VAR WRAP XMP").split()
TAGRE = re.compile(rb"<\s*/?\s*(" + b"|".join(t.encode() for t in sorted(TAGS, key=len, reverse=True)) + rb")(?=[\s>/])", re.I)
MEDIA = {'.jpg':'jpeg','.jpeg':'jpeg','.png':'png','.mng':'mng','.ogg':'ogg','.wav':'wav',
         '.mid':'midi','.midi':'midi','.mp3':'mp3','.gif':'gif','.bmp':'bmp'}

def u2(b,o): return struct.unpack_from('<H',b,o)[0]
def u4(b,o): return struct.unpack_from('<I',b,o)[0]

def xor(data, seed, inc):
    ks = bytes(((seed + inc*i) & 0xff) for i in range(256))
    # keystream has period 256 when inc odd; build generically
    out = bytearray(data)
    s = seed
    for i in range(len(out)):
        out[i] ^= s & 0xff; s += inc
    return bytes(out)

def mtype(name):
    return MEDIA.get(os.path.splitext(name.lower())[1], 'other')

def t2_scan(b):
    """Return (text blobs, resources list, info)"""
    flags = u2(b, 20); crypt = flags & 0x08
    seed, inc = 17, 29
    pos = 48; texts = []; res = []; secs = []
    while pos < len(b):
        n = b[pos]; name = b[pos+1:pos+1+n].decode('latin1'); end = u4(b, pos+1+n)
        body = pos + 1 + n + 4; secs.append(name)
        if name == 'XSI': seed, inc = b[body], b[body+1]
        elif name == 'OBJ':
            p = body
            while p < end:
                t = b[p]
                if t in (1, 2):
                    dsz = u2(b, p+5); d = b[p+7:p+7+dsz]
                    texts.append(xor(d, seed, inc) if crypt else d); p += 7 + dsz
                elif t in (7, 8): p += 5 + u2(b, p+3)
                elif t == 10: p += 4 + b[p+3]
                else: break
        elif name == 'FMTSTR':
            l = u2(b, body); d = b[body+2:body+2+l]; texts.append(xor(d, seed, inc) if crypt else d)
        elif name == 'HTMLRES':
            cnt = u4(b, body); p = body + 8; ents = []
            for _ in range(cnt):
                ro, rs, nl = u4(b,p), u4(b,p+4), u2(b,p+8)
                ents.append((b[p+10:p+10+nl].decode('latin1'), rs, ro)); p += 10 + nl
            res += [(n, rs, p + ro) for n, rs, ro in ents]
        elif name == '$EOF': break
        if end <= pos: break
        pos = end
    return texts, res, {'sections': secs, 'crypt': bool(crypt)}

def t3_scan(b):
    pos = 69; texts = []; res = []; blocks = []
    while pos + 10 <= len(b):
        typ = b[pos:pos+4].decode('latin1'); sz = u4(b, pos+4); body = pos + 10
        blocks.append(typ)
        if typ == 'CPPG':
            mask = b[body+6]; d = b[body+7:body+sz]
            texts.append(bytes(c ^ mask for c in d) if mask else d)
        elif typ == 'CPDF': pass
        elif typ in ('MRES',):
            cnt = u2(b, body); p = body + 2
            for _ in range(cnt):
                eo, es, nl = u4(b,p), u4(b,p+4), b[p+8]
                res.append((bytes(c ^ 0xff for c in b[p+9:p+9+nl]).decode('latin1'), es, body + eo)); p += 9 + nl
        elif typ == 'EOF ': break
        pos = body + sz
    return texts, res, {'blocks': sorted(set(blocks))}

def scan_game(b):
    if b.startswith(T2SIG) or b.startswith(T2RSC):
        kind = 'tads2' if b.startswith(T2SIG) else 'tads2-rsc'
        texts, res, info = t2_scan(b)
        vsn = b[13:19].decode('latin1')
    else:
        kind = 't3' ; texts, res, info = t3_scan(b); vsn = str(u2(b, 11))
    blob = b"\n".join(texts)
    hist = {}
    for m in TAGRE.finditer(blob):
        t = m.group(1).decode().upper(); hist[t] = hist.get(t, 0) + 1
    info.update(kind=kind, vsn=vsn, tags=hist,
                html_mode_on=blob.count(b"\\H+"),
                res_count=len(res), res_bytes=sum(r[1] for r in res),
                res_types=sorted({mtype(r[0]) for r in res}),
                media_res=sum(1 for r in res if mtype(r[0]) != 'other' and not r[0].lower().startswith(('webuires/', '.system/'))),
                webui=any(r[0].startswith('webuires/') for r in res),
                res_names=[r[0] for r in res][:400])
    for n, sz, off in res:
        if n.lower() == 'gameinfo.txt':
            gi = {}
            for line in b[off:off+sz].decode('utf-8', 'replace').splitlines():
                if ':' in line:
                    k, v = line.split(':', 1); gi[k.strip()] = v.strip()
            info['gameinfo'] = {k: gi[k] for k in ('Name', 'Byline', 'IFID', 'Version', 'ReleaseDate', 'FirstPublished') if k in gi}
    # image/sound file names referenced in strings (src="...")
    srcs = set(m.group(1).decode('latin1') for m in re.finditer(rb'src\s*=\s*["\']?([^"\'\s>]+)', blob, re.I))
    info['src_refs'] = sorted(srcs)[:400]
    return info

def is_game(b):
    return b.startswith(T2SIG) or b.startswith(T2RSC) or b.startswith(T3SIG)

def walk(data, label, out, depth=0):
    """Yield (member path, bytes) for every game file in data; record media siblings."""
    if is_game(data):
        out.append((label, data)); return []
    media = []
    if data[:2] == b'PK':
        try: z = zipfile.ZipFile(io.BytesIO(data))
        except Exception: return media
        for zi in z.infolist():
            if zi.is_dir(): continue
            try: d = z.read(zi)
            except Exception:
                import tempfile
                with tempfile.NamedTemporaryFile(suffix='.zip') as tf:
                    tf.write(data); tf.flush()
                    d = subprocess.run(['unzip', '-p', tf.name, zi.filename.replace('[', '[[]')], capture_output=True).stdout
                if not d: continue
            nm = zi.filename
            if mtype(nm) != 'other': media.append((label + '|' + nm, len(d))); continue
            media += walk(d, label + '|' + nm, out, depth + 1) or []
    elif data[:2] in (b'\x1f\x8b', b'\x1f\x9d'):
        try: d = subprocess.run(['gzip', '-dc'], input=data, capture_output=True).stdout
        except Exception: d = b''
        if d: media += walk(d, label + '|gunzip', out, depth + 1) or []
    elif len(data) > 262 and data[257:262] == b'ustar':
        import tarfile
        t = tarfile.open(fileobj=io.BytesIO(data))
        for ti in t.getmembers():
            if not ti.isfile(): continue
            d = t.extractfile(ti).read(); nm = ti.name
            if mtype(nm) != 'other': media.append((label + '|' + nm, len(d))); continue
            media += walk(d, label + '|' + nm, out, depth + 1) or []
    else:
        # bundled executables (maketrx / t3make -exe): game image embedded mid-file
        for sig in (T2SIG, T3SIG):
            i = data.find(sig, 1)
            if i > 0: out.append((label + '@%d' % i, data[i:])); break
    return media

def main():
    outp = open(sys.argv[1], 'w')
    for f in sys.argv[2:]:
        data = open(f, 'rb').read()
        games = []; media = walk(data, os.path.basename(f), games)
        for lab, b in games:
            rec = {'path': lab, 'size': len(b), 'sha256': hashlib.sha256(b).hexdigest(),
                   'container': os.path.basename(f),
                   'ext_media': [m for m, _ in media][:400],
                   'ext_media_types': sorted({mtype(m) for m, _ in media})}
            try: rec.update(scan_game(b))
            except Exception as e: rec['error'] = repr(e)
            outp.write(json.dumps(rec) + "\n")
        if not games:
            outp.write(json.dumps({'path': os.path.basename(f), 'nogame': True,
                                   'ext_media': [m for m, _ in media][:50]}) + "\n")
main()
