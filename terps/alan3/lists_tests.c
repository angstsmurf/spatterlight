#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>

#include "lists.h"

/* Required mocked modules */
#include "syserr.mock"
#include "instance.mock"


Describe(Lists);

BeforeEach(Lists) {
}

AfterEach(Lists) {
}

Ensure(Lists, can_count_length_of_empty_array) {
  Aword array = -1;
  assert_that(lengthOfArray(&array), is_equal_to(0));
}

Ensure(Lists, can_init_array_to_have_length_zero) {
  Aword *array = allocate(100);
  initArray(array);
  assert_that(lengthOfArrayImplementation(array, 5), is_equal_to(0));
}

Ensure(Lists, can_add_one_element_of_size_one_to_empty_array) {
  Aword *array = allocate(100);
  Aword element = 74;

  initArray(array);
  addElement(array, element);
  assert_that(lengthOfArray(array), is_equal_to(1));
  assert_that(array[0], is_equal_to(74));
}

Ensure(Lists, can_add_two_elements_of_size_one_to_empty_array) {
  Aword *array = allocate(100);
  Aword element = 74;

  initArray(array);
  addElement(array, element);
  element = 75;
  addElement(array, element);
  assert_that(lengthOfArray(array), is_equal_to(2));
  assert_that(array[0], is_equal_to(74));
  assert_that(array[1], is_equal_to(75));
}

Ensure(Lists, can_add_one_element_of_size_five_to_empty_array) {
    struct {Aword a; Aword b; Aword c; Aword d; Aword e;} *array = allocate(100);
    struct {Aword a; Aword b; Aword c; Aword d; Aword e;} element = {74, 75, 76, 77, 78};

    initArray(array);
    addElement(array, element);
    assert_that(lengthOfArray(array), is_equal_to(1));
    assert_that(array[0].a, is_equal_to(74));
    assert_that(array[0].b, is_equal_to(75));
    assert_that(array[0].c, is_equal_to(76));
    assert_that(array[0].d, is_equal_to(77));
    assert_that(array[0].e, is_equal_to(78));
}
