/*******************************************************************
* Compiles under C++23 with `import std;` support
*	and new std::print functionality for convenience.
*******************************************************************/

import std;

#include <cstdint>
#include <cassert>

#if defined(_MSC_VER)
// Define a FORCEINLINE macro so we can try to minimize as much of the timing function boilerplate overhead as possible
#define FORCEINLINE __forceinline
#elif defined(__GNUG__)
// I think this works, but don't feel like checking
#define FORCEINLINE __attribute__((always_inline))
#else
#define FORCEINLINE inline
#endif

constexpr std::string_view longestPossibleIntString = "-2147483648";

// Global buffer used for one of the string flip approaches
// Don't do size+1 as we don't care about the null terminator; format_to doesn't add it, and we process everything in ranges
auto sharedCharArrayBuffer = std::make_unique<char[]>(longestPossibleIntString.size());

struct TimingResult
{
	std::chrono::milliseconds min = {};
	std::chrono::milliseconds max = {};
	std::chrono::milliseconds mean = {};
	std::chrono::milliseconds median = {};

	inline std::string toString() const noexcept
	{
		return std::format("Average:{}, Median:{}, Min:{}, Max:{}", mean, median, min, max);
	}
};


/// <summary>
/// Use (value / tens_place % 10) to extract the digits from the integer.
///		This version uses an array to look up the possible 10s places instead of using multiplication or division.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
constexpr int32_t reverseDigits_ModuloLookup(int32_t value) noexcept
{
	constexpr uint64_t tensLookupTable[] = {
		1, 10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000, 100'000'000, 1'000'000'000
	};
	constexpr size_t tensLookupCount = std::size(tensLookupTable);

	if (value < 10 && value > -10)
	{
		return value;
	}

	const bool negate = value < 0;
	// Store the value in uint64 to handle overflow without branching in the main loop
	const uint64_t sourceValue = static_cast<uint64_t>(negate ? -static_cast<int64_t>(value) : static_cast<int64_t>(value));

	// Should never be less than 10 given the above early return
	size_t largestIndex = 1;
	while (largestIndex < tensLookupCount && sourceValue >= tensLookupTable[largestIndex])
	{
		++largestIndex;
	}
	// Will always overshoot by 1
	--largestIndex;

	// If a power of 10, will always result in 1
	if (sourceValue == tensLookupTable[largestIndex])
	{
		return negate ? -1 : 1;
	}

	uint64_t result = 0;

	const size_t halfIndex = (largestIndex + 1) / 2;
	for (size_t index = 0; index < halfIndex; ++index)
	{
		const size_t upperIndex = largestIndex - index;

		const uint64_t lowerTens = tensLookupTable[index];
		const uint64_t upperTens = tensLookupTable[upperIndex];

		const uint64_t lower = (sourceValue / lowerTens) % 10;
		const uint64_t upper = (sourceValue / upperTens) % 10;

		result += (lower * upperTens) + (upper * lowerTens);
	}

	// For an odd number of digits (even index due to 0-based indexing), copy the middle digit over
	if ((largestIndex & 1) == 0)
	{
		const uint64_t tens = tensLookupTable[halfIndex];
		result += ((sourceValue / tens) % 10) * tens;
	}

	if (result > std::numeric_limits<int32_t>::max())
	{
		return 0;
	}

	return negate ? -static_cast<int32_t>(result) : static_cast<int32_t>(result);
}


/// <summary>
/// Use (value / tens_place % 10) to extract the digits from the integer.
///		This version uses multiplication / division to find the highest 10s place.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
constexpr int32_t reverseDigits_ModuloMultiply(int32_t value) noexcept
{
	if (value < 10 && value > -10)
	{
		return value;
	}

	const bool negate = value < 0;
	const uint64_t sourceValue = static_cast<uint64_t>(negate ? -static_cast<int64_t>(value) : static_cast<int64_t>(value));

	// Should never drop below 10 given the early return at the top
	uint64_t upperTens = 10;
	while (sourceValue >= upperTens)
	{
		upperTens *= 10;
	}
	// Will overshoot by one
	upperTens /= 10;

	// If a power of 10, will always result in 1
	if (sourceValue == upperTens)
	{
		return negate ? -1 : 1;
	}

	uint64_t result = 0;

	uint64_t lowerTens = 1;
	for (; lowerTens < upperTens; lowerTens *= 10, upperTens /= 10)
	{
		const uint64_t lower = (sourceValue / lowerTens) % 10;
		const uint64_t upper = (sourceValue / upperTens) % 10;

		result += (lower * upperTens) + (upper * lowerTens);
	}

	// The above loop will end if lowerTens == upperTens; we don't want to treat that the same as swapping the digits
	if (lowerTens == upperTens)
	{
		result += ((sourceValue / lowerTens) % 10) * lowerTens;
	}

	if (result > std::numeric_limits<int32_t>::max())
	{
		return 0;
	}

	return negate ? -static_cast<int32_t>(result) : static_cast<int32_t>(result);
}

/// <summary>
/// Make a character buffer on the stack and reverse the character there.
///		Using a manual swap loop instead of a standard algorithm
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
int reverseDigits_CharArrayStack(int32_t value) noexcept
{
	if (value < 10 && value > -10)
	{
		return value;
	}

	// Don't do size+1 as we don't care about the null terminator; format_to doesn't add it, and we process everything in ranges
	char buffer[longestPossibleIntString.size()];

	const char* const endPtr = std::format_to(buffer, "{}", value);

	const ptrdiff_t count = std::distance((const char*)buffer, endPtr);

	const size_t offsetEnd = value < 0 ? 0 : 1;
	const size_t skip = 1 - offsetEnd;

	const size_t halfCount = (count + skip) / 2;

	for (size_t index = skip; index < halfCount; ++index)
	{
		std::swap(buffer[index], buffer[count - index - offsetEnd]);
	}

	int32_t result = 0;
	std::from_chars(buffer, endPtr, result);

	return result;
}

/// <summary>
/// Make a character buffer on the stack and reverse the character there.
///		Using the std::ranges::reverse algorithm
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
int reverseDigits_CharArrayStack_RangeAlgorithm(int32_t value) noexcept
{
	if (value < 10 && value > -10)
	{
		return value;
	}

	// Don't do size+1 as we don't care about the null terminator; format_to doesn't add it, and we process everything in ranges
	char buffer[longestPossibleIntString.size()];

	char* const endPtr = std::format_to(buffer, "{}", value);

	const std::span<char> digitView = std::span<char>(value < 0 ? buffer+1 : buffer, endPtr);
	std::ranges::reverse(digitView);

	int32_t result = 0;
	std::from_chars(buffer, endPtr, result);

	return result;
}


/// <summary>
/// Use a character buffer on the heap and reverse the character there.
///		Uses a shared buffer that's re-used between runs.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
int reverseDigits_CharArrayHeap_SharedAlloc(int32_t value) noexcept
{
	if (value < 10 && value > -10)
	{
		return value;
	}

	char* const buffer = sharedCharArrayBuffer.get();

	char* const endPtr = std::format_to(buffer, "{}", value);

	const std::span<char> digitView = std::span<char>(value < 0 ? buffer + 1 : buffer, endPtr);
	std::ranges::reverse(digitView);

	int32_t result = 0;
	std::from_chars(buffer, endPtr, result);

	return result;
}


/// <summary>
/// Use a character buffer on the heap and reverse the character there.
///		Uses a unique character buffere that is allocated every time this is called.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
int reverseDigits_CharArrayHeap_AlwaysAlloc(int32_t value) noexcept
{
	if (value < 10 && value > -10)
	{
		return value;
	}

	// Don't do size+1 as we don't care about the null terminator; format_to doesn't add it, and we process everything in ranges
	auto managedPtr = std::make_unique<char[]>(longestPossibleIntString.size());

	char* const buffer = managedPtr.get();

	char* const endPtr = std::format_to(buffer, "{}", value);

	const std::span<char> digitView = std::span<char>(value < 0 ? buffer + 1 : buffer, endPtr);
	std::ranges::reverse(digitView);

	int32_t result = 0;
	std::from_chars(buffer, endPtr, result);

	return result;
}


/// <summary>
/// Checks the outputs of the various methods and outputs them to the console.
///		Weak form of testing the functions to ensure parity.
/// </summary>
/// <param name="value"></param>
void validateDifferentOutputs(int32_t value) noexcept
{
	const int32_t charStackResult = reverseDigits_CharArrayStack(value);
	const int32_t charStackAlgoResult = reverseDigits_CharArrayStack_RangeAlgorithm(value);
	const int32_t charHeapSharedResult = reverseDigits_CharArrayHeap_SharedAlloc(value);
	const int32_t charHeapAllocResult = reverseDigits_CharArrayHeap_AlwaysAlloc(value);
	const int32_t moduloLookupResult = reverseDigits_ModuloLookup(value);
	const int32_t moduloMultiplyResult = reverseDigits_ModuloMultiply(value);


	std::println("[Char Stack     ] Inverting {} = {}", value, charStackResult);
	std::println("[Char Stack Algo] Inverting {} = {}", value, charStackAlgoResult);
	std::println("[Char Shared    ] Inverting {} = {}", value, charHeapSharedResult);
	std::println("[Char Alloc     ] Inverting {} = {}", value, charHeapAllocResult);
	std::println("[Modulo Lookup  ] Inverting {} = {}", value, moduloLookupResult);
	std::println("[Modulo Multiply] Inverting {} = {}", value, moduloMultiplyResult);
	std::print("\n");

	// Validate that the results all match
	assert(charStackResult == charStackAlgoResult);
	assert(charStackAlgoResult == charHeapSharedResult);
	assert(charHeapSharedResult == charHeapAllocResult);
	assert(charHeapAllocResult == moduloLookupResult);
	assert(moduloLookupResult == moduloMultiplyResult);
}

template<int32_t(*Func)(int32_t), int32_t ValueRange, size_t RepeatCount>
FORCEINLINE TimingResult timeFunction()
{
	using namespace std::chrono_literals;

	auto managedTimes = std::make_unique<std::chrono::milliseconds[]>(RepeatCount);
	const auto timingList = std::span<std::chrono::milliseconds>(managedTimes.get(), RepeatCount);

	TimingResult result = {};
	for (size_t repeatIndex = 0; repeatIndex < RepeatCount; ++repeatIndex)
	{
		std::print(".");

		const auto startTime = std::chrono::high_resolution_clock::now();
		for (int32_t testValue = -ValueRange; testValue <= ValueRange; ++testValue)
		{
			// Reversing digits may result in a value that doesn't reverse back to the original (namely on values with trailing zeros)
			//	Unless you reverse at least once before-hand (i.e. 120 reverses to 21 reverses to 12 an back to 21)
			// We use this property to both validate the function results AND provide a means to avoid optimizing away the function calls.
			const int32_t result = Func(testValue);
			const int32_t doubleResult = Func(result);
			const int32_t thirdResult = Func(doubleResult);

			// This has to be here to make use of the values and ensure they're not optimized out.
			if (result != thirdResult)
			{
				std::println("!!!! Failed to maintain the value");
			}
		}
		const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime);

		timingList[repeatIndex] = duration;
		result.mean += duration;
	}

	std::ranges::sort(timingList);

	result.min = timingList[0];
	result.max = timingList[RepeatCount -1];
	result.median = timingList[RepeatCount /2];
	result.mean /= RepeatCount;
	std::print("\n");
	return result;
}









int main ()
{
	// These serve as both validation and process warmup
	validateDifferentOutputs(-1'987'654'321);
	validateDifferentOutputs(256);
	validateDifferentOutputs(-256);
	validateDifferentOutputs(12'345);
	validateDifferentOutputs(25);
	validateDifferentOutputs(-25);
	validateDifferentOutputs(2);
	validateDifferentOutputs(-2);
	validateDifferentOutputs(1);
	validateDifferentOutputs(-1);
	validateDifferentOutputs(0);
	validateDifferentOutputs(10);
	validateDifferentOutputs(9);
	validateDifferentOutputs(1'000'000'003);
	validateDifferentOutputs(-1'000'000'003);
	validateDifferentOutputs(std::numeric_limits<int32_t>::lowest());
	validateDifferentOutputs(std::numeric_limits<int32_t>::lowest()+1);
	validateDifferentOutputs(std::numeric_limits<int32_t>::max());
	validateDifferentOutputs(std::numeric_limits<int32_t>::max()-1);
	validateDifferentOutputs(2'000'000'008);
	validateDifferentOutputs(-2'000'000'008);
	validateDifferentOutputs(1'463'847'412);
	validateDifferentOutputs(-1'463'847'412);


	constexpr int32_t valueTestRange = 2'000'000;
	constexpr size_t repeatCount = 10;

	std::println("\nTiming functions {0}x over range [-{1:L}, {1:L}]. The functions will be called 3x per iteration", repeatCount, valueTestRange);
	std::println("Beginning function timing...\n");



	std::println("Timing 'Char Array Stack' function...");
	const TimingResult charArrayStackResult = timeFunction<&reverseDigits_CharArrayStack, valueTestRange, repeatCount>();


	std::println("Timing 'Char Array Stack - Range Algorithm' function...");
	const TimingResult charArrayStackAlgoResult = timeFunction<&reverseDigits_CharArrayStack_RangeAlgorithm, valueTestRange, repeatCount>();

	std::println("Timing 'Char Array Heap - Shared Alloc' function...");
	const TimingResult charArrayHeapSharedResult = timeFunction<&reverseDigits_CharArrayHeap_SharedAlloc, valueTestRange, repeatCount>();

	std::println("Timing 'Char Array Heap - Always Alloc' function...");
	const TimingResult charArrayHeapAllocResult = timeFunction<&reverseDigits_CharArrayHeap_AlwaysAlloc, valueTestRange, repeatCount>();

	std::println("Timing 'Modulo Lookup' function...");
	const TimingResult moduloLookupResult = timeFunction<&reverseDigits_ModuloLookup, valueTestRange, repeatCount>();

	std::println("Timing 'Modulo Multiply' function...");
	const TimingResult moduloMultiplyResult = timeFunction<&reverseDigits_ModuloMultiply, valueTestRange, repeatCount>();

	std::println("\n=====================================");
	std::println("  Results");
	std::println("=====================================\n");

	
	std::println("Char Stack               ({})", charArrayStackResult.toString());
	std::println("Char Stack - Range Algo  ({})", charArrayStackAlgoResult.toString());
	std::println("Char Heap - Shared Alloc ({})", charArrayHeapSharedResult.toString());
	std::println("Char Heap - Always Alloc ({})", charArrayHeapAllocResult.toString());
	std::println("Modulo Lookup            ({})", moduloLookupResult.toString());
	std::println("Modulo Multiply          ({})", moduloMultiplyResult.toString());

	std::print("\n");
	std::println("## NOTE: These times are not representative of a single function call, but 3 function calls per iteration over a negative -> positive value range.");
	std::println("## As such, the functions have been called {:L} times per timing cycle.", (static_cast<uint64_t>(valueTestRange) * 2ull * 3ull));

	return 0;
}
