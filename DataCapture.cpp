#include "stdafx.h"
#include "DataCapture.h"
#include "GlobalData.h"
#include <thread>
#include <iostream>
#include <atomic>

using namespace System;
DataCapture::DataCapture(WRRUSB2_DLL::WRRUSB2^ sharedDevice) {
    
    captureThread = nullptr;
    CaptureFunction = nullptr;
    device = sharedDevice;  // Store shared device instance
}

void DataCapture::SetCaptureFunction(long (*func)(WRRUSB2_DLL::WRRUSB2^ objWrrUSB)) {
    CaptureFunction = func;
}

void DataCapture::StartCapture() {
    if (capturing.load()) {  
        Console::WriteLine("Capture is already running.");
        return;
    }

    if (CaptureFunction == nullptr) {
        Console::WriteLine("Capture function is not set.");
        return;
    }

    capturing.store(true);  
    Console::WriteLine("Starting capture...");
    captureThread = gcnew Thread(gcnew ThreadStart(this, &DataCapture::RunCaptureLoop));
    captureThread->Start();
}

void DataCapture::RunCaptureLoop() {
    if (!device) {
        device = gcnew WRRUSB2_DLL::WRRUSB2();
    }

    Console::WriteLine("Starting Capture...");

    // Only call ContinuousMeasurement once
    CaptureFunction(device);

    Console::WriteLine("Capture Stopped.");
}


void DataCapture::StopCapture() {
    if (!capturing.load()) {  
        Console::WriteLine("Capture is not running.");
        return;
    }

    capturing.store(false); 
    Console::WriteLine("Stopping capture gracefully...");

    if (captureThread != nullptr) {
        captureThread->Join(); 
        captureThread = nullptr;
    }
}

bool DataCapture::IsCapturing() {
    return capturing.load();
}
