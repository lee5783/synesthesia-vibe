//
//  parser_win32.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#pragma once

#include <cstdio>

struct ParserLocalTime
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

const wchar_t* ParserLangStringPresetInitCodeError();
const wchar_t* ParserLangStringPerFrameCodeError();
const wchar_t* ParserLangStringPerVertexCodeError();
const wchar_t* ParserLangStringWaveInitCodeError();
const wchar_t* ParserLangStringWavePerFrameCodeError();
const wchar_t* ParserLangStringWavePerPointCodeError();
const wchar_t* ParserLangStringShapeInitCodeError();
const wchar_t* ParserLangStringShapePerFrameCodeError();

void ParserLstrcpyW(wchar_t* dst, const wchar_t* src);
void ParserLstrcpyA(char* dst, const char* src);

FILE* ParserWfopen(const wchar_t* path, const wchar_t* mode);

bool ParserSscanfFloatCLocale(const char* buf, float* out);

void ParserGetLocalTime(ParserLocalTime* st);
