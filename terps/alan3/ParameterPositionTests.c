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
        parameterPositions[i].endOfList = false;
        parameterPositions[i].explicitMultiple = false;
    }
    parameterPositions[7].explicitMultiple = true;
    parameterPositions[9].endOfList = true;

    assert_equal(findMultipleParameterPosition(parameterPositions), 7);
}


/*----------------------------------------------------------------------*/
Ensure(ParameterPosition, copyParameterPositionsCopyTheWholeListIncludingTheEndMarker) {
    ParameterPosition original[2];
    ParameterPosition copy[2];

    original[0].endOfList = false;
    original[0].all = true;
    original[0].explicitMultiple = false;
    original[1].endOfList = true;
    copy[0].endOfList = false;
    copy[1].endOfList = false;

    copyParameterPositions(original, copy);

    assert_equal(copy[0].all, original[0].all);
    assert_equal(copy[0].explicitMultiple, original[0].explicitMultiple);
    assert_true(copy[1].endOfList);
}


static ParameterPosition *givenAnyParameterPositionArrayOfLength(int length) {
    ParameterPosition *parameterPositionArray = allocate(sizeof(ParameterPosition)*(length+1));
    int i;
    for (i=0; i<length; i++)
        parameterPositionArray[i].endOfList = false;
    parameterPositionArray[length].endOfList = true;
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

    parameterPosition1[0].endOfList = false;
    parameterPosition1[0].parameters = allocate(sizeof(Parameter)*5);
    setEndOfArray(&parameterPosition1[0].parameters[5-1]);
    parameterPosition1[1].endOfList = true;

    parameterPosition2[0].endOfList = false;
    parameterPosition2[0].parameters = allocate(sizeof(Parameter)*4);
    setEndOfArray(&parameterPosition2[0].parameters[4-1]);
    parameterPosition2[1].endOfList = true;

    assert_false(equalParameterPositions(parameterPosition1, parameterPosition2));
}
