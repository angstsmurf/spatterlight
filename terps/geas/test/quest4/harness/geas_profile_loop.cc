/* Profiling driver: replay a Quest 4 walkthrough in-process N times so a
   sampler has a single long-lived target.  Not a test; built on demand.

     make quest4/harness/geas_profile_loop
     GEAS_SEED=1 ./geas_profile_loop <iterations> <game-file> <command-script>

   It reuses the engine exactly as geas_walkthrough_runner does (same includes,
   same minimal GeasInterface), minus the runner's fight/scumming/marker logic:
   every scripted line is fed straight to run_command.  Games that need those
   extras will diverge, but the per-turn engine work being profiled is the same.
*/

#include <cctype>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../../geas-util.cc"
#include "../../../istring.cc"
#include "../../../readfile.cc"
#include "../../../geasfile.cc"
#include "../../../geas-state.cc"
#include "../../../geas-runner.cc"

namespace {

std::deque<std::string> g_queue;
struct InputExhausted { };

std::string dirname_of (const std::string &p)
{
  std::string::size_type s = p.find_last_of ('/');
  return s == std::string::npos ? std::string (".") : p.substr (0, s);
}

class RunnerInterface : public GeasInterface
{
public:
  int starved = 0;
  static const int kMaxStarved = 200;
protected:
  GeasResult print_normal (const std::string &) override { return r_success; }
  GeasResult print_newline () override { return r_success; }
  void set_foreground (const std::string &) override { }
  void set_background (const std::string &) override { }
  void debug_print (const std::string &) override { }
  GeasResult wait_keypress (const std::string &) override { return r_success; }
  GeasResult pause (int) override { return r_success; }
  std::string get_string () override
  {
    if (g_queue.empty ())
      {
        if (++starved > kMaxStarved) throw InputExhausted ();
        return "";
      }
    starved = 0;
    std::string s = g_queue.front (); g_queue.pop_front (); return s;
  }
  uint make_choice (const std::string &, std::vector<std::string> choices) override
  {
    int c = 1;
    if (g_queue.empty ()) { if (++starved > kMaxStarved) throw InputExhausted (); }
    else { starved = 0; c = atoi (g_queue.front ().c_str ()); g_queue.pop_front (); }
    if (c < 1) c = 1;
    if ((size_t) c > choices.size ()) c = (int) choices.size ();
    return (uint) c - 1;
  }
  std::string absolute_name (const std::string &rel, const std::string &parent) const override
  {
    if (!rel.empty () && rel[0] == '/') return rel;
    return dirname_of (parent) + "/" + rel;
  }
  std::string get_file (const std::string &fn) const override
  {
    std::ifstream f (fn.c_str (), std::ios::binary);
    if (!f) return "";
    std::ostringstream ss; ss << f.rdbuf (); return ss.str ();
  }
};

} // namespace

int main (int argc, char **argv)
{
  if (argc < 4)
    {
      std::cerr << "usage: " << argv[0] << " <iterations> <game> <script>\n";
      return 2;
    }
  long iters = atol (argv[1]);
  std::string game = argv[2], script = argv[3];

  std::ifstream sf (script.c_str ());
  if (!sf) { std::cerr << "cannot open script " << script << "\n"; return 2; }
  std::vector<std::string> cmds;
  std::string line;
  while (std::getline (sf, line))
    {
      std::string t = trim (line);
      if (t.empty ()) continue;
      std::string::size_type p = t.find ("  (");
      if (p != std::string::npos) t = trim (t.substr (0, p));
      if (!t.empty ()) cmds.push_back (t);
    }

  setenv ("GEAS_STRICT_NAMES", "1", 1);

  long total_turns = 0;
  for (long it = 0; it < iters; it++)
    {
      RunnerInterface gi;
      GeasRunner *gr = GeasRunner::get_runner (&gi);
      g_queue.clear ();
      for (const std::string &c : cmds) g_queue.push_back (c);
      try
        {
          gr->set_game (game);
          while (gr->is_running () && !g_queue.empty ())
            {
              std::string c = g_queue.front (); g_queue.pop_front ();
              gr->run_command (c);
              total_turns++;
            }
        }
      catch (const InputExhausted &) { }
      delete gr;
    }
  std::cout << "iterations=" << iters << " total_turns=" << total_turns << "\n";
  return 0;
}
