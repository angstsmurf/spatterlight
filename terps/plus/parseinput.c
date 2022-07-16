//
//  parseinput.c
//  plus
//
//  Created by Administrator on 2022-06-04.
//

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "glk.h"

#include "common.h"
#include "definitions.h"
#include "restorestate.h"
#include "extracommands.h"
#include "parseinput.h"

char **InputWordStrings = NULL;
int WordsInInput = 0;
int WordIndex = 0;
int ProtagonistString = 0;

DictWord *Verbs;
DictWord *Nouns;
DictWord *Adverbs;
DictWord *Prepositions;

int LastVerb = 0, LastNoun = 0, LastPrep = 0, LastPartp = 0, LastNoun2 = 0, LastAdverb = 0;

void FreeInputWords(void) {
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

int CompareUpToHashSign(char *word1, char *word2) {
    size_t len1 = strlen(word1);
    size_t len2 = strlen(word2);
    size_t longest = MAX(len1, len2);
    char w1[longest];
    char w2[longest];
    memcpy(w1, word1, len1);
    memcpy(w2, word2, len2);
    for (int i = 0; i < longest && w1[i] != '#' && w2[i] != '#'; i++) {
        if (w1[i] != w2[i])
            return 0;
    }
    return 1;
}

int YesOrNo(void) {
    glk_request_char_event(Bottom);

    event_t ev;
    int result = 0;
    const char y = tolower((unsigned char)sys[YES][0]);
    const char n = tolower((unsigned char)sys[NO][0]);

    do {
        glk_select(&ev);
        if (ev.type == evtype_CharInput) {
            const char reply = tolower(ev.val1);
            if (reply == y) {
                result = 1;
            } else if (reply == n) {
                result = 2;
            } else {
                Output(sys[ANSWER_YES_OR_NO]);
                glk_request_char_event(Bottom);
            }
        } else
            Updates(ev);
    } while (result == 0);

    Display(Bottom, "%c\n", (char)ev.val1);
    lastwasnewline = 1;
    return (result == 1);
}

static int FindNounWithHash(char *p)
{
    int num_matches = 0;
    int matches[10];

    for (int i = 0; i <= GameHeader.NumItems; i++) {
        if (Items[i].Dictword && (Items[i].Location == MyLoc || Items[i].Location == CARRIED))
            if (CompareUpToHashSign(Nouns[GetDictWord(Items[i].Dictword)].Word, p)) {
                matches[num_matches] = i;
                num_matches++;
            }
    }

    if (num_matches == 1)
        return Items[matches[0]].Dictword;

    if (num_matches) {
        for (int i = 0; i < num_matches; i++) {
            Display(Bottom, "When you say %s, do you mean %s? ", p, Items[matches[i]].Text);
            if (YesOrNo()) {
                Output("Thank-you! ");
                return Items[matches[i]].Dictword;
            }
        }
    }
    return 0;
}

static int ParseWord(char *p, DictWord *dict)
{
    for (int i = 0; dict[i].Word != NULL; i++) {
        if (strcmp(dict[i].Word, p) == 0) {
            return (dict[i].Group);
        }
    }

    return 0;
}

static int MatchingChars(const char *synonym, const char *original)
{
    if (synonym[0] == 0 || original[0] == 0 || synonym == NULL || original == NULL)
        return 0;
    int i;
    for (i = 0; synonym[i] != 0 && original[i] != 0; i++) {
        if (synonym[i] != original[i])
            return 0;
    }

    return (i == strlen(synonym) && (original[i] == 0 || original[i] == ' ' || (i > 0 && original[i-1] == 0)));
}

static int IsSynonymMatch(const char *synonym, const char *original) {
    char c = synonym[0];
    if (c == 0)
        return -1;
    char *found = strchr(original, c);
    while (found) {
        if (found == original || *(found - 1) == ' ') {
            if (MatchingChars(synonym, found))
                return (int)(found - original);
        }
        found = strchr(found + 1, c);
    }
    return -1;
}

static char *StripDoubleSpaces(char *result, int *len) {
    int found = 0;
    do {
        found = 0;
        for (int i = 0; i < *len - 1; i++) {
            if (isspace(result[i]) && isspace(result[i + 1]))
            {
                found = 1;
                char new[256];
                if (i > 0)
                    memcpy(new, result, i);
                int taillen = *len - i;
                if (taillen > 0) {
                    memcpy(new + i, result + i + 1, taillen);
                }
                free(result);
                (*len)--;
                new[*len] = 0;
                result = MemAlloc(*len + 1);
                memcpy(result, new, *len + 1);
                break;
            }
        }
    } while (found == 1);
    result[*len] = 0;
    return result;
}

static char *StripPunctuation(char *buf, int *len) {
    for (int i = 0; i < *len; i++) {
        char c = buf[i];
        if (c == '.' || c == ',' || c == ':' || c == ';' || c == '\'' || c == '\"' || c == '-' || c == '!')
        {
            buf[i] = ' ';
        }
    }
    buf[*len] = 0;
    char *final = MemAlloc(*len + 1);
    memcpy(final, buf, *len + 1);
    return final;
}


static char *ConcatStrings(const char *head, int headlen, const char *mid, int midlen, const char *tail, int taillen)
{
    int totallen = headlen + midlen + taillen;
    char *result = MemAlloc(totallen + 1);
    memcpy(result, head, headlen);
    memcpy(result + headlen, mid, midlen);
    memcpy(result + headlen + midlen, tail, taillen);
    result[totallen] = 0;
    if (totallen > 1)
        result = StripDoubleSpaces(result, &totallen);
    return result;
}


static char *ReplaceString(char *source, const char *pattern, const char *replacement, int *startpos)
{
    char *result = NULL;
    char *head = NULL, *tail = NULL;
    int headlength = *startpos;
    head = MemAlloc(headlength + 1);
    memcpy(head, source, headlength);
    head[*startpos] = 0;
    int tailpos = headlength + (int)strlen(pattern);
    int sourcelen = (int)strlen(source);
    int taillength = sourcelen - tailpos;
    if (taillength < 0 || taillength > sourcelen) {
        taillength = 0;
        tailpos = sourcelen;
    }
    tail = MemAlloc(taillength + 1);
    memcpy(tail, source + tailpos, taillength);
    tail[taillength] = 0;
    int midlength = (int)strlen(replacement);
    result = ConcatStrings(head, headlength, replacement, midlength, tail, taillength);
    free(head);
    free(tail);
    free(source);
    *startpos = headlength + midlength + taillength;
    return result;
}

static char *ReplaceSynonyms(char *buf, int *len)
{
    char *result = MemAlloc(*len + 1);
    for(int i = 0; i < *len; i++)
        result[i] = toupper(buf[i]);
    result = StripDoubleSpaces(result, len);
    int finallen = (int)strlen(result);
    for (int i = 0; Substitutions[i].SynonymString != NULL; i++) {
        char *syn = Substitutions[i].SynonymString;
        finallen = IsSynonymMatch(syn, result);
        while (finallen >= 0) {
            debug_print("Replaced string \"%s\" with \"%s\".\n", syn, Substitutions[i].ReplacementString);
            result = ReplaceString(result, syn, Substitutions[i].ReplacementString, &finallen);
            debug_print("Result: \"%s\"\n", result);
            finallen = IsSynonymMatch(syn, result);
        }
    }
    *len = (int)strlen(result);
    result[*len] = 0;
    free(buf);
    return result;
}

static char **SplitIntoWords(const char *string, int length)
{
    if (length < 1) {
        return NULL;
    }

    int startpos[MAX_WORDS];
    int wordlength[MAX_WORDS];

    int words_found = 0;
    int word_index = 0;
    startpos[0] = 0;
    wordlength[0] = 0;
    int lastwasspace = 1;
    for (int i = 0; string[i] != 0 && i < length && word_index < MAX_WORDS; i++) {
        if (!isspace(string[i])) {
            if (lastwasspace) {
                /* Start a new word */
                startpos[words_found] = i;
                words_found++;
                wordlength[words_found] = 0;
            }
            wordlength[words_found - 1]++;
            lastwasspace = 0;
        } else {
            lastwasspace = 1;
        }
    }

    if (words_found == 0) {
        return NULL;
    }

    wordlength[words_found]--; /* Don't count final newline character */

    /* Now we've created two arrays, one for starting postions
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

    return words;
}

WordToken *TokenWords = NULL;

static int IntendedAsVerb(int i) {
    if (i == 0)
        return 1;
    for (int j = i; j >= 0; j--)
        if (TokenWords[j].Type != ADVERB_TYPE)
            return 0;
    return 1;
}

static void WordNotFoundError(int i) {
    if (IntendedAsVerb(i))
        Display(Bottom, "I don't know how to %s something! ", InputWordStrings[i]);
    else
        Display(Bottom, "I don't know what %s means. ", InputWordStrings[i]);
}

static int TokenizeInputWords(void) {
    if (TokenWords != NULL)
        free(TokenWords);
    int word_not_found = -1;
    int found_verb = 0;
    TokenWords = MemAlloc(WordsInInput * sizeof(WordToken));
    int result = 0;

    for (int i = 0; i < WordsInInput; i++) {

        if (i == 0) {
            result = ParseWord(InputWordStrings[i], Verbs);
            if (result > 0) {
                debug_print("Found verb %s at %d\n", InputWordStrings[i], i);
                found_verb = 1;
                TokenWords[i].Index = result;
                TokenWords[i].Type = VERB_TYPE;
                continue;
            }
        }

        result = ParseWord(InputWordStrings[i], Adverbs);
        if (result > 1) {
            debug_print("Found adverb %s at %d\n", InputWordStrings[i], i);
            TokenWords[i].Index = result;
            TokenWords[i].Type = ADVERB_TYPE;
            continue;
        }

        result = ParseWord(InputWordStrings[i], Nouns);
        if (result > 0 || strcmp(InputWordStrings[i], "ANY") == 0) {
            debug_print("Found noun %s at %d\n", InputWordStrings[i], i);
            TokenWords[i].Index = result;
            LastNoun = result;
            TokenWords[i].Type = NOUN_TYPE;
            continue;
        }

        result = FindNounWithHash(InputWordStrings[i]);
        if (result > 0) {
            debug_print("Found noun %s at %d\n", InputWordStrings[i], i);
            TokenWords[i].Index = result;
            LastNoun = result;
            TokenWords[i].Type = NOUN_TYPE;
            continue;
        }

        result = ParseWord(InputWordStrings[i], Prepositions);
        if (result > 1) {
            if (result == 2 && !found_verb) {
                Output("-USE- may NOT be the first word of your command! ");
                WordIndex = 255;
                return 0;
            } else {
                debug_print("Found preposition %s at %d\n", InputWordStrings[i], i);
                TokenWords[i].Index = result;
                TokenWords[i].Type = PREPOSITION_TYPE;
                continue;
            }
        }

        if (i > 0)
            result = ParseWord(InputWordStrings[i], Verbs);
        if (result > 0) {
            debug_print("Found verb %s at %d\n", InputWordStrings[i], i);
            TokenWords[i].Index = result;
            TokenWords[i].Type = VERB_TYPE;
            found_verb = 1;
            continue;
        }

        result = ParseWord(InputWordStrings[i], ExtraWords);
        if (result > 0) {
            debug_print("Found extra word %s at %d\n", InputWordStrings[i], i);
            if (result == 1) {
                TokenWords[i].Index = LastNoun;
                TokenWords[i].Type = NOUN_TYPE;
            } else if (result ==4) {
                Output("I'm only your puppet! You must figure things out for yourself! ");
                WordIndex = 255;
                return 0;
            } else {
                TokenWords[i].Index = result;
                TokenWords[i].Type = OTHER_TYPE;
            }
            continue;
        }

        word_not_found = i;
    }

    /* We wait to report "word not recognized" errors until we have tokenized all words   */
    /* as we want to give other errors priority. Also helps deciding whether "The rest of */
    /* as we want to give other errors priority. Also helps deciding whether to say       */
    /* "The rest of your command was ignored." */
    if (word_not_found >= 0) {
        WordIndex = word_not_found;
        WordNotFoundError(word_not_found);
        return 0;
    }

    return 1;
}

int IsNextParticiple(int partp, int noun2) {
    if (WordIndex >= WordsInInput) {
        return 0;
    }
    if (partp == 1) { // None
        return 0;
    }
    if (TokenWords[WordIndex].Type == NOUN_TYPE && partp == 2 &&
        (noun2 == 0 || TokenWords[WordIndex].Index == noun2)) {
        CurPartp = 2;
        CurNoun2 = TokenWords[WordIndex++].Index;
        return 1;
    } else if (TokenWords[WordIndex].Type == PREPOSITION_TYPE && WordIndex < WordsInInput - 1 &&
        (partp == 0 || TokenWords[WordIndex].Index == partp)) {
        if (TokenWords[WordIndex + 1].Type == NOUN_TYPE && (noun2 == 0 || TokenWords[WordIndex + 1].Index == noun2)) {
            CurPartp = TokenWords[WordIndex++].Index;
            CurNoun2 = TokenWords[WordIndex++].Index;
            return 1;
        }
    }
    return 0;
}

static int CommandFromTokens(int verb, int noun)
{
    debug_print("CommandFromTokens: WordIndex: %d verb: %d\n", WordIndex, verb);

    if (WordIndex == 0 && verb != 0)
        debug_print("Bug!");

    if (!WordsInInput || (WordIndex >= WordsInInput)) {
        FreeInputWords();
        return 1;
    }

    int initialindex = WordIndex;

    for (int i = WordIndex; i < WordsInInput; i++) {
        size_t len = strlen(InputWordStrings[i]);
        char str[128];
        for (int j = 0; j < len; j++)
            str[j] = InputWordStrings[i][j];
        str[len] = 0;
        debug_print("Word %d: %s\n", i, str);
    }

    if (TokenWords[WordIndex].Type == OTHER_TYPE) {
        int word = TokenWords[WordIndex].Index;
        WordIndex++;
        int nextword = 0;
        if (WordIndex < WordsInInput) {
            nextword = ParseWord(InputWordStrings[WordIndex], ExtraWords);
        }
        if (nextword < 9) {
            nextword = 0;
        } else WordIndex++;
        int result = PerformExtraCommand(word, nextword);
        if (result < 3)
            return result;
    }

    CurPartp = 0;
    CurPrep = 0;
    CurNoun2 = 0;
    CurAdverb = 0;

    if (TokenWords[WordIndex].Type == VERB_TYPE) {
        verb = TokenWords[WordIndex++].Index;
    }

    if (WordIndex < WordsInInput && TokenWords[WordIndex].Type == ADVERB_TYPE) {
        CurAdverb = TokenWords[WordIndex++].Index;
        debug_print("Found adverb \"");
        PrintDictWord(CurAdverb, Adverbs);
        debug_print("\" (%d)\n", CurAdverb);
        if (WordIndex < WordsInInput && verb == 0 && TokenWords[WordIndex].Type == VERB_TYPE) {
            verb = TokenWords[WordIndex++].Index;
        }
    }

    if (verb == 0) {
        WordNotFoundError(0);
        Output("\nTry again please.\n");
        WordIndex = WordsInInput;
        return 1;
    }

    debug_print("Found verb \"");
    PrintDictWord(verb, Verbs);
    debug_print("\" (%d)\n", verb);

    if (WordIndex >= WordsInInput) {
        CurVerb = verb;
        CurNoun = noun;
        return 0;
    }

    if (TokenWords[WordIndex].Type == NOUN_TYPE) {
        noun = TokenWords[WordIndex++].Index;
    }

    if (noun == 0) {
        if (WordIndex < WordsInInput && TokenWords[WordIndex].Type == PREPOSITION_TYPE) {
            CurPrep = TokenWords[WordIndex++].Index;
            if (WordIndex < WordsInInput && TokenWords[WordIndex].Type == NOUN_TYPE) {
                noun = TokenWords[WordIndex++].Index;
            }
        }
    }

    if (CurPrep != 0) {
        debug_print("Found preposition \"");
        PrintDictWord(CurPrep, Prepositions);
        debug_print("\" (%d)\n", CurPrep);
    }

    if (noun != 0) {
        debug_print("Found noun \"");
        PrintDictWord(noun, Nouns);
        debug_print("\" (%d)\n", noun);
    }

    if (WordIndex < WordsInInput && TokenWords[WordIndex].Type == ADVERB_TYPE) {
        CurAdverb = TokenWords[WordIndex++].Index;
        if (CurAdverb > 0) {
            debug_print("Found adverb \"");
            PrintDictWord(CurAdverb, Adverbs);
            debug_print("\" (%d)\n", CurAdverb);
        }
    }

    if (WordIndex < WordsInInput && CurPrep != 0) {
        if (TokenWords[WordIndex].Type == NOUN_TYPE) {
            if (noun != 0) {
                CurNoun2 = TokenWords[WordIndex++].Index;
            } else {
                noun = TokenWords[WordIndex++].Index;
            }
        }
    }

    if (WordIndex < WordsInInput && TokenWords[WordIndex].Type == PREPOSITION_TYPE) {
        CurPartp = TokenWords[WordIndex++].Index;
        if (WordIndex < WordsInInput && TokenWords[WordIndex].Type == NOUN_TYPE) {
            if (CurNoun2 != 0)
                debug_print("Found a third noun. This should never happen.\n");
            CurNoun2 = TokenWords[WordIndex++].Index;
        } else {
            CurPartp = CurPrep;
            CurPrep = 0;
        }
    }

    if (CurPartp != 0) {
        debug_print("Found participle \"");
        PrintDictWord(CurPartp, Prepositions);
        debug_print("\" (%d)\n", CurPartp);
    }

    if (CurNoun2 != 0) {
        debug_print("Found second noun \"");
        PrintDictWord(CurNoun2, Nouns);
        debug_print("\" (%d)\n", CurNoun2);
    } else if (CurPartp != 0) {
        debug_print("Found participle but no second noun. This should never happen.\n");
    }

    if (WordIndex <= initialindex)
        WordIndex++;

    debug_print("Index: %d Words in input: %d ", WordIndex, WordsInInput);

    if (WordIndex < WordsInInput)
        debug_print("There are %d unmatched words remaining. Word at index: %s\n", WordsInInput - WordIndex, InputWordStrings[WordIndex]);
    else
        debug_print("No remaining input words.\n");

    CurVerb = verb;
    CurNoun = noun;

    return 0;
}

static void LineInput(void)
{
    event_t ev;
    char buf[512];
    FreeInputWords();

    do {
        Display(Bottom, "\n%s, %s", Messages[ProtagonistString], sys[WHAT_NOW]);
        glk_request_line_event(Bottom, buf, (glui32)511, 0);

        while (1) {
            glk_select(&ev);

            if (ev.type == evtype_LineInput)
                break;
            else
                Updates(ev);
        }

        int length = ev.val1;
        buf[length] = 0;

        if (Transcript) {
            glk_put_string_stream(Transcript, buf);
            glk_put_string_stream(Transcript, "\n");
        }

        char *result = StripPunctuation(buf, &length);
        debug_print("After stripping punctuation: \"%s\"\n", result);
        result = ReplaceSynonyms(result, &length);

        debug_print("Final result: \"%s\"\n", result);

        InputWordStrings = SplitIntoWords(result, length);
        free(result);

        if (WordsInInput >= MAX_WORDS) {
            Output("Too many words!\n");
            FreeInputWords();
        }

        if (WordsInInput > 0 && InputWordStrings) {
            Output("\n");
            return;
        }

        WordsInInput = 0;
        Output(sys[HUH]);

    } while (WordsInInput == 0 || InputWordStrings == NULL);
}

static int RemainderContainsVerb(void) {
    debug_print("RemainderContainsVerb: WordIndex: %d WordsInInput: %d\n", WordIndex, WordsInInput);
    if (WordIndex < WordsInInput - 1) {
        for (int i = 0; i < WordsInInput; i++)
            if (TokenWords[i].Type == VERB_TYPE)
                return 1;
    }
    return 0;
}

void StopProcessingCommand(void) {
    if (RemainderContainsVerb()) {
        Output("\nThe rest of your input was ignored. ");
    }
    FreeInputWords();
    WordIndex = 0;
    SetBit(STOPTIMEBIT);
}

int GetInput(void)
{
    int result = 0;

    /* Is there input remaining to be analyzed? */
    if (WordIndex >= WordsInInput || WordsInInput < 2) {
        LineInput();

        if (WordsInInput == 0 || InputWordStrings == NULL)
            return 1;

        lastwasnewline = 1;

        if (!TokenizeInputWords()) {
            if (WordIndex < 255)
                Output("\nTry again please.\n");
            StopProcessingCommand();
            return 1;
        }

        result = CommandFromTokens(0, 0);
    } else {
        result = CommandFromTokens(CurVerb, 0);
    }

    return result;
}
