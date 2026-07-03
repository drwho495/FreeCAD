// SPDX-License-Identifier: LGPL-2.1-or-later
//
// WebAssembly build only. Carries FcWasmProcess (see FcWasmProcess.h) through
// AUTOMOC so its Q_OBJECT metaobject and signal definitions are emitted and
// linked exactly once. QProcess is aliased to it via a target-wide
// force-include.
#include "FcWasmProcess.h"
