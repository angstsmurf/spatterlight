/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Module notes:
 *
 * o ...
 */

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Trace flag, set before running. */
static sc_bool npc_trace = FALSE;


/*
 * npc_in_room()
 *
 * Return TRUE if a given NPC is currently in a given room.
 */
sc_bool
npc_in_room (sc_gameref_t game, sc_int npc, sc_int room)
{
  if (npc_trace)
    {
      sc_trace ("NPC: checking NPC %ld in room %ld (NPC is in %ld)\n",
                npc, room, gs_npc_location (game, npc));
    }

  return gs_npc_location (game, npc) - 1 == room;
}


/*
 * npc_count_in_room()
 *
 * Return the count of characters in the room, including the player.
 */
sc_int
npc_count_in_room (sc_gameref_t game, sc_int room)
{
  sc_int count, npc;

  /* Start with the player. */
  count = gs_player_in_room (game, room) ? 1 : 0;

  /* Total up other NPCs inhabiting the room. */
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      if (gs_npc_location (game, npc) - 1 == room)
        count++;
    }
  return count;
}


/*
 * npc_start_npc_walk()
 *
 * Start the given walk for the given NPC.
 */
void
npc_start_npc_walk (sc_gameref_t game, sc_int npc, sc_int walk)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[6];
  sc_int movetime;

  /* Retrieve movetime 0 for the NPC walk. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";
  vt_key[3].integer = walk;
  vt_key[4].string = "MoveTimes";
  vt_key[5].integer = 0;
  movetime = prop_get_integer (bundle, "I<-sisisi", vt_key) + 1;

  /* Set up walkstep. */
  gs_set_npc_walkstep (game, npc, walk, movetime);
}


/*
 * npc_turn_update()
 * npc_setup_initial()
 *
 * Set initial values for NPC states, and update on turns.
 */
void
npc_turn_update (sc_gameref_t game)
{
  sc_int index_;

  /* Set current values for NPC seen states. */
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (!gs_npc_seen (game, index_)
          && npc_in_room (game, index_, gs_playerroom (game)))
        gs_set_npc_seen (game, index_, TRUE);
    }
}

void
npc_setup_initial (sc_gameref_t game)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[5];
  sc_int index_;

  /* Start any walks that do not depend on a StartTask */
  vt_key[0].string = "NPCs";
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      sc_int walk;

      /* Set up invariant parts of the properties key. */
      vt_key[1].integer = index_;
      vt_key[2].string = "Walks";

      /* Process each walk, starting at the last and working backwards. */
      for (walk = gs_npc_walkstep_count (game, index_) - 1; walk >= 0; walk--)
        {
          sc_int starttask;

          /* If StartTask is zero, start walk at game start. */
          vt_key[3].integer = walk;
          vt_key[4].string = "StartTask";
          starttask = prop_get_integer (bundle, "I<-sisis", vt_key);
          if (starttask == 0)
            npc_start_npc_walk (game, index_, walk);
        }
    }

  /* Update seen flags for initial states. */
  npc_turn_update (game);
}


/*
 * Battle system.
 *
 * ADRIFT's optional Battle System gives the player and characters combat
 * attributes -- stamina, strength, accuracy, defence and agility -- and
 * resolves fights when enemies share a room.  The system is enabled per game
 * by the Globals.BattleSystem flag.  Attribute values are configured as [lo,
 * hi] ranges (or single values in version 3.9 games), and, with the exception
 * of stamina, are re-rolled randomly within their range each time they are
 * needed.  Stamina is rolled once at game start and then tracked as mutable
 * game state, as it is depleted by attacks and topped up by recovery.
 *
 * This first group of functions covers reading the configured attributes and
 * initialising stamina; combat resolution builds on these.
 */

/*
 * battle_is_enabled()
 *
 * Return TRUE if the game has the Battle System turned on.
 */
sc_bool
battle_is_enabled (sc_gameref_t game)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[2];

  vt_key[0].string = "Globals";
  vt_key[1].string = "BattleSystem";
  return prop_get_boolean (bundle, "B<-ss", vt_key);
}


/*
 * battle_get_property()
 *
 * Read a named integer from the Battle properties of the player (npc < 0) or
 * of a given NPC.  Returns the supplied fallback if the property is absent.
 */
static sc_int
battle_get_property (sc_gameref_t game, sc_int npc,
                     const sc_char *name, sc_int fallback)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[4], vt_rvalue;

  if (npc < 0)
    {
      vt_key[0].string = "Globals";
      vt_key[1].string = "Battle";
      vt_key[2].string = name;
      if (prop_get (bundle, "I<-sss", &vt_rvalue, vt_key))
        return vt_rvalue.integer;
    }
  else
    {
      vt_key[0].string = "NPCs";
      vt_key[1].integer = npc;
      vt_key[2].string = "Battle";
      vt_key[3].string = name;
      if (prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
        return vt_rvalue.integer;
    }
  return fallback;
}


/*
 * battle_bundle_range()
 *
 * Return the configured [lo,hi] range of a named attribute ("Stamina",
 * "Strength", "Accuracy", "Defense", "Agility") for the player (npc < 0) or an
 * NPC, read directly from the game bundle.  Version 4.0 games store separate
 * <name>Lo and <name>Hi values; version 3.9 games store a single <name> value,
 * which we treat as a degenerate range.  Character attributes are never
 * negative, so a negative probe result unambiguously signals an absent Hi
 * property.  This is the configured (immutable) seed; runtime reads go through
 * battle_attribute_range() below, which serves the mutable state.
 */
static void
battle_bundle_range (sc_gameref_t game, sc_int npc,
                     const sc_char *base, sc_int *lo, sc_int *hi)
{
  sc_char name[32];
  sc_int high;

  snprintf (name, sizeof (name), "%sHi", base);
  high = battle_get_property (game, npc, name, -1);
  if (high < 0)
    {
      /* No Lo/Hi split; fall back to a single version 3.9 value. */
      *lo = *hi = battle_get_property (game, npc, base, 0);
      return;
    }

  snprintf (name, sizeof (name), "%sLo", base);
  *lo = battle_get_property (game, npc, name, 0);
  *hi = (high < *lo) ? *lo : high;
}


/*
 * battle_attribute_slot()
 *
 * Map a ranged combat attribute name to its mutable-state slot, or -1 for
 * a non-ranged attribute such as "Stamina".
 */
static sc_int
battle_attribute_slot (const sc_char *base)
{
  if (strcmp (base, "Strength") == 0)
    return BATTLE_STRENGTH;
  if (strcmp (base, "Accuracy") == 0)
    return BATTLE_ACCURACY;
  if (strcmp (base, "Defense") == 0)
    return BATTLE_DEFENSE;
  if (strcmp (base, "Agility") == 0)
    return BATTLE_AGILITY;
  return -1;
}


/*
 * battle_attribute_range()
 * battle_attribute_max()
 *
 * Return respectively the current [lo,hi] roll range of a named attribute, and
 * its maximum cap, read from the mutable per-character battle state (seeded by
 * battle_start() and altered by type-7 task actions).  The four ranged combat
 * attributes are served from their slots; "Stamina" reports its mutable max.
 * Anything else falls back to the configured bundle value defensively.
 */
static void
battle_attribute_range (sc_gameref_t game, sc_int npc,
                        const sc_char *base, sc_int *lo, sc_int *hi)
{
  const sc_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                        : gs_npc_battle (game, npc);
  const sc_int slot = battle_attribute_slot (base);

  if (slot >= 0)
    {
      *lo = battle->lo[slot];
      *hi = battle->hi[slot];
      return;
    }
  battle_bundle_range (game, npc, base, lo, hi);
}

/*
 * battle_attribute()
 *
 * Return a fresh random roll of an attribute within its current range.
 */
sc_int
battle_attribute (sc_gameref_t game, sc_int npc, const sc_char *base)
{
  sc_int lo, hi;

  battle_attribute_range (game, npc, base, &lo, &hi);
  return sc_randomint (lo, hi);
}

sc_int
battle_attribute_max (sc_gameref_t game, sc_int npc, const sc_char *base)
{
  const sc_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                        : gs_npc_battle (game, npc);
  const sc_int slot = battle_attribute_slot (base);
  sc_int lo, hi;

  if (slot >= 0)
    return battle->max[slot];
  if (strcmp (base, "Stamina") == 0)
    return battle->maxstamina;

  battle_bundle_range (game, npc, base, &lo, &hi);
  return hi;
}


/* Forward declaration; defined with the combat helpers below. */
static sc_int battle_speed_roll (sc_gameref_t game, sc_int npc);

/*
 * battle_seed_attributes()
 *
 * Seed the mutable battle attributes of the player (npc < 0) or an NPC from
 * the configured bundle values: the [lo,hi] range and max cap of each ranged
 * attribute, max stamina, and (NPCs only) attitude and speed.  The configured
 * Hi doubles as the initial max cap.  Attitude and Speed are absent for the
 * player, so battle_get_property returns its 0 fallback there.
 */
static void
battle_seed_attributes (sc_gameref_t game, sc_int npc)
{
  static const sc_char *const names[BATTLE_ATTR_COUNT] = {
    "Strength", "Accuracy", "Defense", "Agility"
  };
  sc_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                  : gs_npc_battle (game, npc);
  sc_int slot, lo, hi;

  for (slot = 0; slot < BATTLE_ATTR_COUNT; slot++)
    {
      battle_bundle_range (game, npc, names[slot], &lo, &hi);
      battle->lo[slot] = lo;
      battle->hi[slot] = hi;
      battle->max[slot] = hi;
    }

  battle_bundle_range (game, npc, "Stamina", &lo, &hi);
  battle->maxstamina = hi;
  battle->attitude = battle_get_property (game, npc, "Attitude", 0);
  battle->speed = battle_get_property (game, npc, "Speed", 0);
  battle->seeded = TRUE;
}

/*
 * battle_start()
 *
 * Initialise battle state at game start.  Seed every mutable attribute from
 * the bundle, then roll a starting stamina within range for the player and for
 * every NPC and zero the recovery counters.  Each NPC's attack cadence counter
 * is primed from its (now seeded) Speed setting.  A no-op when the Battle
 * System is disabled.
 */
void
battle_start (sc_gameref_t game)
{
  sc_int npc, lo, hi;

  if (!battle_is_enabled (game))
    return;

  battle_seed_attributes (game, -1);
  battle_bundle_range (game, -1, "Stamina", &lo, &hi);
  gs_set_playerstamina (game, sc_randomint (lo, hi));
  gs_set_playerstaminacounter (game, 0);

  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      battle_seed_attributes (game, npc);
      battle_bundle_range (game, npc, "Stamina", &lo, &hi);
      gs_set_npc_stamina (game, npc, (hi > 0) ? sc_randomint (lo, hi) : 0);
      gs_set_npc_staminacounter (game, npc, 0);
      gs_set_npc_attackcounter (game, npc, battle_speed_roll (game, npc));
    }
}


/*
 * battle_attitude_from_ui()
 *
 * Remap the "Change Attitude" task-action enum (the editor's combo order,
 * 0 = Ally, 1 = Neutral, 2 = Enemy) to the internal attitude encoding used by
 * the combat code and the bundle (0 = Neutral, 1 = Ally, 2 = Enemy).
 */
static sc_int
battle_attitude_from_ui (sc_int value)
{
  switch (value)
    {
    case 0:  return 1;       /* Ally. */
    case 1:  return 0;       /* Neutral. */
    default: return 2;       /* Enemy. */
    }
}

/*
 * battle_change_attribute()
 *
 * Apply a type-7 "Change battle attribute" task action to the player (npc < 0)
 * or an NPC.  attribute is the ADRIFT attribute index (0 = Attitude, 1 =
 * Stamina, 2 = Max Stamina, 3/5/7/9 = Strength/Accuracy/Defence/Agility, 4/6/8/
 * 0xA = their Max caps, 0xB = Speed).  Attitude and Speed are set to the given
 * enum value; every other attribute changes by the signed delta.  Values are
 * floored at zero, and current stamina is re-clamped to its (possibly changed)
 * maximum.
 */
void
battle_change_attribute (sc_gameref_t game, sc_int npc,
                         sc_int attribute, sc_int value)
{
  sc_battle_t *battle = (npc < 0) ? gs_player_battle (game)
                                  : gs_npc_battle (game, npc);
  sc_int slot, stamina;

  switch (attribute)
    {
    case 0:                            /* Attitude (set, NPCs only). */
      battle->attitude = battle_attitude_from_ui (value);
      break;

    case 1:                            /* Stamina (current, delta). */
      stamina = (npc < 0) ? gs_playerstamina (game)
                          : gs_npc_stamina (game, npc);
      stamina += value;
      if (stamina < 0)
        stamina = 0;
      if (stamina > battle->maxstamina)
        stamina = battle->maxstamina;
      if (npc < 0)
        gs_set_playerstamina (game, stamina);
      else
        gs_set_npc_stamina (game, npc, stamina);
      break;

    case 2:                            /* Max Stamina (delta). */
      battle->maxstamina += value;
      if (battle->maxstamina < 0)
        battle->maxstamina = 0;
      stamina = (npc < 0) ? gs_playerstamina (game)
                          : gs_npc_stamina (game, npc);
      if (stamina > battle->maxstamina)
        {
          if (npc < 0)
            gs_set_playerstamina (game, battle->maxstamina);
          else
            gs_set_npc_stamina (game, npc, battle->maxstamina);
        }
      break;

    case 3: case 5: case 7: case 9:    /* Str/Acc/Def/Agi range (delta). */
      slot = (attribute - 3) / 2;
      battle->lo[slot] += value;
      if (battle->lo[slot] < 0)
        battle->lo[slot] = 0;
      battle->hi[slot] += value;
      if (battle->hi[slot] < 0)
        battle->hi[slot] = 0;
      break;

    case 4: case 6: case 8: case 0xA:  /* Max Str/Acc/Def/Agi (delta). */
      slot = (attribute - 4) / 2;
      battle->max[slot] += value;
      if (battle->max[slot] < 0)
        battle->max[slot] = 0;
      break;

    case 0xB:                          /* Speed (set, NPCs only). */
      battle->speed = value;
      break;

    default:
      break;
    }
}


/*
 * Battle system combat resolution.
 *
 * The combat model is reverse-engineered from the ADRIFT 4 Runner.  Each time
 * an attribute is required it is freshly rolled within its [lo,hi] range as
 * lo + Int(rnd * (hi - lo)) -- note that the high bound is exclusive.  An
 * attack hits when the attacker's effective accuracy strictly exceeds the
 * target's effective agility; a hit does (effective strength - effective
 * defence) points of stamina damage, applied only when positive.  Strength
 * and accuracy gain the wielded weapon's bonuses (a "shoot" weapon replaces
 * base strength entirely); defence gains the protection of all worn armour.
 */

/* Target sentinels returned by battle_select_target(). */
enum { BATTLE_PLAYER = -1, BATTLE_NONE = -2 };

/*
 * battle_roll()
 *
 * Roll an attribute value within [lo, hi), matching the Runner's
 * lo + Int(rnd * (hi - lo)).  Degenerate ranges return their single value.
 */
static sc_int
battle_roll (sc_int lo, sc_int hi)
{
  return (hi > lo) ? sc_randomint (lo, hi - 1) : lo;
}

/*
 * battle_object_battle()
 *
 * Read a named integer from an object's OBJ_BATTLE properties (weapon hit and
 * accuracy, armour protection, weapon method), or zero if absent.
 */
static sc_int
battle_object_battle (sc_gameref_t game, sc_int object, const sc_char *name)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[4], vt_rvalue;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Battle";
  vt_key[3].string = name;
  if (prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
    return vt_rvalue.integer;
  return 0;
}

/*
 * battle_object_is_weapon()
 *
 * Return TRUE if the object is flagged as a weapon.
 */
static sc_bool
battle_object_is_weapon (sc_gameref_t game, sc_int object)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[3], vt_rvalue;

  /* Static objects carry no Weapon property, so probe non-fatally. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Weapon";
  if (prop_get (bundle, "B<-sis", &vt_rvalue, vt_key))
    return vt_rvalue.boolean;
  return FALSE;
}

/*
 * battle_object_owned_by()
 * battle_object_worn_by()
 *
 * Tests for an object held/worn (owned) or specifically worn by the player
 * (npc < 0) or a given NPC.
 */
static sc_bool
battle_object_owned_by (sc_gameref_t game, sc_int object, sc_int npc)
{
  const sc_int position = gs_object_position (game, object);

  if (npc < 0)
    return position == OBJ_HELD_PLAYER || position == OBJ_WORN_PLAYER;
  return (position == OBJ_HELD_NPC || position == OBJ_WORN_NPC)
         && gs_object_parent (game, object) == npc;
}

static sc_bool
battle_object_worn_by (sc_gameref_t game, sc_int object, sc_int npc)
{
  const sc_int position = gs_object_position (game, object);

  if (npc < 0)
    return position == OBJ_WORN_PLAYER;
  return position == OBJ_WORN_NPC && gs_object_parent (game, object) == npc;
}

/*
 * battle_best_weapon()
 *
 * Return the object index of the best weapon wielded by the player (npc < 0)
 * or an NPC -- the owned weapon with the highest hit value -- or -1 for none.
 * Following the Runner, the player wields only carried (not worn) weapons.
 */
static sc_int
battle_best_weapon (sc_gameref_t game, sc_int npc)
{
  sc_int object, best = -1, best_hit = 0;

  for (object = 0; object < gs_object_count (game); object++)
    {
      sc_int hit;

      if (!battle_object_is_weapon (game, object))
        continue;
      if (npc < 0)
        {
          if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
            continue;
        }
      else if (!battle_object_owned_by (game, object, npc))
        continue;

      hit = battle_object_battle (game, object, "HitValue");
      if (best == -1 || hit > best_hit)
        {
          best = object;
          best_hit = hit;
        }
    }
  return best;
}

/*
 * battle_eff_strength()
 * battle_eff_accuracy()
 * battle_eff_agility()
 * battle_eff_defence()
 *
 * Compute a fresh roll of each effective combat attribute for the player
 * (npc < 0) or an NPC, applying the bonuses of the supplied wielded weapon
 * (-1 for none) and any worn armour.
 */
static sc_int
battle_eff_strength (sc_gameref_t game, sc_int npc, sc_int weapon)
{
  sc_int lo, hi, value;

  battle_attribute_range (game, npc, "Strength", &lo, &hi);
  value = battle_roll (lo, hi);
  if (weapon >= 0)
    {
      /* A "shoot" weapon (method 3) supplies all of the strength itself. */
      if (battle_object_battle (game, weapon, "Method") == 3)
        value = 0;
      value += battle_object_battle (game, weapon, "HitValue");
    }
  return value;
}

static sc_int
battle_eff_accuracy (sc_gameref_t game, sc_int npc, sc_int weapon)
{
  sc_int lo, hi, value;

  battle_attribute_range (game, npc, "Accuracy", &lo, &hi);
  value = battle_roll (lo, hi);
  if (weapon >= 0)
    value += battle_object_battle (game, weapon, "Accuracy");
  return value;
}

static sc_int
battle_eff_agility (sc_gameref_t game, sc_int npc)
{
  sc_int lo, hi;

  battle_attribute_range (game, npc, "Agility", &lo, &hi);
  return battle_roll (lo, hi);
}

static sc_int
battle_eff_defence (sc_gameref_t game, sc_int npc)
{
  sc_int lo, hi, value, object;

  battle_attribute_range (game, npc, "Defense", &lo, &hi);
  value = battle_roll (lo, hi);
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (battle_object_worn_by (game, object, npc))
        value += battle_object_battle (game, object, "ProtectionValue");
    }
  return value;
}

/*
 * battle_attitude()
 * battle_speed_roll()
 *
 * Read an NPC's current attitude (0 = neutral, 1 = ally, 2 = enemy) from the
 * mutable battle state, and roll the number of turns until its next attack
 * from its current Speed setting (0 = every turn, 1 = most turns, 2/3/4 =
 * every 2nd/3rd/4th turn).
 */
static sc_int
battle_attitude (sc_gameref_t game, sc_int npc)
{
  return gs_npc_battle (game, npc)->attitude;
}

static sc_int
battle_speed_roll (sc_gameref_t game, sc_int npc)
{
  const sc_int speed = gs_npc_battle (game, npc)->speed;

  switch (speed)
    {
    case 1:  return sc_randomint (1, 2);   /* Most turns. */
    case 2:  return 2;                     /* Every second turn. */
    case 3:  return 3;                     /* Every third turn. */
    case 4:  return 4;                     /* Every fourth turn. */
    default: return 1;                     /* Every turn. */
    }
}

/*
 * battle_print_combatant()
 *
 * Print the name of a combatant.  form selects the grammatical form: 0 for a
 * capitalised subject ("You" / "Goblin"), 1 for an object/lowercase form
 * ("you" / "Goblin"), 2 for a possessive ("your" / "Goblin's").
 */
static void
battle_print_combatant (sc_gameref_t game, sc_int npc, sc_int form)
{
  const sc_filterref_t filter = gs_get_filter (game);
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[3];
  const sc_char *name;

  if (npc < 0)
    {
      pf_buffer_string (filter, (form == 0) ? "You"
                                : (form == 1) ? "you" : "your");
      return;
    }

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Name";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, name);
  if (form == 2)
    pf_buffer_string (filter, "'s");
}

/*
 * battle_npc_battle_task()
 *
 * Read an NPC's 1-based battle task reference (KilledTask or StaminaTask),
 * returning the 0-based task index or -1 if none is set.
 */
static sc_int
battle_npc_battle_task (sc_gameref_t game, sc_int npc, const sc_char *name)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[4], vt_rvalue;

  if (npc < 0)
    return -1;
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Battle";
  vt_key[3].string = name;
  if (prop_get (bundle, "I<-siss", &vt_rvalue, vt_key))
    return vt_rvalue.integer - 1;
  return -1;
}

/*
 * battle_kill()
 *
 * Handle a combatant reaching zero stamina.  The player's death ends the game
 * through SCARE's normal completion path, so the interpreter offers its
 * restart/restore prompt.  An NPC runs its KilledTask if set, otherwise a
 * default death message is shown; the NPC's held and worn objects are dropped
 * into the room it died in, and the NPC is then removed from play.
 */
static void
battle_kill (sc_gameref_t game, sc_int npc, sc_bool visible)
{
  const sc_filterref_t filter = gs_get_filter (game);
  sc_int task, room, object;

  if (npc < 0)
    {
      pf_buffer_string (filter, "\nI'm afraid you are dead!\n");
      game->is_running = FALSE;
      game->has_completed = TRUE;
      return;
    }

  /* Note where the NPC dies before anything can relocate it. */
  room = gs_npc_location (game, npc) - 1;

  task = battle_npc_battle_task (game, npc, "KilledTask");
  if (task >= 0)
    task_run_task (game, task, TRUE);
  else if (visible)
    {
      pf_buffer_character (filter, '\n');
      battle_print_combatant (game, npc, 0);
      pf_buffer_string (filter, " falls down, dead.\n");
    }

  /* Re-home the dead NPC's held and worn objects to that room, so its
   * inventory is not orphaned along with the hidden character. */
  if (room >= 0)
    {
      for (object = 0; object < gs_object_count (game); object++)
        {
          const sc_int position = gs_object_position (game, object);

          if ((position == OBJ_HELD_NPC || position == OBJ_WORN_NPC)
              && gs_object_parent (game, object) == npc)
            gs_object_to_room (game, object, room);
        }
    }

  /* Remove the dead NPC from play (location zero is "hidden"). */
  gs_set_npc_location (game, npc, 0);
}

/*
 * battle_apply_damage()
 *
 * Subtract stamina damage from a combatant, triggering death at zero or the
 * NPC's low-stamina task when dropping below 10% of maximum stamina.
 */
static void
battle_apply_damage (sc_gameref_t game, sc_int npc, sc_int damage,
                     sc_bool visible)
{
  sc_int stamina, maximum, task;

  stamina = (npc < 0) ? gs_playerstamina (game) : gs_npc_stamina (game, npc);
  stamina -= damage;

  if (stamina <= 0)
    {
      if (npc < 0)
        gs_set_playerstamina (game, 0);
      else
        gs_set_npc_stamina (game, npc, 0);
      battle_kill (game, npc, visible);
      return;
    }

  if (npc < 0)
    gs_set_playerstamina (game, stamina);
  else
    gs_set_npc_stamina (game, npc, stamina);

  /* Run the NPC's low-stamina task when dropping below 10% of maximum. */
  maximum = battle_attribute_max (game, npc, "Stamina");
  if (maximum > 0 && stamina < maximum / 10)
    {
      task = battle_npc_battle_task (game, npc, "StaminaTask");
      if (task >= 0)
        task_run_task (game, task, TRUE);
    }
}

/*
 * battle_resolve()
 *
 * Resolve a single attack from attacker against target with the given wielded
 * weapon (-1 for none).  Combat messages are printed only when visible.
 */
static void
battle_resolve (sc_gameref_t game, sc_int attacker, sc_int target,
                sc_int weapon, sc_bool visible)
{
  const sc_filterref_t filter = gs_get_filter (game);

  if (battle_eff_accuracy (game, attacker, weapon)
      > battle_eff_agility (game, target))
    {
      sc_int damage = battle_eff_strength (game, attacker, weapon)
                      - battle_eff_defence (game, target);

      if (visible)
        {
          battle_print_combatant (game, attacker, 0);
          pf_buffer_string (filter, (attacker < 0) ? " hit " : " hits ");
          battle_print_combatant (game, target, 1);
        }
      if (damage > 0)
        {
          if (visible)
            pf_buffer_string (filter, ".\n");
          battle_apply_damage (game, target, damage, visible);
        }
      else if (visible)
        pf_buffer_string (filter,
                          ", but it doesn't seem to do any damage.\n");
    }
  else if (visible)
    {
      battle_print_combatant (game, target, 0);
      pf_buffer_string (filter, (target < 0) ? " manage to avoid "
                                             : " manages to avoid ");
      battle_print_combatant (game, attacker, 2);
      pf_buffer_string (filter, " attack.\n");
    }
}

/*
 * battle_select_target()
 *
 * Choose a target for an attacking NPC, following the Runner: neutrals never
 * attack; allies target enemies and vice versa (their attitudes summing to
 * three), and enemies also target the player.  A uniformly random choice is
 * made among the candidates sharing the NPC's room.  Returns an NPC index,
 * BATTLE_PLAYER, or BATTLE_NONE.
 */
static sc_int
battle_select_target (sc_gameref_t game, sc_int npc)
{
  const sc_int attitude = battle_attitude (game, npc);
  const sc_int location = gs_npc_location (game, npc);
  sc_int other, count, pick;

  if (attitude == 0 || location <= 0)
    return BATTLE_NONE;

  /* Count candidate foes co-located with the NPC, plus the player. */
  count = 0;
  for (other = 0; other < gs_npc_count (game); other++)
    {
      if (other != npc
          && gs_npc_location (game, other) == location
          && gs_npc_stamina (game, other) > 0
          && battle_attitude (game, other) == 3 - attitude)
        count++;
    }
  if (attitude == 2 && location - 1 == gs_playerroom (game)
      && gs_playerstamina (game) > 0)
    count++;

  if (count == 0)
    return BATTLE_NONE;

  /* Pick one candidate at random, re-walking the same candidate order. */
  pick = sc_randomint (1, count);
  for (other = 0; other < gs_npc_count (game); other++)
    {
      if (other != npc
          && gs_npc_location (game, other) == location
          && gs_npc_stamina (game, other) > 0
          && battle_attitude (game, other) == 3 - attitude)
        {
          if (--pick == 0)
            return other;
        }
    }
  return BATTLE_PLAYER;
}

/*
 * battle_recover()
 *
 * Apply automatic stamina recovery for the player (npc < 0) or an NPC: every
 * Recovery turns, restore one point of stamina up to the maximum.
 */
static void
battle_recover (sc_gameref_t game, sc_int npc)
{
  sc_int recovery, counter, stamina, maximum;

  recovery = battle_get_property (game, npc, "Recovery", 0);
  if (recovery <= 0)
    return;

  counter = (npc < 0)
            ? gs_playerstaminacounter (game) : gs_npc_staminacounter (game, npc);
  stamina = (npc < 0) ? gs_playerstamina (game) : gs_npc_stamina (game, npc);
  maximum = battle_attribute_max (game, npc, "Stamina");

  if (counter == 0)
    {
      counter = recovery;
      if (stamina < maximum)
        {
          stamina++;
          if (npc < 0)
            gs_set_playerstamina (game, stamina);
          else
            gs_set_npc_stamina (game, npc, stamina);
        }
    }
  counter--;

  if (npc < 0)
    gs_set_playerstaminacounter (game, counter);
  else
    gs_set_npc_staminacounter (game, npc, counter);
}

/*
 * battle_is_weapon()
 * battle_weapon_method()
 *
 * Public predicates for the command layer.  battle_is_weapon reports whether an
 * object is flagged as a weapon; battle_weapon_method returns a weapon's attack
 * method code (0 chop, 1 cut, 2 hit, 3 shoot, 4 stab, 5 throw), or -1 when the
 * object is not a weapon.
 */
sc_bool
battle_is_weapon (sc_gameref_t game, sc_int object)
{
  return object >= 0 && battle_object_is_weapon (game, object);
}

sc_int
battle_weapon_method (sc_gameref_t game, sc_int object)
{
  if (!battle_is_weapon (game, object))
    return -1;
  return battle_object_battle (game, object, "Method");
}

/*
 * battle_player_default_weapon()
 *
 * Return the weapon the player would attack with by default: the explicitly
 * wielded weapon if it is still held and a weapon, otherwise the best carried
 * weapon (-1 for bare hands).
 */
sc_int
battle_player_default_weapon (sc_gameref_t game)
{
  const sc_int wielded = gs_playerwield (game);

  if (wielded >= 0
      && gs_object_position (game, wielded) == OBJ_HELD_PLAYER
      && battle_object_is_weapon (game, wielded))
    return wielded;
  return battle_best_weapon (game, -1);
}

/*
 * battle_combatant_weapon()
 *
 * Return the weapon a combatant fights with: the player's default weapon
 * (npc < 0) or an NPC's best carried weapon (-1 for bare hands).
 */
sc_int
battle_combatant_weapon (sc_gameref_t game, sc_int npc)
{
  return (npc < 0) ? battle_player_default_weapon (game)
                   : battle_best_weapon (game, npc);
}

/*
 * battle_attribute_report()
 *
 * Status-command helper.  For a named attribute of the player (npc < 0) or an
 * NPC, report the configured [lo, hi] range plus a fresh effective roll that
 * includes the combatant's wielded-weapon and worn-armour bonuses, exactly as
 * the Runner's status display does.
 */
void
battle_attribute_report (sc_gameref_t game, sc_int npc, const sc_char *base,
                         sc_int *lo, sc_int *hi, sc_int *current)
{
  const sc_int weapon = battle_combatant_weapon (game, npc);

  battle_attribute_range (game, npc, base, lo, hi);

  if (strcmp (base, "Strength") == 0)
    *current = battle_eff_strength (game, npc, weapon);
  else if (strcmp (base, "Accuracy") == 0)
    *current = battle_eff_accuracy (game, npc, weapon);
  else if (strcmp (base, "Defense") == 0)
    *current = battle_eff_defence (game, npc);
  else if (strcmp (base, "Agility") == 0)
    *current = battle_eff_agility (game, npc);
  else
    *current = battle_roll (*lo, *hi);
}

/*
 * battle_player_attack()
 *
 * Resolve a player-initiated attack on an NPC.  When weapon is -1 the player's
 * default weapon (wielded, else best carried, else bare hands) is used.
 */
void
battle_player_attack (sc_gameref_t game, sc_int npc, sc_int weapon)
{
  if (weapon < 0)
    weapon = battle_player_default_weapon (game);
  battle_resolve (game, BATTLE_PLAYER, npc, weapon, TRUE);
}

/*
 * battle_tick()
 *
 * Per-turn battle processing: every NPC that is due to attack selects a target
 * and strikes, then automatic stamina recovery is applied to all combatants.
 * A no-op when the Battle System is disabled.
 */
void
battle_tick (sc_gameref_t game)
{
  sc_int npc;

  if (!battle_is_enabled (game))
    return;

  /* NPC attacks, governed by attitude and per-NPC attack cadence. */
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      sc_int counter;

      if (gs_npc_stamina (game, npc) <= 0 || battle_attitude (game, npc) == 0)
        continue;

      counter = gs_npc_attackcounter (game, npc) - 1;
      if (counter <= 0)
        {
          sc_int target = battle_select_target (game, npc);

          if (target != BATTLE_NONE)
            {
              sc_bool visible = (target == BATTLE_PLAYER)
                  || (gs_npc_location (game, npc) - 1 == gs_playerroom (game));
              battle_resolve (game, npc, target,
                              battle_best_weapon (game, npc), visible);
            }
          counter = battle_speed_roll (game, npc);
        }
      gs_set_npc_attackcounter (game, npc, counter);

      /* Stop processing if the player was killed mid-round. */
      if (!game->is_running)
        return;
    }

  /* Automatic stamina recovery for the player and all surviving NPCs. */
  battle_recover (game, -1);
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      if (gs_npc_stamina (game, npc) > 0)
        battle_recover (game, npc);
    }
}


/*
 * npc_room_in_roomgroup()
 *
 * Return TRUE if a given room is in a given group.
 */
static sc_bool
npc_room_in_roomgroup (sc_gameref_t game, sc_int room, sc_int group)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[4];
  sc_int member;

  /* Check roomgroup membership. */
  vt_key[0].string = "RoomGroups";
  vt_key[1].integer = group;
  vt_key[2].string = "List";
  vt_key[3].integer = room;
  member = prop_get_integer (bundle, "I<-sisi", vt_key);
  return member != 0;
}


/* List of direction names, for printing entry/exit messages. */
static const sc_char *const DIRNAMES_4[] = {
  "the north", "the east", "the south", "the west", "above", "below",
  "inside", "outside",
  NULL
};
static const sc_char *const DIRNAMES_8[] = {
  "the north", "the east", "the south", "the west", "above", "below",
  "inside", "outside",
  "the north-east", "the south-east", "the south-west", "the north-west",
  NULL
};

/*
 * npc_random_adjacent_roomgroup_member()
 *
 * Return a random member of group adjacent to given room.
 */
static sc_int
npc_random_adjacent_roomgroup_member (sc_gameref_t game,
                                      sc_int room, sc_int group)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[5];
  sc_bool eightpointcompass;
  sc_int roomlist[12], count, length, index_;

  /* If given room is "hidden", return nothing. */
  if (room == -1)
    return -1;

  /* How many exits to consider? */
  vt_key[0].string = "Globals";
  vt_key[1].string = "EightPointCompass";
  eightpointcompass = prop_get_boolean (bundle, "B<-ss", vt_key);
  if (eightpointcompass)
    length = sizeof (DIRNAMES_8) / sizeof (DIRNAMES_8[0]) - 1;
  else
    length = sizeof (DIRNAMES_4) / sizeof (DIRNAMES_4[0]) - 1;

  /* Poll adjacent rooms. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Exits";
  count = 0;
  for (index_ = 0; index_ < length; index_++)
    {
      sc_int adjacent;

      vt_key[3].integer = index_;
      vt_key[4].string = "Dest";
      adjacent = prop_get_child_count (bundle, "I<-sisis", vt_key);

      if (adjacent > 0 && npc_room_in_roomgroup (game, adjacent - 1, group))
        {
          roomlist[count] = adjacent - 1;
          count++;
        }
    }

  /* Return a random adjacent room, or -1 if nothing is adjacent. */
  return (count > 0) ? roomlist[sc_randomint (0, count - 1)] : -1;
}


/*
 * npc_announce()
 *
 * Helper for npc_tick_npc().
 */
static void
npc_announce (sc_gameref_t game, sc_int npc,
              sc_int room, sc_bool is_exit, sc_int npc_room)
{
  const sc_filterref_t filter = gs_get_filter (game);
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[5], vt_rvalue;
  const sc_char *text, *name, *const *dirnames;
  sc_int dir, dir_match;
  sc_bool eightpointcompass, showenterexit, found;

  /* If no announcement required, return immediately. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "ShowEnterExit";
  showenterexit = prop_get_boolean (bundle, "B<-sis", vt_key);
  if (!showenterexit)
    return;

  /* Get exit or entry text, and NPC name. */
  vt_key[2].string = is_exit ? "ExitText" : "EnterText";
  text = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Name";
  name = prop_get_string (bundle, "S<-sis", vt_key);

  /* Decide on four or eight point compass names list. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "EightPointCompass";
  eightpointcompass = prop_get_boolean (bundle, "B<-ss", vt_key);
  dirnames = eightpointcompass ? DIRNAMES_8 : DIRNAMES_4;

  /* Set invariant key for room exit search. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Exits";

  /* Find the room exit that matches the NPC room. */
  found = FALSE;
  dir_match = 0;
  for (dir = 0; dirnames[dir]; dir++)
    {
      vt_key[3].integer = dir;
      if (prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key))
        {
          sc_int dest;

          /* Get room's direction destination, and compare. */
          vt_key[4].string = "Dest";
          dest = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
          if (dest == npc_room)
            {
              dir_match = dir;
              found = TRUE;
              break;
            }
        }
    }

  /* Print NPC exit/entry details. */
  pf_buffer_character (filter, '\n');
  pf_new_sentence (filter);
  pf_buffer_string (filter, name);
  pf_buffer_character (filter, ' ');
  pf_buffer_string (filter, text);
  if (found)
    {
      pf_buffer_string (filter, is_exit ? " to " : " from ");
      pf_buffer_string (filter, dirnames[dir_match]);
    }
  pf_buffer_string (filter, ".\n");

  /* Handle any associated resource. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Res";
  vt_key[3].integer = is_exit ? 3 : 2;
  res_handle_resource (game, "sisi", vt_key);
}


/*
 * npc_tick_npc_walk()
 *
 * Helper for npc_tick_npc().
 */
static void
npc_tick_npc_walk (sc_gameref_t game, sc_int npc, sc_int walk)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[6];
  sc_int roomgroups, movetimes, walkstep, start, dest, destnum;
  sc_int chartask, objecttask;

  if (npc_trace)
    {
      sc_trace ("NPC: ticking NPC %ld, walk %ld: step %ld\n",
                npc, walk, gs_npc_walkstep (game, npc, walk));
    }

  /* Count roomgroups for later use. */
  vt_key[0].string = "RoomGroups";
  roomgroups = prop_get_child_count (bundle, "I<-s", vt_key);

  /* Get move times array length. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";
  vt_key[3].integer = walk;
  vt_key[4].string = "MoveTimes";
  movetimes = prop_get_child_count (bundle, "I<-sisis", vt_key);

  /* Find a step to match the movetime. */
  for (walkstep = 0; walkstep < movetimes - 1; walkstep++)
    {
      sc_int  movetime;

      vt_key[5].integer = walkstep + 1;
      movetime = prop_get_integer (bundle, "I<-sisisi", vt_key);
      if (gs_npc_walkstep (game, npc, walk) > movetime)
        break;
    }

  /* Sort out a destination. */
  dest = start = gs_npc_location (game, npc) - 1;

  vt_key[4].string = "Rooms";
  vt_key[5].integer = walkstep;
  destnum = prop_get_integer (bundle, "I<-sisisi", vt_key);

  if (destnum == 0)          /* Hidden. */
    dest = -1;
  else if (destnum == 1)     /* Follow player. */
    dest = gs_playerroom (game);
  else if (destnum < gs_room_count (game) + 2)
    dest = destnum - 2;      /* To room. */
  else if (destnum < gs_room_count (game) + 2 + roomgroups)
    {
      sc_int initial;

      /* For roomgroup walks, move only if walksteps has just refreshed. */
      vt_key[4].string = "MoveTimes";
      vt_key[5].integer = 0;
      initial = prop_get_integer (bundle, "I<-sisisi", vt_key);
      if (gs_npc_walkstep (game, npc, walk) == initial)
        {
          sc_int group;

          group = destnum - 2 - gs_room_count (game);
          dest = npc_random_adjacent_roomgroup_member (game, start, group);
          if (dest == -1)
            dest = lib_random_roomgroup_member (game, group);
        }
    }

  /* See if the NPC actually moved. */
  if (start != dest)
    {
      if (npc_trace)
        sc_trace ("NPC: walking NPC %ld moved to %ld\n", npc, dest);

      /* Move NPC to destination. */
      gs_set_npc_location (game, npc, dest + 1);

      /* Announce NPC movements, and handle meeting characters and objects. */
      if (gs_player_in_room (game, start))
        npc_announce (game, npc, start, TRUE, dest);
      else if (gs_player_in_room (game, dest))
        npc_announce (game, npc, dest, FALSE, start);
    }

  /* Handle meeting characters and objects. */
  vt_key[4].string = "CharTask";
  chartask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
  if (chartask >= 0)
    {
      sc_int meetchar;

      /* Run meetchar task if appropriate. */
      vt_key[4].string = "MeetChar";
      meetchar = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
      if ((meetchar == -1 && gs_player_in_room (game, dest))
          || (meetchar >= 0 && dest == gs_npc_location (game, meetchar) - 1))
        {
          if (task_can_run_task (game, chartask))
            task_run_task (game, chartask, TRUE);
        }
    }

  vt_key[4].string = "ObjectTask";
  objecttask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
  if (objecttask >= 0)
    {
      sc_int meetobject;

      /* Run meetobject task if appropriate. */
      vt_key[4].string = "MeetObject";
      meetobject = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
      if (meetobject >= 0 && obj_directly_in_room (game, meetobject, dest))
        {
          if (task_can_run_task (game, objecttask))
            task_run_task (game, objecttask, TRUE);
        }
    }
}


/*
 * npc_tick_npc()
 *
 * Move an NPC one step along current walk.
 */
static void
npc_tick_npc (sc_gameref_t game, sc_int npc)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  sc_vartype_t vt_key[6];
  sc_int walk;
  sc_bool has_moved = FALSE;

  if (npc_trace)
    sc_trace ("NPC: ticking NPC %ld\n", npc);

  /* Set up invariant key parts. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";

  /* Find active walk, and if any found, make a step along it. */
  for (walk = gs_npc_walkstep_count (game, npc) - 1; walk >= 0; walk--)
    {
      sc_int starttask, stoppingtask;

      /* Ignore finished walks. */
      if (gs_npc_walkstep (game, npc, walk) <= 0)
        continue;

      /* Get start task. */
      vt_key[3].integer = walk;
      vt_key[4].string = "StartTask";
      starttask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;

      /*
       * Check that the starter is still complete, and if not, stop walk.
       * Then keep on looking for an active walk.
       */
      if (starttask >= 0 && !gs_task_done (game, starttask))
        {
          if (npc_trace)
            sc_trace ("NPC: stopping NPC %ld walk, start task undone\n", npc);

          gs_set_npc_walkstep (game, npc, walk, -1);
          continue;
        }

      /* Get stopping task. */
      vt_key[4].string = "StoppingTask";
      stoppingtask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;

      /*
       * If any stopping task has completed, ignore this walk but don't
       * actually finish it; more like an event pauser, then.
       *
       * TODO Is this right?
       */
      if (stoppingtask >= 0 && gs_task_done (game, stoppingtask))
        {
          if (npc_trace)
            sc_trace ("NPC: ignoring NPC %ld walk, stop task done\n", npc);

          continue;
        }

      /* Decrement steps. */
      gs_decrement_npc_walkstep (game, npc, walk);

      /* If we just hit a walk end, loop if called for. */
      if (gs_npc_walkstep (game, npc, walk) == 0)
          {
            sc_bool is_loop;

            /* If walk is a loop, restart it. */
            vt_key[4].string = "Loop";
            is_loop = prop_get_boolean (bundle, "B<-sisis", vt_key);
            if (is_loop)
              {
                vt_key[4].string = "MoveTimes";
                vt_key[5].integer = 0;
                gs_set_npc_walkstep (game, npc, walk,
                                     prop_get_integer (bundle,
                                                       "I<-sisisi", vt_key));
              }
            else
              gs_set_npc_walkstep (game, npc, walk, -1);
          }

      /*
       * If not yet made a move on this walk, make one, and once made, make
       * no other
       */
      if (!has_moved)
        {
          npc_tick_npc_walk (game, npc, walk);
          has_moved = TRUE;
        }
    }
}


/*
 * npc_tick_npcs()
 *
 * Move each NPC one step along current walk.
 */
void
npc_tick_npcs (sc_gameref_t game)
{
  const sc_prop_setref_t bundle = gs_get_bundle (game);
  const sc_gameref_t undo = game->undo;
  sc_int npc;

  /*
   * Compare the player location to last turn, to see if the player has moved
   * this turn.  If moved, look for meetings with NPCs.
   *
   * TODO Is this the right place to do this.  After ticking each NPC, rather
   * than before, seems more appropriate.  But the messages come out in the
   * right order by putting it here.
   *
   * Also, note that we take the shortcut of using the undo gamestate here,
   * rather than properly recording the prior location of the player, and
   * perhaps also NPCs, in the live gamestate.
   */
  if (undo && !gs_player_in_room (undo, gs_playerroom (game)))
    {
      for (npc = 0; npc < gs_npc_count (game); npc++)
        {
          sc_int walk;

          /* Iterate each NPC's walks. */
          for (walk = gs_npc_walkstep_count (game, npc) - 1; walk >= 0; walk--)
            {
              sc_vartype_t vt_key[5];
              sc_int chartask;

              /* Ignore finished walks. */
              if (gs_npc_walkstep (game, npc, walk) <= 0)
                continue;

              /* Retrieve any character meeting task for the NPC. */
              vt_key[0].string = "NPCs";
              vt_key[1].integer = npc;
              vt_key[2].string = "Walks";
              vt_key[3].integer = walk;
              vt_key[4].string = "CharTask";
              chartask = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
              if (chartask >= 0)
                {
                  sc_int meetchar;

                  /* Run meetchar task if appropriate. */
                  vt_key[4].string = "MeetChar";
                  meetchar = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
                  if (meetchar == -1 &&
                      gs_player_in_room (game, gs_npc_location (game, npc) - 1))
                    {
                      if (task_can_run_task (game, chartask))
                        task_run_task (game, chartask, TRUE);
                    }
                }
            }
        }
    }

  /* Iterate and tick each individual NPC. */
  for (npc = 0; npc < gs_npc_count (game); npc++)
    npc_tick_npc (game, npc);
}


/*
 * npc_debug_trace()
 *
 * Set NPC tracing on/off.
 */
void
npc_debug_trace (sc_bool flag)
{
  npc_trace = flag;
}
