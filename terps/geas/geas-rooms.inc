// geas-rooms.inc -- Rooms and exits: room lookup and goto_room, the error-
// message table, object display names, the places pane, the exit tables
// (declared and runtime, locks), the room description (look) and pictures.
//
// Split out of geas-runner.cc, which #includes it (same single-TU layout
// as the quest5/aslx-*.inc units); the code is unchanged from the split.

/* Is `room` somewhere the player can actually stand?  Quest's GetRoomID scans
 * _rooms for a RoomName match (V4Game.cs:6348-6365) -- the room's *alias* is
 * never consulted -- and "create room" appends a new, empty entry to that same
 * array (ExecuteCreate, V4Game.cs:5244-5276), which is why runtime-created
 * rooms count too. */
bool geas_implementation::room_exists (const string &room) const
{
  return gf.find_by_name ("room", room) != NULL ||
         has_obj_property ("!createdroom", lcase (room));
}

void geas_implementation::goto_room (string room)
{
  /* PlayGame refuses a room that does not exist: it logs "No such room 'X'" and
   * returns without touching _currentRoom (V4Game.Part2.cs:6674-6684), so the
   * player stays where they were.  geas used to move anyway, which invented a
   * phantom room -- and since rooms and objects share the property table, a
   * name that belongs to an object rendered that object's prefix and look text
   * as the room description.  "pure chaos" (game.asl) does `goto <bed>` when
   * `bed` is an object and the room it means only carries `alias <bed>`, and
   * ended up describing the bed as though it were a place. */
  if (!room_exists (room))
    {
      gi->debug_print ("No such room '" + room + "'");
      return;
    }
  state.location = room;
  regen_var_room();
  regen_var_dirs();
  regen_var_look();
  objs_vars_dirty_ = true;
  /* PlayGame stamps the room object with a "visited" property, which games test
   * with `property <room; visited>` (V4Game.Part2.cs:6688-6708).  Which side of
   * the room's own entry script that happens on is version-dependent: 391..409
   * mark it on the way in, 410 and later only after the script has run, so a
   * 410 room script still sees its own room as unvisited the first time.  The
   * Pilgrims Progress (ASL 400) counts on the early form -- its nine-room field
   * maze only produces Evangelist once every other field is `visited`, and the
   * check lives in the script of the room you walk back into.
   *
   * Quest overwrites the property in place rather than appending a second copy
   * (AddToObjectProperties, V4Game.cs:4041-4059), so only stamp it once -- a
   * long game would otherwise pile up one property record per step taken. */
  if (asl_version_ >= 391 && asl_version_ < 410 && !has_room_property (room, "visited"))
    set_obj_property (room, "visited");
  /* Quest's order (PlayGame): describe the room first, then run its entry
   * script.  If that script gotos another room, the nested goto describes the
   * destination -- so, unlike the old script-then-look order, there is no
   * trailing look() here to redescribe after a redirect. */
  look();
  string scr;
  if (get_room_action (room, "script", scr))
    run_script_as (room, scr);
  if (asl_version_ >= 410 && !has_room_property (room, "visited"))
    set_obj_property (room, "visited");
}

/* Quest keeps its player-error messages in a single array indexed by the
 * PlayerError enum (V4Game.Types.cs:287-326).  SetDefaultPlayerErrorMessages
 * fills it with the built-in texts (V4Game.Part2.cs:425-471) and
 * SetUpUserDefinedPlayerErrors then walks the game block overwriting entries
 * from "error <name; text>" lines (ibid. 1392-1625) -- so the LAST matching
 * line in the block wins, not the first.  error_table is that array in enum
 * order.  "id" is the name geas's own call sites pass to display_error; the
 * name a game writes in ASL is not always the same word (Quest spells
 * DefaultTake "defaultake"), which asl_error_index sorts out.  An entry marked
 * pcase is printed through print_eval_p, which capitalises the first letter
 * after substitution -- the message may open with a #quest.error.gender#
 * pronoun. */
namespace {

enum player_error {
  PE_BadCommand, PE_BadGo, PE_BadGive, PE_BadCharacter, PE_NoItem,
  PE_ItemUnwanted, PE_BadLook, PE_BadThing, PE_DefaultLook, PE_DefaultSpeak,
  PE_BadItem, PE_DefaultTake, PE_BadUse, PE_DefaultUse, PE_DefaultOut,
  PE_BadPlace, PE_BadExamine, PE_DefaultExamine, PE_BadTake, PE_CantDrop,
  PE_DefaultDrop, PE_BadDrop, PE_BadPronoun, PE_AlreadyOpen, PE_AlreadyClosed,
  PE_CantOpen, PE_CantClose, PE_DefaultOpen, PE_DefaultClose, PE_BadPut,
  PE_CantPut, PE_DefaultPut, PE_CantRemove, PE_AlreadyPut, PE_DefaultRemove,
  PE_Locked, PE_DefaultWait, PE_AlreadyTaken, PE_COUNT
};

struct error_entry { const char *id; const char *text; bool pcase; };

const error_entry error_table[] = {
  { "badcommand",     "I don't understand your command. Type HELP for a list of valid commands.", false },
  { "badgo",          "I don't understand your use of 'GO' - you must either GO in some direction, or GO TO a place.", false },
  { "badgive",        "You didn't say who you wanted to give that to.", false },
  { "badcharacter",   "I can't see anybody of that name here.",       false },
  { "noitem",         "You don't have that.",                         false },
  { "itemunwanted",   "#quest.error.gender# doesn't want #quest.error.article#.", true },
  { "badlook",        "You didn't say what you wanted to look at.",   false },
  { "badthing",       "I can't see that here.",                       false },
  { "defaultlook",    "Nothing out of the ordinary.",                 false },
  { "defaultspeak",   "#quest.error.gender# says nothing.",           true },
  { "baditem",        "I can't see that anywhere.",                   false },
  { "defaulttake",    "You pick #quest.error.article# up.",           false },
  { "baduse",         "You didn't say what you wanted to use that on.", false },
  { "defaultuse",     "You can't use that here.",                     false },
  { "defaultout",     "There's nowhere you can go out to around here.", false },
  { "badplace",       "You can't go there.",                          false },
  { "badexamine",     "You didn't say what you wanted to examine.",   false },
  { "defaultexamine", "Nothing out of the ordinary.",                 false },
  { "badtake",        "You can't take #quest.error.article#.",        false },
  { "cantdrop",       "You can't drop that here.",                    false },
  { "defaultdrop",    "You drop #quest.error.article#.",              false },
  { "baddrop",        "You are not carrying such a thing.",           false },
  { "badpronoun",     "I don't know what '#quest.error.pronoun#' you are referring to.", false },
  { "alreadyopen",    "It is already open.",                          false },
  { "alreadyclosed",  "It is already closed.",                        false },
  { "cantopen",       "You can't open that.",                         false },
  { "cantclose",      "You can't close that.",                        false },
  { "defaultopen",    "You open it.",                                 false },
  { "defaultclose",   "You close it.",                                false },
  { "badput",         "You didn't specify what you wanted to put #quest.error.article# on or in.", true },
  { "cantput",        "You can't put that there.",                    false },
  { "defaultput",     "Done.",                                        false },
  { "cantremove",     "You can't remove that.",                       false },
  { "alreadyput",     "It is already there.",                         false },
  { "defaultremove",  "Done.",                                        false },
  { "locked",         "The exit is locked.",                          false },
  { "defaultwait",    "Press a key to continue...",                   false },
  { "alreadytaken",   "You already have that.",                       false },
};
static_assert (sizeof error_table / sizeof error_table[0] == PE_COUNT,
	       "error_table must cover every PlayerError");

/* Quest's switch on the error name (V4Game.Part2.cs:1410-1610).  The name is
 * compared un-lowercased and un-trimmed, and -- the part that bites -- an
 * unrecognised name leaves currentError at 0, so the line overwrites
 * BadCommand instead of being ignored. */
int asl_error_index (const string &name, int asl_version)
{
  if (name == "defaultake")
    return asl_version <= 280 ? PE_BadTake : PE_DefaultTake;
  if (name == "badexamine")
    return asl_version >= 310 ? PE_BadExamine : PE_BadCommand;
  /* "defaulttake" -- the spelling geas's own call sites use -- is not a name
   * Quest's switch knows; a game that writes it is redefining badcommand. */
  if (name == "defaulttake")
    return PE_BadCommand;
  for (int i = 0; i < PE_COUNT; ++i)
    if (name == error_table[i].id)
      return i;
  return PE_BadCommand;
}

}

void geas_implementation::init_error_messages ()
{
  for (int i = 0; i < PE_COUNT; ++i)
    {
      error_messages_.push_back (error_table[i].text);
      error_pcase_.push_back (error_table[i].pcase);
    }

  const GeasBlock *game = gf.find_by_name ("game", "game");
  if (game == NULL)
    {
      gi->debug_print ("display_error: no 'game' block found");
      return;
    }
  bool examine_customised = false;
  std::string::size_type c1, c2;
  for (const string &line: game->data)
    {
      /* The keyword is CI -- Quest scans for it with BeginsWith
       * (SetUpUserDefinedPlayerErrors, V4Game.Part2.cs:1404). */
      string tok = first_token (line, c1, c2);
      if (!ci_equal (tok, "error"))
	continue;
      tok = next_token (line, c1, c2);
      if (!is_param (tok))
	{
	  gi->debug_print ("Bad error line: " + line);
	  continue;
	}
      string text = param_contents (tok);
      std::string::size_type index = text.find (';');
      if (index == string::npos)
	continue;
      /* Only the message is trimmed: Quest takes the name as
       * Left(errorInfo, scp - 1) with no Trim, so "error < badthing; ...>"
       * matches nothing and lands on badcommand. */
      int idx = asl_error_index (text.substr (0, index), asl_version_);
      string msg = trim (text.substr (index + 1));
      if (idx == PE_DefaultExamine)
	examine_customised = true;
      error_messages_[idx] = msg;
      error_pcase_[idx] = true;
      /* Setting the default look message sets the default examine message too,
       * unless an "error <defaultexamine; ...>" line has already been seen. */
      if (idx == PE_DefaultLook && !examine_customised)
	{
	  error_messages_[PE_DefaultExamine] = msg;
	  error_pcase_[PE_DefaultExamine] = true;
	}
    }
}

void geas_implementation::display_error (string errorname, string obj)
{
  display_error_info (errorname, obj, "");
}

void geas_implementation::display_error_info (string errorname, string obj,
					      const string &extra)
{
  GEAS_DBG << "display_error (" << errorname << ", " << obj << ")\n";
  if (obj != "")
    {
      string tmp;
      if (!get_obj_property (obj, "gender", tmp))
	tmp = "it";
      /* Capitalised: every quest.error.gender Quest writes goes through
       * UCase(Left(...)) or GetGender(..., capitalise: true)
       * (V4Game.Part2.cs:4790-4792, 5046-5047, 5094), so a custom message
       * reading it mid-sentence sees "He", not "he". */
      set_svar ("quest.error.gender", pcase (tmp));

      if (!get_obj_property (obj, "article", tmp))
	tmp = "it";
      set_svar ("quest.error.article", tmp);

      GEAS_DBG << "In erroring " << errorname << " / " << obj << ", qeg == "
	   << get_svar ("quest.error.gender") << ", qea == "
	   << get_svar ("quest.error.article") << endl;
      /* quest.error.charactername is deliberately not set here: Quest fills it
       * only in ExecGive's refusal path, so geas does too (see the give
       * handler in try_match). */
    }

  /* Quest has no error of this name: geas's generic verb dispatch needs a
   * refusal of its own, and a game's "error <defaultverb; ...>" line lands on
   * badcommand (asl_error_index), so this text is not overridable. */
  if (errorname == "defaultverb")
    {
      print_eval ("You can't do that.");
      return;
    }

  if (error_messages_.empty ())
    init_error_messages ();

  for (int i = 0; i < PE_COUNT; ++i)
    if (errorname == error_table[i].id)
      {
	string msg = eval_string (error_messages_[i]);
	if (error_pcase_[i])
	  msg = pcase (msg);
	if (extra != "")
	  {
	    if (msg.length () > 0 && msg[msg.length () - 1] == '.')
	      msg = msg.substr (0, msg.length () - 1);
	    msg = msg + " - " + extra + ".";
	  }
	print_formatted (msg);
	return;
      }
  gi->debug_print ("Bad error name " + errorname);
}

string geas_implementation::displayed_name (const string &obj) const
{
  string rv = obj, tmp;

  if (get_obj_property (obj, "alias", tmp))
    rv = tmp;
  else
    {
      for (const auto &i: gf.blocks)
	if (ci_equal (i.name, obj))
	  {
	    rv = i.name;
	    break;
	  }
    }
  return rv;
}

/* The parameter token of a "place [locked] <...>" line, with the optional
 * keyword consumed and `locked` set from it.
 *
 * AddExitFromTag strips a leading "locked " after the direction word, and
 * "place" is one of the direction words it accepts (RoomExits.cs:120-149), so a
 * place locks exactly as a compass direction does.  That parser is 4.10's: the
 * older loader hands the whole line to GetParameter, which scans for the "<" and
 * so walks straight past the keyword without noticing it (V4Game.Part2.cs:
 * 1216-1234).  The keyword is therefore eaten at every version -- otherwise the
 * line reads as malformed and the place disappears -- but only marks the exit
 * locked from 4.10 on, and below that the parameter keeps its older
 * "prefix; dest" reading, which is what Quest makes of it there. */
static string place_tag_param (const string &line, std::string::size_type &c1,
			       std::string::size_type &c2, int version,
			       bool &locked)
{
  string tok = next_token (line, c1, c2);
  locked = false;
  if (ci_equal (tok, "locked"))
    {
      locked = (version >= 410);
      tok = next_token (line, c1, c2);
    }
  return tok;
}

/* For each destination, give:
 * - printed name
 * - accepted name, with prefix
 * - accepted name, without prefix
 * - destination, internal format
 * - script (optional)
 */
vector<vector<string> > geas_implementation::get_places (const string &room)
{
  vector<vector<string> > rv;

  const GeasBlock *gb = gf.find_by_name ("room", room);
  if (gb == NULL)
    return rv;

  std::string::size_type c1, c2;
  for (const auto &line: gb->data)
    {
      string tok = first_token (line, c1, c2);
      if (tok == "place")
	{
	  bool locked;
	  tok = place_tag_param (line, c1, c2, asl_version_, locked);
	  if (!is_param(tok))
	    {
	      gi->debug_print ("Expected parameter after 'place' in " + line);
	      continue;
	    }
	  string dest_param = eval_param (tok);
	  if (dest_param == "")
	    {
	      gi->debug_print ("Parameter empty in " + line);
	      continue;
	    }
	  /* A locked place is still advertised in the exits line: locking gates
	   * the crossing and nothing else (RoomExit.Go, RoomExit.cs:240-259), so
	   * `locked` is of no interest here -- the "go to" handler asks
	   * exit_locked() about it.  From 4.10 the second field of the parameter
	   * is that exit's lock message rather than a prefix, which is why the
	   * split goes through split_exit_dest. */
	  string dest, prefix;
	  split_exit_dest (dest_param, dest, prefix);
	  string rest = trim (line.substr (c2));
	  /* The destination room's alias replaces its name only for a *plain*
	   * place, and only from ASL 3.11 up.  Both halves of that condition are
	   * one `&' in Quest, and it guards the listing (GetGoToExits,
	   * V4Game.Part2.cs:7790-7818) and the name the player has to type
	   * (PlaceExist, ibid. 6568-6594) alike:
	   *
	   *   if ((ASLVersion >= 311) & string.IsNullOrEmpty(Places[i].Script))
	   *
	   * So a scripted `place <elevatorroom> { ... }' shows -- and answers to --
	   * "elevatorroom" even when that room is aliased "elevator", which is what
	   * World's End's weapon store does; and below 3.11 a place is named by the
	   * tag whatever the destination is aliased, which is what saves Space, its
	   * <Science Lab> being a room its author aliased "Sciece Lab".  geas used
	   * to alias every kind at every version, so the listing advertised a name
	   * that GO TO then rejected.
	   *
	   * The room the player is *in* is unaffected: an alias always names that
	   * one (see regen_var_room). */
	  string displayed = (rest == "" && asl_version_ >= 311)
	    ? displayed_name (dest) : dest;
	  string printed_dest = (prefix != "" ? prefix + " " : "") +
	    "|b" + displayed + "|xb";
	  /* Below 2.80 the printed form is cut out of the tag's parameter by
	   * hand.  Quest's own cut has a fencepost error in it: ShowRoomInfoV2
	   * takes the prefix as Trim(Left(place, InStr(place, ";") - 1)) but the
	   * name as Right(place, Len(place) - (InStr(place, ";") + 1)), one
	   * character short, because that +1 assumes a space after the semicolon
	   * (V4Game.Part2.cs:1988-1999).  The usual `place <The; Library>'
	   * spelling has one and comes out right; "Magic Sword" writes `place
	   * <Your Training room;Training Room 1>' without one, and Quest really
	   * does offer to take the player to "Your Training room raining Room
	   * 1".  geas splits properly instead -- a DELIBERATE deviation: the
	   * mangled name is one Quest's own PlaceExist would accept from the
	   * player (it splits the parameter properly, ibid. 6568-6594, which is
	   * why "go to training room 1" works even there), so printing it whole
	   * only ever helps.  The oracle sweep carries the resulting Magic Sword
	   * diff as `deliberate:' -- see FINDINGS.md.  A parameter with no
	   * semicolon at all is printed whole, unTrimmed (ibid. 2000-2003). */
	  if (asl_version_ < 280)
	    {
	      std::string::size_type semi = dest_param.find (';');
	      if (semi == string::npos)
		printed_dest = "|b" + dest_param + "|xb";
	      else
		printed_dest = trim (dest_param.substr (0, semi)) + " |b"
		  + trim (dest_param.substr (semi + 1)) + "|xb";
	    }

	  vector<string> tmp;
	  tmp.push_back (printed_dest);
	  tmp.push_back (prefix + " " + displayed);
	  tmp.push_back (displayed);
	  tmp.push_back (dest);
	  if (rest != "")
	    tmp.push_back (rest);
	  rv.push_back (tmp);
	}
    }

  for (const auto &i: state.exits)
    {
      if (i.src != room)
	continue;
      const string &line = i.dest;
      string tok = first_token (line, c1, c2);
      if (tok == "exit")
	{
	  tok = next_token (line, c1, c2);
	  if (!is_param(tok))
	    continue;
	  /* A directionless "place" exit is stored as "exit <src;dest>", so the
	   * token after "exit" is already the src;dest parameter.  (Directional
	   * dynamic exits, "exit <dir> <src;dest>", have a non-param direction
	   * token and were skipped by the !is_param check above.)  The previous
	   * code unconditionally consumed another token here, which mis-flagged
	   * every place exit as malformed. */
	  tok = param_contents(tok);
	  vector<string> args = split_param (tok);
	  if (args.size() != 2)
	    {
	      gi->debug_print ("Expected two arguments in " + tok);
	      continue;
	    }
	  if (args[0] != room) { report_unsupported ("exit source '" + args[0] + "' does not match room '" + room + "'"); continue; }
	  vector<string> tmp;
	  /* A created place lands in the same Places array the tags fill, so
	   * GetGoToExits gates it on the version in just the same way. */
	  string displayed = (asl_version_ >= 311) ? displayed_name (args[1])
						  : args[1];
	  tmp.push_back ("|b" + displayed + "|xb");
	  tmp.push_back (displayed);
	  tmp.push_back (displayed);
	  tmp.push_back (args[1]);
	  rv.push_back (tmp);
	}
      else if (tok == "destroy")
	{
	  tok = next_token (line, c1, c2);
	  if (tok != "exit") { report_unsupported ("expected 'exit' after 'destroy' in: " + line); continue; }
	  /* The rest of the record is the destination room's *name*, spaces and
	   * all -- "destroy exit Nickel Building" -- so it cannot be read with
	   * next_token, which stops at the first space.  Reading only "Nickel"
	   * matched nothing, and every "destroy exit <room; multi word place>"
	   * silently did nothing: "The Things That Go Bump In The Night"
	   * destroys four of them in its startscript, so the Nickel Building
	   * still listed the cargo door it had just shut (twice over, once the
	   * door opener created it again) and the padlocked Work Shed could be
	   * walked into before its chain was cut.  Quest matches the name in
	   * full, and case-sensitively: at 4.10 FindExit looks the place up in
	   * the room's places dictionary (V4Game.Part2.cs:7885-7891), and below
	   * that DestroyExit compares PlaceName with = (V4Game.cs:3632-3641). */
	  string dest_name = trim (line.substr (c2));

	  for (v2string::iterator j = rv.begin(); j != rv.end(); j ++)
	    if ((*j)[3] == dest_name)
	      {
		rv.erase(j);
		break;
	      }
	}

    }

  GEAS_DBG << "get_places (" << room << ") -> " << rv << "\n";
  return rv;
}

/* An exit in Quest 4.10 is an object like any other: RoomExit's constructor
 * appends a record to _objs with IsExit set (RoomExit.cs:16-27) and names it
 * "<room>.<direction>" for a compass exit, or "<room>.exitN" -- N counted per
 * room, from the room object's own quest.lastexitid property -- for a
 * directionless "place" (UpdateObjectName, RoomExit.cs:172-232).  The objects
 * are created as the room's exit tags are read, in source order, so that is the
 * order "for each exit in <room>" walks them.
 *
 * Two things follow from exits being ordinary object records, and both matter:
 *
 *  - a destroyed exit is still in the list.  DestroyExit hands the exit to
 *    RoomExits.RemoveExit, which only clears the object's Exists flag
 *    (RoomExits.cs:575-591), and ExecForEach filters on IsRoom/IsExit alone --
 *    it never looks at Exists (V4Game.cs:5029-5040).  Tim Hamilton's "The Things
 *    That Go Bump In The Night" is built on this: its game-block description
 *    counts the exits with "for each exit in <#quest.currentroom#> inc
 *    <exitcheck>" and prints "You can go nowhere." when the count is zero, and
 *    the one room it wants to say that about (the Nickel Building, before the
 *    cargo door opens) gets there with a "dec <exitcheck>" *inside* the same
 *    loop -- which only ever cancels out if the two exits it destroyed in the
 *    startscript are still being counted.
 *
 *  - re-creating a compass exit does not add a second object.  SetDirection
 *    reuses the existing RoomExit for a direction the room has already had
 *    (RoomExits.cs:20-33), with a comment saying as much, so "create exit north"
 *    after "destroy exit north" leaves the count alone.  A *place* exit is not
 *    reused -- AddPlaceExit builds a fresh RoomExit and merely marks the old one
 *    non-existent -- so repeatedly destroying and re-creating one place would
 *    keep growing Quest's list.  Nothing in the corpus does that, and geas keeps
 *    no history of destroyed places, so places are deduplicated by destination
 *    here. */
/* One of the eleven direction tags?  "out" counts: Quest gives it a Direction of
 * its own, so it is an exit object like the compass points and takes part in the
 * 4.10 folded exits line (RoomExits.AddExitFromTag, RoomExits.cs:57-115). */
static bool is_dir_name (const string &t)
{
  for (size_t i = 0; i < ARRAYSIZE (dir_names); i ++)
    if (t == dir_names[i])
      return true;
  return false;
}

static bool ci_contains (const vector<string> &v, const string &t)
{
  for (const string &i: v)
    if (ci_equal (i, t))
      return true;
  return false;
}

vector<string> geas_implementation::exit_dir_order (const string &room)
{
  vector<string> rv;
  const GeasBlock *gb = gf.find_by_name ("room", room);
  std::string::size_type c1, c2;
  if (gb != NULL)
    for (const string &line: gb->data)
      {
	string tok = first_token (line, c1, c2);
	if (is_dir_name (tok) && !ci_contains (rv, tok))
	  rv.push_back (tok);
      }
  /* "create exit <dir> <room; dest>" for a direction the room never declared
   * adds a new exit object, and so a new entry at the end. */
  for (const auto &e: state.exits)
    {
      if (!ci_equal (e.src, room))
	continue;
      string tok = first_token (e.dest, c1, c2);
      if (tok != "exit")
	continue;
      tok = next_token (e.dest, c1, c2);
      if (is_dir_name (tok) && !ci_contains (rv, tok))
	rv.push_back (tok);
    }
  return rv;
}

vector<string> geas_implementation::exit_object_names (const string &room)
{
  vector<string> rv;
  const GeasBlock *gb = gf.find_by_name ("room", room);
  /* Quest names the exit after the room object, which carries the spelling used
   * by "define room" rather than whatever casing a goto used. */
  string rname = (gb != NULL && gb->name != "") ? gb->name : room;

  vector<string> seen_dirs, seen_places;
  int place_id = 0;

  std::string::size_type c1, c2;
  if (gb != NULL)
    for (const string &line: gb->data)
      {
	string tok = first_token (line, c1, c2);
	if (is_dir_name (tok))
	  {
	    if (!ci_contains (seen_dirs, tok))
	      {
		seen_dirs.push_back (tok);
		rv.push_back (rname + "." + tok);
	      }
	  }
	else if (tok == "place")
	  {
	    bool locked;
	    tok = place_tag_param (line, c1, c2, asl_version_, locked);
	    if (!is_param (tok))
	      continue;
	    string dest, prefix;
	    split_exit_dest (eval_param (tok), dest, prefix);
	    if (dest != "" && !ci_contains (seen_places, dest))
	      {
		seen_places.push_back (dest);
		rv.push_back (rname + ".exit" + string_int (++place_id));
	      }
	  }
      }

  /* Then the exits made during play, in the order they were made.  "noexit" and
   * "destroy exit" records take nothing away: the object outlives both. */
  for (const auto &e: state.exits)
    {
      if (!ci_equal (e.src, room))
	continue;
      const string &line = e.dest;
      string tok = first_token (line, c1, c2);
      if (tok != "exit")
	continue;
      tok = next_token (line, c1, c2);
      if (is_dir_name (tok))
	{
	  if (!ci_contains (seen_dirs, tok))
	    {
	      seen_dirs.push_back (tok);
	      rv.push_back (rname + "." + tok);
	    }
	  continue;
	}
      /* Directionless: "exit <src; dest>", so the token after "exit" is already
       * the parameter. */
      if (!is_param (tok))
	continue;
      vector<string> p = split_param (param_contents (tok));
      if (p.size () != 2)
	continue;
      if (!ci_contains (seen_places, p[1]))
	{
	  seen_places.push_back (p[1]);
	  rv.push_back (rname + ".exit" + string_int (++place_id));
	}
    }

  return rv;
}

string geas_implementation::exit_dest (const string &room, const string &dir, bool *is_script) const
{
  std::string::size_type c1, c2;
  string tok;
  if (is_script != NULL)
    *is_script = false;

  string dyn_dest;
  bool have_dyn = false;
  for (auto i = state.exits.rbegin(); i != state.exits.rend(); i++)
    if (i->src == room)
      {
	const string &line = i->dest;
	GEAS_DBG << "Processing exit line '" << i->dest << "'\n";
	tok = first_token (line, c1, c2);
	GEAS_DBG << "   first tok is " << tok << " (vs. exit)\n";
	/* A "noexit <dir>" record (pushed by disconnect) removes that exit;
	 * the latest record for the direction wins over an earlier create-exit
	 * or the static room exit below. */
	if (tok == "noexit")
	  {
	    if (next_token (line, c1, c2) == dir)
	      return "";
	    continue;
	  }
	/* From ASL 4.10 on, "destroy exit <room; north>" takes a *directional*
	 * exit away too, not just a "go to" place: DestroyExit hands the exit to
	 * RoomExits.RemoveExit, which clears the exit object's Exists flag
	 * whichever kind it is (V4Game.cs:3595-3606, RoomExits.cs:575-591).
	 * Before 4.10 the same statement only ever searched the room's *places*
	 * list, so a direction name matched nothing and the statement did nothing
	 * at all (V4Game.cs:3608-3654) -- hence the version test.  geas only ever
	 * read these records in get_places (), so a destroyed direction stayed
	 * walkable and stayed in the "You can go ..." list.  Tim Hamilton's "The
	 * Maze" is the game that cares: its one room is a 15x20 hedge maze held in
	 * four strings of y/n, and every move runs a procedure that destroys and
	 * re-creates the four compass exits to match the walls around the new
	 * square.  Without this the maze has no walls at all.  A place exit's
	 * record names a room rather than a direction, so it falls through to
	 * get_places (). */
	if (tok == "destroy")
	  {
	    if (asl_version_ >= 410
		&& next_token (line, c1, c2) == "exit"
		&& ci_equal (next_token (line, c1, c2), dir))
	      return "";
	    continue;
	  }
	if (tok != "exit")
	  continue;
	tok = next_token (line, c1, c2);
	GEAS_DBG << "   second tok is " << tok << " (vs. " << dir << ")\n";
	if (tok != dir)
	  continue;
	tok = next_token (line, c1, c2);
	GEAS_DBG << "   third tok is " << tok << " (expecting parameter)\n";
	if (!is_param (tok)) { report_unsupported ("malformed exit destination: " + line); continue; }
	vector<string> p = split_param (param_contents(tok));
	if (p.size() != 2) { report_unsupported ("unexpected exit parameters in: " + line); continue; }
	if (!ci_equal (p[0], room)) { report_unsupported ("exit source mismatch in: " + line); continue; }
	dyn_dest = p[1];
	have_dyn = true;
	break;
      }

  const GeasBlock *gb = gf.find_by_name ("room", room);
  if (gb == NULL)
    {
      if (!have_dyn)
	gi->debug_print (string ("Trying to find exit <") + dir +
			 "> of nonexistent room <" + room + ">.");
      return dyn_dest;
    }

  /* From ASL 4.10 on a direction's exit is one long-lived object whose script is
   * set once and never cleared: "create exit <dir> <room; dest>" reuses the
   * existing object (SetDirection, RoomExits.cs:20-33), assigns the new
   * destination, and only calls SetScript if the new tag brings a script of its
   * own -- and Go checks for a script before the destination
   * (RoomExit.cs:240-259).  So a declared script exit survives being destroyed
   * and re-created with a plain destination, and still runs.  That is precisely
   * what "The Maze" relies on: its four exits are scripts that move the player
   * by adjusting %xpos% and %ypos%, and the re-creating procedure passes them
   * "<Maze; Maze>", which would otherwise re-enter the room with the coordinates
   * untouched.
   *
   * Before 4.10 there are no exit objects: "create exit north <room; dest>"
   * assigns the room's North field directly and stamps its type back to Text
   * (V4Game.cs:5429-5450), wiping whatever script the block declared.  Half a
   * dozen games in the corpus depend on that -- "Bear Campsite" (ASL 400)
   * declares `south { msg <a Grizzly Bear blocks your path> ... }` and opens the
   * way out with `create exit south <Campsite; Freedom>` once the bear has
   * choked on the fish -- so the dynamic destination has to win there. */
  bool declared_is_script = false;
  string declared = declared_exit_dest (gb, dir, declared_is_script);
  if (declared_is_script && (asl_version_ >= 410 || !have_dyn))
    {
      if (is_script != NULL)
	*is_script = true;
      return declared;
    }
  if (have_dyn)
    return dyn_dest;
  return declared;
}

/* The LAST declaration of a direction wins: the pre-4.10 loader assigns the
 * room's field afresh on every "north " line (V4Game.Part2.cs:1042-1044),
 * and 4.10's AddExitFromTag re-uses and re-assigns the direction's existing
 * exit object (SetDirection, RoomExits.cs:20-33), so a later line overwrites
 * an earlier one either way.
 *
 * ...among the lines Quest reads at all: BeginsWith ("south ") needs the
 * trailing space, so a *bare* direction line assigns nothing and must be
 * skipped here too -- Lovesong's hall carries three stray "south" lines
 * under its real "south <yard>", and taking the last of those would wall
 * the house off. */
static const string *last_dir_decl (const GeasBlock *gb, const string &dir)
{
  std::string::size_type c1, c2;
  const string *decl = NULL;
  for (const string &l: gb->data)
    if (first_token (l, c1, c2) == dir && next_token (l, c1, c2) != "")
      decl = &l;
  return decl;
}

string geas_implementation::declared_exit_dest (const GeasBlock *gb,
						const string &dir,
						bool &is_script) const
{
  std::string::size_type c1, c2;
  is_script = false;
  const string *decl = last_dir_decl (gb, dir);
  if (decl == NULL)
    return "";
  const string &line = *decl;
  first_token (line, c1, c2);   /* just to set c2, the end of the keyword */
  std::string::size_type line_start = c2;
  string tok = next_token (line, c1, c2);
  /* Before 4.10 "out" is not read like the other directions.  Every other
   * direction goes through GetTextOrScript, which decides between the two by
   * looking at the line; "out" is cut in two at its first '>' and both
   * halves are kept -- Out.Text is GetParameter, the contents of the first
   * <...>, and Out.Script is everything after that '>'
   * (V4Game.Part2.cs:1018-1030).  So the parameter is always a destination,
   * whatever keyword stands in front of it: "out msg <You need to get up
   * first.>" leaves Out.Script empty, and GoDirection then hands the message
   * to PlayGame as a room name (ibid. 6324-6356), which prints nothing
   * because no room is called that.  UpdateDoorways advertises Out.Text
   * either way (ibid. 7218-7233), so the room still says "You can go out to
   * You need to get up first..".  The script half is the caller's business.
   *
   * From 4.10 on AddExitFromTag parses "out" with all the rest and a leading
   * keyword really does make the tag a script. */
  if (asl_version_ < 410 && ci_equal (dir, "out"))
    {
      while (tok != "" && !is_param (tok))
	tok = next_token (line, c1, c2);
      return is_param (tok) ? param_contents (tok) : "";
    }
  /* "<dir> locked <dest; lockmessage>": a (initially locked) exit whose
   * destination is the first ;-separated field.  Locking only gates
   * traversal (handled by the caller), so return the destination here.
   *
   * A locked exit that carries a *script* is different: it has no
   * destination at all, and its parameter -- if it has one -- is the lock
   * message (RoomExits.cs:186-191).  Reading that field as a room name
   * would send the player to a room named after the refusal text. */
  if (ci_equal (tok, "locked")) {
    std::string::size_type after_lock = c2;
    tok = next_token (line, c1, c2);
    if (is_param (tok)) {
      string rest = trim (line.substr (c2));
      if (rest != "") {
	is_script = true;
	return rest;
      }
      vector<string> p = split_param (param_contents (tok));
      if (!p.empty ())
	return trim (p[0]);
      return "";
    }
    if (tok != "") {
      is_script = true;
      return trim (line.substr (after_lock));
    }
    return "";
  }
  if (is_param (tok))
    return param_contents(tok);
  if (tok != "")
    {
      is_script = true;
      return trim (line.substr (line_start + 1));
    }
  return "";
}

/* Split the parameter of a directional exit -- "north <...>", and "out <...>" --
 * into the room it leads to and the prefix printed ahead of that room's name.
 *
 * The two ASL generations read a two-field parameter the other way round.  Up to
 * 3.53 GoDirection enters everything *after* the semicolon, so "out <the; town>"
 * means prefix "the" plus destination "town" (V4Game.Part2.cs:6319-6326).  From
 * 4.10 on the exit tag is parsed by AddExitFromTag, which takes the *first*
 * field as the destination and the second as a lock message -- inert unless the
 * tag also says "locked" (RoomExits.cs:186-191); a prefix, if any, comes from
 * the destination room's own "prefix" line instead (RoomExit.cs:231). */
void geas_implementation::split_exit_dest (const string &raw, string &dest,
					   string &prefix) const
{
  prefix = "";
  std::string::size_type i = raw.find (';');
  if (i == string::npos)
    {
      dest = trim (raw);
      return;
    }
  if (asl_version_ >= 410)
    {
      dest = trim (raw.substr (0, i));
      return;
    }
  prefix = trim (raw.substr (0, i));
  dest = trim (raw.substr (i + 1));
}

/* The lockmessage declared in "<dir> locked <dest; lockmessage>", or "". */
bool geas_implementation::exit_declared_locked (const string &room, const string &dir,
						string &message) const
{
  message = "";
  const GeasBlock *gb = gf.find_by_name ("room", room);
  if (gb == NULL)
    return false;
  std::string::size_type c1, c2;
  /* The last declaration wins, skipping bare direction lines -- see
   * last_dir_decl. */
  const string *decl = last_dir_decl (gb, dir);
  if (decl != NULL) {
    const string &line = *decl;
    first_token (line, c1, c2);
    if (!ci_equal (next_token (line, c1, c2), "locked"))
      return false;
    string tok = next_token (line, c1, c2);
    if (is_param (tok)) {
      vector<string> p = split_param (param_contents (tok));
      /* A directional exit that runs a script has no destination to spend the
       * first field on, so the whole parameter is the lock message
       * (RoomExits.cs:186-191).  A *place* exit keeps its destination there even
       * when scripted -- see the place scan below. */
      if (trim (line.substr (c2)) != "") {
	if (!p.empty ())
	  message = trim (p[0]);
      }
      else if (p.size () >= 2)
	message = trim (p[1]);
    }
    return true;
  }

  /* A directionless exit has no direction word to key on, so it answers to its
   * destination room's name, which is what FindExit looks a place up by
   * (V4Game.Part2.cs:7877-7913).  Only from 4.10: below it a room's places are
   * plain records with nothing to lock, and "lock" cannot reach them either. */
  if (asl_version_ >= 410)
    {
      bool found = false;
      for (const string &l: gb->data)
	{
	  if (first_token (l, c1, c2) != "place")
	    continue;
	  bool lk;
	  string tok = place_tag_param (l, c1, c2, asl_version_, lk);
	  if (!is_param (tok))
	    continue;
	  vector<string> p = split_param (param_contents (tok));
	  if (p.empty () || !ci_equal (trim (p[0]), dir))
	    continue;
	  /* The last declaration wins, as above. */
	  found = lk;
	  message = (lk && p.size () >= 2) ? trim (p[1]) : "";
	}
      if (found)
	return true;
    }
  return false;
}

/* The lockmessage declared in "<dir> locked <dest; lockmessage>", or "". */
string geas_implementation::exit_lock_message (const string &room, const string &dir) const
{
  string message;
  exit_declared_locked (room, dir, message);
  return message;
}

/* Is the (room, dir) exit currently locked?  An explicit lock/unlock wins
 * (stored as a "<room>;<dir>=locked|open" property on "!exitlock"); otherwise
 * it is locked iff the room declares it with the "locked" keyword. */
bool geas_implementation::exit_locked (const string &room, const string &dir)
{
  string val;
  if (get_obj_property ("!exitlock", lcase (room) + ";" + lcase (dir), val))
    return ci_equal (val, "locked");
  string message;
  return exit_declared_locked (room, dir, message);
}

/* The right-hand side of a `set numeric <var; ...>`, evaluated as the game's own
 * ASL version does it.  From 3.91 ExecSetVar hands the text to
 * ExpressionHandler, which is the full precedence-respecting evaluator geas has
 * always used; below that it is one binary operation and nothing more
 * (V4Game.cs:7272-7343 -- see eval_double_pre391), so Kingdom's `set numeric
 * <x; 10*3/4>` is thirty there and seven and a half here. */
double geas_implementation::eval_set_numeric (const string &expr)
{
  if (asl_version_ >= 391)
    return eval_double (expr);
  /* ExecSetVar trims the text before it looks for the operator, and the search
   * for a "-" starts at the second character: with the space that follows the
   * semicolon still on the front, " -8-3" would split at the minus sign of the
   * negative number and answer -8 instead of -11. */
  bool div_by_zero = false;
  double rv = eval_double_pre391 (trim (expr), div_by_zero);
  if (div_by_zero)
    gi->debug_print ("Division by zero - The result of this operation has been "
		     "set to zero.");
  return rv;
}

void geas_implementation::look()
{
  string tmp;
  bool described = false;
  /* The look text is read fresh at every room display, not cached from the last
   * move: ShowRoomInfo asks GetObjectProperty("look", ...) each time it runs and
   * writes quest.lookdesc from it, falling back to the room's own tag
   * (V4Game.Part2.cs:3879-3893).  geas only refreshed it on arrival, so a script
   * that did `property <room; look=...>` while the player stood there left the
   * old text on screen until they walked out and back in -- Beam's !intproc116
   * rewrites the Inside Machine that way.  Quest computes it here, before the
   * description tag is found, so a description that changes the property still
   * gets the text as it was when the display began.
   *
   * The room's displayed name is the same story: ShowRoomInfo assembles alias,
   * prefix and suffix a few hundred lines above that (ibid. 3709-3730), so a
   * `property <room; alias=...>` shows on the very next display too. */
  regen_var_room ();
  regen_var_look ();
  /* The objects pair too, and eagerly, because this is the one writer that
   * punctuates: ShowRoomInfo fills quest.objects and quest.formatobjects a
   * couple of hundred lines above the point where it looks for a description
   * tag (V4Game.Part2.cs:3799-3830 against 3924-3956), so a described room sets
   * them just as an undescribed one does, and a description that prints
   * "#quest.formatobjects#" without disturbing anything first gets the " and ".
   * One that does disturb something -- a `show', a `create' -- raises the dirty
   * flag again and the next read rebuilds the pane's comma list, which is what
   * Quest's UpdateObjectList would have left behind. */
  regen_var_objects (true);
  if (get_room_action (state.location, "description", tmp))
    { run_script_as (state.location, tmp); described = true; }
  else if (get_room_property (state.location, "description", tmp))
    { print_formatted (tmp); described = true; }
  else
    {
      /* A room with no description tag of its own falls back to a "description"
       * line in the game block, which stands in for every such room
       * (V4Game.Part2.cs:3899-3921).  It is read straight out of the block: a
       * parameter is text, anything else is a script.  This cannot go through
       * get_obj_action/get_obj_property, because the game block's lines are not
       * rewritten into "action <...>"/"properties <...>" form the way a room's
       * are (readfile.cc:415-420 lists no reserved words for the game pass), so
       * those lookups always came back empty.  Tim Hamilton's "The Maze" is
       * built entirely on this: it has one room, and the game-block description
       * is a `select case <%xpos%>` that prints "Entrance to maze", "Center of
       * maze", "Exit" or plain "Hedge Maze" from the player's coordinates. */
      const GeasBlock *game = gf.find_by_name ("game", "game");
      if (game != NULL)
	{
	  std::string::size_type c1, c2;
	  for (const string &line: game->data)
	    if (first_token (line, c1, c2) == "description")
	      {
		string rest = trim (line.substr (c2));
		if (is_param (rest))
		  print_formatted (param_contents (rest));
		else
		  run_script_as ("game", rest);
		described = true;
		break;
	      }
	}
    }

  if (!described)
    {
      string in_desc;
      bool has_in_desc = get_room_property (state.location, "indescription",
					    tmp);
      /* From ASL 3.50 the loader mirrors a room's tags into the object-property
       * table as well as into the room record, and AddToObjectProperties trims
       * whatever follows the "=" (V4Game.cs:4025) before writing it back over
       * InDescription (V4Game.cs:4111-4118).  So from 3.50 on the stored value
       * has no outer whitespace, and one made of nothing but spaces is stored
       * as the empty string.  TARDIS Escape's Console Room writes
       * `indescription < >` and depends on that without meaning to. */
      if (has_in_desc && asl_version_ >= 350)
	tmp = trim (tmp);
      /* What Quest tests is whether the stored text is empty, not whether the
       * tag was there (ShowRoomInfo, V4Game.Part2.cs:3746), so an `indescription
       * <>` takes the default line below. */
      if (has_in_desc && !tmp.empty ())
	{
	  /* Quest's indescription: a trailing colon is replaced by a space, the
	   * room name and a full stop ("...west end of the:" becomes "...west
	   * end of the Entrance Hall.").  Without a trailing colon the text is
	   * printed verbatim, with no room name appended. */
	  in_desc = tmp;
	  if (!in_desc.empty () && in_desc[in_desc.size () - 1] == ':')
	    print_formatted (in_desc.substr (0, in_desc.size () - 1) + " "
			     + get_svar ("quest.formatroom") + ".");
	  else
	    print_formatted (in_desc);
	}
      else
	/* The sentence ends in a full stop: "You are in " + roomDisplayName +
	 * "." (ShowRoomInfo, V4Game.Part2.cs:3764; the pre-2.80 display builds
	 * the same line at 2050). */
	print_formatted (string ("You are in") + " "
			 + get_svar ("quest.formatroom") + ".");
    }

  /* The object list and the exit lines belong to the *default* room display, and
   * a description tag replaces that display whole: ShowRoomInfo builds "You are
   * in ...", the object list and the three exit lines into one roomDisplayText
   * and prints it only when no description tag was found, then runs the tag
   * instead (V4Game.Part2.cs:3924-3956; the pre-2.80 path does the same at
   * 2088-2117).  A description that wants any of it prints it itself, which is
   * exactly what "Barbarian" and "The Maze" do -- both end their description
   * with `msg <You can go #quest.doorways#.>`, and geas adding its own copy gave
   * them the line twice.
   *
   * quest.formatobjects and the quest.doorways family are readable by game
   * script whether or not this prints them; the objects pair was regenerated
   * above, before the description tag ran. */
  if (!described)
    {
      /* The original Quest runner printed both of these inline in the main text
       * ("There is a key here.", "You can go north, south, east or west.") *and*
       * mirrored them in its objects and compass panes -- the panes were
       * clickable shortcuts, not a replacement for the lines.  So print them in
       * the main window even on a host that provides such panes (see
       * has_objects_window / get_room_exits); a pane stays a supplementary
       * copy. */
      tmp = get_svar ("quest.formatobjects");
      if (asl_version_ < 280)
	print_formatted (characters_display_);
      if (tmp != "")
	print_eval ("There is #quest.formatobjects# here.");

      /* The exits, after the objects.  From ASL 4.10 they are one folded line:
       * UpdateDoorways hands the whole job to
       * RoomExits.GetAvailableDirectionsDescription, which lists compass
       * exits (with "out" among them as a bare word) and places together in
       * the order the room's tags created them (V4Game.Part2.cs:7207-7211,
       * RoomExits.cs:304-353).  quest.doorways holds the list without the
       * sentence around it. */
      if (asl_version_ >= 410)
	{
	  if ((tmp = get_svar ("quest.doorways")) != "")
	    print_formatted ("You can go " + tmp + ".");
	}
      else
	{
	  /* From 2.80 the places line is part of roomDisplayText and the
	   * out/directions string is printed after it, so places come first
	   * (V4Game.Part2.cs:3838-3874, 3948-3955).  Below 2.80 the three are
	   * all part of roomDisplayText, in the other order (ShowRoomInfoV2,
	   * ibid. 2022, 2056, 2092) -- which is the order geas used at every
	   * version.
	   *
	   * The out and directions sentences are also one *line* from 2.80 on,
	   * not two: UpdateDoorways returns them as a single string with a space
	   * between (V4Game.Part2.cs:7308-7312, 7353) and ShowRoomInfo prints
	   * that string with one Print (ibid. 3951-3954).  Below 2.80 each ends
	   * in its own vbCrLf, so there they really are separate lines (ibid.
	   * 2022, 2056).  2.80 bolds the word "out" too, where the older path
	   * leaves it plain; both bold the directions, which is where
	   * quest.doorways.dirs already carries its own markup. */
	  bool places_first = (asl_version_ >= 280);
	  if (places_first && (tmp = get_svar ("quest.doorways.places")) != "")
	    print_formatted ("You can go to " + tmp + ".");
	  /* The *display* form of "out", so an "out <the; town>" prefix shows
	   * up here ("You can go out to the town."), as it does in the
	   * typelib's own version of this line and in the "places" line. */
	  string out_line = "", dirs_line = "";
	  if (get_svar ("quest.doorways.out") != ""
	      && (tmp = get_svar ("quest.doorways.out.display")) != "")
	    out_line = places_first ? "You can go |bout|xb to " + tmp + "."
				    : "You can go out to " + tmp + ".";
	  if ((tmp = get_svar ("quest.doorways.dirs")) != "")
	    dirs_line = "You can go " + tmp + ".";
	  if (places_first && out_line != "" && dirs_line != "")
	    print_formatted (out_line + " " + dirs_line);
	  else
	    {
	      if (out_line != "")
		print_formatted (out_line);
	      if (dirs_line != "")
		print_formatted (dirs_line);
	    }
	  if (!places_first && (tmp = get_svar ("quest.doorways.places")) != "")
	    print_formatted ("You can go to " + tmp + ".");
	}
    }

  /* ...and the punctuated pair lives no longer than the display that made it.
   * ShowRoomInfo's last act, after the default display has been printed or the
   * description tag has run, is a call to UpdateObjectList
   * (V4Game.Part2.cs:3974; ShowRoomInfoV2 does the same at 2156), which rebuilds
   * both strings as the pane's flat comma list.  So the " and " is readable from
   * inside a description tag and from nowhere else: a game that reads
   * quest.formatobjects on any later turn gets the comma list, even though
   * nothing has moved since.  Raising the flag rather than regenerating here
   * keeps that off the cost of walking into a room. */
  objs_vars_dirty_ = true;

  /* The room's "look" text is the one part of this that a description tag does
   * not always replace.  Below ASL 3.10 it is printed after the tag as well as
   * after the default display, and only from 3.10 on is a described room left to
   * print it itself: showLookText starts true and is cleared by
   * `descTagExist And ASLVersion >= 310` (V4Game.Part2.cs:3881-3927, 3958-3963).
   *
   * Below 2.80 there is no quest.lookdesc to read it back out of -- see
   * regen_var_look -- so the text comes from the copy that made. */
  if ((!described || asl_version_ < 310)
      && (tmp = (asl_version_ < 280) ? look_desc_
				     : get_svar ("quest.lookdesc")) != "")
    print_formatted (tmp);
}

/* Display one picture.  See the declaration for which tags land here with
 * `popup' set. */
void geas_implementation::show_picture (const string &spec, bool popup)
{
  string file = spec, res, caption;

  if (popup)
    {
      /* ShowPicture (V4Game.Part2.cs:3664-3690) takes the caption off first
       * and then looks for the size in what is left, so
       * "map.jpg@640x480; The Shire" yields all three -- and a ';' anywhere
       * after an '@' belongs to the caption, not to the size. */
      std::string::size_type semi = file.find (';');
      if (semi != string::npos)
	{
	  caption = trim (file.substr (semi + 1));
	  file = trim (file.substr (0, semi));
	}
      std::string::size_type at = file.find ('@');
      if (at != string::npos)
	{
	  res = trim (file.substr (at + 1));
	  file = trim (file.substr (0, at));
	}
    }

  /* Quest 4 used the caption as the title bar of the picture window rather
   * than as game text -- "If a caption is specified, that caption is used for
   * the window title, rather than the default 'Picture'" (Quest 4.1.5's own
   * quest.chm, script-script.htm).  Neither QuestViva nor geas has a window to
   * title: both draw the image among the text, so the only place left for the
   * author's caption is with it.  QuestViva prints it ahead of the image
   * (ibid. 3686) and geas follows, so that the caption survives a host that
   * cannot show pictures at all -- which is also what keeps the two
   * transcripts together. */
  if (caption != "")
    print_formatted (caption);
  gi->show_image (file, res, caption);
}
