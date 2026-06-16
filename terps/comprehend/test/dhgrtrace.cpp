/* dhgrtrace.cpp -- offline harness to render Coveted Mirror pictures through the
 * Apple II double-hi-res ("<D>") renderer AND the standard hi-res renderer, and
 * to disassemble a picture's opcode stream (tracking pen colour) so a primitive
 * that the two renderers draw differently can be localised.
 *
 * Usage:
 *   dhgrtrace <sideA.woz> <bootB.woz> render <FILE> <idx> <out_prefix>
 *        -> writes <out_prefix>_dhgr.ppm (560x192) and <out_prefix>_std.ppm (280x192)
 *   dhgrtrace <sideA.woz> <bootB.woz> overlay <ROOM> <ridx> <ITEM> <iidx> <pfx>
 *        -> room then item overlay composited, both renderers
 *   dhgrtrace <sideA.woz> <bootB.woz> disasm <FILE> <idx>
 *        -> print the opcode stream with running pen colour
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

extern "C" {
#include "../../readwoz/ciderpress.h"
#include "../../readwoz/woz2nib.h"
}
extern "C" const uint32_t gm_apple2_palette[16];
extern "C" void gm_compute_dhgr_row_words(const uint8_t *aux_row, const uint8_t *main_row, uint16_t *words);
extern "C" void gm_render_dhgr_line_colors(const uint16_t *words, uint8_t *colors560);
static unsigned a2calc(int row){ return (row&7)*0x400 + ((row>>3)&7)*0x80 + (row>>6)*0x28; }

namespace Glk { namespace Comprehend {
// std hi-res
void gmResetScreen(bool white);
void gmDrawImage(const uint8_t *data, size_t size);
void gmBlitToSurface(uint32_t *out, int w, int h);
bool gmInstallDrawingTables(const uint8_t *t2, size_t size);
// double hi-res
bool gmDhgrInstallDrawingTables(const uint8_t *t5, size_t size);
const uint8_t *gmDhgrMainPtr();
const uint8_t *gmDhgrAuxPtr();
void gmDhgrResetScreen(bool white);
void gmDhgrDrawImage(const uint8_t *data, size_t size);
void gmDhgrBlitToSurface(uint32_t *out, int w, int h);
#ifdef DHGR_WATCH
void gmDhgrSetWatch(int off, int page);
#endif
}}
using namespace Glk::Comprehend;

#define DSK_IMAGE_SIZE (35 * 16 * 256)
#define NIB_IMAGE_SIZE (35 * 6656)

static uint8_t *readFile(const char *path, size_t *sz) {
	FILE *f = fopen(path, "rb");
	if (!f) return nullptr;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	uint8_t *d = (uint8_t *)malloc(n);
	if (fread(d, 1, n, f) != (size_t)n) { free(d); fclose(f); return nullptr; }
	fclose(f); *sz = n; return d;
}
static uint8_t *toDsk(uint8_t *data, size_t size, size_t *outSize) {
	if (size >= 4 && data[0]=='W' && data[1]=='O' && data[2]=='Z') {
		size_t nibSize = size; uint8_t *nib = woz2nib(data, &nibSize);
		if (!nib) return nullptr;
		uint8_t *dsk = ReadFullImageFromNib(nib, nibSize); free(nib);
		*outSize = DSK_IMAGE_SIZE; return dsk;
	}
	if (size == NIB_IMAGE_SIZE) { *outSize = DSK_IMAGE_SIZE; return ReadFullImageFromNib(data, size); }
	if (size >= DSK_IMAGE_SIZE) { uint8_t *d=(uint8_t*)malloc(DSK_IMAGE_SIZE); memcpy(d,data,DSK_IMAGE_SIZE); *outSize=DSK_IMAGE_SIZE; return d; }
	return nullptr;
}
struct Disk { uint8_t *dsk; size_t size; A2FileRec *files; size_t n; };
static bool loadDisk(const char *path, Disk *d) {
	size_t raw=0; uint8_t *r = readFile(path, &raw); if (!r) return false;
	size_t ds=0; d->dsk = toDsk(r, raw, &ds); free(r); if (!d->dsk) return false;
	d->size = ds; d->files = GetAllProDOSFiles(d->dsk, ds, &d->n);
	return d->files && d->n;
}
static A2FileRec *find(Disk *d, const char *nm) {
	for (size_t i=0;i<d->n;i++) if (strcasecmp(d->files[i].filename, nm)==0) return &d->files[i];
	return nullptr;
}
// image idx -> stream start within a picture file (same as pics.cpp)
static const uint8_t *imgStream(A2FileRec *f, int idx, size_t *len) {
	const uint8_t *b = f->data;
	uint16_t ver = b[0] | (b[1] << 8);
	size_t bs = (ver == 0x1000) ? 4 : 0;
	size_t p = bs + idx*2;
	uint16_t st = b[p] | (b[p+1] << 8);
	if (ver == 0x1000) st += 4;
	*len = f->datasize - st;
	return b + st;
}
static void writePPM(const char *path, uint32_t *rgba, int w, int h) {
	FILE *o = fopen(path, "wb"); if (!o) return;
	fprintf(o, "P6\n%d %d\n255\n", w, h);
	for (int i=0;i<w*h;i++){ uint32_t p=rgba[i]; uint8_t c[3]={(uint8_t)(p>>24),(uint8_t)(p>>16),(uint8_t)(p>>8)}; fwrite(c,1,3,o); }
	fclose(o);
	fprintf(stderr, "wrote %s (%dx%d)\n", path, w, h);
}

// ---- opcode disassembler (tracks pen) ----------------------------------------
static const char *opName[16] = {"END","TEXTPOS","PEN","CHAR","SHAPE","OUTLINE","FILLCOL","END2",
	"MOVE","BOX","LINE","CIRCLE","DRAWSHP","DELAY","PAINT","RESET"};
static void disasm(const uint8_t *p, size_t len) {
	const uint8_t *e = p + len;
	int pen = 4, penx = 140, peny = 96, fill = 0, brush = 5, n = 0;
	while (p < e && n++ < 100000) {
		const uint8_t *op0 = p;
		uint8_t b = *p++; uint8_t par = b & 0xf, op = b >> 4;
		int a = 0, c = 0;
		auto pos = (int)(op0 - (e - len));
		switch (op) {
		case 0: printf("  [%4d] END\n", pos); return;
		case 1: a=*p++; c=*p++; printf("  [%4d] TEXTPOS %d,%d\n",pos,a+(par&1?256:0),c); break;
		case 2: pen=par; printf("  [%4d] PEN=%d\n",pos,pen); break;
		case 3: a=*p++; printf("  [%4d] CHAR %d\n",pos,a); break;
		case 4: brush=par; printf("  [%4d] SHAPE brush=%d\n",pos,brush); break;
		case 5: a=*p++; printf("  [%4d] OUTLINE %d\n",pos,a); break;
		case 6: fill=*p++; printf("  [%4d] FILLCOL=%d\n",pos,fill); break;
		case 7: a=*p++; c=*p++; printf("  [%4d] PALSUB[%d]=%d\n",pos,a,c); break;
		case 8: a=*p++ +(par&1?256:0); c=*p++; penx=a; peny=c; printf("  [%4d] MOVE %d,%d\n",pos,a,c); break;
		case 9: a=*p++ +(par&1?256:0); c=*p++;
			printf("  [%4d] BOX  (%d,%d)->(%d,%d)  pen=%d %s\n",pos,penx,peny,a,c,pen,
				(pen!=4&&pen!=7)?"<<COLOR(4px cell)":"mono");
			penx=a; peny=c; break;
		case 10: a=*p++ +(par&1?256:0); c=*p++;
			printf("  [%4d] LINE (%d,%d)->(%d,%d)  pen=%d %s%s\n",pos,penx,peny,a,c,pen,
				(pen!=4&&pen!=7)?"<<COLOR(4px cell)":"mono",
				(a==penx)?"  VERTICAL":"");
			penx=a; peny=c; break;
		case 11: a=*p++; printf("  [%4d] CIRCLE r=%d pen=%d\n",pos,a,pen); break;
		case 12: a=*p++ +(par&1?256:0); c=*p++; printf("  [%4d] DRAWSHP %d,%d brush=%d\n",pos,a,c,brush); break;
		case 13: a=*p++; printf("  [%4d] DELAY %d\n",pos,a); break;
		case 14: a=*p++ +(par&1?256:0); c=*p++; printf("  [%4d] PAINT %d,%d fill=%d\n",pos,a,c,fill); break;
		case 15:
			if (par==0) { // skip the RLE bitmap (col-major, 0x80 = run escape) and continue
				uint8_t right=*p++, left=*p++, bottom=*p++, top=*p++;
				int rc=left, rr=top; bool rd=false;
				while(!rd && p<e){ uint8_t bb=*p++; int reps=1; if(bb==0x80){ if(p+2>e)break; reps=*p++ +1; p++; }
					while(reps--){ if(rr!=bottom) rr++; else if(rc==right){rd=true;break;} else {rc++;rr=top;} } }
				printf("  [%4d] RESET/0 RLE-bitmap cols %d-%d rows %d-%d (skipped)\n",pos,left,right,top,bottom);
			}
			else if (par==2) { int rr=*p++,ll=*p++,bb=*p++,tt=*p++; printf("  [%4d] RESET/2 bounds l=%d r=%d t=%d b=%d\n",pos,ll,rr,tt,bb); }
			else printf("  [%4d] RESET/%d\n",pos,par);
			break;
		}
	}
}

static A2FileRec *g_T2, *g_T5;
static void renderStd(A2FileRec *f, int idx, bool reset, bool white) {
	gmInstallDrawingTables(g_T2->data, g_T2->datasize);
	if (reset) gmResetScreen(white);
	size_t len; const uint8_t *s = imgStream(f, idx, &len);
	gmDrawImage(s, len);
}
static void renderDhgr(A2FileRec *f, int idx, bool reset, bool white) {
	gmDhgrInstallDrawingTables(g_T5->data, g_T5->datasize);
	if (reset) gmDhgrResetScreen(white);
	size_t len; const uint8_t *s = imgStream(f, idx, &len);
	gmDhgrDrawImage(s, len);
}

int main(int argc, char **argv) {
	if (argc < 4) { fprintf(stderr,"usage: %s <sideA.woz> <bootB.woz> <cmd> ...\n",argv[0]); return 2; }
	Disk A, B;
	if (!loadDisk(argv[1], &A)) { fprintf(stderr,"cannot load %s\n",argv[1]); return 1; }
	if (!loadDisk(argv[2], &B)) { fprintf(stderr,"cannot load %s\n",argv[2]); return 1; }
	g_T2 = find(&B, "T2"); g_T5 = find(&B, "T5");
	if (!g_T2 || !g_T5) { fprintf(stderr,"T2/T5 not on boot disk\n"); return 1; }
	const char *cmd = argv[3];

	std::vector<uint32_t> dh(560*192), st(280*192);

	if (!strcmp(cmd,"render") && argc>=7) {
		A2FileRec *f = find(&A, argv[4]); int idx = atoi(argv[5]);
		if (!f) { fprintf(stderr,"no file %s\n",argv[4]); return 1; }
		renderDhgr(f, idx, true, true); gmDhgrBlitToSurface(dh.data(),560,192);
		renderStd(f, idx, true, true);  gmBlitToSurface(st.data(),280,192);
		char p[512];
		snprintf(p,sizeof p,"%s_dhgr.ppm",argv[6]); writePPM(p,dh.data(),560,192);
		snprintf(p,sizeof p,"%s_std.ppm",argv[6]);  writePPM(p,st.data(),280,192);
		return 0;
	}
#ifdef DHGR_WATCH
	if (getenv("WATCHOFF"))
		gmDhgrSetWatch((int)strtol(getenv("WATCHOFF"),0,16),
		               getenv("WATCHPAGE")?atoi(getenv("WATCHPAGE")):1);
#endif
	if (!strcmp(cmd,"overlay") && argc>=9) {
		A2FileRec *room = find(&A, argv[4]); int ridx = atoi(argv[5]);
		A2FileRec *item = find(&A, argv[6]); int iidx = atoi(argv[7]);
		if (!room||!item){ fprintf(stderr,"bad room/item\n"); return 1; }
		// DHGR: room (reset white) then item overlay (no reset). Overlays get the
		// 0x00-transparent op15/0 mode (matches hardware single-page write).
		renderDhgr(room, ridx, true, true);
		renderDhgr(item, iidx, false, false);
		gmDhgrBlitToSurface(dh.data(),560,192);
		renderStd(room, ridx, true, true);  renderStd(item, iidx, false, false);
		gmBlitToSurface(st.data(),280,192);
		char p[512];
		snprintf(p,sizeof p,"%s_dhgr.ppm",argv[8]); writePPM(p,dh.data(),560,192);
		snprintf(p,sizeof p,"%s_std.ppm",argv[8]);  writePPM(p,st.data(),280,192);
		// also export the raw DHGR main/aux pages for byte comparison vs MAME
		snprintf(p,sizeof p,"%s_main.page",argv[8]);
		{ FILE*o=fopen(p,"wb"); if(o){fwrite(gmDhgrMainPtr(),1,0x2000,o);fclose(o);fprintf(stderr,"wrote %s\n",p);} }
		snprintf(p,sizeof p,"%s_aux.page",argv[8]);
		{ FILE*o=fopen(p,"wb"); if(o){fwrite(gmDhgrAuxPtr(),1,0x2000,o);fclose(o);fprintf(stderr,"wrote %s\n",p);} }
		return 0;
	}
	if (!strcmp(cmd,"matchoverlay") && argc>=8) {
		// matchoverlay <ROOM> <ridx> <mame_main> <mame_aux> : render ROOM then each
		// item overlay (overlay-0x00-transparent ON) and report match vs MAME.
		A2FileRec *room=find(&A,argv[4]); int ridx=atoi(argv[5]);
		size_t ms,as; uint8_t *mm=readFile(argv[6],&ms), *ma=readFile(argv[7],&as);
		if(!room||!mm||!ma){fprintf(stderr,"bad args\n");return 1;}
		const char *items[]={"OA","OB","OC","OD","OE","OF","OG"};
		printf("overlay : match%% on %s idx %d\n",argv[4],ridx);
		for(auto nm: items){ A2FileRec *f=find(&A,nm); if(!f)continue;
			for(int idx=0; idx<12; idx++){
				size_t len; const uint8_t *s=imgStream(f,idx,&len);
				if(len<2||len>20000) continue;
				double pcts[2];
				for(int mode=0; mode<2; mode++){
					renderDhgr(room,ridx,true,true);
					renderDhgr(f,idx,false,false);
					const uint8_t *om=gmDhgrMainPtr(), *oa=gmDhgrAuxPtr();
					int diff=0,tot=0;
					for(int row=0;row<160;row++) for(int col=0;col<=20;col++){
						unsigned o=a2calc(row)+col; tot+=2;
						if((om[o]&0x7f)!=(mm[o]&0x7f))diff++;
						if((oa[o]&0x7f)!=(ma[o]&0x7f))diff++; }
					pcts[mode]=100.0*(tot-diff)/tot;
				}
				if(pcts[0]>80||pcts[1]>80) printf("  %s %2d : no-fix %.2f%%  with-fix %.2f%%\n",nm,idx,pcts[0],pcts[1]);
			}
		}
		return 0;
	}
	if (!strcmp(cmd,"matchroom") && argc>=6) {
		// matchroom <mame_main.page> <mame_aux.page> : render every RA-RG room idx
		// (reset white, room only) and report byte-match vs the MAME pages over the
		// picture columns 0..20, to identify which room the captured scene is.
		size_t ms,as; uint8_t *mm=readFile(argv[4],&ms), *ma=readFile(argv[5],&as);
		if(!mm||!ma){fprintf(stderr,"cannot read mame pages\n");return 1;}
		const char *files[]={"RA","RB","RC","RD","RE","RF","RG"};
		printf("room idx : match%% (cols0-20 rows0-159)\n");
		for(auto nm: files){ A2FileRec *f=find(&A,nm); if(!f)continue;
			uint16_t ver=f->data[0]|(f->data[1]<<8); int hdr=(ver==0x1000)?4:0;
			int nimg=(f->data[hdr]|(f->data[hdr+1]<<8)) ; // first offset / 2 approx
			nimg = nimg/2; if(nimg<1||nimg>32) nimg=16;
			for(int idx=0; idx<nimg; idx++){
				size_t len; const uint8_t *s=imgStream(f,idx,&len);
				if(len<2||len>20000) continue;
				renderDhgr(f,idx,true,true);
				const uint8_t *om=gmDhgrMainPtr(), *oa=gmDhgrAuxPtr();
				int diff=0,tot=0;
				for(int row=0;row<160;row++) for(int col=0;col<=20;col++){
					unsigned o=a2calc(row)+col; tot+=2;
					if((om[o]&0x7f)!=(mm[o]&0x7f))diff++;
					if((oa[o]&0x7f)!=(ma[o]&0x7f))diff++; }
				double pct=100.0*(tot-diff)/tot;
				if(pct>40) printf("  %s %2d : %.1f%%\n",nm,idx,pct);
			}
		}
		return 0;
	}
	if (!strcmp(cmd,"blitpages") && argc>=7) {
		// blitpages <main.page> <aux.page> <out.ppm> : NTSC-convert a raw DHGR
		// main+aux page pair (8KB each) to a 560x192 PPM.
		size_t ms,as; uint8_t *mp=readFile(argv[4],&ms), *ap=readFile(argv[5],&as);
		if(!mp||!ap){fprintf(stderr,"cannot read page files\n");return 1;}
		std::vector<uint32_t> img(560*192);
		for(int row=0;row<192;row++){
			unsigned base=a2calc(row); uint16_t words[40]; uint8_t col560[560];
			gm_compute_dhgr_row_words(ap+base, mp+base, words);
			gm_render_dhgr_line_colors(words, col560);
			for(int x=0;x<560;x++){ uint32_t rgb=gm_apple2_palette[col560[x]&0xf]; img[row*560+x]=(rgb<<8)|0xff; }
		}
		writePPM(argv[6], img.data(), 560, 192);
		return 0;
	}
	if (!strcmp(cmd,"extract") && argc>=6) {
		// extract a named file (from either disk) to a raw binary
		A2FileRec *f = find(&A, argv[4]); if(!f) f = find(&B, argv[4]);
		if (!f){ fprintf(stderr,"no file %s on either disk\n",argv[4]); return 1; }
		FILE *o=fopen(argv[5],"wb"); if(!o){fprintf(stderr,"cannot write %s\n",argv[5]);return 1;}
		fwrite(f->data,1,f->datasize,o); fclose(o);
		fprintf(stderr,"extracted %s (%zu bytes) -> %s\n",argv[4],f->datasize,argv[5]);
		return 0;
	}
	if (!strcmp(cmd,"dumprle") && argc>=6) {
		A2FileRec *f = find(&A, argv[4]); int idx = atoi(argv[5]);
		if (!f){ fprintf(stderr,"no file %s\n",argv[4]); return 1; }
		size_t len; const uint8_t *s = imgStream(f, idx, &len);
		const uint8_t *p = s, *e = s + len;
		if (*p>>4 != 15 || (*p&0xf)!=0) { printf("not op15/0 (first op=%02x)\n",*p); return 1; }
		p++;
		uint8_t right=*p++, left=*p++, bottom=*p++, top=*p++;
		printf("RLE bounds: left=%d right=%d top=%d bottom=%d  (cols %d..%d, rows %d..%d)\n",
			left,right,top,bottom,left,right,top,bottom);
		// replicate the column-major decode; tally, per source column, total cells
		// and how many are 0x00 (would render black) and how many are 0xff (white).
		int col=left,row=top; bool done=false; long cells=0;
		static int blackPerCol[64]={0}, totPerCol[64]={0}, whitePerCol[64]={0};
		while(!done && p<e){
			uint8_t b=*p++; int reps=1;
			if(b==0x80){ if(p+2>e)break; reps=*p++ +1; b=*p++; }
			while(reps--){
				if(col<64){ totPerCol[col]++; if(b==0x00)blackPerCol[col]++; if(b==0xff)whitePerCol[col]++; }
				cells++;
				if(row!=bottom) row++;
				else if(col==right){ done=true; break; }
				else { col++; row=top; }
			}
		}
		printf("decoded %ld cells; per-column (col: total black white):\n",cells);
		for(int c=left;c<=right&&c<64;c++)
			printf("  col %2d (x~%3d): tot=%3d black=%3d white=%3d %s\n",
				c, c*7, totPerCol[c], blackPerCol[c], whitePerCol[c],
				(blackPerCol[c]>20)?"  <<< tall black run":"");
		return 0;
	}
	if (!strcmp(cmd,"disasm") && argc>=6) {
		A2FileRec *f = find(&A, argv[4]); int idx = atoi(argv[5]);
		if (!f){ fprintf(stderr,"no file %s\n",argv[4]); return 1; }
		size_t len; const uint8_t *s = imgStream(f, idx, &len);
		printf("== %s idx %d (stream %zu bytes) ==\n",argv[4],idx,len);
		disasm(s, len);
		return 0;
	}
	fprintf(stderr,"bad command\n"); return 2;
}
