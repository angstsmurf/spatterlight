// Copyright 2009-2023 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <array>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "dict.h"
#include "memory.h"
#include "options.h"
#include "process.h"
#include "types.h"
#include "unicode.h"
#include "util.h"
#include "zterp.h"

struct Dictionary {
    explicit Dictionary(uint16_t addr) :
        m_addr(addr),
        m_num_separators(user_byte(m_addr)),
        m_entry_length(user_byte(m_addr + m_num_separators + 1)),
        m_num_entries(as_signed(user_word(m_addr + m_num_separators + 2))),
        m_base(m_addr + 1 + m_num_separators + 1 + 2) {
            ZASSERT(m_entry_length >= (zversion <= 3 ? 4 : 6), "dictionary entry length (%d) too small", m_entry_length);
            ZASSERT(m_base + (std::labs(m_num_entries) * m_entry_length) <= memory_size, "reported dictionary length extends beyond memory size");

            m_separators[ZSCII_SPACE] = true;
            for (uint8_t i = 0; i < m_num_separators; i++) {
                m_separators[byte(m_addr + 1 + i)] = true;
            }
        }

    uint16_t find(const uint8_t *token, size_t len) const;

    [[nodiscard]] bool is_sep(uint8_t c) const {
        return m_separators[c];
    }

private:
    uint16_t m_addr;
    uint8_t m_num_separators;
    std::array<bool, UINT8_MAX + 1> m_separators{};
    uint8_t m_entry_length;
    long m_num_entries;
    uint32_t m_base;
};

// Encode the text at “s”, of length “len” (there is not necessarily a
// terminating null character), returning it.
//
// Versions 1 and 2 make this function more complex than it should be.
// In versions 1 and 2, if there is a run of 2 or more characters in a
// different alphabet than the current one (but the same as each other),
// then locking is employed rather than shifting. This has one known
// use: the PDP-10 in Zork 1. With a dictionary word of “pdp10”, the 1
// and 0 are both in A2, and thus must be encoded with a lock, not two
// shifts. This vital change means that you can now refer to the PDP-10
// as “pdp10” instead of “machine”, or “dryer”, or “lid”, all of which
// are synonyms for it.
//
// Z-character 0 is a space (§3.5.1), so theoretically a space should be
// encoded simply with a zero. However, Inform 6.32 encodes space
// (which has the value 32) as a 10-bit ZSCII code, which is the
// Z-characters 5, 6, 1, 0. Assume this is correct.
static std::array<uint8_t, 6> encode_string(const uint8_t *s, size_t len)
{
    int n = 0;
    const int max = zversion <= 3 ? 6 : 9;
    const int shiftbase = zversion <= 2 ? 1 : 3;
    std::array<uint8_t, 12> chars;

    // This is always 0 for V3+, but for V1 and V2 it can change.
    int current_alphabet = 0;

    struct Character {
        int alphabet;
        std::array<uint8_t, 3> body;
        int body_len;
    };

    // Given a ZSCII value, return both the alphabet it’s in as well as
    // a byte array of the Z-character(s) needed to represent it. This
    // handles characters not in the alphabet table by returning an
    // alphabet of 2 and a Z-character of 6 followed by two 5-bit halves
    // of the ZSCII value (see §3.4).
    auto classify = [](uint8_t c) -> Character {
        int pos = atable_pos[c];
        if (pos >= 0) {
            return { pos / 26, {static_cast<uint8_t>(pos % 26 + 6)}, 1 };
        } else {
            return { 2, {6, static_cast<uint8_t>(c >> 5), static_cast<uint8_t>(c & 0x1f)}, 3 };
        }
    };

    for (size_t i = 0; i < len && n < max; i++) {
        Character cur = classify(s[i]);

        // See §3.7.1, and STRING-ZSTR in the pre-V3 ZILCH source.
        bool lock = zversion <= 2 &&
                    cur.alphabet != current_alphabet &&
                    i + 1 < len &&
                    classify(s[i + 1]).alphabet == cur.alphabet;

        int delta = (cur.alphabet - current_alphabet + 3) % 3;
        if (delta != 0) {
            chars[n++] = shiftbase + delta + (lock ? 2 : 0);
        }

        if (lock) {
            current_alphabet = cur.alphabet;
        }

        for (int j = 0; j < cur.body_len; j++) {
            chars[n++] = cur.body[j];
        }
    }

    while (n < max) {
        chars[n++] = 5;
    }

    std::array<uint8_t, 6> encoded;
    auto p = encoded.begin();

    // §3.2:
    // --first byte-------   --second byte---
    // 7    6 5 4 3 2  1 0   7 6 5  4 3 2 1 0
    // bit  --first--  --second---  --third--
    for (int i = 0; i < max; i += 3) {
        *p++ = (chars[i] << 2) | (chars[i + 1] >> 3);
        *p++ = ((chars[i + 1] & 0x7) << 5) | chars[i + 2];
    }

    // §3.2: the MSB of the last encoded word must be set.
    if (zversion <= 3) {
        encoded[2] |= 0x80;
    } else {
        encoded[4] |= 0x80;
    }

    return encoded;
}

uint16_t Dictionary::find(const uint8_t *token, size_t len) const
{
    const uint8_t *match = nullptr;
    auto dict_compar = [](const void *a, const void *b) {
        return std::memcmp(a, b, zversion <= 3 ? 4 : 6);
    };

    auto encoded = encode_string(token, len);

    if (m_num_entries > 0) {
        match = static_cast<uint8_t *>(std::bsearch(encoded.data(), &memory[m_base], m_num_entries, m_entry_length, dict_compar));
    } else {
        for (long i = 0; i < -m_num_entries; i++) {
            const uint8_t *entry = &memory[m_base + (i * m_entry_length)];

            if (dict_compar(encoded.data(), entry) == 0) {
                match = entry;
                break;
            }
        }
    }

    if (match == nullptr) {
        return 0;
    }

    return match - memory.data();
}

static uint16_t lookup_replacement(uint16_t original, const std::vector<uint8_t> &replacement, const Dictionary &dictionary)
{
    uint16_t d = dictionary.find(replacement.data(), replacement.size());

    if (d == 0) {
        return original;
    }

    return d;
}

static void handle_token(const uint8_t *base, const uint8_t *token, size_t len, uint16_t parse, const Dictionary &dictionary, int found, bool ignore_unknown, bool start_of_sentence)
{
    uint16_t d;

    d = dictionary.find(token, len);

    if (len == 1 && is_game(Game::Infocom1234) && start_of_sentence && !options.disable_abbreviations) {
        static const std::vector<uint8_t> examine = { 'e', 'x', 'a', 'm', 'i', 'n', 'e' };
        static const std::vector<uint8_t> again = { 'a', 'g', 'a', 'i', 'n' };
        static const std::vector<uint8_t> wait = { 'w', 'a', 'i', 't' };
        static const std::vector<uint8_t> oops = { 'o', 'o', 'p', 's' };

        if (*token == 'x') {
            d = lookup_replacement(d, examine, dictionary);
        } else if (*token == 'g') {
            d = lookup_replacement(d, again, dictionary);
        } else if (*token == 'z') {
            d = lookup_replacement(d, wait, dictionary);
        } else if (*token == 'o') {
            d = lookup_replacement(d, oops, dictionary);
        }
    }

    if (ignore_unknown && d == 0) {
        return;
    }

    parse = parse + 2 + (found * 4);

    user_store_word(parse, d);

    user_store_byte(parse + 2, len);

    if (zversion <= 4) {
        user_store_byte(parse + 3, token - base + 1);
    } else {
        user_store_byte(parse + 3, token - base + 2);
    }
}

// The behavior of tokenize is described in §15 (under the read opcode)
// and §13.
//
// For the text buffer, byte 0 is ignored in both V3/4 and V5+.
// Byte 1 of V3/4 is the start of the string, while in V5+ it is the
// length of the string.
// Byte 2 of V5+ is the start of the string. V3/4 strings have a null
// terminator, while V5+ do not.
//
// For the parse buffer, byte 0 contains the maximum number of tokens
// that can be read.
// The number of tokens found is stored in byte 1.
// Each token is then represented by a 4-byte chunk with the following
// information:
// • The first two bytes are the byte address of the dictionary entry
//   for the token, or 0 if the token was not found in the dictionary.
// • The next byte is the length of the token.
// • The final byte is the offset in the string of the token.
void tokenize(uint16_t text, uint16_t parse, uint16_t dictaddr, bool ignore_unknown)
{
    uint32_t text_len = 0;
    const int maxwords = user_byte(parse);
    bool in_word = false;
    int found = 0;
    bool start_of_sentence = true;

    if (dictaddr == 0) {
        dictaddr = header.dictionary;
    }

    ZASSERT(dictaddr != 0, "attempt to tokenize without a valid dictionary");

    Dictionary dictionary(dictaddr);

    if (zversion >= 5) {
        text_len = user_byte(text + 1);
    } else {
        while (user_byte(text + 1 + text_len) != 0) {
            text_len++;
        }
    }

    ZASSERT(text + 1 + (zversion >= 5) + text_len <= memory_size, "attempt to tokenize out-of-bounds string");

    const uint8_t *string = &memory[text + 1 + (zversion >= 5 ? 1 : 0)];
    const uint8_t *p = string;
    const uint8_t *lastp = string;

    do {
        if (!in_word && text_len != 0 && !dictionary.is_sep(*p)) {
            in_word = true;
            lastp = p;
        }

        if (text_len == 0 || dictionary.is_sep(*p)) {
            if (in_word && found < maxwords) {
                handle_token(string, lastp, p - lastp, parse, dictionary, found++, ignore_unknown, start_of_sentence);
                start_of_sentence = false;
            }

            // §13.6.1: Separators (apart from a space) are tokens too.
            if (text_len != 0 && *p != ZSCII_SPACE && found < maxwords) {
                handle_token(string, p, 1, parse, dictionary, found++, ignore_unknown, start_of_sentence);
                start_of_sentence = *p == ZSCII_PERIOD;
            }

            in_word = false;
        }

        p++;

    } while (text_len-- > 0 && found < maxwords);

    user_store_byte(parse + 1, found);
}

static void encode_text(uint32_t text, uint16_t len, uint16_t coded)
{
    ZASSERT(text + len <= memory_size, "reported text length extends beyond memory size");

    auto encoded = encode_string(&memory[text], len);

    for (int i = 0; i < 6; i++) {
        user_store_byte(coded + i, encoded[i]);
    }
}

#ifdef SPATTERLIGHT
void public_encode_text(uint32_t text, uint16_t len, uint16_t coded) {
    encode_text(text, len, coded);
}
#endif

void ztokenise()
{
    tokenize(zargs[0], zargs[1], zarg_or(2, 0), zarg_or(3, 0) != 0);
}

void zencode_text()
{
    encode_text(zargs[0] + zargs[2], zargs[1], zargs[3]);
}
