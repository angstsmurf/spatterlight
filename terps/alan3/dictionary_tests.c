#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>

#include "dictionary.h"

#include "memory.h"

#include "lists.h"

/* Mocked modules */
#include "inter.mock"
#include "output.mock"
#include "compatibility.mock"
#include "debug.mock"
#include "utils.mock"
#include "current.mock"
#include "syserr.mock"
#include "args.mock"
#include "instance.mock"
#include "decode.mock"
#include "params.mock"
#include "msg.mock"
#include "score.mock"
#include "state.mock"
#include "term.mock"
#include "readline.mock"
#include "save.mock"
#include "word.mock"
#include "event.mock"
#include "actor.mock"

#include "types.h"

#define MAX_NO_OF_PRONOUN_REFERENCES 3
#define MAX_NO_OF_PRONOUNS 5

static Aword *allocate_memory() {
    int sizeOfPronounList = MAX_NO_OF_PRONOUN_REFERENCES*MAX_NO_OF_PRONOUNS;
    return (Aword *)allocate(sizeOfPronounList);
}


Describe(Dictionary);

BeforeEach(Dictionary) {
    memory = allocate_memory();
}

AfterEach(Dictionary) {
    free(memory);
}

Ensure(Dictionary, no_tests_yet) {
}
