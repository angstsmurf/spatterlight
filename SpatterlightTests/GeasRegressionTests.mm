//
//  GeasRegressionTests.mm
//  SpatterlightTests
//
//  Regression tests for four geas-runner.cc fixes (read verb, directionless
//  place exits, exists-honours-hidden, give/use-on-NPC running the action).
//
//  The geas terp is built as a command-line tool, not a linkable library, so
//  this test unity-includes the geas core translation units directly (verified
//  to compile as a single TU).  A capturing GeasInterface feeds an embedded
//  plain-ASL fixture via get_file and collects all output, so the test needs no
//  Glk, no external process, and no on-disk fixture at runtime.
//

#import <XCTest/XCTest.h>

#include <string>
#include <vector>

// --- Geas core (unity include) ----------------------------------------------
// Quoted includes inside these files resolve relative to their own directory
// (terps/geas), so no extra header search path is required.
#include "../terps/geas/geas-util.cc"
#include "../terps/geas/istring.cc"
#include "../terps/geas/readfile.cc"
#include "../terps/geas/geasfile.cc"
#include "../terps/geas/geas-state.cc"
#include "../terps/geas/geas-runner.cc"

namespace {

// Plain (uncompiled) ASL fixture exercising all four fixed code paths.  Mirrors
// terps/geas/test/geas_fix_regression.asl.
const char *kFixture =
    "define game <Geas Fix Regression Test>\n"
    "asl-version <311>\n"
    "game version <1.0>\n"
    "start <Start>\n"
    "startscript create exit <Start; Vault>\n"
    "command <check> if exists <apple> then msg <EXISTS_TRUE> else msg <EXISTS_FALSE>\n"
    "command <reveal> show <apple>\n"
    "command <getcoin> give <coin>\n"
    "end define\n"
    "\n"
    "define room <Start>\n"
    "look <You are at the start.>\n"
    "east <Second>\n"
    "define object <note>\n"
    "look <A plain looking note.>\n"
    "action <read> msg <READVERB_OK>\n"
    "end define\n"
    "define object <apple>\n"
    "look <A shiny red apple.>\n"
    "properties <hidden>\n"
    "end define\n"
    "define object <coin>\n"
    "look <A gold coin.>\n"
    "end define\n"
    "end define\n"
    "\n"
    "define room <Vault>\n"
    "look <VAULT_REACHED via a directionless place exit.>\n"
    "end define\n"
    "\n"
    "define room <Second>\n"
    "look <The second room. A guard stands here.>\n"
    "west <Start>\n"
    "define object <guard>\n"
    "look <A surly guard.>\n"
    "action <give coin> msg <GIVEVERB_OK>\n"
    "end define\n"
    "end define\n";

// Captures everything geas prints (normal output and the debug channel).
class CaptureInterface : public GeasInterface {
public:
    std::string captured;
protected:
    virtual GeasResult print_normal(const std::string &s) { captured += s; return r_success; }
    virtual GeasResult print_newline() { captured += "\n"; return r_success; }
    virtual void set_foreground(const std::string &) {}
    virtual void set_background(const std::string &) {}
    virtual void debug_print(const std::string &s) { captured += "\n[dbg]" + s + "\n"; }
    virtual std::string get_string() { return ""; }
    virtual uint make_choice(const std::string &, std::vector<std::string>) { return 0; }
    virtual std::string absolute_name(const std::string &rel, const std::string &) const { return rel; }
    virtual std::string get_file(const std::string &) const { return kFixture; }
};

} // namespace

@interface GeasRegressionTests : XCTestCase
@end

@implementation GeasRegressionTests {
    CaptureInterface *_gi;
    GeasRunner *_gr;
}

- (void)setUp {
    _gi = new CaptureInterface();
    _gr = GeasRunner::get_runner(_gi);
    _gr->set_game("fixture");
    XCTAssertTrue(_gr->is_running(), @"fixture failed to load");
}

- (void)tearDown {
    delete _gr; _gr = nullptr;
    delete _gi; _gi = nullptr;
}

// Runs a command and returns only the output it produced.
- (std::string)run:(const char *)cmd {
    size_t before = _gi->captured.size();
    _gr->run_command(cmd);
    return _gi->captured.substr(before);
}

- (BOOL)output:(const std::string &)out contains:(const char *)needle {
    return out.find(needle) != std::string::npos;
}

// Fix 1: the read verb dispatches an object's action <read>.
- (void)testReadVerbDispatchesAction {
    std::string out = [self run:"read note"];
    XCTAssertTrue([self output:out contains:"READVERB_OK"],
                  @"read verb did not dispatch action <read>; got: %s", out.c_str());
}

// Fix 2: exists honours the hidden property (false until show reveals it).
- (void)testExistsHonoursHidden {
    XCTAssertTrue([self output:[self run:"check"] contains:"EXISTS_FALSE"],
                  @"hidden object wrongly reported as existing");
    [self run:"reveal"];
    XCTAssertTrue([self output:[self run:"check"] contains:"EXISTS_TRUE"],
                  @"shown object reported as nonexistent");
}

// Fix 3: a directionless place exit parses (no malformed warning) and is
// reachable via "go to".
- (void)testDirectionlessPlaceExit {
    XCTAssertFalse(_gi->captured.find("malformed") != std::string::npos,
                   @"place exit logged as malformed at startup");
    XCTAssertTrue([self output:[self run:"go to vault"] contains:"VAULT_REACHED"],
                  @"could not navigate to a directionless place exit");
}

// Fix 4: giving an item to an NPC runs the recipient's action, not its name.
- (void)testGiveToNpcRunsAction {
    [self run:"getcoin"]; // put the coin in inventory
    [self run:"east"];    // move to the guard
    std::string out = [self run:"give coin to guard"];
    XCTAssertTrue([self output:out contains:"GIVEVERB_OK"],
                  @"give-to-NPC did not run the action; got: %s", out.c_str());
}

@end
