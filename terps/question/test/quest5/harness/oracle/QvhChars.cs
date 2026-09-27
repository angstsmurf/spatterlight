using System.Text;

namespace QuestViva.Legacy;

/// <summary>
/// Windows-1252 stand-ins for VB's <c>Chr</c>/<c>Asc</c>.
/// </summary>
/// <remarks>
/// <para>
/// <c>Microsoft.VisualBasic.Strings.Chr</c> maps codes 0-255 through the
/// thread's <em>ANSI</em> code page.  On Windows that is 1252, which is what
/// VB6 Quest was written against; on macOS and Linux .NET reports 65001
/// (UTF-8), so <c>Chr(253)</c> decodes the lone byte 0xFD as invalid UTF-8 and
/// yields U+FFFD instead of 'ý'.  <c>Asc</c> has the mirror-image problem.
/// </para>
/// <para>
/// V4Game reads .cas and .qsg files with <c>Encoding.GetEncoding(1252)</c> and
/// then hunts through the result for <c>Chr(253)</c> / <c>Chr(254)</c>
/// delimiters, so off-Windows the delimiters are never found: every CAS game
/// dies in <c>LoadCASFile</c> with "Argument 'Length' must be greater or equal
/// to zero" (the <c>InStr</c> miss returns 0 and the <c>Mid</c> length goes
/// negative).  <c>DecryptString</c> and the two QSG decoders are wrong in the
/// same way for any byte above 0x7F, which would silently mangle text rather
/// than crash.  Pinning both directions to 1252 restores the Windows
/// behaviour, and 1252 round-trips all 256 bytes in .NET.
/// </para>
/// </remarks>
internal static class QvhChars
{
    private static readonly Encoding Cp1252 = Encoding.GetEncoding(1252);

    public static char Chr(int code) =>
        code is >= 0 and <= 255 ? Cp1252.GetString([(byte)code])[0] : (char)code;

    public static int Asc(char c) => Cp1252.GetBytes([c])[0];

    public static int Asc(string s) => Asc(s[0]);
}
