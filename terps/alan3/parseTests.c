#include "cgreen/cgreen.h"
#include "cgreen/mocks.h"

#include "parse.c"

#include "literal.h"

static ACodeHeader acdHeader;

static DictionaryEntry *makeDictionary(void);
static void setUp(void) {
    header = &acdHeader;
    header->maxParameters = 10;
    header->instanceMax = 10;

    memory = allocate(100*sizeof(Aword));

    dictionary = makeDictionary();
}

Describe(Parse);
BeforeEach(Parse) {
    setUp();
}
AfterEach(Parse) {}

/*----------------------------------------------------------------------*/
static void makeEOS(ElementEntry *element) {
    element->code = EOS;
}

static void makeParameterElement(ElementEntry *element) {
    element->code = 0;
}

static void makeWordElement(ElementEntry *element, int code, int next) {
    element->code = code;
    element->next = next;
}


#define DICTIONARY_SIZE 25
#define INSTANCE1_CODE 1
#define INSTANCE1_ADJECTIVE1_DICTIONARY_INDEX 3
#define INSTANCE1_ADJECTIVE2_DICTIONARY_INDEX 11
#define INSTANCE1_NOUN1_DICTIONARY_INDEX 5
#define CONJUNCTION_DICTIONARY_INDEX 21
#define ALL_DICTIONARY_INDEX 13
#define PREPOSITION_CODE 1


static DictionaryEntry *makeDictionary() {
    DictionaryEntry *dictionary = allocate(DICTIONARY_SIZE*sizeof(DictionaryEntry));
    dictionarySize = DICTIONARY_SIZE;
    return dictionary;
}

static void makeDictionaryEntry(int index, int code, int classBits) {
    if (index > dictionarySize)
        syserr("makeDictionaryEntry() outside size of dictionary");
    dictionary[index].code = code;
    dictionary[index].classBits = classBits;
    dictionary[index].string = ((char *)&"all"-(char *)&memory[0])/sizeof(Aword);
}

static void given_AStandardDictionary() {
    dictionary = makeDictionary();
    makeDictionaryEntry(INSTANCE1_ADJECTIVE1_DICTIONARY_INDEX, INSTANCE1_CODE, ADJECTIVE_BIT);
    makeDictionaryEntry(INSTANCE1_ADJECTIVE2_DICTIONARY_INDEX, INSTANCE1_CODE, ADJECTIVE_BIT);
    makeDictionaryEntry(INSTANCE1_NOUN1_DICTIONARY_INDEX, INSTANCE1_CODE, NOUN_BIT);
    makeDictionaryEntry(ALL_DICTIONARY_INDEX, 1, ALL_BIT);
    makeDictionaryEntry(CONJUNCTION_DICTIONARY_INDEX, 1, CONJUNCTION_BIT);
}

static void given_PlayerWordsForANoun(int startAtIndex) {
    currentWordIndex = startAtIndex;
    ensureSpaceForPlayerWords(startAtIndex);
    playerWords[startAtIndex].code = INSTANCE1_ADJECTIVE1_DICTIONARY_INDEX;
    playerWords[startAtIndex+1].code = EOF;
}

static void given_PlayerWordsWithTwoAdjectivesAndANoun(int startAtIndex) {
    currentWordIndex = startAtIndex;
    ensureSpaceForPlayerWords(startAtIndex+2);
    playerWords[startAtIndex].code = INSTANCE1_ADJECTIVE1_DICTIONARY_INDEX;
    playerWords[startAtIndex+1].code = INSTANCE1_ADJECTIVE2_DICTIONARY_INDEX;
    playerWords[startAtIndex+2].code = INSTANCE1_NOUN1_DICTIONARY_INDEX;
    playerWords[startAtIndex+3].code = EOF;
}

static void given_PlayerWordsForTwoNounOnlyParameters(int startAtIndex) {
    currentWordIndex = startAtIndex;
    ensureSpaceForPlayerWords(startAtIndex+2);
    playerWords[startAtIndex].code = INSTANCE1_NOUN1_DICTIONARY_INDEX;
    playerWords[startAtIndex+1].code = CONJUNCTION_DICTIONARY_INDEX;
    playerWords[startAtIndex+2].code = INSTANCE1_NOUN1_DICTIONARY_INDEX;
    playerWords[startAtIndex+3].code = EOF;
}


/*----------------------------------------------------------------------*/
static void given_NoPlayerWords(void) {
    ensureSpaceForPlayerWords(0);
    playerWords[0].code = EOF;
    currentWordIndex = 0;
}


/*----------------------------------------------------------------------*/
static void given_AnEmptyParseTree(ElementEntry *elementTable) {
    setEndOfArray(elementTable);
}


/*----------------------------------------------------------------------*/
static void given_AParseTreeWithOnlyEos(ElementEntry *elementTable) {
    makeEOS(&elementTable[0]);
    setEndOfArray(&elementTable[1]);
}

/*----------------------------------------------------------------------*/
Ensure(Parse, canMatchEndOfSyntax) {
    ElementEntry *element;
    ElementEntry *elementTable;

    elementTable = (ElementEntry *)&memory[20];

    given_NoPlayerWords();

    given_AnEmptyParseTree(elementTable);
    element = elementForEndOfSyntax(elementTable);
    assert_equal(NULL, element);

    given_AParseTreeWithOnlyEos(elementTable);
    element = elementForEndOfSyntax(elementTable);
    assert_not_equal(NULL, element);
    assert_equal(EOS, element->code);
}


/*----------------------------------------------------------------------*/
static void given_AParseTreeWithOnlyParameter(ElementEntry *elementTable) {
    makeParameterElement(&elementTable[0]);
    setEndOfArray(&elementTable[1]);
}


/*----------------------------------------------------------------------*/
static void given_AParameterTreeWithEosAndParameter(ElementEntry *elementTable) {
    makeEOS(&elementTable[0]);
    makeParameterElement(&elementTable[1]);
    setEndOfArray(&elementTable[2]);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, canMatchParameterElement) {
    ElementEntry *element;
    ElementEntry *elementTable;

    elementTable = (ElementEntry *)&memory[50];

    given_AnEmptyParseTree(elementTable);
    element = elementForParameter(elementTable);
    assert_equal(NULL, element);

    given_AParseTreeWithOnlyParameter(elementTable);
    element = elementForParameter(elementTable);
    assert_not_equal(NULL, element);
    assert_equal(0, element->code);

    given_AParameterTreeWithEosAndParameter(elementTable);
    element = elementForParameter(elementTable);
    assert_not_equal(NULL, element);
    assert_equal(0, element->code);
}


/*----------------------------------------------------------------------*/
static void given_AParseTreeWithWordAndEos(ElementEntry *elementTable) {
    makeWordElement(&elementTable[0], 1, 0);
    makeEOS(&elementTable[1]);
    setEndOfArray(&elementTable[2]);
}


/*----------------------------------------------------------------------*/
static void given_PlayerInputOfAPreposition() {
    playerWords[0].code = PREPOSITION_CODE;
    playerWords[1].code = EOF;
}


/*----------------------------------------------------------------------*/
static void given_AParseTreeAllowingWordFollowedByEos(ElementEntry *elementTable) {
    makeWordElement(&elementTable[0], 1, addressOf(&elementTable[1]));
    makeEOS(&elementTable[1]);
    setEndOfArray(&elementTable[2]);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, canParseInputAccordingToParseTree) {

    int ELEMENT_TABLE_ADDRESS = 50;
    ElementEntry *element;
    ElementEntry *elementTable;
    elementTable = (ElementEntry *)&memory[ELEMENT_TABLE_ADDRESS];

    ParameterPosition *parameterPositions = NEW(ParameterPosition);

    SyntaxEntry stx;
    stx.elms = ELEMENT_TABLE_ADDRESS;

    given_NoPlayerWords();

    given_AnEmptyParseTree(elementTable);
    element = parseInputAccordingToSyntax(&stx, parameterPositions);
    assert_equal(NULL, element);

    given_AParseTreeWithOnlyEos(elementTable);
    element = parseInputAccordingToSyntax(&stx, parameterPositions);
    assert_equal(elementTable, element);

    given_AParseTreeWithWordAndEos(elementTable);
    element = parseInputAccordingToSyntax(&stx, parameterPositions);
    assert_equal(&elementTable[1], element);

    makeDictionaryEntry(PREPOSITION_CODE, 1, PREPOSITION_BIT);
    given_PlayerInputOfAPreposition();
    given_AParseTreeAllowingWordFollowedByEos(elementTable);
    element = parseInputAccordingToSyntax(&stx, parameterPositions);
    assert_equal(&elementTable[1], element);
}

/*----------------------------------------------------------------------*/
Ensure(Parse, canSetupParameterForWord) {
    Parameter *messageParameters;

    dictionary = makeDictionary();

    makeDictionaryEntry(2, 23, VERB_BIT);
    memcpy(&memory[12], "qwerty", 7);
    dictionary[2].string = 12;

    messageParameters = newParameterArray();
    ensureSpaceForPlayerWords(2);
    playerWords[1].code = 2;
    litCount = 0;
    addParameterForWord(messageParameters, 1);

    assert_equal(instanceFromLiteral(1), messageParameters[0].instance);
    assert_equal(true, messageParameters[0].useWords);
    assert_equal(1, messageParameters[0].firstWord);
    assert_equal(1, messageParameters[0].lastWord);

    assert_true(isEndOfArray(&messageParameters[1]));

    free(dictionary);
    free(messageParameters);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, canSeeBitsInFlag) {
    assert_true(hasBit(-1, OMNIBIT));
    assert_false(hasBit(0, OMNIBIT));
    assert_true(hasBit(-1, MULTIPLEBIT));
    assert_false(hasBit(0, MULTIPLEBIT));
}


/*----------------------------------------------------------------------*/
Ensure(Parse, canSetupInstanceParametersForMessages) {
    Parameter *parameters = newParameterArray();

    addParameterForInstance(parameters, 2);

    assert_false(isLiteral(parameters[0].instance));
    assert_equal(parameters[0].instance, 2);
    assert_true(isEndOfArray(&parameters[1]));

    free(parameters);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, canSetupStringParametersForMessages) {
    Parameter *parameters = newParameterArray();

    addParameterForString(parameters, "a string");

    assert_true(isLiteral(parameters[0].instance));
    assert_string_equal(fromAptr(literals[literalFromInstance(parameters[0].instance)].value), "a string");
    assert_true(isEndOfArray(&parameters[1]));

    free(parameters);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, canSetupIntegerParametersForMessages) {
    Parameter *parameters = newParameterArray();

    addParameterForInteger(parameters, 14);

    assert_true(isLiteral(parameters[0].instance));
    assert_equal(literals[literalFromInstance(parameters[0].instance)].value, 14);
    assert_true(isEndOfArray(&parameters[1]));

    free(parameters);
}


static void mockedInstanceMatcher(Parameter *parameter) {
    mock(parameter->firstWord, parameter->lastWord);
    parameter->candidates[0].instance = 17;
    parameter->candidates[1].instance = -1;
}

/*----------------------------------------------------------------------*/
Ensure(Parse, canMatchSingleParameter) {
    Parameter *parameters = newParameterArray();
    Parameter *parameter = newParameter(0);

    parameters->firstWord = 1;
    parameters->lastWord = 1;
    addParameterToParameterArray(parameters, parameter);

    playerWords = allocate(10);

    expect(mockedInstanceMatcher,
           when(parameter->firstWord, is_equal_to(parameters[0].firstWord)),
           when(parameter->lastWord, is_equal_to(parameters[0].lastWord)));

    findCandidates(parameters, mockedInstanceMatcher);

    assert_not_equal(parameters[0].candidates, NULL);
    assert_equal(lengthOfParameterArray(parameters[0].candidates), 1);
    assert_equal(parameters[0].candidates[0].instance, 17);
}




static Aint *mockedReferenceFinder(int wordIndex) {
    return (Aint *)mock(wordIndex);
}

/*----------------------------------------------------------------------*/
Ensure(Parse, matchNounPhraseCanMatchSingleNounWithSingleMatch) {
    int theExpectedInstance[2] = {23, EOF};
    int theExpectedWordIndex = 3;
    Parameter *parameter = newParameter(0);

    parameter->firstWord = parameter->lastWord = 3;
    parameter->candidates = newParameterArray();

    given_PlayerWordsForANoun(theExpectedWordIndex);

    expect(mockedReferenceFinder,
       when(wordIndex, is_equal_to(theExpectedWordIndex)),
       will_return(theExpectedInstance));

    matchNounPhrase(parameter, mockedReferenceFinder, mockedReferenceFinder);

    assert_not_equal(parameter->candidates, NULL);
    assert_equal(lengthOfParameterArray(parameter->candidates), 1);
    assert_equal(parameter->candidates[0].instance, theExpectedInstance[0]);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, canMatchNounAndAdjectiveWithSingleMatch) {
    int theExpectedInstance = 55;
    int firstAdjectiveInstances[4] = {23, theExpectedInstance, 33, EOF};
    int theNounInstances[4] = {25, theExpectedInstance, 34, EOF};
    int theExpectedFirstAdjectiveWordIndex = 3;
    int theExpectedNounWordIndex = 4;
    Parameter *parameter = newParameter(0);

    parameter->firstWord = theExpectedFirstAdjectiveWordIndex;
    parameter->lastWord = theExpectedNounWordIndex;
    parameter->candidates = newParameterArray();

    given_AStandardDictionary();

    expect(mockedReferenceFinder,
       when(wordIndex, is_equal_to(theExpectedFirstAdjectiveWordIndex)),
       will_return(firstAdjectiveInstances));
    expect(mockedReferenceFinder,
       when(wordIndex, is_equal_to(theExpectedNounWordIndex)),
       will_return(theNounInstances));

    matchNounPhrase(parameter, mockedReferenceFinder, mockedReferenceFinder);

    assert_not_equal(parameter->candidates, NULL);
    assert_equal(lengthOfParameterArray(parameter->candidates), 1);
    assert_equal(parameter->candidates[0].instance, theExpectedInstance);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, canMatchMultipleAdjectivesAndNounWithSingleMatch) {
    int theExpectedInstance = 55;
    int firstAdjectiveInstances[4] = {23, theExpectedInstance, 33, EOF};
    int secondAdjectiveInstances[4] = {24, theExpectedInstance, 33, EOF};
    int theNounInstances[4] = {25, theExpectedInstance, 34, EOF};
    int theExpectedFirstAdjectiveWordIndex = 3;
    int theExpectedSecondAdjectiveWordIndex = 4;
    int theExpectedNounWordIndex = 5;
    Parameter *parameters = newParameterArray();
    Parameter *parameter = newParameter(0);

    parameter->firstWord = theExpectedFirstAdjectiveWordIndex;
    parameter->lastWord = theExpectedNounWordIndex;
    parameter->candidates = newParameterArray();
    addParameterToParameterArray(parameters, parameter);

    given_PlayerWordsWithTwoAdjectivesAndANoun(theExpectedFirstAdjectiveWordIndex);

    given_AStandardDictionary();

    expect(mockedReferenceFinder,
       when(wordIndex, is_equal_to(theExpectedFirstAdjectiveWordIndex)),
       will_return(firstAdjectiveInstances));
    expect(mockedReferenceFinder,
       when(wordIndex, is_equal_to(theExpectedSecondAdjectiveWordIndex)),
       will_return(secondAdjectiveInstances));
    expect(mockedReferenceFinder,
       when(wordIndex, is_equal_to(theExpectedNounWordIndex)),
       will_return(theNounInstances));

    matchNounPhrase(parameters, mockedReferenceFinder, mockedReferenceFinder);

    assert_not_equal(parameters[0].candidates, NULL);
    assert_equal(lengthOfParameterArray(parameters[0].candidates), 1);
    assert_equal(parameters[0].candidates[0].instance, theExpectedInstance);
}

void mockedAllBuilder(Parameter candidates[])
{
    mock(candidates);
    candidates[0].instance = 1;
    setEndOfArray(&candidates[1]);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, anyAllFindsAnyAllIndication) {
    ParameterPosition *parameterPositions = allocate(5*sizeof(ParameterPosition));

    parameterPositions[0].endOfList = false;
    parameterPositions[1].endOfList = false;
    parameterPositions[2].endOfList = false;
    parameterPositions[3].endOfList = true;

    parameterPositions[0].all = false;
    parameterPositions[1].all = false;
    parameterPositions[2].all = false;
    parameterPositions[3].all = false;
    parameterPositions[4].all = true;

    assert_false(anyAll(parameterPositions));

    parameterPositions[1].all = true;

    assert_true(anyAll(parameterPositions));
}


/*----------------------------------------------------------------------*/
Ensure(Parse, anyAllFindsExplicitMultipleIndication) {
    ParameterPosition *parameterPositions = allocate(5*sizeof(ParameterPosition));

    parameterPositions[0].endOfList = false;
    parameterPositions[1].endOfList = false;
    parameterPositions[2].endOfList = false;
    parameterPositions[3].endOfList = true;

    parameterPositions[0].explicitMultiple = false;
    parameterPositions[1].explicitMultiple = false;
    parameterPositions[2].explicitMultiple = false;
    parameterPositions[3].explicitMultiple = false;
    parameterPositions[4].explicitMultiple = true;

    assert_false(anyExplicitMultiple(parameterPositions));

    parameterPositions[1].explicitMultiple = true;

    assert_true(anyExplicitMultiple(parameterPositions));
}

/*----------------------------------------------------------------------*/
Ensure(Parse, parsePronounSetsPronounMarker) {
    Parameter *parameters = newParameterArray();
    Parameter *parameter = newParameter(0);

    addParameterToParameterArray(parameters, parameter);
    parameter->isPronoun = false;

    parsePronoun(parameters);

    assert_true(parameters[0].isPronoun);
}

/*----------------------------------------------------------------------*/
Ensure(Parse, parseLiteralSetsLiteralMarker) {
    Parameter parameter[2];

    setEndOfArray(&parameter[0]);
    parameter[0].isLiteral = false;

    parseLiteral(&parameter[0]);

    assert_true(parameter[0].isLiteral);
    assert_false(isEndOfArray(&parameter[0]));
    assert_true(isEndOfArray(&parameter[1]));
}

/*----------------------------------------------------------------------*/
Ensure(Parse, parseReferenceToPreviousMultipleParameterSetsThemMarker) {
    Parameter parameter[2];

    setEndOfArray(&parameter[0]);
    parameter[0].isThem = false;

    parseReferenceToPreviousMultipleParameters(&parameter[0]);

    assert_true(parameter[0].isThem);
    assert_false(isEndOfArray(&parameter[0]));
    assert_true(isEndOfArray(&parameter[1]));
}

/*----------------------------------------------------------------------*/
Ensure(Parse, simpleParameterParserCanParseExplicitMultiple) {
    Parameter *parameters = newParameterArray();

    given_AStandardDictionary();
    given_PlayerWordsForTwoNounOnlyParameters(0);

    simpleParameterParser(parameters);

    assert_equal(lengthOfParameterArray(parameters), 2);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, getPreviousMultipleParametersGetsACopy) {
    Parameter *parameters = newParameterArray();
    Parameter *parameter = newParameter(5);

    previousMultipleParameters = newParameterArray();

    addParameterToParameterArray(parameters, parameter);

    getPreviousMultipleParameters(parameters);
    assert_that(equalParameterArrays(parameters, previousMultipleParameters));
}

/*----------------------------------------------------------------------*/
Ensure(Parse, parseAdjectivesAndNounsReturnsEmptyParametersOnEndOfInput) {
    Parameter parameters[2];
    given_NoPlayerWords();

    parseAdjectivesAndNoun(parameters);
    assert_equal(lengthOfParameterArray(parameters), 0);
}


// TODO: Generalise this to replace lengthOfParameterArray() and other loops
static int lengthOfPronounArray(Pronoun *array, int elementSize) {
    int length;
    for (length = 0; !isEndOfArray(&array[length]); length++)
        ;
    return length;
}

/*----------------------------------------------------------------------*/
Ensure(Parse, addPronounForInstanceDontAddSameTwice) {
    pronouns = allocate(3*sizeof(Pronoun));

    pronouns[0].pronoun = 10;
    pronouns[0].instance = 3;
    setEndOfArray(&pronouns[1]);

    assert_equal(lengthOfPronounArray(pronouns, sizeof(Pronoun)), 1);
    addPronounForInstance(7, 3);
    assert_equal(lengthOfPronounArray(pronouns, sizeof(Pronoun)), 2);
    addPronounForInstance(10, 3);
    assert_equal(lengthOfPronounArray(pronouns, sizeof(Pronoun)), 2);
}




static bool mockedReachable(int instance) {
    return instance == 1;
}

static bool handlerFor00NCalled = false;
static bool handlerFor01NCalled = false;
static bool handlerFor0MNCalled = false;
static bool handlerFor10NCalled = false;
static bool handlerFor11NCalled = false;
static bool handlerFor1MNCalled = false;
static bool handlerForM0NCalled = false;
static bool handlerForM1NCalled = false;
static bool handlerForMMNCalled = false;
static bool handlerFor00YCalled = false;
static bool handlerFor01YCalled = false;
static bool handlerFor0MYCalled = false;
static bool handlerFor10YCalled = false;
static bool handlerFor11YCalled = false;
static bool handlerFor1MYCalled = false;
static bool handlerForM0YCalled = false;
static bool handlerForM1YCalled = false;
static bool handlerForMMYCalled = false;

static Parameter *mocked00NHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor00NCalled = true; return allCandidates; }
static Parameter *mocked01NHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor01NCalled = true; return allCandidates; }
static Parameter *mocked0MNHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor0MNCalled = true; return allCandidates; }
static Parameter *mocked10NHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor10NCalled = true; return allCandidates; }
static Parameter *mocked11NHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor11NCalled = true; return allCandidates; }
static Parameter *mocked1MNHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor1MNCalled = true; return allCandidates; }
static Parameter *mockedM0NHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerForM0NCalled = true; return allCandidates; }
static Parameter *mockedM1NHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerForM1NCalled = true; return allCandidates; }
static Parameter *mockedMMNHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerForMMNCalled = true; return allCandidates; }
static Parameter *mocked00YHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor00YCalled = true; return allCandidates; }
static Parameter *mocked01YHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor01YCalled = true; return allCandidates; }
static Parameter *mocked0MYHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor0MYCalled = true; return allCandidates; }
static Parameter *mocked10YHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor10YCalled = true; return allCandidates; }
static Parameter *mocked11YHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor11YCalled = true; return allCandidates; }
static Parameter *mocked1MYHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerFor1MYCalled = true; return allCandidates; }
static Parameter *mockedM0YHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerForM0YCalled = true; return allCandidates; }
static Parameter *mockedM1YHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerForM1YCalled = true; return allCandidates; }
static Parameter *mockedMMYHandler(Parameter allCandidates[], Parameter presentCandidates[]) { handlerForMMYCalled = true; return allCandidates; }

static DisambiguationHandlerTable mockedHandlerTable =
    {
        {   // Present == 0
            {   // Distant == 0
                mocked00NHandler, mocked00YHandler},
            {   // Distant == 1
                mocked01NHandler, mocked01YHandler},
            {   // Distant == M
                mocked0MNHandler, mocked0MYHandler}},
        {   //  Present == 1
            {   // Distant == 0
                mocked10NHandler, mocked10YHandler},
            {   // Distant == 1
                mocked11NHandler, mocked11YHandler},
            {   // Distant == M
                mocked1MNHandler, mocked1MYHandler}},
        {   // Present == M
            {   // Distant == 0
                mockedM0NHandler, mockedM0YHandler},
            {   // Distant == 1
                mockedM1NHandler, mockedM1YHandler},
            {   // Distant == M
                mockedMMNHandler, mockedMMYHandler}}
    };


/*----------------------------------------------------------------------*/
Ensure(Parse, disambiguateCandidatesCanCall00NHandler) {
    Parameter *candidates = newParameterArray();
    setEndOfArray(&candidates[0]); /* == 0 instance */

    disambiguateCandidates(candidates, false, mockedReachable, mockedHandlerTable);
    assert_true(handlerFor00NCalled);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, disambiguateCandidatesCanCall00YHandler) {
    Parameter *candidates = newParameterArray();
    setEndOfArray(&candidates[0]); /* == 0 instance */

    disambiguateCandidates(candidates, true, mockedReachable, mockedHandlerTable);
    assert_true(handlerFor00YCalled);
}


/*----------------------------------------------------------------------*/
Ensure(Parse, disambiguateCandidatesCanCall01NHandler) {
    Parameter *candidates = newParameterArray();
    candidates[0].instance = 2; /* 1 non-present */
    setEndOfArray(&candidates[1]); /* == 1 instance */

    disambiguateCandidates(candidates, false, mockedReachable, mockedHandlerTable);
    assert_true(handlerFor01NCalled);
}

/*----------------------------------------------------------------------*/
Ensure(Parse, disambiguateCandidatesCanCall0MNHandler) {
    Parameter *candidates = newParameterArray();
    candidates[0].instance = 2; /* M non-present */
    candidates[1].instance = 2;
    setEndOfArray(&candidates[2]); /* == 2 instances */

    disambiguateCandidates(candidates, false, mockedReachable, mockedHandlerTable);
    assert_true(handlerFor0MNCalled);
}

/*----------------------------------------------------------------------*/
Ensure(Parse, disambiguateCandidatesCanCall10NHandler) {
    Parameter *candidates = newParameterArray();
    candidates[0].instance = 1; /* 1 present */
    setEndOfArray(&candidates[1]); /* == 1 instance */

    disambiguateCandidates(candidates, false, mockedReachable, mockedHandlerTable);
    assert_true(handlerFor10NCalled);
}

/*----------------------------------------------------------------------*/
Ensure(Parse, disambiguateCandidatesCanCall11NHandler) {
    Parameter *candidates = newParameterArray();
    candidates[0].instance = 1; /* 1 present */
    candidates[1].instance = 2; /* 1 non-present */
    setEndOfArray(&candidates[2]); /* == 2 instances */

    disambiguateCandidates(candidates, false, mockedReachable, mockedHandlerTable);
    assert_true(handlerFor11NCalled);
}

/*----------------------------------------------------------------------*/
Ensure(Parse, disambiguateCandidatesCanCall1MNHandler) {
    Parameter *candidates = newParameterArray();
    candidates[0].instance = 1; /* 1 present */
    candidates[1].instance = 2; /* M non-present */
    candidates[2].instance = 2;
    setEndOfArray(&candidates[3]); /* == 3 instances */

    disambiguateCandidates(candidates, false, mockedReachable, mockedHandlerTable);
    assert_true(handlerFor1MNCalled);
}
