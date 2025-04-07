#pragma once
#include "stdafx.h"

using namespace System;

public ref class DataSaver {
public:
    // Flag to signal the save thread to stop.
    volatile bool stopSaver;

    // Constructor.
    DataSaver();

    

    // This method will run on its own thread.
    void SaveData(array<unsigned short>^ data);

private:
    
};
