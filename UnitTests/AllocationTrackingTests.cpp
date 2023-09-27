#include "pch.h"
#include "CppUnitTest.h"

#include "AllocationTracker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MemoryTrainerTests
{
	TEST_CLASS(AllocationTrackingTests)
	{
	public:
		TEST_METHOD(SingleAllocation)
		{
			AssistantMemory::AllocationTracker t;

			int* a = new int(10);
			*a = 20;
			delete a;
		}

		TEST_METHOD(MultipleSingleAllocations)
		{
			AssistantMemory::AllocationTracker t;

			for (int i = 0; i < 10; ++i)
			{
				int* a = new int(10);
				*a = 20;
				delete a;
			}
		}

		TEST_METHOD(Leak)
		{
			bool hadException = false;
			try
			{
				AssistantMemory::AllocationTracker t;

				new int(10);
			}
			catch (const AssistantMemory::LeakDetectedException&)
			{
				hadException = true;
			}

			Assert::IsTrue(hadException);
		}

		TEST_METHOD(RandomOrderedAllocations)
		{
			int destructionOrder[] = { 3, 1, 6, 5, 8, 2, 4, 7, 9, 0 };

			AssistantMemory::AllocationTracker t;

			int* allocation[10]{};
			for (int i = 0; i < 10; ++i)
			{
				allocation[i] = new int(10);
			}

			for (int dest : destructionOrder)
			{
				delete allocation[dest];
			}
		}

		TEST_METHOD(SingleArrayAllocation)
		{
			AssistantMemory::AllocationTracker t;

			int* array = new int[10];

			delete[] array;
		}

		TEST_METHOD(MismatchArrayNewDelete)
		{
			AssistantMemory::AllocationTracker t;

			int* array = new int[10];

			delete array;
		}
	};
};