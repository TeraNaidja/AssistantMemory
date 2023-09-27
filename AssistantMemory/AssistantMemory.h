#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace AssistantMemory
{
	static constexpr bool CaptureAllocationStackTrace{ true };
	static constexpr int AllocationStackTraceMaxLength = 8;

	class LeakDetectedException : public std::exception
	{
	};

	class AllocationTracker
	{
	public:
		AllocationTracker();
		AllocationTracker(const AllocationTracker&) = delete;
		AllocationTracker(AllocationTracker&&) = delete;

		AllocationTracker& operator=(const AllocationTracker&) = delete;
		AllocationTracker& operator=(AllocationTracker&&) = delete;

		~AllocationTracker() noexcept(false);

	private:
		bool ReportLeaks() const;

		uint8_t m_trackingScope{};
	};

	class StackTrace
	{
	public:
		StackTrace() = default;

		static StackTrace Capture(int a_stackFramesToSkip = 0);

		void ResolveSymbols(std::vector<std::string>& a_callFrames);

	private:
		std::array<uint64_t, AllocationStackTraceMaxLength> m_stackTrace{};
	};
};