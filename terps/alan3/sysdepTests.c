/*======================================================================*\

  sysdepTests.c

  Unit tests for sysdep module in the Alan interpreter

\*======================================================================*/

#include "cgreen/cgreen.h"

#include "sysdep.c"


Describe(Sysdep);
BeforeEach(Sysdep) {}
AfterEach(Sysdep) {}


Ensure(Sysdep, canCompareStringsIgnoringCase)
{
  assert_true(compareStrings("abcd", "abcd")==0);
  assert_true(compareStrings("abcd", "Abcd")==0);
  assert_true(compareStrings("abcd", "aBcd")==0);
  assert_true(compareStrings("abcd", "abCd")==0);
  assert_true(compareStrings("abcd", "abcD")==0);
  assert_true(compareStrings("abcd", "abcd")==0);
  assert_true(compareStrings("ABCD", "Abcd")==0);
  assert_true(compareStrings("ABCD", "aBcd")==0);
  assert_true(compareStrings("ABCD", "abCd")==0);
  assert_true(compareStrings("ABCD", "abcD")==0);
  assert_true(compareStrings("abcd", "abcde")!=0);
  assert_true(compareStrings("abcde", "Abcd")!=0);
  assert_true(compareStrings("abc", "aBcd")!=0);
  assert_true(compareStrings("abd", "abCd")!=0);
  assert_true(compareStrings("bcd", "abcD")!=0);
}

Ensure(Sysdep, canSeeLowerCase)
{
  assert_true(isLowerCase(246));
}

