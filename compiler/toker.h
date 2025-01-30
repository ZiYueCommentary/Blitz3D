/*
  The Toker converts an inout stream into tokens for use by the parser.
*/

#ifndef TOKER_H
#define TOKER_H

#include <vector>

enum {
	DIM = 0x8000, GOTO, GOSUB, EXIT, RETURN,
	IF, THEN, ELSE, ENDIF, ELSEIF,
	WHILE, WEND,
	FOR, TO, STEP, NEXT,
	FUNCTION, ENDFUNCTION,
	TYPE, ENDTYPE, EACH,
	GLOBAL, LOCAL, FIELD, BBCONST,
	SELECT, CASE, DEFAULT, ENDSELECT,
	REPEAT, UNTIL, FOREVER,
	DATA, READ, RESTORE,
	ABS, SGN, MOD,
	PI, BBTRUE, BBFALSE,
	BBINT, BBFLOAT, BBSTR,
	INCLUDE,

	BBNEW, BBDELETE, FIRST, LAST, INSERT, BEFORE, AFTER,
	OBJECT, BBHANDLE,
	AND, OR, LOR, XOR, NOT, SHL, SHR, SAR,

	LE, GE, NE,
	IDENT, INTCONST, BINCONST, HEXCONST, FLOATCONST, STRINGCONST, NULLCONST, INFINITYCONST, POWTWO,
};

extern std::map<std::string, std::string> MacroDefines; // dangerous!

class Toker {
public:
	Toker(std::istream& in, bool debug);

	int pos();
	int curr();
	int next();
	std::string text();
	int lookAhead(int n);

	static int chars_toked;

	static std::map<std::string, int>& getKeywords();

private:
	struct Toke {
		int n, from, to;
		Toke(int n, int f, int t) :n(n), from(f), to(t) {}
	};

	struct ConditionalState {
		bool condition_met;
		bool condition_state;
	};

	std::istream& in;
	std::string line;
	std::vector<Toke> tokes;
	void nextline();
	bool isValidIdentifier(const std::string& str);
	int curr_row, curr_toke;
};

#endif