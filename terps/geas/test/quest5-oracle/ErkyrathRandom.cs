namespace QuestViva.Engine;

// Deterministic RNG mirroring Spatterlight's erkyrath_random()
// (terps/common_utils/randomness.c): the xoshiro128** generator seeded via
// SplitMix32, byte-for-byte the same 32-bit stream as the C implementation.
//
// The oracle uses this in place of .NET's Random so that games calling
// GetRandomInt / GetRandomDouble produce a reproducible sequence — and, more
// importantly, the SAME sequence the future native Geas ASLX engine will
// produce when it routes those built-ins through erkyrath_random() under
// SPATTERLIGHT determinism (seed 1234; see TODO-quest5.md §2 "Determinism").
//
// The inclusive-range mapping follows terps/scarier/a5rand.cpp a5rand_between
// (the ADRIFT 5 analog of GetRandomInt): span = hi-lo+1; lo + rand % span, with
// hi==lo drawing no number and a full-domain guard. Native Geas MUST replicate
// this formula for its transcripts to match the oracle's.
//
// Seed defaults to 1234; override with the QVH_SEED environment variable.
internal sealed class ErkyrathRandom
{
    private readonly uint[] _s = new uint[4];

    public ErkyrathRandom(uint seed)
    {
        // SplitMix32 fill of the 128-bit state — identical to xo_seed_random().
        for (var i = 0; i < 4; i++)
        {
            seed += 0x9E3779B9u;
            var z = seed;
            z ^= z >> 15; z *= 0x85EBCA6Bu;
            z ^= z >> 13; z *= 0xC2B2AE35u;
            z ^= z >> 16;
            _s[i] = z;
        }
    }

    public static ErkyrathRandom FromEnv()
    {
        var env = Environment.GetEnvironmentVariable("QVH_SEED");
        return new ErkyrathRandom(uint.TryParse(env, out var v) ? v : 1234u);
    }

    private static uint Rotl(uint x, int k) => (x << k) | (x >> (32 - k));

    // One 32-bit draw — byte-identical to xo_random() in randomness.c.
    public uint NextUInt32()
    {
        var result = Rotl(_s[1] * 5u, 7) * 9u;
        var t = _s[1] << 9;
        _s[2] ^= _s[0];
        _s[3] ^= _s[1];
        _s[1] ^= _s[2];
        _s[0] ^= _s[3];
        _s[2] ^= t;
        _s[3] = Rotl(_s[3], 11);
        return result;
    }

    // QVH_TRACE_RAND=1: log every draw to stderr for stream-parity debugging
    // against the native engine's ASLX_TRACE_RAND (same numbering/format).
    private static readonly bool Trace =
        Environment.GetEnvironmentVariable("QVH_TRACE_RAND") is { Length: > 0 } t && t != "0";
    private static long _traceSeq;

    // Inclusive [min, max]; mirrors a5rand_between (min==max draws no number).
    public int NextInclusive(int min, int max)
    {
        if (max == min) return min;
        if (max < min) (min, max) = (max, min);
        var span = (uint)((long)max - min);          // exact magnitude, no overflow
        if (span == uint.MaxValue) return (int)(min + NextUInt32()); // full domain
        span += 1u;
        var r = (int)(min + NextUInt32() % span);
        if (Trace) Console.Error.WriteLine($"[rand {++_traceSeq}] between({min},{max})={r}");
        return r;
    }

    // [0, 1) double: 32 random bits over 2^32.
    public double NextDouble()
    {
        var r = NextUInt32() / 4294967296.0;
        if (Trace) Console.Error.WriteLine($"[rand {++_traceSeq}] double={r:G17}");
        return r;
    }
}
