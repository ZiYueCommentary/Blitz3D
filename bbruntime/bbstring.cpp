#include "std.h"
#include "bbsys.h"
#include "../gxruntime/gxutf8.h"
#include "../bbruntime/bbgraphics.h"
#include <time.h>
#include "../MultiLang/MultiLang.h"

#define CHKPOS(x, func) if(x < 0) ErrorLog(func, MultiLang::string_parameter_positive);
#define CHKOFF(x, func) if(x <= 0) ErrorLog(func, MultiLang::string_parameter_greater);

BBStr* bbString(BBStr* s, int n) {
	BBStr* t = new BBStr();
	while (n-- > 0) *t += *s;
	delete s; return t;
}

BBStr* bbLeft(BBStr* s, int n) {
	CHKPOS(n, "Left");
	*s = UTF8::substr(*s, 0, n); return s;
}

BBStr* bbRight(BBStr* s, int n) {
	CHKPOS(n, "Right");
	n = UTF8::length(*s) - n; if (n < 0) n = 0;
	*s = UTF8::substr(*s, n, s->size()); return s;
}

BBStr* bbReplace(BBStr* s, BBStr* from, BBStr* to) {
	std::string&& str = UTF8::replaceAll(s->c_str(), from->c_str(), to->c_str());
	delete s; delete from; delete to;
	return new BBStr(str);
}

int bbInstr(BBStr* s, BBStr* t, int from) {
	CHKOFF(from, "Instr");
	--from;
	int n = UTF8::find(*s, *t, from);
	delete s; delete t;
	return n < 0 ? 0 : n + 1;
}

BBStr* bbSubstr(BBStr* string, int start, int length) {
	BBStr* ret = new BBStr(UTF8::substr(string->c_str(), start, length));
	delete string;
	return ret;
}

BBStr* bbMid(BBStr* s, int o, int n) {
	CHKOFF(o, "Mid"); --o;
	if (o > s->size()) o = s->size();
	if (n >= 0) *s = UTF8::substr(*s, o, n);
	else *s = UTF8::substr(*s, o, s->size());
	return s;
}

BBStr* bbUpper(BBStr* s) {
	for (int k = 0; k < s->size(); ++k) (*s)[k] = toupper((*s)[k]);
	return s;
}

BBStr* bbLower(BBStr* s) {
	for (int k = 0; k < s->size(); ++k) (*s)[k] = tolower((*s)[k]);
	return s;
}

bool isgraph_safe(int chr) {
	if (chr > 127) { return true; }
	return isgraph(chr);
}

BBStr* bbTrim(BBStr* s) {
	int n = 0;
	int p = s->size();
	// currently all characters above the standard ASCII range are simply not trimmed
	while (n < s->size() && !isgraph_safe((byte)(*s)[n])) ++n;
	while (p > n && !isgraph_safe((byte)(*s)[p - 1])) --p;
	*s = UTF8::substr(*s, n, p - n);
	return s;
}

BBStr* bbLSet(BBStr* s, int n) {
	CHKPOS(n, "LSet");
	if (s->size() > n) *s = s->substr(0, n);
	else {
		while (s->size() < n) *s += ' ';
	}
	return s;
}

BBStr* bbRSet(BBStr* s, int n) {
	CHKPOS(n, "RSet");
	if (s->size() > n) *s = s->substr(s->size() - n);
	else {
		while (s->size() < n) *s = ' ' + *s;
	}
	return s;
}

BBStr* bbChr(int n) {
	BBStr* t = new BBStr();
	*t += (char)n; return t;
}

BBStr* bbHex(int n) {
	char buff[33];
	itoa(n, buff, 16);
	return new BBStr(buff);
}

BBStr* bbBin(int n) {
	char buff[33];
	itoa(n, buff, 2);
	return new BBStr(buff);
}

int bbAsc(BBStr* s) {
	int n = s->size() ? (*s)[0] & 255 : -1;
	delete s; return n;
}

int bbLen(BBStr* s) {
	int n = UTF8::length(*s);
	delete s; return n;
}

BBStr* bbHighPrecisionFloatString(float f) {
	std::ostringstream stream;
	stream << std::setprecision(20) << f;
	return new BBStr(stream.str());
}

int bbIsValidUTF8String(BBStr* s) {
	int valid = UTF8::isValidUtf8String(s->c_str());
	delete s;
	return valid;
}

BBStr* bbCurrentDate() {
	time_t t;
	time(&t);
	char buff[256];
	strftime(buff, 256, "%d %b %Y", localtime(&t));
	return new BBStr(buff);
}

BBStr* bbCurrentTime() {
	time_t t;
	time(&t);
	char buff[256];
	strftime(buff, 256, "%H:%M:%S", localtime(&t));
	return new BBStr(buff);
}

bool string_create() {
	return true;
}

bool string_destroy() {
	return true;
}

void string_link(void(*rtSym)(const char*, void*)) {
	rtSym("$String$string%repeat", bbString);
	rtSym("$Left$string%count", bbLeft);
	rtSym("$Right$string%count", bbRight);
	rtSym("$Replace$string$from$to", bbReplace);
	rtSym("%Instr$string$find%from=1", bbInstr);
	rtSym("$Substr$string%start%length", bbSubstr);
	rtSym("$Mid$string%start%count=-1", bbMid);
	rtSym("$Upper$string", bbUpper);
	rtSym("$Lower$string", bbLower);
	rtSym("$Trim$string", bbTrim);
	rtSym("$LSet$string%size", bbLSet);
	rtSym("$RSet$string%size", bbRSet);
	rtSym("$Chr%ascii", bbChr);
	rtSym("%Asc$string", bbAsc);
	rtSym("%Len$string", bbLen);
	rtSym("$Hex%value", bbHex);
	rtSym("$Bin%value", bbBin);
	rtSym("$HighPrecisionFloatString#value", bbHighPrecisionFloatString);
	rtSym("%IsValidUTF8String$str", bbIsValidUTF8String);
	rtSym("$CurrentDate", bbCurrentDate);
	rtSym("$CurrentTime", bbCurrentTime);
}