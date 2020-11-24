#include "cgreen/cgreen.h"
#include "cgreen/mocks.h"

#include "ParameterPosition.c"


/* Private Data */

static ACodeHeader acdHeader;

Describe(ParameterPosition);
BeforeEach(ParameterPosition){
    header = &acdHeader;
    header->maxParameters = 10;
}
AfterEach(ParameterPosition) {}


/*----------------------------------------------------------------------*/
Ensure(ParameterPosition, canUncheckAllParameterPositions) {
    int i;
    ParameterPosition *parameterPositions = allocate((MAXPARAMS+1)*sizeof(ParameterPosition));

    for (i = 0; i < MAXPARAMS; i++)
        parameterPositions[i].checked = i;;
    uncheckAllParameterPositions(parameterPositions);
    for (i = 0; i < MAXPARAMS; i++) {
        assert_false(parameterPositions[i].checked);
    }
}


/*----------------------------------------------------------------------*/
Ensure(ParameterPosition, canFindMultipleParameterPosition) {
    ParameterPosition parameterPositions[10];
    int i;

    for (i=0; i<10; i++) {
        parameterPositions[i].endOfList = FALSE;
        parameterPositions[i].explicitMultiple = FALSE;
    }
    parameterPositions[7].explicitMultiple = TRUE;
    parameterPositions[9].endOfList = TRUE;

    assert_equal(findMultipleParameterPosition(parameterPositions), 7);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterPosition, copyParameterPositionsCopyTheWholeListIncludingTheEndMarker) {
    ParameterPosition original[2];
    ParameterPosition copy[2];

    original[0].endOfList = FALSE;
    original[0].all = TRUE;
    original[0].explicitMultiple = FALSE;
    original[1].endOfList = TRUE;
    copy[0].endOfList = FALSE;
    copy[1].endOfList = FALSE;

    copyParameterPositions(original, copy);

    assert_equal(copy[0].all, original[0].all);
    assert_equal(copy[0].explicitMultiple, original[0].explicitMultiple);
    assert_true(copy[1].endOfList);
}


static ParameterPosition *givenAnyParameterPositionArrayOfLength(int length) {
    ParameterPosition *parameterPositionArray = allocate(sizeof(ParameterPosition)*(length+1));
    int i;
    for (i=0; i<length; i++)
        parameterPositionArray[i].endOfList = FALSE;
    parameterPositionArray[length].endOfList = TRUE;
    return parameterPositionArray;
}


/*----------------------------------------------------------------------*/
Ensure(ParameterPosition, parameterPositionsOfUnequalLengthAreNotEqual) {
    ParameterPosition *parameterPosition1 = givenAnyParameterPositionArrayOfLength(2);
    ParameterPosition *parameterPosition2 = givenAnyParameterPositionArrayOfLength(1);

    assert_false(equalParameterPositions(parameterPosition1, parameterPosition2));
}

/*----------------------------------------------------------------------*/
Ensure(ParameterPosition, parameterPositionsOfEqualLengthWithUnequalLengthParametersAreNotEqual) {
    ParameterPosition parameterPosition1[2];
    ParameterPosition parameterPosition2[2];

    parameterPosition1[0].endOfList = FALSE;
    parameterPosition1[0].parameters = allocate(sizeof(Parameter)*5);
    setEndOfArray(&parameterPosition1[0].parameters[5-1]);
    parameterPosition1[1].endOfList = TRUE;

    parameterPosition2[0].endOfList = FALSE;
    parameterPosition2[0].parameters = allocate(sizeof(Parameter)*4);
    setEndOfArray(&parameterPosition2[0].parameters[4-1]);
    parameterPosition2[1].endOfList = TRUE;

    assert_false(equalParameterPositions(parameterPosition1, parameterPosition2));
}

/*======================================================================*/
TestSuite *parameterPositionTests(void)
{
    TestSuite *suite = create_test_suite();

    add_test_with_context(suite, ParameterPosition, canUncheckAllParameterPositions);
    add_test_with_context(suite, ParameterPosition, canFindMultipleParameterPosition);
    add_test_with_context(suite, ParameterPosition, copyParameterPositionsCopyTheWholeListIncludingTheEndMarker);
    add_test_with_context(suite, ParameterPosition, parameterPositionsOfUnequalLengthAreNotEqual);
    add_test_with_context(suite, ParameterPosition, parameterPositionsOfEqualLengthWithUnequalLengthParametersAreNotEqual);

    return suite;
}
