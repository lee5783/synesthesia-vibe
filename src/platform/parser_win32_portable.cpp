//
//  parser_win32_portable.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

#include "parser_win32.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <locale>
#include <string>

const wchar_t* ParserLangStringPresetInitCodeError()
{
    return L"warning: preset \"%s\": error in 'preset_init' code";
}

const wchar_t* ParserLangStringPerFrameCodeError()
{
    return L"warning: preset \"%s\": error in 'per-frame' code";
}

const wchar_t* ParserLangStringPerVertexCodeError()
{
    return L"warning: preset \"%s\": error in 'per-vertex' code";
}

const wchar_t* ParserLangStringWaveInitCodeError()
{
    return L"warning: preset \"%s\": error in wave %d init code";
}

const wchar_t* ParserLangStringWavePerFrameCodeError()
{
    return L"warning: preset \"%s\": error in wave %d per-frame code";
}

const wchar_t* ParserLangStringWavePerPointCodeError()
{
    return L"warning: preset \"%s\": error in wave %d per-point code";
}

const wchar_t* ParserLangStringShapeInitCodeError()
{
    return L"warning: preset \"%s\": error in shape %d init code";
}

const wchar_t* ParserLangStringShapePerFrameCodeError()
{
    return L"warning: preset \"%s\": error in shape %d per-frame code";
}

void ParserLstrcpyW(wchar_t* dst, const wchar_t* src)
{
    wcscpy(dst, src);
}

void ParserLstrcpyA(char* dst, const char* src)
{
    strcpy(dst, src);
}

static std::string ParserWideToUtf8(const wchar_t* s)
{
    std::string out;
    for (const wchar_t* p = s; *p; ++p)
    {
        unsigned long cp = (unsigned long)(unsigned int)*p;
        if (cp < 0x80)
        {
            out += (char)cp;
        }
        else if (cp < 0x800)
        {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

FILE* ParserWfopen(const wchar_t* path, const wchar_t* mode)
{
    std::string utf8Path = ParserWideToUtf8(path);
    std::string utf8Mode = ParserWideToUtf8(mode);
    return fopen(utf8Path.c_str(), utf8Mode.c_str());
}

bool ParserSscanfFloatCLocale(const char* buf, float* out)
{
    std::istringstream iss(buf);
    iss.imbue(std::locale::classic());
    iss >> *out;
    return !iss.fail();
}

void ParserGetLocalTime(ParserLocalTime* st)
{
    std::time_t t = std::time(nullptr);
    std::tm tmValue;
#if defined(_WIN32)
    localtime_s(&tmValue, &t);
#else
    localtime_r(&t, &tmValue);
#endif
    st->year   = tmValue.tm_year + 1900;
    st->month  = tmValue.tm_mon + 1;
    st->day    = tmValue.tm_mday;
    st->hour   = tmValue.tm_hour;
    st->minute = tmValue.tm_min;
    st->second = tmValue.tm_sec;
}
