/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- runtime state (Phase 2).
 *
 * A small mutable layer over the immutable a5model: the bits of world state the
 * text/display engine needs to read and that later phases will mutate -- object
 * and character locations, variable values, and per-task "completed" flags.
 *
 * For Phase 2 (the static world) the state is initialised straight from the
 * model's property values and is not yet driven by a turn loop; it is shaped so
 * Phases 3-4 can mutate it in place (move objects, set variables, complete
 * tasks) without the display code changing.
 *
 * Mirrors frankendrift's clsObject.Location / clsCharacter.Location property
 * decoding (clsObject.vb:521, clsObject.ExistsAtLocation) and clsVariable.
 */

#ifndef SCARIER_A5STATE_H
#define SCARIER_A5STATE_H

#include "a5model.h"

/* Upper bound on the number of items a single %objects% reference can resolve to
   (e.g. "take all" in a room of dozens of objects). */
#define A5_MAX_ITEMS 256

/* Where an object is.  key meaning depends on `where` (see comments). */
typedef enum {
  A5_OWHERE_NONE = 0,     /* unplaced / unknown                                */
  A5_OWHERE_HIDDEN,       /* "Hidden"/"Nowhere": not in any room               */
  A5_OWHERE_ALLROOMS,     /* "Everywhere": present in every location           */
  A5_OWHERE_LOCATION,     /* key = location key                                */
  A5_OWHERE_LOCGROUP,     /* key = location-group key                          */
  A5_OWHERE_ON_OBJECT,    /* key = object key (on its surface)                 */
  A5_OWHERE_IN_OBJECT,    /* key = object key (inside it)                      */
  A5_OWHERE_HELD_BY,      /* key = character key                               */
  A5_OWHERE_WORN_BY,      /* key = character key                               */
  A5_OWHERE_PART_OBJECT,  /* key = object key                                  */
  A5_OWHERE_PART_CHAR     /* key = character key                               */
} a5_owhere_t;

typedef struct a5_objloc_s {
  a5_owhere_t where;
  const char *key;        /* aliases into the model/DOM; not owned             */
  int is_static;
} a5_objloc_t;

/* One SetLook stack entry (clsEvent.clsLookText): rendered look text gated on a
   location/group key. */
typedef struct a5_looktext_s {
  char *loc_key;          /* OnlyApplyAt gate (owned)                          */
  char *text;             /* rendered look text (owned)                        */
} a5_looktext_t;

/* A runtime property override (set by SetProperty actions in Phase 3). */
typedef struct a5_prop_ov_s {
  char *entity;           /* entity key (object/character/...)                 */
  char *prop;             /* property key                                      */
  char *value;            /* new value (owned)                                 */
} a5_prop_ov_t;

typedef struct a5_state_s {
  const a5_adventure_t *adv;

  a5_objloc_t *obj;       /* [adv->n_objects], parallel to adv->objects        */
  const char **char_loc;  /* [adv->n_characters] location key, or NULL         */
  char **char_position;   /* [adv->n_characters] Standing/Sitting/Lying, owned */
  const char **char_onobj;/* [adv->n_characters] object key the char is on/in,
                             or NULL when "at location" (ExistWhere==AtLocation) */
  char *char_in;          /* [adv->n_characters] when char_onobj set: 1=inside,
                             0=on the surface (ExistWhere InObject vs OnObject)  */

  long  *var_num;         /* [adv->n_variables] numeric value                  */
  char **var_text;        /* [adv->n_variables] text value (owned), or NULL    */

  char  *task_done;       /* [adv->n_tasks] completed flag                     */

  a5_prop_ov_t *ov;       /* property overrides set at runtime                 */
  int n_ov, cap_ov;

  int   game_over;        /* set by EndGame: 0 running, else 1                 */
  char *end_message;      /* EndGame enum "Win"/"Lose"/"Neutral" (owned), NULL */
  int   end_displayed;    /* the win/lose/score/restart block has been emitted */
  int   turns;            /* commands processed (clsAdventure.Turns); the score
                             line shows the count BEFORE the ending command     */

  /* Conversation state (clsAdventure.sConversationCharKey / sConversationNode):
     the NPC the player is currently talking to and the current topic node.
     Both owned; "" when not in a conversation. */
  char *conv_char;
  char *conv_node;

  /* Player "seen" state (clsCharacter.HasSeenCharacter, player-centric): set at
     end of turn for every character the player can see, so HaveSeenCharacter
     and the conversation "characters must be seen" gates work.  [n_characters] */
  char *char_seen;

  /* Player "seen" state for objects (clsCharacter.HasSeenObject): set each turn
     for every object the player can currently see, so a later out-of-scope
     reference can tell "never seen" (HaveBeenSeenByCharacter fails -> "You see
     no such thing.") from "seen but not here now" ("You can't see the X.").
     [n_objects] */
  char *obj_seen;

  /* Player "seen" state for locations (clsCharacter.HasSeenLocation): FD sets
     the flag inside clsCharacter.Move for every location a character moves to,
     plus the player's start location at session init (clsUserSession.vb:222).
     Player-centric like obj_seen/char_seen; Location HaveBeenSeenByCharacter
     restrictions read it.  [n_locations] */
  char *loc_seen;

  /* Last-referenced pronoun targets (clsUserSession.sIt/sThem/sHim/sHer), the
     full display name of the object/character most recently named by the player
     in each pronoun class.  GrabIt recomputes these each turn from the (already
     it->name substituted) input; the next turn's "it"/"them"/"him"/"her" is
     textually replaced by the stored name before parsing.  All owned; ""
     until something has been referenced.  Save/restored. */
  char *s_it;
  char *s_them;
  char *s_him;
  char *s_her;

  /* Transient character context for char-scoped text functions (%CharacterName%
     etc.).  v5 rewrites a character's own text "%CharacterName%" ->
     "%CharacterName[Key]%" at load (FileIO SearchAndReplace); we instead set the
     context key while rendering that character's CharHereDesc / topic reply, so
     a bare %CharacterName% resolves to it.  NULL = default (Player). */
  const char *ctx_char;

  /* Per-turn reference bindings (e.g. "ReferencedObject2" -> "Table1"), set by
     the parser before restriction/action evaluation. */
  char  ref_name[16][32];
  char  ref_value[16][256];
  int   n_refbind;

  /* Multiple-object reference items (the %objects% grammar: "all", "all
     <plural>", "X and Y", comma lists, "... except/but/apart from ..." and
     plural nouns -- clsUserSession.InputMatchesObjects).  When a %objects%/
     %characters% reference resolves to a set, these hold the chosen item keys
     (each aliasing the model, in command order).  ReferencedObject is bound to
     the first; ReferencedObjects is bound to the "key1|key2|..." pipe list so
     the OO/text engine renders the whole set.  The per-item action loop
     (run_action ReferencedObjects) and the bare %objects% list renderer read
     these; n_ref_items == 0 means the ordinary single-object path. */
  const char *ref_items[A5_MAX_ITEMS];
  int   n_ref_items;
  char  ref_items_type;        /* 'o' object / 'c' character                  */

  /* The first object/character slot (%object%==%object1% / %character%) was
     filled by a *plural* %objects%/%characters% reference (FD ReferenceMatch
     "objects"/"characters", not "object1").  The key stays bound for override-key
     matching and the ReferencedObjects/ReferencedCharacters restriction paths,
     but the singular %object%/%object1% (resp. %character%) text token must
     render EMPTY (GetReference returns Nothing unless ReferenceMatch="object1",
     clsUserSession.vb:3990) -- e.g. a give task's "%TheObject[%object%]%" prints
     "nothing".  Set by bind_reference, reset by a5state_clear_refs. */
  int   ref_object1_plural;
  int   ref_character1_plural;

  /* The matched command carries BOTH a genuine plural %objects% reference and a
     separate singular %object% reference (e.g. `hide %objects% in %object%`).
     FD's GetReference (clsUserSession.vb:3990) resolves ReferencedObject only
     to the reference whose ReferenceMatch is "object1" -- never to the plural
     -- so the per-item plural binds (resolve_plural's restriction probes,
     run_general's item loop) must NOT clobber the singular alias: a restriction
     like `ReferencedObject Must HaveProperty ...` keeps testing the container,
     not the item being iterated (Dwarf of Direwood's `hide X, Y and Z in
     beard`).  Set by resolve_refine when it defers the plural, reset by
     a5state_clear_refs; read by bind_reference. */
  int   ref_objects_suppress_singular;

  /* SetLook event sub-event "look stack" (clsEvent.stackLookText): each SetLook
     pushes a (location/group gate, rendered text) entry; a5text_view_location
     appends the most-recent entry whose gate matches the player's location.
     Unused by the shipped corpus, but ported for faithfulness.  Owned. */
  a5_looktext_t *looks;
  int n_looks, cap_looks;

  /* <DisplayOnce> description segments that have already been shown (keyed by
     the segment's DOM node).  `marking_display` is set while rendering real
     output (vs a peek/test render) so a segment is only retired once it has
     actually reached the player -- mirrors clsDescription.ToString's
     Displayed flag gated on UserSession.bTestingOutput. */
  const void **disp_once;
  int n_disp_once, cap_disp_once;
  int marking_display;

  /* Set by a HaveRouteInDirection evaluation (a5restr pass_character) to the
     blocked exit's *own* restriction <Message> when the exit exists but is
     restriction-gated -- frankendrift's sRouteError, which overrides the
     movement restriction's generic "There is no route..." text.  NULL when the
     exit is open or simply absent.  Not owned (a DOM node). */
  const a5_xml_node_t *route_error;

  /* frankendrift's sRestrictionText, as a (non-owned) <Message> DOM node.  Every
     PassRestrictions call updates it: a restriction that fails sets it to that
     restriction's Message (NULL when the restriction has none), a passing one on
     the deciding path clears it, and a call that evaluates *no* single
     restriction (a malformed BracketSequence, whose EvaluateRestrictionBlock
     returns False without calling PassSingleRestriction) leaves it untouched.
     Crucially it is NOT reset between commands, so a command-matching task whose
     restrictions never overwrite it (the malformed-bracket case) inherits the
     previous command's leftover -- e.g. Anno 1700's reference-free OpeningHid
     ("##A#"), which thereby fails *with output* and ticks the turn.  NULL == "". */
  const a5_xml_node_t *restriction_text;
} a5_state_t;

extern a5_state_t *a5state_new  (const a5_adventure_t *adv);
extern void        a5state_free (a5_state_t *st);

/* Index helpers (linear; -1 if absent). */
extern int a5state_object_index    (const a5_state_t *st, const char *key);
extern int a5state_character_index (const a5_state_t *st, const char *key);
extern int a5state_variable_index  (const a5_state_t *st, const char *key);
extern int a5state_task_index      (const a5_state_t *st, const char *key);
extern int a5state_location_index  (const a5_state_t *st, const char *key);

/* Mark a location as seen by the player (clsCharacter.HasSeenLocation = True,
   set on every player move and for the start location).  NULL/unknown keys are
   ignored. */
extern void a5state_mark_loc_seen (a5_state_t *st, const char *lockey);

/* The player's current location key (NULL if unknown). */
extern const char *a5state_player_location (const a5_state_t *st);

/* clsCharacter.IsInGroupOrLocation: is character `charkey` at location `key`, or
   at a location that is a member of group `key`?  charkey NULL => the Player. */
extern int a5state_in_group_or_location (const a5_state_t *st,
                                         const char *charkey, const char *key);

/* SetLook look-text stack (clsEvent): push a rendered look entry; fetch the
   most-recent one whose location/group gate matches the player (or NULL). */
extern void        a5state_push_look (a5_state_t *st, const char *loc_key,
                                      const char *text);
extern const char *a5state_player_look (const a5_state_t *st);

/*
 * Does object `oi` exist at location `lockey`?  When `directly` is set, only a
 * direct placement counts (an object on/in another object, or held by a
 * character, is not "directly" in the room).  Mirrors ExistsAtLocation.
 */
extern int a5state_object_at_location     (const a5_state_t *st, int oi,
                                           const char *lockey, int directly);
extern int a5state_object_key_at_location (const a5_state_t *st, const char *objkey,
                                           const char *lockey, int directly);

/*
 * Is object `oi` *visible* at location `lockey`?  Like a5state_object_at_location
 * but mirrors clsObject.BoundVisible / IsVisibleAtLocation: an object inside a
 * closed opaque openable container resolves to the container key (not the room),
 * so it is hidden.  Used for scope/visibility and end-of-turn seen-tracking,
 * where FD uses CanSeeObject/IsVisibleTo rather than the raw ExistsAtLocation.
 */
extern int a5state_object_visible_at_location (const a5_state_t *st, int oi,
                                           const char *lockey, int directly);

/*
 * Is character `ci` visible at location `lockey`?  Mirrors
 * clsCharacter.IsVisibleAtLocation (via BoundVisible): a character "At Location"
 * matches that location; one "On Object"/"In Object" matches wherever its
 * carrier object exists (resolved through the container chain).  Used by the
 * location renderer's "characters present" list, which includes characters
 * seated on / inside furniture in the room.
 */
extern int a5state_character_at_location (const a5_state_t *st, int ci,
                                          const char *lockey);

/*
 * Property access with the runtime override layer.  Returns the overridden
 * value if SetProperty has changed it, else the model's value, else NULL.
 * `kind` is informational only (overrides are keyed by entity key, which is
 * unique across the adventure).
 */
extern const char *a5state_entity_prop (const a5_state_t *st, const char *entkey,
                                        const char *propkey);
extern void a5state_set_prop (a5_state_t *st, const char *entkey,
                              const char *propkey, const char *value);

/* Runtime object-group membership (AddObjectToGroup/RemoveObjectFromGroup):
   effective membership = runtime override else the model <Member> list. */
extern int  a5state_object_in_group (const a5_state_t *st, const char *grpkey,
                                     const char *objkey);
extern void a5state_set_object_in_group (a5_state_t *st, const char *grpkey,
                                         const char *objkey, int present);

/* A location's inherited Locations-group property (e.g. the dynamic
   ShortLocationDescription darkness property), or NULL. */
extern const a5_prop_t *a5state_location_group_prop (const a5_state_t *st,
                                  const char *lockey, const char *propkey);

/* Conversation state setters (own the string; "" clears). */
extern void a5state_set_conv_char (a5_state_t *st, const char *key);
extern void a5state_set_conv_node (a5_state_t *st, const char *key);

/* <DisplayOnce> tracking: has this description-segment node already been shown,
   and (when marking) record that it has. */
extern int  a5state_disp_once_seen (const a5_state_t *st, const void *node);
extern void a5state_disp_once_mark (a5_state_t *st, const void *node);

/* Per-turn reference bindings. */
extern void        a5state_clear_refs  (a5_state_t *st);
extern void        a5state_bind_ref    (a5_state_t *st, const char *name, const char *value);
extern const char *a5state_lookup_ref  (const a5_state_t *st, const char *name);

/* Variable lookup by Name (text engine) or Key (restrictions). */
extern int         a5state_var_num_by_name (const a5_state_t *st, const char *name, long *out);
extern const char *a5state_var_text_by_name (const a5_state_t *st, const char *name);

#endif
