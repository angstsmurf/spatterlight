/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "archetype.h"
#include "crypt.h"
#include "expression.h"
#include "heap_sort.h"
#include "misc.h"
#include "saveload.h"
#include "sys_object.h"
#include "timestamp.h"
#include "game_stat.h"

namespace Glk {
namespace Archetype {

#define MAX_INPUT_LINE 100

// Adapters that let save_game_state()/load_game_state() drive a Glk strid_t
// through the existing Common::ReadStream / Common::WriteStream interface.
namespace {

class GlkReadStream : public Common::ReadStream {
public:
    explicit GlkReadStream(strid_t s) : _stream(s), _eos(false) {}
    byte readByte() override {
        unsigned char b = 0;
        glui32 n = glk_get_buffer_stream(_stream, (char *)&b, 1);
        if (n == 0) {
            _eos = true;
            return 0;
        }
        return b;
    }
    uint32 read(void *buf, uint32 size) override {
        glui32 n = glk_get_buffer_stream(_stream, (char *)buf, size);
        if (n < size)
            _eos = true;
        return n;
    }
    bool eos() const override { return _eos; }

private:
    strid_t _stream;
    bool _eos;
};

class GlkWriteStream : public Common::WriteStream {
public:
    explicit GlkWriteStream(strid_t s) : _stream(s) {}
    void writeByte(byte b) override {
        unsigned char c = b;
        glk_put_buffer_stream(_stream, (char *)&c, 1);
    }
    uint32 write(const void *buf, uint32 size) override {
        glk_put_buffer_stream(_stream, (char *)buf, size);
        return size;
    }

private:
    strid_t _stream;
};

} // anonymous namespace

Archetype *g_vm;

Archetype::Archetype() :
    _saveSlot(-1), _mainWindow(nullptr), _shouldQuit(false),
    Abbreviate(0x7fffffff), NullStr(nullptr), Translating(false) {
    g_vm = this;
}

void Archetype::runGame() {
    initialize();
    interpret();
    deinitialize();
}

bool Archetype::initialize() {
    crypt_init();
    expression_init();
    heap_sort_init();
    misc_init();
    saveload_init();
    sys_object_init();
    timestamp_init();

    new_xarray(Literals);
    new_xarray(Vocabulary);

    new_xarray(Type_ID_List);
    new_xarray(Object_ID_List);
    new_xarray(Attribute_ID_List);

    Abbreviate = 0x7fffffff;
    new_list(Proximate);
    new_list(object_names);
    new_list(verb_names);

    new_xarray(Type_List);
    new_xarray(Object_List);
    NullStr = NewConstStr("null");

    _mainWindow = glk_window_open(nullptr, 0, 0, wintype_TextBuffer, 1);
    if (!_mainWindow)
        return false;
    glk_set_window(_mainWindow);

    return true;
}

void Archetype::deinitialize() {
    if (_mainWindow)
        glk_window_close(_mainWindow, nullptr);
}

Common::Error Archetype::saveGame() {
    frefid_t fdref = glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Write, 0);
    if (!fdref)
        return Common::Error(Common::kNoGameDataFoundError);

    strid_t fd = glk_stream_open_file(fdref, filemode_Write, 0);
    if (!fd) {
        glk_fileref_destroy(fdref);
        return Common::Error(Common::kNoGameDataFoundError);
    }

    GlkWriteStream stream(fd);
    save_game_state(&stream, Object_List);

    glk_stream_close(fd, nullptr);
    glk_fileref_destroy(fdref);
    return Common::Error(Common::kNoError);
}

Common::Error Archetype::loadGame() {
    frefid_t fdref = glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Read, 0);
    if (!fdref)
        return Common::Error(Common::kNoGameDataFoundError);

    strid_t fd = glk_stream_open_file(fdref, filemode_Read, 0);
    if (!fd) {
        glk_fileref_destroy(fdref);
        return Common::Error(Common::kNoGameDataFoundError);
    }

    GlkReadStream stream(fd);
    bool ok = load_game_state(&stream, Object_List);

    glk_stream_close(fd, nullptr);
    glk_fileref_destroy(fdref);
    return Common::Error(ok ? Common::kNoError : Common::kNoGameDataFoundError);
}

void Archetype::interpret() {
    Translating = false;
    bool success = load_game(&_gameFile);
    _gameFile.close();

    if (!success) {
        error("Could not load game");
        return;
    }

    ContextType context;
    ResultType result;

    undefine(result);

    if (!send_message(OP_SEND, find_message("START"), MainObject, result, context))
        error("Cannot execute; no ''START'' message for main object.");

    cleanup(result);
}

void Archetype::write_internal(const String *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Common::String s = Common::String::vformat(fmt->c_str(), ap);
    va_end(ap);

    _lastOutputText = s;

    if (!loadingSavegame())
        glk_put_buffer((char *)s.c_str(), (glui32)s.size());
}

void Archetype::writeln_internal(const String *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Common::String s = Common::String::vformat(fmt->c_str(), ap);
    va_end(ap);

    s += '\n';
    _lastOutputText = s;

    if (!loadingSavegame())
        glk_put_buffer((char *)s.c_str(), (glui32)s.size());
}

String Archetype::readLine() {
    // WORKAROUND inherited from ScummVM: the original Archetype games prompt
    // for save-file names from the game script before calling save/load. We
    // detect "save"/"load" in the preceding output and skip waiting for input.
    String text = _lastOutputText;
    text.toLowercase();
    if (text.contains("save") || text.contains("load")) {
        writeln();
        return "";
    } else if (loadingSavegame()) {
        return String("load");
    } else if (_saveSlot == -2) {
        _saveSlot = -1;
        return String("look");
    }

    event_t ev;
    char buffer[MAX_INPUT_LINE + 1];

    glk_request_line_event(_mainWindow, buffer, MAX_INPUT_LINE, 0);

    do {
        glk_select(&ev);
        if (ev.type == evtype_LineInput)
            break;
    } while (true);

    buffer[ev.val1] = 0;
    return String(buffer);
}

char Archetype::readKey() {
    glk_request_char_event(_mainWindow);

    event_t ev;
    ev.type = evtype_None;
    while (ev.type != evtype_CharInput) {
        glk_select(&ev);
    }
    return (char)ev.val1;
}

void Archetype::lookup(int the_obj, int the_attr, ResultType &result, ContextType &context,
        DesiredType desired) {
    NodePtr np;
    bool done, first_pass;
    ListType attrs;
    int parent;
    void *p, *original;
    ExprTree e;
    ContextType c;

    cleanup(result);

    if (desired == NAME) {
        result._kind = IDENT;
        result._data._ident.ident_kind = ATTRIBUTE_ID;
        result._data._ident.ident_int = the_attr;
        return;
    }

    if (the_obj == 0)
        return;

    if (!index_xarray(Object_List, the_obj, original)) {
        g_vm->writeln("Internal error: cannot reference object %d", the_obj);
        return;
    }

    if (original == nullptr)
        return;

    c = context;
    c.self = the_obj;
    c.each = 0;

    first_pass = true;
    p = original;
    done = false;

    do {
        ObjectType &obj = *((ObjectPtr)p);
        attrs = obj.attributes;
        parent = obj.inherited_from;

        np = find_item(attrs, the_attr);
        if (np != nullptr || parent == 0) {
            done = true;
        } else {
            if (!index_xarray(Type_List, parent, p)) {
                writeln("Internal error:  lookup cannot find parent type %d", parent);
                return;
            }

            first_pass = false;
        }
    } while (!done);

    if (np == nullptr)
        return;

    switch (desired) {
    case RVALUE:
        eval_expr((ExprPtr)np->data, result, c, RVALUE);
        break;

    case LVALUE:
        if (first_pass) {
            result._kind = ATTR_PTR;
            result._data._attr.acl_attr = np;
        } else {
            result._kind = ATTR_PTR;
            result._data._attr.acl_attr = new NodeType();

            e = new ExprNode();
            undefine(*e);
            eval_expr((ExprPtr)np->data, *e, c, RVALUE);

            result._data._attr.acl_attr->data = e;
            result._data._attr.acl_attr->key = the_attr;
            insert_item(((ObjectPtr)original)->attributes, result._data._attr.acl_attr);
        }
        break;

    default:
        break;
    }
}

static int messageCtr = 0;

bool Archetype::send_message(int transport, int message_sent, int recipient,
        ResultType &result, ContextType &context) {
    bool done, find_other;
    ObjectPtr op, original;
    ResultType r;
    NodePtr np;
    StatementPtr st;
    void *p;
    ContextType c;

    if (message_sent == 0) {
        cleanup(result);
        return false;
    }

    if (DebugMan.isDebugChannelEnabled(DEBUG_MSGS)) {
        ++messageCtr;

        r._kind = IDENT;
        r._data._ident.ident_kind = OBJECT_ID;
        r._data._ident.ident_int = context.self;

        debugN(" : ");
        display_result(r);

        if (transport == OP_SEND)
            debugN(" sending ");
        else
            debugN(" passing ");

        if (index_xarray(Vocabulary, message_sent, p)) {
            String str = String::format("'%s'", ((StringPtr)p)->c_str());
            debugN("%s", str.c_str());
        }

        if (transport == OP_SEND_TO_TYPE)
            r._data._ident.ident_kind = TYPE_ID;

        debugN(" to ");
        r._data._ident.ident_int = recipient;
        display_result(r);
        debug("%s", "");
    }

    if ((((transport == OP_SEND_TO_TYPE) && index_xarray(Type_List, recipient, p))
            || index_xarray(Object_List, recipient, p))
            && (p != nullptr)) {
        c = context;
        c.each = 0;
        c.message = message_sent;
        if (transport == OP_SEND) {
            c.sender = context.self;
            c.self = recipient;
        }

        op = (ObjectPtr)p;
        original = op;
        done = false;
        find_other = false;
        while (!done) {
            if (find_other) {
                st = op->other;
            } else {
                np = find_item(op->methods, message_sent);
                if (np != nullptr)
                    st = (StatementPtr)np->data;
                else
                    st = nullptr;
            }

            if (st != nullptr) {
                exec_stmt(st, result, c);
                return true;
            } else {
                if (op->inherited_from == 0) {
                    if (find_other) {
                        done = true;
                    } else {
                        op = original;
                        find_other = true;
                    }
                } else if (index_xarray(Type_List, op->inherited_from, p)) {
                    op = (ObjectPtr)p;
                } else {
                    error("Internal error:  invalid inheritance");
                    return false;
                }
            }
        }
    }

    result._kind = RESERVED;
    result._data._reserved.keyword = RW_ABSENT;

    return false;
}

void Archetype::eval_expr(ExprTree the_expr, ResultType &result, ContextType &context, DesiredType desired) {
    ResultType r1, r2;
    int i;
    ExprTree e;

    undefine(r1);
    undefine(r2);

    cleanup(result);

    if (the_expr == nullptr)
        return;

    if (the_expr->_kind == IDENT && the_expr->_data._ident.ident_kind == ATTRIBUTE_ID)
        lookup(context.self, the_expr->_data._ident.ident_int, result, context, desired);

    else if (the_expr->_kind == RESERVED) {
        switch (the_expr->_data._reserved.keyword) {
        case RW_READ:
        case RW_KEY:
            result._kind = STR_PTR;
            if (the_expr->_data._reserved.keyword == RW_READ)
                result._data._str.acl_str = MakeNewDynStr(readLine());
            else {
                String s;
                s += readKey();
                result._data._str.acl_str = MakeNewDynStr(s);
            }
            break;

        case RW_MESSAGE:
            result._kind = MESSAGE;
            result._data._msgTextQuote.index = context.message;
            break;

        case RW_EACH:
        case RW_SELF:
        case RW_SENDER:
            result._kind = IDENT;
            result._data._ident.ident_kind = OBJECT_ID;

            switch (the_expr->_data._reserved.keyword) {
            case RW_EACH:
                result._data._ident.ident_int = context.each;
                break;
            case RW_SELF:
                result._data._ident.ident_int = context.self;
                break;
            case RW_SENDER:
                result._data._ident.ident_int = context.sender;
                break;
            default:
                break;
            }
            break;

        default:
            result = *the_expr;
            break;
        }
    } else if (the_expr->_kind == OPER) {
        switch (the_expr->_data._oper.op_name) {
        case OP_SEND:
        case OP_PASS:
            eval_expr(the_expr->_data._oper.left, r1, context, RVALUE);
            eval_expr(the_expr->_data._oper.right, r2, context, RVALUE);

            if (r2._kind == IDENT && (r2._data._ident.ident_kind == OBJECT_ID || r2._data._ident.ident_kind == TYPE_ID)) {
                if (r2._data._ident.ident_kind == OBJECT_ID && r2._data._ident.ident_int == 0) {
                    if (convert_to(STR_PTR, r1))
                        send_to_system(the_expr->_data._oper.op_name, *r1._data._str.acl_str, result, context);

                } else if (convert_to(MESSAGE, r1)) {
                    if (r2._data._ident.ident_kind == TYPE_ID)
                        send_message(OP_SEND_TO_TYPE, r1._data._msgTextQuote.index, r2._data._ident.ident_int,
                            result, context);
                    else
                        send_message(the_expr->_data._oper.op_name, r1._data._msgTextQuote.index,
                            r2._data._ident.ident_int, result, context);
                }
            }
            break;

        case OP_DOT:
            eval_expr(the_expr->_data._oper.left, r1, context, RVALUE);

            if (r1._kind == IDENT && r1._data._ident.ident_kind == OBJECT_ID) {
                eval_expr(the_expr->_data._oper.right, r2, context, NAME);
                if (r2._kind == IDENT && r2._data._ident.ident_kind == ATTRIBUTE_ID)
                    lookup(r1._data._ident.ident_int, r2._data._ident.ident_int, result, context, desired);
            }
            break;

        case OP_ASSIGN:
            if (desired == NAME)
                return;

            eval_expr(the_expr->_data._oper.right, result, context, RVALUE);
            eval_expr(the_expr->_data._oper.left, r1, context, LVALUE);

            if (!assignment(r1, result))
                cleanup(result);
            else if (desired == LVALUE) {
                cleanup(result);
                result._kind = ATTR_PTR;
                result._data._attr.acl_attr = r1._data._attr.acl_attr;
            }
            break;

        case OP_C_MULTIPLY:
        case OP_C_DIVIDE:
        case OP_C_PLUS:
        case OP_C_MINUS:
        case OP_C_CONCAT:
            if (desired == NAME)
                return;

            e = new ExprNode();
            *e = *the_expr;

            switch (the_expr->_data._oper.op_name) {
            case OP_C_MULTIPLY: e->_data._oper.op_name = OP_MULTIPLY; break;
            case OP_C_DIVIDE:   e->_data._oper.op_name = OP_DIVIDE;   break;
            case OP_C_PLUS:     e->_data._oper.op_name = OP_PLUS;     break;
            case OP_C_MINUS:    e->_data._oper.op_name = OP_MINUS;    break;
            case OP_C_CONCAT:   e->_data._oper.op_name = OP_CONCAT;   break;
            default: break;
            }

            eval_expr(e, r1, context, RVALUE);
            e->_data._oper.op_name = OP_ASSIGN;
            e->_data._oper.right = &r1;

            eval_expr(e, result, context, desired);
            delete e;
            break;

        case OP_CHS:
        case OP_NUMERIC:
            eval_expr(the_expr->_data._oper.right, result, context, RVALUE);
            if (!convert_to(NUMERIC, result))
                cleanup(result);
            else if (the_expr->_data._oper.op_name == OP_CHS)
                result._data._numeric.acl_int = -result._data._numeric.acl_int;
            break;

        case OP_STRING:
            eval_expr(the_expr->_data._oper.right, result, context, RVALUE);
            if (!convert_to(STR_PTR, result))
                cleanup(result);
            break;

        case OP_LENGTH:
            eval_expr(the_expr->_data._oper.right, r1, context, RVALUE);
            if (convert_to(STR_PTR, r1)) {
                result._kind = NUMERIC;
                result._data._numeric.acl_int = (int)r1._data._str.acl_str->size();
            }
            break;

        case OP_RANDOM:
            eval_expr(the_expr->_data._oper.right, result, context, RVALUE);
            if (result._kind == NUMERIC)
                result._data._numeric.acl_int = g_vm->getRandomNumber(result._data._numeric.acl_int - 1) + 1;
            else if (convert_to(STR_PTR, result)) {
                String &s = *result._data._str.acl_str;
                s = String(s[g_vm->getRandomNumber((int)s.size() - 1)]);
            }
            break;

        case OP_NOT:
            result._kind = RESERVED;
            if (eval_condition(the_expr->_data._oper.right, context))
                result._data._reserved.keyword = RW_FALSE;
            else
                result._data._reserved.keyword = RW_TRUE;
            break;

        case OP_PLUS:
        case OP_MINUS:
        case OP_MULTIPLY:
        case OP_DIVIDE:
            eval_expr(the_expr->_data._oper.left, r1, context, RVALUE);
            eval_expr(the_expr->_data._oper.right, r2, context, RVALUE);
            if (convert_to(NUMERIC, r1) && convert_to(NUMERIC, r2)) {
                result._kind = NUMERIC;
                switch (the_expr->_data._oper.op_name) {
                case OP_PLUS:
                    result._data._numeric.acl_int = r1._data._numeric.acl_int + r2._data._numeric.acl_int;
                    break;
                case OP_MINUS:
                    result._data._numeric.acl_int = r1._data._numeric.acl_int - r2._data._numeric.acl_int;
                    break;
                case OP_MULTIPLY:
                    result._data._numeric.acl_int = r1._data._numeric.acl_int * r2._data._numeric.acl_int;
                    break;
                case OP_DIVIDE:
                    result._data._numeric.acl_int = r1._data._numeric.acl_int / r2._data._numeric.acl_int;
                    break;
                default: break;
                }
            }
            break;

        case OP_AND:
            result._kind = RESERVED;
            if (eval_condition(the_expr->_data._oper.left, context) && eval_condition(the_expr->_data._oper.right, context))
                result._data._reserved.keyword = RW_TRUE;
            else
                result._data._reserved.keyword = RW_FALSE;
            break;

        case OP_OR:
            if (eval_condition(the_expr->_data._oper.left, context) || eval_condition(the_expr->_data._oper.right, context))
                result._data._reserved.keyword = RW_TRUE;
            else
                result._data._reserved.keyword = RW_FALSE;
            break;

        case OP_POWER:
            eval_expr(the_expr->_data._oper.right, r2, context, RVALUE);
            eval_expr(the_expr->_data._oper.left, r1, context, RVALUE);
            if (convert_to(NUMERIC, r2) && convert_to(NUMERIC, r1)) {
                result._kind = NUMERIC;
                result._data._numeric.acl_int = 1;
                for (i = 1; i <= r2._data._numeric.acl_int; ++i)
                    result._data._numeric.acl_int *= r1._data._numeric.acl_int;
            }
            break;

        case OP_CONCAT:
            eval_expr(the_expr->_data._oper.left, r1, context, RVALUE);
            eval_expr(the_expr->_data._oper.right, r2, context, RVALUE);
            if (convert_to(STR_PTR, r1) && convert_to(STR_PTR, r2)) {
                result._kind = STR_PTR;
                result._data._str.acl_str = MakeNewDynStr(*r1._data._str.acl_str + *r2._data._str.acl_str);
            }
            break;

        case OP_LEFTFROM:
        case OP_RIGHTFROM:
            eval_expr(the_expr->_data._oper.left, r1, context, RVALUE);
            eval_expr(the_expr->_data._oper.right, r2, context, RVALUE);
            if (convert_to(STR_PTR, r1) && convert_to(NUMERIC, r2)) {
                result._kind = STR_PTR;
                if (the_expr->_data._oper.op_name == OP_LEFTFROM)
                    result._data._str.acl_str = MakeNewDynStr(r1._data._str.acl_str->left(r2._data._numeric.acl_int));
                else
                    result._data._str.acl_str = MakeNewDynStr(r1._data._str.acl_str->right(
                        (int)r1._data._str.acl_str->size() - r2._data._numeric.acl_int + 1));
            }
            break;

        case OP_WITHIN:
            eval_expr(the_expr->_data._oper.left, r1, context, RVALUE);
            eval_expr(the_expr->_data._oper.right, r2, context, RVALUE);
            if (convert_to(STR_PTR, r1) && convert_to(STR_PTR, r2)) {
                result._kind = NUMERIC;
                result._data._numeric.acl_int = r2._data._str.acl_str->indexOf(*r1._data._str.acl_str) + 1;
                if (result._data._numeric.acl_int == 0)
                    cleanup(result);
            }
            break;

        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_GT:
        case OP_LE:
        case OP_GE:
            eval_expr(the_expr->_data._oper.left, r1, context, RVALUE);
            eval_expr(the_expr->_data._oper.right, r2, context, RVALUE);

            result._kind = RESERVED;
            if (result_compare(the_expr->_data._oper.op_name, r1, r2))
                result._data._reserved.keyword = RW_TRUE;
            else
                result._data._reserved.keyword = RW_FALSE;
            break;

        default:
            g_vm->writeln("Internal error: \"%s\" not yet supported", Operators[the_expr->_data._oper.op_name]);
            break;
        }

        cleanup(r1);
        cleanup(r2);

        if (DebugMan.isDebugChannelEnabled(DEBUG_EXPR)) {
            debugN(" -- ");
            display_expr(the_expr);
            debugN("  ==>  ");
            display_result(result);
            debug("%s", "");
        }
    } else {
        switch (desired) {
        case RVALUE:
            copy_result(result, *the_expr);
            break;
        case LVALUE:
            result = *the_expr;
            break;
        default:
            break;
        }
    }
}

bool Archetype::eval_condition(ExprTree the_expr, ContextType &context) {
    ResultType result;
    bool failure;

    undefine(result);
    eval_expr(the_expr, result, context, RVALUE);

    failure = (result._kind == RESERVED) && (
        result._data._reserved.keyword == RW_UNDEFINED ||
        result._data._reserved.keyword == RW_FALSE ||
        result._data._reserved.keyword == RW_ABSENT
    );

    cleanup(result);
    return !failure;
}

void Archetype::exec_stmt(StatementPtr the_stmt, ResultType &result, ContextType &context) {
    NodePtr np;
    void *p, *q;
    ResultType r1, r2;
    CasePairPtr this_case;
    bool b;
    ContextType c;
    int i;
    ObjectPtr the_object;
    bool verbose;

    undefine(r1);
    undefine(r2);
    cleanup(result);

    verbose = DebugMan.isDebugChannelEnabled(DEBUG_STMT);

    if (verbose)
        debugN(" == ");

    switch (the_stmt->_kind) {
    case COMPOUND:
        np = nullptr;
        b = false;
        while (!b && iterate_list(the_stmt->_data._compound.statements, np)) {
            cleanup(result);
            exec_stmt((StatementPtr)np->data, result, context);

            b = (result._kind == RESERVED) && (result._data._reserved.keyword == RW_BREAK);

            if (shouldQuit())
                return;
        }
        break;

    case ST_EXPR:
        if (verbose)
            display_expr(the_stmt->_data._expr.expression);

        switch (the_stmt->_data._expr.expression->_kind) {
        case QUOTE_LIT:
            if (index_xarray(Literals, the_stmt->_data._expr.expression->_data._msgTextQuote.index, p)) {
                result._kind = TEXT_LIT;
                result._data._msgTextQuote.index = the_stmt->_data._expr.expression->_data._msgTextQuote.index;
                writeln(*((StringPtr)p));
            }
            break;

        case MESSAGE:
            b = send_message(OP_PASS, the_stmt->_data._expr.expression->_data._msgTextQuote.index,
                context.self, result, context);
            (void)b;
            break;

        default:
            eval_expr(the_stmt->_data._expr.expression, result, context, RVALUE);
            break;
        }
        break;

    case ST_WRITE:
    case ST_WRITES:
    case ST_STOP:
        if (verbose) {
            switch (the_stmt->_kind) {
            case ST_WRITE:  debugN("write ");  break;
            case ST_WRITES: debugN("writes "); break;
            case ST_STOP:   debugN("stop ");   break;
            default: break;
            }

            debugN(" ");
            np = nullptr;
            while (iterate_list(the_stmt->_data._write.print_list, np)) {
                display_expr((ExprTree)np->data);
                if (np->next != the_stmt->_data._write.print_list)
                    debugN(", ");
            }

            debug("%s", "");
        }

        np = nullptr;
        while (iterate_list(the_stmt->_data._write.print_list, np)) {
            cleanup(result);
            eval_expr((ExprTree)np->data, result, context, RVALUE);
            String line = get_result_string(result);
            g_vm->write("%s", line.c_str());
        }

        if (the_stmt->_kind == ST_WRITE) {
            g_vm->writeln();
        } else if (the_stmt->_kind == ST_STOP) {
            g_vm->writeln();
            g_vm->quitGame();
        }
        break;

    case ST_IF:
        if (verbose) {
            debugN("if: Testing ");
            display_expr(the_stmt->_data._if.condition);
        }
        if (eval_condition(the_stmt->_data._if.condition, context)) {
            if (verbose)
                debug(" Evaluated TRUE; executing then branch");
            exec_stmt(the_stmt->_data._if.then_branch, result, context);

        } else if (the_stmt->_data._if.else_branch != nullptr) {
            if (verbose)
                debug(" Evaluated FALSE; executing else branch");
            exec_stmt(the_stmt->_data._if.else_branch, result, context);
        }
        break;

    case ST_CASE:
        if (verbose) {
            debugN("case ");
            display_expr(the_stmt->_data._case.test_expr);
            debug(" of");
        }

        eval_expr(the_stmt->_data._case.test_expr, r1, context, RVALUE);
        np = nullptr;

        while (iterate_list(the_stmt->_data._case.cases, np)) {
            this_case = (CasePairPtr)np->data;

            eval_expr(this_case->value, r2, context, RVALUE);
            if ((r2._kind == RESERVED && r2._data._reserved.keyword == RW_DEFAULT)
                || result_compare(OP_EQ, r1, r2)) {
                exec_stmt(this_case->action, result, context);
                cleanup(r1);
                cleanup(r2);
                return;
            }

            cleanup(r2);
        }

        cleanup(result);
        cleanup(r1);
        break;

    case ST_BREAK:
        result._kind = RESERVED;
        result._data._reserved.keyword = RW_BREAK;
        break;

    case ST_FOR:
        b = false;
        c = context;
        c.each = 1;

        while (!b && c.each <= (int)Object_List.size()) {
            if (eval_condition(the_stmt->_data._loop.selection, c)) {
                exec_stmt(the_stmt->_data._loop.action, result, c);
                b = (result._kind == RESERVED) && (result._data._reserved.keyword == RW_BREAK);
                cleanup(result);
            }

            ++c.each;
        }
        break;

    case ST_WHILE:
        b = false;
        while (!b && eval_condition(the_stmt->_data._loop.selection, context)) {
            exec_stmt(the_stmt->_data._loop.action, result, context);
            b = (result._kind == RESERVED) && (result._data._reserved.keyword == RW_BREAK);
            cleanup(result);

            if (shouldQuit())
                return;
        }
        break;

    case ST_CREATE:
        eval_expr(the_stmt->_data._create.new_name, r1, context, LVALUE);

        result._kind = IDENT;
        result._data._ident.ident_kind = OBJECT_ID;
        result._data._ident.ident_int = 0;

        if (!assignment(r1, result)) {
            cleanup(result);
        } else {
            the_object = new ObjectType();
            the_object->inherited_from = the_stmt->_data._create.archetype;
            new_list(the_object->attributes);
            new_list(the_object->methods);
            the_object->other = nullptr;
            p = the_object;

            i = Dynamic;
            b = true;
            while (access_xarray(Object_List, i, q, PEEK_ACCESS) && q != nullptr)
                ++i;

            if (i > (int)Object_List.size())
                append_to_xarray(Object_List, p);
            else
                b = access_xarray(Object_List, i, p, POKE_ACCESS);
            (void)b;

            ((ExprPtr)r1._data._attr.acl_attr->data)->_data._ident.ident_int = i;
            copy_result(result, *(ExprPtr)r1._data._attr.acl_attr->data);

            cleanup(r1);
        }
        break;

    case ST_DESTROY:
        eval_expr(the_stmt->_data._destroy.victim, result, context, RVALUE);
        if (result._kind == IDENT && result._data._ident.ident_kind == OBJECT_ID
                && index_xarray(Object_List, result._data._ident.ident_int, p)) {
            the_object = (ObjectPtr)p;
            dispose_object(the_object);
            p = nullptr;
            b = access_xarray(Object_List, result._data._ident.ident_int, p, POKE_ACCESS);
            (void)b;
            if (result._data._ident.ident_int == (int)Object_List.size())
                shrink_xarray(Object_List);
        } else {
            error("Can only destroy previously created objects");
        }

        cleanup(result);
        break;

    default:
        error("Internal error:  statement not supported yet");
        break;
    }

    if (verbose)
        debug("%s", "");
}

} // End of namespace Archetype
} // End of namespace Glk
