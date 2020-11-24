/*======================================================================*\

  exeTests.c

  Unit tests for exe module in the Alan interpreter
  using cgreen unit test framework

\*======================================================================*/

#include "cgreen/cgreen.h"

#include "exe.c"

#include "class.h"
#include "Container.h"


static void tearDown() {
  setSyserrHandler(NULL);
}

Describe(Exe);
BeforeEach(Exe) {}
AfterEach(Exe) {tearDown();}


/*----------------------------------------------------------------------*/
Ensure(Exe, canCountTrailingBlanks) {
  char *threeBlanks = "h   ";
  char *fiveBlanks = "     ";
  char *empty = "";
  assert_true(countTrailingBlanks(threeBlanks, strlen(threeBlanks)-1) == 3);
  assert_true(countTrailingBlanks(threeBlanks, 1) == 1);
  assert_true(countTrailingBlanks(threeBlanks, 0) == 0);
  assert_true(countTrailingBlanks(fiveBlanks, strlen(fiveBlanks)-1) == 5);
  assert_true(countTrailingBlanks(empty, strlen(empty)-1) == 0);
}


/*----------------------------------------------------------------------*/
Ensure(Exe, canSkipWordForwards) {
  char *string = "a string of words";

  assert_true(skipWordForwards(string, 0) == 1);
  assert_true(skipWordForwards(string, 2) == 8);
  assert_true(skipWordForwards(string, 9) == 11);
  assert_true(skipWordForwards(string, 12) == 17);
  assert_true(skipWordForwards(string, strlen(string)-1) == strlen(string));
}


/*----------------------------------------------------------------------*/
Ensure(Exe, canSkipWordBackwards) {
  char *string = "a string of words";
  char *emptyString = "";

  assert_true(skipWordBackwards(string, 0) == 0);
  assert_true(skipWordBackwards(string, 4) == 2);
  assert_true(skipWordBackwards(string, 10) == 9);
  assert_true(skipWordBackwards(string, strlen(string)) == 12);

  assert_true(skipWordBackwards(emptyString, strlen(emptyString)) == 0);
}


/*----------------------------------------------------------------------*/
Ensure(Exe, canStripCharsFromString) {
  char *characters;
  char *rest;
  char *result;

  characters = "abcdef";
  result = stripCharsFromStringForwards(3, characters, &rest);
  assert_true(strcmp(result, "abc")==0);
  assert_true(strcmp(rest, "def")==0);

  characters = "ab";
  result = stripCharsFromStringForwards(3, characters, &rest);
  assert_true(strcmp(result, "ab")==0);
  assert_true(strcmp(rest, "")==0);

  characters = "";
  result = stripCharsFromStringForwards(3, characters, &rest);
  assert_true(strcmp(result, "")==0);
  assert_true(strcmp(rest, "")==0);

  characters = "abcdef";
  result = stripCharsFromStringBackwards(3, characters, &rest);
  assert_true(strcmp(result, "def")==0);
  assert_true(strcmp(rest, "abc")==0);

  characters = "ab";
  result = stripCharsFromStringBackwards(3, characters, &rest);
  assert_true(strcmp(result, "ab")==0);
  assert_true(strcmp(rest, "")==0);

  characters = "";
  result = stripCharsFromStringBackwards(3, characters, &rest);
  assert_true(strcmp(result, "")==0);
  assert_true(strcmp(rest, "")==0);
}


/*----------------------------------------------------------------------*/
Ensure(Exe, canStripWordsFromString) {
  char *testString = "this is four  words";
  char *empty = "";
  char *result;
  char *rest;

  result = stripWordsFromStringForwards(3, empty, &rest);
  assert_true(strcmp(result, "") == 0);
  assert_true(strcmp(rest, "") == 0);

  result = stripWordsFromStringForwards(3, testString, &rest);
  assert_true(strcmp(result, "this is four") == 0);
  assert_true(strcmp(rest, "words") == 0);

  result = stripWordsFromStringForwards(7, testString, &rest);
  assert_true(strcmp(result, "this is four  words") == 0);
  assert_true(strcmp(rest, "") == 0);

  result = stripWordsFromStringBackwards(3, empty, &rest);
  assert_true(strcmp(result, "") == 0);
  assert_true(strcmp(rest, "") == 0);

  result = stripWordsFromStringBackwards(3, testString, &rest);
  assert_true(strcmp(result, "is four  words") == 0);
  assert_true(strcmp(rest, "this") == 0);

  result = stripWordsFromStringBackwards(7, testString, &rest);
  assert_true(strcmp(result, "this is four  words") == 0);
  assert_true(strcmp(rest, "") == 0);

  testString = " an initial space";
  result = stripWordsFromStringBackwards(7, testString, &rest);
  assert_true(strcmp(result, "an initial space") == 0);
  assert_true(strcmp(rest, "") == 0);
}


static char testFileName[] = "getStringTestFile";
static FILE *testFile;
static void writeAndOpenGetStringTestFile(int fpos, char *testString)
{
  int i;

  testFile = fopen(testFileName, "wb");
  for (i = 0; i < fpos; i++) fputc(' ', testFile);
  fprintf(testFile, "%s", testString);
  fclose(testFile);
  textFile = fopen(testFileName, "rb");
}


/*----------------------------------------------------------------------*/
Ensure(Exe, canGetString)
{
  int fpos = 55;
  char testString[] = "hejhopp";

  writeAndOpenGetStringTestFile(fpos, testString);
  header->pack = FALSE;
  header->stringOffset = 0;
  assert_true(strcmp(getStringFromFile(fpos, strlen(testString)), testString)==0);
  header->stringOffset = 1;
  assert_true(strcmp(getStringFromFile(fpos, strlen(testString)-1), &testString[1])==0);
  fclose(textFile);
  unlink(testFileName);
}


/*----------------------------------------------------------------------*/
Ensure(Exe, testIncreaseEventQueue) {
  eventQueueSize = 0;
  eventQueue = NULL;
  eventQueueTop = 0;

  increaseEventQueue();

  assert_true(eventQueueSize != 0);
  assert_true(eventQueue != NULL);
}



/*----------------------------------------------------------------------*/
static bool syserrHandlerCalled;

jmp_buf syserrLabel;


static void syserrHandler(char *message) {
  syserrHandlerCalled = TRUE;
  printf("%s\n", message);
  longjmp(syserrLabel, TRUE);
}


// TODO this does not work as expected
// setjmp can not be called in a function
// that is not calling the code doing the
// jumping. Copy syserr handling from
// compiler unit tests
static int triedAndNoSyserrCaught() {
  setSyserrHandler(syserrHandler);
  syserrHandlerCalled = FALSE;
  if (setjmp(syserrLabel) == 0)
    return FALSE;
  else {
    assert_true(syserrHandlerCalled);
    return TRUE;
  }
}

static void failAssertion(void) {
	assert_true(FALSE);
}

/*----------------------------------------------------------------------*/
Ensure(Exe, syserrOnWhereForIllegalId) {
  header->instanceMax = 1;

  if (triedAndNoSyserrCaught()) {
    where(0, DIRECT);
    failAssertion();
  }

  if (triedAndNoSyserrCaught()) {
    where(2, DIRECT);
    failAssertion();
  }
}


/*----------------------------------------------------------------------*/
Ensure(Exe, syserrOnHereForIllegalId) {
  header->instanceMax = 1;

  if (triedAndNoSyserrCaught()) {
    isHere(0, FALSE);
    failAssertion();
  }

  if (triedAndNoSyserrCaught()) {
    isHere(2, FALSE);
    failAssertion();
  }
}


/*----------------------------------------------------------------------*/
Ensure(Exe, syserrOnLocateIllegalId) {
  header->instanceMax = 1;

  if (triedAndNoSyserrCaught()) {
    locate(0, 1);
    failAssertion();
  }

  if (triedAndNoSyserrCaught()) {
    locate(2, 1);
    failAssertion();
  }

  if (triedAndNoSyserrCaught()) {
    locate(1, 0);
    failAssertion();
  }

  if (triedAndNoSyserrCaught()) {
    locate(1, 2);
    failAssertion();
  }
}


/*----------------------------------------------------------------------*/
Ensure(Exe, callingWhereReturnsExpectedValues) {
    int LOCATION_CLASS = 1;
    /* TODO: define NOWHERE == 1 in acode.h */
    int FIRST_INSTANCE = 2;     /* Avoid 1 which is the predefined #nowhere */
    int SECOND_INSTANCE = FIRST_INSTANCE + 1;
    int THIRD_INSTANCE = SECOND_INSTANCE + 1;
    int FOURTH_INSTANCE = THIRD_INSTANCE + 1;
    int MAX_INSTANCE = FOURTH_INSTANCE + 1;

  admin = allocate(MAX_INSTANCE*sizeof(AdminEntry));
  instances = allocate(MAX_INSTANCE*sizeof(InstanceEntry));
  classes = allocate(MAX_INSTANCE*sizeof(ClassEntry));
  header = allocate(sizeof(ACodeHeader));

  header->locationClassId = LOCATION_CLASS;
  header->instanceMax = MAX_INSTANCE;

  instances[FIRST_INSTANCE].parent = LOCATION_CLASS;	/* A location */
  admin[FIRST_INSTANCE].location = THIRD_INSTANCE;
  assert_true(where(FIRST_INSTANCE, DIRECT) == 0);	/* Locations are always nowhere */
  assert_true(where(FIRST_INSTANCE, TRANSITIVE) == 0);

  instances[SECOND_INSTANCE].parent = 0;	/* Not a location */
  admin[SECOND_INSTANCE].location = FIRST_INSTANCE;	/* At FIRST_INSTANCE */
  assert_true(where(SECOND_INSTANCE, DIRECT) == FIRST_INSTANCE);
  assert_true(where(SECOND_INSTANCE, TRANSITIVE) == FIRST_INSTANCE);

  instances[THIRD_INSTANCE].parent = 0;	/* Not a location */
  admin[THIRD_INSTANCE].location = SECOND_INSTANCE;	/* In SECOND_INSTANCE which is at FIRST_INSTANCE */
  assert_true(where(THIRD_INSTANCE, DIRECT) == SECOND_INSTANCE);
  assert_true(where(THIRD_INSTANCE, TRANSITIVE) == FIRST_INSTANCE);

  instances[FOURTH_INSTANCE].parent = 0;	/* Not a location */
  admin[FOURTH_INSTANCE].location = THIRD_INSTANCE; /* In THIRD which is in SECOND which is at FIRST */
  assert_true(where(FOURTH_INSTANCE, DIRECT) == THIRD_INSTANCE);
  assert_true(where(FOURTH_INSTANCE, TRANSITIVE) == FIRST_INSTANCE);

  free(admin);
  free(instances);
  free(classes);
  free(header);
}


/*----------------------------------------------------------------------*/
Ensure(Exe, canGetMembersOfASet) {
  Set *set = newSet(0);
  Aword code[] = {0,	/* Dummy to not start at address 0 */
		  INSTRUCTION(I_SETSIZE),
		  INSTRUCTION(I_RETURN)};

  Stack stack = createStack(50);
  setInterpreterStack(stack);

  memory = code;
  memTop = 100;
  push(stack, toAptr(set));
  assert_equal(0, evaluate(1));
}


/*----------------------------------------------------------------------*/
Ensure(Exe, canGetContainerSize) {
  header = allocate(sizeof(ACodeHeader));
  instances = allocate(4*sizeof(InstanceEntry));
  admin = allocate(4*sizeof(AdminEntry));

  header->instanceMax = 3;
  instances[1].container = 1;
  admin[1].location = 0;
  admin[2].location = 1;
  admin[3].location = 2;

  assert_true(containerSize(1, DIRECT) == 1);
  assert_true(containerSize(1, TRANSITIVE) == 2);

  free(admin);
  free(instances);
  free(header);
}

/*----------------------------------------------------------------------*/
Ensure(Exe, canGenerateRandomInOrderIfRegressionTestOption) {
    regressionTestOption = TRUE;

    /* Should generate next value within interval, if exceeded, restart */
    assert_that(randomInteger(0, 1), is_equal_to(0)); /* First value in range */
    assert_that(randomInteger(0, 1), is_equal_to(1)); /* Second value in range */
    assert_that(randomInteger(5, 6), is_equal_to(5)); /* First value in range */
    assert_that(randomInteger(5, 6), is_equal_to(6)); /* Second value in range */
    assert_that(randomInteger(5, 6), is_equal_to(5)); /* First value in range */
    assert_that(randomInteger(1, 1), is_equal_to(1)); /* First value in range */
    assert_that(randomInteger(5, 8), is_equal_to(6)); /* Second value in range */
    assert_that(randomInteger(5, 8), is_equal_to(7)); /* Third value in range */
}
