#include <cgreen/cgreen.h>

#include "stack.h"


#include "syserr.mock"
#include "instance.mock"


Describe(Stack);

BeforeEach(Stack) {
}

AfterEach(Stack) {
}


Ensure(Stack, can_return_stack_depth) {
    Stack stack = createStack(10);
    int initial_depth = stackDepth(stack);

    push(stack, 1);
    assert_that(stackDepth(stack), is_equal_to(initial_depth+1));

    pop(stack);
    assert_that(stackDepth(stack), is_equal_to(initial_depth));
}
