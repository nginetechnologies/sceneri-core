// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <mutex>
JPH_SUPPRESS_WARNINGS_STD_END

#include <Core/NonCopyable.h>
#include <Core/TickCounter.h>
#include <Core/UnorderedMap.h>

#if defined(JPH_EXTERNAL_PROFILE)

JPH_NAMESPACE_BEGIN

/// Create this class on the stack to start sampling timing information of a particular scope.
///
/// Left unimplemented intentionally. Needs to be implemented by the user of the library.
/// On construction a measurement should start, on destruction it should be stopped.
class alignas(16) ExternalProfileMeasurement : public NonCopyable
{	
public:						
	/// Constructor
									ExternalProfileMeasurement(const char *inName, uint32 inColor = 0);
									~ExternalProfileMeasurement();

private:
	uint8							mUserData[64];
};

JPH_NAMESPACE_END

//////////////////////////////////////////////////////////////////////////////////////////
// Macros to do the actual profiling	
//////////////////////////////////////////////////////////////////////////////////////////

JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Wc++98-compat-pedantic")

// Dummy implementations
#define JPH_PROFILE_THREAD_START(name)			
#define JPH_PROFILE_THREAD_END()				
#define JPH_PROFILE_NEXTFRAME()			
#define JPH_PROFILE_DUMP(...)				
								
// Scope profiling measurement
#define JPH_PROFILE_TAG2(line)		profile##line
#define JPH_PROFILE_TAG(line)		JPH_PROFILE_TAG2(line)

/// Macro to collect profiling information.
///
/// Usage:
///
///		{
///			JPH_PROFILE("Operation");
///			do operation;
///		}
///
#define JPH_PROFILE(...)			ExternalProfileMeasurement JPH_PROFILE_TAG(__LINE__)(__VA_ARGS__)

// Scope profiling for function
#define JPH_PROFILE_FUNCTION()		JPH_PROFILE(JPH_FUNCTION_NAME)

JPH_SUPPRESS_WARNING_POP

#elif defined(JPH_PROFILE_ENABLED)

JPH_NAMESPACE_BEGIN

class ProfileSample;
class ProfileThread;

/// Singleton class for managing profiling information
class Profiler : public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Increments the frame counter to provide statistics per frame
	void						NextFrame();

	/// Dump profiling statistics at the start of the next frame
	/// @param inTag If not empty, this overrides the auto incrementing number in the filename of the dump file
	void						Dump(const string_view &inTag = string_view());

	/// Add a thread to be instrumented
	void						AddThread(ProfileThread *inThread);

	/// Remove a thread from being instrumented
	void						RemoveThread(ProfileThread *inThread);

	/// Singleton instance
	static Profiler *			sInstance;
								
private:
	/// Helper class to freeze ProfileSamples per thread while processing them
	struct ThreadSamples
	{
		String					mThreadName;
		ProfileSample *			mSamplesBegin;
		ProfileSample *			mSamplesEnd;
	};

	/// Helper class to aggregate ProfileSamples
	class Aggregator			
	{	
	public:						
		/// Constructor
								Aggregator(const char *inName)										: mName(inName) { }
								
		/// Accumulate results for a measurement
		void					AccumulateMeasurement(uint64 inCyclesInCallWithChildren, uint64 inCyclesInChildren)
		{
			mCallCounter++;
			mTotalCyclesInCallWithChildren += inCyclesInCallWithChildren;
			mTotalCyclesInChildren += inCyclesInChildren;
			mMinCyclesInCallWithChildren = min(inCyclesInCallWithChildren, mMinCyclesInCallWithChildren);
			mMaxCyclesInCallWithChildren = max(inCyclesInCallWithChildren, mMaxCyclesInCallWithChildren);
		}

		/// Sort descending by total cycles
		bool					operator < (const Aggregator &inRHS) const
		{
			return mTotalCyclesInCallWithChildren > inRHS.mTotalCyclesInCallWithChildren;
		}

		/// Identification			
		const char *			mName;																///< User defined name of this item
								
		/// Statistics				
		uint32					mCallCounter = 0;													///< Number of times AccumulateMeasurement was called
		uint64					mTotalCyclesInCallWithChildren = 0;									///< Total amount of cycles spent in this scope
		uint64					mTotalCyclesInChildren = 0;											///< Total amount of cycles spent in children of this scope
		uint64					mMinCyclesInCallWithChildren = 0xffffffffffffffffUL;				///< Minimum amount of cycles spent per call
		uint64					mMaxCyclesInCallWithChildren = 0;									///< Maximum amount of cycles spent per call
	};							

	using Threads = Array<ThreadSamples>;
	using Aggregators = Array<Aggregator>;
	using KeyToAggregator = UnorderedMap<const char *, size_t>;

	/// Helper function to aggregate profile sample data
	static void					sAggregate(int inDepth, uint32 inColor, ProfileSample *&ioSample, const ProfileSample *inEnd, Aggregators &ioAggregators, KeyToAggregator &ioKeyToAggregator);

	/// Dump profiling statistics
	void						DumpInternal();
	void						DumpList(const char *inTag, const Aggregators &inAggregators);
	void						DumpChart(const char *inTag, const Threads &inThreads, const KeyToAggregator &inKeyToAggregators, const Aggregators &inAggregators);

	mutex						mLock;																///< Lock that protects mThreads
	Array<ProfileThread *>		mThreads;															///< List of all active threads
	bool						mDump = false;														///< When true, the samples are dumped next frame
	String						mDumpTag;															///< When not empty, this overrides the auto incrementing number of the dump filename
};							

// Class that contains the information of a single scoped measurement
class alignas(16) ProfileSample : public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	const char *				mName;																///< User defined name of this item
	uint32						mColor;																///< Color to use for this sample
	uint8						mDepth;																///< Calculated depth
	uint8						mUnused[3];
	uint64						mStartCycle;														///< Cycle counter at start of measurement
	uint64						mEndCycle;															///< Cycle counter at end of measurement
};

/// Collects all samples of a single thread
class ProfileThread : public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	inline						ProfileThread(const string_view &inThreadName);
	inline						~ProfileThread();

	static const uint cMaxSamples = 65536;

	String						mThreadName;														///< Name of the thread that we're collecting information for
	ProfileSample				mSamples[cMaxSamples];												///< Buffer of samples
	uint						mCurrentSample = 0;													///< Next position to write a sample to

	static thread_local ProfileThread *sInstance;
};

/// Create this class on the stack to start sampling timing information of a particular scope
class ProfileMeasurement : public NonCopyable
{	
public:						
	/// Constructor
	inline						ProfileMeasurement(const char *inName, uint32 inColor = 0);
	inline						~ProfileMeasurement();
							
private:
	ProfileSample *				mSample;
	ProfileSample				mTemp;

	static bool					sOutOfSamplesReported;
};

JPH_NAMESPACE_END

#include "Profiler.inl"

//////////////////////////////////////////////////////////////////////////////////////////
// Macros to do the actual profiling	
//////////////////////////////////////////////////////////////////////////////////////////

JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Wc++98-compat-pedantic")

/// Start instrumenting program
#define JPH_PROFILE_START(name)			do { Profiler::sInstance = new Profiler; JPH_PROFILE_THREAD_START(name); } while (false)

/// End instrumenting program
#define JPH_PROFILE_END()				do { JPH_PROFILE_THREAD_END(); delete Profiler::sInstance; Profiler::sInstance = nullptr; } while (false)

/// Start instrumenting a thread
#define JPH_PROFILE_THREAD_START(name)	do { if (Profiler::sInstance) ProfileThread::sInstance = new ProfileThread(name); } while (false)

/// End instrumenting a thread
#define JPH_PROFILE_THREAD_END()		do { delete ProfileThread::sInstance; ProfileThread::sInstance = nullptr; } while (false)
								
/// Scope profiling measurement
#define JPH_PROFILE_TAG2(line)			profile##line
#define JPH_PROFILE_TAG(line)			JPH_PROFILE_TAG2(line)
#define JPH_PROFILE(...)				ProfileMeasurement JPH_PROFILE_TAG(__LINE__)(__VA_ARGS__)

/// Scope profiling for function
#define JPH_PROFILE_FUNCTION()			JPH_PROFILE(JPH_FUNCTION_NAME)
								
/// Update frame counter								
#define JPH_PROFILE_NEXTFRAME()			Profiler::sInstance->NextFrame()

/// Dump profiling info
#define JPH_PROFILE_DUMP(...)			Profiler::sInstance->Dump(__VA_ARGS__)

JPH_SUPPRESS_WARNING_POP

#else

//////////////////////////////////////////////////////////////////////////////////////////
// Dummy profiling instructions
//////////////////////////////////////////////////////////////////////////////////////////

JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Wc++98-compat-pedantic")

#define JPH_PROFILE_START(name)
#define JPH_PROFILE_END()
#define JPH_PROFILE_THREAD_START(name)
#define JPH_PROFILE_THREAD_END()
#define JPH_PROFILE(...)
#define JPH_PROFILE_FUNCTION()
#define JPH_PROFILE_NEXTFRAME()
#define JPH_PROFILE_DUMP(...)

JPH_SUPPRESS_WARNING_POP

#endif
