// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

// Project includes
#include <Core/Core.h>
#include <Core/Memory.h>
#include <Core/STLAllocator.h>
#include <Core/IssueReporting.h>
#include <Math/Math.h>
#include <Math/Vec4.h>
#include <Math/Mat44.h>

#define ENABLE_PHYSICS_DEBUG_RENDERER DEVELOPMENT_BUILD
#if ENABLE_PHYSICS_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER
#endif
