#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>

#include "exe.h"

#include "memory.h"


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



#define MAX_NO_OF_INSTANCES 5

static Aword *allocate_memory() {
    return (Aword *)allocate(sizeof(ACodeHeader)+(MAX_NO_OF_INSTANCES+1)*sizeof(InstanceEntry));
}


Describe(Executor);

BeforeEach(Executor) {
    memory = allocate_memory();
}

AfterEach(Executor) {
    free(memory);
}


Ensure(Executor, can_handle_legacy_directly_containment) {
}
