//
//  parser_host.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#ifndef __BEATDROP_PARSER_PARSER_HOST_H__
#define __BEATDROP_PARSER_PARSER_HOST_H__ 1

#define ERR_ALL    0
#define ERR_INIT   1  //specifically, loading a preset
#define ERR_PRESET 2  //specifically, loading a preset
#define ERR_MISC   3
#define ERR_NOTIFY 4  // a simple notification - not an error at all. ("shuffle is now ON." etc.)
                      // NOTE: each NOTIFY msg clears all the old NOTIFY messages!
#define ERR_SCANNING_PRESETS 5
#define ERR_SHADER_PRECACHE 6

enum
{
	MD2_PS_NONE = 0,
	MD2_PS_2_0 = 2,
	MD2_PS_2_X = 3,
	MD2_PS_3_0 = 4,
	MD2_PS_4_0 = 5, // not supported by milkdrop
};

class ParserHostSink
{
public:
    virtual void  ReportError(wchar_t* message, float duration, int category, bool bold) = 0;
    virtual void  ShowUserMessage(const char* message, float duration, bool isError) = 0;
    virtual float Now() = 0;
    virtual ~ParserHostSink() {}
};

#endif
