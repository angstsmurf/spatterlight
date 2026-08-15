using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;
using QuestViva.Common;
using QuestViva.Legacy;

// qv4 <game.asl|game.cas> <command-script.txt>
//
// Quest 4 ground-truth oracle: replays one of ../../goldens/"<title> - command
// script.txt" through QuestViva's V4Game -- which *is* Quest 4, a line-by-line
// C# translation of Axe's VB6 engine -- and prints a transcript shaped like the
// one geas_walkthrough_runner prints, so the two can be diffed.
//
// The point is to find geas bugs, so everything here bends QuestViva's output
// towards geas's, never the other way round, and every bend is a place where
// the two engines agree but their *harnesses* disagree:
//
//   * menus.  geas's make_choice prints the caption, then "N) option" for each
//     option, then "[choice] N"; V4Game prints "- caption" before the menu and
//     "- <chosen text>" after it, and leaves the list to the UI.  We drop
//     V4Game's two lines and print geas's block in their place.
//   * `ask`.  geas renders it as a two-option Yes/No make_choice, so a script
//     answers it with 1 or 2; V4Game hands the caption to the UI and prints
//     nothing.  Same substitution.
//   * `wait`.  V4Game prints "Press a key to continue..." (V4Game.cs:6115)
//     when the statement carries no message of its own; geas prints nothing
//     for a bare `wait` in any frontend.  Dropped, because it would report
//     finding 2 once per `wait` rather than once.  A `wait <message>` is a
//     different matter: the author's text is kept here and printed by the
//     runner too (geas_walkthrough_runner.cc:139-148), so the two sides meet.
//
// Options:
//   --tick        Tick timers once per turn, matching the runner's --tick
//                 (geas's tick_timers counts down one second per turn).
//   --echo        (accepted and ignored; the transcript always goes to stdout)
//
// Environment: QVH_SEED seeds the patched-in ErkyrathRandomV4 (see
// ../../../quest5/harness/oracle/patch_questviva.py section 8) so that
// $rand(a;b)$ draws the same numbers, in the same order, as geas's
// erkyrath_random().  The corpus comparison runs QVH_SEED=1 against GEAS_SEED=1.

// Quest 4 was VB6, where Str() and Val() are both invariant: a number always
// round-trips through a "." decimal point.  V4Game stores numeric variables by
// calling double.ToString() (V4Game.Part2.cs:521) and reads them back with the
// still-invariant Conversion.Val, so under any culture with a decimal comma
// every fractional number is silently truncated on the way in -- `set numeric
// <x; 3.5>` reads back as 3.  That is a QuestViva translation slip rather than
// Quest behaviour, and it would otherwise be blamed on geas, so pin the
// invariant culture for the whole process.
CultureInfo.DefaultThreadCurrentCulture = CultureInfo.InvariantCulture;
CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;

var tick = false;
var pos = new List<string>();
foreach (var a in args)
{
    if (a == "--tick") tick = true;
    else if (a == "--echo") { /* always echoing */ }
    else if (a.StartsWith("--")) { Console.Error.WriteLine($"unknown option {a}"); return 2; }
    else pos.Add(a);
}
if (pos.Count < 2)
{
    Console.Error.WriteLine("usage: qv4 [--tick] <game> <command-script>");
    return 2;
}

var transcript = new StringBuilder();
var finished = false;
var errorCount = 0;

void Line(string s) => transcript.Append(s).Append('\n');

// Set while a menu answer is being resolved, to swallow the "- <chosen text>"
// line V4Game prints once the choice comes back (its counterpart to the
// "[choice] N" we print instead).
string? swallowNext = null;

// QV4_TRACE=1 streams the transcript and each script step to stderr as they
// happen; the transcript itself is only written at the end, so this is the way
// to see where a game that never returns got to.
var trace = Environment.GetEnvironmentVariable("QV4_TRACE") is { Length: > 0 } tv && tv != "0";

void Emit(string html)
{
    // A string ending in `|xn` is printed with no line break after it, so the
    // next Print continues the same paragraph -- The Devil's Bargain builds
    // most of its room descriptions that way.  TextFormatter has already eaten
    // the tag by the time we see the text and left the flag on the wrapper
    // element instead (TextFormatter.cs:29-33, 196), which is the only place
    // the signal survives.
    var nobr = html.StartsWith("<output nobr=\"true\"", StringComparison.Ordinal);
    var t = Strip(html);
    if (trace) Console.Error.WriteLine("[out] " + t.Replace("\n", "\\n"));
    if (swallowNext != null && t.TrimEnd() == swallowNext) { swallowNext = null; return; }
    swallowNext = null;
    // V4Game.cs:6115 prints this for a `wait` with no message of its own; geas
    // prints nothing there, in any frontend, which is finding 2 -- reported
    // once, not once per `wait`.  It goes out as "|nPress a key to
    // continue...", and that leading newline is not part of the prompt: after
    // a `|xn` line it is what ends the paragraph, which geas's bare wait --
    // printing nothing at all -- still does.
    if (t.Trim() == "Press a key to continue...")
    {
        if (transcript.Length > 0 && transcript[^1] != '\n') transcript.Append('\n');
        return;
    }
    transcript.Append(t);
    if (!nobr && !t.EndsWith('\n')) transcript.Append('\n');
}

// Drop a line V4Game already printed that we are about to print in geas's
// shape (the "- caption" a menu emits before ShowMenu).
void Unprint(string text)
{
    // Trailing blanks are dropped from both sides: V4Game writes the caption
    // line as "- " + caption, so a menu with no caption at all -- Ponyville
    // opens with two of them -- emits a lone "- " that an exact match would
    // miss and leave in the transcript as a stray "-".
    var want = text.TrimEnd();
    var s = transcript.ToString();
    var end = s.Length;
    if (end > 0 && s[end - 1] == '\n') end--;
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t')) end--;
    var start = end - want.Length;
    if (start >= 0 && string.CompareOrdinal(s, start, want, 0, want.Length) == 0)
        transcript.Length = start;
}

var provider = new FileDirectoryGameDataProvider(pos[0]);
var gameData = await provider.GetData();
if (gameData == null) { Console.Error.WriteLine("[fatal] cannot read game"); return 3; }

var game = new V4Game(gameData, null!);
game.PrintText += Emit;
game.LogError += ex => { errorCount++; Console.Error.WriteLine("[error] " + ex.Message); };
game.Finished += () => finished = true;

// SetQuestionResponse is an explicit interface implementation on V4Game, so
// the driver drives the game through IGame throughout.
var igame = (IGame)game;

var player = new HeadlessPlayer(Emit);
if (!await igame.Initialise(player))
{
    Console.Error.WriteLine("[fatal] game failed to initialise");
    return 3;
}

var queue = ReadScript(pos[1]);

if (trace) Console.Error.WriteLine("[step] initialised");

// Begin() returns when the opening turn parks -- on a `wait`, a `choose`, an
// `ask`, or an `enter` reading the player's name -- rather than running to
// completion, because patch_questviva.py section 10 gives it the same
// turn-suspension handshake every other entry point already had.  Upstream it
// awaits DoBeginAsync straight through and any game that parks in its intro
// deadlocks a headless driver.
await game.Begin();
if (trace) Console.Error.WriteLine("[step] begun");
await Settle();
if (trace) Console.Error.WriteLine("[step] settled");

var turns = 0;
// geas's make_choice answers a menu the script ran out of lines for with option
// 1 rather than stopping (geas_walkthrough_runner.cc:173-186), and counts the
// starved reads so a menu that keeps re-asking cannot spin.  Mirror both, or a
// walkthrough whose last command opens a menu -- Attempted Assassination's duel
// -- ends one line into the menu here and wins there.
var starved = 0;
const int maxStarved = 200;
while (!finished && (queue.Count > 0 || player.PendingMenu is not null
                                     || player.PendingQuestion is not null))
{
    if (player.PendingMenu is { } menu)
    {
        player.PendingMenu = null;
        var answer = Answer();
        var (key, text) = ResolveMenu(menu, answer ?? "1");
        Unprint("- " + menu.Caption);
        Line(menu.Caption);
        var n = 1;
        foreach (var kv in menu.Options) Line($"{n++}) {kv.Value}");
        // geas only echoes the choice it actually read from the script.
        if (answer is not null) Line("[choice] " + answer);
        // TrimEnd to match the comparison in Emit: a `choice <Buy a Cup of
        // Coffee >` keeps its trailing space here but not in the printed line,
        // and an untrimmed expectation would never match.
        swallowNext = ("- " + text).TrimEnd();
        await game.SetMenuResponse(key);
    }
    else if (player.PendingQuestion is { } question)
    {
        player.PendingQuestion = null;
        var answer = Answer();
        // geas: choose_yes_no is make_choice (question, {"Yes", "No"}), read
        // with atoi and clamped, so 1 (or anything unparseable) is Yes.
        var yes = !int.TryParse(answer, out var qn) || qn <= 1;
        Line(question);
        Line("1) Yes");
        Line("2) No");
        if (answer is not null) Line("[choice] " + answer);
        await igame.SetQuestionResponse(yes);
    }
    else
    {
        var cmd = queue.Dequeue();
        starved = 0;
        turns++;
        if (trace) Console.Error.WriteLine("[cmd] " + cmd);
        // V4Game echoes "> cmd" itself (ExecCommand, V4Game.Part2.cs:4139), as
        // geas's run_command does, so neither is echoed here.
        if (tick) await game.SendCommand(cmd, 1, null);
        else await game.SendCommand(cmd);
    }
    await Settle();
}

Console.Error.WriteLine($"[diag] turns={turns} leftover={queue.Count} " +
    $"finished={finished} errors={errorCount}");
Console.Out.Write(Normalise(transcript.ToString()));
return 0;

// Auto-continue `wait` / `pause`, which consume no script line in either
// engine (geas's wait_keypress and pause both return immediately).
async Task Settle()
{
    var guard = 0;
    while (!finished && (player.IsWaiting || player.IsPausing) && guard++ < 10000)
    {
        if (player.IsWaiting) { player.IsWaiting = false; await game.FinishWait(); }
        else { player.IsPausing = false; await game.FinishPause(); }
    }
}

// The next script line for a menu or question, or null once the script has run
// out -- in which case the caller takes geas's default of option 1.
string? Answer()
{
    if (queue.Count > 0) { starved = 0; return queue.Dequeue(); }
    if (++starved > maxStarved) finished = true;
    return null;
}

// A menu answer is a 1-based option number, the way geas's make_choice reads it
// (atoi, then clamped into range).  V4Game keys its options by ASL line number,
// so the number has to be turned into a position.
static (string key, string text) ResolveMenu(MenuData menu, string answer)
{
    var n = int.TryParse(answer, out var v) ? v : 1;
    if (n < 1) n = 1;
    if (n > menu.Options.Count) n = menu.Options.Count;
    var kv = menu.Options.ElementAt(n - 1);
    return (kv.Key, kv.Value);
}

// Same script reader as geas_walkthrough_runner: skip a prose header up to a
// "Start:" line, drop blank lines, strip a trailing "  (note)" annotation.
//
// The command script has to arrive in the same character set the game text
// did.  V4Game reads .cas and .asl with Encoding.GetEncoding(1252), and geas
// passes the script's bytes through untouched, so a Windows-1252 script -- the
// Spanish Nearco types `use hueso on pañuelo` -- has to be decoded as 1252
// here as well, or the ñ turns into U+FFFD and Disambiguate never finds the
// object.  Only Nearco is not valid UTF-8, so try that first and fall back.
static Queue<string> ReadScript(string path)
{
    Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
    var bytes = File.ReadAllBytes(path);
    string text;
    try
    {
        text = new UTF8Encoding(false, true).GetString(bytes);
    }
    catch (DecoderFallbackException)
    {
        text = Encoding.GetEncoding(1252).GetString(bytes);
    }
    var raw = text.Replace("\r\n", "\n").Split('\n');
    var begin = 0;
    for (var i = 0; i < raw.Length; i++)
        if (raw[i].Trim().StartsWith("start:", StringComparison.OrdinalIgnoreCase))
        { begin = i + 1; break; }

    var q = new Queue<string>();
    for (var i = begin; i < raw.Length; i++)
    {
        var t = raw[i].Trim();
        var p = t.IndexOf("  (", StringComparison.Ordinal);
        if (p >= 0) t = t[..p].Trim();
        if (t.Length > 0) q.Enqueue(t);
    }
    return q;
}

static string Strip(string s)
{
    s = Regex.Replace(s, "<br\\s*/?>", "\n");
    s = Regex.Replace(s, "<[^>]+>", "");
    return System.Net.WebUtility.HtmlDecode(s);
}

// Trailing whitespace off every line, runs of blank lines collapsed to one,
// leading and trailing blank lines dropped.  The same normalisation is applied
// to the geas side by compare.sh, so neither engine's spacing habits show up as
// a difference.
static string Normalise(string s)
{
    var lines = s.Replace("\r\n", "\n").Split('\n').Select(l => l.TrimEnd());
    var outp = new StringBuilder();
    var blank = 0;
    foreach (var l in lines)
    {
        if (l.Length == 0) { blank++; if (blank > 1) continue; }
        else blank = 0;
        outp.Append(l).Append('\n');
    }
    return outp.ToString().Trim('\n') + "\n";
}

// Everything in IPlayer is a UI hook; headless, only the four the driver reads
// (menu, question, wait, pause) and the text channel do anything.
class HeadlessPlayer(Action<string> emit) : IPlayer
{
    public MenuData? PendingMenu;
    public string? PendingQuestion;
    public bool IsWaiting;
    public bool IsPausing;

    public void ShowMenu(MenuData menuData) => PendingMenu = menuData;
    public void ShowQuestion(string caption) => PendingQuestion = caption;
    public void DoWait() => IsWaiting = true;
    public void DoPause(int ms) => IsPausing = true;

    public void WriteHTML(string html) => emit(html);
    public Task RunScriptAsync(string function, object[]? parameters) => Task.CompletedTask;

    public void SetWindowMenu(MenuData menuData) { }

    // A *synchronous* `playwav` is a suspend, not just a UI hook: PlayMediaAsync
    // parks the script on _waitTcs (V4Game.cs:7690-7696) and only resumes when
    // the UI calls FinishWait as the audio ends.  Headless there is no audio, so
    // without this the game stops dead at its first sound.  geas has no such
    // suspend at all -- its `playwav` returns at once -- so flagging IsWaiting
    // and letting Settle finish it is what matches geas, not a shortcut.
    public Task PlaySoundAsync(string filename, bool synchronous, bool looped)
    {
        if (synchronous) IsWaiting = true;
        return Task.CompletedTask;
    }
    public void StopSound() { }
    public Task<string> GetUrlAsync(string filename) => Task.FromResult(filename);
    public void LocationUpdated(string location) { }
    // Quest shows the game's name in the window title; geas's frontend prints it
    // as a banner line instead.  Report it so compare.py can tell that banner
    // from a real first-line divergence.
    public void UpdateGameName(string name) =>
        Console.Error.WriteLine("[gamename] " + name);
    // geas's GeasInterface::clear_screen prints a blank-line separator rather
    // than wiping anything, so the transcripts keep a mark where Quest cleared.
    public void ClearScreen() => emit("\n");
    public Task ShowPictureAsync(string filename) => Task.CompletedTask;
    public void SetPanesVisible(string data) { }
    public void SetStatusText(string text) { }
    public void SetBackground(string colour) { }
    public void SetForeground(string colour) { }
    public void SetLinkForeground(string colour) { }
    public void Quit() { }
    public void SetFont(string fontName) { }
    public void SetFontSize(string fontSize) { }
    public void Speak(string text) { }
    public void RequestSave(string html) { }
    public void Show(string element) { }
    public void Hide(string element) { }
    public void SetCompassDirections(IEnumerable<string> dirs) { }
    public void SetInterfaceString(string name, string text) { }
    public void SetPanelContents(string html) { }
    // The engine's own ASL error log.  Off by default — set QVH_LOG=1 to see
    // which script line an "[An internal error occurred]" came from.
    public void Log(string text)
    {
        if (Environment.GetEnvironmentVariable("QVH_LOG") == "1")
            Console.Error.WriteLine("[log] " + text);
    }
    public string GetUIOption(UIOption option) => null!;
}
