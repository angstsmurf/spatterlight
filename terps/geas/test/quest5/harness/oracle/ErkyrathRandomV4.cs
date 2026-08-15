namespace QuestViva.Legacy;

// The Quest 4 (`V4Game`) half of the deterministic-RNG patch; the Quest 5 half
// is ErkyrathRandom.cs, and this is the same xoshiro128** generator seeded the
// same way. It lives in its own file because the Legacy assembly does not
// reference Engine, and it maps a draw onto a range differently — see below.
//
// Quest 4's `$rand(a;b)$` is DoInternalFunction "rand" (V4Game.cs:6982-6988):
//
//     Str(Int(Rnd * (b - a + 1)) + a)
//
// a scaling map over VB's Rnd. geas maps the same range with a modulus,
// `a + erkyrath_random() % (b - a + 1)` (geas-runner.cc:6910). Both are
// uniform, so no game can depend on which is used -- but the oracle is here to
// find *engine* divergences, and two different mappings over one stream would
// bury every one of them in RNG noise. So this reproduces geas's mapping
// exactly, including the two edge cases geas handles explicitly: a reversed
// pair is read as the range the game meant, and a==b still consumes a draw
// (`% 1` is 0, but the number is drawn), which keeps the two streams in step.
//
// Seed comes from QVH_SEED, default 1234; the Quest 4 corpus comparison uses 1
// to match the walkthrough suite's GEAS_SEED.
internal sealed class ErkyrathRandomV4
{
    private readonly uint[] _s = new uint[4];

    public ErkyrathRandomV4(uint seed)
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

    public static ErkyrathRandomV4 FromEnv()
    {
        var env = Environment.GetEnvironmentVariable("QVH_SEED");
        return new ErkyrathRandomV4(uint.TryParse(env, out var v) ? v : 1234u);
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

    private static readonly bool Trace =
        Environment.GetEnvironmentVariable("QVH_TRACE_RAND") is { Length: > 0 } t && t != "0";
    private static long _traceSeq;

    // geas's $rand(a;b)$, draw for draw. Returned as a double because the
    // caller still wraps it in VB's Str(), whose leading-space-for-positives
    // behaviour is part of what the oracle is checking.
    public double Rand(double lower, double upper)
    {
        var lo = (long)lower;
        var hi = (long)upper;
        if (hi < lo) (lo, hi) = (hi, lo);
        var range = (ulong)(hi - lo) + 1UL;
        var r = lo + (long)(NextUInt32() % range);
        if (Trace) Console.Error.WriteLine($"[rand {++_traceSeq}] rand({lo},{hi})={r}");
        return r;
    }
}
