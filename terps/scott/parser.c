//
//  parser.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-19.

#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "scott.h"
#include "scottdefines.h"

#include "bsd.h"

#define MAX_WORDLENGTH 128
#define MAX_WORDS 128
#define MAX_BUFFER 128

extern struct Command *CurrentCommand;

glui32 **UnicodeWords = NULL;
char **CharWords = NULL;
static int WordsInInput = 0;

static int lastnoun = 0;

static glui32 *FirstErrorMessage = NULL;

const char *EnglishDirections[NUMBER_OF_DIRECTIONS] = {
    NULL, "north", "south", "east", "west", "up", "down",
    "n", "s", "e", "w", "u", "d", " "
};
const char *SpanishDirections[NUMBER_OF_DIRECTIONS] = {
    NULL, "norte", "sur", "este", "oeste", "arriba", "abajo",
    "n", "s", "e", "o", "u", "d", "w"
};
const char *GermanDirections[NUMBER_OF_DIRECTIONS] = {
    NULL, "norden", "sueden", "osten", "westen", "oben", "unten",
    "n", "s", "o", "w", "u", "d", " "
};

const char *Directions[NUMBER_OF_DIRECTIONS];

const char *ExtraCommands[NUMBER_OF_EXTRA_COMMANDS] = {
    NULL,
    "restart",
    "#restart",
    "save",
    "#save",
    "restore",
    "load",
    "#restore",
    "transcript",
    "#transcript",
    "script",
    "#script",
    "oops",
    "undo",
    "bom",
    "#undo",
    "ram",
    "ramload",
    "ramrestore",
    "qload",
    "quickload",
    "#qload",
    "ramsave",
    "qsave",
    "quicksave",
    "#qsave",
    "except",
    "but",
    "#flicker",
    "", "", "", "", ""
};

const char *GermanExtraCommands[NUMBER_OF_EXTRA_COMMANDS] = {
    NULL,
    "restart",
    "#restart",
    "save",
    "#save",
    "restore",
    "load",
    "#restore",
    "transcript",
    "#transcript",
    "script",
    "#script",
    "oops",
    "undo",
    "bom",
    "#undo",
    "ram",
    "ramload",
    "ramrestore",
    "qload",
    "quickload",
    "#qload",
    "ramsave",
    "qsave",
    "quicksave",
    "#qsave",
    "ausser",
    "bis",
    "#flicker",
    "laden",
    "wiederherstellen",
    "transkript",
    "rueckgaengig",
    "neustarten"
};

const char *SpanishExtraCommands[NUMBER_OF_EXTRA_COMMANDS] = {
    NULL,
    "restart",
    "#restart",
    "save",
    "#save",
    "restore",
    "load",
    "#restore",
    "transcript",
    "#transcript",
    "script",
    "#script",
    "oops",
    "undo",
    "bom",
    "#undo",
    "ram",
    "ramload",
    "ramrestore",
    "qload",
    "quickload",
    "#qload",
    "ramsave",
    "qsave",
    "quicksave",
    "#qsave",
    "excepto",
    "menos",
    "#flicker",
    "reanuda",
    "cargar",
    "transcripcion",
    "deshacer",
    "reinicia"
};

extra_command ExtraCommandsKey[NUMBER_OF_EXTRA_COMMANDS] = {
    NO_COMMAND, RESTART, RESTART, SAVE, SAVE, RESTORE, RESTORE,
    RESTORE, SCRIPT, SCRIPT, SCRIPT, SCRIPT, UNDO, UNDO, UNDO, UNDO,
    RAM, RAMLOAD, RAMLOAD, RAMLOAD, RAMLOAD, RAMLOAD, RAMSAVE,
    RAMSAVE, RAMSAVE, RAMSAVE, EXCEPT, EXCEPT, FLICKER,
    RESTORE, RESTORE, SCRIPT, UNDO, RESTART
};

const char *EnglishExtraNouns[NUMBER_OF_EXTRA_NOUNS] = {
    NULL,
    "game",
    "story",
    "on",
    "off",
    "load",
    "restore",
    "save",
    "move",
    "command",
    "turn",
    "all",
    "everything",
    "it",
    " ",
    " ",
};

const char *GermanExtraNouns[NUMBER_OF_EXTRA_NOUNS] = {
    NULL, "spiel", "story", "on", "off", "wiederherstellen",
    "laden", "speichern", "move", "verschieben", "runde",
    "alle", "alles", "es", "einschalten", "ausschalten"
};

const char *SpanishExtraNouns[NUMBER_OF_EXTRA_NOUNS] = {
    NULL, "juego", "story", "on", "off", "cargar",
    "reanuda", "conserva", "move", "command", "jugada",
    "toda", "todo", "eso", "activar", "desactivar"
};

const char *ExtraNouns[NUMBER_OF_EXTRA_NOUNS];

const extra_command ExtraNounsKey[NUMBER_OF_EXTRA_NOUNS] = {
    NO_COMMAND, GAME, GAME, ON, OFF, RAMLOAD,
    RAMLOAD, RAMSAVE, COMMAND, COMMAND, COMMAND,
    ALL, ALL, IT, ON, OFF
};

#define NUMBER_OF_ABBREVIATIONS 6

const char *Abbreviations[NUMBER_OF_ABBREVIATIONS] = { NULL, "i", "l",
    "x", "z", "q" };

const char *AbbreviationsKey[NUMBER_OF_ABBREVIATIONS] = {
    NULL, "inventory", "look", "examine", "wait", "quit"
};

const char *EnglishSkipList[NUMBER_OF_SKIPPABLE_WORDS] = {
    NULL, "at", "to", "in", "into", "the",
    "a", "an", "my", "quickly", "carefully", "quietly",
    "slowly", "violently", "fast", "hard", "now", "room"
};

const char *GermanSkipList[NUMBER_OF_SKIPPABLE_WORDS] = {
    NULL, "nach", "die", "der", "das", "im", "mein", "meine", "an",
    "auf", "den", "lassen", "lass", "fallen", " ", " ", " ", " "
};

const char *SkipList[NUMBER_OF_SKIPPABLE_WORDS];

const char *EnglishDelimiterList[NUMBER_OF_DELIMITERS] = { NULL, ",", "and",
    "then", " " };

const char *GermanDelimiterList[NUMBER_OF_DELIMITERS] = { NULL, ",", "und",
    "dann", "and" };

const char *DelimiterList[NUMBER_OF_DELIMITERS];

static void FreeStrings(void)
{
    if (FirstErrorMessage != NULL) {
        free(FirstErrorMessage);
        FirstErrorMessage = NULL;
    }
    if (WordsInInput == 0) {
        if (UnicodeWords != NULL || CharWords != NULL) {
            Fatal("ERROR! Wordcount 0 but word arrays not empty!\n");
        }
        return;
    }
    for (int i = 0; i < WordsInInput; i++) {
        if (UnicodeWords[i] != NULL)
            free(UnicodeWords[i]);
        if (CharWords[i] != NULL)
            free(CharWords[i]);
    }
    free(UnicodeWords);
    UnicodeWords = NULL;
    free(CharWords);
    CharWords = NULL;
    WordsInInput = 0;
}

static void CreateErrorMessage(const char *fchar, glui32 *second, const char *tchar)
{
    if (FirstErrorMessage != NULL)
        return;
    glui32 *first = ToUnicode(fchar);
    glui32 *third = ToUnicode(tchar);
    glui32 buffer[MAX_BUFFER];
    int i, j = 0, k = 0;
    for (i = 0; first[i] != 0 && i < MAX_BUFFER; i++)
        buffer[i] = first[i];
    if (second != NULL) {
        for (j = 0; second[j] != 0 && i + j < MAX_BUFFER; j++)
            buffer[i + j] = second[j];
    }
    if (third != NULL) {
        for (k = 0; third[k] != 0 && i + j + k < MAX_BUFFER; k++)
            buffer[i + j + k] = third[k];
        free(third);
    }
    int length = i + j + k;
    FirstErrorMessage = MemAlloc((length + 1) * 4);
    memcpy(FirstErrorMessage, buffer, length * 4);
    FirstErrorMessage[length] = 0;
    free(first);
}

/* Single-byte mapping for bytes >= 0x80. Default: map byte to same codepoint (Latin‑1). Overrides below. */
static glui32 MapLatin1(unsigned char b)
{
    return (glui32)b;
}

static glui32 Map_Spanish(unsigned char b)
{
    switch (b) {
    case 0x83:
        return 0x00BF; /* ¿ */
    case 0x80:
        return 0x00A1; /* ¡ */
    case 0x82:
        return 0x00FC; /* ü */
    case '{':
        return 0x00E1; /* á */
    case '}':
        return 0x00ED; /* í */
    case '|':
        return 0x00F3; /* ó */
    case '~':
        return 0x00F1; /* ñ */
    case 0x84:
        return 0x00E9; /* é */
    case 0x85:
        return 0x00FA; /* ú */
    default:
        return (glui32)b;
    }
}

static glui32 MapTI994A(unsigned char b)
{
    switch (b) {
    case '@':
        return 0x00A9; /* © */
    case '}':
        return 0x00FC; /* ü */
    case 12:
        return 0x00F6; /* ö */
    case '{':
        return 0x00E4; /* ä */
    default:
        return (glui32)b;
    }
}

/* Determine mapping function pointer based on CurrentGame */
typedef glui32(*map_fn)(unsigned char);

static map_fn SelectMapper(void)
{
    if (Game && (CurrentGame == GREMLINS_SPANISH ||
                 CurrentGame == GREMLINS_SPANISH_C64))
        return Map_Spanish;
    if (Game && CurrentGame == TI994A)
        return MapTI994A;
    /* default: Latin-1 */
    return MapLatin1;
}

/* Perform sequence folding for German digraphs such as ue->ü, oe->ö, ae->ä, ss->ß. */
static glui32 *FoldGermanSequences(const glui32 *in, size_t in_len,
                                     size_t *out_len)
{
    glui32 *out = MemAlloc((in_len + 1) * sizeof(glui32));
    /* at most as large as input */
    size_t write_pos = 0;
    for (size_t i = 0; i < in_len; ++i) {
        glui32 cp = in[i];
        if (i + 1 < in_len) {
            glui32 next = in[i + 1];
            /* sequences are ASCII letters */
            if (cp == 'u' && next == 'e' &&
                !(i > 0 && in[i - 1] == 'e')) { /* No 'ü' in 'Abenteuer' */
                out[write_pos++] = 0x00FC; /* ü */
                ++i;
                continue;
            }
            else if (cp == 'U' && next == 'E') {
                out[write_pos++] = 0xdc; // Ü
                ++i;
            }
            else if (cp == 'o' && next == 'e') {
                out[write_pos++] = 0x00F6; /* ö */
                ++i;
                continue;
            }
            else if (cp == 'a' && next == 'e') {
                out[write_pos++] = 0x00E4; /* ä */
                ++i;
                continue;
            }
            /* Not a sequences, just a single character */
            /* substitution. */
            else if (cp == '"') {
                out[write_pos++] = 0x2019; /* ’ */
                continue;
            }
            /* As far as I can tell, only five words in the German */
            /* Gremlins output text use the double-s ß character: */
            /* 'außer', 'draußen', 'Straße', 'schießt', and 'geschweißt' */
            /* Simply checking the two preceding characters seems to be */
            /* sufficient to avoid false positives. */
            else if (cp == 's' && next == 's' && i > 1 &&
                    ((in[i - 2] == 'a' && in[i - 1] == 'u') ||
                     (in[i - 2] == 'r' && in[i - 1] == 'a') ||
                     (in[i - 2] == 'i' && in[i - 1] == 'e') ||
                     (in[i - 2] == 'e' && in[i - 1] == 'i'))) {
                    out[write_pos++] = 0x00DF; /* ß */
                    ++i;
                    continue;
            }
        }
        out[write_pos++] = cp;
    }
    out[write_pos] = 0;
    *out_len = write_pos;
    return out;
}

glui32 *ToUnicode(const char *string)
{
    if (string == NULL)
        return NULL;

    size_t in_len = strlen(string);
    if (in_len == 0) {
        glui32 *empty = MemAlloc(sizeof(glui32));
        empty[0] = 0;
        return empty;
    }

    map_fn mapper = SelectMapper();

    size_t cap = in_len + 1;
    glui32 *tmp = MemAlloc(cap * sizeof(glui32));
    size_t out_len = 0;

    size_t i = 0;
    while (i < in_len) {
        unsigned char b = (unsigned char)string[i];
        glui32 mapped = mapper(b);

        if (mapped == 13 || mapped == 10) {
            lastwasnewline = 1;
            mapped = 10;
        } else {
            lastwasnewline = 0;
        }

        /* Special handling: We want the copyright symbol */
        /* to be followed by a space in TI-99/4A games.   */
        if (mapper == MapTI994A && mapped == 0x00A9 && out_len < cap) {
            tmp[out_len++] = mapped;
            mapped = ' ';
        }

        if (out_len + 1 >= cap) {
            cap = cap * 2 + 8;
            tmp = MemRealloc(tmp, cap * sizeof(glui32));
        }
        tmp[out_len++] = mapped;
        i++;
    }

    if (out_len + 1 >= cap) {
        cap = out_len + 2;
        tmp = MemRealloc(tmp, cap * sizeof(glui32));
    }
    /* NUL terminate */
    tmp[out_len] = 0;

    /* If the current game is German,
     * do sequence folding */
    if (Game && (CurrentGame == GREMLINS_GERMAN ||
                 CurrentGame == GREMLINS_GERMAN_C64)) {
        size_t folded_len;
        glui32 *folded = FoldGermanSequences(tmp, out_len, &folded_len);
        free(tmp);
        tmp = folded;
        out_len = folded_len;
    }

    /* Shrink to exact size */
    glui32 *result = MemAlloc((out_len + 1) * sizeof(glui32));
    memcpy(result, tmp, (out_len + 1) * sizeof(glui32));
    free(tmp);
    return result;
}

static char *FromUnicode(glui32 *unicode_string, int origlength)
{
    int sourcepos = 0;
    int destpos = 0;

    char dest[MAX_WORDLENGTH];
    glui32 unichar = unicode_string[sourcepos];
    while (unichar != 0 && destpos + 3 < MAX_WORDLENGTH && sourcepos < origlength) {
        switch (unichar) {
        case '.':
        case ',':
        case ';':
            if (origlength == 1) {
                dest[destpos++] = 'a';
                dest[destpos++] = 'n';
                dest[destpos++] = 'd';
            } else {
                dest[destpos] = (char)unichar;
            }
            break;
        case 0xf6: // ö
            dest[destpos++] = 'o';
            dest[destpos] = 'e';
            break;
        case 0xe4: // ä
            dest[destpos++] = 'a';
            dest[destpos] = 'e';
            break;
        case 0xfc: // ü
            dest[destpos] = 'u';
            if (CurrentGame == GREMLINS_GERMAN ||
                CurrentGame == GREMLINS_GERMAN_C64) {
                destpos++;
                dest[destpos] = 'e';
            }
            break;
        case 0xdf: // ß
            dest[destpos++] = 's';
            dest[destpos] = 's';
            break;
        case 0xed: // í
            dest[destpos] = 'i';
            break;
        case 0xe1: // á
            dest[destpos] = 'a';
            break;
        case 0xf3: // ó
            dest[destpos] = 'o';
            break;
        case 0xf1: // ñ
            dest[destpos] = 'n';
            break;
        case 0xe9: // é
            dest[destpos] = 'e';
            break;
        default:
            dest[destpos] = (char)unichar;
            break;
        }
        sourcepos++;
        destpos++;
        unichar = unicode_string[sourcepos];
    }
    if (destpos == 0)
        return NULL;
    char *result = MemAlloc(destpos + 1);
    memcpy(result, dest, destpos);

    result[destpos] = 0;
    return result;
}

static int MatchYMCA(glui32 *string, int length, int index)
{
    const char *ymca = "y.m.c.a.";
    int i;
    for (i = 0; i < 8; i++) {
        if (i + index > length || string[index + i] != ymca[i])
            return i;
    }
    return i;
}

/* Turns a unicode glui32 string into lower-case ASCII. */
/* Converts German and Spanish diacritical characters */
/* into non-diacritical equivalents */
/* Coalesces runs of whitespace into a single standard space. */
/* Turns word-ending commas and periods into separate strings. */
void SplitIntoWords(glui32 *string, int length)
{
    if (length < 1) {
        return;
    }

    glk_buffer_to_lower_case_uni(string, 512, MIN(length, 512));
    glk_buffer_canon_normalize_uni(string, 512, MIN(length, 512));

    int startpos[MAX_WORDS];
    int wordlength[MAX_WORDS];

    int words_found = 0;
    int word_index = 0;
    int foundspace = 0;
    int foundcomma = 0;
    startpos[0] = 0;
    wordlength[0] = 0;
    int lastwasspace = 1;
    for (int i = 0; string[i] != 0 && i < length && word_index < MAX_WORDS;
         i++) {
        foundspace = 0;
        switch (string[i]) {
        case 'y': {
            int ymca = MatchYMCA(string, length, i);
            if (ymca > 3) {
                /* Start a new word */
                startpos[words_found] = i;
                wordlength[words_found] = ymca;
                words_found++;
                wordlength[words_found] = 0;
                i += ymca;
                if (i < length)
                    foundspace = 1;
                lastwasspace = 0;
            }
        } break;
            /* Unicode space and tab variants */
        case ' ':
        case '\t':
        case '!':
        case '?':
        case '\"':
        case 0x83:   // ¿
        case 0x80:   // ¡
        case 0xa0:   // non-breaking space
        case 0x2000: // en quad
        case 0x2001: // em quad
        case 0x2003: // em
        case 0x2004: // three-per-em
        case 0x2005: // four-per-em
        case 0x2006: // six-per-em
        case 0x2007: // figure space
        case 0x2009: // thin space
        case 0x200A: // hair space
        case 0x202f: // narrow no-break space
        case 0x205f: // medium mathematical space
        case 0x3000: // ideographic space
            foundspace = 1;
            break;
        case '.':
        case ',':
        case ';':
            foundcomma = 1;
            break;
        default:
            break;
        }
        if (!foundspace) {
            if (lastwasspace || foundcomma) {
                /* Start a new word */
                startpos[words_found] = i;
                words_found++;
            }
            wordlength[words_found - 1]++;
            lastwasspace = 0;
        } else {
            /* Check if the last character of previous word was a period or a comma */
            lastwasspace = 1;
            foundcomma = 0;
        }
    }

    if (words_found == 0) {
        return;
    }

    wordlength[words_found]--; /* Don't count final newline
                                  character */

    /* We've created two arrays, one for starting positions
     and one for word length. Now we convert these into an
     array of strings */
    glui32 **words = MemAlloc(words_found * sizeof(*words));
    char **words8 = MemAlloc(words_found * sizeof(*words8));

    for (int i = 0; i < words_found; i++) {
        words[i] = (glui32 *)MemAlloc((wordlength[i] + 1) * 4);
        memcpy(words[i], string + startpos[i], wordlength[i] * 4);
        words[i][wordlength[i]] = 0;
        words8[i] = FromUnicode(words[i], wordlength[i]);
    }
    UnicodeWords = words;
    WordsInInput = words_found;
    CharWords = words8;
}

void LineInput(void)
{
    event_t ev;
    glui32 unibuf[512];

    do {
        Display(Bottom, "\n%s", sys[WHAT_NOW]);
        glk_request_line_event_uni(Bottom, unibuf, (glui32)511, 0);

        while (1) {
            glk_select(&ev);

            if (ev.type == evtype_LineInput)
                break;
            else
                Updates(ev);
        }

        unibuf[ev.val1] = 0;
        lastwasnewline = 1;

        SplitIntoWords(unibuf, ev.val1);

        if (WordsInInput == 0 || CharWords == NULL)
            Output(sys[HUH]);
        else {
            return;
        }

    } while (WordsInInput == 0 || CharWords == NULL);
    return;
}

int WhichWord(const char *word, const char **list, int word_length,
              int list_length)
{
    int n = 1;
    for (int ne = 1; ne < list_length; ne++) {
        const char *tp = list[ne];
        if (!tp)
            continue;
        if (*tp == '*')
            tp++;
        else
            n = ne;
        if (xstrncasecmp(word, tp, word_length) == 0)
            return n;
    }
    return 0;
}

/* List identifiers for static spec tables
 * (so tables can be const). */
typedef enum {
    L_VERBS,
    L_DIRECTIONS,
    L_ABBREVS,
    L_SKIP,
    L_NOUNS,
    L_EXTRACMD,
    L_EXTRANOUN,
    L_DELIM
} ListId;

typedef enum {
    KIND_NORMAL,
    KIND_DIRECTIONS,
    KIND_ABBREVIATIONS,
    KIND_SKIP,
    KIND_EXTRACMD,
    KIND_EXTRANOUN,
    KIND_DELIMITER
} SearchKind;

typedef struct {
    ListId list_id;
    int use_game_word_length; /* 1 => use GameHeader.WordLength, 0 => use strlen(word) */
    SearchKind kind;
} SearchSpec;


/* Map ListId to actual pointers and lengths (lengths when dynamic are provided at call) */
static int get_list_and_length(ListId id, const char ***plist)
{
    switch (id) {
        case L_VERBS:
            *plist = Verbs;
            return GameHeader.NumWords + 1;
        case L_DIRECTIONS:
            *plist = Directions;
            return NUMBER_OF_DIRECTIONS;
        case L_ABBREVS:
            *plist = Abbreviations;
            return NUMBER_OF_ABBREVIATIONS;
        case L_SKIP:
            *plist = SkipList;
            return NUMBER_OF_SKIPPABLE_WORDS;
        case L_NOUNS:
            *plist = Nouns;
            return GameHeader.NumWords + 1;
        case L_EXTRACMD:
            *plist = ExtraCommands;
            return NUMBER_OF_EXTRA_COMMANDS;
        case L_EXTRANOUN:
            *plist = ExtraNouns;
            return NUMBER_OF_EXTRA_NOUNS;
        case L_DELIM:
            *plist = DelimiterList;
            return NUMBER_OF_DELIMITERS;
        default:
            *plist = NULL;
            return 0;
    }
}

/* Core unified search routine.
 * - order: array of SearchSpec describing priority order to try.
 * - out_list: returns the matched list pointer (may be set to NULL if no match).
 *
 * Return values:
 * - For normal matches: index (same as WhichWord).
 * - For mapped extra commands/nouns: (mapped_value + GameHeader.NumWords).
 * - For skip matches: 0 (but out_list set to SkipList).
 * - If nothing matched: 0 and *out_list = NULL.
 */
static int unified_search(const char *word, const SearchSpec *order, const char ***out_list)
{
    *out_list = NULL;
    int list_size = sizeof(*order) / sizeof(order[0]);
    for (int s = 0; s < list_size; s++) {
        const SearchSpec *spec = &order[s];
        const char **list = NULL;
        int list_length = get_list_and_length(spec->list_id, &list);
        if (!list || list_length <= 1)
            continue;
        int match_len = spec->use_game_word_length ? GameHeader.WordLength : (int)strlen(word);
        int idx = WhichWord(word, list, match_len, list_length);
        if (!idx)
            continue;

        switch (spec->kind) {
            case KIND_NORMAL:
                *out_list = list;
                return idx;

            case KIND_DIRECTIONS:
                /* Return adjusted index (preserve original numeric mapping logic). */
                if (idx == 13)
                    idx = 4;
                if (idx > 6)
                    idx -= 6;
                *out_list = list;
                return idx;

            case KIND_ABBREVIATIONS: {
                int ab_idx = idx;
                if (ab_idx > 0 && AbbreviationsKey[ab_idx]) {
                    int v = WhichWord(AbbreviationsKey[ab_idx], Verbs, GameHeader.WordLength, GameHeader.NumWords + 1);
                    if (v) {
                        *out_list = Verbs;
                        return v;
                    }
                }
                break; /* fall through to next spec if abbrev didn't map */
            }

            case KIND_SKIP:
                *out_list = list;
                return 0; /* skippable */

            case KIND_EXTRACMD: {
                *out_list = list;
                int mapped = ExtraCommandsKey[idx];
                return mapped + GameHeader.NumWords;
            }

            case KIND_EXTRANOUN: {
                *out_list = list;
                int mapped = ExtraNounsKey[idx];
                return mapped + GameHeader.NumWords;
            }

            case KIND_DELIMITER:
                *out_list = list;
                return idx;
        }
    }

    *out_list = NULL;
    return 0;
}

/* Static const search orders to reduce wrapper code repetition */
static const SearchSpec verb_search_order[] = {
    { L_VERBS,      1, KIND_NORMAL },
    { L_DIRECTIONS, 1, KIND_DIRECTIONS },
    { L_ABBREVS,    1, KIND_ABBREVIATIONS },
    { L_SKIP,       0, KIND_SKIP },       /* use full string length for skip */
    { L_NOUNS,      1, KIND_NORMAL },
    { L_EXTRACMD,   0, KIND_EXTRACMD },
    { L_EXTRANOUN,  0, KIND_EXTRANOUN },
    { L_DELIM,      0, KIND_DELIMITER }
};

static const SearchSpec noun_search_order[] = {
    { L_NOUNS,      1, KIND_NORMAL },
    { L_DIRECTIONS, 1, KIND_DIRECTIONS },
    { L_EXTRANOUN,  0, KIND_EXTRANOUN },
    { L_SKIP,       0, KIND_SKIP },       /* use full string length for skip */
    { L_VERBS,      1, KIND_NORMAL },
    { L_DELIM,      0, KIND_DELIMITER }
};

/* Thin wrappers that preserve previous call-sites and list-pointer semantics. */
static int FindVerb(const char *string, const char ***list)
{
    return unified_search(string, verb_search_order, list);
}

static int FindNoun(const char *string, const char ***list)
{
    return unified_search(string, noun_search_order, list);
}

static struct Command *CommandFromStrings(int index, struct Command *previous);

static int FindExtaneousWords(int *index, int noun)
{
    /* Looking for extraneous words that should invalidate the command */
    int original_index = *index;
    if (*index >= WordsInInput) {
        return 0;
    }
    const char **list = NULL;
    int verb = 0;
    int stringlength = strlen(CharWords[*index]);

    int secondnoun = WhichWord(CharWords[*index], Nouns, GameHeader.WordLength, GameHeader.NumWords + 1);
    if (secondnoun) {
        if (MapSynonym(secondnoun) == MapSynonym(noun)) {
            *index = *index + 1;
            return 0;
        }
    }

    list = SkipList;
    do {
        verb = WhichWord(CharWords[*index], SkipList, stringlength,
            NUMBER_OF_SKIPPABLE_WORDS);
        if (verb)
            *index = *index + 1;
    } while (verb && *index < WordsInInput);

    if (*index >= WordsInInput)
        return 0;

    FindVerb(CharWords[*index], &list);

    if (list == DelimiterList) {
        if (*index > original_index)
            *index = *index - 1;
        return 0;
    }

    if (list == NULL) {
        if (*index >= WordsInInput)
            *index = WordsInInput - 1;
        CreateErrorMessage(sys[I_DONT_KNOW_WHAT_A], UnicodeWords[*index], sys[IS]);
    } else {
        CreateErrorMessage(sys[I_DONT_UNDERSTAND], NULL, NULL);
    }

    return 1;
}

static struct Command *CreateCommandStruct(int verb, int noun, int verbindex,
    int nounindex, struct Command *previous)
{
    struct Command *command = MemAlloc(sizeof(struct Command));
    command->verb = verb;
    command->noun = noun;
    command->allflag = 0;
    command->item = 0;
    command->previous = previous;
    command->verbwordindex = verbindex;
    if (noun && nounindex > 0) {
        command->nounwordindex = nounindex - 1;
    } else {
        command->nounwordindex = 0;
    }
    command->next = CommandFromStrings(nounindex, command);
    return command;
}

static struct Command *CommandFromStrings(int index, struct Command *previous)
{
    if (index < 0 || index >= WordsInInput) {
        return NULL;
    }
    const char **list = NULL;
    int verb = 0;
    int i = index;

    do {
        /* Checking if it is a verb */
        verb = FindVerb(CharWords[i++], &list);
    } while ((list == SkipList || list == DelimiterList) && i < WordsInInput);

    int verbindex = i - 1;

    if (list == Directions) {
        /* It is a direction */
        if (verb == 0 || FindExtaneousWords(&i, 0) != 0)
            return NULL;
        return CreateCommandStruct(GO, verb, 0, i, previous);
    }

    int found_noun_at_verb_position = 0;
    int lastverb = 0;

    if (list == Nouns || list == ExtraNouns) {
        /* It is a noun */
        /* If we find no verb, we try copying the verb from the previous command */
        if (previous) {
            lastverb = previous->verb;
        }
        /* Unless the game is German, where we allow the noun to come before the verb */
        if (CurrentGame != GREMLINS_GERMAN && CurrentGame != GREMLINS_GERMAN_C64) {
            if (!previous) {
                CreateErrorMessage(sys[I_DONT_KNOW_HOW_TO], UnicodeWords[i - 1],
                    sys[SOMETHING]);
                return NULL;
            } else {
                verbindex = previous->verbwordindex;
            }
            if (FindExtaneousWords(&i, verb) != 0)
                return NULL;

            return CreateCommandStruct(lastverb, verb, verbindex, i, previous);
        } else {
            found_noun_at_verb_position = 1;
        }
    }

    if (list == NULL || list == SkipList) {
        CreateErrorMessage(sys[I_DONT_KNOW_HOW_TO], UnicodeWords[i - 1],
            sys[SOMETHING]);
        return NULL;
    }

    if (i == WordsInInput) {
        if (lastverb) {
            return CreateCommandStruct(lastverb, verb, previous->verbwordindex, i,
                previous);
        } else if (found_noun_at_verb_position) {
            CreateErrorMessage(sys[I_DONT_KNOW_HOW_TO], UnicodeWords[i - 1],
                sys[SOMETHING]);
            return NULL;
        } else {
            return CreateCommandStruct(verb, 0, i - 1, i, previous);
        }
    }

    int noun = 0;

    do {
        /* Check if it is a noun */
        noun = FindNoun(CharWords[i++], &list);
    } while (list == SkipList && i < WordsInInput);

    if (list == Nouns || list == Directions || list == ExtraNouns) {
        /* It is a noun */

        /* Check if it is an ALL followed by EXCEPT */
        int except = 0;
        if (list == ExtraNouns && i < WordsInInput && noun - GameHeader.NumWords == ALL) {
            int stringlength = strlen(CharWords[i]);
            except = WhichWord(CharWords[i], ExtraCommands, stringlength,
                NUMBER_OF_EXTRA_COMMANDS);
        }
        if (ExtraCommandsKey[except] != EXCEPT && FindExtaneousWords(&i, noun) != 0)
            return NULL;
        /* If we found a noun where a verb was expected, check
           again to see if it matches a verb as well */
        if (found_noun_at_verb_position) {
            int realverb = WhichWord(CharWords[i - 1], Verbs, GameHeader.WordLength,
                GameHeader.NumWords);
            if (realverb) {
                noun = verb;
                verb = realverb;
            } else if (lastverb) {
                noun = verb;
                verb = lastverb;
            }
        }
        return CreateCommandStruct(verb, noun, verbindex, i, previous);
    }

    if (list == DelimiterList) {
        /* It is a delimiter */
        return CreateCommandStruct(verb, 0, verbindex, i, previous);
    }

    if (list == Verbs && found_noun_at_verb_position) {
        /* It is a verb */
        /* Check if it is an ALL followed by EXCEPT */
        int except = 0;
        if (i < WordsInInput && verb - GameHeader.NumWords == ALL) {
            int stringlength = strlen(CharWords[i]);
            except = WhichWord(CharWords[i], ExtraCommands, stringlength,
                NUMBER_OF_EXTRA_COMMANDS);
        }
        if (ExtraCommandsKey[except] != EXCEPT && FindExtaneousWords(&i, 0) != 0)
            return NULL;
        return CreateCommandStruct(noun, verb, i - 1, i, previous);
    }

    CreateErrorMessage(sys[I_DONT_KNOW_WHAT_A], UnicodeWords[i - 1], sys[IS]);
    return NULL;
}

static int CreateAllCommands(struct Command *command)
{

    if (GameHeader.NumItems > 2048)
        Fatal("Bad number of items");
    int exceptions[2048];
    int exceptioncount = 0;

    int location = CARRIED;
    if (command->verb == TAKE)
        location = MyLoc;

    struct Command *next = command->next;
    /* Check if the ALL command is followed by EXCEPT */
    while (next && next->verb == GameHeader.NumWords + EXCEPT) {
        for (int i = 0; i <= GameHeader.NumItems; i++) {
            if (Items[i].AutoGet && xstrncasecmp(Items[i].AutoGet, CharWords[next->nounwordindex], GameHeader.WordLength) == 0) {
                exceptions[exceptioncount++] = i;
            }
        }
        /* Remove the EXCEPT command from the linked list of commands */
        next = next->next;
        free(command->next);
        command->next = next;
    }

    struct Command *c = command;
    int found = 0;
    for (int i = 0; i < GameHeader.NumItems; i++) {
        if (Items[i].AutoGet != NULL && Items[i].AutoGet[0] != '*' && Items[i].Location == location) {
            int exception = 0;
            for (int j = 0; j < exceptioncount; j++) {
                if (exceptions[j] == i) {
                    exception = 1;
                    break;
                }
            }
            if (!exception) {
                if (found) {
                    c->next = MemAlloc(sizeof(struct Command));
                    c->next->previous = c;
                    c = c->next;
                }
                found = 1;
                c->verb = command->verb;
                c->noun = WhichWord(Items[i].AutoGet, Nouns, GameHeader.WordLength,
                    GameHeader.NumWords);
                c->item = i;
                c->next = NULL;
                c->nounwordindex = 0;
                c->allflag = 1;
            }
        }
    }
    if (found == 0) {
        if (command->verb == TAKE)
            CreateErrorMessage(sys[NOTHING_HERE_TO_TAKE], NULL, NULL);
        else
            CreateErrorMessage(sys[YOU_HAVE_NOTHING], NULL, NULL);
        return 0;
    } else {
        c->next = next;
        c->allflag = 1 | LASTALL;
    }
    return 1;
}

void FreeCommands(void)
{
    while (CurrentCommand && CurrentCommand->previous)
        CurrentCommand = CurrentCommand->previous;
    while (CurrentCommand) {
        struct Command *temp = CurrentCommand;
        CurrentCommand = CurrentCommand->next;
        free(temp);
    }
    CurrentCommand = NULL;
    FreeStrings();
    if (FirstErrorMessage)
        free(FirstErrorMessage);
    FirstErrorMessage = NULL;
}

static void PrintPendingError(void)
{
    if (FirstErrorMessage) {
        glk_put_string_stream_uni(glk_window_get_stream(Bottom), FirstErrorMessage);
        free(FirstErrorMessage);
        FirstErrorMessage = NULL;
        StopTime = 1;
    }
}

int GetInput(int *vb, int *no)
{
    if (CurrentCommand && CurrentCommand->next) {
        CurrentCommand = CurrentCommand->next;
    } else {
        PrintPendingError();
        if (CurrentCommand)
            FreeCommands();
        LineInput();
        CurrentCommand = CommandFromStrings(0, NULL);
    }

    if (CurrentCommand == NULL) {
        PrintPendingError();
        return 1;
    }

    /* Hack to make ALLE FALLEN LASSEN work in German Gremlins    */
    /* The normal verb <-> noun switching mechanism gets confused */
    /* by the fact that the game lists FALLEN and LASSEN as both  */
    /* verbs and nouns. */

    if ((CurrentGame == GREMLINS_GERMAN || CurrentGame == GREMLINS_GERMAN_C64) && CurrentCommand->verb - GameHeader.NumWords == ALL && CurrentCommand->noun == 123) {
        CurrentCommand->verb = DROP;
        CurrentCommand->noun = ALL + GameHeader.NumWords;
    }

    /* Hack to make RESTORE and RESTART work in Robin of Sherwood */
    /* instead of being understood as REST, a synonym of WAIT.     */
    if (Game->type == SHERWOOD_VARIANT && CurrentCommand->verb == 56) {
        char *verbword = CharWords[CurrentCommand->verbwordindex];
        if (xstrncasecmp(verbword, "restore", 7) == 0)
            CurrentCommand->verb = GameHeader.NumWords + RESTORE;
        else if (xstrncasecmp(verbword, "restart", 7) == 0)
            CurrentCommand->verb = GameHeader.NumWords + RESTART;
    }

    /* We use NumWords + verb for our extra commands */
    /* such as UNDO and TRANSCRIPT */
    if (CurrentCommand->verb > GameHeader.NumWords) {
        if (!PerformExtraCommand(0)) {
            CreateErrorMessage(sys[I_DONT_UNDERSTAND], NULL, NULL);
        }
        return 1;
        /* And NumWords + noun for our extra nouns */
        /* such as ALL */
    } else if (CurrentCommand->noun > GameHeader.NumWords) {
        CurrentCommand->noun -= GameHeader.NumWords;
        if (CurrentCommand->noun == ALL) {
            if (CurrentCommand->verb != TAKE && CurrentCommand->verb != DROP) {
                CreateErrorMessage(sys[CANT_USE_ALL], NULL, NULL);
                return 1;
            }
            if (!CreateAllCommands(CurrentCommand))
                return 1;
        } else if (CurrentCommand->noun == IT) {
            CurrentCommand->noun = lastnoun;
        }
    }

    *vb = CurrentCommand->verb;
    *no = CurrentCommand->noun;

    if (*no > 6) {
        lastnoun = *no;
    }

    return 0;
}

int RecheckForExtraCommand(void)
{
    const char *VerbWord = CharWords[CurrentCommand->verbwordindex];

    int ExtraVerb = WhichWord(VerbWord, ExtraCommands, GameHeader.WordLength,
        NUMBER_OF_EXTRA_COMMANDS);
    if (!ExtraVerb) {
        return 0;
    }
    int ExtraNoun = 0;
    if (CurrentCommand->noun) {
        const char *NounWord = CharWords[CurrentCommand->nounwordindex];
        ExtraNoun = WhichWord(NounWord, ExtraNouns, strlen(NounWord),
            NUMBER_OF_EXTRA_NOUNS);
    }
    CurrentCommand->verb = ExtraCommandsKey[ExtraVerb];
    if (ExtraNoun)
        CurrentCommand->noun = ExtraNounsKey[ExtraNoun];

    return PerformExtraCommand(1);
}
