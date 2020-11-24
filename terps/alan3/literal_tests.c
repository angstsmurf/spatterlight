#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>

#include "literal.h"

/* Dependencies */
#include "memory.h"

/* Mocks */
#include "syserr.mock"
#include "instance.mock"


Describe(Literal);

BeforeEach(Literal) {
    litCount = 0;
    literals = (LiteralEntry*)allocate(100);
}

AfterEach(Literal) {
    free(literals);
}

Ensure(Literal, can_create_a_integer_literal) {
    createIntegerLiteral(42);

    assert_that(litCount, is_equal_to(1));
    assert_that(literals[1].type, is_equal_to(NUMERIC_LITERAL));
    assert_that(literals[1].value, is_equal_to(42));
}

Ensure(Literal, can_create_a_string_literal) {
    createStringLiteral("a string");

    assert_that(litCount, is_equal_to(1));
    assert_that(literals[1].type, is_equal_to(STRING_LITERAL));
    assert_that(fromAptr(literals[1].value), is_equal_to_string("a string"));
}

Ensure(Literal, can_quote_a_dollar_sign_in_literal) {
    createStringLiteral("a $-sign");

    assert_that(litCount, is_equal_to(1));
    assert_that(literals[1].type, is_equal_to(STRING_LITERAL));
    assert_that(fromAptr(literals[1].value), is_equal_to_string("a $_-sign"));
}

Ensure(Literal, can_quote_multiple_dollar_sign_in_literal) {
    createStringLiteral("$$$$");

    assert_that(litCount, is_equal_to(1));
    assert_that(literals[1].type, is_equal_to(STRING_LITERAL));
    assert_that(fromAptr(literals[1].value), is_equal_to_string("$_$_$_$_"));
}

Ensure(Literal, can_quote_a_symbol_in_string_literal) {
    createStringLiteral("a $p symbol");

    assert_that(litCount, is_equal_to(1));
    assert_that(literals[1].type, is_equal_to(STRING_LITERAL));
    assert_that(fromAptr(literals[1].value), is_equal_to_string("a $_p symbol"));
}
