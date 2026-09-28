/*
 * Copyright (C) 2006  Mark J. Tilford
 * Copyright (C) 2021-2026  Petter Sjölund
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

#ifndef __readfile_hh
#define __readfile_hh

#include "questionfile.hh"
#include "general.hh"

#include <vector>
#include <string>
#include <ostream>
#include <iostream>

extern std::vector<std::string> tokenize (const std::string &s);
extern std::string next_token (const std::string &full, std::string::size_type &tok_start, std::string::size_type &tok_end, bool cvt_paren = false);
extern std::string first_token (const std::string &s, std::string::size_type &t_start, std::string::size_type &t_end);
extern std::string nth_token (const std::string &s, int n);
extern std::string get_token (const std::string &s, bool cvt_paren = false);
extern bool find_token (const std::string &s, const std::string &tok, std::string::size_type &tok_start, std::string::size_type &tok_end, bool cvt_paren = false);
extern QuestionFile read_question_file (QuestionInterface *, const std::string &);

enum trim_modes { TRIM_SPACES, TRIM_UNDERSCORE, TRIM_BRACE };
extern std::string trim (const std::string &, trim_modes mode = TRIM_SPACES);

extern void report_error(const std::string &s);

//std::ostream &operator<< (std::ostream &o, const std::vector<std::string> &v);


#endif
