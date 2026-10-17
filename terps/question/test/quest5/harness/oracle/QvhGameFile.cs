using System.Collections;
using System.Diagnostics.CodeAnalysis;
using QuestViva.Common;

// Loads a game from disk for the oracles (qvh, and qv4, which compiles this file
// in too). Upstream dropped its file-based IGameDataProvider after v6.0.0: a
// GameData now takes the game's bytes plus a dictionary of the files beside it
// (custom libraries, Quest 4 libraries and resources). The dictionary here reads
// those from the game's directory on demand, as FileDirectoryGameDataProvider's
// GetAdjacentFile did.
public static class QvhGameFile
{
    public static GameData Load(string path) =>
        new(File.ReadAllBytes(path), path, new AdjacentFiles(Path.GetDirectoryName(Path.GetFullPath(path))!));

    private sealed class AdjacentFiles(string dir) : IReadOnlyDictionary<string, byte[]>
    {
        public bool TryGetValue(string key, [MaybeNullWhen(false)] out byte[] value)
        {
            var file = Path.Combine(dir, key);
            value = File.Exists(file) ? File.ReadAllBytes(file) : null;
            return value != null;
        }

        public bool ContainsKey(string key) => File.Exists(Path.Combine(dir, key));

        public byte[] this[string key] =>
            TryGetValue(key, out var value) ? value : throw new KeyNotFoundException(key);

        public IEnumerable<string> Keys => Directory.EnumerateFiles(dir).Select(f => Path.GetFileName(f));
        public IEnumerable<byte[]> Values => Keys.Select(k => this[k]);
        public int Count => Keys.Count();

        public IEnumerator<KeyValuePair<string, byte[]>> GetEnumerator() =>
            Keys.Select(k => new KeyValuePair<string, byte[]>(k, this[k])).GetEnumerator();

        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
    }
}
