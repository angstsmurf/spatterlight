/* register_ifiction.c    Register modules for babel's ifiction API
 *
 * 2006 by L. Ross Raszewski
 *
 * This code is freely usable for all purposes.
 *
 * This work is licensed under the Creative Commons Attribution2.5 License.
 * To view a copy of this license, visit
 * http://creativecommons.org/licenses/by/2.5/ or send a letter to
 * Creative Commons,
 * 543 Howard Street, 5th Floor,
 * San Francisco, California, 94105, USA.
 *
 * This file depends on modules.h
 *
 * This version of register.c is stripped down to include only the
 * needed functionality for the ifiction api
 */

#include <stdlib.h>
#include "treaty.h"

char *format_registry[] = {
        #define TREATY_REGISTER
        #define CONTAINER_REGISTER
        #define FORMAT_REGISTER
        #include "modules.h"
        NULL
};
