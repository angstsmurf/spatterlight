/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* 
 *   layer2.c MPEG audio layer2 support
 *   
 *   Created by: Tomislav Uzelac Mar 1996
 *.  merged with amp, May 19 1997
 *   
 *   Ported to Win32, modified for C++, and adapted for use in HTML TADS
 *   by Michael J. Roberts, October 1998.  Changes copyright 1998 Michael
 *   J. Roberts.  
 */

/* for MS VC++, turn off warnings about initializing floats from doubles */
#ifdef _MSC_VER
#pragma warning(disable: 4305; disable: 4244)
#endif

#include "mpegamp.h"


/* ------------------------------------------------------------------------ */
/*
 *   Static tables
 */

static const char t_nbal0[27]=
    {4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2};
static const char t_nbal1[30]=
    {4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2};
static const char t_nbal2[8] =
    {4,4,3,3,3,3,3,3};
static const char t_nbal3[12]=
    {4,4,3,3,3,3,3,3,3,3,3,3};
static const char t_nbalMPG2[30]=
    {4,4,4,4,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};

static const char t_alloc0[27][16] =
{                     /* table B.2a ISO/IEC 11172-3 */
    {0,1,3,5,6,7,8,9,10,11,12,13,14,15,16,17},
    {0,1,3,5,6,7,8,9,10,11,12,13,14,15,16,17},
    {0,1,3,5,6,7,8,9,10,11,12,13,14,15,16,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,17},
    {0,1,2,17},
    {0,1,2,17},
    {0,1,2,17}
};

static const char t_alloc1[30][16] =
{                     /* table B.2b ISO/IEC 11172-3 */
    {0,1,3,5,6,7,8,9,10,11,12,13,14,15,16,17},
    {0,1,3,5,6,7,8,9,10,11,12,13,14,15,16,17},
    {0,1,3,5,6,7,8,9,10,11,12,13,14,15,16,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,3,4,5,6,17},
    {0,1,2,17},
    {0,1,2,17},
    {0,1,2,17},
    {0,1,2,17},
    {0,1,2,17},
    {0,1,2,17},
    {0,1,2,17}
};

static const char t_alloc2[8][16] =
{              /* table B.2c ISO/IEC 11172-3 */
    {0,1,2,4,5,6,7,8,9,10,11,12,13,14,15,16},
    {0,1,2,4,5,6,7,8,9,10,11,12,13,14,15,16},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127}
};
    
static const char t_alloc3[12][16] =
{                 /* table B.2d ISO/IEC 11172-3 */
    {0,1,2,4,5,6,7,8,9,10,11,12,13,14,15,16},
    {0,1,2,4,5,6,7,8,9,10,11,12,13,14,15,16},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127},
    {0,1,2,4,5,6,7,127}
};
    
static const char t_allocMPG2[30][16] =
{              /* table B.1. ISO/IEC 13818-3 */
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {0,1,2,4,5,6,7,8},
    {0,1,2,4,5,6,7,8},
    {0,1,2,4,5,6,7,8},
    {0,1,2,4,5,6,7,8},
    {0,1,2,4,5,6,7,8},
    {0,1,2,4,5,6,7,8},
    {0,1,2,4,5,6,7,8},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4},
    {0,1,2,4}
};

static const double t_scalefactor[64] =
{
    2.00000000000000, 1.58740105196820, 1.25992104989487,
    1.00000000000000, 0.79370052598410, 0.62996052494744, 0.50000000000000,
    0.39685026299205, 0.31498026247372, 0.25000000000000, 0.19842513149602,
    0.15749013123686, 0.12500000000000, 0.09921256574801, 0.07874506561843,
    0.06250000000000, 0.04960628287401, 0.03937253280921, 0.03125000000000,
    0.02480314143700, 0.01968626640461, 0.01562500000000, 0.01240157071850,
    0.00984313320230, 0.00781250000000, 0.00620078535925, 0.00492156660115,
    0.00390625000000, 0.00310039267963, 0.00246078330058, 0.00195312500000,
    0.00155019633981, 0.00123039165029, 0.00097656250000, 0.00077509816991,
    0.00061519582514, 0.00048828125000, 0.00038754908495, 0.00030759791257,
    0.00024414062500, 0.00019377454248, 0.00015379895629, 0.00012207031250,
    0.00009688727124, 0.00007689947814, 0.00006103515625, 0.00004844363562,
    0.00003844973907, 0.00003051757813, 0.00002422181781, 0.00001922486954,
    0.00001525878906, 0.00001211090890, 0.00000961243477, 0.00000762939453,
    0.00000605545445, 0.00000480621738, 0.00000381469727, 0.00000302772723,
    0.00000240310869, 0.00000190734863, 0.00000151386361, 0.00000120155435,
    1E-20
};

static const double t_c[18] =
{
    0,
    1.33333333333, 1.60000000000, 1.14285714286,
    1.77777777777, 1.06666666666, 1.03225806452,
    1.01587301587, 1.00787401575, 1.00392156863,
    1.00195694716, 1.00097751711, 1.00048851979,
    1.00024420024, 1.00012208522, 1.00006103888,
    1.00003051851, 1.00001525902
};

static const double t_d[18] =
{
    0,
    0.500000000,  0.500000000,  0.250000000,  0.500000000,
    0.125000000,  0.062500000,  0.031250000,  0.015625000,
    0.007812500,  0.003906250,  0.001953125,  0.0009765625,
    0.00048828125,0.00024414063,0.00012207031,
    0.00006103516,0.00003051758
};

static const float t_dd[18]=
{
    -1, -0.5, -0.5, -0.75, -0.5, -0.875, -0.9375, -0.96875, -0.984375,
    -0.992188, -0.996094, -0.998047, -0.999023, -0.999512, -0.999756, -0.999878, -0.999939,
    -0.999969
};

static const char t_grouping[18]={0,3,5,0,9,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*
 *   int t_nlevels[18] =
 *   {0,3,5,7,9,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767,65535};
 */
static const int t_nlevels[18] =
    {0,3,7,7,15,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767,65535};


static const float t_nli[18]=
{
    0, 0.5, 0.25, 0.25, 0.125, 0.125, 0.0625, 0.03125, 0.015625, 0.0078125, 0.00390625,
    0.00195313, 0.000976563, 0.000488281, 0.000244141, 0.00012207, 6.10352e-05, 3.05176e-05
}; 

static const int t_bpc[18] = {0,5,7,3,10,4,5,6,7,8,9,10,11,12,13,14,15,16};


/* ------------------------------------------------------------------------ */
/*
 *   Implementation 
 */

int CMpegAmp::layer2_frame(struct AUDIO_HEADER *header,int cnt)
{
int i,s,sb,ch,gr,bitrate,bound;
char (*nbal)[],(*bit_alloc_index)[][16];
unsigned char allocation[2][32];
unsigned char scfsi[2][32];
float scalefactor[2][32][3];
float subband_sample[2][32][36];
int sblimit,nlevels,grouping; 

float  c,d;
int no_of_bits,mpi;                             
unsigned short sb_sample_buf[3];        

int hsize,fs,mean_frame_size;

        hsize=4;
        if (header->protection_bit==0) hsize+=2;

        bitrate=t_bitrate[header->ID][3-header->layer][header->bitrate_index];
        fs=t_sampling_frequency[header->ID][header->sampling_frequency];
        if (header->ID) mean_frame_size=144000*bitrate/fs;
        else mean_frame_size=72000*bitrate/fs;

        /* layers 1 and 2 do not have a 'bit reservoir'
         */
        append=data=0;

        fillbfr(mean_frame_size + header->padding_bit - hsize);

        switch (header->mode)
                {
                case 0 : 
                case 2 : nch=2; bound=32; bitrate=bitrate/2;  
                        break;
                case 3 : nch=1; bound=32; 
                        break;
                case 1 : nch=2; bitrate=bitrate/2; bound=(header->mode_extension+1)*4; 
                }
                
        if (header->ID==1) switch (header->sampling_frequency) {
                case 0 : switch (bitrate)       /* 0 = 44.1 kHz */
                                {
                                case 56  :
                                case 64  :
                                case 80  : bit_alloc_index=(char(*)[][16])&t_alloc0;
                                           nbal=(char (*)[])&t_nbal0;
                                           sblimit=27;
                                           break;
                                case 96  :
                                case 112 :
                                case 128 :
                                case 160 :
                                case 192 : bit_alloc_index=(char(*)[][16])&t_alloc1;
                                           nbal=(char (*)[])&t_nbal1;
                                           sblimit=30;  
                                           break;
                                case 32  :
                                case 48  : bit_alloc_index=(char(*)[][16])&t_alloc2;
                                           nbal=(char (*)[])&t_nbal2;
                                           sblimit=8;
                                           break;
                                default  : warn(" bit alloc info no gud ");
                                }
                                break;
                case 1 : switch (bitrate)       /* 1 = 48 kHz */
                                {
                                case 56  : 
                                case 64  :
                                case 80  :
                                case 96  :
                                case 112 :
                                case 128 :
                                case 160 :
                                case 192 : bit_alloc_index=(char(*)[][16])&t_alloc0;
                                           nbal=(char (*)[])&t_nbal0;
                                           sblimit=27;
                                           break;
                                case 32  :
                                case 48  : bit_alloc_index=(char(*)[][16])&t_alloc2;
                                           nbal=(char (*)[])&t_nbal2;
                                           sblimit=8;
                                           break;
                                default  : warn(" bit alloc info no gud ");
                                }
                                break;
                case 2 : switch (bitrate)       /* 2 = 32 kHz */
                                {
                        case 56  :
                        case 64  :
                        case 80  : bit_alloc_index=(char(*)[][16])&t_alloc0;
                                   nbal=(char (*)[])&t_nbal0;
                                   sblimit=27;
                                   break;
                        case 96  :
                        case 112 :
                        case 128 :
                        case 160 :
                        case 192 : bit_alloc_index=(char(*)[][16])&t_alloc1;
                                   nbal=(char (*)[])&t_nbal1;
                                   sblimit=30;
                                   break;
                        case 32  :
                        case 48  : bit_alloc_index=(char(*)[][16])&t_alloc3;
                                   nbal=(char (*)[])&t_nbal3;
                                   sblimit=12;
                                   break;
                        default  : warn("bit alloc info not ok\n");
                        }
                        break;                                                    
                default  : warn("sampling freq. not ok/n");
        } else {
                bit_alloc_index=(char (*)[][16])&t_allocMPG2;
                nbal=(char (*)[])&t_nbalMPG2;
                sblimit=30;
        }

/*
 * bit allocation per subband per channel decoding *****************************
 */

        if (bound==32) bound=sblimit;   /* bound=32 means there is no intensity stereo */
         
        for (sb=0;sb<bound;sb++)
                for (ch=0;ch<nch;ch++)
                        allocation[ch][sb]=getbits((*nbal)[sb]);

        for (sb=bound;sb<sblimit;sb++)
                allocation[1][sb] = allocation[0][sb] = getbits((*nbal)[sb]);


/*
 * scfsi ***********************************************************************
 */

        for (sb=0;sb<sblimit;sb++)
                for (ch=0;ch<nch;ch++)
                        if (allocation[ch][sb]!=0) scfsi[ch][sb]=getbits(2);
                        else scfsi[ch][sb]=0;

/*
 * scalefactors ****************************************************************
 */

        for (sb=0;sb<sblimit;sb++)
        for (ch=0;ch<nch;ch++)
                if (allocation[ch][sb]!=0) {
                        scalefactor[ch][sb][0]=t_scalefactor[getbits(6)];
                        switch (scfsi[ch][sb])
                        {
                        case 0: scalefactor[ch][sb][1]=t_scalefactor[getbits(6)];
                                scalefactor[ch][sb][2]=t_scalefactor[getbits(6)];
                                break;
                        case 1: scalefactor[ch][sb][2]=t_scalefactor[getbits(6)];
                                scalefactor[ch][sb][1]=scalefactor[ch][sb][0];
                                break;
                        case 2: scalefactor[ch][sb][1]=scalefactor[ch][sb][0];
                                scalefactor[ch][sb][2]=scalefactor[ch][sb][0];
                                break;
                        case 3: scalefactor[ch][sb][2]=t_scalefactor[getbits(6)];
                                scalefactor[ch][sb][1]=scalefactor[ch][sb][2];
                        }                       
                } 
                else scalefactor[ch][sb][0]=scalefactor[ch][sb][1]=\
                        scalefactor[ch][sb][2]=0.0;


/*
 * samples *********************************************************************
 */

        for (gr=0;gr<12;gr++) {
                /*
                 * normal ********************************
                 */
        
                for (sb=0;sb<bound;sb++)
                for (ch=0;ch<nch;ch++)
                if (allocation[ch][sb]!=0) {
                        mpi=(*bit_alloc_index)[sb][allocation[ch][sb]]; 
                        no_of_bits=t_bpc[mpi];
                        c=t_c[mpi];
                        d=t_d[mpi];
                        grouping=t_grouping[mpi];
                        nlevels=t_nlevels[mpi];

                        if (grouping) {
                                int samplecode=getbits(no_of_bits);
                                convert_samplecode(samplecode,grouping,sb_sample_buf);

                                for (s=0;s<3;s++)
                                        subband_sample[ch][sb][3*gr+s]=requantize_sample (sb_sample_buf[s],nlevels,c,d,scalefactor[ch][sb][gr/4]);
                        } else {
                                for (s=0;s<3;s++) sb_sample_buf[s]=getbits(no_of_bits);
                                
                                for (s=0;s<3;s++) { 
                                        /*subband_sample[ch][sb][3*gr+s]=requantize_sample (sb_sample_buf[s],nlevels,c,d,scalefactor[ch][sb][gr/4]);*/
                                        subband_sample[ch][sb][3*gr+s]=(t_dd[mpi]+sb_sample_buf[s]*t_nli[mpi])*c*scalefactor[ch][sb][gr>>2];
                                }
                        }
                } else 
                        for (s=0;s<3;s++) subband_sample[ch][sb][3*gr+s]=0;


                /*
                 * joint stereo ********************************************
                 */
      
                for (sb=bound;sb<sblimit;sb++)
                if (allocation[0][sb]!=0) {
                        /*ispravka!
                        */
                        mpi=(*bit_alloc_index)[sb][allocation[0][sb]];  
                        no_of_bits=t_bpc[mpi];
                        c=t_c[mpi];
                        d=t_d[mpi];
                        grouping=t_grouping[mpi];
                        nlevels=t_nlevels[mpi]; 
                   
                        if (grouping) {
                                int samplecode=getbits(no_of_bits);
                                convert_samplecode(samplecode,grouping,sb_sample_buf);

                                for (s=0;s<3;s++) {
                                        subband_sample[0][sb][3*gr+s]=requantize_sample (sb_sample_buf[s],nlevels,c,d,scalefactor[0][sb][gr/4]);
                                        subband_sample[1][sb][3*gr+s]=subband_sample[0][sb][3*gr+s];
                                }
                        } else {
                                for (s=0;s<3;s++) sb_sample_buf[s]=getbits(no_of_bits);

                                for (s=0;s<3;s++) { 
                                        subband_sample[0][sb][3*gr+s]=subband_sample[1][sb][3*gr+s]=\
                                                (t_dd[mpi]+sb_sample_buf[s]*t_nli[mpi])*c*scalefactor[0][sb][gr>>2];
                                }
                        }

                } else for (s=0;s<3;s++) {
                        subband_sample[0][sb][3*gr+s]=0;
                        subband_sample[1][sb][3*gr+s]=0;
                }

                /*
                 * the rest *******************************************
                 */
                for (sb=sblimit;sb<32;sb++)
                        for (ch=0;ch<nch;ch++)

                                for (s=0;s<3;s++) subband_sample[ch][sb][3*gr+s]=0;
        }       

        /*
         * this is, in fact, horrible, but I had to adjust it to amp/mp3. The hack to make downmixing
         * work is as ugly as possible.
         */

        if (A_DOWNMIX && header->mode!=3) {
                for (ch=0;ch<nch;ch++) 
                        for (sb=0;sb<32;sb++)
                                for (i=0;i<36;i++)
                                        subband_sample[0][sb][i]=(subband_sample[0][sb][i]+subband_sample[1][sb][i])*0.5f;
                nch=1;
        }

        for (ch=0;ch<nch;ch++) {
                for (sb=0;sb<32;sb++) 
                        for (i=0;i<18;i++) res[sb][i]=subband_sample[ch][sb][i]; 
                for (i=0;i<18;i++)
                        poly(ch,i);
        }
        printout();
        for (ch=0;ch<nch;ch++) {
                for (sb=0;sb<32;sb++)
                        for (i=0;i<18;i++) res[sb][i]=subband_sample[ch][sb][i+18];
                for (i=0;i<18;i++)
                        poly(ch,i);
        }
        printout();

        if (A_DOWNMIX && header->mode!=3) nch=2;

        return 0;
}                        
/****************************************************************************/  
/****************************************************************************/

void CMpegAmp::convert_samplecode(unsigned int samplecode,
                                  unsigned int nlevels,
                                  unsigned short* sb_sample_buf)
{
int i;

        for (i=0;i<3;i++) {
                *sb_sample_buf=samplecode%nlevels;
                samplecode=samplecode/nlevels;
                sb_sample_buf++;
        }
}

float CMpegAmp::requantize_sample(unsigned short s4,unsigned short nlevels,
                                  float c,float d,float factor)
{
register float s,s2,s3;
s3=-1.0+s4*2.0/(nlevels+1);
s2=c*(s3+d);
s=factor*s2;
return s;
} 
