/* register.c    Register modules for the babel handler api
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
 * The purpose of this file is to create the treaty_registry array.
 * This array is a null-terminated list of the known treaty modules.
 */

#include <stdlib.h>
#include "modules.h"


TREATY treaty_registry[] = {
        #define TREATY_REGISTER
        #include "modules.h"
        NULL
        };

TREATY container_registry[] = {
        #define CONTAINER_REGISTER
        #include "modules.h"
        NULL

};

