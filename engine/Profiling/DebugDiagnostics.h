// DebugDiagnostics.h - Install debug-time diagnostics for MSVC (moved to Profiling)
#pragma once

namespace DebugDiagnostics {
// Installs MSVC CRT report hook and unhandled exception logging in Debug builds.
// No-ops in Release builds or non-MSVC compilers.
void Install();
}

