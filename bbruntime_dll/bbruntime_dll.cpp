#pragma warning( disable:4786 )

#include "bbruntime_dll.h"
#include "../debugger/debugger.h"

#include <map>
#include <eh.h>
#include <float.h>

#include "../bbruntime/bbruntime.h"

#include "../gxruntime/gxutf8.h"

#include "../MultiLang/MultiLang.h"
#include "../bbruntime/bbsys.h"

class DummyDebugger : public Debugger {
public:
	virtual void debugRun() {}
	virtual void debugStop() {}// bbruntime_panic(0); }
	virtual bool debugStmt(int srcpos, const char* file) { return true; }
	virtual void debugEnter(void* frame, void* env, const char* func) {}
	virtual void debugLeave() {}
	virtual void debugLog(const char* msg) {}
	virtual void debugMsg(const char* e, bool serious) {
		if (serious) {
			MessageBoxW(gx_runtime->hwnd, UTF8::convertToUtf16(e).c_str(), MultiLang::runtime_error, MB_APPLMODAL);
		}
	}
	virtual void debugSys(void* msg) {}
	virtual void internalLog(const char* msg) {}
};

static HINSTANCE hinst;
static std::map<const char*, void*> syms;
std::map<const char*, void*>::iterator sym_it;
static gxRuntime* gx_runtime;

inline const char* getCharPtr(std::string str) {
	char* cha = new char[str.size() + 1];
	memcpy(cha, str.c_str(), str.size() + 1);
	const char* p = cha;
	return p;
}

inline std::string replace_all(const std::string& string, const std::string& pattern, const std::string& newpat) {
	std::string str = string;
	const uint32_t nsize = newpat.size();
	const uint32_t psize = pattern.size();

	for (uint32_t pos = str.find(pattern, 0); pos != std::string::npos; pos = str.find(pattern, pos + nsize))
	{
		str.replace(pos, psize, newpat);
	}
	return str;
}

inline void throw_mav() {
	if (ErrorMessagePool::memoryAccessViolation == 0) {
		RTEX(MultiLang::memory_access_violation);
	}
	else {
		std::string s = "";
		for (int i = 0; i < ErrorMessagePool::size; i++) {
			if (!ErrorMessagePool::memoryAccessViolation[i].empty()) {
				s = s + ErrorMessagePool::memoryAccessViolation[i] + "\n";
			}
		}
		if (ErrorMessagePool::hasMacro) {
			s = replace_all(s, "_CaughtError_", std::format("{0}: {1}", errorfunc, errorlog));
			s = replace_all(s, "_AvailPhys_", to_string(gx_runtime->getAvailPhys()));
			s = replace_all(s, "_AvailVirtual_", to_string(gx_runtime->getAvailVirtual()));
		}
		RTEX(UTF8::convertToAnsi(s).c_str());
	}
}

static void rtSym(const char* sym, void* pc) {
	syms[sym] = pc;
	std::string symSpare = sym;
	if (sym[0] == '%' || sym[0] == '$' || sym[0] == '#') symSpare = symSpare.insert(1, "Blitz_");
	else symSpare = symSpare.insert(0, "Blitz_");
	syms[getCharPtr(symSpare)] = pc;
}

static void killer() {
	ExitProcess(-1);
}

static void _cdecl seTranslator(unsigned int u, EXCEPTION_POINTERS* pExp) {
	switch(u) {
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			bbruntime_panic(MultiLang::integer_divide_zero);
			break;
		case EXCEPTION_ACCESS_VIOLATION:
			throw_mav();
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			bbruntime_panic(MultiLang::illegal_instruction);
			break;
		case EXCEPTION_STACK_OVERFLOW:
			bbruntime_panic(MultiLang::stack_overflow);
			break;
		case EXCEPTION_INT_OVERFLOW:
			bbruntime_panic(MultiLang::integer_overflow);
			break;
		case EXCEPTION_FLT_OVERFLOW:
			bbruntime_panic(MultiLang::float_overflow);
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			bbruntime_panic(MultiLang::float_divide_zero);
			break;
	}
	bbruntime_panic(MultiLang::unknown_runtime_exception);
}

int Runtime::version() {
	return VERSION;
}

const char* Runtime::nextSym() {
	if(!syms.size()) {
		bbruntime_link(rtSym);
		sym_it = syms.begin();
	}
	if(sym_it == syms.end()) {
		syms.clear(); return 0;
	}
	return (sym_it++)->first;
}

int Runtime::symValue(const char* sym) {
	std::map<const char*, void*>::iterator it = syms.find(sym);
	if(it != syms.end()) return (int)it->second;
	return -1;
}

void Runtime::startup(HINSTANCE h) {
	hinst = h;
}

void Runtime::shutdown() {
	trackmem(false);
	syms.clear();
}

void Runtime::execute(void (*pc)(), const char* args, Debugger* dbg) {

	bool debug = !!dbg;

	static DummyDebugger dummydebug;

	if(!dbg) dbg = &dummydebug;

	trackmem(true);

#ifndef _DEBUG
	_se_translator_function old_trans = _set_se_translator(seTranslator);
	_control87(_RC_NEAR | _PC_24 | _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL, 0xfffff);
#endif

	//strip spaces from ends of args...
	std::string params = args;
	while(params.size() && params[0] == ' ') params = params.substr(1);
	while(params.size() && params[params.size() - 1] == ' ') params = params.substr(0, params.size() - 1);

	//Fix the issue of NTF Mod clipping outside monitor boundaries in "fullscreen" mode when you have the system scale set to
	//something different than 100%.
	SetProcessDPIAware();

	if(gx_runtime = gxRuntime::openRuntime(hinst, params, dbg)) {
		bbruntime_run(gx_runtime, pc, debug);

		gxRuntime* t = gx_runtime;
		gx_runtime = 0;
		gxRuntime::closeRuntime(t);
	}

#ifndef _DEBUG
	_control87(_CW_DEFAULT, 0xfffff);
	_set_se_translator(old_trans);
#endif
}

void Runtime::asyncStop() {
	if(gx_runtime) gx_runtime->asyncStop();
}

void Runtime::asyncRun() {
	if(gx_runtime) gx_runtime->asyncRun();
}

void Runtime::asyncEnd() {
	if(gx_runtime) gx_runtime->asyncEnd();
}

void Runtime::checkmem(std::streambuf* buf) {
	std::ostream out(buf);
	::checkmem(out);
}

Runtime* _cdecl runtimeGetRuntime() {
	static Runtime runtime;
	return &runtime;
}

/********************** BUTT UGLY DLL->EXE HOOK! *************************/

static void* module_pc;
static std::map<std::string, int> module_syms;
static std::map<std::string, int> runtime_syms;
static Runtime* runtime;

static void fail() {
	MessageBox(0, MultiLang::unable_run_module, 0, 0);
	ExitProcess(-1);
}

struct Sym {
	std::string name;
	int value;
};

static Sym getSym(void** p) {
	Sym sym;
	char* t = (char*)*p;
	while(char c = *t++) sym.name += c;
	sym.value = *(int*)t + (int)module_pc;
	*p = t + 4; return sym;
}

static int findSym(const std::string& t) {
	std::map<std::string, int>::iterator it;

	it = module_syms.find(t);
	if(it != module_syms.end()) return it->second;
	it = runtime_syms.find(t);
	if(it != runtime_syms.end()) return it->second;

	std::string err = std::format(MultiLang::cant_find_symbol, t);
	MessageBox(0, err.c_str(), 0, 0);
	ExitProcess(0);
	return 0;
}

static void link() {

	while(const char* sc = runtime->nextSym()) {

		std::string t(sc);

		if(t[0] == '_') {
			runtime_syms["_" + t] = runtime->symValue(sc);
			continue;
		}

		if(t[0] == '!') t = t.substr(1);

		if(!isalnum(t[0])) t = t.substr(1);

		for(int k = 0; k < t.size(); ++k) {
			if(isalnum(t[k]) || t[k] == '_') continue;
			t = t.substr(0, k); break;
		}

		runtime_syms["_f" + tolower(t)] = runtime->symValue(sc);
	}

	HRSRC hres = FindResource(0, MAKEINTRESOURCE(1111), RT_RCDATA); if(!hres) fail();
	HGLOBAL hglo = LoadResource(0, hres); if(!hglo) fail();
	void* p = LockResource(hglo); if(!p) fail();

	int sz = *(int*)p; p = (int*)p + 1;

	//replace malloc for service pack 2 Data Execution Prevention (DEP).
	module_pc = VirtualAlloc(0, sz, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	memcpy(module_pc, p, sz);
	p = (char*)p + sz;

	int k, cnt;

	cnt = *(int*)p; p = (int*)p + 1;
	for(k = 0; k < cnt; ++k) {
		Sym sym = getSym(&p);
		if(sym.value < (int)module_pc || sym.value >= (int)module_pc + sz) fail();
		module_syms[sym.name] = sym.value;
	}

	cnt = *(int*)p; p = (int*)p + 1;
	for(k = 0; k < cnt; ++k) {
		Sym sym = getSym(&p);
		int* pp = (int*)sym.value;
		int dest = findSym(sym.name);
		*pp += dest - (int)pp;
	}

	cnt = *(int*)p; p = (int*)p + 1;
	for(k = 0; k < cnt; ++k) {
		Sym sym = getSym(&p);
		int* pp = (int*)sym.value;
		int dest = findSym(sym.name);
		*pp += dest;
	}

	runtime_syms.clear();
	module_syms.clear();
}

extern "C" _declspec(dllexport) int _stdcall bbWinMain();
extern "C" BOOL _stdcall _DllMainCRTStartup(HANDLE, DWORD, LPVOID);

bool WINAPI DllMain(HANDLE module, DWORD reason, void* reserved) {
	return TRUE;
}

int __stdcall bbWinMain() {

	HINSTANCE inst = GetModuleHandle(0);

	_DllMainCRTStartup(inst, DLL_PROCESS_ATTACH, 0);

#ifdef BETA
	int ver = VERSION & 0x7fff;
	string t = std::format(MultiLang::created_with_beta, itoa(ver / 100), itoa(ver % 100));
	MessageBox(GetDesktopWindow(), t.c_str(), MultiLang::blitz3d_message, MB_OK);
#endif

	runtime = runtimeGetRuntime();
	runtime->startup(inst);

	link();

	//get cmd_line and params
	std::string cmd = GetCommandLine(), params;
	while(cmd.size() && cmd[0] == ' ') cmd = cmd.substr(1);
	if(cmd.find('\"') == 0) {
		int n = cmd.find('\"', 1);
		if(n != std::string::npos) {
			params = cmd.substr(n + 1);
			cmd = cmd.substr(1, n - 1);
		}
	}
	else {
		int n = cmd.find(' ');
		if(n != std::string::npos) {
			params = cmd.substr(n + 1);
			cmd = cmd.substr(0, n);
		}
	}

	runtime->execute((void(*)())module_pc, params.c_str(), 0);
	runtime->shutdown();

	_DllMainCRTStartup(inst, DLL_PROCESS_DETACH, 0);

	ExitProcess(0);
	return 0;
}