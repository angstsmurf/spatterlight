/*
  casdump -- dump a Quest 4 game (.asl or .cas) as the engine parses it.

  Unity-includes the loader half of the engine and prints every GeasBlock
  that read_geas_file () builds: block type, name, initial parent, and the
  raw data lines the getters actually read.  Because it goes through
  readfile.cc, it speaks all three QCGF (.cas) versions and shows the game
  *after* preprocessing -- !include files merged, !addto blocks folded into
  their targets, and the CAS token stream decoded -- which is exactly the
  text the runtime's property and action lookups walk.

  ../../../uncas.pl is the other decoder: an independent Perl reimplementation
  that turns a .cas back into compilable ASL source, reinlining and
  pretty-printing as it goes.  Use uncas.pl to read a game the way its author
  wrote it; use casdump to see what geas believes the game says (and to
  bisect a divergence between the two decoders).

      make quest4/harness/casdump          # from terps/geas/test
      ./casdump <game.cas|game.asl>
*/

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/* Only the loader is used, but GeasInterface's out-of-line virtuals (and so
 * its vtable) live in geas-runner.cc, so the whole engine comes along -- the
 * same unity include the walkthrough runner uses. */
#include "../../../geas-util.cc"
#include "../../../istring.cc"
#include "../../../readfile.cc"
#include "../../../geasfile.cc"
#include "../../../geas-state.cc"
#include "../../../geas-runner.cc"

/* Just enough interface for the loader: file access and a place for load-time
 * diagnostics to land.  Errors readfile reports through print_normal go to
 * stderr so the dump on stdout stays clean. */
struct DumpInterface : public GeasInterface
{
  GeasResult print_normal (const std::string &s) override
  { std::cerr << s; return r_success; }
  GeasResult print_newline () override
  { std::cerr << "\n"; return r_success; }
  std::string absolute_name (const std::string &rel,
			     const std::string &parent) const override
  {
    std::string::size_type s = parent.find_last_of ('/');
    if (s == std::string::npos) return rel;
    return parent.substr (0, s + 1) + rel;
  }
  std::string get_file (const std::string &fn) const override
  {
    std::ifstream f (fn, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf ();
    return ss.str ();
  }
  void set_foreground (const std::string &) override {}
  void set_background (const std::string &) override {}
  std::string get_string () override { return ""; }
  uint make_choice (const std::string &, std::vector<std::string>) override
  { return 0; }
};

int main (int argc, char **argv)
{
  if (argc != 2)
    {
      std::cerr << "usage: casdump <game.cas|game.asl>\n";
      return 1;
    }
  DumpInterface gi;
  GeasFile gf = read_geas_file (&gi, argv[1]);
  if (gf.blocks.empty ())
    {
      std::cerr << "casdump: no blocks parsed from " << argv[1] << "\n";
      return 1;
    }
  for (const auto &b : gf.blocks)
    {
      std::cout << "define " << b.blocktype;
      if (!b.name.empty ())
	std::cout << " <" << b.name << ">";
      if (!b.parent.empty ())
	std::cout << "  ' parent: " << b.parent;
      std::cout << "\n";
      for (const auto &line : b.data)
	std::cout << "    " << line << "\n";
      std::cout << "end define\n\n";
    }
  return 0;
}
