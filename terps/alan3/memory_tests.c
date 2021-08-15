#include <cgreen/cgreen.h>

#include "memory.h"

/* Mocks */
#include "syserr.mock"
#include "instance.mock"


Describe(Memory);
BeforeEach(Memory) {}
AfterEach(Memory) {}

Ensure(Memory, can_convert_a_single_pointer_to_and_from_aptr) {
    assert_that(fromAptr(toAptr("")), is_equal_to_string(""));
}

Ensure(Memory, can_convert_two_pointers_to_and_from_aptr) {
    Aptr a = toAptr("a");
    Aptr b = toAptr("b");

    assert_that(fromAptr(a), is_equal_to_string("a"));
    assert_that(fromAptr(b), is_equal_to_string("b"));
}

Ensure(Memory, can_convert_three_pointers_to_and_from_aptr) {
    Aptr a = toAptr("a");
    Aptr b = toAptr("b");
    Aptr c = toAptr("c");

    assert_that(fromAptr(b), is_equal_to_string("b"));
    assert_that(fromAptr(a), is_equal_to_string("a"));
    assert_that(fromAptr(c), is_equal_to_string("c"));
}

Ensure(Memory, will_not_return_same_aptr_for_different_pointers) {
    Aptr aptrs[1000];
    int i, j;

    for (i=0; i<1000; i++) {
        aptrs[i] = toAptr(&aptrs[i]);
        for (j=0; j < i; j++) {
            if (aptrs[i] == aptrs[j])
                fail_test("Same aptr allocated twice");
        }
    }

}

Ensure(Memory, will_forget_aptr_for_deallocated_memory) {
    void *allocated = allocate(5);
    Aptr aptr = toAptr(allocated);

    assert_that(fromAptr(aptr), is_equal_to(allocated));
    deallocate(allocated);
    expect(syserr);
    fromAptr(aptr);
}
