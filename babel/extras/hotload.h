/* hotload.h : The babel registry hotloader header
  L. Ross Raszewski
  This code is freely usable for all purposes.
 
  This work is licensed under the Creative Commons Attribution 4.0 License.
  To view a copy of this license, visit
  https://creativecommons.org/licenses/by/4.0/ or send a letter to
  Creative Commons,
  PO Box 1866,
  Mountain View, CA 94042, USA.
 
*/

#include "treaty.h"

typedef TREATY (*load_treaty)(char *, void *);

int babel_hotload(char *, char *, load_treaty, void *, void *);

