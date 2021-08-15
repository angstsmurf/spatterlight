#include "cgreen/cgreen.h"
#include "cgreen/mocks.h"

#include "reverse.c"

Describe(Reverse);
BeforeEach(Reverse){};
AfterEach(Reverse){};

Ensure(Reverse, canSeeIfReversalIsAlreadyDone) {
  addressesDone = NULL;
  doneSize = 0;
  numberDone = 0;

  assert_that(alreadyDone(0));
  assert_that(doneSize, is_equal_to(0));

  assert_that(alreadyDone(1), is_false);
  assert_that(doneSize, is_equal_to(100));
  assert_that(numberDone, is_equal_to(1));

  assert_that(alreadyDone(2), is_false);
  assert_that(doneSize, is_equal_to(100));
  assert_that(numberDone, is_equal_to(2));

  assert_that(alreadyDone(1));
  assert_that(doneSize, is_equal_to(100));
  assert_that(numberDone, is_equal_to(2));
}

static void mySyserr(char *string) {
    mock(string);
}

Ensure(Reverse, reverseTableProhibitsTableElementsSmallerThanAword) {
    setSyserrHandler(mySyserr);
    expect(mySyserr);

    reverseTable(0, sizeof(Aword)-1);
}

Ensure(Reverse, reverseTableProhibitsTableElementsNotMultipleOfAword) {
    setSyserrHandler(mySyserr);
    expect(mySyserr);

    reverseTable(0, 4*sizeof(Aword)+1);
}
