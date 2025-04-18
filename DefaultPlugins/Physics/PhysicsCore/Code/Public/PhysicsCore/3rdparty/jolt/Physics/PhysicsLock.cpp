// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/PhysicsLock.h>

JPH_NAMESPACE_BEGIN

thread_local uint32 PhysicsLock::sLockedMutexes = 0;

JPH_NAMESPACE_END
