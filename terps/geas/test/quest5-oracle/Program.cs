using System.Text;
using System.Text.RegularExpressions;
using QuestViva.Common;
using QuestViva.Engine;

// qvh <game.quest|game.aslx> [command-script.txt]
//
// Ground-truth oracle for the planned native Geas ASLX (Quest 5) engine.
// Loads a Quest 5 game headless, drives it with a command script, and writes a
// normalised plain-text transcript to stdout. Diagnostics go to stderr.
//
// The command script mirrors QuestViva's own WalkthroughRunner step format so
// that a game's built-in <walkthrough> and hand-written scripts are driven the
// same way the reference player drives them:
//   plain line        -> SendCommand (also feeds a pending `get input`)
//   menu:KEY          -> answer a `show menu` (KEY is an option key)
//   answer:yes|no     -> answer an `ask`
//   assert:EXPR       -> evaluate EXPR, record Pass/Fail in the transcript
//   label:... delay:N -> ignored (editor bookkeeping)
//   event:NAME;PARAM  -> SendEvent
//   # ...             -> comment
// `wait` / `DoPause` "press any key" prompts are auto-continued (they consume
// no script line), matching WalkthroughRunner.
//
// Leniency for human-authored walkthroughs (which lack the menu:/answer:
// prefixes): while a menu or question is pending, a bare line is resolved as a
// menu key / option number / yes-no answer.

if (args.Length < 1)
{
    Console.Error.WriteLine("usage: qvh <game> [script]");
    return 2;
}

var transcript = new StringBuilder();
var emitCount = 0;

void Emit(string html)
{
    var t = Strip(html);
    if (t.Length == 0) return;
    emitCount++;
    transcript.Append(t);
    if (!t.EndsWith('\n')) transcript.Append('\n');
}

void Line(string s) => transcript.Append(s).Append('\n');

var provider = new FileDirectoryGameDataProvider(args[0]);
var gameData = await provider.GetData();
var world = new WorldModel(gameData, null);
world.LogError += ex => Console.Error.WriteLine("[error] " + ex.Message);
world.PrintText += Emit;                 // legacy output path (ASL < v540)

TaskScheduler.UnobservedTaskException += (_, e) =>
    Console.Error.WriteLine("[unobserved] " + e.Exception.GetBaseException().Message);

var player = new HeadlessPlayer(Emit);
if (!await world.Initialise(player))
{
    Console.Error.WriteLine("[fatal] game failed to initialise");
    return 3;
}

var echoCommands = world.Version >= WorldModelVersion.v520; // engine self-echoes below v520
Console.Error.WriteLine($"[diag] version={world.Version} objects={world.Objects.Count()} " +
    $"hasStartGame={world.Elements.ContainsKey(ElementType.Function, "StartGame")} " +
    $"errors={(world.Errors == null ? 0 : world.Errors.Count)}");

// Begin() completes when the opening turn suspends (finished, or blocked on a
// prompt). No polling needed — the suspend TCS is the real settle signal.
await world.Begin();
await AutoAdvance();
Console.Error.WriteLine($"[diag] after begin: emits={emitCount} state={world.State}");

var stepCount = 0;
var scriptExhausted = true;
if (args.Length >= 2)
{
    foreach (var raw in File.ReadLines(args[1]))
    {
        if (world.State == GameState.Finished) { scriptExhausted = false; break; }
        var cmd = raw.Trim();
        if (cmd.Length == 0 || cmd.StartsWith('#')) continue;
        stepCount++;

        if (player.PendingMenu is { } menu)
        {
            var key = ResolveMenuKey(menu, cmd);
            player.PendingMenu = null;
            if (key == null)
                Console.Error.WriteLine($"[warn] step {stepCount}: '{cmd}' is not a valid menu option; cancelling menu");
            await world.SetMenuResponse(key);
        }
        else if (player.PendingQuestion != null)
        {
            player.PendingQuestion = null;
            await world.SetQuestionResponse(ParseYesNo(cmd));
        }
        else if (cmd.StartsWith("assert:"))
        {
            var expr = cmd[7..];
            var ok = await world.AssertAsync(expr);
            Line($"[assert {(ok ? "PASS" : "FAIL")}] {expr}");
            if (!ok) Console.Error.WriteLine($"[assert-fail] {expr}");
        }
        else if (cmd.StartsWith("label:") || cmd.StartsWith("delay:") || cmd.StartsWith("runtime:"))
        {
            // editor bookkeeping — ignore
        }
        else if (cmd.StartsWith("event:"))
        {
            var parts = cmd[6..].Split(';', 2);
            await world.SendEvent(parts[0], parts.Length > 1 ? parts[1] : "");
        }
        else
        {
            if (echoCommands) Line("> " + cmd);
            await world.SendCommand(cmd);
        }

        await AutoAdvance();
    }
}

Console.Error.WriteLine($"[diag] end: steps={stepCount} emits={emitCount} state={world.State} " +
    $"scriptExhausted={scriptExhausted}");
Line($"[state={world.State}]");

Console.Out.Write(Normalise(transcript.ToString()));
return 0;

// Auto-continue `wait`/`DoPause` prompts, which carry no player text. A menu or
// ask callback can itself hit a wait, so loop until the turn settles clean.
async Task AutoAdvance()
{
    while (world.State != GameState.Finished && (player.IsWaiting || player.IsPausing))
    {
        if (player.IsWaiting)  { player.IsWaiting = false;  await world.FinishWait(); }
        if (player.IsPausing)  { player.IsPausing = false;  await world.FinishPause(); }
    }
}

// Resolve a script line to a menu option key: exact key, exact display text,
// or 1-based option number. Returns null (cancel) if nothing matches.
static string? ResolveMenuKey(MenuData menu, string line)
{
    if (line.StartsWith("menu:")) line = line[5..].Trim();
    if (menu.Options.ContainsKey(line)) return line;
    foreach (var kv in menu.Options)
        if (string.Equals(kv.Value, line, StringComparison.OrdinalIgnoreCase)) return kv.Key;
    if (int.TryParse(line, out var n) && n >= 1 && n <= menu.Options.Count)
        return menu.Options.Keys.ElementAt(n - 1);
    return null;
}

static bool ParseYesNo(string line)
{
    if (line.StartsWith("answer:")) line = line[7..].Trim();
    return line.Trim().ToLowerInvariant() is "yes" or "y" or "true" or "1";
}

// Strip the constrained HTML Quest emits down to plain text.
static string Strip(string s)
{
    s = Regex.Replace(s, "<br\\s*/?>", "\n");
    s = Regex.Replace(s, "<[^>]+>", "");
    return System.Net.WebUtility.HtmlDecode(s);
}

// Normalise for a stable diff: strip trailing whitespace per line, collapse
// runs of blank lines to a single blank line, ensure a trailing newline.
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
    // trim leading/trailing blank lines
    return outp.ToString().Trim('\n') + "\n";
}

// Every IPlayer member is a UI hook; a headless run no-ops all but the ones the
// driver inspects (menu / question / wait / pause) and the text channels.
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
    // Quest 5 (v540+) delivers game text as JS calls: JS.addText(html) arrives
    // here as RunScriptAsync("addText", [html]). That is the real output channel.
    public Task RunScriptAsync(string function, object[]? parameters)
    {
        if (function == "addText" && parameters is { Length: > 0 })
            emit(parameters[0]?.ToString() ?? "");
        return Task.CompletedTask;
    }

    public void SetWindowMenu(MenuData menuData) { }
    public Task PlaySoundAsync(string filename, bool synchronous, bool looped) => Task.CompletedTask;
    public void StopSound() { }
    public Task<string> GetUrlAsync(string filename) => Task.FromResult(filename);
    public void LocationUpdated(string location) { }
    public void UpdateGameName(string name) { }
    public void ClearScreen() { }
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
    public void Log(string text) { }
    public string GetUIOption(UIOption option) => null!;
}
