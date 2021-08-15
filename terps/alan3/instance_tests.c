#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>

#include "instance.h"

#include "memory.h"


/* Mocked modules */
#include "literal.mock"
#include "class.mock"
#include "attribute.mock"
#include "syserr.mock"
#include "compatibility.mock"
#include "current.mock"
#include "location.mock"
#include "inter.mock"
#include "output.mock"
#include "dictionary.mock"
#include "msg.mock"
#include "container.mock"
#include "checkentry.mock"
#include "actor.mock"
#include "debug.mock"
#include "params.mock"


static int class_count = 5;

static Aword *allocate_memory(size_t instance_count) {
    size_t total_size = sizeof(ACodeHeader) +
        (class_count+1)*sizeof(ClassEntry) +
        (instance_count+1)*sizeof(InstanceEntry) +
        (instance_count+1)*sizeof(AdminEntry);
    return (Aword *)allocate(total_size*sizeof(Aword));
}

static void setup_memory(size_t instance_count) {
    memory = allocate_memory(instance_count);
    header = (ACodeHeader*)memory;

    header->classMax = 5;
    header->entityClassId = 1;
    header->thingClassId = 2;
    header->objectClassId = 3;
    header->locationClassId = 4;
    header->actorClassId = 5;

    int classes_start = sizeof(ACodeHeader)/sizeof(Aword);
    classes = (ClassEntry*)&memory[classes_start];
    classes[header->entityClassId].parent = 0;
    classes[header->thingClassId].parent = header->entityClassId;
    classes[header->objectClassId].parent = header->thingClassId;
    classes[header->locationClassId].parent = header->entityClassId;
    classes[header->actorClassId].parent = header->entityClassId;

    header->instanceMax = instance_count;

    int admin_start = classes_start + (class_count+1)*sizeof(ClassEntry)/sizeof(Aword);
    admin = (AdminEntry*)&memory[admin_start];

    int instances_start = admin_start + (instance_count+1)*sizeof(AdminEntry)/sizeof(Aword);
    instances = (InstanceEntry*)&memory[instances_start];
}


#define MAX_NO_OF_INSTANCES 5

static int instance_count;

Describe(Instance);

BeforeEach(Instance) {
    setup_memory(MAX_NO_OF_INSTANCES);
    instance_count = 0;
}

AfterEach(Instance) {
    free(memory);
}

static int given_an_instance_at(char *name, int parent, int location) {
    instance_count++;
    instances[instance_count].parent = parent;
    admin[instance_count].location = location;
    return instance_count;
}

static int given_a_container_instance_at(char *name, int parent, int location) {
    Aint instance = given_an_instance_at(name, parent, location);
    instances[instance].container = 1;
    return instance;
}

Ensure(Instance, can_handle_direct_transitivity) {
    Aint container = given_an_instance_at("container", OBJECT, 1);
    Aint containee = given_an_instance_at("containee", OBJECT, container);
    Aint non_containee = given_an_instance_at("non_containee", OBJECT, 5);

    instances[container].container = 1;

    assert_that(isIn(containee, container, DIRECT));
    assert_that(!isIn(non_containee, container, DIRECT));
}


Ensure(Instance, IN_can_handle_transitive_transitivity) {
    Aint location = given_an_instance_at("location", LOCATION, 0);
    Aint outer_container = given_a_container_instance_at("outer_container", OBJECT, location);
    Aint intermediate_container = given_a_container_instance_at("intermediate_container", OBJECT, outer_container);
    Aint containee = given_an_instance_at("containee", OBJECT, intermediate_container);
    Aint non_containee = given_an_instance_at("non_containee", OBJECT, location);

    assert_that(isIn(containee, outer_container, TRANSITIVE));
    assert_that(isIn(intermediate_container, outer_container, TRANSITIVE));
    assert_that(!isIn(non_containee, outer_container, DIRECT));
}

Ensure(Instance, IN_can_handle_indirect_transitivity) {
    Aint location = given_an_instance_at("location", LOCATION, 0);
    Aint outer_container = given_a_container_instance_at("outer_container", OBJECT, location);
    Aint intermediate_container = given_a_container_instance_at("intermediate_container", OBJECT, outer_container);
    Aint containee = given_an_instance_at("containee", OBJECT, intermediate_container);
    Aint non_containee = given_an_instance_at("non_containee", OBJECT, location);

    assert_that(isIn(containee, outer_container, INDIRECT));
    assert_that(!isIn(intermediate_container, outer_container, INDIRECT));
    assert_that(!isIn(non_containee, outer_container, INDIRECT));
}

Ensure(Instance, IN_can_handle_direct_transitivity) {
    Aint location = given_an_instance_at("location", LOCATION, 0);
    Aint outer_container = given_a_container_instance_at("outer_container", OBJECT, location);
    Aint intermediate_container = given_a_container_instance_at("intermediate_container", OBJECT, outer_container);
    Aint containee = given_an_instance_at("containee", OBJECT, intermediate_container);
    Aint non_containee = given_an_instance_at("non_containee", OBJECT, location);

    assert_that(!isIn(containee, outer_container, DIRECT));
    assert_that(isIn(intermediate_container, outer_container, DIRECT));
    assert_that(isIn(containee, intermediate_container, DIRECT));
    assert_that(!isIn(non_containee, outer_container, DIRECT));
}


Ensure(Instance, can_see_if_an_instance_is_direct_at_a_location) {
    Aint location1 = given_an_instance_at("location1", LOCATION, 0);
    Aint location2 = given_an_instance_at("location2", LOCATION, 0);
    Aint instance1 = given_an_instance_at("instance1", OBJECT, location1);
    Aint instance2 = given_an_instance_at("instance1", OBJECT, location2);

    assert_that(isAt(instance1, location1, DIRECT));
    assert_that(!isAt(instance1, location2, DIRECT));
    assert_that(!isAt(instance2, location1, DIRECT));
    assert_that(isAt(instance2, location2, DIRECT));
}

Ensure(Instance, can_see_if_an_instance_is_direct_at_another_instance) {
    Aint location = given_an_instance_at("location", LOCATION, 0);
    Aint instance1 = given_an_instance_at("instance1", OBJECT, location);
    Aint instance2 = given_an_instance_at("instance1", OBJECT, location);

    assert_that(isAt(instance1, instance2, DIRECT));
}

Ensure(Instance, can_see_if_an_instance_is_not_direct_at_another_instance) {
    Aint location1 = given_an_instance_at("location1", LOCATION, 0);
    Aint location2 = given_an_instance_at("location2", LOCATION, 0);
    Aint instance1 = given_an_instance_at("instance1", OBJECT, location1);
    Aint instance2 = given_an_instance_at("instance1", OBJECT, location2);

    assert_that(!isAt(instance1, instance2, DIRECT));
}

Ensure(Instance, can_see_if_an_instance_is_transitively_at_location) {
    Aint location = given_an_instance_at("location", LOCATION, 0);
    Aint container = given_an_instance_at("container", OBJECT, location);
    Aint instance = given_an_instance_at("instance", OBJECT, container);

    assert_that(isAt(instance, location, TRANSITIVE));
}

Ensure(Instance, can_see_if_an_instance_is_not_transitive_at_location) {
    Aint location1 = given_an_instance_at("location1", LOCATION, 0);
    Aint location2 = given_an_instance_at("location2", LOCATION, 0);

    Aint container = given_an_instance_at("container", OBJECT, location1);
    Aint instance = given_an_instance_at("instance", OBJECT, container);

    assert_that(!isAt(instance, location2, TRANSITIVE));
}


/* Nested locations */
Ensure(Instance, can_see_if_a_location_is_direct_at_another) {
    Aint outer = given_an_instance_at("outer", LOCATION, 0);
    Aint inner = given_an_instance_at("inner", LOCATION, outer);

    assert_that(isAt(inner, outer, DIRECT));
}

Ensure(Instance, can_see_if_a_location_is_not_direct_at_another) {
    Aint outer = given_an_instance_at("outer", LOCATION, 0);
    Aint other = given_an_instance_at("other", LOCATION, 0);
    Aint inner = given_an_instance_at("inner", LOCATION, other);

    assert_that(!isAt(inner, outer, DIRECT));
}

Ensure(Instance, can_see_if_a_location_is_not_direct_at_another2) {
    Aint outer = given_an_instance_at("outer", LOCATION, 0);
    Aint other = given_an_instance_at("other", LOCATION, outer);
    Aint inner = given_an_instance_at("inner", LOCATION, other);

    assert_that(!isAt(inner, outer, DIRECT));
}

Ensure(Instance, can_see_if_a_location_is_transitive_at_another) {
    Aint outer = given_an_instance_at("outer", LOCATION, 0);
    Aint inner = given_an_instance_at("inner", LOCATION, outer);

    assert_that(isAt(inner, outer, TRANSITIVE));
}

Ensure(Instance, can_see_if_a_location_is_not_transitive_at_another) {
    Aint outer = given_an_instance_at("outer", LOCATION, 0);
    Aint other = given_an_instance_at("other", LOCATION, 0);
    Aint inner = given_an_instance_at("inner", LOCATION, other);

    assert_that(!isAt(inner, outer, TRANSITIVE));
}

Ensure(Instance, can_see_if_a_location_is_transitive_at_another2) {
    Aint outer = given_an_instance_at("outer", LOCATION, 0);
    Aint intermediate = given_an_instance_at("intermediate", LOCATION, outer);
    Aint inner = given_an_instance_at("inner", LOCATION, intermediate);

    assert_that(isAt(inner, outer, TRANSITIVE));
}
