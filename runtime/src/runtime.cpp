#include "pfy/runtime.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace pfy {
namespace {

struct BreakSignal {};
struct ContinueSignal {};
struct ReturnSignal {
    Value value;
};

std::string numberText(double value) {
    if (!std::isfinite(value)) return "null";
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    return stream.str();
}

std::size_t utf8SequenceLength(unsigned char lead) {
    if ((lead & 0x80) == 0) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

} // namespace

bool Env::get(const std::string& name, Value& out) const {
    auto found = values.find(name);
    if (found != values.end()) {
        out = found->second;
        return true;
    }
    if (parent) return parent->get(name, out);
    return false;
}

bool Env::assign(const std::string& name, const Value& value) {
    auto found = values.find(name);
    if (found != values.end()) {
        found->second = value;
        return true;
    }
    if (parent) return parent->assign(name, value);
    return false;
}

void Env::define(const std::string& name, Value value) { values[name] = std::move(value); }

Runtime::Runtime(std::ostream& output) : output_(output), globals_(std::make_shared<Env>()) {
    installBuiltins();
}

void Runtime::installBuiltins() {
    auto add = [&](const std::string& name,
                   std::function<Value(Runtime&, const std::vector<Value>&, const NodePtr&)> call) {
        auto native = std::make_shared<Native>();
        native->name = name;
        native->call = std::move(call);
        globals_->define(name, Value(native));
        builtinNames_[name] = true;
    };

    add("print", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr&) {
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i) runtime.print(" ");
            runtime.print(runtime.stringify(args[i]));
        }
        runtime.print("\n");
        return Value();
    });

    add("len", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.size() != 1) throw RuntimeError("len expects one argument", node->line, node->column);
        const Value& value = args[0];
        if (value.isString()) return Value(static_cast<double>(runtime.utf8Length(value.asString())));
        if (value.isArray()) return Value(static_cast<double>(value.asArray()->items.size()));
        if (value.isObject()) return Value(static_cast<double>(value.asObject()->fields.size()));
        throw RuntimeError("len expects a string, array or object", node->line, node->column);
    });

    add("type", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.size() != 1) throw RuntimeError("type expects one argument", node->line, node->column);
        return Value(runtime.typeName(args[0]));
    });

    add("str", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.size() != 1) throw RuntimeError("str expects one argument", node->line, node->column);
        return Value(runtime.stringify(args[0]));
    });

    add("num", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.size() != 1) throw RuntimeError("num expects one argument", node->line, node->column);
        const Value& value = args[0];
        if (value.isNumber()) return value;
        if (value.isBool()) return Value(value.asBool() ? 1.0 : 0.0);
        if (value.isString()) {
            try {
                std::size_t used = 0;
                double parsed = std::stod(value.asString(), &used);
                if (used != value.asString().size()) throw std::invalid_argument("trailing input");
                return Value(parsed);
            } catch (...) {
                throw RuntimeError("num could not parse that string", node->line, node->column);
            }
        }
        (void)runtime;
        throw RuntimeError("num expects a number, boolean or string", node->line, node->column);
    });

    add("keys", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.size() != 1 || !args[0].isObject()) {
            throw RuntimeError("keys expects an object", node->line, node->column);
        }
        auto result = std::make_shared<Array>();
        for (const auto& field : args[0].asObject()->fields) result->items.emplace_back(field.first);
        (void)runtime;
        return Value(result);
    });

    add("has", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.size() != 2 || !args[0].isObject() || !args[1].isString()) {
            throw RuntimeError("has expects an object and a string key", node->line, node->column);
        }
        (void)runtime;
        return Value(args[0].asObject()->fields.find(args[1].asString()) !=
                     args[0].asObject()->fields.end());
    });

    add("push", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.size() != 2 || !args[0].isArray()) {
            throw RuntimeError("push expects an array and a value", node->line, node->column);
        }
        (void)runtime;
        args[0].asArray()->items.push_back(args[1]);
        return args[0];
    });

    add("range", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.empty() || args.size() > 3) {
            throw RuntimeError("range expects one to three numbers", node->line, node->column);
        }
        double start = 0.0;
        double end = 0.0;
        double stepValue = 1.0;
        if (args.size() == 1) {
            end = runtime.number(args[0], node, "range");
        } else {
            start = runtime.number(args[0], node, "range");
            end = runtime.number(args[1], node, "range");
            if (args.size() == 3) stepValue = runtime.number(args[2], node, "range");
        }
        if (stepValue == 0.0) throw RuntimeError("range step cannot be zero", node->line, node->column);
        auto result = std::make_shared<Array>();
        if ((stepValue > 0 && start > end) || (stepValue < 0 && start < end)) return Value(result);
        std::size_t guard = 0;
        for (double current = start; stepValue > 0 ? current < end : current > end; current += stepValue) {
            if (++guard > 1'000'000) throw RuntimeError("range is too large", node->line, node->column);
            result->items.emplace_back(current);
        }
        return Value(result);
    });

    add("json", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) {
        if (args.size() != 1) throw RuntimeError("json expects one argument", node->line, node->column);
        return Value(runtime.json(args[0]));
    });

    add("error", [](Runtime& runtime, const std::vector<Value>& args, const NodePtr& node) -> Value {
        std::string message;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i) message += " ";
            message += runtime.stringify(args[i]);
        }
        throw RuntimeError(message.empty() ? "error()" : message, node->line, node->column);
    });
}

void Runtime::step(const NodePtr& node) {
    if (++steps_ > 5'000'000) {
        throw RuntimeError("execution step limit exceeded", node->line, node->column);
    }
}

std::size_t Runtime::utf8Length(const std::string& text) const {
    std::size_t count = 0;
    for (unsigned char byte : text) {
        if ((byte & 0xC0) != 0x80) ++count;
    }
    return count;
}

std::string Runtime::utf8At(const std::string& text, std::size_t index) const {
    std::size_t current = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        unsigned char lead = static_cast<unsigned char>(text[offset]);
        std::size_t length = std::min<std::size_t>(utf8SequenceLength(lead), text.size() - offset);
        if (current == index) return text.substr(offset, length);
        offset += length;
        ++current;
    }
    return {};
}

std::string Runtime::typeName(const Value& value) const {
    if (value.isNull()) return "null";
    if (value.isBool()) return "bool";
    if (value.isNumber()) return "number";
    if (value.isString()) return "string";
    if (value.isArray()) return "array";
    if (value.isObject()) return "object";
    if (value.isFunction()) return "function";
    if (value.isNative()) return "native";
    return "unknown";
}

bool Runtime::truthy(const Value& value) const {
    if (value.isNull()) return false;
    if (value.isBool()) return value.asBool();
    if (value.isNumber()) return value.asNumber() != 0.0 && !std::isnan(value.asNumber());
    if (value.isString()) return !value.asString().empty();
    if (value.isArray()) return !value.asArray()->items.empty();
    if (value.isObject()) return !value.asObject()->fields.empty();
    return true;
}

double Runtime::number(const Value& value, const NodePtr& node, const char* context) const {
    if (!value.isNumber()) {
        throw RuntimeError(std::string(context) + " expects a number", node->line, node->column);
    }
    return value.asNumber();
}

bool Runtime::equal(const Value& left, const Value& right) const {
    struct ContainerPair {
        const void* left;
        const void* right;
        bool operator==(const ContainerPair& other) const {
            return left == other.left && right == other.right;
        }
    };
    struct ContainerPairHash {
        std::size_t operator()(const ContainerPair& pair) const {
            const auto leftHash = std::hash<const void*>{}(pair.left);
            const auto rightHash = std::hash<const void*>{}(pair.right);
            return leftHash ^ (rightHash + 0x9e3779b9 + (leftHash << 6) + (leftHash >> 2));
        }
    };

    std::vector<std::pair<Value, Value>> pending{{left, right}};
    std::unordered_set<ContainerPair, ContainerPairHash> compared;
    while (!pending.empty()) {
        auto values = std::move(pending.back());
        pending.pop_back();
        const Value& a = values.first;
        const Value& b = values.second;
        if (a.data.index() != b.data.index()) return false;
        if (a.isNull()) continue;
        if (a.isBool()) {
            if (a.asBool() != b.asBool()) return false;
        } else if (a.isNumber()) {
            if (a.asNumber() != b.asNumber()) return false;
        } else if (a.isString()) {
            if (a.asString() != b.asString()) return false;
        } else if (a.isFunction()) {
            if (a.asFunction() != b.asFunction()) return false;
        } else if (a.isNative()) {
            if (a.asNative() != b.asNative()) return false;
        } else if (a.isArray()) {
            auto leftArray = a.asArray();
            auto rightArray = b.asArray();
            if (leftArray == rightArray) continue;
            if (leftArray->items.size() != rightArray->items.size()) return false;
            if (!compared.insert({leftArray.get(), rightArray.get()}).second) continue;
            for (std::size_t i = 0; i < leftArray->items.size(); ++i) {
                pending.emplace_back(leftArray->items[i], rightArray->items[i]);
            }
        } else if (a.isObject()) {
            auto leftObject = a.asObject();
            auto rightObject = b.asObject();
            if (leftObject == rightObject) continue;
            if (leftObject->fields.size() != rightObject->fields.size()) return false;
            if (!compared.insert({leftObject.get(), rightObject.get()}).second) continue;
            auto leftIt = leftObject->fields.begin();
            auto rightIt = rightObject->fields.begin();
            for (; leftIt != leftObject->fields.end(); ++leftIt, ++rightIt) {
                if (leftIt->first != rightIt->first) return false;
                pending.emplace_back(leftIt->second, rightIt->second);
            }
        }
    }
    return true;
}

std::string Runtime::escapeJson(const std::string& text) const {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : text) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c) << std::dec << std::setfill(' ');
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    out << '"';
    return out.str();
}

std::string Runtime::toJson(const Value& value, int depth, bool pretty) const {
    if (depth > 64) return "\"<max-depth>\"";
    if (value.isNull()) return "null";
    if (value.isBool()) return value.asBool() ? "true" : "false";
    if (value.isNumber()) return numberText(value.asNumber());
    if (value.isString()) return escapeJson(value.asString());
    if (value.isFunction()) return escapeJson("<function " + value.asFunction()->declaration->text + ">");
    if (value.isNative()) return escapeJson("<native " + value.asNative()->name + ">");
    const std::string indent = pretty ? std::string((depth + 1) * 2, ' ') : std::string();
    const std::string closeIndent = pretty ? std::string(depth * 2, ' ') : std::string();
    if (value.isArray()) {
        const auto& items = value.asArray()->items;
        if (items.empty()) return "[]";
        std::ostringstream out;
        out << '[';
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i) out << ',';
            if (pretty) out << '\n' << indent;
            out << toJson(items[i], depth + 1, pretty);
        }
        if (pretty) out << '\n' << closeIndent;
        out << ']';
        return out.str();
    }
    if (value.isObject()) {
        const auto& fields = value.asObject()->fields;
        if (fields.empty()) return "{}";
        std::ostringstream out;
        out << '{';
        bool first = true;
        for (const auto& field : fields) {
            if (!first) out << ',';
            first = false;
            if (pretty) out << '\n' << indent;
            out << escapeJson(field.first) << (pretty ? ": " : ":") << toJson(field.second, depth + 1, pretty);
        }
        if (pretty) out << '\n' << closeIndent;
        out << '}';
        return out.str();
    }
    return "null";
}

std::string Runtime::stringify(const Value& value) const {
    if (value.isString()) return value.asString();
    return toJson(value, 0, false);
}

std::string Runtime::json(const Value& value) const { return toJson(value, 0, true); }

Value Runtime::module() const {
    auto object = std::make_shared<Object>();
    for (const auto& entry : globals_->values) {
        const bool isBuiltin = builtinNames_.find(entry.first) != builtinNames_.end() &&
                               entry.second.isNative() &&
                               entry.second.asNative()->name == entry.first;
        if (!isBuiltin) {
            object->fields.emplace(entry.first, entry.second);
        }
    }
    return Value(object);
}

Value Runtime::run(const NodePtr& program) {
    steps_ = 0;
    try {
        execute(program, globals_);
    } catch (const ReturnSignal&) {
        throw RuntimeError("return outside a function", program->line, program->column);
    } catch (const BreakSignal&) {
        throw RuntimeError("break outside a loop", program->line, program->column);
    } catch (const ContinueSignal&) {
        throw RuntimeError("continue outside a loop", program->line, program->column);
    }
    return module();
}

void Runtime::executeStatements(const NodePtr& block, const std::shared_ptr<Env>& env) {
    for (const auto& statement : block->children) {
        execute(statement, env);
    }
}

void Runtime::execute(const NodePtr& statement, const std::shared_ptr<Env>& env) {
    step(statement);
    switch (statement->kind) {
        case NodeKind::Program:
        case NodeKind::Block:
            executeStatements(statement, env);
            return;
        case NodeKind::Let: {
            env->define(statement->text, evaluate(statement->children[0], env));
            return;
        }
        case NodeKind::Set: {
            Value value = evaluate(statement->children[1], env);
            assignTarget(statement->children[0], value, env);
            return;
        }
        case NodeKind::Function: {
            auto function = std::make_shared<Function>();
            function->declaration = statement;
            function->closure = env;
            env->define(statement->text, Value(function));
            return;
        }
        case NodeKind::If: {
            if (truthy(evaluate(statement->children[0], env))) {
                execute(statement->children[1], std::make_shared<Env>(env));
            } else if (statement->children[2]->kind != NodeKind::Null) {
                execute(statement->children[2], std::make_shared<Env>(env));
            }
            return;
        }
        case NodeKind::While: {
            while (truthy(evaluate(statement->children[0], env))) {
                try {
                    execute(statement->children[1], std::make_shared<Env>(env));
                } catch (const ContinueSignal&) {
                    continue;
                } catch (const BreakSignal&) {
                    break;
                }
                step(statement);
            }
            return;
        }
        case NodeKind::For: {
            Value iterable = evaluate(statement->children[0], env);
            std::vector<Value> items;
            if (iterable.isArray()) {
                items = iterable.asArray()->items;
            } else if (iterable.isObject()) {
                for (const auto& field : iterable.asObject()->fields) items.emplace_back(field.first);
            } else if (iterable.isString()) {
                for (std::size_t i = 0; i < utf8Length(iterable.asString()); ++i) {
                    items.emplace_back(utf8At(iterable.asString(), i));
                }
            } else {
                throw RuntimeError("for expects an array, object or string", statement->line, statement->column);
            }
            for (const auto& item : items) {
                auto loopEnv = std::make_shared<Env>(env);
                loopEnv->define(statement->text, item);
                try {
                    execute(statement->children[1], loopEnv);
                } catch (const ContinueSignal&) {
                    continue;
                } catch (const BreakSignal&) {
                    break;
                }
                step(statement);
            }
            return;
        }
        case NodeKind::Return:
            throw ReturnSignal{evaluate(statement->children[0], env)};
        case NodeKind::Break:
            throw BreakSignal{};
        case NodeKind::Continue:
            throw ContinueSignal{};
        case NodeKind::Expression:
            evaluate(statement->children[0], env);
            return;
        default:
            throw RuntimeError("not a statement", statement->line, statement->column);
    }
}

void Runtime::assignTarget(const NodePtr& target, const Value& value, const std::shared_ptr<Env>& env) {
    if (target->kind == NodeKind::Identifier) {
        if (!env->assign(target->text, value)) globals_->define(target->text, value);
        return;
    }
    if (target->kind == NodeKind::Member) {
        Value object = evaluate(target->children[0], env);
        if (!object.isObject()) {
            throw RuntimeError("member assignment requires an object", target->line, target->column);
        }
        object.asObject()->fields[target->text] = value;
        return;
    }
    if (target->kind == NodeKind::Index) {
        Value object = evaluate(target->children[0], env);
        Value index = evaluate(target->children[1], env);
        if (object.isArray()) {
            if (!index.isNumber()) throw RuntimeError("array index must be a number", target->line, target->column);
            double raw = index.asNumber();
            if (raw < 0 || std::floor(raw) != raw) {
                throw RuntimeError("array index must be a non-negative integer", target->line, target->column);
            }
            auto& items = object.asArray()->items;
            std::size_t position = static_cast<std::size_t>(raw);
            if (position > items.size()) {
                throw RuntimeError("array index is out of range", target->line, target->column);
            }
            if (position == items.size()) items.push_back(value);
            else items[position] = value;
            return;
        }
        if (object.isObject()) {
            if (!index.isString()) throw RuntimeError("object key must be a string", target->line, target->column);
            object.asObject()->fields[index.asString()] = value;
            return;
        }
        throw RuntimeError("index assignment requires an array or object", target->line, target->column);
    }
    throw RuntimeError("invalid assignment target", target->line, target->column);
}

Value Runtime::evaluate(const NodePtr& expression, const std::shared_ptr<Env>& env) {
    step(expression);
    switch (expression->kind) {
        case NodeKind::Null: return Value();
        case NodeKind::Bool: return Value(expression->boolean);
        case NodeKind::Number: return Value(expression->number);
        case NodeKind::String: return Value(expression->text);
        case NodeKind::Identifier: {
            Value value;
            if (!env->get(expression->text, value)) {
                throw RuntimeError("undefined variable '" + expression->text + "'", expression->line,
                                   expression->column);
            }
            return value;
        }
        case NodeKind::Array: {
            auto array = std::make_shared<Array>();
            for (const auto& item : expression->children) array->items.push_back(evaluate(item, env));
            return Value(array);
        }
        case NodeKind::Object: {
            auto object = std::make_shared<Object>();
            for (const auto& field : expression->fields) {
                object->fields[field.first] = evaluate(field.second, env);
            }
            return Value(object);
        }
        case NodeKind::Unary: {
            if (expression->text == "!") return Value(!truthy(evaluate(expression->children[0], env)));
            double value = number(evaluate(expression->children[0], env), expression, "unary operator");
            return Value(expression->text == "-" ? -value : value);
        }
        case NodeKind::Binary: {
            if (expression->text == "&&") {
                if (!truthy(evaluate(expression->children[0], env))) return Value(false);
                return Value(truthy(evaluate(expression->children[1], env)));
            }
            if (expression->text == "||") {
                if (truthy(evaluate(expression->children[0], env))) return Value(true);
                return Value(truthy(evaluate(expression->children[1], env)));
            }
            Value left = evaluate(expression->children[0], env);
            Value right = evaluate(expression->children[1], env);
            const std::string& op = expression->text;
            if (op == "==") return Value(equal(left, right));
            if (op == "!=") return Value(!equal(left, right));
            if (op == "+") {
                if (left.isString() || right.isString()) {
                    return Value(stringify(left) + stringify(right));
                }
                if (left.isArray() && right.isArray()) {
                    auto result = std::make_shared<Array>();
                    result->items = left.asArray()->items;
                    result->items.insert(result->items.end(), right.asArray()->items.begin(),
                                         right.asArray()->items.end());
                    return Value(result);
                }
                return Value(number(left, expression, "operator +") + number(right, expression, "operator +"));
            }
            if (op == "-") return Value(number(left, expression, "operator -") - number(right, expression, "operator -"));
            if (op == "*") return Value(number(left, expression, "operator *") * number(right, expression, "operator *"));
            if (op == "/") {
                double denominator = number(right, expression, "operator /");
                if (denominator == 0.0) throw RuntimeError("division by zero", expression->line, expression->column);
                return Value(number(left, expression, "operator /") / denominator);
            }
            if (op == "%") {
                double denominator = number(right, expression, "operator %");
                if (denominator == 0.0) throw RuntimeError("division by zero", expression->line, expression->column);
                return Value(std::fmod(number(left, expression, "operator %"), denominator));
            }
            if (op == "<" || op == "<=" || op == ">" || op == ">=") {
                if (left.isString() && right.isString()) {
                    if (op == "<") return Value(left.asString() < right.asString());
                    if (op == "<=") return Value(left.asString() <= right.asString());
                    if (op == ">") return Value(left.asString() > right.asString());
                    return Value(left.asString() >= right.asString());
                }
                double a = number(left, expression, "comparison");
                double b = number(right, expression, "comparison");
                if (op == "<") return Value(a < b);
                if (op == "<=") return Value(a <= b);
                if (op == ">") return Value(a > b);
                return Value(a >= b);
            }
            throw RuntimeError("unknown operator '" + op + "'", expression->line, expression->column);
        }
        case NodeKind::Call: {
            Value callee = evaluate(expression->children[0], env);
            std::vector<Value> arguments;
            for (std::size_t i = 1; i < expression->children.size(); ++i) {
                arguments.push_back(evaluate(expression->children[i], env));
            }
            return call(callee, arguments, expression);
        }
        case NodeKind::Index: {
            Value object = evaluate(expression->children[0], env);
            Value index = evaluate(expression->children[1], env);
            if (object.isArray()) {
                double raw = number(index, expression, "array index");
                if (raw < 0 || std::floor(raw) != raw) {
                    throw RuntimeError("array index must be a non-negative integer", expression->line, expression->column);
                }
                auto& items = object.asArray()->items;
                std::size_t position = static_cast<std::size_t>(raw);
                if (position >= items.size()) throw RuntimeError("array index is out of range", expression->line, expression->column);
                return items[position];
            }
            if (object.isObject()) {
                if (!index.isString()) throw RuntimeError("object key must be a string", expression->line, expression->column);
                auto found = object.asObject()->fields.find(index.asString());
                if (found == object.asObject()->fields.end()) {
                    throw RuntimeError("missing object key '" + index.asString() + "'", expression->line, expression->column);
                }
                return found->second;
            }
            if (object.isString()) {
                double raw = number(index, expression, "string index");
                if (raw < 0 || std::floor(raw) != raw) {
                    throw RuntimeError("string index must be a non-negative integer", expression->line, expression->column);
                }
                std::size_t position = static_cast<std::size_t>(raw);
                if (position >= utf8Length(object.asString())) {
                    throw RuntimeError("string index is out of range", expression->line, expression->column);
                }
                return Value(utf8At(object.asString(), position));
            }
            throw RuntimeError("indexing requires an array, object or string", expression->line, expression->column);
        }
        case NodeKind::Member: {
            Value object = evaluate(expression->children[0], env);
            if (object.isObject()) {
                auto found = object.asObject()->fields.find(expression->text);
                if (found == object.asObject()->fields.end()) {
                    throw RuntimeError("missing object field '" + expression->text + "'", expression->line, expression->column);
                }
                return found->second;
            }
            if (object.isArray() && expression->text == "length") {
                return Value(static_cast<double>(object.asArray()->items.size()));
            }
            if (object.isString() && expression->text == "length") {
                return Value(static_cast<double>(utf8Length(object.asString())));
            }
            throw RuntimeError("member access requires an object", expression->line, expression->column);
        }
        default:
            throw RuntimeError("not an expression", expression->line, expression->column);
    }
}

Value Runtime::call(const Value& callee, const std::vector<Value>& args, const NodePtr& node) {
    if (callee.isFunction()) return callFunction(callee.asFunction(), args, node);
    if (callee.isNative()) return callee.asNative()->call(*this, args, node);
    throw RuntimeError("value is not callable", node->line, node->column);
}

Value Runtime::callFunction(const FunctionPtr& function, const std::vector<Value>& args, const NodePtr& node) {
    if (++callDepth_ > 128) {
        --callDepth_;
        throw RuntimeError("maximum call depth exceeded", node->line, node->column);
    }
    auto env = std::make_shared<Env>(function->closure);
    const auto& parameters = function->declaration->params;
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        env->define(parameters[i], i < args.size() ? args[i] : Value());
    }
    Value result;
    try {
        executeStatements(function->declaration->children[0], env);
    } catch (const ReturnSignal& returned) {
        result = returned.value;
    } catch (...) {
        --callDepth_;
        throw;
    }
    --callDepth_;
    return result;
}

} // namespace pfy
