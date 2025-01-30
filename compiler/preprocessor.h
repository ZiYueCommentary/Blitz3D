#ifndef EXPRESSION_EVALUATOR_H
#define EXPRESSION_EVALUATOR_H

#include <string>
#include <map>

struct PreprocessorOps {
    std::string value;
    enum Type { NUMBER, BOOLEAN } type;

    PreprocessorOps(const std::string& v, Type t);

    static PreprocessorOps fromNumber(const std::string& num);
    static PreprocessorOps fromBoolean(bool b);
    bool toBoolean() const;
};

bool isNumber(const std::string& s);

PreprocessorOps&& evaluateOperation(const PreprocessorOps& lhs, const PreprocessorOps& rhs, const std::string& op);

bool evaluateExpression(const std::string& expr);

PreprocessorOps&& processToken(const std::string& token);

#endif // EXPRESSION_EVALUATOR_H