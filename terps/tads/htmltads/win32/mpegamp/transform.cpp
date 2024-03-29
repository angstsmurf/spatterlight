/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* 
 *   transform.c imdct and polyphase(DCT) transforms
 *   
 *   Created by: tomislav uzelac May 1996 Karl Anders Oygard optimized
 *   this for speed, Mar 13 97 Some optimisations based on ideas from
 *   Michael Hipp's mpg123 package
 *   
 *   Ported to Win32, modified for C++, and adapted for use in HTML TADS
 *   by Michael J. Roberts, October 1998.  Changes copyright 1998 Michael
 *   J. Roberts.  
 */

/* for MS VC++, turn off warnings about initializing floats from doubles */
#ifdef _MSC_VER
#pragma warning(disable: 4305; disable: 4244)
#endif

/*
 * Comments for this file:
 *
 * The polyphase algorithm is clearly the most cpu consuming part of mpeg 1
 * layer 3 decoding.  Thus, there has been some effort to optimise this
 * particular algorithm.  Currently, everything has been kept in straight C
 * with no assembler optimisations, but in order to provide efficient paths
 * for different architectures, alternative implementations of some
 * critical sections has been done.  You may want to experiment with these,
 * to see which suits your architecture better.
 *
 * Selection of the different implementations is done with the following
 * defines:
 *
 *     HAS_AUTOINCREMENT
 *
 *         Define this if your architecture supports preincrementation of
 *         pointers when referencing (applies to e.g. 68k)
 *
 * For those who are optimising amp, check out the Pentium rdtsc code
 * (define PENTIUM_RDTSC).  This code uses the rdtsc counter for showing
 * how many cycles are spent in different parts of the code.
 */

#include <math.h>
#include <sys/types.h>
//#include <unistd.h>
#include <string.h>

#include "mpegamp.h"


/* ------------------------------------------------------------------------ */
/*
 *   Tables and constants 
 */

#define PI12      0.261799387f
#define PI36      0.087266462f

const float CMpegAmp::t_sin[4][36] =
{
    {
        -0.032160,    0.103553,   -0.182543,    0.266729,   -0.353554,    0.440377,
        -0.524563,    0.603553,   -0.674947,    0.736575,   -0.786566,    0.823400,
        -0.845957,    0.853554,   -0.845957,    0.823399,   -0.786566,    0.736575,
        -0.674947,    0.603553,   -0.524564,    0.440378,   -0.353553,    0.266729,
        -0.182544,    0.103553,   -0.032160,   -0.029469,    0.079459,   -0.116293,
        0.138851,   -0.146446,    0.138851,   -0.116293,    0.079459,   -0.029469
    },
    {
        -0.032160,    0.103553,   -0.182543,    0.266729,   -0.353554,    0.440377,
        -0.524563,    0.603553,   -0.674947,    0.736575,   -0.786566,    0.823400,
        -0.845957,    0.853554,   -0.845957,    0.823399,   -0.786566,    0.736575,
        -0.675590,    0.608761,   -0.537300,    0.461749,   -0.382683,    0.300706,
        -0.214588,    0.120590,   -0.034606,   -0.026554,    0.049950,   -0.028251,
        0.000000,    0.000000,    0.000000,    0.000000,    0.000000,    0.000000
    },
    {
        -0.103553,    0.353554,   -0.603553,    0.786566,   -0.853554,    0.786566,
        -0.603553,    0.353553,   -0.103553,   -0.079459,    0.146446,   -0.079459,
        0.000000,    0.000000,    0.000000,    0.000000,    0.000000,    0.000000,
        0.000000,    0.000000,    0.000000,    0.000000,    0.000000,    0.000000,
        0.000000,    0.000000,    0.000000,    0.000000,    0.000000,    0.000000,
        0.000000,    0.000000,    0.000000,    0.000000,    0.000000,    0.000000
    },
    {
        0.000000,    0.000000,    0.000000,    0.000000,    0.000000,    0.000000,
        -0.127432,    0.379410,   -0.608182,    0.792598,   -0.915976,    0.967944,
        -0.953717,    0.923880,   -0.887011,    0.843391,   -0.793353,    0.737277,
        -0.674947,    0.603553,   -0.524564,    0.440378,   -0.353553,    0.266729,
        -0.182544,    0.103553,   -0.032160,   -0.029469,    0.079459,   -0.116293,
        0.138851,   -0.146446,    0.138851,   -0.116293,    0.079459,   -0.029469
    }
};

const float CMpegAmp::t_2cos[4][18] =
{
    { -0.174311,  -0.517638,  -0.845237,  -1.147153,  -1.414214,  -1.638304, -1.812616,  -1.931852,  -1.992389,  
    0.174311,   0.517638,   0.845237,   1.147153,   1.414214,   1.638304, 1.812616,   1.931852,   1.992389},
    { -0.174311,  -0.517638,  -0.845237,  -1.147153,  -1.414214,  -1.638304, -1.812616,  -1.931852,  -1.992389,
    0.174311,   0.517638,   0.845237,   1.147153,   1.414214,   1.638304, 1.812616,   1.931852,   1.992389},
    { -0.517638, -1.41421, -1.93185, 0.517638, 1.41421, 1.93185,0,0,0,0,0,0,0,0,0,0,0,0},
    { -0.174311,  -0.517638,  -0.845237,  -1.147153,  -1.414214,  -1.638304, -1.812616,  -1.931852,  -1.992389,
    0.174311,   0.517638,   0.845237,   1.147153,   1.414214,   1.638304, 1.812616,   1.931852,   1.992389}
};

float CMpegAmp::t_dewindow[17][32] =
{
    {
        0.000000000 ,-0.000442505 , 0.003250122 ,-0.007003784 , 0.031082153 ,-0.078628540 , 0.100311279 ,-0.572036743 ,
        1.144989014 , 0.572036743 , 0.100311279 , 0.078628540 , 0.031082153 , 0.007003784 , 0.003250122 , 0.000442505 ,
        0.000000000 ,-0.000442505 , 0.003250122 ,-0.007003784 , 0.031082153 ,-0.078628540 , 0.100311279 ,-0.572036743 ,
        1.144989014 , 0.572036743 , 0.100311279 , 0.078628540 , 0.031082153 , 0.007003784 , 0.003250122 , 0.000442505 ,
    },
    {
        -0.000015259 ,-0.000473022 , 0.003326416 ,-0.007919312 , 0.030517578 ,-0.084182739 , 0.090927124 ,-0.600219727 ,
        1.144287109 , 0.543823242 , 0.108856201 , 0.073059082 , 0.031478882 , 0.006118774 , 0.003173828 , 0.000396729 ,
        -0.000015259 ,-0.000473022 , 0.003326416 ,-0.007919312 , 0.030517578 ,-0.084182739 , 0.090927124 ,-0.600219727 ,
        1.144287109 , 0.543823242 , 0.108856201 , 0.073059082 , 0.031478882 , 0.006118774 , 0.003173828 , 0.000396729 ,
    },
    {
        -0.000015259 ,-0.000534058 , 0.003387451 ,-0.008865356 , 0.029785156 ,-0.089706421 , 0.080688477 ,-0.628295898 ,
        1.142211914 , 0.515609741 , 0.116577148 , 0.067520142 , 0.031738281 , 0.005294800 , 0.003082275 , 0.000366211 ,
        -0.000015259 ,-0.000534058 , 0.003387451 ,-0.008865356 , 0.029785156 ,-0.089706421 , 0.080688477 ,-0.628295898 ,
        1.142211914 , 0.515609741 , 0.116577148 , 0.067520142 , 0.031738281 , 0.005294800 , 0.003082275 , 0.000366211 ,
    },
    {
        -0.000015259 ,-0.000579834 , 0.003433228 ,-0.009841919 , 0.028884888 ,-0.095169067 , 0.069595337 ,-0.656219482 ,
        1.138763428 , 0.487472534 , 0.123474121 , 0.061996460 , 0.031845093 , 0.004486084 , 0.002990723 , 0.000320435 ,
        -0.000015259 ,-0.000579834 , 0.003433228 ,-0.009841919 , 0.028884888 ,-0.095169067 , 0.069595337 ,-0.656219482 ,
        1.138763428 , 0.487472534 , 0.123474121 , 0.061996460 , 0.031845093 , 0.004486084 , 0.002990723 , 0.000320435 ,
    },
    {
        -0.000015259 ,-0.000625610 , 0.003463745 ,-0.010848999 , 0.027801514 ,-0.100540161 , 0.057617187 ,-0.683914185 ,
        1.133926392 , 0.459472656 , 0.129577637 , 0.056533813 , 0.031814575 , 0.003723145 , 0.002899170 , 0.000289917 ,
        -0.000015259 ,-0.000625610 , 0.003463745 ,-0.010848999 , 0.027801514 ,-0.100540161 , 0.057617187 ,-0.683914185 ,
        1.133926392 , 0.459472656 , 0.129577637 , 0.056533813 , 0.031814575 , 0.003723145 , 0.002899170 , 0.000289917 ,
    },
    {
        -0.000015259 ,-0.000686646 , 0.003479004 ,-0.011886597 , 0.026535034 ,-0.105819702 , 0.044784546 ,-0.711318970 ,
        1.127746582 , 0.431655884 , 0.134887695 , 0.051132202 , 0.031661987 , 0.003005981 , 0.002792358 , 0.000259399 ,
        -0.000015259 ,-0.000686646 , 0.003479004 ,-0.011886597 , 0.026535034 ,-0.105819702 , 0.044784546 ,-0.711318970 ,
        1.127746582 , 0.431655884 , 0.134887695 , 0.051132202 , 0.031661987 , 0.003005981 , 0.002792358 , 0.000259399 ,
    },
    {
        -0.000015259 ,-0.000747681 , 0.003479004 ,-0.012939453 , 0.025085449 ,-0.110946655 , 0.031082153 ,-0.738372803 ,
        1.120223999 , 0.404083252 , 0.139450073 , 0.045837402 , 0.031387329 , 0.002334595 , 0.002685547 , 0.000244141 ,
        -0.000015259 ,-0.000747681 , 0.003479004 ,-0.012939453 , 0.025085449 ,-0.110946655 , 0.031082153 ,-0.738372803 ,
        1.120223999 , 0.404083252 , 0.139450073 , 0.045837402 , 0.031387329 , 0.002334595 , 0.002685547 , 0.000244141 ,
    },
    {
        -0.000030518 ,-0.000808716 , 0.003463745 ,-0.014022827 , 0.023422241 ,-0.115921021 , 0.016510010 ,-0.765029907 ,
        1.111373901 , 0.376800537 , 0.143264771 , 0.040634155 , 0.031005859 , 0.001693726 , 0.002578735 , 0.000213623 ,
        -0.000030518 ,-0.000808716 , 0.003463745 ,-0.014022827 , 0.023422241 ,-0.115921021 , 0.016510010 ,-0.765029907 ,
        1.111373901 , 0.376800537 , 0.143264771 , 0.040634155 , 0.031005859 , 0.001693726 , 0.002578735 , 0.000213623 ,
    },
    {
        -0.000030518 ,-0.000885010 , 0.003417969 ,-0.015121460 , 0.021575928 ,-0.120697021 , 0.001068115 ,-0.791213989 ,
        1.101211548 , 0.349868774 , 0.146362305 , 0.035552979 , 0.030532837 , 0.001098633 , 0.002456665 , 0.000198364 ,
        -0.000030518 ,-0.000885010 , 0.003417969 ,-0.015121460 , 0.021575928 ,-0.120697021 , 0.001068115 ,-0.791213989 ,
        1.101211548 , 0.349868774 , 0.146362305 , 0.035552979 , 0.030532837 , 0.001098633 , 0.002456665 , 0.000198364 ,
    },
    {
        -0.000030518 ,-0.000961304 , 0.003372192 ,-0.016235352 , 0.019531250 ,-0.125259399 ,-0.015228271 ,-0.816864014 ,
        1.089782715 , 0.323318481 , 0.148773193 , 0.030609131 , 0.029937744 , 0.000549316 , 0.002349854 , 0.000167847 ,
        -0.000030518 ,-0.000961304 , 0.003372192 ,-0.016235352 , 0.019531250 ,-0.125259399 ,-0.015228271 ,-0.816864014 ,
        1.089782715 , 0.323318481 , 0.148773193 , 0.030609131 , 0.029937744 , 0.000549316 , 0.002349854 , 0.000167847 ,
    },
    {
        -0.000030518 ,-0.001037598 , 0.003280640 ,-0.017349243 , 0.017257690 ,-0.129562378 ,-0.032379150 ,-0.841949463 ,
        1.077117920 , 0.297210693 , 0.150497437 , 0.025817871 , 0.029281616 , 0.000030518 , 0.002243042 , 0.000152588 ,
        -0.000030518 ,-0.001037598 , 0.003280640 ,-0.017349243 , 0.017257690 ,-0.129562378 ,-0.032379150 ,-0.841949463 ,
        1.077117920 , 0.297210693 , 0.150497437 , 0.025817871 , 0.029281616 , 0.000030518 , 0.002243042 , 0.000152588 ,
    },
    {
        -0.000045776 ,-0.001113892 , 0.003173828 ,-0.018463135 , 0.014801025 ,-0.133590698 ,-0.050354004 ,-0.866363525 ,
        1.063217163 , 0.271591187 , 0.151596069 , 0.021179199 , 0.028533936 ,-0.000442505 , 0.002120972 , 0.000137329 ,
        -0.000045776 ,-0.001113892 , 0.003173828 ,-0.018463135 , 0.014801025 ,-0.133590698 ,-0.050354004 ,-0.866363525 ,
        1.063217163 , 0.271591187 , 0.151596069 , 0.021179199 , 0.028533936 ,-0.000442505 , 0.002120972 , 0.000137329 ,
    },
    {
        -0.000045776 ,-0.001205444 , 0.003051758 ,-0.019577026 , 0.012115479 ,-0.137298584 ,-0.069168091 ,-0.890090942 ,
        1.048156738 , 0.246505737 , 0.152069092 , 0.016708374 , 0.027725220 ,-0.000869751 , 0.002014160 , 0.000122070 ,
        -0.000045776 ,-0.001205444 , 0.003051758 ,-0.019577026 , 0.012115479 ,-0.137298584 ,-0.069168091 ,-0.890090942 ,
        1.048156738 , 0.246505737 , 0.152069092 , 0.016708374 , 0.027725220 ,-0.000869751 , 0.002014160 , 0.000122070 ,
    },
    {
        -0.000061035 ,-0.001296997 , 0.002883911 ,-0.020690918 , 0.009231567 ,-0.140670776 ,-0.088775635 ,-0.913055420 ,
        1.031936646 , 0.221984863 , 0.151962280 , 0.012420654 , 0.026840210 ,-0.001266479 , 0.001907349 , 0.000106812 ,
        -0.000061035 ,-0.001296997 , 0.002883911 ,-0.020690918 , 0.009231567 ,-0.140670776 ,-0.088775635 ,-0.913055420 ,
        1.031936646 , 0.221984863 , 0.151962280 , 0.012420654 , 0.026840210 ,-0.001266479 , 0.001907349 , 0.000106812 ,
    },
    {
        -0.000061035 ,-0.001388550 , 0.002700806 ,-0.021789551 , 0.006134033 ,-0.143676758 ,-0.109161377 ,-0.935195923 ,
        1.014617920 , 0.198059082 , 0.151306152 , 0.008316040 , 0.025909424 ,-0.001617432 , 0.001785278 , 0.000106812 ,
        -0.000061035 ,-0.001388550 , 0.002700806 ,-0.021789551 , 0.006134033 ,-0.143676758 ,-0.109161377 ,-0.935195923 ,
        1.014617920 , 0.198059082 , 0.151306152 , 0.008316040 , 0.025909424 ,-0.001617432 , 0.001785278 , 0.000106812 ,
    },
    {
        -0.000076294 ,-0.001480103 , 0.002487183 ,-0.022857666 , 0.002822876 ,-0.146255493 ,-0.130310059 ,-0.956481934 ,
        0.996246338 , 0.174789429 , 0.150115967 , 0.004394531 , 0.024932861 ,-0.001937866 , 0.001693726 , 0.000091553 ,
        -0.000076294 ,-0.001480103 , 0.002487183 ,-0.022857666 , 0.002822876 ,-0.146255493 ,-0.130310059 ,-0.956481934 ,
        0.996246338 , 0.174789429 , 0.150115967 , 0.004394531 , 0.024932861 ,-0.001937866 , 0.001693726 , 0.000091553 ,
    },
    {
        -0.000076294 ,-0.001586914 , 0.002227783 ,-0.023910522 ,-0.000686646 ,-0.148422241 ,-0.152206421 ,-0.976852417 ,
        0.976852417 , 0.152206421 , 0.148422241 , 0.000686646 , 0.023910522 ,-0.002227783 , 0.001586914 , 0.000076294 ,
        -0.000076294 ,-0.001586914 , 0.002227783 ,-0.023910522 ,-0.000686646 ,-0.148422241 ,-0.152206421 ,-0.976852417 ,
        0.976852417 , 0.152206421 , 0.148422241 , 0.000686646 , 0.023910522 ,-0.002227783 , 0.001586914 , 0.000076294 ,
    }
};

const float CMpegAmp::b1 = 1.997590912;
const float CMpegAmp::b2 = 1.990369453;
const float CMpegAmp::b3 = 1.978353019;
const float CMpegAmp::b4 = 1.961570560;
const float CMpegAmp::b5 = 1.940062506;
const float CMpegAmp::b6 = 1.913880671;
const float CMpegAmp::b7 = 1.883088130;
const float CMpegAmp::b8 = 1.847759065;
const float CMpegAmp::b9 = 1.807978586;
const float CMpegAmp::b10= 1.763842529;
const float CMpegAmp::b11= 1.715457220;
const float CMpegAmp::b12= 1.662939225;
const float CMpegAmp::b13= 1.606415063;
const float CMpegAmp::b14= 1.546020907;
const float CMpegAmp::b15= 1.481902251;
const float CMpegAmp::b16= 1.414213562;
const float CMpegAmp::b17= 1.343117910;
const float CMpegAmp::b18= 1.268786568;
const float CMpegAmp::b19= 1.191398609;
const float CMpegAmp::b20= 1.111140466;
const float CMpegAmp::b21= 1.028205488;
const float CMpegAmp::b22= 0.942793474;
const float CMpegAmp::b23= 0.855110187;
const float CMpegAmp::b24= 0.765366865;
const float CMpegAmp::b25= 0.673779707;
const float CMpegAmp::b26= 0.580569355;
const float CMpegAmp::b27= 0.485960360;
const float CMpegAmp::b28= 0.390180644;
const float CMpegAmp::b29= 0.293460949;
const float CMpegAmp::b30= 0.196034281;
const float CMpegAmp::b31= 0.098135349;




/* ------------------------------------------------------------------------ */
/*
 *   Implementation 
 */

void CMpegAmp::imdct_init()
{
  int i;

  for(i=0;i<36;i++) /* 0 */
    win[0][i] = (float) sin(PI36 *(i+0.5));
  for(i=0;i<18;i++) /* 1 */
    win[1][i] = (float) sin(PI36 *(i+0.5));
  for(i=18;i<24;i++)
    win[1][i] = 1.0f;
  for(i=24;i<30;i++)
    win[1][i] = (float) sin(PI12 *(i+0.5-18));
  for(i=30;i<36;i++)
    win[1][i] = 0.0f;
  for(i=0;i<6;i++) /* 3 */
    win[3][i] = 0.0f;
  for(i=6;i<12;i++)
    win[3][i] = (float) sin(PI12 * (i+ 0.5 - 6.0));
  for(i=12;i<18;i++)
    win[3][i] = 1.0f;
  for(i=18;i<36;i++)
    win[3][i] = (float) sin(PI36 * (i + 0.5));
}

/* This uses Byeong Gi Lee's Fast Cosine Transform algorithm to decompose
   the 36 point and 12 point IDCT's into 9 point and 3 point IDCT's,
   respectively. Then the 9 point IDCT is computed by a modified version of
   Mikko Tommila's IDCT algorithm, based on the WFTA. See his comments
   before the first 9 point IDCT. The 3 point IDCT is already efficient to
   implement. -- Jeff Tsay. */
/* I got the unrolled IDCT from Jeff Tsay; the code is presumably by 
   Francois-Raymond Boyer - I unrolled it a little further. tu */

void CMpegAmp::imdct(int win_type,int sb,int ch)
{
/*------------------------------------------------------------------*/
/*                                                                  */
/*    Function: Calculation of the inverse MDCT                     */
/*    In the case of short blocks the 3 output vectors are already  */
/*    overlapped and added in this modul.                           */
/*                                                                  */
/*    New layer3                                                    */
/*                                                                  */
/*------------------------------------------------------------------*/

       float tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9, tmp10, tmp11;

       register float  save;
       float  pp1, pp2;
       float   *win_bt;
       int     i, p, ss;
                 float *in = xr[ch][sb];
                 float *s_p = s[ch][sb];
                 float *res_p = res[sb];
                 float out[36];

   if(win_type == 2){
                for(p=0;p<36;p+=9) {
                        out[p]   = out[p+1] = out[p+2] = out[p+3] =
                        out[p+4] = out[p+5] = out[p+6] = out[p+7] =
                        out[p+8] = 0.0f;
                }

        for(ss=0;ss<18;ss+=6) {

        /*
         *  12 point IMDCT
         */

                /* Begin 12 point IDCT */

                /* Input aliasing for 12 pt IDCT */
                in[5+ss]+=in[4+ss];in[4+ss]+=in[3+ss];in[3+ss]+=in[2+ss];
                in[2+ss]+=in[1+ss];in[1+ss]+=in[0+ss];

                /* Input aliasing on odd indices (for 6 point IDCT) */
                in[5+ss] += in[3+ss];  in[3+ss]  += in[1+ss];

                /* 3 point IDCT on even indices */

                pp2 = in[4+ss] * 0.500000000f;
                pp1 = in[2+ss] * 0.866025403f;
                save = in[0+ss] + pp2;
                tmp1 = in[0+ss] - in[4+ss];
                tmp0 = save + pp1;
                tmp2 = save - pp1;

                /* End 3 point IDCT on even indices */

                /* 3 point IDCT on odd indices (for 6 point IDCT) */

                pp2 = in[5+ss] * 0.500000000f;
                pp1 = in[3+ss] * 0.866025403f;
                save = in[1+ss] + pp2;
                tmp4 = in[1+ss] - in[5+ss];
                tmp5 = save + pp1;
                tmp3 = save - pp1;

                /* End 3 point IDCT on odd indices */

                /* Twiddle factors on odd indices (for 6 point IDCT) */

                tmp3 *= 1.931851653f;
                tmp4 *= 0.707106781f;
                tmp5 *= 0.517638090f;

                /* Output butterflies on 2 3 point IDCT's (for 6 point IDCT) */

                save = tmp0;
                tmp0 += tmp5;
                tmp5 = save - tmp5;

                save = tmp1;
                tmp1 += tmp4;
                tmp4 = save - tmp4;

                save = tmp2;
                tmp2 += tmp3;
                tmp3 = save - tmp3;

                /* End 6 point IDCT */

                /* Twiddle factors on indices (for 12 point IDCT) */

                tmp0 *= 0.504314480f;
                tmp1 *= 0.541196100f;
                tmp2 *= 0.630236207f;
                tmp3 *= 0.821339815f;
                tmp4 *= 1.306562965f;
                tmp5 *= 3.830648788f;

                /* End 12 point IDCT */

                /* Shift to 12 point modified IDCT, multiply by window type 2 */
                tmp8  = tmp0 * -0.793353340f;
                tmp9  = tmp0 * -0.608761429f;
                tmp7  = tmp1 * -0.923879532f;
                tmp10 = tmp1 * -0.382683432f;
                tmp6  = tmp2 * -0.991444861f;
                tmp11 = tmp2 * -0.130526192f;

                tmp0  = tmp3;
                tmp1  = tmp4 *  0.382683432f;
                tmp2  = tmp5 *  0.608761429f;

                tmp3  = tmp5 * -0.793353340f;
                tmp4  = tmp4 * -0.923879532f;
                tmp5  = tmp0 * -0.991444861f;

                tmp0 *= 0.130526192f;

                out[ss + 6]  += tmp0;
                out[ss + 7]  += tmp1;
                out[ss + 8]  += tmp2;
                out[ss + 9]  += tmp3;
                out[ss + 10] += tmp4;
                out[ss + 11] += tmp5;
                out[ss + 12] += tmp6;
                out[ss + 13] += tmp7;
                out[ss + 14] += tmp8;
                out[ss + 15] += tmp9;
                out[ss + 16] += tmp10;
                out[ss + 17] += tmp11;

        }
        if (sb&1) {
                for (i=0;i<18;i+=2) res_p[i]=out[i] + s_p[i];
                for (i=1;i<18;i+=2) res_p[i]=-out[i] - s_p[i];
        } else
                for (i=0;i<18;i++) res_p[i]=out[i] + s_p[i];
        for (i=18;i<36;i++) s_p[i-18]=out[i];

    } else {
/*
 * 36 point IDCT ****************************************************************
 */
        float tmp[18];

      /* input aliasing for 36 point IDCT */
      in[17]+=in[16]; in[16]+=in[15]; in[15]+=in[14]; in[14]+=in[13];
      in[13]+=in[12]; in[12]+=in[11]; in[11]+=in[10]; in[10]+=in[9];
      in[9] +=in[8];  in[8] +=in[7];  in[7] +=in[6];  in[6] +=in[5];
      in[5] +=in[4];  in[4] +=in[3];  in[3] +=in[2];  in[2] +=in[1];
      in[1] +=in[0];

      /* 18 point IDCT for odd indices */
      
      /* input aliasing for 18 point IDCT */
      in[17]+=in[15]; in[15]+=in[13]; in[13]+=in[11]; in[11]+=in[9];
      in[9] +=in[7];  in[7] +=in[5];  in[5] +=in[3];  in[3] +=in[1];
      

{
   float tmp0,tmp1,tmp2,tmp3,tmp4,tmp0_,tmp1_,tmp2_,tmp3_;
   float tmp0o,tmp1o,tmp2o,tmp3o,tmp4o,tmp0_o,tmp1_o,tmp2_o,tmp3_o;

/* Fast 9 Point Inverse Discrete Cosine Transform
//
// By  Francois-Raymond Boyer
//         mailto:boyerf@iro.umontreal.ca
//         http://www.iro.umontreal.ca/~boyerf
//
// The code has been optimized for Intel processors
//  (takes a lot of time to convert float to and from iternal FPU representation)
//
// It is a simple "factorization" of the IDCT matrix.
*/
   /* 9 point IDCT on even indices */
   {
   /* 5 points on odd indices (not realy an IDCT) */
   float i0 = in[0]+in[0];
   float i0p12 = i0 + in[12];

   tmp0 = i0p12 + in[4]*1.8793852415718f  + in[8]*1.532088886238f   + in[16]*0.34729635533386f;
   tmp1 = i0    + in[4]                   - in[8] - in[12] - in[12] - in[16];
   tmp2 = i0p12 - in[4]*0.34729635533386f - in[8]*1.8793852415718f  + in[16]*1.532088886238f;
   tmp3 = i0p12 - in[4]*1.532088886238f   + in[8]*0.34729635533386f - in[16]*1.8793852415718f;
   tmp4 = in[0] - in[4]                   + in[8] - in[12]          + in[16];
   }
   {
   float i6_ = in[6]*1.732050808f;              

   tmp0_ = in[2]*1.9696155060244f  + i6_ + in[10]*1.2855752193731f  + in[14]*0.68404028665134f;
   tmp1_ = (in[2]                        - in[10]                   - in[14])*1.732050808f;
   tmp2_ = in[2]*1.2855752193731f  - i6_ - in[10]*0.68404028665134f + in[14]*1.9696155060244f;
   tmp3_ = in[2]*0.68404028665134f - i6_ + in[10]*1.9696155060244f  - in[14]*1.2855752193731f;
   }

   /* 9 point IDCT on odd indices */
   {
   /* 5 points on odd indices (not realy an IDCT) */
   float i0 = in[0+1]+in[0+1];
   float i0p12 = i0 + in[12+1];

   tmp0o = i0p12   + in[4+1]*1.8793852415718f  + in[8+1]*1.532088886238f       + in[16+1]*0.34729635533386f;
   tmp1o = i0      + in[4+1]                   - in[8+1] - in[12+1] - in[12+1] - in[16+1];
   tmp2o = i0p12   - in[4+1]*0.34729635533386f - in[8+1]*1.8793852415718f      + in[16+1]*1.532088886238f;
   tmp3o = i0p12   - in[4+1]*1.532088886238f   + in[8+1]*0.34729635533386f     - in[16+1]*1.8793852415718f;
   tmp4o = (in[0+1] - in[4+1]                   + in[8+1] - in[12+1]            + in[16+1])*0.707106781f; /* Twiddled */
   }
   {
   /* 4 points on even indices */
   float i6_ = in[6+1]*1.732050808f;            /* Sqrt[3] */

   tmp0_o = in[2+1]*1.9696155060244f  + i6_ + in[10+1]*1.2855752193731f  + in[14+1]*0.68404028665134f;
   tmp1_o = (in[2+1]                        - in[10+1]                   - in[14+1])*1.732050808f;
   tmp2_o = in[2+1]*1.2855752193731f  - i6_ - in[10+1]*0.68404028665134f + in[14+1]*1.9696155060244f;
   tmp3_o = in[2+1]*0.68404028665134f - i6_ + in[10+1]*1.9696155060244f  - in[14+1]*1.2855752193731f;
   }

   /* Twiddle factors on odd indices
   // and
   // Butterflies on 9 point IDCT's
   // and
   // twiddle factors for 36 point IDCT
   */
   {
   float e, o;
   e = tmp0 + tmp0_; o = (tmp0o + tmp0_o)*0.501909918f; tmp[0] = (e + o)*(-0.500476342f*.5f);    tmp[17] = (e - o)*(-11.46279281f*.5f);
   e = tmp1 + tmp1_; o = (tmp1o + tmp1_o)*0.517638090f; tmp[1] = (e + o)*(-0.504314480f*.5f);    tmp[16] = (e - o)*(-3.830648788f*.5f);
   e = tmp2 + tmp2_; o = (tmp2o + tmp2_o)*0.551688959f; tmp[2] = (e + o)*(-0.512139757f*.5f);    tmp[15] = (e - o)*(-2.310113158f*.5f);
   e = tmp3 + tmp3_; o = (tmp3o + tmp3_o)*0.610387294f; tmp[3] = (e + o)*(-0.524264562f*.5f);    tmp[14] = (e - o)*(-1.662754762f*.5f);
                                                        tmp[4] = (tmp4 + tmp4o)*(-0.541196100f); tmp[13] = (tmp4 - tmp4o)*(-1.306562965f);
   e = tmp3 - tmp3_; o = (tmp3o - tmp3_o)*0.871723397f; tmp[5] = (e + o)*(-0.563690973f*.5f);    tmp[12] = (e - o)*(-1.082840285f*.5f);
   e = tmp2 - tmp2_; o = (tmp2o - tmp2_o)*1.183100792f; tmp[6] = (e + o)*(-0.592844523f*.5f);    tmp[11] = (e - o)*(-0.930579498f*.5f);
   e = tmp1 - tmp1_; o = (tmp1o - tmp1_o)*1.931851653f; tmp[7] = (e + o)*(-0.630236207f*.5f);    tmp[10] = (e - o)*(-0.821339815f*.5f);
   e = tmp0 - tmp0_; o = (tmp0o - tmp0_o)*5.736856623f; tmp[8] = (e + o)*(-0.678170852f*.5f);    tmp[9] =  (e - o)*(-0.740093616f*.5f);
   }
   }
        /* shift to modified IDCT */
        win_bt = win[win_type];

        if (sb&1) {
                res_p[0] =  -tmp[9]  * win_bt[0] + s_p[0];
                res_p[1] =-(-tmp[10] * win_bt[1] + s_p[1]);
                res_p[2] =  -tmp[11] * win_bt[2] + s_p[2];
                res_p[3] =-(-tmp[12] * win_bt[3] + s_p[3]);
                res_p[4] =  -tmp[13] * win_bt[4] + s_p[4];
                res_p[5] =-(-tmp[14] * win_bt[5] + s_p[5]);
                res_p[6] =  -tmp[15] * win_bt[6] + s_p[6];
                res_p[7] =-(-tmp[16] * win_bt[7] + s_p[7]);
                res_p[8] =  -tmp[17] * win_bt[8] + s_p[8];
           
                res_p[9] = -(tmp[17] * win_bt[9] + s_p[9]);
                res_p[10]=  tmp[16] * win_bt[10] + s_p[10];
                res_p[11]=-(tmp[15] * win_bt[11] + s_p[11]);
                res_p[12]=  tmp[14] * win_bt[12] + s_p[12];
                res_p[13]=-(tmp[13] * win_bt[13] + s_p[13]);
                res_p[14]=  tmp[12] * win_bt[14] + s_p[14];
                res_p[15]=-(tmp[11] * win_bt[15] + s_p[15]);
                res_p[16]=  tmp[10] * win_bt[16] + s_p[16];
                res_p[17]=-(tmp[9]  * win_bt[17] + s_p[17]);
        } else {
                res_p[0] = -tmp[9]  * win_bt[0] + s_p[0];
                res_p[1] = -tmp[10] * win_bt[1] + s_p[1];
                res_p[2] = -tmp[11] * win_bt[2] + s_p[2];
                res_p[3] = -tmp[12] * win_bt[3] + s_p[3];
                res_p[4] = -tmp[13] * win_bt[4] + s_p[4];
                res_p[5] = -tmp[14] * win_bt[5] + s_p[5];
                res_p[6] = -tmp[15] * win_bt[6] + s_p[6];
                res_p[7] = -tmp[16] * win_bt[7] + s_p[7];
                res_p[8] = -tmp[17] * win_bt[8] + s_p[8];
           
                res_p[9] = tmp[17] * win_bt[9] + s_p[9];
                res_p[10]= tmp[16] * win_bt[10] + s_p[10];
                res_p[11]= tmp[15] * win_bt[11] + s_p[11];
                res_p[12]= tmp[14] * win_bt[12] + s_p[12];
                res_p[13]= tmp[13] * win_bt[13] + s_p[13];
                res_p[14]= tmp[12] * win_bt[14] + s_p[14];
                res_p[15]= tmp[11] * win_bt[15] + s_p[15];
                res_p[16]= tmp[10] * win_bt[16] + s_p[16];
                res_p[17]= tmp[9]  * win_bt[17] + s_p[17];
        }

        s_p[0]= tmp[8]  * win_bt[18];
        s_p[1]= tmp[7]  * win_bt[19];
        s_p[2]= tmp[6]  * win_bt[20];
        s_p[3]= tmp[5]  * win_bt[21];
        s_p[4]= tmp[4]  * win_bt[22];
        s_p[5]= tmp[3]  * win_bt[23];
        s_p[6]= tmp[2]  * win_bt[24];
        s_p[7]= tmp[1]  * win_bt[25];
        s_p[8]= tmp[0]  * win_bt[26];

        s_p[9]= tmp[0]  * win_bt[27];
        s_p[10]= tmp[1]  * win_bt[28];
        s_p[11]= tmp[2]  * win_bt[29];
        s_p[12]= tmp[3]  * win_bt[30];
        s_p[13]= tmp[4]  * win_bt[31];
        s_p[14]= tmp[5]  * win_bt[32];
        s_p[15]= tmp[6]  * win_bt[33];
        s_p[16]= tmp[7]  * win_bt[34];
        s_p[17]= tmp[8]  * win_bt[35];
    }
}

/* fast DCT according to Lee[84]
 * reordering according to Konstantinides[94]
 */ 
void CMpegAmp::poly(const int ch,int f)
{
int start = u_start[ch];
int div = u_div[ch];
float (*u_p)[16];

#if defined(PENTIUM_RDTSC)
unsigned int cnt4, cnt3, cnt2, cnt1;

        __asm__(".byte 0x0f,0x31" : "=a" (cnt1), "=d" (cnt4));
#endif

        {
        float d16,d17,d18,d19,d20,d21,d22,d23,d24,d25,d26,d27,d28,d29,d30,d31;
        float d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15;

        /* step 1: initial reordering and 1st (16 wide) butterflies
        */

        d0 = res[ 0][f]; d16=(d0  - res[31][f]) *  b1; d0 += res[31][f];
        d1 = res[ 1][f]; d17=(d1  - res[30][f]) *  b3; d1 += res[30][f];
        d3 = res[ 2][f]; d19=(d3  - res[29][f]) *  b5; d3 += res[29][f];
        d2 = res[ 3][f]; d18=(d2  - res[28][f]) *  b7; d2 += res[28][f];
        d6 = res[ 4][f]; d22=(d6  - res[27][f]) *  b9; d6 += res[27][f];
        d7 = res[ 5][f]; d23=(d7  - res[26][f]) * b11; d7 += res[26][f];
        d5 = res[ 6][f]; d21=(d5  - res[25][f]) * b13; d5 += res[25][f];
        d4 = res[ 7][f]; d20=(d4  - res[24][f]) * b15; d4 += res[24][f];
        d12= res[ 8][f]; d28=(d12 - res[23][f]) * b17; d12+= res[23][f];
        d13= res[ 9][f]; d29=(d13 - res[22][f]) * b19; d13+= res[22][f];
        d15= res[10][f]; d31=(d15 - res[21][f]) * b21; d15+= res[21][f];
        d14= res[11][f]; d30=(d14 - res[20][f]) * b23; d14+= res[20][f];
        d10= res[12][f]; d26=(d10 - res[19][f]) * b25; d10+= res[19][f];
        d11= res[13][f]; d27=(d11 - res[18][f]) * b27; d11+= res[18][f];
        d9 = res[14][f]; d25=(d9  - res[17][f]) * b29; d9 += res[17][f];
        d8 = res[15][f]; d24=(d8  - res[16][f]) * b31; d8 += res[16][f];

        {
        float c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15;

/* a test to see what can be done with memory separation
 * first we process indexes 0-15
*/
        c0 = d0 + d8 ; c8 = ( d0 - d8 ) *  b2;
        c1 = d1 + d9 ; c9 = ( d1 - d9 ) *  b6;
        c2 = d2 + d10; c10= ( d2 - d10) * b14;
        c3 = d3 + d11; c11= ( d3 - d11) * b10;
        c4 = d4 + d12; c12= ( d4 - d12) * b30;
        c5 = d5 + d13; c13= ( d5 - d13) * b26;
        c6 = d6 + d14; c14= ( d6 - d14) * b18;
        c7 = d7 + d15; c15= ( d7 - d15) * b22;
        
        /* step 3: 4-wide butterflies
        */
        d0 = c0 + c4 ; d4 = ( c0 - c4 ) *  b4;
        d1 = c1 + c5 ; d5 = ( c1 - c5 ) * b12;
        d2 = c2 + c6 ; d6 = ( c2 - c6 ) * b28;
        d3 = c3 + c7 ; d7 = ( c3 - c7 ) * b20;
        
        d8 = c8 + c12; d12= ( c8 - c12) *  b4;
        d9 = c9 + c13; d13= ( c9 - c13) * b12;
        d10= c10+ c14; d14= (c10 - c14) * b28;
        d11= c11+ c15; d15= (c11 - c15) * b20;


        /* step 4: 2-wide butterflies
        */
        {
        float rb8 = b8;
        float rb24 = b24;

/**/    c0 = d0 + d2 ; c2 = ( d0 - d2 ) *  rb8;
        c1 = d1 + d3 ; c3 = ( d1 - d3 ) * rb24;
/**/    c4 = d4 + d6 ; c6 = ( d4 - d6 ) *  rb8;
        c5 = d5 + d7 ; c7 = ( d5 - d7 ) * rb24;
/**/    c8 = d8 + d10; c10= ( d8 - d10) *  rb8;
        c9 = d9 + d11; c11= ( d9 - d11) * rb24;
/**/    c12= d12+ d14; c14= (d12 - d14) *  rb8;
        c13= d13+ d15; c15= (d13 - d15) * rb24;
        }

        /* step 5: 1-wide butterflies
        */
        {
        float rb16 = b16;

        /* this is a little 'hacked up'
        */
        d0 = (-c0 -c1) * 2; d1 = ( c0 - c1 ) * rb16; 
        d2 = c2 + c3; d3 = ( c2 - c3 ) * rb16; 
        d3 -= d2;

        d4 = c4 +c5; d5 = ( c4 - c5 ) * rb16;
        d5 += d4;
        d7 = -d5;
        d7 += ( c6 - c7 ) * rb16; d6 = +c6 +c7;

        d8 = c8 + c9 ; d9 = ( c8 - c9 ) * rb16;
        d11= +d8 +d9;
        d11 +=(c10 - c11) * rb16; d10= c10+ c11; 

        d12 = c12+ c13; d13 = (c12 - c13) * rb16;
        d13 += -d8-d9+d12;
        d14 = c14+ c15; d15 = (c14 - c15) * rb16;
        d15-=d11;
        d14 += -d8 -d10;
        }

        /* step 6: final resolving & reordering
         * the other 32 are stored for use with the next granule
         */

        u_p = (float (*)[16]) &poly_u[ch][div][0][start];

/*16*/  u_p[ 0][0] =+d1 ;
        u_p[ 2][0] = +d9 -d14;
/*20*/  u_p[ 4][0] = +d5 -d6;
        u_p[ 6][0] = -d10 +d13;
/*24*/  u_p[ 8][0] =d3;
        u_p[10][0] = -d8 -d9 +d11 -d13;
/*28*/  u_p[12][0] = +d7;
        u_p[14][0] = +d15;

        /* the other 32 are stored for use with the next granule
         */

        u_p = (float (*)[16]) &poly_u[ch][!div][0][start];

/*0*/   u_p[16][0] = d0;
        u_p[14][0] = -(+d8 );
/*4*/   u_p[12][0] = -(+d4 );
        u_p[10][0] = -(-d8 +d12 );
/*8*/   u_p[ 8][0] = -(+d2 );
        u_p[ 6][0] = -(+d8 +d10 -d12 );
/*12*/  u_p[ 4][0] = -(-d4 +d6 );
        u_p[ 2][0] = -d14;
        u_p[ 0][0] = -d1;
        }

        {
        float c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15;

/* memory separation, second part
*/
/* 2
*/
        c0=d16 + d24; c8= (d16 - d24) *  b2;
        c1=d17 + d25; c9= (d17 - d25) *  b6;
        c2=d18 + d26; c10= (d18 - d26) * b14;
        c3=d19 + d27; c11= (d19 - d27) * b10;
        c4=d20 + d28; c12= (d20 - d28) * b30;
        c5=d21 + d29; c13= (d21 - d29) * b26;
        c6=d22 + d30; c14= (d22 - d30) * b18;
        c7=d23 + d31; c15= (d23 - d31) * b22;

/* 3
*/
        d16= c0+ c4; d20= (c0 - c4) *  b4;
        d17= c1+ c5; d21= (c1 - c5) * b12;
        d18= c2+ c6; d22= (c2 - c6) * b28;
        d19= c3+ c7; d23= (c3 - c7) * b20;

        d24= c8+ c12; d28= (c8 - c12) *  b4;
        d25= c9+ c13; d29= (c9 - c13) * b12;
        d26= c10+ c14; d30= (c10 - c14) * b28;
        d27= c11+ c15; d31= (c11 - c15) * b20;

/* 4
*/
        {
        float rb8 = b8;
        float rb24 = b24;

/**/    c0= d16+ d18; c2= (d16 - d18) *  rb8;
        c1= d17+ d19; c3= (d17 - d19) * rb24;
/**/    c4= d20+ d22; c6= (d20 - d22) *  rb8;
        c5= d21+ d23; c7= (d21 - d23) * rb24;
/**/    c8= d24+ d26; c10= (d24 - d26) *  rb8;
        c9= d25+ d27; c11= (d25 - d27) * rb24;
/**/    c12= d28+ d30; c14= (d28 - d30) *  rb8;
        c13= d29+ d31; c15= (d29 - d31) * rb24;
        }

/* 5
*/
        {
        float rb16 = b16;
        d16= c0+ c1; d17= (c0 - c1) * rb16;
        d18= c2+ c3; d19= (c2 - c3) * rb16;

        d20= c4+ c5; d21= (c4 - c5) * rb16;
        d20+=d16; d21+=d17;
        d22= c6+ c7; d23= (c6 - c7) * rb16;
        d22+=d16; d22+=d18;
        d23+=d16; d23+=d17; d23+=d19;


        d24= c8+ c9; d25= (c8 - c9) * rb16;
        d26= c10+ c11; d27= (c10 - c11) * rb16;
        d26+=d24;
        d27+=d24; d27+=d25;

        d28= c12+ c13; d29= (c12 - c13) * rb16;
        d28-=d20; d29+=d28; d29-=d21;
        d30= c14+ c15; d31= (c14 - c15) * rb16;
        d30-=d22;
        d31-=d23;
        }

        /* step 6: final resolving & reordering 
         * the other 32 are stored for use with the next granule
         */
        
        u_p = (float (*)[16]) &poly_u[ch][!div][0][start];

        u_p[ 1][0] = -(+d30 );  
        u_p[ 3][0] = -(+d22 -d26 );
        u_p[ 5][0] = -(-d18 -d20 +d26 );
        u_p[ 7][0] = -(+d18 -d28 );
        u_p[ 9][0] = -(+d28 );
        u_p[11][0] = -(+d20 -d24 );
        u_p[13][0] = -(-d16 +d24 );
        u_p[15][0] = -(+d16 );

        /* the other 32 are stored for use with the next granule
         */

        u_p = (float (*)[16]) &poly_u[ch][div][0][start];

        u_p[15][0] = +d31;
        u_p[13][0] = +d23 -d27;
        u_p[11][0] = -d19 -d20 -d21 +d27;
        u_p[ 9][0] = +d19 -d29;
        u_p[ 7][0] = -d18 +d29;
        u_p[ 5][0] = +d18 +d20 +d21 -d25 -d26;
        u_p[ 3][0] = -d17 -d22 +d25 +d26;
        u_p[ 1][0] = +d17 -d30;
        }
        }

#if defined(PENTIUM_RDTSC)
        __asm__(".byte 0x0f,0x31" : "=a" (cnt3), "=d" (cnt4));
#endif

        /* we're doing dewindowing and calculating final samples now
         */

#if defined(ARCH_i586)
        /* x86 assembler optimisations.  These optimisations are tuned
           specifically for Intel Pentiums. */

            asm("movl $15,%%eax\n\t"\
        "1:\n\t"\
        "flds (%0)\n\t"\
        "fmuls (%1)\n\t"\
        "flds 4(%0)\n\t"\
        "fmuls 4(%1)\n\t"\
        "flds 8(%0)\n\t"\
        "fmuls 8(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 12(%0)\n\t"\
        "fmuls 12(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 16(%0)\n\t"\
        "fmuls 16(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 20(%0)\n\t"\
        "fmuls 20(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 24(%0)\n\t"\
        "fmuls 24(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 28(%0)\n\t"\
        "fmuls 28(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 32(%0)\n\t"\
        "fmuls 32(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 36(%0)\n\t"\
        "fmuls 36(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 40(%0)\n\t"\
        "fmuls 40(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 44(%0)\n\t"\
        "fmuls 44(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 48(%0)\n\t"\
        "fmuls 48(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 52(%0)\n\t"\
        "fmuls 52(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 56(%0)\n\t"\
        "fmuls 56(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 60(%0)\n\t"\
        "fmuls 60(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "addl $64,%0\n\t"\
        "addl $128,%1\n\t"\
        "subl $4,%%esp\n\t"\
        "faddp\n\t"\
        "fistpl (%%esp)\n\t"\
        "popl %%ecx\n\t"\
        "cmpl $32767,%%ecx\n\t"\
        "jle 2f\n\t"\
        "movw $32767,%%cx\n\t"\
        "jmp 3f\n\t"\
        "2: cmpl $-32768,%%ecx\n\t"\
        "jge 3f\n\t"\
        "movw $-32768,%%cx\n\t"\
        "3: movw %%cx,(%2)\n\t"\
        "addl %3,%2\n\t"\
        "decl %%eax\n\t"\
        "jns 1b\n\t"\

        "testb $1,%4\n\t"\
        "je 4f\n\t"

        "flds (%0)\n\t"\
        "fmuls (%1)\n\t"\
        "flds 8(%0)\n\t"\
        "fmuls 8(%1)\n\t"\
        "flds 16(%0)\n\t"\
        "fmuls 16(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 24(%0)\n\t"\
        "fmuls 24(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 32(%0)\n\t"\
        "fmuls 32(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 40(%0)\n\t"\
        "fmuls 40(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 48(%0)\n\t"\
        "fmuls 48(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 56(%0)\n\t"\
        "fmuls 56(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "subl $4,%%esp\n\t"\
        "subl $64,%0\n\t"\
        "subl $192,%1\n\t"\
        "faddp\n\t"\
        "fistpl (%%esp)\n\t"\
        "popl %%ecx\n\t"\
        "cmpl $32767,%%ecx\n\t"\
        "jle 2f\n\t"\
        "movw $32767,%%cx\n\t"\
        "jmp 3f\n\t"\
        "2: cmpl $-32768,%%ecx\n\t"\
        "jge 3f\n\t"\
        "movw $-32768,%%cx\n\t"\
        "3: movw %%cx,(%2)\n\t"\

        "movl %5,%%ecx\n\t"\
        "sall $3,%%ecx\n\t"\
        "addl %%ecx,%1\n\t"\
        "addl %3,%2\n\t"\
        "movl $14,%%eax\n\t"\

        "1:flds 4(%0)\n\t"\
        "fmuls 56(%1)\n\t"\
        "flds (%0)\n\t"\
        "fmuls 60(%1)\n\t"\
        "flds 12(%0)\n\t"\
        "fmuls 48(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubp\n\t"\
        "flds 8(%0)\n\t"\
        "fmuls 52(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 20(%0)\n\t"\
        "fmuls 40(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 16(%0)\n\t"\
        "fmuls 44(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 28(%0)\n\t"\
        "fmuls 32(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 24(%0)\n\t"\
        "fmuls 36(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 36(%0)\n\t"\
        "fmuls 24(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 32(%0)\n\t"\
        "fmuls 28(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 44(%0)\n\t"\
        "fmuls 16(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 40(%0)\n\t"\
        "fmuls 20(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 52(%0)\n\t"\
        "fmuls 8(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 48(%0)\n\t"\
        "fmuls 12(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 60(%0)\n\t"\
        "fmuls (%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 56(%0)\n\t"\
        "fmuls 4(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "subl $64,%0\n\t"\
        "subl $128,%1\n\t"\
        "subl $4,%%esp\n\t"\
        "fsubp\n\t"\
        "fistpl (%%esp)\n\t"\
        "popl %%ecx\n\t"\
        "cmpl $32767,%%ecx\n\t"\
        "jle 2f\n\t"\
        "movw $32767,%%cx\n\t"\
        "jmp 3f\n\t"\
        "2: cmpl $-32768,%%ecx\n\t"\
        "jge 3f\n\t"\
        "movw $-32768,%%cx\n\t"\
        "3: movw %%cx,(%2)\n\t"\
        "addl %3,%2\n\t"\
        "decl %%eax\n\t"\
        "jns 1b\n\t"\
        "jmp 5f\n\t"\

        "4:flds 4(%0)\n\t"\
        "fmuls 4(%1)\n\t"\
        "flds 12(%0)\n\t"\
        "fmuls 12(%1)\n\t"\
        "flds 20(%0)\n\t"\
        "fmuls 20(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 28(%0)\n\t"\
        "fmuls 28(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 36(%0)\n\t"\
        "fmuls 36(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 44(%0)\n\t"\
        "fmuls 44(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 52(%0)\n\t"\
        "fmuls 52(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 60(%0)\n\t"\
        "fmuls 60(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "subl $4,%%esp\n\t"\
        "subl $64,%0\n\t"\
        "subl $192,%1\n\t"\
        "faddp\n\t"\
        "fistpl (%%esp)\n\t"\
        "popl %%ecx\n\t"\
        "cmpl $32767,%%ecx\n\t"\
        "jle 2f\n\t"\
        "movw $32767,%%cx\n\t"\
        "jmp 3f\n\t"\
        "2: cmpl $-32768,%%ecx\n\t"\
        "jge 3f\n\t"\
        "movw $-32768,%%cx\n\t"\
        "3: movw %%cx,(%2)\n\t"\

        "movl %5,%%ecx\n\t"\
        "sall $3,%%ecx\n\t"\
        "addl %%ecx,%1\n\t"\
        "addl %3,%2\n\t"\

        "movl $14,%%eax\n\t"\
        "1:flds (%0)\n\t"\
        "fmuls 60(%1)\n\t"\
        "flds 4(%0)\n\t"\
        "fmuls 56(%1)\n\t"\
        "flds 8(%0)\n\t"\
        "fmuls 52(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubp\n\t"\
        "flds 12(%0)\n\t"\
        "fmuls 48(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 16(%0)\n\t"\
        "fmuls 44(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 20(%0)\n\t"\
        "fmuls 40(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 24(%0)\n\t"\
        "fmuls 36(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 28(%0)\n\t"\
        "fmuls 32(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 32(%0)\n\t"\
        "fmuls 28(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 36(%0)\n\t"\
        "fmuls 24(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 40(%0)\n\t"\
        "fmuls 20(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 44(%0)\n\t"\
        "fmuls 16(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 48(%0)\n\t"\
        "fmuls 12(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 52(%0)\n\t"\
        "fmuls 8(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "flds 56(%0)\n\t"\
        "fmuls 4(%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "fsubrp\n\t"\
        "flds 60(%0)\n\t"\
        "fmuls (%1)\n\t"\
        "fxch %%st(2)\n\t"\
        "faddp\n\t"\
        "subl $64,%0\n\t"\
        "subl $128,%1\n\t"\
        "subl $4,%%esp\n\t"\
        "fsubp\n\t"\
        "fistpl (%%esp)\n\t"\
        "popl %%ecx\n\t"\
        "cmpl $32767,%%ecx\n\t"\
        "jle 2f\n\t"\
        "movw $32767,%%cx\n\t"\
        "jmp 3f\n\t"\
        "2: cmpl $-32768,%%ecx\n\t"\
        "jge 3f\n\t"\
        "movw $-32768,%%cx\n\t"\
        "3: movw %%cx,(%2)\n\t"\
        "addl %3,%2\n\t"\
        "decl %%eax\n\t"\
        "jns 1b\n\t"\

        "5:"\
            : : "b" (poly_u[ch][div]), "d" (t_dewindow[0] + 16 - start), "S" (&sample_buffer[f>>(2-nch)][nch==2?0:(f&1?16:0)][ch]), "m" (sizeof(short) * nch), "m" (div), "m" (start)\
            : "eax", "ecx", "memory");
#else
        {
          short *samples = (&sample_buffer[f>>(2-nch)][nch==2?0:(f&1?16:0)][ch]);
          int out, j;

#define PUT_SAMPLE(out)                 \
                if (out > 32767)        \
                  *samples = 32767;     \
                else                    \
                  if (out < -32768)     \
                    *samples = -32768;  \
                  else                  \
                    *samples = out;     \
                                        \
                samples += nch;

#if defined(SUPERHACK)
          /* These is a simple implementation which should be nicer to the
             cache; computation of samples are done in one pass rather than
             two.  However, for various reasons which I do not have time to
             investigate, it runs quite a lot slower than two pass
             computations.  If you have time, you are welcome to look into
             it. */

          {
            float (*u_ptr)[16] = poly_u[ch][div];
            const float *dewindow2 = t_dewindow[0] + start;

            {
              float outf1, outf2, outf3, outf4;

              outf1  = u_ptr[0][ 0] * dewindow[0x0];
              outf2  = u_ptr[0][ 1] * dewindow[0x1];
              outf3  = u_ptr[0][ 2] * dewindow[0x2];
              outf4  = u_ptr[0][ 3] * dewindow[0x3];
              outf1 += u_ptr[0][ 4] * dewindow[0x4];
              outf2 += u_ptr[0][ 5] * dewindow[0x5];
              outf3 += u_ptr[0][ 6] * dewindow[0x6];
              outf4 += u_ptr[0][ 7] * dewindow[0x7];
              outf1 += u_ptr[0][ 8] * dewindow[0x8];
              outf2 += u_ptr[0][ 9] * dewindow[0x9];
              outf3 += u_ptr[0][10] * dewindow[0xa];
              outf4 += u_ptr[0][11] * dewindow[0xb];
              outf1 += u_ptr[0][12] * dewindow[0xc];
              outf2 += u_ptr[0][13] * dewindow[0xd];
              outf3 += u_ptr[0][14] * dewindow[0xe];
              outf4 += u_ptr[0][15] * dewindow[0xf];

              out = outf1 + outf2 + outf3 + outf4;

              dewindow += 32;
              dewindow2 += 32;
              u_ptr++;

              if (out > 32767)
                samples[0] = 32767;
              else
                if (out < -32768)
                  samples[0] = -32768;
                else
                  samples[0] = out;
            }

            if (div & 0x1) {
              for (j = 1; j < 16; ++j) {
                float outf1, outf2, outf3, outf4;

                outf1  = u_ptr[0][ 0] * dewindow[0x0];
                outf3  = u_ptr[0][ 0] * dewindow2[0xf];
                outf2  = u_ptr[0][ 1] * dewindow[0x1];
                outf4  = u_ptr[0][ 1] * dewindow2[0xe];
                outf1 += u_ptr[0][ 2] * dewindow[0x2];
                outf3 += u_ptr[0][ 2] * dewindow2[0xd];
                outf2 += u_ptr[0][ 3] * dewindow[0x3];
                outf4 += u_ptr[0][ 3] * dewindow2[0xc];
                outf1 += u_ptr[0][ 4] * dewindow[0x4];
                outf3 += u_ptr[0][ 4] * dewindow2[0xb];
                outf2 += u_ptr[0][ 5] * dewindow[0x5];
                outf4 += u_ptr[0][ 5] * dewindow2[0xa];
                outf1 += u_ptr[0][ 6] * dewindow[0x6];
                outf3 += u_ptr[0][ 6] * dewindow2[0x9];
                outf2 += u_ptr[0][ 7] * dewindow[0x7];
                outf4 += u_ptr[0][ 7] * dewindow2[0x8];
                outf1 += u_ptr[0][ 8] * dewindow[0x8];
                outf3 += u_ptr[0][ 8] * dewindow2[0x7];
                outf2 += u_ptr[0][ 9] * dewindow[0x9];
                outf4 += u_ptr[0][ 9] * dewindow2[0x6];
                outf1 += u_ptr[0][10] * dewindow[0xa];
                outf3 += u_ptr[0][10] * dewindow2[0x5];
                outf2 += u_ptr[0][11] * dewindow[0xb];
                outf4 += u_ptr[0][11] * dewindow2[0x4];
                outf1 += u_ptr[0][12] * dewindow[0xc];
                outf3 += u_ptr[0][12] * dewindow2[0x3];
                outf2 += u_ptr[0][13] * dewindow[0xd];
                outf4 += u_ptr[0][13] * dewindow2[0x2];
                outf1 += u_ptr[0][14] * dewindow[0xe];
                outf3 += u_ptr[0][14] * dewindow2[0x1];
                outf2 += u_ptr[0][15] * dewindow[0xf];
                outf4 += u_ptr[0][15] * dewindow2[0x0];

                dewindow += 32;
                dewindow2 += 32;
                u_ptr++;

                out = outf1 + outf2;

                if (out > 32767)
                  samples[j * 2] = 32767;
                else
                  if (out < -32768)
                    samples[j * 2] = -32768;
                  else
                    samples[j * 2] = out;

                out = outf4 - outf3;

                if (out > 32767)
                  samples[64 - (j * 2)] = 32767;
                else
                  if (out < -32768)
                    samples[64 - (j * 2)] = -32768;
                  else
                    samples[64 - (j * 2)] = out;
              }

              {
                float outf2, outf4;

                outf2  = u_ptr[0][ 0] * dewindow[0x0];
                outf4  = u_ptr[0][ 2] * dewindow[0x2];
                outf2 += u_ptr[0][ 4] * dewindow[0x4];
                outf4 += u_ptr[0][ 6] * dewindow[0x6];
                outf2 += u_ptr[0][ 8] * dewindow[0x8];
                outf4 += u_ptr[0][10] * dewindow[0xa];
                outf2 += u_ptr[0][12] * dewindow[0xc];
                outf4 += u_ptr[0][14] * dewindow[0xe];

                out = outf2 + outf4;

                if (out > 32767)
                  samples[16 * 2] = 32767;
                else
                  if (out < -32768)
                    samples[16 * 2] = -32768;
                  else
                    samples[16 * 2] = out;
              }
            } else {
              for (j = 1; j < 16; ++j) {
                float outf1, outf2, outf3, outf4;

                outf1  = u_ptr[0][ 0] * dewindow[0x0];
                outf3  = u_ptr[0][ 0] * dewindow2[0xf];
                outf2  = u_ptr[0][ 1] * dewindow[0x1];
                outf4  = u_ptr[0][ 1] * dewindow2[0xe];
                outf1 += u_ptr[0][ 2] * dewindow[0x2];
                outf3 += u_ptr[0][ 2] * dewindow2[0xd];
                outf2 += u_ptr[0][ 3] * dewindow[0x3];
                outf4 += u_ptr[0][ 3] * dewindow2[0xc];
                outf1 += u_ptr[0][ 4] * dewindow[0x4];
                outf3 += u_ptr[0][ 4] * dewindow2[0xb];
                outf2 += u_ptr[0][ 5] * dewindow[0x5];
                outf4 += u_ptr[0][ 5] * dewindow2[0xa];
                outf1 += u_ptr[0][ 6] * dewindow[0x6];
                outf3 += u_ptr[0][ 6] * dewindow2[0x9];
                outf2 += u_ptr[0][ 7] * dewindow[0x7];
                outf4 += u_ptr[0][ 7] * dewindow2[0x8];
                outf1 += u_ptr[0][ 8] * dewindow[0x8];
                outf3 += u_ptr[0][ 8] * dewindow2[0x7];
                outf2 += u_ptr[0][ 9] * dewindow[0x9];
                outf4 += u_ptr[0][ 9] * dewindow2[0x6];
                outf1 += u_ptr[0][10] * dewindow[0xa];
                outf3 += u_ptr[0][10] * dewindow2[0x5];
                outf2 += u_ptr[0][11] * dewindow[0xb];
                outf4 += u_ptr[0][11] * dewindow2[0x4];
                outf1 += u_ptr[0][12] * dewindow[0xc];
                outf3 += u_ptr[0][12] * dewindow2[0x3];
                outf2 += u_ptr[0][13] * dewindow[0xd];
                outf4 += u_ptr[0][13] * dewindow2[0x2];
                outf1 += u_ptr[0][14] * dewindow[0xe];
                outf3 += u_ptr[0][14] * dewindow2[0x1];
                outf2 += u_ptr[0][15] * dewindow[0xf];
                outf4 += u_ptr[0][15] * dewindow2[0x0];

                dewindow += 32;
                dewindow2 += 32;
                u_ptr++;

                out = outf1 + outf2;

                if (out > 32767)
                  samples[j * 2] = 32767;
                else
                  if (out < -32768)
                    samples[j * 2] = -32768;
                  else
                    samples[j * 2] = out;

                out = outf3 - outf4;

                if (out > 32767)
                  samples[64 - (j * 2)] = 32767;
                else
                  if (out < -32768)
                    samples[64 - (j * 2)] = -32768;
                  else
                    samples[64 - (j * 2)] = out;
              }

              {
                float outf2, outf4;

                outf2  = u_ptr[0][ 1] * dewindow[0x1];
                outf4  = u_ptr[0][ 3] * dewindow[0x3];
                outf2 += u_ptr[0][ 5] * dewindow[0x5];
                outf4 += u_ptr[0][ 7] * dewindow[0x7];
                outf2 += u_ptr[0][ 9] * dewindow[0x9];
                outf4 += u_ptr[0][11] * dewindow[0xb];
                outf2 += u_ptr[0][13] * dewindow[0xd];
                outf4 += u_ptr[0][15] * dewindow[0xf];

                out = outf2 + outf4;

                if (out > 32767)
                  samples[16 * 2] = 32767;
                else
                  if (out < -32768)
                    samples[16 * 2] = -32768;
                  else
                    samples[16 * 2] = out;
              }
            }
          }
#elif defined(HAS_AUTOINCREMENT)
          const float *dewindow = t_dewindow[0] + 15 - start;

          /* This is tuned specifically for architectures with
             autoincrement and -decrement. */

          {
            float *u_ptr = (float*) poly_u[ch][div];

            u_ptr--;

            for (j = 0; j < 16; ++j) {
              float outf1, outf2, outf3, outf4;

              outf1  = *++u_ptr * *++dewindow;
              outf2  = *++u_ptr * *++dewindow;
              outf3  = *++u_ptr * *++dewindow;
              outf4  = *++u_ptr * *++dewindow;
              outf1 += *++u_ptr * *++dewindow;
              outf2 += *++u_ptr * *++dewindow;
              outf3 += *++u_ptr * *++dewindow;
              outf4 += *++u_ptr * *++dewindow;
              outf1 += *++u_ptr * *++dewindow;
              outf2 += *++u_ptr * *++dewindow;
              outf3 += *++u_ptr * *++dewindow;
              outf4 += *++u_ptr * *++dewindow;
              outf1 += *++u_ptr * *++dewindow;
              outf2 += *++u_ptr * *++dewindow;
              outf3 += *++u_ptr * *++dewindow;
              outf4 += *++u_ptr * *++dewindow;

              out = outf1 + outf2 + outf3 + outf4;

              dewindow += 16;

              PUT_SAMPLE(out)
            }

            if (div & 0x1) {
              {
                float outf2, outf4;

                outf2  = u_ptr[ 1] * dewindow[0x1];
                outf4  = u_ptr[ 3] * dewindow[0x3];
                outf2 += u_ptr[ 5] * dewindow[0x5];
                outf4 += u_ptr[ 7] * dewindow[0x7];
                outf2 += u_ptr[ 9] * dewindow[0x9];
                outf4 += u_ptr[11] * dewindow[0xb];
                outf2 += u_ptr[13] * dewindow[0xd];
                outf4 += u_ptr[15] * dewindow[0xf];

                out = outf2 + outf4;

                PUT_SAMPLE(out)
              }

              dewindow -= 31;
              dewindow += start;
              dewindow += start;
              u_ptr -= 16;

              for (; j < 31; ++j) {
                float outf1, outf2, outf3, outf4;

                outf1  = *++u_ptr * *--dewindow;
                outf2  = *++u_ptr * *--dewindow;
                outf3  = *++u_ptr * *--dewindow;
                outf4  = *++u_ptr * *--dewindow;
                outf1 += *++u_ptr * *--dewindow;
                outf2 += *++u_ptr * *--dewindow;
                outf3 += *++u_ptr * *--dewindow;
                outf4 += *++u_ptr * *--dewindow;
                outf1 += *++u_ptr * *--dewindow;
                outf2 += *++u_ptr * *--dewindow;
                outf3 += *++u_ptr * *--dewindow;
                outf4 += *++u_ptr * *--dewindow;
                outf1 += *++u_ptr * *--dewindow;
                outf2 += *++u_ptr * *--dewindow;
                outf3 += *++u_ptr * *--dewindow;
                outf4 += *++u_ptr * *--dewindow;

                out = outf2 - outf1 + outf4 - outf3;

                dewindow -= 16;
                u_ptr -= 32;

                PUT_SAMPLE(out)
              }
            } else {
              {
                float outf2, outf4;

                outf2  = u_ptr[ 2] * dewindow[ 0x2];
                outf4  = u_ptr[ 4] * dewindow[ 0x4];
                outf2 += u_ptr[ 6] * dewindow[ 0x6];
                outf4 += u_ptr[ 8] * dewindow[ 0x8];
                outf2 += u_ptr[10] * dewindow[ 0xa];
                outf4 += u_ptr[12] * dewindow[ 0xc];
                outf2 += u_ptr[14] * dewindow[ 0xe];
                outf4 += u_ptr[16] * dewindow[0x10];

                out = outf2 + outf4;

                PUT_SAMPLE(out)
              }

              dewindow -= 31;
              dewindow += start;
              dewindow += start;
              u_ptr -= 16;

              for (; j < 31; ++j) {
                float outf1, outf2, outf3, outf4;

                outf1  = *++u_ptr * *--dewindow;
                outf2  = *++u_ptr * *--dewindow;
                outf3  = *++u_ptr * *--dewindow;
                outf4  = *++u_ptr * *--dewindow;
                outf1 += *++u_ptr * *--dewindow;
                outf2 += *++u_ptr * *--dewindow;
                outf3 += *++u_ptr * *--dewindow;
                outf4 += *++u_ptr * *--dewindow;
                outf1 += *++u_ptr * *--dewindow;
                outf2 += *++u_ptr * *--dewindow;
                outf3 += *++u_ptr * *--dewindow;
                outf4 += *++u_ptr * *--dewindow;
                outf1 += *++u_ptr * *--dewindow;
                outf2 += *++u_ptr * *--dewindow;
                outf3 += *++u_ptr * *--dewindow;
                outf4 += *++u_ptr * *--dewindow;

                out = outf1 - outf2 + outf3 - outf4;

                dewindow -= 16;
                u_ptr -= 32;

                PUT_SAMPLE(out)
              }
            }
          }
#else
          const float *dewindow = t_dewindow[0] + 16 - start;

          /* These optimisations are tuned specifically for architectures
             without autoincrement and -decrement. */

          {
            float (*u_ptr)[16] = poly_u[ch][div];

            for (j = 0; j < 16; ++j) {
              float outf1, outf2, outf3, outf4;

              outf1  = u_ptr[0][ 0] * dewindow[0x0];
              outf2  = u_ptr[0][ 1] * dewindow[0x1];
              outf3  = u_ptr[0][ 2] * dewindow[0x2];
              outf4  = u_ptr[0][ 3] * dewindow[0x3];
              outf1 += u_ptr[0][ 4] * dewindow[0x4];
              outf2 += u_ptr[0][ 5] * dewindow[0x5];
              outf3 += u_ptr[0][ 6] * dewindow[0x6];
              outf4 += u_ptr[0][ 7] * dewindow[0x7];
              outf1 += u_ptr[0][ 8] * dewindow[0x8];
              outf2 += u_ptr[0][ 9] * dewindow[0x9];
              outf3 += u_ptr[0][10] * dewindow[0xa];
              outf4 += u_ptr[0][11] * dewindow[0xb];
              outf1 += u_ptr[0][12] * dewindow[0xc];
              outf2 += u_ptr[0][13] * dewindow[0xd];
              outf3 += u_ptr[0][14] * dewindow[0xe];
              outf4 += u_ptr[0][15] * dewindow[0xf];

              out = outf1 + outf2 + outf3 + outf4;

              dewindow += 32;
              u_ptr++;

              PUT_SAMPLE(out)
            }

            if (div & 0x1) {
              {
                float outf2, outf4;

                outf2  = u_ptr[0][ 0] * dewindow[0x0];
                outf4  = u_ptr[0][ 2] * dewindow[0x2];
                outf2 += u_ptr[0][ 4] * dewindow[0x4];
                outf4 += u_ptr[0][ 6] * dewindow[0x6];
                outf2 += u_ptr[0][ 8] * dewindow[0x8];
                outf4 += u_ptr[0][10] * dewindow[0xa];
                outf2 += u_ptr[0][12] * dewindow[0xc];
                outf4 += u_ptr[0][14] * dewindow[0xe];

                out = outf2 + outf4;

                PUT_SAMPLE(out)
              }

              dewindow -= 48;
              dewindow += start;
              dewindow += start;

              for (; j < 31; ++j) {
                float outf1, outf2, outf3, outf4;

                --u_ptr;

                outf1  = u_ptr[0][ 0] * dewindow[0xf];
                outf2  = u_ptr[0][ 1] * dewindow[0xe];
                outf3  = u_ptr[0][ 2] * dewindow[0xd];
                outf4  = u_ptr[0][ 3] * dewindow[0xc];
                outf1 += u_ptr[0][ 4] * dewindow[0xb];
                outf2 += u_ptr[0][ 5] * dewindow[0xa];
                outf3 += u_ptr[0][ 6] * dewindow[0x9];
                outf4 += u_ptr[0][ 7] * dewindow[0x8];
                outf1 += u_ptr[0][ 8] * dewindow[0x7];
                outf2 += u_ptr[0][ 9] * dewindow[0x6];
                outf3 += u_ptr[0][10] * dewindow[0x5];
                outf4 += u_ptr[0][11] * dewindow[0x4];
                outf1 += u_ptr[0][12] * dewindow[0x3];
                outf2 += u_ptr[0][13] * dewindow[0x2];
                outf3 += u_ptr[0][14] * dewindow[0x1];
                outf4 += u_ptr[0][15] * dewindow[0x0];

                out = -outf1 + outf2 - outf3 + outf4;

                dewindow -= 32;

                PUT_SAMPLE(out)
              }
            } else {
              {
                float outf2, outf4;

                outf2  = u_ptr[0][ 1] * dewindow[0x1];
                outf4  = u_ptr[0][ 3] * dewindow[0x3];
                outf2 += u_ptr[0][ 5] * dewindow[0x5];
                outf4 += u_ptr[0][ 7] * dewindow[0x7];
                outf2 += u_ptr[0][ 9] * dewindow[0x9];
                outf4 += u_ptr[0][11] * dewindow[0xb];
                outf2 += u_ptr[0][13] * dewindow[0xd];
                outf4 += u_ptr[0][15] * dewindow[0xf];

                out = outf2 + outf4;

                PUT_SAMPLE(out)
              }

              dewindow -= 48;
              dewindow += start;
              dewindow += start;

              for (; j < 31; ++j) {
                float outf1, outf2, outf3, outf4;

                --u_ptr;

                outf1  = u_ptr[0][ 0] * dewindow[0xf];
                outf2  = u_ptr[0][ 1] * dewindow[0xe];
                outf3  = u_ptr[0][ 2] * dewindow[0xd];
                outf4  = u_ptr[0][ 3] * dewindow[0xc];
                outf1 += u_ptr[0][ 4] * dewindow[0xb];
                outf2 += u_ptr[0][ 5] * dewindow[0xa];
                outf3 += u_ptr[0][ 6] * dewindow[0x9];
                outf4 += u_ptr[0][ 7] * dewindow[0x8];
                outf1 += u_ptr[0][ 8] * dewindow[0x7];
                outf2 += u_ptr[0][ 9] * dewindow[0x6];
                outf3 += u_ptr[0][10] * dewindow[0x5];
                outf4 += u_ptr[0][11] * dewindow[0x4];
                outf1 += u_ptr[0][12] * dewindow[0x3];
                outf2 += u_ptr[0][13] * dewindow[0x2];
                outf3 += u_ptr[0][14] * dewindow[0x1];
                outf4 += u_ptr[0][15] * dewindow[0x0];

                out = outf1 - outf2 + outf3 - outf4;

                dewindow -= 32;

                PUT_SAMPLE(out)
              }
            }
          }
#endif                                                                  
        }
#endif

        --u_start[ch];
        u_start[ch] &= 0xf;
        u_div[ch]=u_div[ch] ? 0 : 1;

#if defined(PENTIUM_RDTSC)
        __asm__(".byte 0x0f,0x31" : "=a" (cnt2), "=d" (cnt4));
                        
        if (cnt2-cnt1 < min_cycles) {
          min_cycles = cnt2-cnt1;
          printf("%d, %d cycles, %d\n", cnt3-cnt1, min_cycles, start);
        }
#endif
}

void CMpegAmp::premultiply()
{
  int i,t;
  static int done;

  /* only do this precalculation on static data once */
  if (done)
      return;
  done = TRUE;
  
  for (i = 0; i < 17; ++i)
    for (t = 0; t < 32; ++t)
      t_dewindow[i][t] *= 16383.5f;
}
