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

typedef struct a5_character_s {
  const char *key;
  const char *name;
  const char *article;
  const char *prefix;
  const char *type;                 /* Player / NonPlayer                    */
  const char *perspective;          /* FirstPerson / SecondPerson / ...      */
  const char **descriptors; int n_descriptors;
  a5_prop_t *props;         int n_props;
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
  const char **commands;    int n_commands;
  int repeatable;
  const a5_xml_node_t *restrictions; /* <Restrictions> node, or NULL         */
  const a5_xml_node_t *actions;      /* <Actions> node, or NULL              */
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

typedef struct a5_event_s {
  const char *key;
  const char *type;
  const a5_xml_node_t *node;        /* SubEvent list etc.                    */
} a5_event_t;

typedef struct a5_group_s {
  const char *key;
  const char *type;                 /* Locations / Objects / Characters      */
  const char *name;
  const char **members;     int n_members;
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

  const char *title;
  const char *author;
  const char *version;

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
