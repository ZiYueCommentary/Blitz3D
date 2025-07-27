#include "libs.h"
#include "../MultiLang/MultiLang.h"
#include <Windows.h>

int bcc_ver;
int lnk_ver;
int run_ver;
int dbg_ver;

std::string home;
Linker* linkerLib;
Runtime* runtimeLib;

Module* runtimeModule;
Environ* runtimeEnviron;
std::vector<std::string> keyWords;
std::vector<UserFunc> userFuncs;

static HMODULE linkerHMOD, runtimeHMOD;

static Type* typeof(int c) {
	switch (c) {
	case '%':return Type::int_type;
	case '#':return Type::float_type;
	case '$':return Type::string_type;
	}
	return Type::void_type;
}

static int curr;
static std::string text;

static int next(std::istream& in) {

	text = "";

	int t = 0;

	for (;;) {
		while (isspace(in.peek())) in.get();
		if (in.eof()) return curr = 0;
		t = in.get(); if (t != ';') break;
		while (!in.eof() && in.get() != '\n') {}
	}

	if (isalpha(t)) {
		text += (char)t;
		while (isalnum(in.peek()) || in.peek() == '_') text += (char)in.get();
		return curr = -1;
	}
	if (t == '\"') {
		while (in.peek() != '\"') text = text + (char)in.get();
		in.get();
		return curr = -2;
	}

	return curr = t;
}

static const char* linkRuntime() {

	while (const char* sym = runtimeLib->nextSym()) {

		std::string s(sym);

		int pc = runtimeLib->symValue(sym);

		//internal?
		if (s[0] == '_') {
			runtimeModule->addSymbol(("_" + s).c_str(), pc);
			continue;
		}

		bool cfunc = false;

		if (s[0] == '!') {
			cfunc = true;
			s = s.substr(1);
		}

		keyWords.push_back(s);

		//global!
		int start = 0, end, k;
		Type* t = Type::void_type;
		if (!isalpha(s[0])) { start = 1; t = typeof(s[0]); }
		for (k = 1; k < s.size(); ++k) {
			if (!isalnum(s[k]) && s[k] != '_') break;
		}
		end = k;
		DeclSeq* params = new DeclSeq();
		std::string n = s.substr(start, end - start);
		while (k < s.size()) {
			Type* t = typeof(s[k++]);
			int from = k;
			for (; isalnum(s[k]) || s[k] == '_'; ++k) {}
			std::string str = s.substr(from, k - from);
			ConstType* defType = 0;
			if (s[k] == '=') {
				int from = ++k;
				if (s[k] == '\"') {
					for (++k; s[k] != '\"'; ++k) {}
					std::string t = s.substr(from + 1, k - from - 1);
					defType = new ConstType(t); ++k;
				}
				else {
					if (s[k] == '-') ++k;
					for (; isdigit(s[k]); ++k) {}
					if (t == Type::int_type) {
						int n = atoi(s.substr(from, k - from));
						defType = new ConstType(n);
					}
					else {
						float n = atof(s.substr(from, k - from));
						defType = new ConstType(n);
					}
				}
			}
			Decl* d = params->insertDecl(str, t, DECL_PARAM, defType);
		}

		FuncType* f = new FuncType(t, params, false, cfunc);
		n = tolower(n);
		runtimeEnviron->funcDecls->insertDecl(n, f, DECL_FUNC);
		runtimeModule->addSymbol(("_f" + n).c_str(), pc);
	}
	return 0;
}

static std::set<std::string> _ulibkws;

static const char* loadUserLib(const std::string& userlib) {

	std::string t = home + "/userlibs/" + userlib;

	std::string lib = "";
	std::ifstream in(t.c_str());

	next(in);
	while (curr) {

		if (curr == '.') {

			if (next(in) != -1) return MultiLang::expect_identifier;

			if (text == "lib") {
				if (next(in) != -2) return MultiLang::expect_string_afrer_directive;
				lib = text;

			}
			else {
				return MultiLang::unknown_decl_directive;
			}
			next(in);

		}
		else if (curr == -1) {

			if (!lib.size()) return MultiLang::function_decl_without_directive;

			std::string id = text;
			std::string lower_id = tolower(id);

			if (_ulibkws.count(lower_id)) return MultiLang::duplicate_identifier;
			_ulibkws.insert(lower_id);

			Type* ty = 0;
			switch (next(in)) {
			case '%':ty = Type::int_type; break;
			case '#':ty = Type::float_type; break;
			case '$':ty = Type::string_type; break;
			}
			if (ty) next(in);
			else ty = Type::void_type;

			DeclSeq* params = new DeclSeq();

			if (curr != '(') return MultiLang::expect_left_bracket_after_function_identifier;
			next(in);
			if (curr != ')') {
				for (;;) {
					if (curr != -1) break;
					std::string arg = text;

					Type* ty = 0;
					switch (next(in)) {
					case '%':ty = Type::int_type; break;
					case '#':ty = Type::float_type; break;
					case '$':ty = Type::string_type; break;
					case '*':ty = Type::null_type; break;
					}
					if (ty) next(in);
					else ty = Type::int_type;

					ConstType* defType = 0;

					Decl* d = params->insertDecl(arg, ty, DECL_PARAM, defType);

					if (curr != ',') break;
					next(in);
				}
			}
			if (curr != ')') return MultiLang::expect_right_bracket_after_function_identifier;

			keyWords.push_back(id);

			FuncType* fn = new FuncType(ty, params, true, true);

			runtimeEnviron->funcDecls->insertDecl(lower_id, fn, DECL_FUNC);

			if (next(in) == ':') {	//real name?
				next(in);
				if (curr != -1 && curr != -2) return MultiLang::expect_identifier_or_string_after_alias;
				id = text;
				next(in);
			}

			userFuncs.push_back(UserFunc(lower_id, id, lib));
		}
		else {
			return MultiLang::missing_colon_userlib;
		}
	}
	return 0;
}

static const char* linkUserLibs() {

	_ulibkws.clear();

	WIN32_FIND_DATA fd;

	HANDLE h = FindFirstFile((home + "/userlibs/*.decls").c_str(), &fd);

	if (h == INVALID_HANDLE_VALUE) return 0;

	const char* err = 0;

	do {
		if (err = loadUserLib(fd.cFileName)) {
			static char buf[64];
			sprintf(buf, "Error in userlib '%s' - %s", fd.cFileName, err);
			err = buf; break;
		}

	} while (FindNextFile(h, &fd));

	FindClose(h);

	_ulibkws.clear();

	return err;
}

const char* openLibs() {

	char* p = getenv("blitzpath");
	if (!p) {
		char workingDir[128];
		GetCurrentDirectory(128, workingDir);
		home = workingDir; home += "\\\\..";
		_putenv_s("blitzpath", home.c_str());
	}
	else {
		home = std::string(p);
	}

	linkerHMOD = LoadLibrary((home + "/bin/linker.dll").c_str());
	if (!linkerHMOD) return MultiLang::unable_open_linker_dll;

	typedef Linker* (_cdecl* GetLinker)();
	GetLinker gl = (GetLinker)GetProcAddress(linkerHMOD, "linkerGetLinker");
	if (!gl) return MultiLang::error_in_linker_dll;
	linkerLib = gl();

	runtimeHMOD = LoadLibrary((home + "/bin/runtime.dll").c_str());
	if (!runtimeHMOD) return MultiLang::unable_open_runtime_dll;

	typedef Runtime* (_cdecl* GetRuntime)();
	GetRuntime gr = (GetRuntime)GetProcAddress(runtimeHMOD, "runtimeGetRuntime");
	if (!gr) return MultiLang::error_in_runtime_dll;
	runtimeLib = gr();

	bcc_ver = VERSION;
	lnk_ver = linkerLib->version();
	run_ver = runtimeLib->version();

	if ((lnk_ver >> 16) != (bcc_ver >> 16) ||
		(run_ver >> 16) != (bcc_ver >> 16) ||
		(lnk_ver >> 16) != (bcc_ver >> 16)) return MultiLang::library_version_error;

	runtimeLib->startup(GetModuleHandle(0));

	runtimeModule = linkerLib->createModule();
	runtimeEnviron = new Environ("", Type::int_type, 0, 0);

	keyWords.clear();
	userFuncs.clear();

	return 0;
}

const char* linkLibs() {

	if (const char* p = linkRuntime()) return p;

	if (const char* p = linkUserLibs()) return p;

	return 0;
}

void closeLibs() {

	delete runtimeEnviron;
	if (linkerLib) linkerLib->deleteModule(runtimeModule);
	if (runtimeLib) runtimeLib->shutdown();
	if (runtimeHMOD) FreeLibrary(runtimeHMOD);
	if (linkerHMOD) FreeLibrary(linkerHMOD);

	runtimeEnviron = 0;
	linkerLib = 0;
	runtimeLib = 0;
	runtimeHMOD = 0;
	linkerHMOD = 0;
}