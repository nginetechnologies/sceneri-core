#pragma once

#include <Common/Memory/Containers/ForwardDeclarations/String.h>

namespace ngine::Scripting
{
	// #define SCRIPTING_USE_UNICODE

#ifdef SCRIPTING_USE_UNICODE
	using StringType = UnicodeString;
#define SCRIPT_STRING_LITERAL MAKE_UNICODE_LITERAL
#else
	using StringType = String;
#define SCRIPT_STRING_LITERAL
#endif

}
