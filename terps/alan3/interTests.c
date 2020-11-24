#include "cgreen/cgreen.h"

#include "inter.c"


static void syserrHandler(char *message) {
    assert_true_with_message(FALSE, "Unexpected call to syserr()");
}

static Stack theStack;


Describe(Inter);
BeforeEach(Inter) {
  theStack = createStack(50);
  setInterpreterStack(theStack);
  setSyserrHandler(syserrHandler);
  memTop = 100;
}
AfterEach(Inter) {
  deleteStack(theStack);
  setSyserrHandler(NULL);
}

/*----------------------------------------------------------------------*/
Ensure(Inter, testBlockInstructions)
{
  Aword blockInstructionCode[] = {4, /* Dummy to not execute at zero */
				  4,
				  INSTRUCTION(I_FRAME),
				  4,
				  INSTRUCTION(I_RETURN)};
  Aword localsInstructionCode[] = {4, /* Dummy to not execute at zero */
				   33, /* Value */
				   1, /* Local variable (starts at 1) */
				   0, /* Number of blocks down */
				   INSTRUCTION(I_SETLOCAL),
				   1, /* Local variable (starts at 1) */
				   0, /* Number of blocks down */
				   INSTRUCTION(I_GETLOCAL),
				   INSTRUCTION(I_RETURN)};

  memory = blockInstructionCode;

  /* Add a block with four local variables */
  interpret(1);

  assert_equal(1/*old fp*/ + 4/*Locals*/ + 1/*The extra "4"*/, stackDepth(theStack));

  memory = localsInstructionCode;
  interpret(1);
  assert_true(pop(theStack) == 33);
}


/*----------------------------------------------------------------------*/
Ensure(Inter, testLoopInstruction)
{
  Aword loopInstructionCode1[] = {4, /* Dummy to not execute at zero */
				  43, /* Marker */
				  12, /* Limit */
				  1, /* Index */
				  INSTRUCTION(I_LOOP),
				  INSTRUCTION(I_RETURN)};
  memory = loopInstructionCode1;
  interpret(1);			/* Should not do anything */
  assert_true(pop(theStack) == 1 && pop(theStack) == 12); /* Index and limit untouched */
  assert_true(pop(theStack) == 43);		/* So the stack should contain the marker */
}


/*----------------------------------------------------------------------*/
Ensure(Inter, testLoopEndInstruction)
{
  Aword loopEndInstructionCode[] = {4, /* Dummy to not execute at zero */
				  1,
				  INSTRUCTION(I_FRAME),
				  4, /* Marker on the stack */
				  12, /* End value */
				  9, /* Start value */
				  INSTRUCTION(I_LOOP),
				  INSTRUCTION(I_DUP),
				  1,
				  0,
				  INSTRUCTION(I_SETLOCAL),
				  INSTRUCTION(I_LOOPEND),
				  INSTRUCTION(I_RETURN)};
  memory = loopEndInstructionCode;
  interpret(1);
  assert_true(getLocal(theStack, 0, 1) == 12);
  assert_true(pop(theStack) == 4);
}


/*----------------------------------------------------------------------*/
Ensure(Inter, testGoToLoop) {
  Aword testGoToLoopCode[] = {0,
			      INSTRUCTION(I_LOOP), /* 1 */
			      4,
			      INSTRUCTION(I_LOOP),
			      4,
			      INSTRUCTION(I_LOOPNEXT),
			      INSTRUCTION(I_LOOPNEXT),
			      INSTRUCTION(I_LOOPEND),
			      5,
			      INSTRUCTION(I_LOOPEND)}; /* 9 */
  memory = testGoToLoopCode;
  pc = 9;
  jumpBackToStartOfMatchingLOOP();
  assert_true(pc == 1);
}


/*----------------------------------------------------------------------*/
Ensure(Inter, testLoopNext) {
  Aword testLoopNextCode[] = {0,
			      INSTRUCTION(I_LOOP),
			      4, /* 2 */
			      INSTRUCTION(I_LOOP),
			      4,
			      INSTRUCTION(I_LOOPNEXT),
			      INSTRUCTION(I_LOOPNEXT),
			      INSTRUCTION(I_LOOPEND),
			      5,
			      INSTRUCTION(I_LOOPEND)}; /* 9 */
  memory = testLoopNextCode;
  pc = 2;
  nextLoop();
  assert_true(pc == 9);
}


/*----------------------------------------------------------------------*/
Ensure(Inter, testCountInstruction)
{
  Aword testCountInstructionCode[] = {0,
				      INSTRUCTION(I_COUNT), /* 7 */
				      INSTRUCTION(I_RETURN)}; /* 8 */
  memory = testCountInstructionCode;

  /* Execute an I_COUNT */
  push(theStack, 2);			/* Faked COUNT value */
  push(theStack, 44);			/* Faked limit */
  push(theStack, 5);			/* Faked loop index */
  interpret(1);
  assert_equal(5, pop(theStack));	/* Values intact */
  assert_equal(44, pop(theStack));	/* Values intact */
  assert_equal(3, pop(theStack));	/* Incremented COUNT */
}


/*----------------------------------------------------------------------*/
Ensure(Inter, testMaxInstruction) {
  Aword testMaxInstructionCode[] = {0,
				    INSTRUCTION(I_MAX),
				    INSTRUCTION(I_RETURN)};

  push(theStack, 3);			/* Previous aggregate value */
  push(theStack, 11);			/* Limit */
  push(theStack, 12);			/* Index */
  push(theStack, 2);			/* Attribute value */
  memory = testMaxInstructionCode;
  interpret(1);
  assert_equal(12, pop(theStack));
  assert_equal(11, pop(theStack));
  assert_equal(3, pop(theStack));

  push(theStack, 3);			/* Previous aggregate value */
  push(theStack, 11);			/* Limit */
  push(theStack, 12);			/* Index */
  push(theStack, 4);			/* Attribute value */
  memory = testMaxInstructionCode;
  interpret(1);
  assert_equal(12, pop(theStack));
  assert_equal(11, pop(theStack));
  assert_equal(4, pop(theStack));
}


/*----------------------------------------------------------------------*/
Ensure(Inter, MaxInstanceForBeta3DoesDetractTheLiteralInstance) {
  Aword testMaxInstanceCode[] = {0,
				 CURVAR(V_MAX_INSTANCE),
				 INSTRUCTION(I_RETURN)};
  header->instanceMax = 12;
  memory = testMaxInstanceCode;
  interpret(1);
  assert_true(pop(theStack) == header->instanceMax-1);
}


/*----------------------------------------------------------------------*/
Ensure(Inter, MaxInstanceInstructionForPreBeta3ReturnsNumberOfInstances) {
  Aword testMaxInstanceCode[] = {0,
				 CURVAR(V_MAX_INSTANCE),
				 INSTRUCTION(I_RETURN)};
  header->instanceMax = 12;
  header->version[3] = 3;
  header->version[2] = 0;
  header->version[1] = 2;
  header->version[0] = 'b';
  memory = testMaxInstanceCode;
  interpret(1);
  assert_that(pop(theStack), is_equal_to(header->instanceMax));
}


/*----------------------------------------------------------------------*/
TestSuite *interTests(void)
{
  TestSuite *suite = create_test_suite();

  add_test_with_context(suite, Inter, testBlockInstructions);
  add_test_with_context(suite, Inter, testGoToLoop);
  add_test_with_context(suite, Inter, testLoopNext);
  add_test_with_context(suite, Inter, testLoopInstruction);
  add_test_with_context(suite, Inter, testLoopEndInstruction);
  add_test_with_context(suite, Inter, testMaxInstruction);
  add_test_with_context(suite, Inter, testCountInstruction);
  add_test_with_context(suite, Inter, MaxInstanceInstructionForPreBeta3ReturnsNumberOfInstances);
  add_test_with_context(suite, Inter, MaxInstanceForBeta3DoesDetractTheLiteralInstance);

  return suite;
}
