#include "cgreen/cgreen.h"

#define RUNNING_UNITTESTS
#include "output.c"

Describe(Output);
BeforeEach(Output) {}
AfterEach(Output) {}


Ensure(Output, testUpdateColumn) {
  assert_true(updateColumn(0, "\n") == 1);
  assert_true(updateColumn(11, "123456789") == 20);
}

Ensure(Output, testPunctuationNext) {
  assert_true(punctuationNext("."));
  assert_true(punctuationNext("!"));
  assert_true(punctuationNext("?"));
  assert_true(punctuationNext(".$p"));
  assert_true(punctuationNext(".$n"));
  assert_true(punctuationNext(".$t"));
  assert_true(!punctuationNext("a."));
  assert_true(!punctuationNext("$p."));
}

Ensure(Output, testSpaceEquivalent){
  assert_true(isSpaceEquivalent("$p"));
  assert_true(isSpaceEquivalent("$pafdjljf"));
  assert_true(isSpaceEquivalent("$t"));
  assert_true(isSpaceEquivalent("$n"));
  assert_true(isSpaceEquivalent("$i"));
  assert_true(!isSpaceEquivalent("$$"));
  assert_true(!isSpaceEquivalent("abc"));
  assert_true(!isSpaceEquivalent("..."));
  assert_true(!isSpaceEquivalent(""));
  assert_true(isSpaceEquivalent(" "));
}
