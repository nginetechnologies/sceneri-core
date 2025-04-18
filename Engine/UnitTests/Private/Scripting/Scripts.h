#pragma once

#include <Engine/Scripting/Parser/StringType.h>
#include <Common/Memory/Containers/String.h>

namespace ngine
{
	constexpr Scripting::StringType::ConstView ScriptSourceFibonacci = SCRIPT_STRING_LITERAL(R"(
		function fibonacci_tail(n: integer, prev: integer, current: integer): integer
			if n == 1 then
				return current
			else
				return fibonacci_tail(n - 1, current, prev + current)
			end
		end
		function fibonacci(n: integer): integer
			if n == 0 then
				return 0
			else
				return fibonacci_tail(n, 0, 1)
			end
		end
		assert(fibonacci(30) == 832040)
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceRepeatUntil = SCRIPT_STRING_LITERAL(R"(
		local x = 0
		repeat
		  local y = x * 2
		  x = x + 1
		until y == 10
		assert(x == 6 and y == nil)
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceBreak = SCRIPT_STRING_LITERAL(R"(
		local sum = 0
		local x, y = 1, 0
		while y < 5 do
			repeat
				local _x = x
				sum = sum + 1
				if y > 1 then
					break
				end
				x = x + 1
			until _x == 5
			x = 1
			y = y + 1
			if y == 3 then
				break
			end
		end
		assert(sum == 11)
	)");

	// from https://www.lua.org/tests/
	constexpr Scripting::StringType::ConstView ScriptSourceAssignment = SCRIPT_STRING_LITERAL(R"(
		local res, res2 = 27
		local a, b = 1, 2+3
		assert(a==1 and b==5)
		local function f(): (integer, integer, integer) return 10, 11, 12 end
		local a, b, c, d = 1 and nil, 1 or nil, (1 and (nil or 1)), 6
		assert(not a and b and c and d==6)
		d = 20
		a, b, c, d = f()
		assert(a==10 and b==11 and c==12 and d==nil)
		a,b = f(), 1, 2, 3, f()
		assert(a==10 and b==1)
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceOrderOperator = SCRIPT_STRING_LITERAL(R"(
		assert(not(1<1) and (1<2) and not(2<1))
		assert(not('a'<'a') and ('a'<'b') and not('b'<'a'))
		assert((1<=1) and (1<=2) and not(2<=1))
		assert(('a'<='a') and ('a'<='b') and not('b'<='a'))
		assert(not(1>1) and not(1>2) and (2>1))
		assert(not('a'>'a') and not('a'>'b') and ('b'>'a'))
		assert((1>=1) and not(1>=2) and (2>=1))
		assert(('a'>='a') and not('a'>='b') and ('b'>='a'))
		assert(1.3 < 1.4 and 1.3 <= 1.4 and not (1.3 < 1.3) and 1.3 <= 1.3)
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceLogicalOperators = SCRIPT_STRING_LITERAL(R"(
		a, b = 10, 1
		assert(a<b == false and a>b == true)
		assert((10 and 2) == 2)
		assert((10 or 2) == 10)
		assert((10 or assert(nil)) == 10)
		assert(not (nil and assert(nil)))
		assert((nil or "alo") == "alo")
		assert((nil and 10) == nil)
		assert((false and 10) == false)
		assert((true or 10) == true)
		assert((false or 10) == 10)
		assert(not nil == true)
		assert(not not nil == false)
		assert(not not 1 == true)
		assert(not not a == true)
		assert(not not (6 or nil) == true)
		assert(not not (nil and 56) == false)
		assert(not not (nil and true) == false)
		assert(not 10 == false)
		assert(not 0.5 == false)
		assert(not "x" == false)
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceLocalVariables = SCRIPT_STRING_LITERAL(R"(
		local function f(x: integer): integer | nil x = nil; return x end
		assert(f(10) == nil)

		local function f(): integer | nil local x; return x end
		assert(f(10) == nil)

		local function f(x: integer): (integer | nil, integer | nil) x = nil; local y; return x, y end
		assert(f(10) == nil)

		do
		local i = 10
		do local i = 100; assert(i==100) end
		do local i = 1000; assert(i==1000) end
		assert(i == 10)
		if i ~= 10 then
			local i = 20
		else
			local i = 30
			assert(i == 30)
		end
		end

		f = nil

		local f
		local x = 1

		a = nil

		function f3 (a: integer)
			local _1, _2, _3, _4, _5
			local _6, _7, _8, _9, _10
			local x = 3
			local b = a
			local c,d = a,b
			if (d == b) then
				local x = 'q'
				x = b
				assert(x == 2)
			else
				assert(nil)
			end
			assert(x == 3)
			local f = 10
		end

		local b=10
		local a; repeat local b; a,b=1,2; assert(a+1==b); until a+b==3
		assert(x == 1)
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceLocalFunctions = SCRIPT_STRING_LITERAL(R"(
		do
			local function f(a: integer,b: integer,c: nil): (nil, integer) return c, b end
			local function g(): (nil, integer) return f(1,2) end
			local a, b = g()
			assert(a == nil and b == 2)
		end
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceObjectFunctions = SCRIPT_STRING_LITERAL(R"(
		local function f(a: integer): integer return a end
		local a: integer = 10
		
		assert(a.f() == 10)
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceBraceInitialization = SCRIPT_STRING_LITERAL(R"(
		local vec3: vec3f = { 1, 2, 3 };
		assert(vec3 == (vec3f{ 0, 1, 1 } + vec3f{1, 1, 2}));
		
		local asset_id: asset = { "7161186c-74df-4087-ba74-910838a35189" };
		local asset_id_with_type: asset = { "7161186c-74df-4087-ba74-910838a35189", "6eaa8ffd-1930-476b-bcef-32460d50f6c1" };
		local tag_id: tag = { "7568a6d8-f91e-4e4a-aee2-329d630ba249" };
	)");

	constexpr Scripting::StringType::ConstView ScriptSourceMath = SCRIPT_STRING_LITERAL(R"(
		assert(abs(-1.0) == 1.0)
		assert(abs(1.0) == 1.0)
		assert(isclose(acos(1.0), 0.0))
		assert(isclose(asin(0.0), 0.0))
		assert(isclose(atan(0.0), 0.0))
		assert(isclose(atan2(0.0, 1.0), 0.0))
		assert(isclose(ceil(1.1), 2.0))
		assert(isclose(cbrt(8.0), 2.0))
		assert(isclose(cos(0.0), 1.0))
		assert(isclose(exp(1.0), e))
		assert(isclose(floor(1.1), 1.0))
		assert(isclose(fract(1.1), 0.1))
		assert(isclose(isqrt(4.0), 0.5))
		assert(isclose(fmod(1.5, 1.0), 0.5))
		assert(isclose(log(e), 1.0))
		assert(isclose(log2(64.0), 6.0))
		assert(isclose(log10(100.0), 2.0))
		assert(max(1.0, 2.0) == 2.0)
		assert(min(1.0, 2.0) == 1.0)
		assert(isclose(rcp(0.2), 5.0))
		assert(isclose(pow(2.0, 2.0), 4.0))
		assert(isclose(pow2(2.0), 4.0))
		assert(isclose(pow10(2.0), 100.0))
		local rand = random()
		assert(rand >= 0.0 and rand <= 1.0)
		rand = random(100.0)
		assert(rand >= 1.0 and rand <= 100.0)
		rand = random(70.0, 80.0)
		assert(rand >= 70.0 and rand <= 80.0)
		assert(round(1.1) == 1.0)
		assert(round(1.6) == 2.0)
		assert(sign(1.0) == 1.0)
		assert(sign(0.0) == 0.0)
		assert(sign(-1.0) == -1.0)
		assert(signnonzero(-1.0) == -1.0)
		assert(signnonzero(0.0) == 1.0)
		assert(signnonzero(-0.0) == -1.0)
		assert(isclose(sin(0.0), 0.0))
		assert(isclose(sqrt(4.0), 2.0))
		assert(isclose(tan(0.0), 0.0))
		assert(trunc(1.1) == 1)
		assert(trunc(pi) == 3)
		assert(trunc(pi2) == 6)
		assert(trunc(e) == 2)
		
		assert(isclose(dot(vec3f{1.0, 0.0, 0.0}, vec3f{0.0, 1.0, 0.0}), 0.0))
		assert(isclose(distance(vec3f{0.0, 0.0, 0.0}, vec3f{0.0, 1.0, 0.0}), 1.0))
		assert(all(cross(vec3f{1.0, 0.0, 0.0}, vec3f{0.0, 1.0, 0.0}) == vec3f{0, 0, 1}))
		assert(isclose(length(vec3f{2.0, 0.0, 0.0}), 2.0))
		assert(isclose(length_squared(vec3f{2.0, 0.0, 0.0}), 4.0))
		assert(isclose(inverse_length(vec3f{2.0, 0.0, 0.0}), 0.5))
		assert(all(normalize(vec3f{0.0, 5.0, 0.0}) == vec3f{0.0, 1.0, 0.0}))
	)");
}
