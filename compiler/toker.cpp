#include "std.h"
#include "toker.h"
#include "ex.h"
#include "preprocessor.h"
#include "../MultiLang/MultiLang.h"
#include <cctype>
#include <regex>
#include <chrono>
#include <functional>

int Toker::chars_toked;

std::map<std::string, std::string> MacroDefines;

static std::map<std::string, int> alphaTokes, lowerTokes;

static void makeKeywords()
{
    static bool made;
    if (made) return;

    alphaTokes["Dim"] = DIM;
    alphaTokes["Goto"] = GOTO;
    alphaTokes["Gosub"] = GOSUB;
    alphaTokes["Return"] = RETURN;
    alphaTokes["Exit"] = EXIT;
    alphaTokes["If"] = IF;
    alphaTokes["Then"] = THEN;
    alphaTokes["Else"] = ELSE;
    alphaTokes["EndIf"] = ENDIF;
    alphaTokes["End If"] = ENDIF;
    alphaTokes["ElseIf"] = ELSEIF;
    alphaTokes["Else If"] = ELSEIF;
    alphaTokes["While"] = WHILE;
    alphaTokes["Wend"] = WEND;
    alphaTokes["For"] = FOR;
    alphaTokes["To"] = TO;
    alphaTokes["Step"] = STEP;
    alphaTokes["Next"] = NEXT;
    alphaTokes["Function"] = FUNCTION;
    alphaTokes["End Function"] = ENDFUNCTION;
    alphaTokes["Type"] = TYPE;
    alphaTokes["End Type"] = ENDTYPE;
    alphaTokes["Each"] = EACH;
    alphaTokes["Local"] = LOCAL;
    alphaTokes["Global"] = GLOBAL;
    alphaTokes["Field"] = FIELD;
    alphaTokes["Const"] = BBCONST;
    alphaTokes["Select"] = SELECT;
    alphaTokes["Case"] = CASE;
    alphaTokes["Default"] = DEFAULT;
    alphaTokes["End Select"] = ENDSELECT;
    alphaTokes["Repeat"] = REPEAT;
    alphaTokes["Until"] = UNTIL;
    alphaTokes["Forever"] = FOREVER;
    alphaTokes["Data"] = DATA;
    alphaTokes["Read"] = READ;
    alphaTokes["Restore"] = RESTORE;
    alphaTokes["Abs"] = ABS;
    alphaTokes["Sgn"] = SGN;
    alphaTokes["Mod"] = MOD;
    alphaTokes["Pi"] = PI;
    alphaTokes["True"] = BBTRUE;
    alphaTokes["False"] = BBFALSE;
    alphaTokes["Int"] = BBINT;
    alphaTokes["Float"] = BBFLOAT;
    alphaTokes["Str"] = BBSTR;
    alphaTokes["Include"] = INCLUDE;

    alphaTokes["New"] = BBNEW;
    alphaTokes["Delete"] = BBDELETE;
    alphaTokes["First"] = FIRST;
    alphaTokes["Last"] = LAST;
    alphaTokes["Insert"] = INSERT;
    alphaTokes["Before"] = BEFORE;
    alphaTokes["After"] = AFTER;
    alphaTokes["Object"] = OBJECT;
    alphaTokes["Handle"] = BBHANDLE;

    alphaTokes["And"] = AND;
    alphaTokes["Or"] = OR;
    alphaTokes["Lor"] = LOR;
    alphaTokes["Xor"] = XOR;
    alphaTokes["Not"] = NOT;
    alphaTokes["Shl"] = SHL;
    alphaTokes["Shr"] = SHR;
    alphaTokes["Sar"] = SAR;

    alphaTokes["Null"] = NULLCONST;
    alphaTokes["Infinity"] = INFINITYCONST;
    alphaTokes["PowTwo"] = POWTWO;

#if BETA
    alphaTokes["Switch"] = SELECT;
    alphaTokes["End Switch"] = ENDSELECT;
    alphaTokes["Array"] = DIM;
    alphaTokes["Interger"] = BBINT;
    alphaTokes["Method"] = FUNCTION;
    alphaTokes["Fun"] = FUNCTION;
    alphaTokes["End Method"] = ENDFUNCTION;
    alphaTokes["EndFun"] = ENDFUNCTION;
    alphaTokes["End Fun"] = ENDFUNCTION;
    alphaTokes["EndType"] = ENDTYPE;
    alphaTokes["EndSwitch"] = ENDSELECT;
    alphaTokes["EndSel"] = ENDSELECT;
    alphaTokes["Final"] = BBCONST;
    alphaTokes["End While"] = WEND;
#endif

    std::map<std::string, int>::const_iterator it;
    for (it = alphaTokes.begin(); it != alphaTokes.end(); ++it)
    {
        lowerTokes[tolower(it->first)] = it->second;
    }
    made = true;
}

Toker::Toker(std::istream& in, bool debug) :in(in), curr_row(-1)
{
    MacroDefines["__DEBUG__"] = debug ? "true" : "false";
    MacroDefines["__VERSION__"] = itoa((VERSION & 0xffff) / 1000) + "." + itoa((VERSION & 0xffff) % 1000);
    makeKeywords();
    nextline();
}

std::map<std::string, int>& Toker::getKeywords()
{
    makeKeywords();
    return alphaTokes;
}

int Toker::pos()
{
    return ((curr_row) << 16) | (tokes[curr_toke].from);
}

int Toker::curr()
{
    return tokes[curr_toke].n;
}

std::string Toker::text()
{
    int from = tokes[curr_toke].from, to = tokes[curr_toke].to;
    return line.substr(from, to - from);
}

int Toker::lookAhead(int n)
{
    return tokes[curr_toke + n].n;
}

void Toker::nextline()
{
    static std::vector<ConditionalState> conditionalStack;
    static bool skipLine = false;
    static const int maxMacroDepth = 100;
    static int macroDepth = 0;

    ++curr_row;
    curr_toke = 0;
    tokes.clear();
    if (in.eof())
    {
        conditionalStack.clear();
        skipLine = false;

        line.resize(1); line[0] = EOF;
        tokes.push_back(Toke(EOF, 0, 1));
        return;
    }

    getline(in, line); line += '\n';
    chars_toked += line.size();

    // Test and strip leading whitespace's for preprocessor directives only
    size_t firstNonWhitespace = line.find_first_not_of(" \t");
    if (firstNonWhitespace != std::string::npos)
    {
        std::string strippedLine = line.substr(firstNonWhitespace);
        if (strippedLine.starts_with("#"))
        {
            line = strippedLine;
        }
    }

    if (line.starts_with("#")) // Only parse when a Preprocessor directive has been found with #
    {
        if (line.starts_with("#define"))
        {
            if (!skipLine)
            {
                std::string define = line.substr(8);
                std::string name, content;
                int sep = define.find(' ');
                if (sep != std::string::npos)
                {
                    name = define.substr(0, sep);
                    content = define.substr(sep + 1);
                    content.pop_back();
                }
                else
                {
                    name = define;
                    name.pop_back();
                    content = "1";
                }
                if (!isValidIdentifier(name)) throw Ex(std::format(MultiLang::invalid_macro_name, name, curr_row));
                if (MacroDefines.contains(name)) throw Ex(std::format(MultiLang::redefinition_of_macro, name, curr_row));
                MacroDefines[name] = content;
            }
            tokes.push_back(Toke('\n', 0, 1));
            return;
        }

        if (line.starts_with("#undef"))
        {
            if (!skipLine)
            {
                std::string name = line.substr(7);
                name.pop_back();
                if (!isValidIdentifier(name)) throw Ex(std::format(MultiLang::invalid_macro_name, name, curr_row));
                MacroDefines.erase(name);
            }
            tokes.push_back(Toke('\n', 0, 1));
            return;
        }

        if (line.starts_with("#ifdef")) {
            std::string name = line.substr(7);
            name.pop_back();
            bool condition = MacroDefines.contains(name);

            conditionalStack.push_back({ condition, condition });
            skipLine = !condition;

            tokes.push_back(Toke('\n', 0, 1));
            return;
        }

        if (line.starts_with("#ifndef")) {
            std::string name = line.substr(8);
            name.pop_back();
            bool condition = !MacroDefines.contains(name);

            conditionalStack.push_back({ condition, condition });
            skipLine = !condition;

            tokes.push_back(Toke('\n', 0, 1));
            return;
        }

        if (line.starts_with("#if_")) {
            std::string condition = line.substr(line.find_first_of(' ') + 1);
            condition.pop_back();

            for (const auto& def : MacroDefines) {
                size_t pos = 0;
                while ((pos = condition.find(def.first, pos)) != std::string::npos) {
                    condition.replace(pos, def.first.length(), def.second);
                    pos += def.second.length();
                }
            }

            try {
                bool result = evaluateExpression(condition);
                conditionalStack.push_back({ result, result });
                skipLine = !result;
            }
            catch (const std::exception& e) {
                throw Ex(std::format(MultiLang::error_evaluating_condition, e.what(), curr_row));
                conditionalStack.push_back({ false, false });
                skipLine = true;
            }

            tokes.push_back(Toke('\n', 0, 1));
            return;
        }

        if (line.starts_with("#elseif_")) {
            if (conditionalStack.empty()) {
                throw Ex(std::format(MultiLang::elseif_without_if, curr_row));
                skipLine = true;
                tokes.push_back(Toke('\n', 0, 1));
                return;
            }

            if (conditionalStack.back().condition_met) {
                skipLine = true;
                tokes.push_back(Toke('\n', 0, 1));
                return;
            }

            std::string condition = line.substr(line.find_first_of(' ') + 1);
            condition.pop_back();

            for (const auto& def : MacroDefines) {
                size_t pos = 0;
                while ((pos = condition.find(def.first, pos)) != std::string::npos) {
                    condition.replace(pos, def.first.length(), def.second);
                    pos += def.second.length();
                }
            }

            try {
                bool result = evaluateExpression(condition);
                conditionalStack.back().condition_state = result;
                conditionalStack.back().condition_met |= result;
                skipLine = !result;
            }
            catch (const std::exception& e) {
                throw Ex(std::format(MultiLang::error_evaluating_condition, e.what(), curr_row));
                skipLine = true;
            }

            tokes.push_back(Toke('\n', 0, 1));
            return;
        }

        if (line.starts_with("#else_")) {
            if (conditionalStack.empty()) {
                throw Ex(std::format(MultiLang::else_without_if_macro, curr_row));
                skipLine = true;
                tokes.push_back(Toke('\n', 0, 1));
                return;
            }

            bool shouldExecute = !conditionalStack.back().condition_met;
            conditionalStack.back().condition_state = shouldExecute;
            conditionalStack.back().condition_met |= shouldExecute;
            skipLine = !shouldExecute;

            tokes.push_back(Toke('\n', 0, 1));
            return;
        }

        if (line.starts_with("#endif_")) {
            if (!conditionalStack.empty()) {
                conditionalStack.pop_back();
                if (!conditionalStack.empty()) {
                    skipLine = !conditionalStack.back().condition_state;
                }
                else {
                    skipLine = false;
                }
            }
            tokes.push_back(Toke('\n', 0, 1));
            return;
        }

        if (line.starts_with("#error"))
        {
            if (!skipLine) {
                std::string errMsg = line.substr(7);
                errMsg.pop_back();
                throw Ex(std::format(MultiLang::error_macro, errMsg, curr_row));
                exit(1);
            }
        }
    }

    if (skipLine)
    {
        tokes.push_back(Toke('\n', 0, 1));
        return;
    }

    for (const auto& def : MacroDefines)
    {
        static auto replaceAll =
            [](const std::string_view& str, const std::map<std::string, std::string>& MacroDefines) {
            std::string result;
            result.reserve(str.size());
            size_t pos = 0;

            while (pos < str.size()) {
                if (str[pos] == '"') {
                    size_t endPos = str.find('"', pos + 1);
                    if (endPos == std::string::npos) break;
                    result.append(str.substr(pos, endPos - pos + 1));
                    pos = endPos + 1;
                    continue;
                }

                bool matched = false;
                for (const auto& [name, value] : MacroDefines) {
                    size_t matchPos = str.find(name, pos);
                    if (matchPos == pos) {
                        bool isStartValid = (matchPos == 0 || !isalnum(str[matchPos - 1]) && str[matchPos - 1] != '_');
                        bool isEndValid = (matchPos + name.size() == str.size() || !isalnum(str[matchPos + name.size()]) && str[matchPos + name.size()] != '_');
                        if (isStartValid && isEndValid) {
                            result.append(value);
                            pos += name.size();
                            matched = true;
                            break;
                        }
                    }
                }

                if (!matched) {
                    result.push_back(str[pos]);
                    ++pos;
                }
            }

            return result;
            };

        if (++macroDepth > maxMacroDepth)
        {
            throw Ex(std::format(MultiLang::macro_exceeded, curr_row));
            exit(1);
        }

        line = replaceAll(line, MacroDefines);

        --macroDepth;
    }

    static auto replaceAll =
        [](const std::string_view& str, const std::string_view& pattern, std::function<std::string && ()> newpat) {
        std::string result;
        result.reserve(str.size());

        size_t pos = 0;
        size_t prev_pos = 0;
        bool in_quotes = false;

        while (pos < str.size()) {
            if (str[pos] == '"') {
                in_quotes = !in_quotes;
            }

            if (!in_quotes && str.find(pattern, pos) == pos) {
                result.append(str.substr(prev_pos, pos - prev_pos));
                result.append(newpat());
                pos += pattern.size();
                prev_pos = pos;
            }
            else {
                ++pos;
            }
        }

        result.append(str.substr(prev_pos));
        return std::move(result);
        };

    line = replaceAll(line, "__DATE__", []() -> std::string&& {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream compilerDate;

        compilerDate << std::put_time(std::localtime(&in_time_t), "%b %d %Y");
        return std::move(compilerDate.str());
        });
    line = replaceAll(line, "__TIME__", []() -> std::string&& {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream compilerTime;

        compilerTime << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
        return std::move(compilerTime.str());
        });
    line = replaceAll(line, "__LINE__", [&]() -> std::string&& {
        return std::move(std::to_string(curr_row + 1));
        });

    for (int k = 0; k < line.size(); )
    {
        int c = line[k], from = k;
        if (c == '\n')
        {
            tokes.push_back(Toke(c, from, ++k));
            continue;
        }
        if (isspace(c)) { ++k; continue; }
        if (c == ';')
        {
            for (++k; line[k] != '\n'; ++k) {}
            continue;
        }
        if (c == '.' && isdigit(line[k + 1]))
        {
            for (k += 2; isdigit(line[k]); ++k) {}
            tokes.push_back(Toke(FLOATCONST, from, k));
            continue;
        }
        if (isdigit(c))
        {
            for (++k; isdigit(line[k]); ++k) {}
            if (line[k] == '.')
            {
                for (++k; isdigit(line[k]); ++k) {}
                tokes.push_back(Toke(FLOATCONST, from, k));
                continue;
            }
            tokes.push_back(Toke(INTCONST, from, k));
            continue;
        }
        if (c == '%' && (line[k + 1] == '0' || line[k + 1] == '1'))
        {
            for (k += 2; line[k] == '0' || line[k] == '1'; ++k) {}
            tokes.push_back(Toke(BINCONST, from, k));
            continue;
        }
        if (c == '$' && isxdigit(line[k + 1]))
        {
            for (k += 2; isxdigit(line[k]); ++k) {}
            tokes.push_back(Toke(HEXCONST, from, k));
            continue;
        }
        if (isalpha(c))
        {
            for (++k; isalnum(line[k]) || line[k] == '_'; ++k) {}

            std::string ident = tolower(line.substr(from, k - from));

            if (line[k] == ' ' && isalpha(line[k + 1]))
            {
                int t = k;
                for (t += 2; isalnum(line[t]) || line[t] == '_'; ++t) {}
                std::string s = tolower(line.substr(from, t - from));
                if (lowerTokes.find(s) != lowerTokes.end())
                {
                    k = t; ident = s;
                }
            }

            std::map<std::string, int>::iterator it = lowerTokes.find(ident);

            if (it == lowerTokes.end())
            {
                for (int n = from; n < k; ++n) line[n] = tolower(line[n]);
                tokes.push_back(Toke(IDENT, from, k));
                continue;
            }

            tokes.push_back(Toke(it->second, from, k));
            continue;
        }
        if (c == '\"')
        {
            for (++k; line[k] != '\"' && line[k] != '\n'; ++k) {}
            if (line[k] == '\"') ++k;
            tokes.push_back(Toke(STRINGCONST, from, k));
            continue;
        }
        int n = line[k + 1];
        if ((c == '<' && n == '>') || (c == '>' && n == '<') || (c == '!' && n == '='))
        {
            tokes.push_back(Toke(NE, from, k += 2));
            continue;
        }
        if ((c == '<' && n == '=') || (c == '=' && n == '<'))
        {
            tokes.push_back(Toke(LE, from, k += 2));
            continue;
        }
        if ((c == '>' && n == '=') || (c == '=' && n == '>'))
        {
            tokes.push_back(Toke(GE, from, k += 2));
            continue;
        }
        //Modern logical operators: &, |, !, !=
        if (c == '&') {
            if (n != ' ') line = line.insert(k, 1, ' ');
            tokes.push_back(Toke(AND, from, k += 2));
            continue;
        }
        if (c == '|' && n != '|') {
            if (n != ' ') line = line.insert(k, 1, ' ');
            tokes.push_back(Toke(OR, from, k += 2));
            continue;
        }
        if (c == '|' && n == '|') {
            if (line[k + 2] != ' ') line = line.insert(k + 2, 1, ' ');
            tokes.push_back(Toke(LOR, from, k += 2));
            continue;
        }
        if (c == '!') {
            if (n != ' ') line = line.insert(k, 1, ' ');
            tokes.push_back(Toke(NOT, from, k += 2));
            continue;
        }
        tokes.push_back(Toke(c, from, ++k));
    }
    if (!tokes.size()) exit(0);
}

int Toker::next()
{
    if (++curr_toke == tokes.size()) nextline();
    return curr();
}

bool Toker::isValidIdentifier(const std::string& str) {
    if (str.empty() || !isalpha(str[0])) return false;
    for (char c : str) {
        if (!isalnum(c) && c != '_' && c != '(' && c != ')') return false;
    }
    return true;
}