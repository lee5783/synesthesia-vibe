//
//  path_utils.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

#include "path_utils.h"
#include <wchar.h>

void RemoveExtension(wchar_t *str)
{
    wchar_t *p = wcsrchr(str, L'.');
    if (p) *p = 0;
}
