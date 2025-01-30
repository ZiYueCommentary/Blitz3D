#ifndef GXUTF8_H
#define GXUTF8_H

#include <string>
#include <atlstr.h>

class UTF8 {
private:
    UTF8() {}
    UTF8(const UTF8& o) {}
public:
    static int measureCodepoint(char chr);
    static int decodeCharacter(const char* buf, int index);
    static int encodeCharacter(int chr, char* result);
    static std::string convertToUtf8(const std::string& str);
    static std::string convertToAnsi(const std::string& str);
    static bool isValidUtf8String(const std::string& str);
    static int length(const std::string& str);
    static int find(const std::string& str, const std::string& sstr, int from);
    static void popBack(std::string& str);
    static std::string substr(const std::string& str, int start, int length);
    static std::wstring convertToUtf16(const std::string& str); 
    static std::string replaceAll(const std::string_view& string, const std::string_view& pattern, const std::string_view& newpat);
    static std::string getSystemFontFile(const std::string& faceName);
};

#endif