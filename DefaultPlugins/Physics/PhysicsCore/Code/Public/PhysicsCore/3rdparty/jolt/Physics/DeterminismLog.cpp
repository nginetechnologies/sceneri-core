// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/DeterminismLog.h>

#ifdef JPH_ENABLE_DETERMINISM_LOG

JPH_NAMESPACE_BEGIN

DeterminismLog DeterminismLog::sLog;

JPH_NAMESPACE_END

#endif // JPH_ENABLE_DETERMINISM_LOG
