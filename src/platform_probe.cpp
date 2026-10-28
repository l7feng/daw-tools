#include <windows.h>
#include <mmsystem.h>
#include <iostream>
int main() {
    std::wcout << L"Studio Workbench platform probe; enumeration only\n";
    std::wcout << L"wave outputs=" << waveOutGetNumDevs() << L"\n";
    for (UINT i=0;i<waveOutGetNumDevs();++i) {
        WAVEOUTCAPSW caps{};
        if(waveOutGetDevCapsW(i,&caps,sizeof(caps))!=MMSYSERR_NOERROR) return 2;
        std::wcout << L"output " << i << L": " << caps.szPname << L" channels=" << caps.wChannels << L"\n";
    }
    std::wcout << L"wave inputs=" << waveInGetNumDevs() << L"\n";
    std::wcout << L"MIDI inputs=" << midiInGetNumDevs() << L"\n";
    for(UINT i=0;i<midiInGetNumDevs();++i) {
        MIDIINCAPSW caps{};
        if(midiInGetDevCapsW(i,&caps,sizeof(caps))!=MMSYSERR_NOERROR) return 3;
        std::wcout << L"MIDI input " << i << L": " << caps.szPname << L"\n";
    }
    std::wcout << L"No devices opened; no playback or recording performed.\n";
    return 0;
}
