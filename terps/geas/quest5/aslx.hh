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
#include <set>
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

    // StringList / ObjectList and *Dict (ordered) storage. Held behind a
    // shared_ptr so that lists and dictionaries are REFERENCE types, exactly
    // like Quest's QuestList<T> / QuestDictionary<T>: copying a Value (function
    // argument binding, `y = x` assignment, reading a field) shares the same
    // backing, so `list add`/`dictionary add` through any alias is visible
    // everywhere. Builtins that derive a *new* collection from an input must
    // call detach() first (see aslx-runtime-builtins.inc).
    //
    // Entries of both are full Values, not strings: Quest's collections hold
    // typed/boxed entries, and a single list/dictionary can mix objects,
    // strings, numbers, dictionaries... (spondre builds lists of dictionaries
    // and indexes them, `match["score"]`; CoreParser's
    // currentcommandresolvedelements dict holds resolved objects alongside a
    // boolean `multiple`). The container-level Type enum (StringList/ObjectList,
    // StringDict/ObjectDict/ScriptDict) is retained only for TypeOf and for the
    // NewXxx constructors' declared kind -- a loaded StringList holds String
    // entries and an ObjectList holds ObjectRef entries by construction.
    using DictEntry = std::pair<std::string, Value>;
    std::shared_ptr<std::vector<Value>> list_store;
    std::shared_ptr<std::vector<DictEntry>> dict_store;

    // Lazily-allocating accessors so a freshly-typed list/dict Value is usable
    // without a separate init step. The const overloads return a shared empty
    // when the store is null (a null list reads as empty, never faults). All
    // are defined out-of-line (they need Value to be complete).
    std::vector<Value> &list();
    const std::vector<Value> &list() const;
    std::vector<DictEntry> &dict();
    const std::vector<DictEntry> &dict() const;

    // Give this Value its own private copy of the list/dict backing, breaking
    // the reference-sharing. Used before building a derived collection.
    void detach();

    // Allocate the backing for a collection-typed Value if absent. Every
    // collection CREATION site must end with this invariant: a fresh (even
    // empty) collection owns a store, because copies alias the store and a
    // late lazy allocation would happen on one copy only -- .NET reference
    // semantics require `f(NewStringDictionary())` mutations through the
    // parameter to be visible to the caller.
    void ensure_backing();

    std::string debug_string() const;
};

inline std::vector<Value> &Value::list() {
    if (!list_store)
        list_store = std::make_shared<std::vector<Value>>();
    return *list_store;
}
inline const std::vector<Value> &Value::list() const {
    static const std::vector<Value> empty;
    return list_store ? *list_store : empty;
}
inline std::vector<Value::DictEntry> &Value::dict() {
    if (!dict_store)
        dict_store = std::make_shared<std::vector<DictEntry>>();
    return *dict_store;
}
inline const std::vector<Value::DictEntry> &Value::dict() const {
    static const std::vector<DictEntry> empty;
    return dict_store ? *dict_store : empty;
}
inline void Value::detach() {
    if (list_store)
        list_store = std::make_shared<std::vector<Value>>(*list_store);
    if (dict_store)
        dict_store = std::make_shared<std::vector<DictEntry>>(*dict_store);
    ensure_backing();  // a detached collection owns a store even when empty
}

inline void Value::ensure_backing() {
    switch (type) {
    case Type::StringList:
    case Type::ObjectList:
        list();
        break;
    case Type::StringDict:
    case Type::ObjectDict:
    case Type::ScriptDict:
        dict();
        break;
    default:
        break;
    }
}

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

    // Child ordering (QuestViva's SortIndex MetaField): elements start in
    // creation order; a runtime parent change bumps the element to the end of
    // its new parent's children (WorldModel.UpdateElementSortOrder). Children
    // enumerations sort by this.
    long sort_index = 0;

    // Fields in insertion order (Quest's field bag is unordered, but preserving
    // order keeps dumps stable and readable).
    std::vector<std::pair<std::string, Value>> fields;
    std::vector<std::string> inherits;   // <inherit name="..."/> type names

    Element *parent = nullptr;
    std::vector<Element *> children;

    const Value *field(const std::string &n) const;
    Value &set_field(const std::string &n, Value v);
    // Erase an own field (v530+ null assignment: Fields.Set REMOVES the
    // attribute, so HasAttribute goes false and inherited values re-resolve).
    void remove_field(const std::string &n);
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
    // Non-fatal notices (e.g. a media command no-opped headless). Kept separate
    // from `errors` so the boot/command-driving 0-error invariants stay strict.
    std::vector<std::string> warnings;

    // Set by the `finish` script command; the driver ends the session on it.
    bool finished = false;

    // Monotonic SortIndex source: creation order at load, bumped past
    // everything by runtime parent changes (see Element::sort_index).
    long next_sort_index = 0;

    Element *find(const std::string &n) const;
    const std::string *implied_type(const std::string &elem_type,
                                    const std::string &property) const;

    // Runtime element creation/destruction (the `create`/`create timer`/
    // `create turnscript`/`destroy` script commands). create_object appends a
    // new root-level element (with an optional inherited type) and registers it
    // by name; destroy unregisters an object/timer so find() no longer resolves
    // it. Both mirror QuestViva's ObjectFactory.CreateObject / DestroyElement.
    Element *create_object(const std::string &name, const std::string &type = "",
                           const std::string &elem_type = "object");
    void destroy_element(const std::string &name);
};

// Load from a file path. Sniffs the content: a PK zip is treated as a .quest
// package (game.aslx extracted from it); otherwise it is parsed as raw .aslx.
// Returns false and fills world.errors on failure.
//
// core_dir points at the bundled Quest Core libraries (terps/geas/quest5/aslx-core);
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

// Overlay a saved-game ASLX document (Quest's native .quest-save: an <asl
// original="..."> doc re-declaring elements with their saved state) onto an
// already-loaded World -- the freshly reloaded original game. Elements whose
// name already exists are DISPLACED by the save's version (QuestViva's
// Elements.Add override semantics); new names are created. `overlaid` receives
// every element name the save (re)defined, so the caller can drop live
// elements the save omitted. Returns false on an XML/version parse error.
bool overlay_aslx_buffer(const char *data, size_t len, World &world,
                         std::set<std::string> &overlaid,
                         const std::string &core_dir = "");

// Extract game.aslx from a .quest (zip) buffer into `out`. Returns false if the
// buffer is not a zip or has no game.aslx.
bool extract_game_aslx(const uint8_t *data, size_t len, std::string &out);

// One entry of a .quest zip package, as listed by zip_list_entries. `offset`
// is the payload's byte offset in the package (past the local header), so a
// stored (method 0) entry's bytes can be used straight from the file.
struct ZipEntryInfo {
    std::string name;           // entry path as stored, e.g. "pics/troll.png"
    uint16_t method = 0;        // 0 = stored, 8 = deflate
    uint32_t comp_size = 0;     // payload size in the package
    uint32_t raw_size = 0;      // decompressed size
    size_t offset = 0;          // payload offset in the package buffer
};

// List a zip's central directory. Returns false if the buffer is not a zip.
// Entries with unsupported compression methods are still listed (extraction
// rejects them); directory entries (trailing '/') are skipped.
bool zip_list_entries(const uint8_t *data, size_t len,
                      std::vector<ZipEntryInfo> &out);

// Extract one listed entry into `out`. Returns false on a corrupt package or
// an unsupported compression method.
bool zip_extract_entry(const uint8_t *data, size_t len, const ZipEntryInfo &e,
                       std::string &out);

// Find an entry by name: exact match first, then the first case-insensitive
// match -- QuestViva keeps package resources in an OrdinalIgnoreCase
// dictionary and resolves URLs the same way. Returns nullptr if absent.
const ZipEntryInfo *zip_find_entry(const std::vector<ZipEntryInfo> &entries,
                                   const std::string &name);

// Human-readable dump of the element tree, for the loader debug mode / tests.
std::string dump(const World &world);

}  // namespace aslx

#endif  // GEAS_ASLX_HH
