/*----------------------------------------------------------------------*\

  dumpacd


  Dump an .ACD file in something like a symblic format.

  Handles 2.5 & 2.6 formats.

\*----------------------------------------------------------------------*/

#include "types.h"
#include "acode.h"

#include "reverse.h"

#define endOfTable(x) ((*(Aword *) x) == EOF)


/* The Amachine memory */
Aword *memory;
AcdHdr *header;

static int memTop = 0;			/* Top of load memory */
static Aword acdcrc = 0;		/* Checksum calculated at load time */

static char *acdfnm;

/* Dump flags */

static int dumpdict, dumpatrs, dumpacts, dumpobjs, dumplocs, dumpstxs,
dumpvrbs, dumpevts, dumpcnts, dumpmsgs, dumpstms;

int eot(Aword *adr)
{
  return *adr == EOF;
}



static void syserr(char str[])
{
  printf("ERROR - %s\n", str);
  exit(0);
}


/* Up to 2.6 the message table held (fpos, len) pairs pointing into the text
   file instead of addresses to statement code, and a syntax element flagged
   "multiple" with a plain Boolean. */
static int isPreV27(void)
{
  return header->vers[0] == 2 && header->vers[1] < 7;
}


static void indent(int level)
{
  int i;

  for (i = 0; i < level; i++)
    printf("    ");
}



/*----------------------------------------------------------------------

  dumpAwords()

  Dump a list of Awords

 */
static void dumpAwords(Aaddr awords)
{
  Aword *adr;

  if (awords == 0) return;

  for (adr = addrTo(awords); *(adr+1) != EOF; adr++)
    printf("%d, ", *adr);
  printf("%d\n", *adr);
}




/*----------------------------------------------------------------------

  dumpWrdClass()

  Dump the classes of a Word in the dictionary

 */
static void dumpWrdClass(Aword class)
{
  if ((class&((Aword)1L<<WRD_VRB)) != 0) printf("Verb ");
  if ((class&((Aword)1L<<WRD_CONJ)) != 0) printf("Conjunction ");
  if ((class&((Aword)1L<<WRD_BUT)) != 0) printf("But ");
  if ((class&((Aword)1L<<WRD_THEM)) != 0) printf("Them ");
  if ((class&((Aword)1L<<WRD_IT)) != 0) printf("It ");
  if ((class&((Aword)1L<<WRD_NOUN)) != 0) printf("Noun ");
  if ((class&((Aword)1L<<WRD_ADJ)) != 0) printf("Adjective ");
  if ((class&((Aword)1L<<WRD_PREP)) != 0) printf("Preposition ");
  if ((class&((Aword)1L<<WRD_ALL)) != 0) printf("All ");
  if ((class&((Aword)1L<<WRD_DIR)) != 0) printf("Direction ");
  if ((class&((Aword)1L<<WRD_NOISE)) != 0) printf("Noise ");
  if ((class&((Aword)1L<<WRD_SYN)) != 0) printf("Synonym ");
  printf("\n");
}



/*----------------------------------------------------------------------

  dumpDict()

  Dump the dictionary

 */
static void dumpDict(int level, Aword dict)
{
  WrdElem *wrd;

  if (dict == 0) return;

  for (wrd = (WrdElem *)addrTo(dict); !endOfTable(wrd); wrd++) {
    indent(level);
    printf("WRD: \n");
    indent(level+1);
    printf("WRD: %d(0x%x)", wrd->wrd, wrd->wrd);
    if (wrd->wrd != 0)
      printf(" -> \"%s\"", (char *)&memory[wrd->wrd]);
    printf("\n");
    indent(level+1);
    printf("CLASS: %d = ", wrd->class); dumpWrdClass(wrd->class);
    indent(level+1);
    printf("CODE: %d\n", wrd->code);
    indent(level+1);
    printf("ADJREFS: %d(0x%x)", wrd->adjrefs, wrd->adjrefs);
    if (wrd->adjrefs != 0) {
      printf(" -> ");
      dumpAwords(wrd->adjrefs);
    } else
      printf("\n");
    indent(level+1);
    printf("NOUNREFS: %d(0x%x)", wrd->nounrefs, wrd->nounrefs);
    if (wrd->nounrefs != 0) {
      printf(" -> ");
      dumpAwords(wrd->nounrefs);
    } else
      printf("\n");
  }
}



/*----------------------------------------------------------------------

  dumpAtrs()

  Dump a list of attributes

 */
static void dumpAtrs(int level, Aword atrs)
{
  AtrElem *atr;
  int atrno = 1;

  if (atrs == 0) return;

  for (atr = (AtrElem *)addrTo(atrs); !endOfTable(atr); atr++, atrno++) {
    indent(level);
    printf("ATR: #%d\n", atrno);
    indent(level+1);
    printf("VAL: %d\n", atr->val);
    indent(level+1);
    printf("STRADR: %d(0x%x)", atr->stradr, atr->stradr);
    if (atr->stradr != 0)
      printf(" -> \"%s\"", (char *)&memory[atr->stradr]);
    printf("\n");
  }
}



/*----------------------------------------------------------------------

  dumpChks()

  Dump a list of checks

 */
static void dumpChks(int level, Aword chks)
{
  ChkElem *chk;

  if (chks == 0) return;

  for (chk = (ChkElem *)addrTo(chks); !endOfTable(chk); chk++) {
    indent(level);
    printf("CHK:\n");
    indent(level+1);
    printf("EXP: %d(0x%x)\n", chk->exp, chk->exp);
    indent(level+1);
    printf("STMS: %d(0x%x)\n", chk->stms, chk->stms);
  }
}



/*----------------------------------------------------------------------

  dumpQual()

  Dump a Qualifier

 */
static void dumpQual(Aword qual)
{
  switch (qual) {
  case Q_DEFAULT: printf("Default"); break;
  case Q_AFTER: printf("After"); break;
  case Q_BEFORE: printf("Before"); break;
  case Q_ONLY: printf("Only"); break;
  }
}


/*----------------------------------------------------------------------

  dumpAlts()

  Dump a list of verbs

 */
static void dumpAlts(int level, Aword alts)
{
  AltElem *alt;

  if (alts == 0) return;

  for (alt = (AltElem *)addrTo(alts); !endOfTable(alt); alt++) {
    indent(level);
    printf("ALT:\n");
    indent(level+1);
    printf("PARAM: %d\n", alt->param);
    indent(level+1);
    printf("QUAL: "); dumpQual(alt->qual); printf("\n");
    indent(level+1);
    printf("CHECKS: %d(0x%x)\n", alt->checks, alt->checks);
    dumpChks(level+2, alt->checks);
    indent(level+1);
    printf("ACTION: %d(0x%x)\n", alt->action, alt->action);
  }
}



/*----------------------------------------------------------------------

  dumpVrbs()

  Dump a list of verbs

 */
static void dumpVrbs(int level, Aword vrbs)
{
  VrbElem *vrb;

  if (vrbs == 0) return;

  for (vrb = (VrbElem *)addrTo(vrbs); !endOfTable(vrb); vrb++) {
    indent(level);
    printf("VRB:\n");
    indent(level+1);
    printf("CODE: %d\n", vrb->code);
    indent(level+1);
    printf("ALT: %d(0x%x)\n", vrb->alts, vrb->alts);
    dumpAlts(level+2, vrb->alts);
  }
}



/*----------------------------------------------------------------------

  dumpCnts()

  Dump a list of verbs

 */
static void dumpCnts(int level, Aword cnts)
{
  CntElem *cnt;

  if (cnts == 0) return;

  for (cnt = (CntElem *)addrTo(cnts); !endOfTable(cnt); cnt++) {
    indent(level);
    printf("CNT:\n");
    indent(level+1);
    printf("LIMS: %d(0x%x)\n", cnt->lims, cnt->lims);
    indent(level+1);
    printf("HEADER: %d(0x%x)\n", cnt->header, cnt->header);
    indent(level+1);
    printf("EMPTY: %d(0x%x)\n", cnt->empty, cnt->empty);
    indent(level+1);
    printf("PARENT: %d\n", cnt->parent);
    indent(level+1);
    printf("NAM: %d(0x%x)\n", cnt->nam, cnt->nam);

  }
}



/*----------------------------------------------------------------------

  dumpObjs()

  Dump a list of objects

 */
static void dumpObjs(int level, Aword objs)
{
  ObjElem *obj;
  int objno = header->objmin;

  if (objs == 0) return;

  for (obj = (ObjElem *)addrTo(objs); !endOfTable(obj); obj++, objno++) {
    indent(level);
    printf("OBJ: #%d\n", objno);
    indent(level+1);
    printf("LOC: %d\n", obj->loc);
    indent(level+1);
    printf("DESCRIBE: %s\n", obj->describe?"Yes":"No");
    indent(level+1);
    printf("ATRS: %d(0x%x)\n", obj->atrs, obj->atrs);
    dumpAtrs(level+2, obj->atrs);
    indent(level+1);
    printf("CONT: %d(0x%x)\n", obj->cont, obj->cont);
    indent(level+1);
    printf("VRBS: %d(0x%x)\n", obj->vrbs, obj->vrbs);
    dumpVrbs(level+2, obj->vrbs);
    indent(level+1);
    printf("DSCR1: %d(0x%x)\n", obj->dscr1, obj->dscr1);
    indent(level+1);
    printf("ART: %s\n", obj->art?"Yes":"No");
    indent(level+1);
    printf("DSCR2: %d(0x%x)\n", obj->dscr2, obj->dscr2);
  }
}



/*----------------------------------------------------------------------

  dumpActs()

  Dump a list of actors

 */
static void dumpActs(int level, Aword acts)
{
  ActElem *act;
  int actno = header->actmin;

  if (acts == 0) return;

  for (act = (ActElem *)addrTo(acts); !endOfTable(act); act++, actno++) {
    indent(level);
    printf("ACT: #%d\n", actno);
    indent(level+1);
    printf("LOC: %d\n", act->loc);
    indent(level+1);
    printf("DESCRIBE: %s\n", act->describe?"Yes":"No");
    indent(level+1);
    printf("NAM: %d(0x%x)\n", act->nam, act->nam);
    indent(level+1);
    printf("ATRS: %d(0x%x)\n", act->atrs, act->atrs);
    dumpAtrs(level+2, act->atrs);
    indent(level+1);
    printf("CONT: %d(0x%x)\n", act->cont, act->cont);
    indent(level+1);
    printf("SCRIPT: %d(0x%x)\n", act->script, act->script);
    indent(level+1);
    printf("SCRADR: %d(0x%x)\n", act->scradr, act->scradr);
    indent(level+1);
    printf("STEP: %d\n", act->step);
    indent(level+1);
    printf("COUNT: %d\n", act->count);
    indent(level+1);
    printf("VRBS: %d(0x%x)\n", act->vrbs, act->vrbs);
    dumpVrbs(level+2, act->vrbs);
    indent(level+1);
    printf("DSCR: %d(0x%x)\n", act->dscr, act->dscr);
  }
}



/*----------------------------------------------------------------------

  dumpExts()

  Dump a list of exits

 */
static void dumpExts(int level, Aword exts)
{
  ExtElem *ext;

  if (exts == 0) return;

  for (ext = (ExtElem *)addrTo(exts); !endOfTable(ext); ext++) {
    indent(level);
    printf("EXT:\n");
    indent(level+1);
    printf("CODE: %d\n", ext->code);
    indent(level+1);
    printf("CHECKS: %d(0x%x)\n", ext->checks, ext->checks);
    dumpChks(level+2, ext->checks);
    indent(level+1);
    printf("ACTION: %d(0x%x)\n", ext->action, ext->action);
    indent(level+1);
    printf("NEXT: %d(0x%x)\n", ext->next, ext->next);
  }
}



/*----------------------------------------------------------------------

  dumpLocs()

  Dump a list of locations

 */
static void dumpLocs(int level, Aword locs)
{
  LocElem *loc;
  int locno = header->locmin;

  if (locs == 0) return;

  for (loc = (LocElem *)addrTo(locs); !endOfTable(loc); loc++, locno++) {
    indent(level);
    printf("LOC: #%d\n", locno);
    indent(level+1);
    printf("NAMS: %d(0x%x)\n", loc->nams, loc->nams);
    indent(level+1);
    printf("DSCR: %d(0x%x)\n", loc->dscr, loc->dscr);
    indent(level+1);
    printf("DOES: %d(0x%x)\n", loc->does, loc->does);
    indent(level+1);
    printf("DESCRIBE: %d\n", loc->describe);
    indent(level+1);
    printf("ATRS: %d(0x%x)\n", loc->atrs, loc->atrs);
    dumpAtrs(level+2, loc->atrs);
    indent(level+1);
    printf("EXTS: %d(0x%x)\n", loc->exts, loc->exts);
    dumpExts(level+2, loc->exts);
    indent(level+1);
    printf("VRBS: %d(0x%x)\n", loc->vrbs, loc->vrbs);
    dumpVrbs(level+2, loc->vrbs);
  }
}



/*----------------------------------------------------------------------

  dumpElms()

  Dump a list of syntax elements

 */
static void dumpElms(int level, Aword elms)
{
  ElmElem *elm;

  if (elms == 0) return;

  for (elm = (ElmElem *)addrTo(elms); !endOfTable(elm); elm++) {
    indent(level);
    printf("ELM: #%d\n", elm->code);
    indent(level+1);
    if (isPreV27())		/* Was a plain Boolean before 2.7 */
      printf("MULTIPLE: %s\n", elm->flags?"Yes":"No");
    else
      printf("FLAGS: %d(0x%x)\n", elm->flags, elm->flags);
    indent(level+1);
    printf("NEXT (%s): %d(0x%x)\n", elm->code==-2?"cla":"elm", elm->next, elm->next);
    if(elm->code != EOS) {
      dumpElms(level+2, elm->next);
    }
  }
}



/*----------------------------------------------------------------------

  dumpStxs()

  Dump a list of syntax descriptions

 */
static void dumpStxs(int level, Aword stxs)
{
  StxElem *stx;

  if (stxs == 0) return;

  for (stx = (StxElem *)addrTo(stxs); !endOfTable(stx); stx++) {
    indent(level);
    printf("STX: #%d\n", stx->code);
    indent(level+1);
    printf("ELMS: %d(0x%x)\n", stx->elms, stx->elms);
    dumpElms(level+2, stx->elms);
  }
}



/*----------------------------------------------------------------------

  dumpMsgs()

  Dump the message table

 */
static void dumpMsgs(int level, Aword msgs)
{
  int msgno;

  if (msgs == 0) return;

  if (isPreV27()) {
    MsgElem26 *msg;

    for (msg = (MsgElem26 *)addrTo(msgs), msgno = 0; !endOfTable(msg);
	 msg++, msgno++) {
      indent(level);
      printf("MSG: #%d%s\n", msgno,
	     msgno == M_ARTICLE26? " (ARTICLE)": "");
      indent(level+1);
      printf("FPOS: %d(0x%x)\n", msg->fpos, msg->fpos);
      indent(level+1);
      printf("LEN: %d\n", msg->len);
    }
    if (msgno != M_MSGMAX26)
      printf("WARNING! Expected %d messages in a %d.%d file, found %d.\n",
	     M_MSGMAX26, header->vers[0], header->vers[1], msgno);
  } else {
    MsgElem *msg;

    for (msg = (MsgElem *)addrTo(msgs), msgno = 0; !endOfTable(msg);
	 msg++, msgno++) {
      indent(level);
      printf("MSG: #%d\n", msgno);
      indent(level+1);
      printf("STMS: %d(0x%x)\n", msg->stms, msg->stms);
    }
    if (msgno != MSGMAX)
      printf("WARNING! Expected %d messages in a %d.%d file, found %d.\n",
	     MSGMAX, header->vers[0], header->vers[1], msgno);
  }
}



/*----------------------------------------------------------------------

  dumpStms()

  Dump a list of statements

 */
static void dumpStms(Aword pc)
{
  Aword i;

  while(TRUE) {
    printf("\n%4x: ", pc);
    if (pc > memTop)
      syserr("Dumping outside program memory.");

    i = memory[pc++];
    
    switch (I_CLASS(i)) {
    case C_CONST:
      printf("PUSH  \t%5d", I_OP(i));
      break;
    case C_CURVAR:
      switch (I_OP(i)) {
      case V_PARAM:
	printf("PARAM");
	break;
      case V_CURLOC:
	printf("CURLOC");
	break;
      case V_CURACT:
	printf("CURACT");
	break;
      case V_CURVRB:
	printf("CURVRB");
	break;
      case V_SCORE:
	printf("CURSCORE");
	break;
      default:
	syserr("Unknown CURVAR instruction.");
	break;
      }
      break;
      
    case C_STMOP: 
      switch (I_OP(i)) {
      case I_PRINT: {
	printf("PRINT");
	break;
      }
      case I_SYSTEM: {
	printf("SYSTEM");
	break;
      }
      case I_GETSTR: {
	printf("GETST");
	break;
      }
      case I_QUIT: {
	printf("QUIT");
	break;
      }
      case I_LOOK: {
	printf("LOOK");
	break;
      }
      case I_SAVE: {
	printf("SAVE");
	break;
      }
      case I_RESTORE: {
	printf("RESTORE");
	break;
      }
      case I_RESTART: {
	printf("RESTART");
	break;
      }
      case I_LIST: {
	printf("LIST");
	break;
      }
      case I_EMPTY: {
	printf("EMPTY");
	break;
      }
      case I_SCORE: {
	printf("SCORE");
	break;
      }
      case I_VISITS: {
	printf("VISITS");
	break;
      }
      case I_SCHEDULE: {
	printf("SCHEDULE");
	break;
      }
      case I_CANCEL: {
	printf("CANCEL");
	break;
      }
      case I_MAKE: {
	printf("MAKE");
	break;
      }
      case I_SET: {
	printf("SET");
	break;
      }
      case I_STRSET: {
	printf("STRSET");
	break;
      }
      case I_INCR: {
	printf("INCR");
	break;
      }
      case I_DECR: {
	printf("DECR");
	break;
      }
      case I_ATTRIBUTE: {
	printf("ATTRIBUTE");
	break;
      }
      case I_STRATTR: {
	printf("STRATTR");
	break;
      }
      case I_LOCATE: {
	printf("LOCATE");
	break;
      }
      case I_WHERE: {
	printf("WHERE");
	break;
      }
      case I_HERE: {
	printf("HERE");
	break;
      }
      case I_NEAR: {
	printf("NEAR");
	break;
      }
      case I_USE: {
	printf("USE");
	break;
      }
      case I_IN: {
	printf("IN ");
	break;
      }
      case I_DESCRIBE: {
	printf("DESCRIBE ");
	break;
      }
      case I_SAY: {
	printf("SAY");
	break;
      }
      case I_SAYINT: {
	printf("SAYINT");
	break;
      }
      case I_SAYSTR: {
	printf("SAYSTR");
	break;
      }
      case I_IF: {
	printf("IF");
	break;
      }
      case I_ELSE: {
	printf("ELSE");
	break;
      }
      case I_ENDIF: {
	printf("ENDIF");
	break;
      }
      case I_AND: {
	printf("AND");
	break;
      }
      case I_OR: {
	printf("OR");
	break;
      }
      case I_NE: {
	printf("NE");
	break;
      }
      case I_EQ: {
	printf("EQ ");
	break;
      }
      case I_STREQ: {
	printf("STREQ ");
	break;
      }
      case I_STREXACT: {
	printf("STREXACT ");
	break;
      }
      case I_LE: {
	printf("LE ");
	break;
      }
      case I_GE: {
	printf("GE ");
	break;
      }
      case I_LT: {
	printf("LT ");
	break;
      }
      case I_GT: {
	printf("GT ");
	break;
      }
      case I_PLUS: {
	printf("PLUS ");
	break;
      }
      case I_MINUS: {
	printf("MINUS ");
	break;
      }
      case I_MULT: {
	printf("MULT ");
	break;
      }
      case I_DIV: {
	printf("DIV ");
	break;
      }
      case I_NOT: {
	printf("NOT ");
	break;
      }
      case I_MAX: {
	printf("MAX ");
	break;
      }
      case I_SUM: {
	printf("SUM ");
	break;
      }
      case I_COUNT: {
	printf("COUNT ");
	break;
      }
      case I_RND: {
	printf("RANDOM ");
	break;
      }
      case I_BTW: {
	printf("BETWEEN ");
	break;
      }
      case I_CONTAINS: {
	printf("CONTAINS ");
	break;
      }

      case I_DEPSTART:
	printf("DEPSTART");
	break;

      case I_DEPCASE:
	printf("DEPCASE");
	break;

      case I_DEPEXEC: {
	printf("DEPEXEC");
	break;
      }
	
      case I_DEPELSE:
	printf("DEPELSE");
	break;

      case I_DEPEND:
	printf("DEPEND");
	break;

      case I_RETURN:
	printf("RETURN");
	printf("\n");
	return;

      default:
	syserr("Unknown STMOP instruction.");
	break;
      }
      break;

    default:
      syserr("Unknown instruction class.");
      break;
    }
  }
}



/*----------------------------------------------------------------------

  dumpACD()

  Dump the header and all data

 */
static void dumpACD(void)
{
  printf("VERSION: %d.%d(%d)%c\n", header->vers[0],
	 header->vers[1], header->vers[2], header->vers[3]?header->vers[3]:' ');
  printf("SIZE: %d(0x%x)\n", header->size, header->size);
  printf("PACK: %s\n", header->pack?"Yes":"No");
  printf("PAGLEN: %d\n", header->paglen);
  printf("PAGWIDTH: %d\n", header->pagwidth);
  printf("DEBUG: %s\n", header->debug?"Yes":"No");
  printf("DICT: %d(0x%x)\n", header->dict, header->dict);
  if (dumpdict) dumpDict(1, header->dict);
  printf("OATRS: %d(0x%x)\n", header->oatrs, header->oatrs);
  if (dumpatrs) dumpAtrs(1, header->oatrs);
  printf("LATRS: %d(0x%x)\n", header->latrs, header->latrs);
  if (dumpatrs) dumpAtrs(1, header->latrs);
  printf("AATRS: %d(0x%x)\n", header->aatrs, header->aatrs);
  if (dumpatrs) dumpAtrs(1, header->aatrs);
  printf("ACTS: %d(0x%x)\n", header->acts, header->acts);
  if (dumpacts) dumpActs(1, header->acts);
  printf("OBJS: %d(0x%x)\n", header->objs, header->objs);
  if (dumpobjs) dumpObjs(1, header->objs);
  printf("LOCS: %d(0x%x)\n", header->locs, header->locs);
  if (dumplocs) dumpLocs(1, header->locs);
  printf("STXS: %d(0x%x)\n", header->stxs, header->stxs);
  if (dumpstxs) dumpStxs(1, header->stxs);
  printf("VRBS: %d(0x%x)\n", header->vrbs, header->vrbs);
  if (dumpvrbs) dumpVrbs(1, header->vrbs);
  printf("EVTS: %d(0x%x)\n", header->evts, header->evts);
  printf("CNTS: %d(0x%x)\n", header->cnts, header->cnts);
  if (dumpcnts) dumpCnts(1, header->cnts);
  printf("RULS: %d(0x%x)\n", header->ruls, header->ruls);
  printf("INIT: %d(0x%x)\n", header->init, header->init);
  printf("START: %d(0x%x)\n", header->start, header->start);
  printf("MSGS: %d(0x%x)\n", header->msgs, header->msgs);
  if (dumpmsgs) dumpMsgs(1, header->msgs);
  printf("OBJ-MIN: %3d MAX: %3d\n", header->objmin, header->objmax);
  printf("ACT-MIN: %3d MAX: %3d\n", header->actmin, header->actmax);
  printf("CNT-MIN: %3d MAX: %3d\n", header->cntmin, header->cntmax);
  printf("LOC-MIN: %3d MAX: %3d\n", header->locmin, header->locmax);
  printf("DIR-MIN: %3d MAX: %3d\n", header->dirmin, header->dirmax);
  printf("EVT-MIN: %3d MAX: %3d\n", header->evtmin, header->evtmax);
  printf("RUL-MIN: %3d MAX: %3d\n", header->rulmin, header->rulmax);
  printf("MAXSCORE: %d\n", header->maxscore);
  printf("SCORES: %d(0x%x)\n", header->scores, header->scores);
  printf("FREQ: %d(0x%x)\n", header->freq, header->freq);
  printf("ACDCRC: 0x%x ", header->acdcrc);
  if (acdcrc != header->acdcrc)
    printf("WARNING! Expected 0x%x\n", acdcrc);
  else
    printf("Ok.\n");
  printf("TXTCRC: 0x%x\n", header->txtcrc);
  if (dumpstms != 0)
    dumpStms(dumpstms);
}



/*----------------------------------------------------------------------

  load()

 */
static void load(char acdfnm[])
{
  AcdHdr tmphdr;
  FILE *codfil;
  int i;

  if ((codfil = fopen(acdfnm, "rb")) == NULL) {
    printf("Could not open ACD-file '%s'\n", acdfnm);
    exit(1);
  }

  fread(&tmphdr, sizeof(tmphdr), 1, codfil);
  rewind(codfil);

  /* Allocate and load memory */

#ifdef REVERSED
  reverseHdr(&tmphdr);
#endif

  memory = malloc(tmphdr.size*sizeof(Aword));
  header = (AcdHdr *) memory;

  memTop = fread(addrTo(0), sizeof(Aword), tmphdr.size, codfil);
  if (memTop != tmphdr.size)
    printf("WARNING! Could not read all ACD code.");

  /* Calculate the checksum while the code is still exactly as on file;
     reversing it leaves "done" markers behind in some of the tables. */
  for (i = sizeof(*header)/sizeof(Aword); i < memTop; i++) {
    acdcrc += memory[i]&0xff;
    acdcrc += (memory[i]>>8)&0xff;
    acdcrc += (memory[i]>>16)&0xff;
    acdcrc += (memory[i]>>24)&0xff;
  }

#ifdef REVERSED
  printf("Hmm, this is a little-endian machine, please wait a moment while I fix byte ordering....\n");
  /* Reverse all words in the ACD file */
  reverseACD(header->vers[0] == 2 && header->vers[1] == 5);
  printf("OK.\n");
#endif
}

/*----------------------------------------------------------------------

  Option handling

  The original tool used Thomas Nilsson's SPA package, which is not part of
  this source tree, so the few options it needs are parsed by hand.

 */
static struct {
  char *name;
  int *flag;
  char *doc;
} flags[] = {
  {"dict", &dumpdict, "dump details on dictionary entries"},
  {"atrs", &dumpatrs, "dump details on default attribute entries"},
  {"acts", &dumpacts, "dump details on actor entries"},
  {"objs", &dumpobjs, "dump details on object entries"},
  {"locs", &dumplocs, "dump details on location entries"},
  {"stxs", &dumpstxs, "dump details on syntax entries"},
  {"vrbs", &dumpvrbs, "dump details on verb entries"},
  {"evts", &dumpevts, "dump details on event entries"},
  {"cnts", &dumpcnts, "dump details on container entries"},
  {"msgs", &dumpmsgs, "dump details on message entries"},
  {NULL, NULL, NULL}
};


static void usage(void)
{
  int i;

  printf("Usage: dumpacd <acdfile> [-help] [options]\n");
  printf("    -%-17s%s\n", "help", "this help");
  for (i = 0; flags[i].name != NULL; i++)
    printf("    -%-17s%s\n", flags[i].name, flags[i].doc);
  printf("    -%-17s%s\n", "stms <address>",
	 "dump statement opcodes starting at <address>");
}


static void badArg(char *arg)
{
  printf("Unknown or malformed argument: '%s'\n", arg);
  usage();
  exit(EXIT_FAILURE);
}


static void arguments(int argc, char **argv)
{
  int i, j;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      if (acdfnm != NULL)
	badArg(argv[i]);
      acdfnm = argv[i];
      continue;
    }
    if (strcmp(&argv[i][1], "help") == 0) {
      usage();
      exit(EXIT_SUCCESS);
    }
    if (strcmp(&argv[i][1], "stms") == 0) {
      if (++i == argc)
	badArg(argv[i-1]);
      dumpstms = strtol(argv[i], NULL, 0);
      continue;
    }
    for (j = 0; flags[j].name != NULL; j++)
      if (strcmp(&argv[i][1], flags[j].name) == 0) {
	*flags[j].flag = TRUE;
	break;
      }
    if (flags[j].name == NULL)
      badArg(argv[i]);
  }

  if (acdfnm == NULL) {
    usage();
    exit(EXIT_FAILURE);
  }
}



/*======================================================================

  main()

  Open the file, load it and start dumping...


  */
int main(int argc, char **argv)
{
  arguments(argc, argv);
  load(acdfnm);
  dumpACD();
  exit(0);
}
