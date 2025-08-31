// DebugDiagnostics.cpp - MSVC Debug diagnostics (CRT assert and stack capture)
#include "DebugDiagnostics.h"

#if defined(_MSC_VER) && !defined(NDEBUG)

#  include <windows.h>
#  include <DbgHelp.h>
#  include <crtdbg.h>
#  include <cstdio>
#  include <ctime>
#  include <fstream>
#  include <mutex>
#  include <string>

// Link with DbgHelp for symbol resolution (best-effort; if unavailable we still log addresses)
#  pragma comment(lib, "DbgHelp.lib")

namespace {
std::mutex g_logMutex;

std::string NowStamp() {
    std::time_t t = std::time(nullptr);
    char buf[64]{};
    std::tm tmBuf{};
    localtime_s(&tmBuf, &t);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return buf;
}

void WriteLine(std::ofstream& out, const std::string& s) {
    out << s << "\n";
    out.flush();
    OutputDebugStringA((s + "\n").c_str());
}

void LogStack(std::ofstream& out, unsigned framesToSkip = 0, unsigned framesToCapture = 64) {
    void* stack[128]{};
    USHORT n = RtlCaptureStackBackTrace(framesToSkip + 1, framesToCapture, stack, nullptr);

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);

    constexpr size_t MaxNameLen = 256;
    char buffer[sizeof(SYMBOL_INFO) + MaxNameLen];
    PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MaxNameLen - 1;

    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);
    DWORD displacement = 0;

    for (USHORT i = 0; i < n; ++i) {
        DWORD64 addr = reinterpret_cast<DWORD64>(stack[i]);
        std::string lineOut = "  [" + std::to_string(i) + "] 0x" + std::to_string((unsigned long long)addr);
        if (SymFromAddr(process, addr, 0, pSymbol)) {
            lineOut += "  ";
            lineOut += pSymbol->Name;
        }
        if (SymGetLineFromAddr64(process, addr, &displacement, &line)) {
            lineOut += "  (";
            lineOut += line.FileName ? line.FileName : "?";
            lineOut += ":" + std::to_string(line.LineNumber) + ")";
        }
        WriteLine(out, lineOut);
    }
}

int __cdecl ReportHookA(int reportType, char* message, int* returnValue) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    try {
        std::ofstream out("debug_crt_assert.log", std::ios::app);
        if (out) {
            WriteLine(out, "===== CRT Report (" + NowStamp() + ") =====");
            switch (reportType) {
                case _CRT_WARN:       WriteLine(out, "Type: WARN"); break;
                case _CRT_ERROR:      WriteLine(out, "Type: ERROR"); break;
                case _CRT_ASSERT:     WriteLine(out, "Type: ASSERT"); break;
                default:              WriteLine(out, "Type: OTHER"); break;
            }
            WriteLine(out, std::string("Message: ") + (message ? message : "<null>"));
            WriteLine(out, "Stack:");
            LogStack(out, /*skip*/1);
            WriteLine(out, "===========================================");
        }
    } catch (...) {
        // Swallow any logging errors
    }
    // Let the normal dialog still appear (return 0). Set *returnValue if you want to change behavior.
    return 0;
}
} // namespace

void DebugDiagnostics::Install() {
    // Route CRT reports (asserts) to our hook
    _CrtSetReportHook(ReportHookA);
    // Ensure asserts also go to debugger output
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_WNDW);
}

#else

void DebugDiagnostics::Install() {}

#endif // _MSC_VER && !_NDEBUG
