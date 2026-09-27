using System.Reflection;

// The Legacy assembly is built with IsAotCompatible, and walking a record's
// fields at runtime is not something the trimmer can follow.  qv4 is a JIT-only
// test oracle -- it is never published AOT or trimmed -- and doing this by hand
// instead would be seventy-odd assignments that go quietly stale the moment
// upstream adds a field, which is the one failure mode a ground-truth oracle
// must not have.
#pragma warning disable IL2067, IL2070, IL2072, IL2075, IL3050

namespace QuestViva.Legacy;

/// <summary>
/// VB6 record semantics for the types QuestViva translated from VB6 <c>Type</c>
/// declarations.
/// </summary>
/// <remarks>
/// <para>
/// <c>ObjectType</c>, <c>RoomType</c> and the smaller records nested beside
/// them in <c>V4Game.Types.cs</c> were all <c>Type</c> blocks in Axe's VB6
/// Quest 4.  QuestViva declares them <c>class</c>, which is right for the
/// storage but wrong for the two places the original relied on their being
/// values: an unassigned array slot was a real zeroed record rather than
/// <c>null</c>, and <c>a = b</c> copied every field rather than aliasing.
/// Both are patched at their call sites -- see sections 11 and 12 of
/// <c>patch_questviva.py</c> -- using the two helpers here.
/// </para>
/// <para>
/// The line between "was a VB6 <c>Type</c>" and "was a VB6 class module" is
/// drawn at nesting: the records live inside <c>V4Game</c>, while
/// <c>RoomExits</c> and <c>RoomExit</c> are top-level classes holding a
/// back-reference to the game.  Those two were class modules in VB6 as well,
/// so a record assignment copied the *reference* to them and <see
/// cref="V4Game.QvhCopyRecord"/> does the same.
/// </para>
/// </remarks>
public partial class V4Game
{
    private const BindingFlags QvhRecordFields =
        BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance;

    /// <summary>
    /// A VB6 record was a <c>Type</c>, not a class module -- and the tell is
    /// nesting inside <c>V4Game</c>, as the remarks above set out.
    /// </summary>
    private static bool IsQvhRecord(Type t) => t.IsClass && t.DeclaringType == typeof(V4Game);

    /// <summary>
    /// The zeroed record VB6 left in an array slot nobody had filled in:
    /// strings empty rather than <c>null</c>, nested records themselves
    /// zeroed, and arrays allocated but empty of used elements (every loop
    /// over them runs 1..Number*, which is no iterations at all).
    /// </summary>
    internal static T QvhZeroedRecord<T>() => (T)QvhZeroed(typeof(T));

    private static object QvhZeroed(Type t)
    {
        var record = Activator.CreateInstance(t);

        foreach (var f in t.GetFields(QvhRecordFields))
        {
            if (IsQvhRecord(f.FieldType))
            {
                f.SetValue(record, QvhZeroed(f.FieldType));
            }
            else if (f.GetValue(record) != null)
            {
                // A field the constructor already initialised: leave it be.
            }
            else if (f.FieldType == typeof(string))
            {
                f.SetValue(record, "");
            }
            else if (f.FieldType.IsArray)
            {
                f.SetValue(record, Array.CreateInstance(f.FieldType.GetElementType()!, 1));
            }
        }

        return record;
    }

    /// <summary>
    /// What <c>objs(n) = objs(id)</c> did in VB6: a field-by-field copy, deep
    /// through nested records and the dynamic arrays inside them, and shallow
    /// through anything that was a class module.
    /// </summary>
    internal static T QvhCopyRecord<T>(T src) => (T)QvhCopy(src);

    private static object QvhCopy(object src)
    {
        if (src is null) return null;

        var t = src.GetType();

        if (t.IsValueType || t == typeof(string)) return src;

        if (t.IsArray)
        {
            var source = (Array)src;
            var copy = Array.CreateInstance(t.GetElementType()!, source.Length);
            for (var i = 0; i < source.Length; i++) copy.SetValue(QvhCopy(source.GetValue(i)), i);
            return copy;
        }

        // A class module: VB6 copied the reference, so do that.
        if (!IsQvhRecord(t)) return src;

        var record = Activator.CreateInstance(t);
        foreach (var f in t.GetFields(QvhRecordFields)) f.SetValue(record, QvhCopy(f.GetValue(src)));
        return record;
    }
}
