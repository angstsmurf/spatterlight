/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * resample.inc - Resampling helper template.         / / \  \
 *                                                   | <  /   \_
 * By Bob and entheh.                                |  \/ /\   /
 *                                                    \_  /  > /
 * In order to find a good trade-off between            | \ / /
 * speed and accuracy in this code, some tests          |  ' /
 * were carried out regarding the behaviour of           \__/
 * long long ints with gcc. The following code
 * was tested:
 *
 * int a, b, c;
 * c = ((long long)a * b) >> 16;
 *
 * DJGPP GCC Version 3.0.3 generated the following assembly language code for
 * the multiplication and scaling, leaving the 32-bit result in EAX.
 *
 * movl  -8(%ebp), %eax    ; read one int into EAX
 * imull -4(%ebp)          ; multiply by the other; result goes in EDX:EAX
 * shrdl $16, %edx, %eax   ; shift EAX right 16, shifting bits in from EDX
 *
 * Note that a 32*32->64 multiplication is performed, allowing for high
 * accuracy. On the Pentium 2 and above, shrdl takes two cycles (generally),
 * so it is a minor concern when four multiplications are being performed
 * (the cubic resampler). On the Pentium MMX and earlier, it takes four or
 * more cycles, so this method is unsuitable for use in the low-quality
 * resamplers.
 *
 * Since "long long" is a gcc-specific extension, we use LONG_LONG instead,
 * defined in dumb.h. We may investigate later what code MSVC generates, but
 * if it seems too slow then we suggest you use a good compiler.
 *
 * FIXME: these comments are somewhat out of date now.
 */

void dumb_reset_resampler(DUMB_RESAMPLER *resampler, SRCTYPE *src,
                          int src_channels, long pos, long start, long end,
                          int quality) {
    int i;
    resampler->src = src;
    resampler->pos = pos;
    resampler->subpos = 0;
    resampler->start = start;
    resampler->end = end;
    resampler->dir = 1;
    resampler->pickup = NULL;
    resampler->pickup_data = NULL;
    if (quality < 0) {
        resampler->quality = 0;
    } else if (quality > DUMB_RQ_N_LEVELS - 1) {
        resampler->quality = DUMB_RQ_N_LEVELS - 1;
    } else {
        resampler->quality = quality;
    }
    for (i = 0; i < src_channels * 3; i++)
        resampler->X[i] = 0;
    resampler->overshot = -1;
    resampler->fir_resampler_ratio = 0;
    resampler_clear(resampler->fir_resampler[0]);
    resampler_clear(resampler->fir_resampler[1]);
    resampler_set_quality(resampler->fir_resampler[0], resampler->quality);
    resampler_set_quality(resampler->fir_resampler[1], resampler->quality);
}

DUMB_RESAMPLER *dumb_start_resampler(SRCTYPE *src, int src_channels, long pos,
                                     long start, long end, int quality) {
    DUMB_RESAMPLER *resampler = malloc(sizeof(*resampler));
    if (!resampler)
        return NULL;
    dumb_reset_resampler(resampler, src, src_channels, pos, start, end,
                         quality);
    return resampler;
}

#define UPDATE_VOLUME(pvol, vol)                                               \
    {                                                                          \
        if (pvol) {                                                            \
            vol##r += vol##d;                                                  \
            if ((vol##d < 0 && vol##r <= vol##t) ||                            \
                (vol##d > 0 && vol##r >= vol##t)) {                            \
                pvol->volume = pvol->target;                                   \
                if (pvol->declick_stage == 0 || pvol->declick_stage >= 3)      \
                    pvol->declick_stage++;                                     \
                pvol = NULL;                                                   \
                vol = vol##t * vol##m;                                         \
            } else {                                                           \
                vol = vol##r * vol##m;                                         \
            }                                                                  \
        }                                                                      \
    }

/* Create mono source resampler. */
#define SUFFIX2 _1
#define SRC_CHANNELS 1
#define DIVIDE_BY_SRC_CHANNELS(x) (x)
#define COPYSRC(dstarray, dstindex, srcarray, srcindex)                        \
    (dstarray)[dstindex] = (srcarray)[srcindex]
#define COPYSRC2(dstarray, dstindex, condition, srcarray, srcindex)            \
    (dstarray)[dstindex] = condition ? (srcarray)[srcindex] : 0
#define MONO_DEST_VOLUME_PARAMETERS DUMB_VOLUME_RAMP_INFO *volume
#define MONO_DEST_VOLUME_VARIABLES vol, volr, vold, volt, volm
#define MONO_DEST_VOLUME_ZEROS 0
#define SET_MONO_DEST_VOLUME_VARIABLES                                         \
    {                                                                          \
        if (volume) {                                                          \
            volr = volume->volume;                                             \
            vold = volume->delta;                                              \
            volt = volume->target;                                             \
            volm = volume->mix;                                                \
            vol = volr * volm;                                                 \
            if (volr == volt)                                                  \
                volume = NULL;                                                 \
        } else {                                                               \
            vol = 0;                                                           \
            volr = 0;                                                          \
            vold = 0;                                                          \
            volt = 0;                                                          \
            volm = 0;                                                          \
        }                                                                      \
    }
#define RETURN_MONO_DEST_VOLUME_VARIABLES                                      \
    if (volume)                                                                \
    volume->volume = volr
#define MONO_DEST_VOLUMES_ARE_ZERO (vol == 0 && volt == 0)
#define POKE_FIR(offset)                                                       \
    {                                                                          \
        resampler_write_sample_float(resampler->fir_resampler[0],              \
                                     FIR(x[offset]));                          \
    }
#define MONO_DEST_PEEK_FIR                                                     \
    *dst = resampler_get_sample_float(resampler->fir_resampler[0]) * vol *     \
           16777216.0f
#define MONO_DEST_MIX_FIR                                                      \
    {                                                                          \
        *dst++ += resampler_get_sample_float(resampler->fir_resampler[0]) *    \
                  vol * 16777216.0f;                                           \
        UPDATE_VOLUME(volume, vol);                                            \
    }
#define ADVANCE_FIR resampler_remove_sample(resampler->fir_resampler[0], 1)
#define STEREO_DEST_PEEK_FIR                                                   \
    {                                                                          \
        float sample =                                                         \
            resampler_get_sample_float(resampler->fir_resampler[0]);           \
        *dst++ = sample * lvol * 16777216.0f;                                  \
        *dst++ = sample * rvol * 16777216.0f;                                  \
    }
#define STEREO_DEST_MIX_FIR                                                    \
    {                                                                          \
        float sample =                                                         \
            resampler_get_sample_float(resampler->fir_resampler[0]);           \
        *dst++ += sample * lvol * 16777216.0f;                                 \
        *dst++ += sample * rvol * 16777216.0f;                                 \
        UPDATE_VOLUME(volume_left, lvol);                                      \
        UPDATE_VOLUME(volume_right, rvol);                                     \
    }
#include "resamp2.inc"

/* Create stereo source resampler. */
#define SUFFIX2 _2
#define SRC_CHANNELS 2
#define DIVIDE_BY_SRC_CHANNELS(x) ((x) >> 1)
#define COPYSRC(dstarray, dstindex, srcarray, srcindex)                        \
    {                                                                          \
        (dstarray)[(dstindex)*2] = (srcarray)[(srcindex)*2];                   \
        (dstarray)[(dstindex)*2 + 1] = (srcarray)[(srcindex)*2 + 1];           \
    }
#define COPYSRC2(dstarray, dstindex, condition, srcarray, srcindex)            \
    {                                                                          \
        if (condition) {                                                       \
            (dstarray)[(dstindex)*2] = (srcarray)[(srcindex)*2];               \
            (dstarray)[(dstindex)*2 + 1] = (srcarray)[(srcindex)*2 + 1];       \
        } else {                                                               \
            (dstarray)[(dstindex)*2] = 0;                                      \
            (dstarray)[(dstindex)*2 + 1] = 0;                                  \
        }                                                                      \
    }

#define MONO_DEST_VOLUME_PARAMETERS                                            \
    DUMB_VOLUME_RAMP_INFO *volume_left, DUMB_VOLUME_RAMP_INFO *volume_right
#define MONO_DEST_VOLUME_VARIABLES                                             \
    lvol, lvolr=0, lvold, lvolt, lvolm, rvol, rvolr=0, rvold, rvolt, rvolm
#define MONO_DEST_VOLUME_ZEROS 0, 0
#define SET_MONO_DEST_VOLUME_VARIABLES                                         \
    {                                                                          \
        if (volume_left) {                                                     \
            lvolr = volume_left->volume;                                       \
            lvold = volume_left->delta;                                        \
            lvolt = volume_left->target;                                       \
            lvolm = volume_left->mix;                                          \
            lvol = lvolr * lvolm;                                              \
            if (lvolr == lvolt)                                                \
                volume_left = NULL;                                            \
        } else {                                                               \
            lvol = 0;                                                          \
            lvolr = 0;                                                         \
            lvold = 0;                                                         \
            lvolt = 0;                                                         \
            lvolm = 0;                                                         \
        }                                                                      \
        if (volume_right) {                                                    \
            rvolr = volume_right->volume;                                      \
            rvold = volume_right->delta;                                       \
            rvolt = volume_right->target;                                      \
            rvolm = volume_right->mix;                                         \
            rvol = rvolr * rvolm;                                              \
            if (rvolr == rvolt)                                                \
                volume_right = NULL;                                           \
        } else {                                                               \
            rvol = 0;                                                          \
            rvolr = 0;                                                         \
            rvold = 0;                                                         \
            rvolt = 0;                                                         \
            rvolm = 0;                                                         \
        }                                                                      \
    }
#define RETURN_MONO_DEST_VOLUME_VARIABLES                                      \
    {                                                                          \
        if (volume_left)                                                       \
            volume_left->volume = lvolr;                                       \
        if (volume_right)                                                      \
            volume_right->volume = rvolr;                                      \
    }
#define MONO_DEST_VOLUMES_ARE_ZERO                                             \
    (lvol == 0 && lvolt == 0 && rvol == 0 && rvolt == 0)
#define POKE_FIR(offset)                                                       \
    {                                                                          \
        resampler_write_sample_float(resampler->fir_resampler[0],              \
                                     FIR(x[(offset)*2 + 0]));                  \
        resampler_write_sample_float(resampler->fir_resampler[1],              \
                                     FIR(x[(offset)*2 + 1]));                  \
    }
#define MONO_DEST_PEEK_FIR                                                     \
    {                                                                          \
        *dst =                                                                 \
            (resampler_get_sample_float(resampler->fir_resampler[0]) * lvol +  \
             resampler_get_sample_float(resampler->fir_resampler[1]) * rvol) * \
            16777216.0f;                                                       \
    }
#define MONO_DEST_MIX_FIR                                                      \
    {                                                                          \
        *dst++ +=                                                              \
            (resampler_get_sample_float(resampler->fir_resampler[0]) * lvol +  \
             resampler_get_sample_float(resampler->fir_resampler[1]) * rvol) * \
            16777216.0f;                                                       \
        UPDATE_VOLUME(volume_left, lvol);                                      \
        UPDATE_VOLUME(volume_right, rvol);                                     \
    }
#define ADVANCE_FIR                                                            \
    {                                                                          \
        resampler_remove_sample(resampler->fir_resampler[0], 1);               \
        resampler_remove_sample(resampler->fir_resampler[1], 1);               \
    }
#define STEREO_DEST_PEEK_FIR                                                   \
    {                                                                          \
        *dst++ = resampler_get_sample_float(resampler->fir_resampler[0]) *     \
                 lvol * 16777216.0f;                                           \
        *dst++ = resampler_get_sample_float(resampler->fir_resampler[1]) *     \
                 rvol * 16777216.0f;                                           \
    }
#define STEREO_DEST_MIX_FIR                                                    \
    {                                                                          \
        *dst++ += resampler_get_sample_float(resampler->fir_resampler[0]) *    \
                  lvol * 16777216.0f;                                          \
        *dst++ += resampler_get_sample_float(resampler->fir_resampler[1]) *    \
                  rvol * 16777216.0f;                                          \
        UPDATE_VOLUME(volume_left, lvol);                                      \
        UPDATE_VOLUME(volume_right, rvol);                                     \
    }
#include "resamp2.inc"

void dumb_end_resampler(DUMB_RESAMPLER *resampler) {
    if (resampler)
        free(resampler);
}

#undef FIR
#undef SRCBITS
#undef SRCTYPE
#undef SUFFIX
