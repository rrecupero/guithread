#pragma once

#include <atomic>
#include <vcclr.h>
using namespace System;
using namespace System::Collections::Concurrent;

extern std::atomic<bool> capturing;  

public ref class GlobalData
{
private:
    static initonly ConcurrentQueue<array<unsigned short>^>^ dataQueue = gcnew ConcurrentQueue<array<unsigned short>^>();
    static int someGlobalValue = 0;

    // Private constructor to prevent instantiation
    GlobalData() {}

public:
    // Static method to access the singleton instance
    static property ConcurrentQueue<array<unsigned short>^>^ DataQueue
    {
        ConcurrentQueue<array<unsigned short>^>^ get() { return dataQueue; }
    }

    static property int SomeGlobalValue
    {
        int get() { return someGlobalValue; }
        void set(int value) { someGlobalValue = value; }
    }
};
