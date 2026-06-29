/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- in-memory object model.
 *
 * A typed index over the parsed XML DOM (a5xml).  Each record carries the hot,
 * cross-cutting fields most code needs (keys, names, types, property maps) plus
 * a pointer back into the DOM for the structure that later phases interpret
 * (descriptions, restrictions, actions, walks, topics, sub-events).  The model
 * owns the underlying a5_xml_doc, so all the const char * fields alias into it
 * and stay valid until a5model_free().
 *
 * Mirrors frankendrift's clsAdventure collections (FileIO.Load500).
 */

#ifndef SCARIER_A5MODEL_H
#define SCARIER_A5MODEL_H

#include <stdint.h>

#include "a5xml.h"

/*
 * One ADRIFT property value on an entity.  ADRIFT is property-driven: most world
 * state lives here.  A property is either a flag (just a Key -> value/value_node
 * both NULL), a simple scalar (value set), or a rich text value (value_node set
 * to the <Value> description block).
 */
typedef struct a5_prop_s {
  const char *key;
  const char *value;                  /* simple scalar value, or NULL        */
  const a5_xml_node_t *value_node;    /* rich <Value> block, or NULL         */
} a5_prop_t;

typedef struct a5_object_s {
  const char *key;
  const char *article;
  const char *prefix;
  const char **names;       int n_names;        /* aliases (<Name> list)      */
  a5_prop_t *props;         int n_props;
  const a5_xml_node_t *node;
} a5_object_t;

typedef struct a5_location_s {
  const char *key;
  a5_prop_t *props;         int n_props;
  const a5_xml_node_t *node;        /* ShortDescription/LongDescription/Movement here */
} a5_location_t;

/* One conversation node of a character (clsTopic): a reply gated by keywords or
   a command pattern, with its own restrictions and actions.  Greet/Farewell
   topics carry no keyword; Ask/Tell match on comma-separated keywords; Command
   matches the player's subject against a command pattern. */
typedef struct a5_topic_s {
  const char *key;
  const char *parent_key;           /* <ParentKey> ("" = top-level node)     */
  const char *keywords;             /* <Keywords> (comma list or pattern)    */
  int is_intro, is_ask, is_tell, is_command, is_farewell, stay_in_node;
  const a5_xml_node_t *conversation; /* the reply <Description> block         */
  const a5_xml_node_t *restrictions; /* <Restrictions>, or NULL              */
  const a5_xml_node_t *actions;      /* <Actions>, or NULL                   */
} a5_topic_t;

/* One <Control> "Start|Stop|Suspend|Resume Completion|UnCompletion <TaskKey>"
   (EventOrWalkControl): trigger an event or walk on a task (un)completing.
   Shared by events and walks, so it is defined before both. */
typedef enum {
  A5_CTRL_START   = 0,
  A5_CTRL_STOP    = 1,
  A5_CTRL_SUSPEND = 2,
  A5_CTRL_RESUME  = 3
} a5_ctrl_t;
typedef struct a5_eventctrl_s {
  a5_ctrl_t control;
  int on_completion;                /* 1 = Completion, 0 = UnCompletion      */
  const char *task_key;
} a5_eventctrl_t;

/* One <Step> of a walk (clsWalk.clsStep): a destination key (location / group /
   character / "%Player%") reached after a turn duration (random range). */
typedef struct a5_walkstep_s {
  const char *location;             /* destination key                       */
  long ft_from, ft_to;              /* duration in turns (random range)      */
} a5_walkstep_t;

/* One <Activity> of a walk (clsWalk.SubWalk): like an event SubEvent, but with
   the extra ComesAcross trigger and an OnlyApplyAt (sKey3) display gate. */
typedef enum {
  A5_SW_FROM_LAST    = 0,           /* FromLastSubWalk                       */
  A5_SW_FROM_START   = 1,           /* FromStartOfWalk                       */
  A5_SW_BEFORE_END   = 2,           /* BeforeEndOfWalk                       */
  A5_SW_COMES_ACROSS = 3            /* ComesAcross <char> (meets the char)   */
} a5_sw_when_t;
typedef enum {
  A5_SW_DISPLAY   = 0,              /* DisplayMessage (oDescription)         */
  A5_SW_EXECTASK  = 1,              /* ExecuteTask sKey2                     */
  A5_SW_UNSETTASK = 2               /* UnsetTask sKey2                       */
} a5_sw_what_t;
typedef struct a5_subwalk_s {
  a5_sw_when_t when;
  long ft_from, ft_to;              /* turn offset (random range)            */
  const char *come_key;             /* ComesAcross subject (%Player%/key)    */
  a5_sw_what_t what;
  const char *task_key;             /* sKey2 (ExecuteTask/UnsetTask)         */
  const char *only_apply_at;        /* sKey3 (OnlyApplyAt loc/group gate)    */
  const a5_xml_node_t *description; /* <Action> DisplayMessage block         */
} a5_subwalk_t;

/* One character walk (clsWalk): a turn-based state machine that moves the
   character along its steps, runs sub-walk activities, and is started/stopped by
   WalkControls on task (un)completion. */
typedef struct a5_walk_s {
  const char *char_key;             /* owning character key                  */
  const char *description;
  int loops;                        /* <Loops>                               */
  int start_active;                 /* <StartActive>                         */
  a5_walkstep_t  *steps;     int n_steps;
  a5_subwalk_t   *subwalks;  int n_subwalks;
  a5_eventctrl_t *controls;  int n_controls;
} a5_walk_t;

typedef struct a5_character_s {
  const char *key;
  const char *name;
  const char *article;
  const char *prefix;
  const char *type;                 /* Player / NonPlayer                    */
  const char *perspective;          /* FirstPerson / SecondPerson / ...      */
  const char **descriptors; int n_descriptors;
  a5_prop_t *props;         int n_props;
  a5_topic_t *topics;       int n_topics;
  a5_walk_t  *walks;        int n_walks;
  const a5_xml_node_t *node;
} a5_character_t;

/* One <Specific> constraint of a Specific (override) task: a reference index of
   a given type, optionally restricted to one or more keys ("" = match any). */
typedef struct a5_specific_s {
  const char *type;                 /* Object / Character / Location / ...    */
  const char **keys;        int n_keys;   /* allowed keys ("" entry = any)    */
} a5_specific_t;

typedef struct a5_task_s {
  const char *key;
  long priority;
  const char *type;                 /* General / Specific / System           */
  const char *location_trigger;     /* <LocationTrigger>: a System task armed
                                       when the Player moves into this location
                                       (clsCharacter.Move), or NULL            */
  const char **commands;    int n_commands;
  int repeatable;
  int continue_lower;               /* <Continue>ContinueAlways: keep running
                                       lower-priority matching tasks after this */
  const a5_xml_node_t *restrictions; /* <Restrictions> node, or NULL         */
  const a5_xml_node_t *actions;      /* <Actions> node, or NULL              */
  const a5_xml_node_t *fail_override;/* <FailOverride> Description, or NULL:
                                        shown for a "get all"-style command when
                                        no item passed (clsTask.FailOverride,
                                        clsUserSession.vb:788) -- "There is
                                        nothing worth taking here." */
  const a5_xml_node_t *node;

  /* Specific-override task linkage (clsTask Specifics / GeneralKey). */
  const char *general_key;          /* <GeneralTask> parent key, or NULL     */
  const char *override_type;        /* <SpecificOverrideType>, or NULL       */
  a5_specific_t *specifics; int n_specifics;
} a5_task_t;

typedef struct a5_variable_s {
  const char *key;
  const char *name;
  const char *type;                 /* Numeric / Text                        */
  const char *initial;              /* InitialValue text                     */
  const a5_xml_node_t *node;
} a5_variable_t;

/* One <SubEvent>: at `when` (an offset measured in turns, a random range
   [ft_from, ft_to]) do `what` to `key` (clsEvent.SubEvent). */
typedef enum {
  A5_SE_FROM_LAST  = 0,             /* FromLastSubEvent                      */
  A5_SE_FROM_START = 1,             /* FromStartOfEvent                      */
  A5_SE_BEFORE_END = 2              /* BeforeEndOfEvent                      */
} a5_se_when_t;
typedef enum {
  A5_SE_DISPLAY   = 0,              /* DisplayMessage (oDescription)         */
  A5_SE_SETLOOK   = 1,              /* SetLook                               */
  A5_SE_EXECTASK  = 2,              /* ExecuteTask sKey                      */
  A5_SE_UNSETTASK = 3               /* UnsetTask sKey                        */
} a5_se_what_t;
typedef struct a5_subevent_s {
  a5_se_when_t when;
  long ft_from, ft_to;              /* turn offset (random range)            */
  a5_se_what_t what;
  const char *key;                  /* task/location key (ExecuteTask/...)   */
  const a5_xml_node_t *description; /* <Action> Description (DisplayMessage) */
} a5_subevent_t;

typedef struct a5_event_s {
  const char *key;
  const char *type;                 /* TurnBased / TimeBased                 */
  int when_start;                   /* 1 Immediately/2 BetweenXY/3 AfterATask */
  long start_from, start_to;        /* StartDelay random range               */
  long length_from, length_to;      /* Length random range                   */
  int repeating;
  int repeat_countdown;
  a5_subevent_t  *subevents; int n_subevents;
  a5_eventctrl_t *controls;  int n_controls;
  const a5_xml_node_t *node;
} a5_event_t;

typedef struct a5_group_s {
  const char *key;
  const char *type;                 /* Locations / Objects / Characters      */
  const char *name;
  const char **members;     int n_members;
  a5_prop_t *props;         int n_props;  /* group <Property> list; for Objects
                                             groups these are inherited by member
                                             objects (clsItem htblInheritedProperties) */
  const a5_xml_node_t *node;
} a5_group_t;

typedef struct a5_propdef_s {
  const char *key;
  const char *type;                 /* Boolean / Integer / Text / ...        */
  const char *property_of;          /* Objects / Characters / Locations      */
  const char *dependent_key;
  const a5_xml_node_t *node;
} a5_propdef_t;

typedef struct a5_alr_s {
  const char *key;
  const char *old_text;             /* OldText                               */
  const a5_xml_node_t *new_text;    /* NewText description block              */
  const a5_xml_node_t *node;
} a5_alr_t;

typedef struct a5_udf_s {
  const char *key;
  const char *name;
  const a5_xml_node_t *node;
} a5_udf_t;

typedef struct a5_adventure_s {
  a5_xml_doc_t *doc;                /* owned                                 */
  const a5_xml_node_t *root;
  const a5_xml_node_t *introduction;
  const a5_xml_node_t *end_game_text;   /* <EndGameText> (clsAdventure.WinningText),
                                           shown after the win/lose line, or NULL */

  const char *title;
  const char *author;
  const char *version;
  int show_first_location;          /* <ShowFirstLocation> (default 1): show
                                       the start room after the intro          */
  int show_exits;                   /* <ShowExits> (default 1): append the
                                       "Exits are .../An exit leads ..." listing
                                       to each location view (clsAdventure.
                                       bShowExits default True)                 */
  int wait_turns;                   /* <WaitTurns> (default 3): turns a single
                                       "wait"/"z" advances                      */
  int hp_passing;                   /* <TaskExecution> == HighestPriorityPassingTask:
                                       a failing-with-output task does NOT claim
                                       the turn; the scan keeps looking for a
                                       passing task (clsAdventure.TaskExecution,
                                       default HighestPriorityTask => 0)        */
  const char *dir_re[12];           /* <DirectionNorth>..<DirectionDown>:
                                       localized direction synonym specs, indexed
                                       by DirectionsEnum (North=0, East=1,
                                       South=2, West=3, Up=4, Down=5, In=6,
                                       Out=7, NorthEast=8, SouthEast=9,
                                       SouthWest=10, NorthWest=11); NULL when the
                                       game omits the field (=> English default).
                                       Slash-separated synonyms, first = display
                                       name (clsAdventure.sDirectionsRE /
                                       Global.DirectionName)                     */

  a5_object_t    *objects;    int n_objects;
  a5_location_t  *locations;  int n_locations;
  a5_character_t *characters; int n_characters;
  a5_task_t      *tasks;      int n_tasks;
  a5_variable_t  *variables;  int n_variables;
  a5_event_t     *events;     int n_events;
  a5_group_t     *groups;     int n_groups;
  a5_propdef_t   *propdefs;   int n_propdefs;
  a5_alr_t       *alrs;       int n_alrs;
  a5_udf_t       *udfs;       int n_udfs;
} a5_adventure_t;

/* Build the model from an already-parsed doc (takes ownership of doc). */
extern a5_adventure_t *a5model_from_doc (a5_xml_doc_t *doc);

/* Full pipeline: read a Blorb/.taf file, deobfuscate, inflate, parse, model. */
extern a5_adventure_t *a5model_load (const char *path);

extern void a5model_free (a5_adventure_t *adv);

/* Property lookup within a record's property array. */
extern const a5_prop_t *a5_prop_find (const a5_prop_t *props, int n,
                                      const char *key);

/* Per-type key lookups (linear; adequate for load + Phase 1). */
extern const a5_object_t    *a5model_object    (const a5_adventure_t *a, const char *key);
extern const a5_location_t  *a5model_location  (const a5_adventure_t *a, const char *key);
extern const a5_character_t *a5model_character (const a5_adventure_t *a, const char *key);
extern const a5_task_t      *a5model_task      (const a5_adventure_t *a, const char *key);
extern const a5_variable_t  *a5model_variable  (const a5_adventure_t *a, const char *key);

#endif
