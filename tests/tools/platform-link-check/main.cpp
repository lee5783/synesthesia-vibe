//
//  main.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

#include "state.h"
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <memory>

void RemoveExtension(wchar_t *str);

int main()
{
    wchar_t testName[] = L"platform-link-check-name.milk";
    RemoveExtension(testName);
    if (wcscmp(testName, L"platform-link-check-name") != 0)
    {
        fwprintf(stderr, L"RemoveExtension mismatch: %ls\n", testName);
        return EXIT_FAILURE;
    }

    std::unique_ptr<CState> state(new CState());
    state->Default();

    const wchar_t* exportPath = L"platform-link-check-export.milk";
    if (!state->Export(exportPath))
    {
        fwprintf(stderr, L"CState::Export failed\n");
        return EXIT_FAILURE;
    }

    printf("platform-link-check: OK\n");
    return EXIT_SUCCESS;
}
