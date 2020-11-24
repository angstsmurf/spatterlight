#include "cgreen/cgreen.h"
#include "cgreen/mocks.h"


#include "instance.c"

Describe(Instance);

BeforeEach(Instance) {}
AfterEach(Instance) {}

static ACodeHeader localHeader;

void setUp() {
    header = &localHeader;
    literals = NULL;
}


void tearDown() {
    free(literals);
}


static void given_a_literal_table_of(int size) {
    literals = allocate(sizeof(LiteralEntry)*(size+1));
}

static void given_that_literal_has_value(int literal_number, int value) {
    literals[literal_number].value = value;
}


Ensure(Instance, canAccessLiteralValue) {
    int value = 45;
    int literal_number = 1;
    
    int attribute_number_for_literal_value = 0;

    given_a_literal_table_of(1);
    given_that_literal_has_value(1, value);

    assert_equal(value, literalAttribute(instanceFromLiteral(literal_number), attribute_number_for_literal_value));
}

static void given_number_of_instances(int count) {
    header->instanceMax = count;
    admin = allocate((count+1)*sizeof(AdminEntry));
    instances = allocate((count+1)*sizeof(InstanceEntry));
}


Ensure(Instance, stopsLookingInsideContainersWhenEncounteringLocation) {
    header->locationClassId = 42;

	given_number_of_instances(3);

    /* Make them all containers */
    instances[1].container = 1;
    instances[2].container = 1;
    instances[3].container = 1;

    /* Nest them */
    admin[1].location = 2;
    admin[2].location = 3;
    admin[3].location = 0;

    /* Make intermediate into location */
    instances[2].parent = LOCATION;

    assert_that(isIn(1, 3, TRANSITIVE), is_false);
}
