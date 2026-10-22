#pragma once

#include "pfy/ast.hpp"
#include "pfy/error.hpp"

#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace pfy {

struct Value;
struct Array;
struct Object;
struct Function;
struct Native;

using ArrayPtr = std::shared_ptr<Array>;
using ObjectPtr = std::shared_ptr<Object>;
using FunctionPtr = std::shared_ptr<Function>;
using NativePtr = std::shared_ptr<Native>;

struct Value {
    using Storage = std::variant<std::monostate, bool, double, std::string, ArrayPtr, ObjectPtr,
                                 FunctionPtr, NativePtr>;

    Storage data;

    Value() = default;
    Value(std::nullptr_t) : data(std::monostate{}) {}
    Value(bool value) : data(value) {}
    Value(double value) : data(value) {}
    Value(int value) : data(static_cast<double>(value)) {}
    Value(std::string value) : data(std::move(value)) {}
    Value(const char* value) : data(std::string(value)) {}
    Value(ArrayPtr value) : data(std::move(value)) {}
    Value(ObjectPtr value) : data(std::move(value)) {}
    Value(FunctionPtr value) : data(std::move(value)) {}
    Value(NativePtr value) : data(std::move(value)) {}

    bool isNull() const { return std::holds_alternative<std::monostate>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isNumber() const { return std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<std::string>(data); }
    bool isArray() const { return std::holds_alternative<ArrayPtr>(data); }
    bool isObject() const { return std::holds_alternative<ObjectPtr>(data); }
    bool isFunction() const { return std::holds_alternative<FunctionPtr>(data); }
    bool isNative() const { return std::holds_alternative<NativePtr>(data); }

    bool asBool() const { return std::get<bool>(data); }
    double asNumber() const { return std::get<double>(data); }
    const std::string& asString() const { return std::get<std::string>(data); }
    std::string& asString() { return std::get<std::string>(data); }
    ArrayPtr asArray() const { return std::get<ArrayPtr>(data); }
    ObjectPtr asObject() const { return std::get<ObjectPtr>(data); }
    FunctionPtr asFunction() const { return std::get<FunctionPtr>(data); }
    NativePtr asNative() const { return std::get<NativePtr>(data); }
};

struct Array {
    std::vector<Value> items;
};

struct Object {
    std::map<std::string, Value> fields;
};

struct Env;

struct Function {
    NodePtr declaration;
    std::shared_ptr<Env> closure;
};

struct Native {
    std::string name;
    std::function<Value(class Runtime&, const std::vector<Value>&, const NodePtr&)> call;
};

struct Env {
    std::map<std::string, Value> values;
    std::shared_ptr<Env> parent;

    explicit Env(std::shared_ptr<Env> parentEnv = nullptr) : parent(std::move(parentEnv)) {}
    bool get(const std::string& name, Value& out) const;
    bool assign(const std::string& name, const Value& value);
    void define(const std::string& name, Value value);
};

class RuntimeError : public Error {
public:
    RuntimeError(std::string message, int line, int column)
        : Error(std::move(message), line, column) {}
};

// Executes a parsed program. The module object contains every top-level binding
// except the built-in functions; the CLI prints it with --json.
class Runtime {
public:
    explicit Runtime(std::ostream& output);

    Value run(const NodePtr& program);
    Value module() const;
    std::string stringify(const Value& value) const;
    std::string json(const Value& value) const;
    void print(const std::string& text) { output_ << text; }

private:
    std::ostream& output_;
    std::shared_ptr<Env> globals_;
    std::map<std::string, bool> builtinNames_;
    int callDepth_ = 0;
    std::size_t steps_ = 0;

    void installBuiltins();
    void step(const NodePtr& node);
    void execute(const NodePtr& statement, const std::shared_ptr<Env>& env);
    void executeStatements(const NodePtr& block, const std::shared_ptr<Env>& env);
    Value evaluate(const NodePtr& expression, const std::shared_ptr<Env>& env);
    Value call(const Value& callee, const std::vector<Value>& args, const NodePtr& node);
    Value callFunction(const FunctionPtr& function, const std::vector<Value>& args, const NodePtr& node);

    bool truthy(const Value& value) const;
    bool equal(const Value& left, const Value& right) const;
    double number(const Value& value, const NodePtr& node, const char* context) const;
    std::string typeName(const Value& value) const;
    std::string utf8At(const std::string& text, std::size_t index) const;
    std::size_t utf8Length(const std::string& text) const;
    std::string toJson(const Value& value, int depth, bool pretty) const;
    std::string escapeJson(const std::string& text) const;

    void assignTarget(const NodePtr& target, const Value& value, const std::shared_ptr<Env>& env);
};

} // namespace pfy
