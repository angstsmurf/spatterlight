// aslx.hh -- Quest 5 (.aslx / .quest) loader for Geas.
//
// This is the first piece of the "second engine" described in TODO-quest5.md:
// a native port of the Quest 5 WorldModel that shares the Glk frontend with the
// existing ASL text interpreter. Milestone 1 is the loader only -- unzip a
// .quest package (or read a raw .aslx), parse the XML into a typed element
// tree, and dump it. Scripts and expressions are stored verbatim here; running
// them is a later milestone.
//
// The data model and the tag/field-loading rules mirror QuestViva's
// src/Engine/GameLoader (ElementLoaders.cs / AttributeLoaders.cs /
// ExtendedAttributeLoaders.cs), which is the reference the ground-truth oracle
// diffs against.

#ifndef GEAS_ASLX_HH
#define GEAS_ASLX_HH

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace aslx {

// A typed field value. Only the storage side matters for the loader; arithmetic
// and coercion belong to the expression evaluator (a later milestone). Object
// references and object lists are "lazy" in Quest -- stored as element names and
// resolved once the whole tree is loaded -- so we keep them as names here too.
struct Value {
    enum class Type {
        Null,
        String,
        Int,
        Double,
        Boolean,
        Script,         // raw script text, run later
        StringList,     // simplestringlist / stringlist
        ObjectRef,      // lazy object reference (element name)
        ObjectList,     // lazy object list (element names)
        StringDict,     // stringdictionary / simplestringdictionary
        ObjectDict,     // objectdictionary / simpleobjectdictionary
        ScriptDict      // scriptdictionary (key -> script text)
    };

    Type type = Type::Null;
    std::string declared_type;  // the ASLX type name it was loaded as (for dumps)
    // Set for a <attr type="listextend">: this list *extends* the same-named
    // list inherited from the type chain rather than replacing it (QuestViva's
    // Fields.AddFieldExtension). The merge happens at field-read time.
    bool list_extend = false;

    std::string str;            // String / Script / ObjectRef
    long integer = 0;           // Int
    double dbl = 0.0;           // Double
    bool boolean = false;       // Boolean
    std::vector<std::string> list;                          // StringList / ObjectList
    std::vector<std::pair<std::string, std::string>> dict;  // *Dict (ordered)

    std::string debug_string() const;
};

// One element of the world model: object, command, function, game, type, etc.
// Containment is by nesting (parent/children). Inheritance is by <inherit> and
// is recorded here as a list of type names, resolved by the engine later.
struct Element {
    std::string name;
    std::string elem_type;   // "object", "command", "verb", "function", "game",
                             // "type", "turnscript", "timer", "walkthrough",
                             // "exit", "resource", "output", "javascript",
                             // "delegate"
    bool anonymous = false;
    int source_line = 0;

    // Fields in insertion order (Quest's field bag is unordered, but preserving
    // order keeps dumps stable and readable).
    std::vector<std::pair<std::string, Value>> fields;
    std::vector<std::string> inherits;   // <inherit name="..."/> type names

    Element *parent = nullptr;
    std::vector<Element *> children;

    const Value *field(const std::string &n) const;
    Value &set_field(const std::string &n, Value v);
    bool has_field(const std::string &n) const { return field(n) != nullptr; }
};

// The loaded world: every element plus the top-level metadata that lives outside
// elements (templates, implied types, includes).
struct World {
    int asl_version = 0;
    std::string version_string;
    std::string game_name;

    std::vector<std::unique_ptr<Element>> elements;  // owns all elements
    std::vector<Element *> roots;                    // top-level (parent == null)
    std::map<std::string, Element *> by_name;

    // name -> body text. command flag kept for the (rare) templatetype="command".
    std::vector<std::pair<std::string, std::string>> templates;
    std::vector<std::pair<std::string, std::string>> dynamic_templates;
    std::vector<std::pair<std::string, std::string>> verb_templates;

    // "element~property" -> type, from <implied>.
    std::map<std::string, std::string> implied_types;

    std::vector<std::string> includes;   // <include ref="..."/> (unresolved in M1)
    std::vector<std::string> errors;

    Element *find(const std::string &n) const;
    const std::string *implied_type(const std::string &elem_type,
                                    const std::string &property) const;
};

// Load from a file path. Sniffs the content: a PK zip is treated as a .quest
// package (game.aslx extracted from it); otherwise it is parsed as raw .aslx.
// Returns false and fills world.errors on failure.
//
// core_dir points at the bundled Quest Core libraries (terps/geas/aslx-core);
// if empty, the ASLX_CORE environment variable is consulted. <include ref="..">
// directives resolve against the game's own directory first, then core_dir,
// then core_dir/Languages -- matching QuestViva's GetLibraryStream() order.
bool load_file(const std::string &path, World &world,
               const std::string &core_dir = "");

// Load from an in-memory buffer already known to be .aslx XML (UTF-8, optional
// BOM). core_dir/game_dir seed the <include> search path (see load_file).
bool load_aslx_buffer(const char *data, size_t len, World &world,
                      const std::string &core_dir = "",
                      const std::string &game_dir = "");

// Extract game.aslx from a .quest (zip) buffer into `out`. Returns false if the
// buffer is not a zip or has no game.aslx.
bool extract_game_aslx(const uint8_t *data, size_t len, std::string &out);

// Human-readable dump of the element tree, for the loader debug mode / tests.
std::string dump(const World &world);

}  // namespace aslx

#endif  // GEAS_ASLX_HH
