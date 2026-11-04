// SPDX-License-Identifier: GPL-3.0-or-later
#include "DiagnosticSafety.h"
#include <cassert>
int main() {
    assert(DiagnosticSafety::isReadOnly());
    DiagnosticSafety::setReadOnly(false);
    assert(!DiagnosticSafety::isReadOnly());
    DiagnosticSafety::setReadOnly(true);
    assert(DiagnosticSafety::isReadOnly());
}
