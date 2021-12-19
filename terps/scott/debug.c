//
//  debug.c
//  scott
//
//  Created by Administrator on 2022-01-19.
//

#include "debug.h"

void print_header_info(int header[]) {
    uint16_t value;
    for (int i = 0; i < 8; i++)
    {
        value=header[i];
        fprintf(stderr, "b $%X %d: ", 0x494d + 0x3FE5 + i * 2, i);
        switch(i)
        {
            case 0 :
                fprintf(stderr, "???? =\t%x\n",value);
                break;

            case 1 :
                fprintf(stderr, "Number of items =\t%d\n",value);
                break;

            case 2 :
                fprintf(stderr, "Number of actions =\t%d\n",value);
                break;

            case 3 :
                fprintf(stderr, "Number of words =\t%d\n",value);
                break;

            case 4 :
                fprintf(stderr, "Number of rooms =\t%d\n",value);
                break;

            case 5 :
                fprintf(stderr, "Max carried items =\t%d\n",value);
                break;

            case 6 :
                fprintf(stderr, "Word length =\t%d\n",value);
                break;

            case 7 :
                fprintf(stderr, "Number of messages =\t%d\n",value);
                break;

            case 8 :
                fprintf(stderr, "???? =\t%x\n",value);
                break;

            case 9 :
                fprintf(stderr, "???? =\t%x\n",value);
                break;

            case 10 :
                fprintf(stderr, "???? =\t%x\n",value);
                break;

            case 11 :
                fprintf(stderr, "???? =\t%x\n",value);
                break;

            default :
                break;
        }
    }
}

void list_action_addresses(FILE *ptr) {
    int offset = 0x8110 - 0x3fe5;
    int low_byte, high_byte;
    fseek(ptr,offset,SEEK_SET);
    for (int i = 52; i<102; i++) {
        low_byte = fgetc(ptr);
        high_byte = fgetc(ptr);
        fprintf(stderr, "Address of action %d: %x\n", i, low_byte + 256 * high_byte );
    }
}

void list_condition_addresses(FILE *ptr) {
    int offset = 0x33A0 - 34;
    int low_byte, high_byte;
    fseek(ptr,offset,SEEK_SET);
    for (int i = 0; i<20; i++) {
        low_byte = fgetc(ptr);
        high_byte = fgetc(ptr);
        fprintf(stderr, "Address of condition %d: %x\n", i, low_byte + 256 * high_byte );
    }

}
