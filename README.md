# AssistantMemory
A bare-bones memory tracker aimed at giving some more training wheels when it comes to proper memory management.
- Memory leak detection
- Mismatching new/delete array/instance types.

# Limitations
Currently it is only tested to work on Windows x64 with MSVC.
Only complains about memory leaking out of the scope the AllocationTracker is installed, so static memory is somewhat ignored.

# Installation
- Add the AssistantMemory.h and AssistantMemory.cpp files to your project
- Add an instance of the AssistantMemory::AllocationTracker class to the scope you want to monitor
```cpp
int main(int argc, char** argv)
{
	AssistantMemory::AllocationTracker t;
	//[...]
}
```
- Feature configuration values can be found on top of the AssistantMemory.h file.