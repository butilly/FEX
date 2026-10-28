// SPDX-License-Identifier: MIT
#pragma once

namespace FEX::Windows::Logging {
void InitEarly();
void StartupTrace(const char* Message);
void Init();
} // namespace FEX::Windows::Logging
