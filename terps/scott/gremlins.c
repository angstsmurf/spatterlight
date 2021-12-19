//
//  gremlins.c
//  scott
//
//  Created by Administrator on 2022-01-10.
//

#include "gremlins.h"

#include "scott.h"
#include "sagadraw.h"
#include "parser.h"


extern Room *Rooms;
extern Item *Items;

extern int AnimationFlag;

extern int Counters[16];
extern const char **Messages;

#define GREMLINS_ANIMATION_RATE 670

void update_gremlins_animations(void) {
    if (Rooms[MyLoc].Image == 255) {
        glk_request_timer_events(0);
        return;
    }
    OpenGraphicsWindow();
    if (Graphics == NULL) {
        glk_request_timer_events(0);
        return;
    }

    int timer_delay = GREMLINS_ANIMATION_RATE;
    switch(MyLoc) {
        case 1: /* Bedroom */
            if (Items[50].Location == 1)  /* Gremlin throwing darts */
            {
                if (AnimationFlag)
                {
                    DrawImage(60); /* Gremlin throwing dart frame 1 */
                } else {
                    DrawImage(59); /* Gremlin throwing dart frame 2 */
                }
            }
            break;
        case 17: /* Dotty's Tavern */
            if (Items[82].Location == 17)  /* Gang of GREMLINS */
            {
                if (AnimationFlag)
                {
                    DrawImage(49); /* Gremlin hanging from curtains frame 1 */
                    DrawImage(51); /* Gremlin ear frame 1 */
                    DrawImage(54); /* Gremlin's mouth frame 1 */
                } else {
                    DrawImage(50); /* Gremlin hanging from curtains frame 2 */
                    DrawImage(52); /* Gremlin ear frame 2 */
                    DrawImage(53); /* Gremlin's mouth frame 2 */
                }
            }
            break;
        case 16: /* Behind a Bar */
            if (Items[82].Location == 16)  /* Gang of GREMLINS */
            {
                if (AnimationFlag)
                {
                    DrawImage(57); /* Flasher gremlin frame 1 */
                    DrawImage(24); /* Gremlin tongue frame 1 */
                    if (CurrentGame == GREMLINS_GERMAN)
                        DrawImage(46); /* Gremlin ear frame 1 */
                    else
                        DrawImage(73); /* Gremlin ear frame 1 */
                } else {
                    DrawImage(58); /* Flasher gremlin frame 2 */

                    if (CurrentGame == GREMLINS_GERMAN) {
                        DrawImage(33); /* Gremlin tongue frame 2 */
                        DrawImage(23); /* Gremlin ear frame 2 */
                    } else {
                        DrawImage(72); /* Gremlin tongue frame 2 */
                        if (CurrentGame == GREMLINS_SPANISH)
                            DrawImage(23); /* Gremlin ear frame 2 */
                        else
                            DrawImage(74); /* Gremlin ear frame 2 */
                    }
                }
            }
            break;
        case 19: /* Square */
            if (Items[82].Location == 19)  /* Gang of GREMLINS */
            {
                if (AnimationFlag)
                {
                    DrawImage(55); /* Silhouette of Gremlins frame 1 */
                } else {
                    DrawImage(71); /* Silhouette of Gremlins frame 1 */
                }
            }
            break;
        case 6: /* on a road */
            if (Items[82].Location == 6)  /* Gang of GREMLINS */
            {
                if (AnimationFlag)
                {
                    if (GameInfo->subtype == LOCALIZED) {
                        DrawImage(25); /* Silhouette 2 of Gremlins  */
                    } else {
                        DrawImage(75); /* Silhouette 2 of Gremlins  */
                    }
                } else {
                    DrawImage(48); /* Silhouette 2 of Gremlins flipped */
                }
            }
            break;
        case 3:  /* Kitchen */
            if (Counters[2] == 2)  /* Blender is on */
            {
                if (AnimationFlag)
                {
                    DrawImage(56); /* Blended Gremlin */
                } else {
                    DrawImage(12); /* Blended Gremlin flipped */
                }
            }
            break;
        default:
            timer_delay = 0;
            break;
    }
    AnimationFlag = (AnimationFlag == 0);
    glk_request_timer_events(timer_delay);
}

void gremlins_look(void) {
    if (Rooms[MyLoc].Image != 255) {
        if (MyLoc == 17 && Items[82].Location == 17)
            DrawImage(45); /* Bar full of Gremlins */
        else
            DrawImage(Rooms[MyLoc].Image);
        AnimationFlag = 0;
        update_gremlins_animations();
    }
    // Ladder image at the top of the department store
    if (MyLoc == 34 && Items[53].Location == MyLoc) {
        DrawImage(42);
    }
}

void LoadExtraGremlinsData(void)
{
    for (int i = I_DONT_UNDERSTAND; i <= THATS_BEYOND_MY_POWER; i++)
        sys[i] = system_messages[6 - I_DONT_UNDERSTAND + i];

    for (int i = YOU_ARE; i <= HIT_ENTER; i++)
        sys[i] = system_messages[17 - YOU_ARE + i];

    sys[DROPPED] = system_messages[2];
    sys[TAKEN] = system_messages[3];
    sys[OK] = system_messages[4];
    sys[PLAY_AGAIN] = system_messages[5];
    sys[YOURE_CARRYING_TOO_MUCH] = system_messages[24];
    sys[IM_DEAD] = system_messages[25];

    for (int i = 0; i < MAX_SYSMESS; i++)
        fprintf(stderr, "\"%s\",\n", sys[i]);
}

extern const char **Verbs;
extern const char **Nouns;

void LoadExtraGermanGremlinsData(void)
{
    Verbs[0] = "AUTO\0";
    Nouns[0] = "ANY\0";

    sys[I_DONT_KNOW_WHAT_A] = system_messages[15];
    sys[IS] = system_messages[16];
    sys[I_DONT_KNOW_HOW_TO] = "Ich weiss nicht, wie man etwas \"";
    sys[SOMETHING] = "\" macht. ";
    sys[WHAT] = system_messages[8];
    sys[YOU_ARE] = "Ich bin ";
    sys[YES] = "Ja";
    sys[NO] = "Nein";
    sys[ANSWER_YES_OR_NO] = "Antworte Ja oder Nein.\n";
    sys[I_DONT_UNDERSTAND] = "Ich verstehe nicht. ";
    sys[ARE_YOU_SURE] = "Sind Sie sicher? ";
    sys[NOTHING_HERE_TO_TAKE] = "Hier gibt es nichts zu nehmen. ";
    sys[YOU_HAVE_NOTHING] = "Ich traege nichts. ";
    sys[MOVE_UNDONE] = "Verschieben rueckgaengig gemacht. ";
    sys[SAVED] = "Spiel gespeichert. ";
    sys[CANT_USE_ALL] ="Sie koennen ALLES nicht mit diesem Verb verwenden. ";
    sys[TRANSCRIPT_ON] = "Das Transkript ist jetzt eingeschaltet. ";
    sys[TRANSCRIPT_OFF] = "Das Transkript ist jetzt deaktiviert. ";
    sys[NO_TRANSCRIPT] = "Es wird kein Transkript ausgefuehrt. ";
    sys[TRANSCRIPT_ALREADY] = "Eine Transkript laeuft bereits. ";
    sys[FAILED_TRANSCRIPT] = "Transkriptdatei konnte nicht erstellt werden. ";
    sys[TRANSCRIPT_START] = "Beginn einer Transkript.\n\n";
    sys[TRANSCRIPT_END] = "\n\nEnde eniner Transkript.\n";

    Messages[90] = "Ehe ich etwas anderes mache, much aich erst alles andere fallenlassen. ";

    for (int i = 0; i < NUMBER_OF_DIRECTIONS; i++)
        Directions[i] = GermanDirections[i];
    for (int i = 0; i < NUMBER_OF_SKIPPABLE_WORDS; i++)
        SkipList[i] = GermanSkipList[i];
    for (int i = 0; i < NUMBER_OF_DELIMITERS; i++)
        DelimiterList[i] = GermanDelimiterList[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_NOUNS; i++)
        ExtraNouns[i] = GermanExtraNouns[i];
}

void LoadExtraSpanishGremlinsData(void)
{
    Verbs[0] = "AUTO\0";
    Nouns[0] = "ANY\0";

    for (int i = YOU_ARE; i <= HIT_ENTER; i++)
        sys[i] = system_messages[15 - YOU_ARE + i];
    for (int i = I_DONT_UNDERSTAND; i <= THATS_BEYOND_MY_POWER; i++)
        sys[i] = system_messages[6 - I_DONT_UNDERSTAND + i];

    for (int i = DROPPED; i <= OK; i++)
        sys[i] = system_messages[2 - DROPPED + i];
    sys[PLAY_AGAIN] = system_messages[5];
    sys[YOURE_CARRYING_TOO_MUCH] = system_messages[22];
    sys[IM_DEAD] = system_messages[23];
    sys[YOU_CANT_GO_THAT_WAY] = system_messages[14];
    sys[WHAT] = sys[HUH];
    sys[YES] = "s}";
    sys[NO] = "no";
    sys[ANSWER_YES_OR_NO] = "Contesta s} o no.\n";
    sys[I_DONT_KNOW_WHAT_A] = "No s\x84 qu\x84 es un \"";
    sys[IS] = "\". ";
    sys[I_DONT_KNOW_HOW_TO] = "No s\x84 c|mo \"";
    sys[SOMETHING] = "\" algo. ";

    sys[ARE_YOU_SURE] = "\x83\x45stas segura? ";
    sys[NOTHING_HERE_TO_TAKE] = "No hay nada aqu} para tomar. ";
    sys[YOU_HAVE_NOTHING] = "No llevo nada. ";
    sys[MOVE_UNDONE] = "Deshacer. ";
    sys[SAVED] = "Juego guardado. ";
    sys[CANT_USE_ALL] ="No puedes usar TODO con este verbo. ";
    sys[TRANSCRIPT_ON] = "Transcripci|n en. ";
    sys[TRANSCRIPT_OFF] = "Transcripci|n desactivada. ";
    sys[NO_TRANSCRIPT] = "No se est{ ejecutando ninguna transcripci|n. ";
    sys[TRANSCRIPT_ALREADY] = "Ya se est{ ejecutando una transcripci|n. ";
    sys[FAILED_TRANSCRIPT] = "No se pudo crear el archivo de transcripci|n. ";
    sys[TRANSCRIPT_START] = "Comienzo de una transcripci|n.\n\n";
    sys[TRANSCRIPT_END] = "\n\nFin de una transcripci|n.\n";

    for (int i = 0; i < NUMBER_OF_DIRECTIONS; i++)
        Directions[i] = SpanishDirections[i];
    for (int i = 0; i < NUMBER_OF_EXTRA_NOUNS; i++)
        ExtraNouns[i] = SpanishExtraNouns[i];
}

void gremlins_action(int parameter) {
    DrawImage(68); /* Mogwai */
    Display(Bottom, "\n%s\n", sys[HIT_ENTER]);
    HitEnter();
    Look();
}

