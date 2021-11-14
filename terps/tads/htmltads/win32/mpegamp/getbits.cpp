/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* 
 *   getbits.c bit level routines, input buffer
 *   
 *   Created by: tomislav uzelac Apr 1996 
 *.  better synchronization, tomislav uzelac, Apr 23 1997
 *   
 *   Ported to Win32, modified for C++, and adapted for use in HTML TADS
 *   by Michael J. Roberts, October 1998.  Changes copyright 1998
 *   Michael J. Roberts.  
 */
#include <string.h>
#include <stdio.h>

#include "mpegamp.h"

/* 
 * buffer and bit manipulation functions ***************************************
 */
int CMpegAmp::_fillbfr(unsigned int size)
{
        _bptr=0;
        return get_input(_buffer, size);
}

int CMpegAmp::readsync()
{
        _bptr=0;
        _buffer[0]=_buffer[1];
        _buffer[1]=_buffer[2];
        _buffer[2]=_buffer[3];
        return get_input(&_buffer[3],1);
}

unsigned int CMpegAmp::_getbits(int n)
{
unsigned int pos,ret_value;

        pos = _bptr >> 3;
        ret_value = _buffer[pos] << 24 |
                    _buffer[pos+1] << 16 |
                    _buffer[pos+2] << 8 |
                    _buffer[pos+3];
        ret_value <<= _bptr & 7;
        ret_value >>= 32 - n;
        _bptr += n;
        return ret_value;
}       

int CMpegAmp::fillbfr(unsigned int advance)
{
int overflow,retval;

        retval=get_input(&buffer[append], advance);
        
        if ( append + advance >= BUFFER_SIZE ) {
                overflow = append + advance - BUFFER_SIZE;
                memcpy (buffer,&buffer[BUFFER_SIZE], overflow);
                if (overflow < 4) memcpy(&buffer[BUFFER_SIZE],buffer,4);
                append = overflow;
        } else {
                if (append==0) memcpy(&buffer[BUFFER_SIZE],buffer,4);
                append+=advance;
        }
        return retval;
}

unsigned int CMpegAmp::getbits(int n)
{
        if (n) {
        unsigned int pos,ret_value;

                pos = data >> 3;
                ret_value = buffer[pos] << 24 |
                        buffer[pos+1] << 16 |
                        buffer[pos+2] << 8 |
                        buffer[pos+3];
                ret_value <<= data & 7;
                ret_value >>= 32 - n;

                data += n;
                data &= (8*BUFFER_SIZE)-1;

                return ret_value;
        } else
                return 0;
}

/*
 * header and side info parsing stuff ******************************************
 */
void CMpegAmp::parse_header(struct AUDIO_HEADER *header) 
{
        header->ID=_getbits(1);
        header->layer=_getbits(2);
        header->protection_bit=_getbits(1);
        header->bitrate_index=_getbits(4);
        header->sampling_frequency=_getbits(2);
        header->padding_bit=_getbits(1);
        header->private_bit=_getbits(1);
        header->mode=_getbits(2);
        header->mode_extension=_getbits(2);
        if (!header->mode) header->mode_extension=0;
        header->copyright=_getbits(1);
        header->original=_getbits(1);
        header->emphasis=_getbits(2);
}

int CMpegAmp::header_sanity_check(struct AUDIO_HEADER *header)
{
        if (    header->layer==0 ||
                header->bitrate_index==15 ||
                header->sampling_frequency==3) return -1;

        /* an additional check to make shure that stuffing never gets mistaken
         * for a syncword. This rules out some legal layer1 streams, but who
         * cares about layer1 anyway :-). I must get this right sometime.
         */
        if ( header->ID==1 && header->layer==3 && header->protection_bit==1) return -1;
        return 0;
}


int CMpegAmp::gethdr(struct AUDIO_HEADER *header)
{
int s,retval;
struct AUDIO_HEADER tmp;

        /* TODO: add a simple byte counter to check only first, say, 1024
         * bytes for a new header and then return GETHDR_SYN
         */
        if ((retval=_fillbfr(4))!=0) return retval;

        for(;;) {
                while ((s=_getbits(12)) != 0xfff) {
                        if (s==0xffe) {
                                parse_header(&tmp);
                                if (header_sanity_check(&tmp)==0) return GETHDR_NS;
                        }
                        if ((retval=readsync())!=0) return retval;
                }
        
                parse_header(&tmp);
                if (header_sanity_check(&tmp)!=0) {
                        if ((retval=readsync())!=0) return retval;
                } else break;
        }

        if (tmp.layer==3) return GETHDR_FL1;
        /* if (tmp.layer==2) return GETHDR_FL2; */
        if (tmp.bitrate_index==0) return GETHDR_FF;

        memcpy(header,&tmp,sizeof(tmp));
        return 0;
}

/* dummy function, to get crc out of the way
*/
void CMpegAmp::getcrc()
{
        _fillbfr(2);
        _getbits(16);
}

/* sizes of side_info:
 * MPEG1   1ch 17    2ch 32
 * MPEG2   1ch  9    2ch 17
 */
void CMpegAmp::getinfo(struct AUDIO_HEADER *header,struct SIDE_INFO *info)
{
int gr,ch,scfsi_band,region,window;
int nch;        
        if (header->mode==3) {
                nch=1;
                if (header->ID) {
                        _fillbfr(17);
                        info->main_data_begin=_getbits(9);
                        _getbits(5);
                } else {
                        _fillbfr(9);
                        info->main_data_begin=_getbits(8);
                        _getbits(1);
                }
        } else {
                nch=2;
                if (header->ID) {
                        _fillbfr(32);
                        info->main_data_begin=_getbits(9);
                        _getbits(3);
                } else {
                        _fillbfr(17);
                        info->main_data_begin=_getbits(8);
                        _getbits(2);
                }
        }

        if (header->ID) for (ch=0;ch<nch;ch++)
                for (scfsi_band=0;scfsi_band<4;scfsi_band++)
                        info->scfsi[ch][scfsi_band]=_getbits(1);

        for (gr=0;gr<(header->ID ? 2:1);gr++)
                for (ch=0;ch<nch;ch++) {
                        info->part2_3_length[gr][ch]=_getbits(12);
                        info->big_values[gr][ch]=_getbits(9);
                        info->global_gain[gr][ch]=_getbits(8);
                        if (header->ID) info->scalefac_compress[gr][ch]=_getbits(4);
                        else info->scalefac_compress[gr][ch]=_getbits(9);
                        info->window_switching_flag[gr][ch]=_getbits(1);

                        if (info->window_switching_flag[gr][ch]) {
                                info->block_type[gr][ch]=_getbits(2);
                                info->mixed_block_flag[gr][ch]=_getbits(1);

                                for (region=0;region<2;region++)
                                        info->table_select[gr][ch][region]=_getbits(5);
                                info->table_select[gr][ch][2]=0;

                                for (window=0;window<3;window++)
                                        info->subblock_gain[gr][ch][window]=_getbits(3);
                        } else {
                                for (region=0;region<3;region++)
                                        info->table_select[gr][ch][region]=_getbits(5);

                                info->region0_count[gr][ch]=_getbits(4);
                                info->region1_count[gr][ch]=_getbits(3);
                                info->block_type[gr][ch]=0;
                        }

                        if (header->ID) info->preflag[gr][ch]=_getbits(1);
                        info->scalefac_scale[gr][ch]=_getbits(1);
                        info->count1table_select[gr][ch]=_getbits(1);
                }
        return;
}

int CMpegAmp::dummy_getinfo(int n)
{
        n-=4;

        /* make sure we have enough data available */
        if (file_bytes_avail < n)
                return GETHDR_EOF;

        /* deduct the amount we're skipping from the available size */
        file_bytes_avail -= n;

        /* skip the bytes */
        if (SetFilePointer(in_file, n, 0, FILE_CURRENT) == 0xffffffff)
                return GETHDR_ERR;

        /* success */
        return 0;
}

int CMpegAmp::get_input(unsigned char *bp, unsigned int size)
{
        DWORD actual;

        /* 
         *   if the stop flag is set, return EOF to indicate that we can't
         *   read any more data 
         */
        if (stop_flag)
                return GETHDR_EOF;

        /* if we don't have enough bytes available, return EOF */
        if (file_bytes_avail < size)
                return GETHDR_EOF;

        /* read the bytes */
        if (ReadFile(in_file, bp, size, &actual, 0) == 0)
                return GETHDR_ERR;
        else if (actual != size)
                return GETHDR_EOF;

        /* deduct the amount read from the available bytes */
        file_bytes_avail -= actual;

        /* success */
        return 0;
}

