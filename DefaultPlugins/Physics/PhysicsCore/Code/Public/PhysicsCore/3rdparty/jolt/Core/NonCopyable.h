// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Class that makes another class non-copyable. Usage: Inherit from NonCopyable.
class NonCopyable
{
public:
			NonCopyable() = default;
			NonCopyable(const NonCopyable &) = delete;
	void	operator = (const NonCopyable &) = delete;
};

JPH_NAMESPACE_END