// TestApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "AssistantMemory.h"

int main(int argc, char** argv)
{
	AssistantMemory::AllocationTracker t;

//	for (int i = 0; i < 10; ++i)
//	{
//		int* a = new int(10);
//
//		*a = 20;
//
//		delete a;
//	}

    std::cout << "Hello World!\n";

	int* b = new int(10);
}

