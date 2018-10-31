/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* 
 *   mpegamp.h - main AMP MPEG Audio player definitions
 *   
 *   Created by: tomislav uzelac Mar/Apr, Jul 96
 *   
 *   Ported to Win32, modified for C++, and adapted for use in HTML TADS
 *   by Michael J. Roberts, October 1998.  Changes copyright 1998 Michael
 *   J. Roberts.  
 *   
 *   Everything is now wrapped in a C++ class, to eliminate global
 *   variables so that this can be more readily used as a subsystem.  
 */

#ifndef MPEGAMP_H
#define MPEGAMP_H

#include <Windows.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/*
 *   General definitions
 */

/* 
 *   these should not be touched 
 */
#define         SYNCWORD        0xfff
#define         TRUE            1
#define         FALSE           0

/* 
 *   version 
 */
#define         MAJOR           0
#define         MINOR           7
#define         PATCH           6

#define MAX(a,b)        ((a) > (b) ? (a) : (b))
#define MAX3(a,b,c)     ((a) > (b) ? MAX(a, c) : MAX(b, c))
#define MIN(a,b)        ((a) < (b) ? (a) : (b))


/* ------------------------------------------------------------------------ */
/*
 *   Configuration settings 
 */

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF

/* Define as __inline if that's what the C compiler calls it.  */
#define inline __inline

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H


/* ------------------------------------------------------------------------ */
/* 
 *   gethdr() error codes 
 */
#define GETHDR_ERR 0x1
#define GETHDR_NS  0x2
#define GETHDR_FL1 0x4
#define GETHDR_FL2 0x8
#define GETHDR_FF  0x10
#define GETHDR_SYN 0x20
#define GETHDR_EOF 0x30


/* ------------------------------------------------------------------------ */
/*
 *   Audio header structure 
 */
struct AUDIO_HEADER
{
    int ID;
    int layer;
    int protection_bit;
    int bitrate_index;
    int sampling_frequency;
    int padding_bit;
    int private_bit;
    int mode;
    int mode_extension;
    int copyright;
    int original;
    int emphasis;
};

/*
 *   Sideband info structure 
 */
struct SIDE_INFO
{
    int main_data_begin;
    int scfsi[2][4];
    int part2_3_length[2][2];
    int big_values[2][2];
    int global_gain[2][2];
    int scalefac_compress[2][2];
    int window_switching_flag[2][2];
    int block_type[2][2];
    int mixed_block_flag[2][2];
    int table_select[2][2][3];
    int subblock_gain[2][2][3];
    int region0_count[2][2];
    int region1_count[2][2];
    int preflag[2][2];
    int scalefac_scale[2][2];
    int count1table_select[2][2];
};


/* ------------------------------------------------------------------------ */
/*
 *   AMP mpeg decoder class 
 */
class CMpegAmp
{
public:
    CMpegAmp()
    {
        min_cycles = 99999999;
        memset(poly_u, 0, sizeof(poly_u));
        memset(u_start, 0, sizeof(u_start));
        memset(u_div, 0, sizeof(u_div));
        memset(&layer3_info, 0, sizeof(layer3_info));
        memset(non_zero, 0, sizeof(non_zero));
        int min_cycles_misc2a = 99999999;
        int min_cycles_misc2b = 99999999;
        memset(no_of_imdcts, 0, sizeof(no_of_imdcts));
        stop_flag = FALSE;
    }
    
    void initialise_decoder(void);

protected:
    /* flag indicating that we are to stop playback as soon as possible */
    volatile int stop_flag;

    void warn(const char *msg, ...);
    void stop_playback();
    
    void statusDisplay(struct AUDIO_HEADER *header, int frameNo);
    void initialise_globals(void);
    void report_header_error(int err);
    void dump(int *length);

    /* handle of the input file */
    HANDLE in_file;

    /* number of bytes remaining in the file */
    unsigned long file_bytes_avail;

    int scalefac_l[2][2][22];
    int scalefac_s[2][2][13][3];

    /* statics */
    static const int t_b8_l[2][3][22];
    static const int t_b8_s[2][3][13];
    static const short t_bitrate[2][3][15];
    static const int t_sampling_frequency[2][3];

    int is[2][578];
    float xr[2][32][18];

    const int *t_l,*t_s;
    int nch;

    int A_QUIET,A_SHOW_CNT,A_FORMAT_WAVE,A_DUMP_BINARY;
    int A_WRITE_TO_AUDIO,A_WRITE_TO_FILE;
    short pcm_sample[64];
    int A_AUDIO_PLAY;
    int A_SET_VOLUME,A_SHOW_TIME;
    int A_MSG_STDOUT;
    int A_DOWNMIX;

    /* 
     *   buffer for the 'bit reservoir' 
     */
#define BUFFER_SIZE     4096
#define BUFFER_AUX      2048
    unsigned char buffer[BUFFER_SIZE + BUFFER_AUX];
    int append, data, f_bdirty, bclean_bytes;

    /*
     *   header parsing functions 
     */
    int gethdr(struct AUDIO_HEADER *header);
    void getcrc();
    void getinfo(struct AUDIO_HEADER *header,struct SIDE_INFO *info);  

    /*
     *   buffer and bit manipulation functions 
     */
    int fillbfr(unsigned int advance);
    unsigned int getbits(int n);
    int dummy_getinfo(int n);
    int rewind_stream(int nbytes);

    inline int _fillbfr(unsigned int size);
    inline int readsync();
    inline int get_input(unsigned char *bp, unsigned int size);
    inline unsigned int _getbits(int n);

    /* 
     *   header and side info parsing stuff 
     */
    inline void parse_header(struct AUDIO_HEADER *header);
    inline int header_sanity_check(struct AUDIO_HEADER *header);
    
    /* 
     *   internal buffer, _bptr holds the position in _bits_ 
     */
    unsigned char _buffer[32];
    int _bptr;

    /* from layer2.cpp */
    int layer2_frame(struct AUDIO_HEADER *header, int cnt);
    void convert_samplecode(unsigned int samplecode,
                            unsigned int nlevels,
                            unsigned short* sb_sample_buf);
    float requantize_sample(unsigned short s4,unsigned short nlevels,
                            float c,float d,float factor);

    /* from layer3.cpp */
    int layer3_frame(struct AUDIO_HEADER *header, int cnt);

    SIDE_INFO layer3_info;

    /* From: buffer.c */
    void printout(void);

    /* from getdata.cpp */
    int decode_scalefactors(struct SIDE_INFO *info,
                            struct AUDIO_HEADER *header,int gr,int ch);

    /* from huffman.cpp */
    inline unsigned int viewbits(int n);
    inline void sackbits(int n);
    inline int huffman_decode(int tbl,int *x,int *y);
    inline int _qsign(int x,int *q);
    int decode_huffman_data(struct SIDE_INFO *info,int gr,int ch, int ssize);
    int non_zero[2];
    static const int t_linbits[32];

    /* From: audio.c */
    void displayUsage();

    /* from misc2.cpp */
    inline float fras_l(int sfb,int global_gain,int scalefac_scale,
                        int scalefac,int preflag);
    inline float fras_s(int global_gain,int subblock_gain,
                        int scalefac_scale,int scalefac);
    inline float fras2(int is,float a);
    void requantize_mono(int gr,int ch,struct SIDE_INFO *info,
                         struct AUDIO_HEADER *header);
    int find_isbound(int isbound[3],int gr,struct SIDE_INFO *info,
                     struct AUDIO_HEADER *header);
    inline void stereo_s(int l,float a[2],int pos,int ms_flag,
                         int is_pos,struct AUDIO_HEADER *header);
    inline void stereo_l(int l,float a[2],int ms_flag,int is_pos,
                         struct AUDIO_HEADER *header);
    void requantize_ms(int gr,struct SIDE_INFO *info,
                       struct AUDIO_HEADER *header);
    void alias_reduction(int ch);
    
    void requantize_downmix(int gr,struct SIDE_INFO *info,
                            struct AUDIO_HEADER *header);
    void calculate_t43(void);

    int no_of_imdcts[2];
    int min_cycles_misc2a;
    int min_cycles_misc2b;
    float t_43[8192];

    /* from transform.cpp */
    void imdct_init();
    void imdct(int win_type,int sb,int ch);
    void poly(int ch,int i);
    void premultiply();

    int min_cycles;
    float poly_u[2][2][17][16];
    int u_start[2];
    int u_div[2];
    
    short sample_buffer[18][32][2];
    float res[32][18];
    float s[2][32][18];
    float win[4][36];

    /* table statics - see transform.cpp */
    static const float t_sin[4][36];
    static const float t_2cos[4][18];

    static const float b1; static const float b2; static const float b3;
    static const float b4; static const float b5; static const float b6;
    static const float b7; static const float b8; static const float b9;
    static const float b10; static const float b11; static const float b12;
    static const float b13; static const float b14; static const float b15;
    static const float b16; static const float b17; static const float b18;
    static const float b19; static const float b20; static const float b21;
    static const float b22; static const float b23; static const float b24;
    static const float b25; static const float b26; static const float b27;
    static const float b28; static const float b29; static const float b30;
    static const float b31;

    static float t_dewindow[17][32];

    /* from getdata */
    static const char t_slen1[16];
    static const char t_slen2[16];

    /*
     *   the maximum value of is_pos. for short blocks is_max[sfb=0] ==
     *   is_max[6], it's sloppy but i'm sick of wasting storage. blaah...  
     */
    int is_max[21]; 
    int intensity_scale;

    /* 
     *   my implementation of MPEG2 scalefactor decoding is, admitably,
     *   horrible anyway, just take a look at pg.18 of MPEG2 specs, and
     *   you'll know what this is all about 
     */
    static const char spooky_table[2][3][3][4];
};

#endif /* MPEGAMP_H */

