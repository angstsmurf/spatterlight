// FrankenDrift.Headless -- a minimal stdin/stdout console runner for the
// FrankenDrift ADRIFT engine, used to capture *ground-truth* transcripts for
// the Scarier ADRIFT 5 port (terps/scarier/a5*).  It drives the same engine
// (FrankenDrift.Adrift) the GUI/Glk runners use -- via UserSession.Process --
// but with a text-only frontend so transcripts can be diffed against Scarier's
// headless a5run harness.
//
//   dotnet run --project FrankenDrift.Headless -- <game.blorb|game.taf> [script.txt]
//
// Commands are read from script.txt (one per line) if given, else from stdin.
// Engine output (HTML-ish) is stripped to plain text the way the Glk renderer
// shows it.  Real-time timer events are intentionally NOT pumped, so runs are
// deterministic and comparable to Scarier's turn-based loop.

using System.Text;
using FrankenDrift.Glue;

namespace FrankenDrift.Headless
{
    // A trivial RichTextBox: the engine only ever reads/writes .Text on the
    // input box (the current command) and uses txtOutput as a tag target.
    internal class TextBox : RichTextBox
    {
        private readonly StringBuilder _sb = new();
        public void Clear() => _sb.Clear();
        public int TextLength => _sb.Length;
        public string Text { get => _sb.ToString(); set { _sb.Clear(); _sb.Append(value); } }
        public string SelectedText { get => ""; set { } }
        public int SelectionStart { get; set; }
        public int SelectionLength { get; set; }
        public bool IsDisposed => false;
    }

    internal class HeadlessRunner : UIGlue, frmRunner
    {
        private readonly TextBox _out = new();
        private readonly TextBox _in = new();
        private readonly Queue<string> _scriptedAnswers;

        public HeadlessRunner(Queue<string> scriptedAnswers) { _scriptedAnswers = scriptedAnswers; }

        // ---- frmRunner -------------------------------------------------------
        public FrankenDrift.Glue.Infragistics.Win.UltraWinToolbars.UltraToolbarsManager UTMMain
            => throw new NotImplementedException();
        public RichTextBox txtOutput => _out;
        public RichTextBox txtInput => _in;
        public bool Locked => false;
        public void ReloadMacros() { }
        public void SaveLayout() { }
        public void SetBackgroundColour() { }
        public void UpdateStatusBar(string desc, string score, string user) { }
        public void SubmitCommand() { }
        public void Close() { }

        // ---- UIGlue ----------------------------------------------------------
        public void ErrMsg(string message, Exception ex = null)
            => Console.Error.WriteLine("ERR: " + message + (ex is null ? "" : " :: " + ex));
        public void MakeNote(string msg) { }
        public void EnableButtons() { }
        public void SetGameName(string name) { }
        public void ScrollToEnd() { }
        public void ShowInfo(string info, string title = null) => EmitHtml((title is null ? "" : title + "\n") + info);
        // Save/restore paths come from the environment so a script's bare
        // `save` / `restore` commands round-trip through a known .tas file --
        // used to cross-validate Scarier's FrankenDrift-compatible save format.
        public string QuerySavePath()
            => Environment.GetEnvironmentVariable("FD_SAVE_PATH") ?? "";
        public string QueryRestorePath()
            => Environment.GetEnvironmentVariable("FD_RESTORE_PATH") ?? "";
        public QueryResult QuerySaveBeforeQuit() => QueryResult.NO;
        public void InitInput() { }
        public void ShowCoverArt(byte[] img) { }
        public void DoEvents() { }
        public string GetAppDataPath() => Path.GetTempPath();
        public string GetExecutableLocation() => AppContext.BaseDirectory;
        public string GetExecutablePath() => AppContext.BaseDirectory;
        public string GetClaimedAdriftVersion() => "5.0.0";

        public bool AskYesNoQuestion(string question, string title = null)
        {
            EmitHtml((title is null ? "" : title + "\n") + question);
            // Honour scripted answers so confirmation prompts are deterministic.
            if (_scriptedAnswers.Count > 0)
            {
                var a = _scriptedAnswers.Peek().Trim().ToLower();
                if (a == "yes" || a == "y" || a == "no" || a == "n")
                {
                    _scriptedAnswers.Dequeue();
                    return a.StartsWith("y");
                }
            }
            return false;
        }

        public void OutputHTML(string source) => EmitHtml(source);

        // ---- HTML -> plain text ---------------------------------------------
        // Mirrors GlkHtmlWin's handling: <br>=newline, style tags dropped,
        // <cls> clears, entities decoded.  Media tags (img/audio) dropped.
        private static void EmitHtml(string src)
        {
            if (string.IsNullOrEmpty(src)) return;
            // The engine echoes the entered command as a Wingdings cursor glyph
            // ("<c><font face="Wingdings" ...>O</font> input</c>").  The Glk
            // frontend suppresses this (input is echoed by the line-input
            // widget); we do too, and print our own "> cmd" line instead.
            if (src.StartsWith("<c><font face=\"Wingdings\"")) return;
            src = src.Replace("\r\n", "\n");
            var sb = new StringBuilder();
            int i = 0;
            while (i < src.Length)
            {
                char c = src[i];
                if (c == '<')
                {
                    int end = src.IndexOf('>', i);
                    if (end < 0) { sb.Append(src.Substring(i)); break; }
                    string tag = src.Substring(i + 1, end - i - 1);
                    string low = tag.ToLowerInvariant().Trim();
                    if (low == "br" || low == "br/" || low == "br /")
                        sb.Append('\n');
                    else if (low == "cls")
                        sb.Clear();       // screen clear: drop buffered text
                    // all other tags (c/b/i/center/tt/font/img/audio/waitkey...) drop
                    i = end + 1;
                }
                else
                {
                    sb.Append(c);
                    i++;
                }
            }
            string txt = sb.ToString()
                .Replace("&lt;", "<").Replace("&gt;", ">")
                .Replace("&perc;", "%").Replace("&quot;", "\"")
                .Replace("&amp;", "&");
            Console.Out.Write(txt);
        }
    }

    internal class GlonkMap : Map
    {
        public void RecalculateNode(object node) { }
        public void SelectNode(string key) { }
    }

    // A System.Random whose integer draws come from the SAME erkyrath xoshiro128**
    // stream Scarier uses (terps/common_utils/randomness.c).  With FD_RNG=xoshiro
    // this replaces SharedModule.r, so RAND-selected text (OneOf/Random tasks,
    // combat rolls, dream variants) lines up byte-for-byte with the a5run harness
    // instead of diverging on .NET's System.Random.
    //
    // The seeding (SplitMix32) and the generator are transcribed verbatim from
    // randomness.c (xo_seed_random / xo_random).  The draw mapping mirrors
    // a5rand_between: SharedModule.Random(iMin,iMax) calls r.Next(iMin,iMax+1),
    // i.e. inclusive [iMin,iMax] with span = iMax-iMin+1; when that span is 1 no
    // value is drawn (a fixed FromTo consumes no RNG, keeping the stream aligned),
    // otherwise the value is iMin + next() % span.  Set FD_RNG_TRACE=1 to echo each
    // draw as "RAND(lo,hi)=r" for diffing against Scarier's A5_TRACE_RAND output.
    internal sealed class XoshiroRandom : Random
    {
        private readonly uint[] _s = new uint[4];
        private static readonly bool _trace =
            Environment.GetEnvironmentVariable("FD_RNG_TRACE") is not null;

        public XoshiroRandom(uint seed)
        {
            // xo_seed_random: SplitMix32 expansion of a single 32-bit seed.
            unchecked
            {
                for (int i = 0; i < 4; i++)
                {
                    seed += 0x9E3779B9u;
                    uint z = seed;
                    z ^= z >> 15; z *= 0x85EBCA6Bu;
                    z ^= z >> 13; z *= 0xC2B2AE35u;
                    z ^= z >> 16;
                    _s[i] = z;
                }
            }
        }

        // xo_random: xoshiro128** next(), returning a full 32-bit word.
        private uint NextRaw()
        {
            unchecked
            {
                uint t1x5 = _s[1] * 5u;
                uint result = ((t1x5 << 7) | (t1x5 >> 25)) * 9u;
                uint t1s9 = _s[1] << 9;
                _s[2] ^= _s[0];
                _s[3] ^= _s[1];
                _s[1] ^= _s[2];
                _s[0] ^= _s[3];
                _s[2] ^= t1s9;
                uint t3 = _s[3];
                _s[3] = (t3 << 11) | (t3 >> 21);
                return result;
            }
        }

        // Global.Random(iMin,iMax) -> r.Next(iMin, iMax+1): inclusive [iMin,iMax].
        public override int Next(int minValue, int maxValue)
        {
            long span = (long)maxValue - minValue;   // == hi - lo + 1
            if (span <= 1) return minValue;          // FromTo hi==lo: no draw
            int r = minValue + (int)(NextRaw() % (uint)span);
            if (_trace) Console.Error.WriteLine($"RAND({minValue},{maxValue - 1})={r}");
            return r;
        }

        // Global.Random(iMax) -> r.Next(iMax+1): inclusive [0, iMax].
        public override int Next(int maxValue) => Next(0, maxValue);
        public override int Next() => Next(0, int.MaxValue);
        protected override double Sample() => NextRaw() / 4294967296.0;
        public override double NextDouble() => Sample();
    }

    public static class Program
    {
        public static int Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.Error.WriteLine("usage: fd-headless <game.blorb|game.taf> [script.txt]");
                return 1;
            }
            string gameFile = args[0];

            // Read the command script up front so yes/no prompts can consult it.
            var lines = new List<string>();
            if (args.Length > 1)
                lines.AddRange(File.ReadAllLines(args[1]));
            else
                for (string l; (l = Console.In.ReadLine()) != null;) lines.Add(l);

            var answers = new Queue<string>(lines);
            var runner = new HeadlessRunner(answers);

            Adrift.SharedModule.Glue = runner;
            Adrift.SharedModule.fRunner = runner;
            Glue.Application.SetFrontend(runner);

            // Make RAND reproducible run-to-run.  SharedModule.r is a private
            // static Random lazily seeded from Now.Ticks; force a fixed seed via
            // reflection so transcripts are stable.
            //
            // FD_RNG=xoshiro swaps in XoshiroRandom, which draws from the SAME
            // erkyrath xoshiro128** stream as Scarier -- so RAND-selected text
            // (dream variants, combat rolls, OneOf picks) matches the a5run harness
            // byte-for-byte.  Left unset, .NET's System.Random is used (a different
            // algorithm), so RAND text will NOT match; ground-truth diffing is then
            // meaningful only on the (large) RAND-independent portion.
            int seed = 1234;
            var sEnv = Environment.GetEnvironmentVariable("FD_SEED");
            if (sEnv != null && int.TryParse(sEnv, out var s)) seed = s;
            Random rng = Environment.GetEnvironmentVariable("FD_RNG") == "xoshiro"
                ? new XoshiroRandom((uint)seed)
                : new Random(seed);
            var rField = typeof(Adrift.SharedModule).GetField("r",
                System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
            rField?.SetValue(null, rng);
            Adrift.SharedModule.UserSession =
                new Adrift.RunnerSession { Map = new GlonkMap(), bShowShortLocations = true };

            if (!Adrift.SharedModule.UserSession.OpenAdventure(gameFile))
            {
                Console.Error.WriteLine("failed to open: " + gameFile);
                return 2;
            }

            foreach (var raw in lines)
            {
                string cmd = raw.Trim();
                // Comments/blank lines mirror Scarier's a5run script handling.
                if (cmd.Length == 0 || cmd.StartsWith("#")) continue;
                Console.Out.Write("\n> " + cmd + "\n");
                runner.txtInput.Text = cmd;
                Adrift.SharedModule.UserSession.Process(cmd);
                if (Adrift.SharedModule.Adventure != null)
                    Adrift.SharedModule.Adventure.Turns += 1;
                if (Adrift.SharedModule.Adventure == null ||
                    Adrift.SharedModule.Adventure.eGameState !=
                        Adrift.clsAction.EndGameEnum.Running)
                    break;
            }
            Console.Out.Flush();
            return 0;
        }
    }
}
