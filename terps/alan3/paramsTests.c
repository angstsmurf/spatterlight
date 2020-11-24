#include "cgreen/cgreen.h"
#include "assert.h"

#ifndef __APPLE__
#include <malloc.h>
#endif

#include "params.c"

Describe(ParameterArray);
BeforeEach(ParameterArray) {
  header->maxParameters = 4;
  header->instanceMax = 8;
}
AfterEach(ParameterArray) {}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, canFindLastParameterInAList) {
    Parameter *parameters = newParameterArray();
    Parameter *parameter = newParameter(5);
    int i;

    for (i=0; i<5; i++)
        addParameterToParameterArray(parameters, parameter);

    assert_equal(&parameters[5], findEndOfParameterArray(parameters));
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, canSetAndGetParameters) {
    int numberOfParameters = 4;
    Parameter *parameters = newParameterArray();
    Parameter *parameter = newParameter(19);
    int p;

    int i;

    for (i = 0; i < numberOfParameters; i++)
        addParameterToParameterArray(parameters, parameter);

    assert_equal(lengthOfParameterArray(parameters), numberOfParameters);

    for (p = 0; p<numberOfParameters; p++)
        parameters[p].instance = p;

    setGlobalParameters(parameters);

    assert_equal(lengthOfParameterArray(getGlobalParameters()), lengthOfParameterArray(parameters));

    for (p = 0; !isEndOfArray(&parameters[p]); p++)
        assert_equal(getGlobalParameter(p)->instance, p);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, getWillAllocateStoredParameters) {
    assert_not_equal(getGlobalParameters(), NULL);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, can_find_multiple_position) {
    Parameter parameters[10];
    int i;

    for (i=0; i<10; i++)
        parameters[i].instance = 2;
    parameters[7].instance = 0;
    setEndOfArray(&parameters[9]);

    assert_equal(findMultiplePosition(parameters), 7);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, returns_minus_one_for_no_multiple_position) {
    Parameter parameters[10];
    int i;

    for (i=0; i<10; i++)
        parameters[i].instance = 2;
    setEndOfArray(&parameters[9]);

    assert_equal(findMultiplePosition(parameters), -1);
}


/*----------------------------------------------------------------------*/
static Parameter *givenAnyParameterArrayOfLength(int length) {
    Parameter *parameters = newParameterArray();
    assert(header->instanceMax > length);
    parameters->instance = 1;   /* To patch over the endOfArray indicator */
    setEndOfArray(&parameters[length]);
    return parameters;
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, unequal_length_parameter_arrays_are_not_equal) {
    Parameter *parameters1 = givenAnyParameterArrayOfLength(4);
    Parameter *parameters2 = givenAnyParameterArrayOfLength(5);

    assert_false(equalParameterArrays(parameters1, parameters2));
}

/*----------------------------------------------------------------------*/
Ensure(ParameterArray, subtractParameterArraysCanSubtractNullArray) {
    Parameter *parameters1 = givenAnyParameterArrayOfLength(4);
    Parameter *parameters2 = NULL;

    subtractParameterArrays(parameters1, parameters2);
    assert_equal(4, lengthOfParameterArray(parameters1));
}

/*----------------------------------------------------------------------*/
Ensure(ParameterArray, lengthOfParameterArrayReturnsZeroForNULLArray) {
    assert_that(lengthOfParameterArray(NULL), is_equal_to(0));
}

/*----------------------------------------------------------------------*/
Ensure(ParameterArray, lengthOfParameterArrayReturnsCorrectLength) {
    Parameter *parameters = newParameterArray();
    Parameter *parameter = newParameter(3);
    int i;

    for (i=0; i<3; i++)
        addParameterToParameterArray(parameters, parameter);
    assert_that(lengthOfParameterArray(parameters), is_equal_to(3));
}

/*----------------------------------------------------------------------*/
Ensure(ParameterArray, copyParameterCopiesCandidates) {
    Parameter *theOriginal = newParameter(2);
    Parameter *theCopy = newParameter(0);

    theOriginal->candidates = newParameterArray();
    addParameterToParameterArray(theOriginal->candidates, theCopy); /* NOTE Careful to not add the original ;-) */

    copyParameter(theCopy, theOriginal);

    assert_equal(theOriginal->instance, theCopy->instance);
    assert_true(equalParameterArrays(theOriginal->candidates, theCopy->candidates));
    assert_not_equal(theOriginal->candidates, theCopy->candidates);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, dontCrashOnCopyNullToNull) {
    copyParameterArray(NULL, NULL);
}

static bool syserrCalled = FALSE;
static void syserrHandler(char *message) {
    syserrCalled = TRUE;
}

/*----------------------------------------------------------------------*/
Ensure(ParameterArray, bailsOutOnCopyToNull) {
    ParameterArray theCopy = newParameterArray();

    setSyserrHandler(syserrHandler);
    copyParameterArray(NULL, theCopy);
    assert_that(syserrCalled);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, canCopyArrayToEmpty) {
    ParameterArray theOriginal = newParameterArray();
    ParameterArray theCopy = newParameterArray();
    Parameter *aParameter = newParameter(12);

    addParameterToParameterArray(theOriginal, aParameter);

    assert_that(lengthOfParameterArray(theCopy), is_equal_to(0));
    copyParameterArray(theCopy, theOriginal);
    assert_that(lengthOfParameterArray(theCopy), is_equal_to(lengthOfParameterArray(theOriginal)));
    assert_that(theCopy[0].instance, is_equal_to(aParameter->instance));
}



/*----------------------------------------------------------------------*/
Ensure(ParameterArray, addParameterSetsEndOfArray) {
    Parameter *parameters = newParameterArray();
    Parameter *parameter = newParameter(1);

    setEndOfArray(&parameters[0]);
    assert_true(lengthOfParameterArray(parameters) == 0);
    addParameterToParameterArray(parameters, parameter);
    assert_true(lengthOfParameterArray(parameters) == 1);
    addParameterToParameterArray(parameters, parameter);
    assert_true(lengthOfParameterArray(parameters) == 2);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, intersectParameterArraysReturnsAnEmptyResultForTwoEmpty) {
    Parameter *first = newParameterArray();
    Parameter *second = newParameterArray();

    intersectParameterArrays(first, second);

    assert_true(lengthOfParameterArray(first) == 0);
}


static Parameter *givenAParameterArrayWithOneParameter(Parameter *theParameter) {
    Parameter *theArray = newParameterArray();
    addParameterToParameterArray(theArray, theParameter);
    return theArray;
}

static Parameter *givenAParameterArrayWithTwoParameters(Parameter *theFirstParameter, Parameter *theSecondParameter) {
    Parameter *theArray = newParameterArray();
    addParameterToParameterArray(theArray, theFirstParameter);
    addParameterToParameterArray(theArray, theSecondParameter);
    return theArray;
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, intersectParameterArraysReturnsAnEmptyIfEitherIsEmpty) {
    Parameter *theParameter = newParameter(1);
    Parameter *oneParameterArray = givenAParameterArrayWithOneParameter(theParameter);
    Parameter *emptyParameterArray = newParameterArray();

    intersectParameterArrays(oneParameterArray, emptyParameterArray);

    assert_true(lengthOfParameterArray(oneParameterArray) == 0);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, intersectParameterArraysReturnsTheSameIfBothAreEqual) {
    Parameter *theParameter = newParameter(1);
    Parameter *first = givenAParameterArrayWithOneParameter(theParameter);
    Parameter *second = givenAParameterArrayWithOneParameter(theParameter);

    intersectParameterArrays(first, second);

    assert_true(equalParameterArrays(first, second));
}


/*----------------------------------------------------------------------*/
Ensure(ParameterArray, intersectParameterArraysReturnsTheCommonParameter) {
    Parameter *aParameter = newParameter(1);
    Parameter *anotherParameter = newParameter(2);
    Parameter *aThirdParameter = newParameter(3);
    Parameter *first = givenAParameterArrayWithTwoParameters(aParameter, anotherParameter);
    Parameter *second = givenAParameterArrayWithTwoParameters(anotherParameter, aThirdParameter);

    intersectParameterArrays(first, second);

    assert_equal(lengthOfParameterArray(first), 1);
}

/*----------------------------------------------------------------------*/
Ensure(ParameterArray, canCompactSparseArray)
{
    int initialLength = 7;
    int actual_member_count;
    Parameter *compacted = newParameterArray();
    Parameter *parameter = newParameter(14);
    int i;

    assert(initialLength < MAXINSTANCE);
    for (i = 0; i<initialLength; i++)
        addParameterToParameterArray(compacted, parameter);

    compacted[1].instance = 0;
    compacted[3].instance = 0;
    compacted[6].instance = 0;
    actual_member_count = initialLength - 3;

    compressParameterArray(compacted);

    assert_that(lengthOfParameterArray(compacted), is_equal_to(actual_member_count));
}

/*----------------------------------------------------------------------*/
xEnsure(ParameterArray, freesSubordinateParameterArrays) {
    Parameter *parameter = newParameter(7);
#ifndef __APPLE__
    struct mallinfo mallocinfo;
    size_t used = mallinfo().uordblks;
#endif

    Parameter *parameterArray = newParameterArray();
    addParameterToParameterArray(parameterArray, parameter);
    parameterArray[0].candidates = newParameterArray();

    freeParameterArray(parameterArray);

#ifndef __APPLE__
    mallocinfo = mallinfo();
    assert_that(mallocinfo.uordblks, is_equal_to(used));
#endif
}

/*----------------------------------------------------------------------*/
Ensure(ParameterArray, canCopyParameterWithAndWithoutCaniddates) {
    Parameter *original = newParameter(1);
    Parameter *copy;

    /* Basic test that the instance values are copied */
    copy = newParameter(0);
    copyParameter(copy, original);

    assert_that(copy->candidates, is_null);
    assert_that(copy->instance, is_equal_to(original->instance));


    /* Original has no candidates, copy has */
    copy->candidates = newParameterArray();
    original->candidates = NULL;

    copyParameter(copy, original);

    assert_that(copy->candidates, is_null);


    /* Original has candidates, copy hasn't */
    original->candidates = newParameterArray();
    copy->candidates = NULL;

    copyParameter(copy, original);

    assert_that(copy->candidates, is_non_null);
    assert_that(copy->candidates, is_not_equal_to(original->candidates));

    /* Original has candidates, copy has too */
    original->candidates = newParameterArray();
    copy->candidates = newParameterArray();

    copyParameter(copy, original);

    assert_that(copy->candidates, is_non_null);
    assert_that(copy->candidates, is_not_equal_to(original->candidates));
}
