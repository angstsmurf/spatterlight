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

  long  *var_num;         /* [adv->n_variables] numeric value                  */
  char **var_text;        /* [adv->n_variables] text value (owned), or NULL    */

  char  *task_done;       /* [adv->n_tasks] completed flag                     */

  a5_prop_ov_t *ov;       /* property overrides set at runtime                 */
  int n_ov, cap_ov;

  int   game_over;        /* set by EndGame: 0 running, else 1                 */
  char *end_message;      /* Win/Lose/etc. (owned), or NULL                   */

  /* Per-turn reference bindings (e.g. "ReferencedObject2" -> "Table1"), set by
     the parser before restriction/action evaluation. */
  char  ref_name[16][32];
  char  ref_value[16][256];
  int   n_refbind;
} a5_state_t;

extern a5_state_t *a5state_new  (const a5_adventure_t *adv);
extern void        a5state_free (a5_state_t *st);

/* Index helpers (linear; -1 if absent). */
extern int a5state_object_index    (const a5_state_t *st, const char *key);
extern int a5state_character_index (const a5_state_t *st, const char *key);
extern int a5state_variable_index  (const a5_state_t *st, const char *key);
extern int a5state_task_index      (const a5_state_t *st, const char *key);

/* The player's current location key (NULL if unknown). */
extern const char *a5state_player_location (const a5_state_t *st);

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
 * Property access with the runtime override layer.  Returns the overridden
 * value if SetProperty has changed it, else the model's value, else NULL.
 * `kind` is informational only (overrides are keyed by entity key, which is
 * unique across the adventure).
 */
extern const char *a5state_entity_prop (const a5_state_t *st, const char *entkey,
                                        const char *propkey);
extern void a5state_set_prop (a5_state_t *st, const char *entkey,
                              const char *propkey, const char *value);

/* Per-turn reference bindings. */
extern void        a5state_clear_refs  (a5_state_t *st);
extern void        a5state_bind_ref    (a5_state_t *st, const char *name, const char *value);
extern const char *a5state_lookup_ref  (const a5_state_t *st, const char *name);

/* Variable lookup by Name (text engine) or Key (restrictions). */
extern int         a5state_var_num_by_name (const a5_state_t *st, const char *name, long *out);
extern const char *a5state_var_text_by_name (const a5_state_t *st, const char *name);

#endif
