#include "cgreen/cgreen.h"
#include "cgreen/mocks.h"

#include "AltInfo.c"

Describe(AltInfo);
BeforeEach(AltInfo) {}
AfterEach(AltInfo) {}

Ensure(AltInfo, canPrimeAltInfo) {
  AltInfo altInfos[2];
  const int level = 17;
  const int parameter = 18;
  const int instance = 19;
  const int class = 20;

  primeAltInfo(altInfos, level, parameter, instance, class);

  assert_that(level, is_equal_to(altInfos[0].level));
  assert_that(parameter, is_equal_to(altInfos[0].parameter));
  assert_that(instance, is_equal_to(altInfos[0].instance));
  assert_that(class, is_equal_to(altInfos[0].class));
}

/*----------------------------------------------------------------------*/
Ensure(AltInfo, canDuplicateAltInfoArray) {
  AltInfo original[20];
  AltInfo *duplicate;
  static int COPYSIZE = 15;
  int i;

  for (i = 0; i < COPYSIZE; i++) {
	  original[i].instance = i;
	  original[i].end = FALSE;
  }
  original[COPYSIZE].end = TRUE;

  duplicate = duplicateAltInfoArray(original);
  for (i = 0; i < COPYSIZE; i++) {
      assert_that(i, is_equal_to(duplicate[i].instance));
      assert_that(!duplicate[i].end);
  }
  assert_true(duplicate[COPYSIZE].end);
  free(duplicate);
}


/*----------------------------------------------------------------------*/
Ensure(AltInfo, canFindLastAltInfoIndex) {
  AltInfo array[20];
  static int SIZE = 13;
  int i;

  for (i = 0; i < SIZE; i++)
    array[i].end = FALSE;
  array[SIZE].end = TRUE;

  assert_equal(lastAltInfoIndex(array), SIZE-1);
}

/*----------------------------------------------------------------------*/
Ensure(AltInfo, canFindNextFreeAltInfo) {
  AltInfo array[20];
  static int SIZE = 13;
  int i;

  for (i = 0; i < SIZE; i++)
    array[i].end = FALSE;
  array[SIZE].end = TRUE;

  assert_equal(nextFreeAltInfo(array), &array[SIZE]);
}

/*----------------------------------------------------------------------*/
AltEntry altEntry;

AltEntry *finder(int verb, int parameterNumber, int theInstance, int theClass) {
  return &altEntry;
}

Ensure(AltInfo, canAddGlobalAlternatives) {
    AltInfo altInfos[10];
    altInfos[0].end = TRUE;

    addGlobalAlternatives(altInfos, current.verb, &finder);

    assert_that(!altInfos[0].end);
    assert_that(&altEntry, is_equal_to(altInfos[0].alt));
    assert_that(altInfos[0].parameter, is_equal_to(NO_PARAMETER));
    assert_that(altInfos[0].instance, is_equal_to(NO_INSTANCE));
    assert_that(altInfos[0].class, is_equal_to(NO_CLASS));
    assert_that(altInfos[1].end);
}


static AltInfo *mockedAlternativeFinder(int verb, Parameter parameters[]) {
    mock(verb);
    return NULL;
}

Ensure(AltInfo, possibleIndicatesFalseOnEmptyAlternatives) {
    expect(mockedAlternativeFinder, when(verb, is_equal_to(0)));
    globalParameters = newParameterArray();
    assert_false(possibleWithFinder(current.verb, globalParameters, mockedAlternativeFinder));
}

Ensure(AltInfo, canFindExistingVerbEntriesAndReturnsNullForOthers) {
    VerbEntry entries[] = {{1,0}, {2,0}, {-1,0}};

    assert_that(findVerbEntry(1, entries), is_equal_to(&entries[0]));
    assert_that(findVerbEntry(2, entries), is_equal_to(&entries[1]));
    assert_that(findVerbEntry(3, entries), is_null);
}

Ensure(AltInfo, findVerbEntryCanMatchNegativeVerbCode) {
    VerbEntry entries[] = {{1,0}, {-3,0}, {-1,0}};

    /* Meta verbs have negative codes in the entry, but since EOF is -1
     * a verb with code 1 has -2 in the entry */
    assert_that(findVerbEntry(2, entries), is_equal_to(&entries[1]));
}


TestSuite *altInfoTests() {
  TestSuite *suite = create_test_suite();

  add_test_with_context(suite, AltInfo, canPrimeAltInfo);
  add_test_with_context(suite, AltInfo, canDuplicateAltInfoArray);
  add_test_with_context(suite, AltInfo, canFindLastAltInfoIndex);
  add_test_with_context(suite, AltInfo, canFindNextFreeAltInfo);
  add_test_with_context(suite, AltInfo, canAddGlobalAlternatives);
  add_test_with_context(suite, AltInfo, possibleIndicatesFalseOnEmptyAlternatives);
  add_test_with_context(suite, AltInfo, canFindExistingVerbEntriesAndReturnsNullForOthers);

  return suite;
}
