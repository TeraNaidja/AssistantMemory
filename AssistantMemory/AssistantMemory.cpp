#include "AssistantMemory.h"

#include <cassert>
#include <crtdbg.h>
#include <new>
#include <cstdio>
#include <cstdint>
#include <mutex>

#include <cassert>
#include <Windows.h>
#include <DbgHelp.h>

namespace Detail
{
	class StackTraceSymbolLoader
	{
	public:
		StackTraceSymbolLoader()
		{
			HANDLE currentProcess = GetCurrentProcess();
			SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
			if (!SymInitialize(currentProcess, nullptr, TRUE))
			{
				char buffer[128];
				std::snprintf(buffer, sizeof(buffer), "Failed to initialize SymHandler. ErrorCode: %i", GetLastError());
				assert(false && buffer);
			}
		}

		~StackTraceSymbolLoader()
		{
			HANDLE currentProcess = GetCurrentProcess();
			SymCleanup(currentProcess);
		}
	};

	[[maybe_unused]] StackTraceSymbolLoader s_SymbolHelper;

	struct EAllocationFlag
	{
		static constexpr uint8_t IsArray = (1 << 0);
	};

	struct TrackingBlock
	{
		TrackingBlock* m_next{nullptr};
		TrackingBlock* m_prev{nullptr};
		uint8_t m_allocationScope{0};
		uint8_t m_allocationFlags{0};
		size_t m_allocationSizeBytes{ 0 };
		AssistantMemory::StackTrace m_allocationTrace;
	};

	static constexpr size_t MemoryAlignmentBytes = 8;
	static constexpr size_t TrackingBlockPadding = (sizeof(TrackingBlock)) & (MemoryAlignmentBytes - 1);
	static constexpr size_t TrackingBlockSizeWithPadding = sizeof(TrackingBlock) + TrackingBlockPadding;

	static TrackingBlock* g_lastTrackingBlock = nullptr;
	static std::atomic<uint8_t> g_trackingScopeIdentifier{ 0 };
	static std::mutex g_allocationMutex = {};

	void* DoAllocation(std::size_t a_memorySize, uint8_t a_allocationFlag, TrackingBlock*& a_resultBlock)
	{
		if (void* ptr = std::malloc(a_memorySize + TrackingBlockSizeWithPadding))
		{
			a_resultBlock = static_cast<TrackingBlock*>(ptr);
			*a_resultBlock = TrackingBlock();
			a_resultBlock->m_allocationScope = g_trackingScopeIdentifier;
			a_resultBlock->m_allocationFlags = a_allocationFlag;
			if constexpr (AssistantMemory::CaptureAllocationStackTrace)
			{
				a_resultBlock->m_allocationTrace = AssistantMemory::StackTrace::Capture(3); //Skip the DoAllocation and std::new frames
			}
			a_resultBlock->m_allocationSizeBytes = a_memorySize;

			{
				std::unique_lock<std::mutex> lock(g_allocationMutex);
				a_resultBlock->m_prev = g_lastTrackingBlock;
				if (g_lastTrackingBlock != nullptr)
				{
					g_lastTrackingBlock->m_next = a_resultBlock;
				}
				g_lastTrackingBlock = a_resultBlock;
			}

			return static_cast<void*>(static_cast<std::uint8_t*>(ptr) + TrackingBlockSizeWithPadding);
		}

		throw std::bad_alloc{};
	}

	void DoDeallocation(void* a_ptr, std::size_t /*a_memorySize*/, uint8_t& a_allocationFlags)
	{
		TrackingBlock* trackingBlock = reinterpret_cast<TrackingBlock*>(static_cast<uint8_t*>(a_ptr) - TrackingBlockSizeWithPadding);
		a_allocationFlags = trackingBlock->m_allocationFlags;

		{
			std::unique_lock<std::mutex> lock(g_allocationMutex);
			if (trackingBlock->m_next != nullptr)
			{
				trackingBlock->m_next->m_prev = trackingBlock->m_prev;
			}
			if (trackingBlock->m_prev != nullptr)
			{
				trackingBlock->m_prev->m_next = trackingBlock->m_next;
			}
			if (g_lastTrackingBlock == trackingBlock)
			{
				g_lastTrackingBlock = trackingBlock->m_prev;
			}
		}

		std::free(trackingBlock);
	}
}

void* operator new(std::size_t sz)
{
	if (sz == 0)
	{
		++sz; // avoid std::malloc(0) which may return nullptr on success
	}

	Detail::TrackingBlock* block;
	return DoAllocation(sz, 0, block);
}

// no inline, required by [replacement.functions]/3
void* operator new[](std::size_t sz)
{
	if (sz == 0)
	{
		++sz; // avoid std::malloc(0) which may return nullptr on success
	}

	Detail::TrackingBlock* block;
	return DoAllocation(sz, Detail::EAllocationFlag::IsArray, block);
}

void operator delete(void* ptr) noexcept
{
	std::puts("3) delete(void*)");
	std::free(ptr);
	assert(false);
}

void operator delete(void* ptr, std::size_t size) noexcept
{
	uint8_t flags;
	Detail::DoDeallocation(ptr, size, flags);
    assert((flags & Detail::EAllocationFlag::IsArray) == 0);
}

void operator delete[](void* ptr) noexcept
{
	uint8_t flags;
	Detail::DoDeallocation(ptr, 0, flags);
    assert((flags & Detail::EAllocationFlag::IsArray) == Detail::EAllocationFlag::IsArray);
}

void operator delete[](void* ptr, std::size_t size) noexcept
{
	uint8_t flags;
    Detail::DoDeallocation(ptr, size, flags);
    assert((flags & Detail::EAllocationFlag::IsArray) == Detail::EAllocationFlag::IsArray);
}

AssistantMemory::AllocationTracker::AllocationTracker()
	: m_trackingScope(++Detail::g_trackingScopeIdentifier)
{
}

AssistantMemory::AllocationTracker::~AllocationTracker() noexcept(false)
{
	if (ReportLeaks())
	{
		throw LeakDetectedException();
	}
	--Detail::g_trackingScopeIdentifier;
}

bool AssistantMemory::AllocationTracker::ReportLeaks() const
{
	bool foundLeakingInThisScope = false;
	Detail::TrackingBlock* block = Detail::g_lastTrackingBlock;
	std::vector<std::string> callStack;
	while (block != nullptr)
	{
		if (block->m_allocationScope >= m_trackingScope)
		{
			std::printf("Found leaking memory block at 0X%p, size: %llu bytes\n", block, block->m_allocationSizeBytes);
			std::printf("Allocation was performed at:\n");
			block->m_allocationTrace.ResolveSymbols(callStack);
			for (const std::string& frame : callStack)
			{
				std::printf("\t%s\n", frame.c_str());
			}

			foundLeakingInThisScope = true;
		}
		block = block->m_prev;
	}

	return foundLeakingInThisScope;
}

AssistantMemory::StackTrace AssistantMemory::StackTrace::Capture(int a_stackFramesToSkip)
{
	StackTrace result{};
	RtlCaptureStackBackTrace(a_stackFramesToSkip, AllocationStackTraceMaxLength, reinterpret_cast<void**>(result.m_stackTrace.data()), nullptr);
	return result;
}

void AssistantMemory::StackTrace::ResolveSymbols(std::vector<std::string>& a_callFrames)
{
	char symbolBuffer[512];

	DWORD64 offset;
	IMAGEHLP_SYMBOL64_PACKAGE symbol{};
	symbol.sym.SizeOfStruct = sizeof(symbol);
	symbol.sym.MaxNameLength = MAX_SYM_NAME;

	IMAGEHLP_LINE64 line{};

	HANDLE currentProcess = GetCurrentProcess();

	a_callFrames.reserve(AllocationStackTraceMaxLength);
	a_callFrames.clear();

	for (uint64_t stackFrame : m_stackTrace)
	{
		if (stackFrame == 0)
		{
			break;
		}

		if (SymGetSymFromAddr64(currentProcess, stackFrame, &offset, &symbol.sym))
		{
			int symbolLength = 0;
			DWORD dis;
			if (SymGetLineFromAddr64 (currentProcess, stackFrame, &dis, &line))
			{
				symbolLength = snprintf(symbolBuffer, sizeof(symbolBuffer), "%s Line %lu", symbol.sym.Name, line.LineNumber);
			}
			else
			{
				symbolLength = snprintf(symbolBuffer, sizeof(symbolBuffer), "%s", symbol.sym.Name);
			}
			a_callFrames.emplace_back(symbolBuffer, symbolLength);
		}
		else
		{
			assert(false && "Failed to get symbol from address");
		}
	}
}