#pragma once

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(4324 4310 4458 4100 4296 4701)

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wunused-parameter")
DISABLE_CLANG_WARNING("-Wunused-variable")
DISABLE_CLANG_WARNING("-Wshadow-field-in-constructor")
DISABLE_CLANG_WARNING("-Wshadow-field")
DISABLE_CLANG_WARNING("-Wshadow")
DISABLE_CLANG_WARNING("-Wimplicit-int-float-conversion")
DISABLE_CLANG_WARNING("-Wimplicit-int-conversion")
DISABLE_CLANG_WARNING("-Wdeprecated-copy-dtor")
DISABLE_CLANG_WARNING("-Wanon-enum-enum-conversion")
DISABLE_CLANG_WARNING("-Wmissing-template-arg-list-after-template-kw")

PUSH_GCC_WARNINGS
DISABLE_CLANG_WARNING("-Wunused-parameter");

#include "gli.hpp"
#include "view.hpp"
#include "generate_mipmaps.hpp"

POP_CLANG_WARNINGS
POP_GCC_WARNINGS
POP_MSVC_WARNINGS
