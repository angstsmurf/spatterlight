/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* qstr.hh -- the handful of byte-string helpers that both engines and both
 * Glk frontends kept writing for themselves.  Header-only and free of any
 * engine's types, so the Quest 5 side can use it without taking a dependency
 * on the Quest 4 one (question-util.hh) or the other way round.
 *
 * Everything here is ASCII-only on purpose: the strings are UTF-8 (or
 * Latin-1 in an old Quest 4 game), and a fold or a whitespace test that
 * looked past 0x7f would cut multi-byte sequences in half.  No locale is
 * ever set in these programs, so this is also exactly what the <cctype>
 * calls the helpers replace were doing. */

#ifndef QUESTION_QSTR_HH
#define QUESTION_QSTR_HH

#include <string>
#include <string_view>

namespace qstr {

inline bool is_space (char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

inline char lower_char (char c)
{
    return c >= 'A' && c <= 'Z' ? (char) (c + ('a' - 'A')) : c;
}

/* `s` without its leading and trailing whitespace. */
inline std::string trim (std::string_view s)
{
    size_t a = 0, b = s.size ();
    while (a < b && is_space (s[a])) ++a;
    while (b > a && is_space (s[b - 1])) --b;
    return std::string (s.substr (a, b - a));
}

inline std::string ascii_lower (std::string s)
{
    for (char &c : s) c = lower_char (c);
    return s;
}

inline bool starts_with (std::string_view s, std::string_view prefix)
{
    return s.size () >= prefix.size () &&
           s.compare (0, prefix.size (), prefix) == 0;
}

inline bool ends_with (std::string_view s, std::string_view suffix)
{
    return s.size () >= suffix.size () &&
           s.compare (s.size () - suffix.size (), suffix.size (), suffix) == 0;
}

/* Case-insensitive equality. */
inline bool iequal (std::string_view a, std::string_view b)
{
    if (a.size () != b.size ()) return false;
    for (size_t i = 0; i < a.size (); i++)
        if (lower_char (a[i]) != lower_char (b[i])) return false;
    return true;
}

/* Case-insensitive find: where `needle` first occurs in `hay` at or after
 * `from`, or npos. */
inline size_t ifind (std::string_view hay, std::string_view needle,
                     size_t from = 0)
{
    for (size_t i = from; i + needle.size () <= hay.size (); i++)
        if (iequal (hay.substr (i, needle.size ()), needle)) return i;
    return std::string::npos;
}

} /* namespace qstr */

#endif /* QUESTION_QSTR_HH */
