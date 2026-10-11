//
//  nseel_hoststub.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

extern "C" {
void NSEEL_HOSTSTUB_EnterMutex();
void NSEEL_HOSTSTUB_LeaveMutex();
}

void NSEEL_HOSTSTUB_EnterMutex() {}
void NSEEL_HOSTSTUB_LeaveMutex() {}
