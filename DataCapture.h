#pragma once

using namespace System;
using namespace System::Threading;


ref class DataCapture {
public:
    DataCapture(WRRUSB2_DLL::WRRUSB2^ sharedDevice);
    void StartCapture();
    void StopCapture();
    bool IsCapturing();
   

    

    // Setter function to assign an external function (callback)
    void SetCaptureFunction(long (*func)(WRRUSB2_DLL::WRRUSB2^ objWrrUSB));

private:
    
    Thread^ captureThread;
    long (*CaptureFunction)(WRRUSB2_DLL::WRRUSB2^ objWrrUSB);  // Function pointer
    void RunCaptureLoop();
    WRRUSB2_DLL::WRRUSB2^ device;
    
};
