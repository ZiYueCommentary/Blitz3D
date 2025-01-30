#include "preprocessor.h"
#include "ex.h"
#include "toker.h"
#include "../MultiLang/MultiLang.h"
#include <algorithm>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <format>

std::map<std::string, int> precedence = {
        {"||", 1},
        {"&&", 2},
        {"==", 3},
        {"!=", 3},
        {"<", 3},
        {"<=", 3},
        {">", 3},
        {">=", 3}
};

PreprocessorOps::PreprocessorOps(const std::string& v, Type t) : value(v), type(t) {}

PreprocessorOps PreprocessorOps::fromNumber(const std::string& num) {
    return PreprocessorOps(num, NUMBER);
}

PreprocessorOps PreprocessorOps::fromBoolean(bool b) {
    return PreprocessorOps(b ? "true" : "false", BOOLEAN);
}

bool PreprocessorOps::toBoolean() const {
    if (type == BOOLEAN) {
        return value == "true";
    }
    try {
        return std::stod(value) != 0;
    }
    catch (...) {
        return false;
    }
}

bool isNumber(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        return ::isdigit(c) || c == '.' || c == '-';
        });
}

PreprocessorOps&& evaluateOperation(const PreprocessorOps& lhs, const PreprocessorOps& rhs, const std::string& op) {
    if (op == "&&") {
        bool lhsValue = (lhs.type == PreprocessorOps::BOOLEAN) ? (lhs.value == "true") : (std::stod(lhs.value) != 0);
        bool rhsValue = (rhs.type == PreprocessorOps::BOOLEAN) ? (rhs.value == "true") : (std::stod(rhs.value) != 0);

        bool result = lhsValue && rhsValue;
        return std::move(PreprocessorOps::fromBoolean(result));
    }

    if (op == "||") {
        bool lhsValue = (lhs.type == PreprocessorOps::BOOLEAN) ? (lhs.value == "true") : (std::stod(lhs.value) != 0);
        bool rhsValue = (rhs.type == PreprocessorOps::BOOLEAN) ? (rhs.value == "true") : (std::stod(rhs.value) != 0);

        bool result = lhsValue || rhsValue;
        return std::move(PreprocessorOps::fromBoolean(result));
    }

    if (op == ">" || op == ">=" || op == "<" || op == "<=" || op == "==" || op == "!=") {
        double lhsNum = std::stod(lhs.value);
        double rhsNum = std::stod(rhs.value);
        bool result;

        if (op == ">") result = lhsNum > rhsNum;
        else if (op == ">=") result = lhsNum >= rhsNum;
        else if (op == "<") result = lhsNum < rhsNum;
        else if (op == "<=") result = lhsNum <= rhsNum;
        else if (op == "==") result = lhsNum == rhsNum;
        else result = lhsNum != rhsNum;

        return std::move(PreprocessorOps::fromBoolean(result));
    }

    throw Ex(std::format(MultiLang::unsupported_operator, op));
}

bool evaluateExpression(const std::string& expr) {
    std::istringstream tokens(expr);
    std::stack<PreprocessorOps> values;
    std::stack<std::string> operators;

    auto shouldEvaluate = [&](const std::string& op1) {
        if (operators.empty()) return false;
        if (operators.top() == "(") return false;
        return precedence[op1] <= precedence[operators.top()];
        };

    auto applyOperator = [&]() {
        if (values.size() < 2 || operators.empty()) return;

        PreprocessorOps rhs = values.top(); values.pop();
        PreprocessorOps lhs = values.top(); values.pop();
        std::string op = operators.top(); operators.pop();

        if (op == "&&") {
            bool lhsValue = lhs.toBoolean();
            bool rhsValue = rhs.toBoolean();
            bool result = lhsValue && rhsValue;
            values.push(PreprocessorOps::fromBoolean(result));
            return;
        }

        if (op == "||") {
            bool lhsValue = lhs.toBoolean();
            bool rhsValue = rhs.toBoolean();
            bool result = lhsValue || rhsValue;
            values.push(PreprocessorOps::fromBoolean(result));
            return;
        }

        if ((op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") &&
            isNumber(lhs.value) && isNumber(rhs.value)) {
            double lhsNum = std::stod(lhs.value);
            double rhsNum = std::stod(rhs.value);
            bool result;

            if (op == "==") result = lhsNum == rhsNum;
            else if (op == "!=") result = lhsNum != rhsNum;
            else if (op == "<") result = lhsNum < rhsNum;
            else if (op == "<=") result = lhsNum <= rhsNum;
            else if (op == ">") result = lhsNum > rhsNum;
            else result = lhsNum >= rhsNum;

            values.push(PreprocessorOps::fromBoolean(result));
            return;
        }

        if (!isNumber(lhs.value) || !isNumber(rhs.value)) {
            values.push(PreprocessorOps::fromBoolean(false));
            return;
        }

        PreprocessorOps result = evaluateOperation(lhs, rhs, op);
        values.push(result);
        };

    std::string token;
    bool expectingToken = true;

    while (tokens >> token) {
        if (token.starts_with("(")) {
            operators.push("(");
            if (token.length() > 1) {
                token = token.substr(1);
            }
            else {
                continue;
            }
        }

        if (token.find(')') != std::string::npos) {
            std::string value = token.substr(0, token.find(')'));
            if (!value.empty()) {
                values.push(processToken(value));
            }

            while (!operators.empty() && operators.top() != "(") {
                applyOperator();
            }

            if (!operators.empty() && operators.top() == "(") {
                operators.pop();
            }
            continue;
        }

        if (precedence.contains(token)) {
            while (shouldEvaluate(token)) {
                applyOperator();
            }
            operators.push(token);
            expectingToken = true;
            continue;
        }

        if (expectingToken) {
            values.push(processToken(token));
            expectingToken = false;
        }
    }

    while (!operators.empty()) {
        applyOperator();
    }

    if (values.empty()) {
        throw Ex(MultiLang::expression_evaluation_resulted_in_no_value);
    }

    PreprocessorOps tokenResult = values.top();
    return tokenResult.toBoolean();
}

PreprocessorOps&& processToken(const std::string& token)
{
    if (MacroDefines.contains(token)) {
        std::string value = MacroDefines.at(token);
        return isNumber(value) ? PreprocessorOps::fromNumber(value) : PreprocessorOps::fromBoolean(value == "true");
    }

    if (isNumber(token)) {
        return PreprocessorOps::fromNumber(token);
    }

    if (token == "true" || token == "false") {
        return PreprocessorOps::fromBoolean(token == "true");
    }

    return std::move(PreprocessorOps(token, PreprocessorOps::NUMBER));
}
