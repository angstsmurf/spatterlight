#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>

#include "compatibility.h"

#include "syserr.mock"
#include "instance.mock"


Describe(Compatibility);
BeforeEach(Compatibility) {}
AfterEach(Compatibility) {}

static char v3b1[4] = {'b', 1, 0, 3};
static char v3b2[4] = {'b', 2, 0, 3};
static char v3b3[4] = {'b', 3, 0, 3};
static char v3b4[4] = {'b', 4, 0, 3};
static char v3b5[4] = {'b', 5, 0, 3};


Ensure(Compatibility, can_identify_pre_beta3) {
    assert_that(isPreBeta3(v3b1));
    assert_that(isPreBeta3(v3b2));
    assert_that(!isPreBeta3(v3b3));
    assert_that(!isPreBeta3(v3b4));
    assert_that(!isPreBeta3(v3b5));
}

Ensure(Compatibility, can_identify_pre_beta4) {
    assert_that(isPreBeta4(v3b1));
    assert_that(isPreBeta4(v3b2));
    assert_that(isPreBeta4(v3b3));
    assert_that(!isPreBeta4(v3b4));
    assert_that(!isPreBeta4(v3b5));
}

Ensure(Compatibility, can_identify_pre_beta5) {
    assert_that(isPreBeta5(v3b1));
    assert_that(isPreBeta5(v3b2));
    assert_that(isPreBeta5(v3b3));
    assert_that(isPreBeta5(v3b4));
    assert_that(!isPreBeta5(v3b5));
}
