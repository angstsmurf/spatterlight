#include "cgreen/cgreen.h"

#include "word.c"

Describe(Word);
BeforeEach(Word) {}
AfterEach(Word) {}


Ensure(Word, canAllocatePlayerWordsFromEmpty) {
  assert_equal(playerWords, NULL);
  ensureSpaceForPlayerWords(1);
  assert_not_equal(playerWords, NULL);
  assert_equal(playerWordsLength, PLAYER_WORDS_EXTENT);
}

Ensure(Word, canExtendPlayerWords) {
  assert_equal(playerWords, NULL);
  ensureSpaceForPlayerWords(0);
  ensureSpaceForPlayerWords(PLAYER_WORDS_EXTENT);
  assert_not_equal(playerWords, NULL);
  assert_equal(playerWordsLength, 2*PLAYER_WORDS_EXTENT);
}
