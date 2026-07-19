// aslx-runtime-builtins.inc -- built-in expression functions (ExpressionOwner /
// StringFunctions subset). Textually included at the end of aslx-runtime.cc.
// A focused milestone-2 set: enough type/attribute, list, string and number
// helpers to run a hand-written .aslx. GetRandomInt/Double route through the
// deterministic Rng (erkyrath_random). String/list positions are 1-based, as in
// Quest (VB-style Mid/Left/Right/Instr).

namespace aslx {

static std::string to_lower(std::string s) {
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// .NET's ToLowerInvariant/ToUpperInvariant (behind LCase/UCase/CapFirst) are
// full-Unicode; a std::tolower fold is ASCII-only, which broke Greek games
// (Iron John: the input "βασιλιά" must LCase-match the alias "Βασιλιά", and
// CapFirst("το") must print "Το"). Simple one-codepoint case mappings for the
// scripts the corpus contains: ASCII, Latin-1, Latin Extended-A, Greek,
// Cyrillic. No locale variants or SpecialCasing — same as .NET's invariant
// simple mapping over these ranges.
static uint32_t cp_to_lower(uint32_t c) {
    if (c < 0x80) return (uint32_t)std::tolower((int)c);
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 0x20;    // Latin-1
    if (c == 0x178) return 0xFF;                                 // Ÿ -> ÿ
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177))
        return (c % 2 == 0) ? c + 1 : c;                         // Āā..Ŵŵ pairs
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
        return (c % 2 == 1) ? c + 1 : c;                         // Ĺĺ..Žž pairs
    if (c == 0x386) return 0x3AC;                                // Ά -> ά
    if (c >= 0x388 && c <= 0x38A) return c + 0x25;               // Έ Ή Ί
    if (c == 0x38C) return 0x3CC;                                // Ό -> ό
    if (c == 0x38E || c == 0x38F) return c + 0x3F;               // Ύ Ώ
    if (c >= 0x391 && c <= 0x3AB && c != 0x3A2) return c + 0x20; // Α..Ϋ
    if (c >= 0x410 && c <= 0x42F) return c + 0x20;               // А..Я
    if (c >= 0x400 && c <= 0x40F) return c + 0x50;               // Ѐ..Џ
    return c;
}
static uint32_t cp_to_upper(uint32_t c) {
    if (c < 0x80) return (uint32_t)std::toupper((int)c);
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 0x20;    // Latin-1
    if (c == 0xFF) return 0x178;                                 // ÿ -> Ÿ
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177))
        return (c % 2 == 1) ? c - 1 : c;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
        return (c % 2 == 0) ? c - 1 : c;
    if (c == 0x3AC) return 0x386;                                // ά -> Ά
    if (c >= 0x3AD && c <= 0x3AF) return c - 0x25;               // έ ή ί
    if (c == 0x3CC) return 0x38C;                                // ό -> Ό
    if (c == 0x3CD || c == 0x3CE) return c - 0x3F;               // ύ ώ
    if (c == 0x3C2) return 0x3A3;                                // final ς -> Σ
    if (c >= 0x3B1 && c <= 0x3CB) return c - 0x20;               // α..ϋ
    if (c >= 0x430 && c <= 0x44F) return c - 0x20;               // а..я
    if (c >= 0x450 && c <= 0x45F) return c - 0x50;               // ѐ..џ
    return c;
}
// Codepoint-wise UTF-8 case conversion; first_only converts just the leading
// codepoint (CapFirst). Malformed bytes pass through unchanged.
static std::string utf8_case(const std::string &s, bool upper, bool first_only) {
    std::string out;
    out.reserve(s.size());
    bool done = false;
    for (size_t i = 0; i < s.size();) {
        unsigned char b = (unsigned char)s[i];
        uint32_t cp = 0;
        size_t len = 1;
        if (b < 0x80) { cp = b; }
        else if ((b & 0xE0) == 0xC0 && i + 1 < s.size()) { cp = ((b & 0x1Fu) << 6) | ((unsigned char)s[i+1] & 0x3Fu); len = 2; }
        else if ((b & 0xF0) == 0xE0 && i + 2 < s.size()) { cp = ((b & 0x0Fu) << 12) | (((unsigned char)s[i+1] & 0x3Fu) << 6) | ((unsigned char)s[i+2] & 0x3Fu); len = 3; }
        else if ((b & 0xF8) == 0xF0 && i + 3 < s.size()) { cp = ((b & 0x07u) << 18) | (((unsigned char)s[i+1] & 0x3Fu) << 12) | (((unsigned char)s[i+2] & 0x3Fu) << 6) | ((unsigned char)s[i+3] & 0x3Fu); len = 4; }
        else { out += (char)b; ++i; continue; }
        if (!done) {
            cp = upper ? cp_to_upper(cp) : cp_to_lower(cp);
            if (first_only) done = true;
        }
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
        i += len;
    }
    return out;
}

// Recursive "does e inherit type t" (directly or transitively).
static bool does_inherit(World &w, Element *e, const std::string &t) {
    if (!e) return false;
    for (const std::string &parent : e->inherits) {
        if (parent == t) return true;
        if (does_inherit(w, w.find(parent), t)) return true;
    }
    return false;
}

// ASLX type name for a value, matching WorldModel.ConvertTypeToTypeName.
static std::string type_name(const Value &v) {
    switch (v.type) {
    case Value::Type::Null: return "null";
    case Value::Type::String: return "string";
    case Value::Type::Int: return "int";
    case Value::Type::Double: return "double";
    case Value::Type::Boolean: return "boolean";
    case Value::Type::ObjectRef: return "object";
    case Value::Type::Script: return "script";
    case Value::Type::StringList: return "stringlist";
    case Value::Type::ObjectList: return "objectlist";
    case Value::Type::StringDict: return "stringdictionary";
    case Value::Type::ObjectDict: return "objectdictionary";
    case Value::Type::ScriptDict: return "scriptdictionary";
    }
    return "null";
}

// System.String.CompareTo -- the comparer behind OrderBy in QuestViva's
// ObjectListSort/StringListSort -- is a culture WORD sort, not ordinal:
// letters compare case-insensitively first ("archimedes" < "Attendant"), and
// strings equal ignoring case put lowercase before uppercase at the first
// differing position. EFMB sorts its turnscripts by name and relies on this.
static bool dotnet_string_less(const std::string &a, const std::string &b) {
    size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i) {
        int la = std::tolower((unsigned char)a[i]);
        int lb = std::tolower((unsigned char)b[i]);
        if (la != lb) return la < lb;
    }
    if (a.size() != b.size()) return a.size() < b.size();
    for (size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = (unsigned char)a[i], cb = (unsigned char)b[i];
        if (ca != cb) return std::islower(ca) && std::isupper(cb);
    }
    return false;
}

// Template lookups: later registrations win (Quest's m_templateLookup overwrite).
static const std::string *find_template(World &w, const std::string &name) {
    for (auto it = w.templates.rbegin(); it != w.templates.rend(); ++it)
        if (it->first == name) return &it->second;
    return nullptr;
}
static const std::string *find_dyn_template(World &w, const std::string &name) {
    for (auto it = w.dynamic_templates.rbegin(); it != w.dynamic_templates.rend();
         ++it)
        if (it->first == name) return &it->second;
    return nullptr;
}

Value Interp::call_builtin(const std::string &name, std::vector<Value> &args,
                           bool &handled, Context &ctx) {
    handled = true;
    const std::string fn = to_lower(name);
    const size_t n = args.size();
    auto arg = [&](size_t i) -> const Value & {
        static Value nullv;
        nullv = vnull();
        return i < n ? args[i] : nullv;
    };
    auto obj_of = [&](const Value &v) -> Element * {
        return v.type == Value::Type::ObjectRef ? world_.find(v.str) : nullptr;
    };
    // The current containment parent, read from the (own) `parent` field so a
    // runtime MoveObject (which sets obj.parent) is reflected -- not the static
    // load-time Element::parent pointer.
    auto parent_of = [&](Element *e) -> Element * {
        if (!e) return nullptr;
        const Value *p = e->field("parent");
        return (p && p->type == Value::Type::ObjectRef) ? world_.find(p->str)
                                                        : nullptr;
    };
    auto sarg = [&](size_t i) { return to_string(arg(i)); };
    auto iarg = [&](size_t i) { return (long)std::llround(as_double(arg(i))); };
    // Still reachable by name: `destroy` only unregisters (storage stays in
    // world.elements), and QuestViva removes destroyed elements entirely, so
    // every world enumeration must skip the dead ones. Without this, a
    // destroyed turnscript whose NAME is later reused (GetUniqueElementName
    // counts up from what find() sees) shows up twice in AllTurnScripts --
    // both entries resolving to the live one, running its script twice a turn
    // (Jacqueline's SetTurnTimeout chain fired at half the authored delay).
    auto live = [&](const std::unique_ptr<Element> &up) {
        return world_.find(up->name) == up.get();
    };

    // QuestViva ExpressionOwner argument checking, needed verbatim in the
    // transcripts of buggy games: a null argument throws .NET's
    // ArgumentNullException message ("Value cannot be null. (Parameter 'x')",
    // ArgumentNullException.ThrowIfNull), and a wrong-typed one throws
    // GetParameter's "<Fn> function expected <type> parameter but was passed
    // '<value>'". The error unwinds to the enclosing expression root, which
    // wraps it as "Error evaluating expression '...': ...".
    auto throw_null = [&](const char *param) {
        error(std::string("Value cannot be null. (Parameter '") + param + "')");
    };
    auto expect_object = [&](const char *caller, const char *param = "obj") {
        if (arg(0).type == Value::Type::Null) throw_null(param);
        if (arg(0).type != Value::Type::ObjectRef)
            error(std::string(caller) +
                  " function expected object parameter but was passed '" +
                  to_string(arg(0)) + "'");
    };
    auto expect_dictionary = [&](const char *caller) {
        if (arg(0).type == Value::Type::Null) throw_null("obj");
        if (arg(1).type == Value::Type::Null) throw_null("key");
        if (arg(0).type != Value::Type::StringDict &&
            arg(0).type != Value::Type::ObjectDict &&
            arg(0).type != Value::Type::ScriptDict)
            error(std::string(caller) +
                  " function expected dictionary parameter but was passed '" +
                  to_string(arg(0)) + "'");
    };
    {
        // The (obj, property) attribute getters / has-checks all start with
        // ThrowIfNull(obj) + ThrowIfNull(property).
        static const struct { const char *lower, *canon; } kObjProp[] = {
            {"getattribute", "GetAttribute"}, {"getboolean", "GetBoolean"},
            {"getint", "GetInt"}, {"getdouble", "GetDouble"},
            {"getstring", "GetString"}, {"hasattribute", "HasAttribute"},
            {"hasstring", "HasString"}, {"hasint", "HasInt"},
            {"hasboolean", "HasBoolean"}, {"hasobject", "HasObject"},
            {"hasdouble", "HasDouble"}, {"hasscript", "HasScript"},
            {"hasdelegateimplementation", "HasDelegateImplementation"},
        };
        for (const auto &m : kObjProp) {
            if (fn == m.lower && n == 2) {
                expect_object(m.canon);
                if (arg(1).type == Value::Type::Null) throw_null("property");
                break;
            }
        }
        if ((fn == "getdirectchildren" || fn == "getallchildobjects" ||
             fn == "getattributenames") && n >= 1 && arg(0).type == Value::Type::Null)
            throw_null("obj");
        if (fn == "contains" && n == 2 && arg(0).type == Value::Type::Null)
            throw_null("parent");
        static const struct { const char *lower, *canon; } kDictItem[] = {
            {"dictionaryitem", "DictionaryItem"},
            {"stringdictionaryitem", "StringDictionaryItem"},
            {"objectdictionaryitem", "ObjectDictionaryItem"},
            {"scriptdictionaryitem", "ScriptDictionaryItem"},
            {"dictionarycontains", "DictionaryContains"},
        };
        for (const auto &m : kDictItem) {
            if (fn == m.lower && n == 2) {
                expect_dictionary(m.canon);
                break;
            }
        }
    }

    // ---- attribute / type -------------------------------------------------
    if (fn == "getattribute" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return f ? *f : vnull();
    }
    if (fn == "getboolean" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::Boolean && f->boolean);
    }
    if (fn == "getint" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vint(f ? (long)std::llround(as_double(*f)) : 0);
    }
    if (fn == "getdouble" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vdouble(f ? as_double(*f) : 0);
    }
    if (fn == "getstring" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vstr(f ? to_string(*f) : std::string());
    }
    if (fn == "hasattribute" && n == 2)
        return vbool(resolve_field(obj_of(arg(0)), sarg(1)) != nullptr);
    if (fn == "hasstring" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::String);
    }
    if (fn == "hasint" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && (f->type == Value::Type::Int || f->type == Value::Type::Double));
    }
    if (fn == "hasboolean" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::Boolean);
    }
    if (fn == "hasobject" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::ObjectRef);
    }
    if (fn == "hasdouble" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::Double);
    }
    // HasScript / HasDelegateImplementation: a field of Script type. We store a
    // delegate-implementation body as Script too, so both map to the same check.
    if ((fn == "hasscript" || fn == "hasdelegateimplementation") && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vbool(f && f->type == Value::Type::Script);
    }
    if (fn == "getobject" && n == 1) {
        // Exits, turnscripts and the game element are all ElementType.Object in
        // QuestViva (ObjectType.Exit/TurnScript/Game), so GetObject resolves
        // them -- ProcessTextCommand_Exit and dynamically-created turnscripts
        // ("turnscript = GetObject(name)") rely on this.
        Element *e = world_.find(sarg(0));
        return e && (e->elem_type == "object" || e->elem_type == "exit" ||
                     e->elem_type == "turnscript" || e->elem_type == "game")
                   ? vobj(e->name) : vnull();
    }
    if (fn == "gettimer" && n == 1) {
        Element *e = world_.find(sarg(0));
        return e && e->elem_type == "timer" ? vobj(e->name) : vnull();
    }
    // RunDelegateFunction(obj, "delegatename", params...): the expression form
    // of rundelegate; returns the implementation script's return value.
    if (fn == "rundelegatefunction" && n >= 2) {
        std::vector<Value> params(args.begin() + 2, args.end());
        return interp_run_delegate(*this, obj_of(arg(0)), sarg(1), params);
    }
    // GetUIOption: player-UI configuration lookup. Headless has no overrides, so
    // every option reads back empty (Core's "= true"/LengthOf > 0 guards fail).
    if (fn == "getuioption" && n == 1) return vstr(std::string());
    // GetFileURL: front-end URL for a packaged resource. Headless there is no
    // web root; the filename itself is a serviceable stand-in (it only ever
    // lands inside HTML the presentation milestone will interpret).
    if (fn == "getfileurl" && n == 1) return vstr(sarg(0));
    // GetUniqueElementName (WorldModel.GetUniqueElementName): the name itself
    // if free, else the name minus trailing digits with the first free number
    // appended (timeout -> timeout1, timeout2, ...).
    if (fn == "getuniqueelementname" && n == 1) {
        std::string base = sarg(0);
        if (!world_.find(base)) return vstr(base);
        std::string root = base;
        while (!root.empty() && std::isdigit((unsigned char)root.back()))
            root.pop_back();
        for (long k = 1;; ++k) {
            std::string cand = root + std::to_string(k);
            if (!world_.find(cand)) return vstr(cand);
        }
    }
    // IsGameRunning: State != Finished (ExpressionOwner.IsGameRunning). Turn
    // scripts guard on this every turn.
    if (fn == "isgamerunning" && n == 0) return vbool(!world_.finished);
    if (fn == "doesinherit" && n == 2)
        return vbool(does_inherit(world_, obj_of(arg(0)), sarg(1)));
    if (fn == "typeof" && n == 1) return vstr(type_name(arg(0)));
    if (fn == "typeof" && n == 2) {
        const Value *f = resolve_field(obj_of(arg(0)), sarg(1));
        return vstr(f ? type_name(*f) : std::string("null"));
    }
    if (fn == "isdefined" && n == 1)
        return vbool(ctx.locals.count(sarg(0)) > 0);
    // Eval(expr[, scope]): evaluate an expression string, optionally with a
    // dictionary supplying local variables (QuestViva ExpressionOwner.Eval). The
    // text processor's {=...} uses this; ParamsForTextProcessor is usually empty.
    if (fn == "eval" && (n == 1 || n == 2)) {
        Context local;
        if (n == 2)
            for (auto &kv : arg(1).dict())
                local.locals[kv.first] = kv.second;  // values are typed per-entry
        return eval(sarg(0), local);
    }

    // ---- world model: objects, children, attributes -----------------------
    if (fn == "allobjects" && n == 0) {
        std::vector<std::string> names;
        for (auto &up : world_.elements)
            if (up->elem_type == "object" && live(up)) names.push_back(up->name);
        return vobjlist(names);
    }
    if (fn == "allcommands" && n == 0) {
        // Verbs ARE commands (QuestViva loads <verb> as ElementType.Command
        // with IsVerb=true) -- ScopeCommands depends on seeing them here.
        std::vector<std::string> names;
        for (auto &up : world_.elements)
            if ((up->elem_type == "command" || up->elem_type == "verb") &&
                live(up))
                names.push_back(up->name);
        return vobjlist(names);
    }
    if (fn == "allexits" && n == 0) {
        std::vector<std::string> names;
        for (auto &up : world_.elements)
            if (up->elem_type == "exit" && live(up)) names.push_back(up->name);
        return vobjlist(names);
    }
    if (fn == "getexitbylink" && n == 2) {
        // ExpressionOwner.GetExitByLink: the first ElementType.Object element
        // (exits included -- Elements.Objects is the whole Object element
        // type, not ObjectType.Object) parented in `from` whose `to` field is
        // the target, returned as its NAME (a string), or null. Hawk the
        // Hunter's NPC movement messages use it every walk tick.
        if (arg(0).type == Value::Type::Null) throw_null("from");
        if (arg(1).type == Value::Type::Null) throw_null("to");
        for (int i = 0; i < 2; ++i)
            if (arg(i).type != Value::Type::ObjectRef)
                error("GetExitByLink function expected object parameter but "
                      "was passed '" + to_string(arg(i)) + "'");
        Element *fromEl = obj_of(arg(0)), *toEl = obj_of(arg(1));
        for (auto &up : world_.elements) {
            if (!live(up)) continue;
            if (up->elem_type != "object" && up->elem_type != "exit" &&
                up->elem_type != "turnscript" && up->elem_type != "game")
                continue;
            if (parent_of(up.get()) != fromEl) continue;
            const Value *f = resolve_field(up.get(), "to");
            if (f && f->type == Value::Type::ObjectRef && obj_of(*f) == toEl)
                return vstr(up->name);
        }
        return vnull();
    }
    if (fn == "allturnscripts" && n == 0) {
        std::vector<std::string> names;
        for (auto &up : world_.elements)
            if (up->elem_type == "turnscript" && live(up))
                names.push_back(up->name);
        return vobjlist(names);
    }
    // Children enumerate in SortIndex order (Elements.GetDirectChildren sorts
    // by the SortIndex MetaField): creation order, except a runtime parent
    // change moves the element to the end of its new parent's children.
    auto direct_children = [&](Element *e) {
        std::vector<Element *> kids;
        for (auto &up : world_.elements)
            if (up->elem_type == "object" && live(up) &&
                parent_of(up.get()) == e)
                kids.push_back(up.get());
        std::stable_sort(kids.begin(), kids.end(),
                         [](const Element *a, const Element *b) {
                             return a->sort_index < b->sort_index;
                         });
        return kids;
    };
    if (fn == "getdirectchildren" && n == 1) {
        Element *e = obj_of(arg(0));
        std::vector<std::string> names;
        if (e)
            for (Element *k : direct_children(e)) names.push_back(k->name);
        return vobjlist(names);
    }
    if (fn == "getallchildobjects" && n == 1) {
        Element *e = obj_of(arg(0));
        std::vector<std::string> names;
        // Recurse through the parent-field tree. A visited set guards against a
        // cycle introduced by a bad runtime parent assignment.
        std::function<void(Element *)> rec = [&](Element *x) {
            for (Element *k : direct_children(x)) {
                if (std::find(names.begin(), names.end(), k->name) != names.end())
                    continue;
                names.push_back(k->name);
                rec(k);
            }
        };
        if (e) rec(e);
        return vobjlist(names);
    }
    if (fn == "contains" && n == 2) {
        Element *parent = obj_of(arg(0)), *child = obj_of(arg(1));
        bool found = false;
        int guard = 0;
        for (Element *p = parent_of(child); p && guard < 10000;
             p = parent_of(p), ++guard)
            if (p == parent) { found = true; break; }
        return vbool(found);
    }
    if (fn == "getattributenames" && (n == 1 || n == 2)) {
        Element *e = obj_of(arg(0));
        bool inh = (n == 2) && truthy(arg(1));
        std::vector<std::string> names;
        auto collect = [&](Element *x) {
            if (!x) return;
            for (auto &kv : x->fields)
                if (std::find(names.begin(), names.end(), kv.first) == names.end())
                    names.push_back(kv.first);
        };
        collect(e);
        if (inh && e)
            for (auto it = e->inherits.rbegin(); it != e->inherits.rend(); ++it)
                collect(world_.find(*it));
        return vstrlist(names);
    }

    // ---- dictionaries -----------------------------------------------------
    // The constructors must allocate the backing EAGERLY: a fresh dictionary
    // is one .NET object and every copy of the Value must alias it. With a
    // lazy (null) store, passing an empty dictionary to a function and adding
    // through the parameter would allocate on the copy only, and the caller's
    // dictionary would stay empty (broke v550 disambiguation menus).
    if ((fn == "newdictionary" || fn == "newstringdictionary") && n == 0) {
        Value v; v.type = Value::Type::StringDict; v.dict(); return v;
    }
    if (fn == "newobjectdictionary" && n == 0) {
        Value v; v.type = Value::Type::ObjectDict; v.dict(); return v;
    }
    if (fn == "newscriptdictionary" && n == 0) {
        Value v; v.type = Value::Type::ScriptDict; v.dict(); return v;
    }
    if (fn == "dictionarycount" && n == 1) return vint((long)arg(0).dict().size());
    if (fn == "dictionarycontains" && n == 2) {
        for (auto &kv : arg(0).dict()) if (kv.first == sarg(1)) return vbool(true);
        return vbool(false);
    }
    // The *DictionaryItem getters index the underlying .NET dictionary: a
    // missing key throws KeyNotFoundException's message (seen verbatim in the
    // oracle transcripts), not null.
    auto missing_key = [&]() {
        error("The given key '" + sarg(1) + "' was not present in the dictionary.");
    };
    if (fn == "dictionaryitem" && n == 2) {
        for (auto &kv : arg(0).dict()) if (kv.first == sarg(1)) return kv.second;
        missing_key();
    }
    if (fn == "stringdictionaryitem" && n == 2) {
        for (auto &kv : arg(0).dict())
            if (kv.first == sarg(1)) return vstr(to_string(kv.second));
        missing_key();
    }
    if (fn == "objectdictionaryitem" && n == 2) {
        for (auto &kv : arg(0).dict())
            if (kv.first == sarg(1))
                return kv.second.type == Value::Type::ObjectRef
                           ? kv.second : vobj(to_string(kv.second));
        missing_key();
    }
    if (fn == "scriptdictionaryitem" && n == 2) {
        for (auto &kv : arg(0).dict()) if (kv.first == sarg(1)) return kv.second;
        missing_key();
    }
    if (fn == "stringlistitem" && n == 2) {
        const Value &l = arg(0); long i = iarg(1);
        if (i < 0 || (size_t)i >= l.list().size()) return vnull();
        return vstr(to_string(l.list()[i]));
    }
    if (fn == "objectlistitem" && n == 2) {
        const Value &l = arg(0); long i = iarg(1);
        if (i < 0 || (size_t)i >= l.list().size()) return vnull();
        const Value &e = l.list()[i];
        return e.type == Value::Type::ObjectRef ? e : vobj(to_string(e));
    }

    // ---- templates --------------------------------------------------------
    if (fn == "template" && n == 1) {
        const std::string *t = find_template(world_, sarg(0));
        return vstr(t ? *t : std::string());
    }
    if (fn == "dynamictemplate" && n >= 1) {
        const std::string *t = find_dyn_template(world_, sarg(0));
        if (!t) {  // fall back to a static template of the same name
            const std::string *st = find_template(world_, sarg(0));
            return vstr(st ? *st : std::string());
        }
        Context tc;
        if (n == 2) {
            // One trailing arg binds "object" (an element) or "text" (a string).
            tc.locals[arg(1).type == Value::Type::ObjectRef ? "object" : "text"] =
                arg(1);
        } else {
            for (size_t i = 1; i < n; ++i)
                tc.locals["object" + std::to_string(i)] = arg(i);
        }
        return vstr(to_string(eval(*t, tc)));
    }

    // ---- misc string ------------------------------------------------------
    if (fn == "capfirst" && n == 1) {
        return vstr(utf8_case(sarg(0), true, true));
    }
    if (fn == "safexml" && n == 1) {
        return vstr(safe_xml_escape(sarg(0)));
    }

    // ---- lists ------------------------------------------------------------
    if ((fn == "newlist" || fn == "newstringlist") && n == 0) return vstrlist({});
    if (fn == "newobjectlist" && n == 0) return vobjlist({});
    if (fn == "listcount" && n == 1) {
        // QuestViva's ListCount takes any ICollection -- dictionaries count
        // too (Hawk's finale checks ListCount(game.score_achievements) > 0 on
        // a dictionary before printing the achievements header).
        if (arg(0).type == Value::Type::StringDict ||
            arg(0).type == Value::Type::ObjectDict ||
            arg(0).type == Value::Type::ScriptDict)
            return vint((long)arg(0).dict().size());
        return vint((long)arg(0).list().size());
    }
    if (fn == "listitem" && n == 2) {
        const Value &l = arg(0);
        long i = iarg(1);
        if (i < 0 || (size_t)i >= l.list().size()) return vnull();
        return l.list()[i];  // entries are typed
    }
    if (fn == "listcontains" && n == 2) {
        for (const Value &e : arg(0).list())
            if (values_equal(e, arg(1))) return vbool(true);
        return vbool(false);
    }
    if (fn == "listexclude" && n == 2) {
        // QuestList.Exclude filters ALL matching entries into a new list; the
        // second arg may be a single element or a list of elements to exclude.
        Value out = arg(0);
        out.detach();  // return a new list; do not mutate the input's backing
        auto excluded = [&](const Value &e) {
            if (is_list(arg(1))) {
                for (const Value &x : arg(1).list())
                    if (values_equal(e, x)) return true;
                return false;
            }
            return values_equal(e, arg(1));
        };
        auto &v = out.list();
        v.erase(std::remove_if(v.begin(), v.end(), excluded), v.end());
        return out;
    }
    if (fn == "listcombine" && n == 2) {
        Value out = arg(0);
        out.detach();  // return a new list; do not mutate the input's backing
        for (const Value &e : arg(1).list()) out.list().push_back(e);
        return out;
    }
    // ObjectListSort(list, attr[, attr2...]): stably order an object list by one
    // or more attributes (ExpressionOwner.ObjectListSort -> OrderBy/ThenBy). A
    // missing attribute sorts as empty. Descending reverses the result.
    if ((fn == "objectlistsort" || fn == "objectlistsortdescending") && n >= 2) {
        Value out = arg(0);
        out.detach();
        std::vector<std::string> attrs;
        for (int i = 1; i < n; ++i) attrs.push_back(to_string(arg(i)));
        auto key_at = [&](const Value &el, size_t k) -> std::string {
            Element *e = world_.find(to_string(el));
            const Value *f = e ? resolve_field(e, attrs[k]) : nullptr;
            return f ? to_string(*f) : std::string();
        };
        std::stable_sort(out.list().begin(), out.list().end(),
                         [&](const Value &a, const Value &b) {
                             for (size_t k = 0; k < attrs.size(); ++k) {
                                 std::string ka = key_at(a, k), kb = key_at(b, k);
                                 if (ka != kb)
                                     return dotnet_string_less(ka, kb);
                             }
                             return false;
                         });
        if (fn == "objectlistsortdescending")
            std::reverse(out.list().begin(), out.list().end());
        return out;
    }
    if ((fn == "stringlistsort" || fn == "stringlistsortdescending") && n == 1) {
        Value out = arg(0);
        out.detach();
        std::stable_sort(out.list().begin(), out.list().end(),
                         [](const Value &a, const Value &b) {
                             return dotnet_string_less(to_string(a),
                                                       to_string(b));
                         });
        if (fn == "stringlistsortdescending")
            std::reverse(out.list().begin(), out.list().end());
        return out;
    }

    // ---- strings ----------------------------------------------------------
    if (fn == "lcase" && n == 1) return vstr(utf8_case(sarg(0), false, false));
    if (fn == "ucase" && n == 1) return vstr(utf8_case(sarg(0), true, false));
    if (fn == "left" && n == 2) {
        std::string s = sarg(0); long k = iarg(1);
        if (k < 0) k = 0;
        return vstr(s.substr(0, std::min((size_t)k, s.size())));
    }
    if (fn == "right" && n == 2) {
        std::string s = sarg(0); long k = iarg(1);
        if (k < 0) k = 0;
        size_t kk = std::min((size_t)k, s.size());
        return vstr(s.substr(s.size() - kk));
    }
    if (fn == "mid" && (n == 2 || n == 3)) {
        std::string s = sarg(0);
        long start = iarg(1);  // 1-based
        if (start < 1) start = 1;
        if ((size_t)start > s.size()) return vstr("");
        size_t from = (size_t)(start - 1);
        if (n == 3) {
            long len = iarg(2);
            if (len < 0) len = 0;
            return vstr(s.substr(from, (size_t)len));
        }
        return vstr(s.substr(from));
    }
    if (fn == "instr" && (n == 2 || n == 3)) {
        long start = 1; std::string s, find;
        if (n == 2) { s = sarg(0); find = sarg(1); }
        else { start = iarg(0); s = sarg(1); find = sarg(2); }
        if (start < 1) start = 1;
        if ((size_t)(start - 1) > s.size()) return vint(0);
        size_t pos = s.find(find, (size_t)(start - 1));
        return vint(pos == std::string::npos ? 0 : (long)pos + 1);
    }
    if (fn == "replace" && n == 3) {
        std::string s = sarg(0), a = sarg(1), b = sarg(2);
        if (!a.empty()) {
            size_t pos = 0;
            while ((pos = s.find(a, pos)) != std::string::npos) {
                s.replace(pos, a.size(), b);
                pos += b.size();
            }
        }
        return vstr(s);
    }
    if (fn == "trim" && n == 1) return vstr(rt_trim(sarg(0)));
    // ExpressionOwner.IsInt/IsDouble: int.TryParse / double.TryParse (decimal
    // point only, no exponent/thousands) on the string form.
    if (fn == "isint" && n == 1) {
        std::string s = rt_trim(sarg(0));
        size_t i = (s.size() && (s[0] == '-' || s[0] == '+')) ? 1 : 0;
        bool ok = i < s.size();
        for (size_t k = i; k < s.size(); ++k)
            if (!std::isdigit((unsigned char)s[k])) { ok = false; break; }
        return vbool(ok);
    }
    if (fn == "isdouble" && n == 1) {
        std::string s = rt_trim(sarg(0));
        size_t i = (s.size() && (s[0] == '-' || s[0] == '+')) ? 1 : 0;
        bool digits = false, dot = false, ok = i < s.size();
        for (size_t k = i; k < s.size() && ok; ++k) {
            if (std::isdigit((unsigned char)s[k])) digits = true;
            else if (s[k] == '.' && !dot) dot = true;
            else ok = false;
        }
        return vbool(ok && digits);
    }
    if ((fn == "lengthof") && n == 1) return vint((long)sarg(0).size());
    if (fn == "startswith" && n == 2) {
        std::string s = sarg(0), p = sarg(1);
        return vbool(s.compare(0, p.size(), p) == 0);
    }
    if (fn == "endswith" && n == 2) {
        std::string s = sarg(0), p = sarg(1);
        return vbool(s.size() >= p.size() &&
                     s.compare(s.size() - p.size(), p.size(), p) == 0);
    }
    if (fn == "split" && n == 2) {
        std::string s = sarg(0), delim = sarg(1);
        std::vector<std::string> parts;
        if (delim.empty()) { parts.push_back(s); }
        else {
            size_t pos = 0, next;
            while ((next = s.find(delim, pos)) != std::string::npos) {
                parts.push_back(s.substr(pos, next - pos));
                pos = next + delim.size();
            }
            parts.push_back(s.substr(pos));
        }
        return vstrlist(parts);
    }
    if (fn == "join" && n == 2) {
        std::string out, delim = sarg(1);
        const Value &l = arg(0);
        for (size_t i = 0; i < l.list().size(); ++i) {
            if (i) out += delim;
            out += to_string(l.list()[i]);
        }
        return vstr(out);
    }
    if ((fn == "cstr" || fn == "tostring") && n == 1) return vstr(sarg(0));
    if (fn == "isnumeric" && n == 1) {
        std::string s = rt_trim(sarg(0));
        if (s.empty()) return vbool(false);
        char *end = nullptr;
        std::strtod(s.c_str(), &end);
        return vbool(end && *end == 0);
    }

    // ---- regex (parser primitives) ----------------------------------------
    // IsRegexMatch/GetMatchStrength/Populate mirror QuestViva's Utility.*; the
    // optional 3rd arg is a RegexCache cacheID (the command name). See the
    // rewrite_dotnet_regex note at the top of aslx-runtime.cc.
    if (fn == "isregexmatch" && (n == 2 || n == 3)) {
        std::string cid = sarg(2);
        auto cr = compiled_regex(sarg(0), n == 3 ? &cid : nullptr);
        if (!cr->valid) return vbool(false);
        return vbool(std::regex_search(sarg(1), cr->re));
    }
    if (fn == "getmatchstrength" && (n == 2 || n == 3)) {
        std::string cid = sarg(2), input = sarg(1);
        auto cr = compiled_regex(sarg(0), n == 3 ? &cid : nullptr);
        std::smatch m;
        if (!cr->valid || !std::regex_search(input, m, cr->re)) {
            world_.errors.push_back("String '" + input +
                                    "' is not a match for Regex '" + sarg(0) + "'");
            return vint((long)input.size());
        }
        return vint((long)input.size() - named_group_len(*cr, m));
    }
    if (fn == "populate" && (n == 2 || n == 3)) {
        std::string cid = sarg(2), input = sarg(1);
        auto cr = compiled_regex(sarg(0), n == 3 ? &cid : nullptr);
        Value out; out.type = Value::Type::StringDict; out.dict();
        std::smatch m;
        if (!cr->valid || !std::regex_search(input, m, cr->re)) {
            world_.errors.push_back("String '" + input +
                                    "' is not a match for Regex '" + sarg(0) + "'");
            return out;
        }
        for (const auto &g : named_groups(*cr, m))
            out.dict().emplace_back(g.first, vstr(g.second));
        return out;
    }

    // ---- numbers ----------------------------------------------------------
    auto num_of = [&](const Value &v) -> double {
        if (v.type == Value::Type::String) return std::strtod(v.str.c_str(), nullptr);
        return as_double(v);
    };
    if (fn == "cint" && n == 1) return vint((long)std::llround(num_of(arg(0))));
    if (fn == "cdbl" && n == 1) return vdouble(num_of(arg(0)));
    // ToInt/ToDouble parse a string; ToString formats a number. (CInt/CDbl
    // coerce any value; ToInt/ToDouble are the string-parse forms.)
    if (fn == "toint" && n == 1) return vint((long)std::llround(num_of(arg(0))));
    if (fn == "todouble" && n == 1) return vdouble(num_of(arg(0)));
    if (fn == "tostring" && n == 1) return vstr(to_string(arg(0)));
    if (fn == "abs" && n == 1) {
        if (arg(0).type == Value::Type::Int) return vint(std::labs(arg(0).integer));
        return vdouble(std::fabs(as_double(arg(0))));
    }
    if (fn == "max" && n == 2)
        return as_double(arg(0)) >= as_double(arg(1)) ? arg(0) : arg(1);
    if (fn == "min" && n == 2)
        return as_double(arg(0)) <= as_double(arg(1)) ? arg(0) : arg(1);
    // Clone / ShallowClone (ExpressionOwner -> Element.Clone): copy the
    // element under a fresh "<root>N" name (GetUniqueElementName strips
    // trailing digits), copying the inherited-type stack and every own field
    // except `name` -- collections get their own backing (Fields.Clone runs
    // clone-on-set) -- and, for the deep form, recursively cloning children
    // into the copy. The clone lands in the same parent, at the end.
    if ((fn == "clone" || fn == "shallowclone") && n == 1) {
        if (arg(0).type == Value::Type::Null) throw_null("obj");
        Element *src = obj_of(arg(0));
        if (!src)
            error(std::string(fn == "clone" ? "Clone" : "ShallowClone") +
                  " function expected object parameter but was passed '" +
                  to_string(arg(0)) + "'");
        std::function<Element *(Element *, bool)> do_clone =
            [&](Element *from, bool deep) -> Element * {
            std::string root = from->name;
            while (!root.empty() &&
                   std::isdigit((unsigned char)root.back()))
                root.pop_back();
            std::string nm;
            int k = 0;
            do { nm = root + std::to_string(++k); } while (world_.find(nm));
            Element *e = world_.create_object(nm, "", from->elem_type);
            log_create(e);  // Element.Clone creates via ObjectFactory -- undoable
            e->inherits = from->inherits;  // the clone shares the type stack
            for (const auto &kv : from->fields) {
                if (kv.first == "name") continue;
                Value v = kv.second;
                v.detach();
                e->set_field(kv.first, v);
            }
            if (deep) {
                std::vector<Element *> kids;
                for (auto &up : world_.elements)
                    if (live(up) && parent_of(up.get()) == from)
                        kids.push_back(up.get());
                std::stable_sort(kids.begin(), kids.end(),
                                 [](const Element *a, const Element *b) {
                                     return a->sort_index < b->sort_index;
                                 });
                for (Element *kid : kids) {
                    Element *ck = do_clone(kid, true);
                    ck->set_field("parent", vobj(e->name));
                }
            }
            return e;
        };
        return vobj(do_clone(src, fn == "clone")->name);
    }

    // ExpressionOwner.ShowMenu -- the EXPRESSION form, which QuestViva AWAITS:
    // print the caption, get the selection from the synchronous host provider,
    // echo " - <display>", and return the key ("" on cancel).
    if (fn == "showmenu" && n == 3) {
        MenuData md;
        md.caption = sarg(0);
        md.allow_cancel = truthy(arg(2));
        const Value &options = arg(1);
        if (options.type == Value::Type::StringList) {
            for (const Value &e2 : options.list()) {
                std::string s2 = to_string(e2);
                md.options.emplace_back(s2, s2);
            }
        } else if (options.type == Value::Type::StringDict) {
            for (const auto &kv : options.dict())
                md.options.emplace_back(kv.first, to_string(kv.second));
        } else {
            error("ShowMenu cannot handle this options type");
        }
        if (!menu_provider)
            error("ShowMenu (expression form) requires a host menu provider");
        print_via_core(md.caption, ctx);
        std::string key;
        if (menu_provider(md, key)) {
            for (const auto &kv : md.options)
                if (kv.first == key) {
                    print_via_core(" - " + kv.second, ctx);
                    break;
                }
            return vstr(key);
        }
        return vstr("");  // cancelled: result ?? string.Empty
    }

    if (fn == "getrandomint" && n == 2)
        return vint(active_rng().between(iarg(0), iarg(1)));
    if (fn == "getrandomdouble" && n == 0)
        return vdouble(active_rng().next_double());

    handled = false;
    return vnull();
}

}  // namespace aslx
