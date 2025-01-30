#include "std.h"
#include "bbsys.h"
#include "bbruntime.h"
#include "../gxruntime/gxutf8.h"
#include "../MultiLang/MultiLang.h"
#include <codecvt>

std::string* ErrorMessagePool::memoryAccessViolation = 0;
int ErrorMessagePool::size = 0;
bool ErrorMessagePool::hasMacro = false;

void bbEnd() {
    RTEX(0);
}

void bbStop() {
    gx_runtime->debugStop();
    if (!gx_runtime->idle()) RTEX(0);
}

void bbDisableClose() {
    HMENU hmenu = GetSystemMenu(gx_runtime->hwnd, false);
    RemoveMenu(hmenu, SC_CLOSE, MF_BYCOMMAND);
}

void bbAppTitle(BBStr* ti, BBStr* cp) {
    gx_runtime->setTitle(*ti, *cp);
    delete ti; delete cp;
}

void bbRuntimeError(BBStr* str) {
    std::string t = *str; delete str;
    if (t.size() > 255) t[255] = 0;
    static char err[256];
    strcpy(err, t.c_str());
    RTEX(UTF8::convertToAnsi(err).c_str());
}

void bbMemoryAccessViolation() {
    extern void throw_mav();
    throw_mav();
}

void bbInitErrorMsgs(int number, bool hasMacro) {
    delete[] ErrorMessagePool::memoryAccessViolation;
    ErrorMessagePool::memoryAccessViolation = new std::string[number];
    ErrorMessagePool::size = number;
    ErrorMessagePool::hasMacro = hasMacro;
}

void bbSetErrorMsg(int pos, BBStr* str) {
    if (ErrorMessagePool::memoryAccessViolation != 0 && pos < ErrorMessagePool::size) {
        ErrorMessagePool::memoryAccessViolation[pos] = str->c_str();
    }
    delete str;
}

BBStr* bbGetException() {
    return new BBStr(std::format("{0}: {1}", errorfunc, errorlog));
}

void bbClearException() {
    errorfunc = "";
    errorlog = "";
}

BBStr* bbGetUserLanguage() {
    wchar_t buf[6]; // should enough
    GetUserDefaultLocaleName(buf, 6);
    return new BBStr(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(buf));
}

BBStr* bbGetLocaleInfo() {
    char szLangName[10];
    GetLocaleInfoA(GetSystemDefaultLCID(), LOCALE_SABBREVLANGNAME, szLangName, sizeof(szLangName) / sizeof(szLangName[0]));
    return new BBStr(szLangName);
}

int bbExecFile(BBStr* f) {
    std::string t = *f;
    delete f;
    int n = gx_runtime->execute(t);
    if (!gx_runtime->idle()) RTEX(0);
    return n;
}

void bbDelay(int ms) {
    if (!gx_runtime->delay(ms)) RTEX(0);
}

int bbMilliSecs() {
    return gx_runtime->getMilliSecs();
}

BBStr* bbCommandLine() {
    return new BBStr(gx_runtime->commandLine());
}

BBStr* bbSystemProperty(BBStr* p) {
    std::string t = gx_runtime->systemProperty(*p);
    delete p; return new BBStr(t);
}

BBStr* bbGetEnv(BBStr* env_var) {
    char* p = getenv(env_var->c_str());
    BBStr* val = new BBStr(p ? p : "");
    delete env_var;
    return val;
}

void bbSetEnv(BBStr* env_var, BBStr* val) {
    std::string t = *env_var + "=" + *val;
    putenv(t.c_str());
    delete env_var;
    delete val;
}

gxTimer* bbCreateTimer(int hertz) {
    gxTimer* t = gx_runtime->createTimer(hertz);
    return t;
}

int bbWaitTimer(gxTimer* t) {
    int n = t->wait();
    delete t;
    if (!gx_runtime->idle()) RTEX(0);
    return n;
}

void bbFreeTimer(gxTimer* t) {
    gx_runtime->freeTimer(t);
    delete t;
}

std::string utf16_to_utf8(std::u16string&& utf16_string) {
    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> convert;
    auto p = reinterpret_cast<const int16_t*>(utf16_string.data());
    return convert.to_bytes(p, p + utf16_string.size());
}

BBStr* bbGetClipboardContents() {
    OpenClipboard(nullptr);
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    BBStr* str;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        char16_t* pszText = static_cast<char16_t*>(GlobalLock(data));
        std::u16string wtext(pszText);
        GlobalUnlock(data);
        str = new BBStr(utf16_to_utf8(std::move(wtext)));
    }
    else {
        str = new BBStr("");
    }
    CloseClipboard();
    return str;
}

void bbSetClipboardContents(BBStr* contents) {
    std::wstring chs = UTF8::convertToUtf16(contents->data());
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (chs.size() + 1) * sizeof(wchar_t));
    memcpy(GlobalLock(hMem), chs.data(), (chs.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(hMem);
    OpenClipboard(nullptr);
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
}

void bbMessageBox(BBStr* title, BBStr* text) {
    MessageBoxW(gx_runtime->hwnd, UTF8::convertToUtf16(text->c_str()).c_str(), UTF8::convertToUtf16(title->c_str()).c_str(), MB_APPLMODAL | MB_ICONINFORMATION);
    delete title; delete text;
}

void bbDebugLog(BBStr* t) {
    gx_runtime->debugLog(t->c_str());
    delete t;
}

void _bbDebugStmt(int pos, const char* file) {
    if (gx_runtime->debugStmt(pos, file)) {
        return;
    }

    if (!gx_runtime->idle()) RTEX(0);
}

void _bbDebugEnter(void* frame, void* env, const char* func) {
    gx_runtime->debugEnter(frame, env, func);
}

void _bbDebugLeave() {
    gx_runtime->debugLeave();
}

bool basic_create();
bool basic_destroy();
void basic_link(void (*rtSym)(const char* sym, void* pc));
bool math_create();
bool math_destroy();
void math_link(void (*rtSym)(const char* sym, void* pc));
bool string_create();
bool string_destroy();
void string_link(void (*rtSym)(const char* sym, void* pc));
bool stream_create();
bool stream_destroy();
void stream_link(void (*rtSym)(const char* sym, void* pc));
bool sockets_create();
bool sockets_destroy();
void sockets_link(void (*rtSym)(const char* sym, void* pc));
bool filesystem_create();
bool filesystem_destroy();
void filesystem_link(void (*rtSym)(const char* sym, void* pc));
bool bank_create();
bool bank_destroy();
void bank_link(void (*rtSym)(const char* sym, void* pc));
bool graphics_create();
bool graphics_destroy();
void graphics_link(void (*rtSym)(const char* sym, void* pc));
bool input_create();
bool input_destroy();
void input_link(void (*rtSym)(const char* sym, void* pc));
bool audio_create();
bool audio_destroy();
void audio_link(void (*rtSym)(const char* sym, void* pc));

bool userlibs_create();
void userlibs_destroy();
void userlibs_link(void (*rtSym)(const char* sym, void* pc));

bool blitz3d_create();
bool blitz3d_destroy();
void blitz3d_link(void (*rtSym)(const char* sym, void* pc));

void bbruntime_link(void (*rtSym)(const char* sym, void* pc)) {

    rtSym("End", bbEnd);
    rtSym("Stop", bbStop);
    rtSym("AppTitle$title$close_prompt=\"\"", bbAppTitle);
    rtSym("RuntimeError$message", bbRuntimeError);
    rtSym("MemoryAccessViolation", bbMemoryAccessViolation);
    rtSym("InitErrorMsgs%number%hasMacro=0", bbInitErrorMsgs);
    rtSym("SetErrorMsg%pos$message", bbSetErrorMsg);
    rtSym("$GetException", bbGetException);
    rtSym("ClearException", bbClearException);
    rtSym("ExecFile$command", bbExecFile);
    rtSym("Delay%millisecs", bbDelay);
    rtSym("%MilliSecs", bbMilliSecs);
    rtSym("$CommandLine", bbCommandLine);
    rtSym("$SystemProperty$property", bbSystemProperty);
    rtSym("$GetEnv$env_var", bbGetEnv);
    rtSym("SetEnv$env_var$value", bbSetEnv);
    rtSym("DisableClose", bbDisableClose);
    rtSym("$GetUserLanguage", bbGetUserLanguage);
    rtSym("$GetLocaleInfo", bbGetLocaleInfo);

    rtSym("%CreateTimer%hertz", bbCreateTimer);
    rtSym("%WaitTimer%timer", bbWaitTimer);
    rtSym("FreeTimer%timer", bbFreeTimer);
    rtSym("$GetClipboardContents", bbGetClipboardContents);
    rtSym("SetClipboardContents$contents", bbSetClipboardContents);
    rtSym("MessageBox$title$text", bbMessageBox);
    rtSym("DebugLog$text", bbDebugLog);

    rtSym("_bbDebugStmt", _bbDebugStmt);
    rtSym("_bbDebugEnter", _bbDebugEnter);
    rtSym("_bbDebugLeave", _bbDebugLeave);

    basic_link(rtSym);
    math_link(rtSym);
    string_link(rtSym);
    stream_link(rtSym);
    sockets_link(rtSym);
    filesystem_link(rtSym);
    bank_link(rtSym);
    graphics_link(rtSym);
    input_link(rtSym);
    audio_link(rtSym);
    blitz3d_link(rtSym);
    userlibs_link(rtSym);
}

//start up error
static void sue(const char* t) {
    std::string p = std::format(MultiLang::startup_error, t);
    gx_runtime->debugError(p.c_str());
}

bool bbruntime_create() {
    INIT(basic);
    INIT(math);
    INIT(string);
    INIT(stream);
    INIT(sockets);
    INIT(filesystem);
    INIT(bank);
    INIT(graphics);
    INIT(input);
    INIT(audio);
    INIT(blitz3d);
    return true;
}

bool bbruntime_destroy() {
    userlibs_destroy();
    blitz3d_destroy();
    audio_destroy();
    input_destroy();
    graphics_destroy();
    bank_destroy();
    filesystem_destroy();
    sockets_destroy();
    stream_destroy();
    string_destroy();
    math_destroy();
    basic_destroy();
    return true;
}

inline static const wchar_t* CharToWchar(const char* ch) {
    const size_t len = strlen(ch) + 1;
    wchar_t* wch = new wchar_t[len];
    mbstowcs(wch, ch, len);
    return wch;
}

inline const char* getCharPtr(std::string str) {
    char* cha = new char[str.size() + 1];
    memcpy(cha, str.c_str(), str.size() + 1);
    const char* p = cha;
    return p;
}

inline static unsigned long ExceptionFilter(PEXCEPTION_POINTERS ex, PEXCEPTION_POINTERS& pex) {
    if (
        ex->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        ex->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION ||
        ex->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW ||
        ex->ExceptionRecord->ExceptionCode == EXCEPTION_INT_OVERFLOW ||
        ex->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_OVERFLOW ||
        ex->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_DIVIDE_BY_ZERO ||
        ex->ExceptionRecord->ExceptionCode == 0xE0000001
        ) {
        pex = ex;
        return EXCEPTION_EXECUTE_HANDLER;
    } else if (ex->ExceptionRecord->ExceptionCode == 0xE0000002) {
        errorfunc = getCharPtr(reinterpret_cast<const char*>(ex->ExceptionRecord->ExceptionInformation[0]));
        errorlog = getCharPtr(reinterpret_cast<const char*>(ex->ExceptionRecord->ExceptionInformation[1]));
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

inline static void program(void (*pc)()) {
    PEXCEPTION_POINTERS ex = NULL;
    __try {
        if (!gx_runtime->idle()) RTEX(0);
        pc();
        gx_runtime->debugInfo(MultiLang::program_ended);
    }
    __except (ExceptionFilter(GetExceptionInformation(), ex)) {
        switch (ex->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            bbruntime_panic(MultiLang::integer_divide_zero);
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
        case 0xE0000001:
            bbruntime_panic(CharToWchar(reinterpret_cast<const char*>(ex->ExceptionRecord->ExceptionInformation[0])));
            break;
        }
    }
}

const char* bbruntime_run(gxRuntime* rt, void (*pc)(), bool dbg) {
    debug = dbg;
    gx_runtime = rt;

    if (!bbruntime_create()) return MultiLang::unable_start_program;
    const char* t = 0;
    try {
        program(pc);
    }
    catch (bbEx x) {
        t = x.err;
    }
    catch (std::exception e) {
        t = e.what();
    }
    catch (...) {
        t = MultiLang::unknown_exception_thrown;
    }
    bbruntime_destroy();
    return t;
}

void bbruntime_panic(const wchar_t* err) {
    MessageBoxW(gx_runtime->hwnd, err, MultiLang::runtime_error, MB_APPLMODAL);
    ExitProcess(-1);
}