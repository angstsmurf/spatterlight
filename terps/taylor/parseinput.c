//
//  parseinput.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-06-04.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "extracommands.h"
#include "parseinput.h"
#include "taylor.h"
#include "utility.h"

char **InputWordStrings = NULL; /* Input split into word strings */
uint8_t Word[5];               /* Dictionary codes for the parsed words (0 = not found) */
int WordPositions[5];          /* Index into InputWordStrings for each Word entry */
int WordsInInput = 0;          /* Number of word strings in InputWordStrings */
int WordIndex = 0;             /* Position in InputWordStrings of the next command */

int FoundExtraCommand = 0;     /* Set when parser finds a non-dictionary extra command */

/* Free all allocated word strings and reset input state. */
void FreeInputWords(void)
{
    WordIndex = 0;
    if (InputWordStrings != NULL) {
        for (int i = 0; i < WordsInInput && InputWordStrings[i] != NULL; i++) {
            free(InputWordStrings[i]);
            InputWordStrings[i] = NULL;
        }
        free(InputWordStrings);
        InputWordStrings = NULL;
    }
    WordsInInput = 0;
}

/* Split a raw input string into individual word strings. Words are
   delimited by whitespace; commas, periods, and semicolons also start
   a new word (for command chaining like "GO NORTH,LOOK"). Results are
   stored in the global InputWordStrings array. */
void SplitIntoWords(const char *string, int length)
{
    if (length < 1) {
        return;
    }

    int startpos[MAX_WORDS];
    int wordlength[MAX_WORDS];

    int words_found = 0;
    int foundcomma = 0;
    int lastwasspace = 1;
    for (int i = 0; string[i] != 0 && i < length; i++) {
        char c = string[i];
        if (c == '.' || c == ',' || c == ';')
            foundcomma = 1;
        if (!isspace((unsigned char)c)) {
            if (lastwasspace || foundcomma) {
                /* Start a new word */
                if (words_found >= MAX_WORDS)
                    break;
                startpos[words_found] = i;
                wordlength[words_found] = 0;
                words_found++;
                foundcomma = 0;
            }
            wordlength[words_found - 1]++;
            lastwasspace = 0;
        } else {
            lastwasspace = 1;
            foundcomma = 0;
        }
    }

    if (words_found == 0) {
        return;
    }

    /* Now we've created two arrays, one for starting positions
     and one for word length. Now we convert these into an array of strings */
    char **words = MemAlloc(words_found * sizeof(*words));

    debug_print("%d words found\n", words_found);
    for (int i = 0; i < words_found; i++) {
        words[i] = (char *)MemAlloc((wordlength[i] + 1));
        memcpy(words[i], string + startpos[i], wordlength[i]);
        words[i][wordlength[i]] = 0;
        debug_print("Word %d: \"%s\"\n", i, words[i]);
    }

    WordsInInput = words_found;
    InputWordStrings = words;
}

/* Read a line of input from the player via Glk, displaying a prompt
   appropriate to the game version. Loops until at least one word is
   entered. Handles non-input events (timer, resize) while waiting. */
void LineInput(void)
{
    event_t ev;
    char buf[512];
    /* Discard any leftover word state from the previous turn and flush
       pending output so the prompt appears below it. */
    FreeInputWords();
    OutFlush();
    FirstAfterInput = 0;
    LastVerb = 0;
    do {
        /* Pretend the previous character was a newline so OutChar's
           capitalization logic treats the prompt as a fresh sentence. */
        LastChar = '\n';
        if (Version == QUESTPROBE3_TYPE) {
            /* QP3 prefixes the prompt with the active character's name
               ("THING I want you to" / "HUMAN TORCH I want you to"),
               except in the intro room. */
            if (MyLoc != 6) {
                if (IsThing == 0)
                    SysMessage(8);
                else
                    SysMessage(9);
            }
            OutCaps();
            /* "I want you to" */
            SysMessage(10);
        } else
            OutString("> ");
        OutFlush();
        glk_request_line_event(Bottom, buf, (glui32)511, 0);

        /* Pump Glk events until the line-input completes. Non-input events
           (timer, resize, etc.) are dispatched to Updates() so the
           interpreter keeps rendering. Transcript is parked across those
           dispatches to keep repeated room descriptions out of the saved log
           when redrawing. */
        while (1) {
            glk_select(&ev);

            if (ev.type == evtype_LineInput)
                break;
            else {
                strid_t ParkedTranscript = Transcript;
                Transcript = NULL;
                Updates(ev);
                Transcript = ParkedTranscript;
            }
        }

        FirstAfterInput = 1;

        int length = ev.val1;
        buf[length] = 0;

        SplitIntoWords(buf, length);

        /* Defensive: the splitter caps at MAX_WORDS so this path is
           currently unreachable, but if it ever fires, discard the input
           and reprompt rather than overrunning downstream buffers. */
        if (WordsInInput >= MAX_WORDS) {
            Display(Bottom, "Too many words!\n");
            FreeInputWords();
        }

        if (WordsInInput > 0 && InputWordStrings) {
            return;
        }

        /* Empty input (whitespace only) — print "Huh?" and reprompt. */
        WordsInInput = 0;
        Display(Bottom, "Huh?");
        FirstAfterInput = 0;

    } while (WordsInInput == 0 || InputWordStrings == NULL);
}

/* Single-letter abbreviation expansions (padded to 4 chars) */
static const char *Abbreviations[] = { "I   ", "L   ", "X   ", "Z   ", "Q   ", "Y   ", NULL };
static const char *AbbreviationValue[] = { "INVE", "LOOK", "EXAM", "WAIT", "QUIT", "YES ", NULL };

/* Sentinel byte that terminates the dictionary in the game file */
#define DICT_END 126

/* Look up a word in the game dictionary. The dictionary is a flat list of
   5-byte entries (4-char word + 1-byte code), terminated by code DICT_END.
   Returns the dictionary code, or 0 if not found (after also checking
   single-letter abbreviations and extra commands). */
int ParseWord(const char *word)
{
    char key[5];
    size_t word_len = strlen(word);
    unsigned char *entry = FileImage + VerbBase;

    if (word_len >= 4) {
        memcpy(key, word, 4);
    } else {
        memcpy(key, word, word_len);
        memset(key + word_len, ' ', 4 - word_len);
    }
    key[4] = 0;
    for (int i = 0; i < 4; i++)
        key[i] = (char)toupper((unsigned char)key[i]);

    while (*entry != DICT_END) {
        if (memcmp(entry, key, 4) == 0)
            return entry[4];
        entry += 5;
    }

    if (word_len == 1) {
        for (int i = 0; Abbreviations[i] != NULL; i++) {
            if (memcmp(Abbreviations[i], key, 4) == 0)
                return ParseWord(AbbreviationValue[i]);
        }
    }

    if (ParseExtraCommand(word)) {
        FoundExtraCommand = 1;
    }
    return 0;
}

/* Words that separate chained commands in a single input line */
static const char *Delimiters[] = { ",", ".", ";", "AND", "THEN", NULL };

/* Check if a word is a command delimiter (case-insensitive). */
static int IsDelimiterWord(const char *word)
{
    size_t word_len = strlen(word);
    for (int i = 0; Delimiters[i] != NULL; i++) {
        const char *delim = Delimiters[i];
        if (strlen(delim) != word_len)
            continue;
        int matched = 1;
        for (size_t j = 0; j < word_len; j++) {
            if (toupper((unsigned char)word[j]) != delim[j]) {
                matched = 0;
                break;
            }
        }
        if (matched)
            return 1;
    }
    return 0;
}

/* Advance WordIndex past the next command delimiter in InputWordStrings.
   Skips consecutive delimiters. Returns 1 if another command follows,
   0 if no more commands remain (triggering a new LineInput). */
static int FindNextCommandDelimiter(void)
{
    if (WordIndex >= WordsInInput - 1 || WordsInInput < 2)
        return 0;
    while (++WordIndex < WordsInInput) {
        if (IsDelimiterWord(InputWordStrings[WordIndex])) {
            WordIndex++;
            while (WordIndex < WordsInInput && IsDelimiterWord(InputWordStrings[WordIndex]))
                WordIndex++;
            if (WordIndex >= WordsInInput)
                return 0;
            else
                return 1;
        }
    }

    return 0;
}

/* Parse the next command from the input. If there are remaining chained
   commands (separated by delimiters), parse the next one; otherwise read
   a new line. Looks up each word in the dictionary and fills the Word[]
   array with up to 5 dictionary codes. Applies game-specific word fixups
   for Blizzard Pass and Questprobe 3. */
void Parser(void)
{
    FoundExtraCommand = 0;

    if (!FindNextCommandDelimiter()) {
        LineInput();
        if (WordsInInput == 0 || InputWordStrings == NULL)
            return;
    } else {
        FirstAfterInput = 0;
        OutChar(' ');
    }

    int out_index = 0;

    for (int offset = 0; offset < 5 && WordIndex + offset < WordsInInput; offset++) {
        int input_pos = WordIndex + offset;
        Word[out_index] = ParseWord(InputWordStrings[input_pos]);
        /* Hack for certain Blizzard Pass verbs */
        /* which are detected as nouns. */
        /* The TaylorMade dictionary format has a single word list and does not
           differentiate between verbs and nouns, but Blizzard Pass seems confused
           about this. */
        if (CurrentGame == BLIZZARD_PASS && out_index == 0) {
            /* Change SKIN to VSKI */
            if (Word[0] == 249)
                Word[0] = 43;
            /* Change POLI to VPOL */
            else if (Word[0] == 175)
                Word[0] = 44;
            /* Change noun DRAW(bridge) to verb DRAW */
            else if (Word[0] == 134)
                Word[0] = 49;
        }
        if (Version == QUESTPROBE3_TYPE && out_index == 1) {
            /* Understand WALL and FIRE as the wall of fire */
            if ((Word[1] == 54 || Word[1] == 44) && (ObjectLoc[10] == MyLoc || ObjectLoc[11] == MyLoc))
                Word[1] = 77;
            /* Understand MASTERS as Alicia Masters */
            if (Word[1] == 106 && ObjectLoc[46] == MyLoc)
                Word[1] = 121;
        }
        if (Word[out_index]) {
            debug_print("Word %d is %d\n", out_index, Word[out_index]);
            WordPositions[out_index] = input_pos;
            out_index++;
        } else {
            if (out_index > 0 && IsDelimiterWord(InputWordStrings[input_pos]))
                break;
            debug_print("Word at position %d, %s, was not recognized\n", input_pos, InputWordStrings[input_pos]);
        }
    }
    for (int i = out_index; i < 5; i++) {
        Word[i] = 0;
        WordPositions[i] = 0;
    }
}
