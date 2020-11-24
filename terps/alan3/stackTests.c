#include "cgreen/cgreen.h"

#include "stack.c"

static Stack theStack;


Describe(Stack);

BeforeEach(Stack) {
  theStack = createStack(50);
}

AfterEach(Stack) {
  deleteStack(theStack);
}



/*----------------------------------------------------------------------*/
Ensure(Stack, allocatesCorrectSpaceInNewFrame) {
  /* Add a block with four local variables */
  newFrame(theStack, 4);
  assert_equal(1/*old fp*/ + 4/*Locals*/, stackDepth(theStack));
}


/*----------------------------------------------------------------------*/
Ensure(Stack, testNewFrameInStack) {
    Aword previous_fp = theStack->framePointer;
    
    /* Add a block with four local variables */
    newFrame(theStack, 4);
    assert_that(stackDepth(theStack), is_equal_to(1/*old fp*/ + 4/*Locals*/));
    assert_that(theStack->framePointer, is_equal_to(1));
    assert_that( getLocal(theStack, 0,1), is_equal_to(0));
  
    setLocal(theStack, 0,1,14);
    assert_that(getLocal(theStack, 0,1), is_equal_to(14));
    assert_that(theStack->stack[theStack->stackp - 4], is_equal_to(14));
    assert_that(theStack->stack[theStack->stackp - 5], is_equal_to(previous_fp));

    endFrame(theStack);
    assert_that(stackDepth(theStack), is_equal_to(0));
}


/*----------------------------------------------------------------------*/
Ensure(Stack, testFrameInFrame) {
  /* Add a block with one local variable */
  newFrame(theStack, 1);
  setLocal(theStack, 0,1,14);
  assert_equal(14, getLocal(theStack, 0,1));

  newFrame(theStack, 1);
  setLocal(theStack, 0,1,15);
  assert_equal(15, getLocal(theStack, 0,1));
  assert_equal(14, getLocal(theStack, 1,1));
  endFrame(theStack);
  endFrame(theStack);
  assert_equal(0, stackDepth(theStack));
}

/*----------------------------------------------------------------------*/
Ensure(Stack, testPushAndPop) {
  Stack myStack = createStack(10);

  push(myStack, 1);
  push(myStack, 2);
  push(myStack, 3);
  push(myStack, 4);
  assert_equal(4, stackDepth(myStack));
  assert_equal(4, pop(myStack));
  assert_equal(3, pop(myStack));
  assert_equal(2, pop(myStack));
  assert_equal(1, pop(myStack));
  assert_equal(0, stackDepth(myStack));
}


TestSuite *stackTests()
{
    TestSuite *suite = create_test_suite();

    set_setup(suite, setup);
    set_teardown(suite, teardown);

    add_test_with_context(suite, Stack, allocatesCorrectSpaceInNewFrame);
    add_test_with_context(suite, Stack, testNewFrameInStack);
    add_test_with_context(suite, Stack, testFrameInFrame);
    add_test_with_context(suite, Stack, testPushAndPop);

    return suite;
}
