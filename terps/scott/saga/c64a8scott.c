//
//  c64a8scott.c
//  Spatterlight
//
//  The ScottFree-specific code required by
//  the common code in common_sagadraw/c64a8draw.c
//  to draw the bitmap graphics of Atari 8-bit and
//  Commodore 64 games.
//
//  Created by Administrator on 2025-12-15.
//

#include "scott.h"
#include "c64a8draw.h"
#include "c64a8scott.h"

// clang-format off

/* C64 colors */
static const RGB  white =  0xffffff;
static const RGB  red =    0xbf6148;
static const RGB  purple = 0xb159b9;
static const RGB  green =  0x79d570;
static const RGB  blue =   0x5f48e9;
static const RGB  yellow = 0xf7ff6c;
static const RGB  orange = 0xba8620;
static const RGB  lred =   0xe79a84;
static const RGB  grey =   0xa7a7a7;
static const RGB  lgreen = 0xc0ffb9;
static const RGB  lblue =  0xa28fff;
static const RGB  brown =  0x837000;

/* Atari 8-bit colors */
static const RGB colors[256] = {
    0x000000 , // 0
    0x0e0e0e , // 1
    0x1d1d1d , // 2
    0x2c2c2c , // 3
    0x3b3b3b , // 4
    0x4a4a4a , // 5
    0x595959 , // 6
    0x686868 , // 7
    0x777777 , // 8
    0x868686 , // 9
    0x959595 , // 10
    0xa4a4a4 , // 11
    0xb3b3b3 , // 12
    0xc2c2c2 , // 13
//  0xd1d1d1 , // 14
    0xe0e0e0 ,
    0xe0e0e0 , // 15
    0x340000 , // 16
    0x430e00 , // 17
//  0x521d00 , // 18
    0xad5f64 ,
    0x612c00 , // 19
    0x703b00 , // 20
    0x7e4a00 , // 21
    0x8d5900 , // 22
    0x9c6800 , // 23
    0xab7700 , // 24
    0xba8600 , // 25
    0xc9950b , // 26
    0xd8a41a , // 27
    0xe7b329 , // 28
    0xeec238 , // 29
    0xeed146 , // 30
    0xeee055 , // 31
    0x3a0000 , // 32
    0x490100 , // 33
    0x581000 , // 34
    0x671f00 , // 35
    0x762e00 , // 36
//  0x853d00 , // 37
    0xad5f64 ,
    0x944c02 , // 38
    0xa35b11 , // 39
    0xb26a20 , // 40
    0xc1792f , // 41
    0xd0883e , // 42
    0xdf974d , // 43
    0xeea65c , // 44
    0xeeb56b , // 45
    0xeec47a , // 46
    0xeed289 , // 47
    0x360000 , // 48
    0x450000 , // 49
//  0x54050b , // 50
    0xad5f64 ,
    0x62141a , // 51
    0x712329 , // 52
    0x803238 , // 53
//  0x8f4146 , // 54
    0xad5f64 ,
    0x9e5055 , // 55
    0xad5f64 , // 56
    0xbc6e73 , // 57
//  0xcb7d82 , // 58
    0xad5f64 ,
    0xda8c91 , // 59
    0xe99aa0 , // 60
    0xeea9af , // 61
    0xeeb8be , // 62
    0xeec7cd , // 63
    0x270034 , // 64
    0x360043 , // 65
    0x450052 , // 66
    0x540f61 , // 67
    0x621e70 , // 68
    0x712d7e , // 69
    0x803c8d , // 70
    0x8f4b9c , // 71
    0x9e5aab , // 72
    0xad69ba , // 73
    0xbc78c9 , // 74
    0xcb87d8 , // 75
    0xda96e7 , // 76
    0xe9a5ee , // 77
    0xeeb4ee , // 78
    0xeec3ee , // 79
    0x0f0071 , // 80
    0x1e0080 , // 81
    0x2d008f , // 82
    0x3c0f9e , // 83
    0x4b1ead , // 84
    0x5a2dbc , // 85
//  0x693ccb , // 86
    0x4b1ead ,
    0x784bda , // 87
    0x875ae9 , // 88
    0x9669ee , // 89
    0xa578ee , // 90
    0xb487ee , // 91
    0xc396ee , // 92
    0xd2a5ee , // 93
    0xe0b4ee , // 94
    0xeec3ee , // 95
    0x000099 , // 96
    0x0400a7 , // 97
    0x1307b6 , // 98
    0x2216c5 , // 99
    0x3125d4 , // 100
    0x4034e3 , // 101
    0x4f43ee , // 102
    0x5e52ee , // 103
    0x6d61ee , // 104
    0x7c70ee , // 105
    0x8b7eee , // 106
    0x9a8dee , // 107
    0xa89cee , // 108
    0xb7abee , // 109
    0xc6baee , // 110
    0xd5c9ee , // 111
    0x0000a1 , // 112
    0x0002b0 , // 113
    0x0011bf , // 114
    0x0a20ce , // 115
    0x192fdd , // 116
    0x283eec , // 117
    0x374dee , // 118
    0x465cee , // 119
    0x546bee , // 120
    0x637aee , // 121
    0x7289ee , // 122
    0x8198ee , // 123
    0x90a7ee , // 124
    0x9fb6ee , // 125
    0xaec4ee , // 126
    0xbdd3ee , // 127
    0x00008a , // 128
    0x000e99 , // 129
    0x001da8 , // 130
    0x002cb6 , // 131
    0x073bc5 , // 132
//  0x164ad4 , // 133
    0x3468ee ,
    0x2559e3 , // 134
    0x3468ee , // 135
    0x4377ee , // 136
    0x5286ed , // 137
    0x6195ee , // 138
    0x70a4ee , // 139
    0x7eb3ee , // 140
    0x8dc2ee , // 141
    0x9cd1ee , // 142
    0xabe0ee , // 143
    0x000d56 , // 144
    0x001c65 , // 145
    0x002a74 , // 146
    0x003983 , // 147
    0x004892 , // 148
    0x0f57a1 , // 149
    0x1e66b0 , // 150
    0x2d75bf , // 151
    0x3c84ce , // 152
    0x4b93dd , // 153
    0x5aa2ec , // 154
    0x69b1ee , // 155
    0x78c0ee , // 156
    0x87cfee , // 157
    0x96deee , // 158
    0xa5edee , // 159
    0x001812 , // 160
    0x002721 , // 161
    0x003630 , // 162
    0x00453f , // 163
    0x05544e , // 164
    0x14625d , // 165
    0x23716c , // 166
    0x32807b , // 167
    0x418f8a , // 168
    0x509e99 , // 169
    0x5fada8 , // 170
    0x6ebcb6 , // 171
    0x7dcbc5 , // 172
    0x8cdad4 , // 173
    0x9ae9e3 , // 174
    0xa9eeee , // 175
    0x001c00 , // 176
    0x002b00 , // 177
    0x003a00 , // 178
    0x054900 , // 179
    0x145807 , // 180
    0x236716 , // 181
    0x327625 , // 182
    0x418534 , // 183
    0x509443 , // 184
    0x5fa352 , // 185
    0x6eb261 , // 186
    0x7dc170 , // 187
    0x8cd07e , // 188
    0x9adf8d , // 189
    0xa9ee9c , // 190
    0xb8eeab , // 191
    0x001c00 , // 192
    0x002b00 , // 193
    0x0e3a00 , // 194
    0x1c4900 , // 195
    0x2b5800 , // 196
    0x3a6700 , // 197
//  0x497600 , // 198
    0x2b5800 ,
//  0x588500 , // 199
    0x3a6700 , // 197
    0x679405 , // 200
    0x76a314 , // 201
    0x85b223 , // 202
    0x94c132 , // 203
    0xa3d041 , // 204
    0xb2df50 , // 205
    0xc1ee5f , // 206
    0xd0ee6e , // 207
    0x0a1600 , // 208
    0x192500 , // 209
    0x283400 , // 210
    0x374300 , // 211
    0x465200 , // 212
    0x546100 , // 213
    0x637000 , // 214
    0x727e00 , // 215
//  0x818d00 , // 216
    0x637000 ,
    0x909c00 , // 217
    0x9fab00 , // 218
    0xaeba0b , // 219
    0xbdc91a , // 220
    0xccd829 , // 221
    0xdbe738 , // 222
    0xeaee46 , // 223
    0x220c00 , // 224
    0x311b00 , // 225
    0x402a00 , // 226
    0x4f3800 , // 227
//  0x5e4700 , // 228
    0x944c02 ,
    0x6d5600 , // 229
    0x7c6500 , // 230
    0x8b7400 , // 231
    0x9a8300 , // 232
    0xa89200 , // 233
    0xb7a100 , // 234
    0xc6b002 , // 235
    0xd5bf11 , // 236
    0xe4ce20 , // 237
    0xeedd2f , // 238
    0xeeec3e , // 239
    0x340000 , // 240
    0x430e00 , // 241
    0x521d00 , // 242
    0x612c00 , // 243
    0x703b00 , // 244
    0x7e4a00 , // 245
    0x8d5900 , // 246
//  0x9c6800 , // 247
    0xad5f64 ,
//  0xab7700 , // 248
    0x8d5900 ,
    0xba8600 , // 249
    0xc9950b , // 250
    0xd8a41a , // 251
    0xe7b329 , // 252
    0xeec238 , // 253
    0xeed146 , // 254
//  0xeee055 , // 255
    0xba8600
};

// clang-format on

void TransAtariColorScott(uint8_t index, uint8_t value)
{
    SetColor(index, colors[value]);
}

/*
 The values below are determined by looking at the games
 running in the VICE C64 emulator.
 I have no idea how the original interpreter calculates
 them. I might have made some mistakes. */

void TransC64ColorScott(uint8_t index, uint8_t value)
{
    debug_print("Color %d: %d\n", index, value);
    switch (value) {
        case 2:
        case 3:
        case 4:
        case 8:
        case 9:
        case 10:
        case 12:
        case 14:
        case 15:
        case 255:
        case 137:
        case 142:
            SetColor(index, white);
            break;
        case 35:
        case 36:
        case 38:
        case 40:
        case 244:
        case 246:
        case 248:
            SetColor(index, brown);
            break;
        case 24:
        case 26:
        case 30:
        case 46:
        case 16:
        case 230:
        case 237:
        case 238:
        case 252:
            SetColor(index, yellow);
            break;
        case 50:
        case 51:
        case 52:
        case 53:
        case 54:
        case 56:
        case 58:
        case 59:
        case 60:
        case 66:
        case 62:
            SetColor(index, orange);
            break;
        case 67:
        case 68:
        case 69:
        case 70:
        case 71:
            SetColor(index, red);
            break;
        case 0:
        case 77:
        case 81:
        case 84:
        case 85:
        case 86:
        case 87:
        case 97:
        case 101:
        case 102:
        case 103:
        case 105:
        case 224:
            SetColor(index, purple);
            break;
        case 89:
            SetColor(index, lred);
            break;
        case 1:
        case 7:
        case 116:
        case 135:
        case 148:
        case 151:
        case 153:
            SetColor(index, blue);
            break;
        case 110:
        case 157:
            SetColor(index, lblue);
            break;
        case 161:
            SetColor(index, grey);
            break;
        case 17:
        case 20:
        case 179:
        case 182:
        case 183:
        case 194:
        case 195:
        case 196:
        case 197:
        case 198:
        case 199:
        case 200:
        case 212:
        case 214:
        case 215:
        case 216:
            SetColor(index, green);
            break;
        case 201:
            SetColor(index, lgreen);
            break;
        default:
            fprintf(stderr, "Unknown color %d ", value);
            break;
    }
    fprintf(stderr, "\n");
}

static USImage *current_image = NULL;

int C64A8AdjustScott(int width, int height, int *x_origin) {

    USImage *image = current_image;
    if (image == NULL)
        return width;

    glui32 curheight, curwidth;
    glk_window_get_size(Graphics, &curwidth, &curheight);

    if (image->usage == IMG_ROOM || (width == 38 && *x_origin < 1)) {
        width = width - 1 - image->cropright / 8;
        left_margin = image->cropleft;

        ImageWidth = width * 8 - 17 + left_margin;
        ImageHeight = height + 2;

        if (image->index == 19 && image->systype == SYS_ATARI8) {
            width++;
            ImageWidth = 308;
        }

        int width_in_pixels = ImageWidth * pixel_size;
        while (width_in_pixels > curwidth && pixel_size > 1) {
            pixel_size--;
            width_in_pixels = ImageWidth * pixel_size;
        }

        x_offset = (curwidth - width_in_pixels) / 2;

        int optimal_height = ImageHeight * pixel_size;

        if (curheight != optimal_height) {
            winid_t parent = glk_window_get_parent(Graphics);
            if (parent)
                glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                           optimal_height, NULL);
        }
    }

    return width;
}

int DrawC64A8Image(USImage *image)
{
    current_image = image;
    return DrawC64A8ImageFromData(image->imagedata, image->datasize, CurrentGame == COUNT_US || CurrentGame == VOODOO_CASTLE_US, C64A8AdjustScott, image->systype == SYS_ATARI8 ? TransAtariColorScott : TransC64ColorScott);
}

