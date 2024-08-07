/*----------------------------------------------------------------------

  params.c

  Various utility functions for handling parameters

  ----------------------------------------------------------------------*/
#include "params.h"

/* IMPORTS */
#include "lists.h"
#include "memory.h"
#include "literal.h"
#include "syserr.h"

/* PUBLIC DATA */
Parameter *globalParameters = NULL;



/*======================================================================*/
Parameter *newParameter(int id) {
    Parameter *parameter = NEW(Parameter);
    parameter->instance = id;
    parameter->candidates = NULL;

    return parameter;
}


/*======================================================================*/
Parameter *newParameterArray(void) {
    Parameter *newArray = allocate((MAXINSTANCE+1)*sizeof(Parameter));
    setEndOfArray(newArray);
    return newArray;
}


/*======================================================================*/
void freeParameterArray(ParameterArray arrayPointer) {
    Parameter *p;

    for (p = arrayPointer; !isEndOfArray(p); p++)
        if (p->candidates != NULL)
            freeParameterArray(p->candidates);
    deallocate(arrayPointer);
}


/*======================================================================*/
Parameter *ensureParameterArrayAllocated(ParameterArray currentArray) {
    if (currentArray == NULL)
        return newParameterArray();
    else {
        clearParameterArray(currentArray);
        return currentArray;
    }
}


/*======================================================================*/
bool parameterArrayIsEmpty(ParameterArray array) {
    return array == NULL || lengthOfParameterArray(array) == 0;
}


/*======================================================================*/
void clearParameter(Parameter *parameter) {
    Parameter *candidates = parameter->candidates;
    memset(parameter, 0, sizeof(Parameter));
    parameter->candidates = candidates;
    if (parameter->candidates != NULL)
        clearParameterArray(parameter->candidates);
}


/*======================================================================*/
void setGlobalParameters(ParameterArray newParameters) {
    if (globalParameters == NULL)
        globalParameters = newParameterArray();
    copyParameterArray(globalParameters, newParameters);
}


/*======================================================================*/
Parameter *getGlobalParameters(void) {
    if (globalParameters == NULL)
        globalParameters = newParameterArray();
    return globalParameters;
}


/*======================================================================*/
Parameter *getGlobalParameter(int parameterIndex) {
    return &globalParameters[parameterIndex];
}


/*======================================================================*/
Parameter *findEndOfParameterArray(Parameter *parameters) {
    Parameter *parameter;
    for (parameter = parameters; !isEndOfArray(parameter); parameter++);
    return parameter;
}


/*======================================================================*/
/* A parameter position with code == 0 means this is a multiple position.
 * We must loop over this position (and replace it by each present in the
 * matched list)
 */
int findMultiplePosition(Parameter parameters[]) {
    // TODO: this should look at the isAll and isExplicitMultiple flags instead
    int multiplePosition;
    for (multiplePosition = 0; !isEndOfArray(&parameters[multiplePosition]); multiplePosition++)
    if (parameters[multiplePosition].instance == 0)
        return multiplePosition;
    return -1;
}


/*======================================================================*/
void compressParameterArray(Parameter theArray[])
{
    int i, j;

    for (i = 0, j = 0; !isEndOfArray(&theArray[j]); j++)
        if (theArray[j].instance != 0)
            theArray[i++] = theArray[j];
    setEndOfArray(&theArray[i]);
}


/*======================================================================*/
int lengthOfParameterArray(Parameter theArray[])
{
    int i = 0;

    if (theArray == NULL) return 0;

    while (!isEndOfArray(&theArray[i]))
        i++;
    return i;
}


/*======================================================================*/
bool equalParameterArrays(Parameter parameters1[], Parameter parameters2[])
{
    int i;

    if ((parameters1 == NULL) != (parameters2 == NULL))
        return false;
    if (parameters1 == NULL) // Because then parameter2 is also NULL
        return true;
    for (i = 0; !isEndOfArray(&parameters1[i]); i++) {
        if (isEndOfArray(&parameters2[i])) return false;
        if (parameters1[i].instance != parameters2[i].instance) return false;
    }
    return isEndOfArray(&parameters2[i]);
}


/*======================================================================*/
bool inParameterArray(Parameter theArray[], Aword theCode)
{
    int i;

    for (i = 0; !isEndOfArray(&theArray[i]) && theArray[i].instance != theCode; i++);
    return (theArray[i].instance == theCode);
}


/*======================================================================*/
void copyParameter(Parameter *to, Parameter *from) {
    Parameter *toCandidates = to->candidates;

    *to = *from;
    if (from->candidates != NULL) {
        if (toCandidates == NULL)
            to->candidates = newParameterArray();
        else
            to->candidates = toCandidates;
        copyParameterArray(to->candidates, from->candidates);
    } else if (toCandidates != NULL)
        freeParameterArray(toCandidates);
}


/*======================================================================*/
void addParameterToParameterArray(ParameterArray theArray, Parameter *theParameter)
{
    if (theArray == NULL) syserr("Adding to null parameter array");

    int i;

    for (i = 0; !isEndOfArray(&theArray[i]) && i < MAXINSTANCE; i++)
        ;
    if (isEndOfArray(&theArray[i])) {
        copyParameter(&theArray[i], theParameter);
        setEndOfArray(&theArray[i+1]);
    } else
        syserr("Couldn't find end of ParameterArray");
}


/*======================================================================*/
void copyParameterArray(ParameterArray to, ParameterArray from)
{
    int i;

    if (to == NULL && from == NULL) return;

    if (to == NULL)
        syserr("Copying to null parameter array");
    else {
        clearParameterArray(to);
        for (i = 0; !isEndOfArray(&from[i]); i++)
            addParameterToParameterArray(to, &from[i]);
    }
}


/*======================================================================*/
void subtractParameterArrays(Parameter theArray[], Parameter remove[])
{
    int i;

    if (remove == NULL) return;

    for (i = 0; !isEndOfArray(&theArray[i]); i++)
        if (inParameterArray(remove, theArray[i].instance))
            theArray[i].instance = 0;		/* Mark empty */
    compressParameterArray(theArray);
}


/*======================================================================*/
void clearParameterArray(Parameter theArray[]) {
    Parameter *p = NULL;

    for (p = &theArray[0]; !isEndOfArray(p); p++)
        clearParameter(p);
    setEndOfArray(theArray);
}


/*======================================================================*/
void intersectParameterArrays(Parameter one[], Parameter other[])
{
    int i, last = 0;


    for (i = 0; !isEndOfArray(&one[i]); i++)
        if (inParameterArray(other, one[i].instance))
            one[last++] = one[i];
    setEndOfArray(&one[last]);
}


/*======================================================================*/
void copyReferencesToParameterArray(Aint references[], Parameter parameterArray[])
{
    int i;

    for (i = 0; !isEndOfArray(&references[i]); i++) {
        parameterArray[i].instance = references[i];
        parameterArray[i].firstWord = EOF; /* Ensure that there is no word that can be used */
    }
    setEndOfArray(&parameterArray[i]);
}


/*======================================================================*/
void addParameterForInstance(Parameter *parameters, int instance) {
    Parameter *parameter = findEndOfParameterArray(parameters);

    parameter->instance = instance;
    parameter->useWords = false;

    setEndOfArray(parameter+1);
}


/*======================================================================*/
void addParameterForInteger(ParameterArray parameters, int value) {
    Parameter *parameter = findEndOfParameterArray(parameters);

    createIntegerLiteral(value);
    parameter->instance = instanceFromLiteral(litCount);
    parameter->useWords = false;

    setEndOfArray(parameter+1);
}

/*======================================================================*/
void addParameterForString(Parameter *parameters, char *value) {
    Parameter *parameter = findEndOfParameterArray(parameters);

    createStringLiteral(value);
    parameter->instance = instanceFromLiteral(litCount);
    parameter->useWords = false;

    setEndOfArray(parameter+1);
}

/*======================================================================*/
void printParameterArray(Parameter parameters[]) {
    int i;
    printf("[");
    for (i = 0; !isEndOfArray(&parameters[i]); i++) {
    printf("%d ", (int)parameters[i].instance);
    }
    printf("]\n");
}
