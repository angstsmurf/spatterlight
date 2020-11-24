#include <cgreen/cgreen.h>

#include "utils.c"

Describe(Utils);
BeforeEach(Utils) {}
AfterEach(Utils) {}

Ensure(Utils, can_match_exact_strings) {
    char *pattern = "exact";
    char *input = "exact";

    assert_that(match(pattern, input));
}

Ensure(Utils, can_match_string_with_trailing_wildcard) {
    char *pattern = "ex*";
    char *input1 = "exact";
    char *input2 = "examine";

    assert_that(match(pattern, input1));
    assert_that(match(pattern, input2));
}
