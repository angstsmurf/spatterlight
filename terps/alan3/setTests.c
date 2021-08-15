#include "cgreen/cgreen.h"

#include "set.c"

Describe(Set);
BeforeEach(Set) {}
AfterEach(Set) {}


Ensure(Set, testAddToSet) {
  Set aSet = {0, 0, NULL};
  int i;

  /* Add a single value */
  addToSet(&aSet, 0);
  assert_true(aSet.size == 1);
  assert_true(aSet.members[0] == 0);
  /* Add it again, should not extend the set */
  addToSet(&aSet, 0);
  assert_true(aSet.size == 1);

  /* Add a number of elements so that we need to extend */
  for (i = 1; i<6; i++) {
    addToSet(&aSet, i);
    assert_true(aSet.size == i+1);
    assert_true(aSet.members[i] == i);
  }
  assert_true(aSet.size == 6);
}

Ensure(Set, testSetUnion) {
  Set *set0 = newSet(0);
  Set *set678 = newSet(3);
  Set *set456 = newSet(3);
  Set *theUnion;

  /* Test adding an empty set */
  theUnion = setUnion(set0, set0);
  assert_true(setSize(theUnion)==0);

  addToSet(set678, 6);
  addToSet(set678, 7);
  addToSet(set678, 8);
  theUnion = setUnion(set0, set678);
  assert_true(setSize(theUnion)==3);
  assert_true(inSet(theUnion, 6));
  assert_true(inSet(theUnion, 7));
  assert_true(inSet(theUnion, 8));

  addToSet(set456, 4);
  addToSet(set456, 5);
  addToSet(set456, 6);
  theUnion = setUnion(set456, set678);
  assert_true(setSize(theUnion)==5);
  assert_true(inSet(theUnion, 4));
  assert_true(inSet(theUnion, 5));
  assert_true(inSet(theUnion, 6));
  assert_true(inSet(theUnion, 7));
  assert_true(inSet(theUnion, 8));
}

Ensure(Set, testSetRemove) {
  Set *aSet = newSet(0);
  int i;

  /* Add a number of elements to remove */
  for (i = 1; i<6; i++)
    addToSet(aSet, i);

  assert_true(aSet->size == 5);

  removeFromSet(aSet, 1);
  assert_true(aSet->size == 4);

  /* Do it again, should not change... */
  removeFromSet(aSet, 1);
  assert_true(aSet->size == 4);

  removeFromSet(aSet, 5);
  assert_true(aSet->size == 3);
  removeFromSet(aSet, 4);
  assert_true(aSet->size == 2);
  removeFromSet(aSet, 3);
  assert_true(aSet->size == 1);
  removeFromSet(aSet, 2);
  assert_true(aSet->size == 0);

  removeFromSet(aSet, 1);
  assert_true(aSet->size == 0);
}

Ensure(Set, testInSet) {
  Set *aSet = newSet(0);
  int i;

  assert_true(!inSet(aSet, 0));
  for (i = 6; i>0; i--)
    addToSet(aSet, i);
  for (i = 1; i<7; i++)
    assert_true(inSet(aSet, i));
}

Ensure(Set, testClearSet) {
  Set *aSet = newSet(0);
  int i;

  for (i = 6; i>0; i--)
    addToSet(aSet, i);
  clearSet(aSet);
  assert_true(setSize(aSet) == 0);
}


Ensure(Set, testCompareSets) {
  Set *set1 = newSet(0);
  Set *set2 = newSet(0);
  Set *set3 = newSet(0);
  Set *set4 = newSet(0);

  addToSet(set3, 4);
  addToSet(set4, 4);

  assert_true(equalSets(set1, set2));
  assert_true(!equalSets(set1, set3));
  assert_true(equalSets(set3, set4));
}
