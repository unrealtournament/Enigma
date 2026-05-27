/*
 * Copyright (C) 2002,2003,2004 Daniel Heck
 * Copyright (C) 2007,2008,2009 Ronald Lamprecht
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "Value.hh"

#include "errors.hh"
#include "enigma.hh"
#include "main.hh"
#include "Object.hh"
#include "world.hh"

#include <vector>

namespace enigma {
    
/* -------------------- Value implementation -------------------- */

    Value::Value() : type(NIL) {
    }

    Value::Value(const char* str) : type (STRING) {
        val.str = new char[strlen(str)+1];
        strcpy(val.str, str);
    }

    Value::Value(double d) : type(DOUBLE) {
        val.dval[0] = d;
    }

    Value::Value(int i) : type(DOUBLE) {
        val.dval[0] = i;
    }

    Value::Value(bool b) : type(BOOL) {
        val.dval[0] = b;
    }

    Value::Value(const Object *obj) : type (OBJECT) {
        if (obj != nullptr) {
            if (Value v = obj->getAttr("name");
                    v && v.type == STRING && strcmp(v.val.str, "") != 0) {
                val.str = new char[strlen(v.val.str)+1];
                strcpy(val.str, v.val.str);
                type = NAMEDOBJECT;
            } else
                val.dval[0] = obj->getId();
        } else
             val.dval[0] = 0;
    }

    Value::Value(const ObjectList &aList) : type (GROUP) {
        std::string descriptor;
        for (Object *obj : aList) {
            if (obj == nullptr) {
                descriptor.append("#0,");
            } else {
                Value v = obj->getAttr("name");
                if (v && v.type == STRING && strcmp(v.val.str, "") != 0) {
                    descriptor.append(v.toString());
                    descriptor.append(",");
                } else {
                    descriptor.append(ecl::strf("#%d,", obj->getId()));
                }
            }
        }
        val.str =  new char[descriptor.size() + 1];
        strcpy(val.str, descriptor.c_str());
//        Log << "Value ObjectList '" << descriptor << "'\n";
    }

    Value::Value(const TokenList& tokenList) : type(TOKENS) {
        std::string descriptor;
        for (auto& token : tokenList) {
            switch (token.type) {
                case STRING:
                case NAMEDOBJECT:
                    ASSERT(token.val.str[0] != 0, XLevelRuntime, "TokenList: illegal empty string value");
                    descriptor.append(token.val.str);
                    break;
                case OBJECT:
                    descriptor.append(ecl::strf("#%d", (int)(token.val.dval[0])));
                    break;
                case GROUP:
                    descriptor.append("%");
                    descriptor.append(token.val.str);
                    break;
                default:
                    ASSERT(false, XLevelRuntime, "TokenList: illegal value type");
                    break;
            }
            descriptor.append(";");
        }
        val.str =  new char[descriptor.size() + 1];
        strcpy(val.str, descriptor.c_str());
//        Log << "Value TokenList '" << descriptor << "'\n";
    }

    Value::Value(ecl::V2 pos) : type(POSITION) {
        val.dval[0] = pos[0];
        val.dval[1] = pos[1];
    }

    Value::Value(GridPos gpos) : type(GRIDPOS) {
        val.dval[0] = gpos.x;
        val.dval[1] = gpos.y;
    }

    Value::Value(Type t) : type(t) {
        switch (t) {
            case POSITION:
            case GRIDPOS:
                val.dval[0] = 0;
                val.dval[1] = 0;
                break;
            case DOUBLE:
            case BOOL:
            case OBJECT:
                val.dval[0] = 0;
                break;
            case STRING:
            case GROUP:
            case TOKENS:
                val.str = new char[1];
                val.str[0] = 0;
                break;
            case NAMEDOBJECT:
                ASSERT(false, XLevelRuntime, "Value: illegal type usage");
                break;
            case DEFAULT:
            case NIL:
                break;
        }
    }

    Value::~Value() {
        clear();
    }

    Value::Value(const std::string& str) : type(STRING) {
        val.str = new char[str.length()+1];
        strcpy(val.str, str.c_str());
    }

    Value::Value (const Value& other) : type(NIL) {
        this->operator=(other);
    }

    Value& Value::operator= (const Value& other) {
        if (this != &other) {
            switch (other.type) {
                case STRING:
                    assign(other.val.str);
                    break;
                case GROUP:
                    assign(other.val.str);
                    type = GROUP;
                    break;
                case TOKENS:
                    assign(other.val.str);
                    type = TOKENS;
                    break;
                case NAMEDOBJECT:
                    assign(other.val.str);
                    type = NAMEDOBJECT;
                    break;
                default:
                    clear();
                    type = other.type;
                    val = other.val;
            }
        }
        return *this;
    }

    bool Value::operator==(const Value& other) const {
        if (type != other.type)
            return false;
        switch (type) {
            case DOUBLE:
            case BOOL:
            case OBJECT: return val.dval[0] == other.val.dval[0];
            case STRING:
            case GROUP:
            case TOKENS:
            case NAMEDOBJECT: return strcmp(val.str, other.val.str) == 0;
            case POSITION:
            case GRIDPOS:
                return (val.dval[0] == other.val.dval[0]) && (val.dval[1] == other.val.dval[1]);
            case DEFAULT:
            case NIL:
                return true;
        }
        return false;
    }

    bool Value::operator!=(const Value& other) const {
        return !(*this == other);
    }

    bool Value::operator==(int i) const {
        return toInt() == i;
    }

    bool Value::operator!=(int i) const {
        return toInt() != i;
    }

    bool Value::operator<=(int i) const {
        return toInt() <= i;
    }

    bool Value::operator>=(int i) const {
        return toInt() >= i;
    }

    Value::operator bool() const {
        return !isDefault();
    }

    double Value::toDouble() const {
        switch (type) {
            case DOUBLE:
                return val.dval[0];
            case BOOL:
                return (val.dval[0] != 0) ? 1 : 0;
            case STRING:
                return atof(val.str);  // TODO use strtod and eval remaining part of string
            default:
                return 0.0;
        }
    }

    int Value::toInt() const {
        switch (type) {
            case DOUBLE:
                return ecl::round_nearest<int>(val.dval[0]);
            case BOOL:
                return val.dval[0] != 0 ? 1 : 0;
            case STRING:
                if (val.str[0] == '%')
                    return std::strtol(&val.str[1], nullptr, 0);
                else
                    return std::strtol(val.str, nullptr, 0);
            default: return 0;
        }
    }

    Object *Value::toObject() const {
        switch (type) {
            case OBJECT:
                return Object::getObject(ecl::round_nearest<int>(val.dval[0]));
            case NAMEDOBJECT:
            case STRING:
                return GetNamedObject(val.str);
            default:
                return nullptr;
        }
    }

    ObjectList Value::toObjectList() const {
        return getObjectList(nullptr);
    }

    TokenList Value::toTokenList() const {
        TokenList result;
        switch (type) {
            case OBJECT:
            case NAMEDOBJECT:
            case STRING:
            case GROUP:
            case POSITION:
            case GRIDPOS:
                result.push_back(*this);
                break;
            case TOKENS: {
                std::vector<std::string> vs;
                ecl::split_copy(std::string(val.str), ';', back_inserter(vs));
                for (auto & str : vs) {
                    if (str.empty())
                        continue;
                    if (str[0] == '#') {
                        // an object id
                        Value v(OBJECT);
                        v.val.dval[0] = atoi(str.c_str() + 1);
                        result.push_back(v);
                    } else if (str[0] == '%') {
                        // a group
                        Value v(NIL);
                        v.assign(str.c_str() + 1);
                        v.type = GROUP;
                        result.push_back(v);
                    } else {
                        // a string
                        result.push_back(Value(str));
                    }
                }
                break;
            }
            case DEFAULT:
            case NIL:
            case BOOL:
            case DOUBLE: break;
        }
        return result;
    }

    ecl::V2 Value::toVec() const {
        switch (type) {
            case POSITION:
            case GRIDPOS:
                return ecl::V2(val.dval[0], val.dval[1]);
            case NAMEDOBJECT:
            case STRING:
            case OBJECT:
                if (Object* obj = toObject()) {
                    switch (obj->getObjectType()) {
                        case Object::STONE:
                        case Object::FLOOR:
                        case Object::ITEM:
                            return dynamic_cast<GridObject*>(obj)->getOwnerPos().toVec();
                        case Object::ACTOR: return dynamic_cast<Actor*>(obj)->get_pos();
                        default: break;
                    }
                } else if (type != OBJECT) {
                    return GetNamedPosition(val.str).toVec();
                }
            default:
                break;
        }
        // all other cases
        return ecl::V2(-1, -1);
    }

    GridPos Value::toGridPos() const {
        return GridPos(toVec());
    }

    void Value::assign(const char* s) {
        clear();
        type = STRING;
        val.str = new char[strlen(s)+1];
        strcpy(val.str, s);
    }

    void Value::assign(double d) {
        clear();
        type = DOUBLE;
        val.dval[0] = d;
    }

    void Value::clear() {
        switch (type) {
            case STRING:
            case NAMEDOBJECT:
            case GROUP:
            case TOKENS:
               delete[] val.str;
               break;
        }
        type = NIL;
    }

    Value::Type Value::getType() const {
        if (type == NAMEDOBJECT)
            return OBJECT;
        return type;
    }

    double Value::getDouble() const {
        ASSERT(type == DOUBLE, XLevelRuntime, "get_double: type not double");
        return val.dval[0];
    }

    const char* Value::getString() const {
        ASSERT(type == STRING, XLevelRuntime, "get_string: type not string");
        return val.str;
    }

    bool Value::isDefault() const {
        return type == DEFAULT;
    }

    std::string Value::toString() const {
        switch (type) {
            case DOUBLE: {
                return ecl::strf("%g", val.dval[0]);  // need drop of trailing zeros and point for int
            }
            case STRING: return val.str;
            case NIL:
            case DEFAULT:
            default: return "";
        }
    }

    bool Value::toBool() const {
        switch (type) {
            case BOOL :
            case DOUBLE :
                return val.dval[0] != 0;
            case NIL :
            case DEFAULT :
                return false;
            default :
                return true;
        }
    }

    ecl::V2 Value::centeredPos() const {
        switch (type) {
            case POSITION:
                return ecl::V2(val.dval[0], val.dval[1]);
            case GRIDPOS:
                return ecl::V2(val.dval[0] + 0.5, val.dval[1] + 0.5);
            case NAMEDOBJECT:
            case STRING:
            case OBJECT:
                if (Object* obj = toObject(); obj != nullptr)
                    switch (obj->getObjectType()) {
                        case Object::STONE:
                        case Object::FLOOR:
                        case Object::ITEM:
                            return dynamic_cast<GridObject*>(obj)->getOwnerPos().center();
                        case Object::ACTOR: return dynamic_cast<Actor*>(obj)->get_pos();
                        default: break;
                    }
                else if (type != OBJECT) {
                    return GetNamedPosition(val.str).centeredPos();
                }
            default:
                // all other cases
                return ecl::V2(-1, -1);
        }
    }

    ObjectList Value::getObjectList(Object *reference) const {
        ObjectList result;
        switch (type) {
            case STRING:
                if (std::string(val.str).find_first_of("*?") != std::string::npos) {
                    // wildcards in object name - we need to add all objects
                    result = GetNamedGroup(val.str, reference);
                    break;
                } else if (std::string(val.str) == "@") {
                    // self-reference
                    result.push_back(reference);
                    break;
                }
                // otherwise it is a single object name - fall through
                [[fallthrough]];
            case NAMEDOBJECT:
            case OBJECT:
                result.push_back(toObject());
                break;
            case GROUP:
                std::vector<std::string> vs;
                ecl::split_copy(std::string(val.str), ',', back_inserter(vs));
                for (const std::string &name : vs) {
                    if (!name.empty()) {
                        if (name[0] == '#') {
                            result.push_back(Object::getObject(atoi(name.c_str() + 1)));
                        } else {
                            result.push_back(GetNamedObject(name));
                        }
                    }
                }
                break;
        }
        return result;
    }

    PositionList Value::getPositionList(Object *reference) const {
        PositionList result;
        switch (type) {
            case STRING:
                if (std::string(val.str).find_first_of("*?") != std::string::npos) {
                    // wildcards in object name - we need to add all objects
                    result = GetNamedPositionList(val.str, reference);
                    break;
                }
                [[fallthrough]];
            case NAMEDOBJECT:
            case OBJECT:
            case POSITION:
            case GRIDPOS:
                result.push_back(*this);
                break;
            case GROUP: {
                std::vector<std::string> vs;
                ecl::split_copy(std::string(val.str), ',', back_inserter(vs));
                for (const std::string &str : vs) {
                    if (str.empty())
                        continue;
                    if (str[0] == '#') {
                        result.push_back(Object::getObject(atoi(str.c_str() + 1)));
                    } else {
                        result.push_back(GetNamedPosition(str));
                    }
                }
                break;
            }
            default:
                break;
        }
        return result;
    }

    bool Value::finalizeNearestObjectReference(Object *reference) {
        if (type != STRING)
            return false;
        if (std::string str = toString(); str.find("@@") != 0 && str.find('@') == 0
                && str.find_first_of("*?") != std::string::npos) {
            ObjectList result = GetNamedGroup(val.str, reference);
            clear();
            if (!result.empty() && result.front() != nullptr) {
                if (Value v = result.front()->getAttr("name");
                        v && v.type == STRING && strcmp(v.val.str, "") != 0) {
                    val.str = new char[strlen(v.val.str)+1];
                    strcpy(val.str, v.val.str);
                    type = NAMEDOBJECT;
                    return true;
                }
            }
            // otherwise it resolves to no object
            type = OBJECT;
            val.dval[0] = 0;
            return true;
        }
        return false;
    }

    bool Value::maybeNearestObjectReference() const {
        if (type == STRING || type == NAMEDOBJECT || type == GROUP || type == TOKENS)
            return std::string(val.str).find('@') != std::string::npos;
        return false;
    }

    Direction to_direction (const Value &v) {
        int val = ecl::Clamp(v.toInt(), -1, 3);
        return static_cast<Direction>(val);
    }
    
} // namespace enigma
