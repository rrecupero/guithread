/* =================================================================================

MDE - S25-35

Raman Spectrometer Custom Code
--------------------------------------------------------------------------------
FileName: SampleAPL.cpp

================================================================================= */
//-----------------------------------------------------------------------------
// include
//-----------------------------------------------------------------------------




#include "stdafx.h"
#include <vcclr.h>
#include "GlobalData.h"      
#include "DataCapture.h"     
#include "DataSaver.h"
#include <mutex>


//-----------------------------------------------------------------------------
// define
//-----------------------------------------------------------------------------
#define  IMAGE_HEADER_SIZE                  256     //Header size
#define  DATA_COUNT_MAX_VALUE               1       //Data count
#define  DATA_TRANSMIT_COUNT                1       //Data transmit count
#define  FREQUENCYMODE_FRAME_COUNT          1       //Frame count(Frequency specified mode)
#define  OTHERMODE_FRAME_COUNT              5       //Frame count(Capture mode continuous and trigger)
#define  SINGLE_FRAME_COUNT                 1 
#define  EXPOSURE_TIME                      20000   //Exposure Time(usec)
#define  CYCLE_TIME                         210000  //Cycle time(usec)
#define  REPEAT_COUNT                       5       //Repeat count



//-----------------------------------------------------------------------------
// namespace
//-----------------------------------------------------------------------------
using namespace System;
using namespace System::IO;
using namespace System::Collections::Generic;
using namespace System::Threading;
using namespace System::Runtime::InteropServices;
using namespace System::Diagnostics;
using namespace WRRUSB2_DLL;
using namespace System::Collections::Concurrent;




//-----------------------------------------------------------------------------
// global
//-----------------------------------------------------------------------------
std::mutex deviceMutex; // Define a mutex
int g_nDeviceId;  // Device ID
gcroot<array<unsigned short>^> gCapturedData; //Image data

//-----------------------------------------------------------------------------
// enum
//-----------------------------------------------------------------------------
enum class E_CAPTUREMODE {                               //Capture mode
    D_CAPTUREMODE_COUNT = 0x00000000,                   //Frequency specified meausrement mode
    D_CAPTUREMODE_CONTINUOUS = 0x00000001,              //Continous measurement mode
    D_CAPTUREMODE_TRIGGER = 0x00000002,                 //Trigger mode

};

enum class E_TRIGGERMODE {
    D_TRIGGERMODE_INTERNAL = 0x00000000,                //Internal synchronous mode
    D_TRIGGERMODE_SOFTASYNC = 0x00000001,               //Software asynchronous mode
    D_TRIGGERMODE_SOFTSYNC = 0x00000002,                //Software synchronous mode
    D_TRIGGERMODE_EXTASYNCEDGE = 0x00000003,            //External asynchronous edge sense mode
    D_TRIGGERMODE_EXTASYNCLEVEL = 0x00000004,           //External asynchronous level sense mode
    D_TRIGGERMODE_EXTSYNCEDGE = 0x00000005,             //External synchronous edge sense mode
    D_TRIGGERMODE_EXTSYNCLEVEL = 0x00000006,            //External synchronous level sense mode
    D_TRIGGERMODE_EXTSYNCPULSE = 0x00000007,            //External synchronous pulse mode
};

enum class E_CALIBRATIONCOEFFICIENT {
    D_CALIBRATIONCOEFFICIENT_WAVELENGTH = 0x00000000,   //Wavelength calibration coefficient
    D_CALIBRATIONCOEFFICIENT_SENSIBILITY = 0x00000001,  //Sensibility calibration coefficient
};

enum class E_GAINMODE {
    D_GAINMODE_SENSOR = 0x00000000,                     //SensorGainMode
    D_GAINMODE_CIRCUIT = 0x00000001,                    //CircuitGainMode
};

enum class E_SENSORGAINMODE {
    D_SENSORGAINMODE_LOWGAIN = 0x00000000,              //Low Gain
    D_SENSORGAINMODE_HIGHGAIN = 0x00000001,             //High Gain
    D_SENSORGAINMODE_NOTHING = 0x000000FF,              //Nothing
};


//-----------------------------------------------------------------------------
// function
//-----------------------------------------------------------------------------
long InitializeDevice(WRRUSB2^ objWrrUSB);
long UninitializeDevice(WRRUSB2^ objWrrUSB);
void PrintMenu(void);
long StartMeasureForFrequencyMode(WRRUSB2^ objWrrUSB);
long StartMeasureForContinuousMode(WRRUSB2^ objWrrUSB);
long StartMeasureForTriggerMode(WRRUSB2^ objWrrUSB);
long StartMeasureForSingleSpectrum(WRRUSB2^ objWrrUSB);
long GetDeivceInformation(WRRUSB2^ objWrrUSB);
long GetCalibrationValue(WRRUSB2^ objWrrUSB);
long GetLDPower(WRRUSB2^ objWrrUSB);
long SetLDPower(WRRUSB2^ objWrrUSB);
long GetLDPowerEnable(WRRUSB2^ objWrrUSB);
long SetLDPowerEnable(WRRUSB2^ objWrrUSB);
long GetLDTemperature(WRRUSB2^ objWrrUSB);
long GetLDBoardTemperature(WRRUSB2^ objWrrUSB);
bool DisplayData(array<unsigned short>^ aryusImageData);
bool SaveData(array<unsigned short>^ aryusImageData);
long ContinuousMeasurement(WRRUSB2_DLL::WRRUSB2^ objWrrUSB);
void SetupCapture(DataCapture^ capture);
void OnProcessExit(Object^ sender, EventArgs^ e);



String^ CheckCommandFile() {
    String^ commandFilePath = "command.txt";
    String^ completedFilePath = "command_completed.txt";

    try {
        // Debug the current directory
        //Console::WriteLine("[Debug] Checking for command file at: " +
        //    System::IO::Path::GetFullPath(commandFilePath));
        //Console::Out->Flush();

        if (System::IO::File::Exists(commandFilePath)) {
            Console::WriteLine("[Debug] Command file found!");
            Console::Out->Flush();

            // Read the command
            String^ command = System::IO::File::ReadAllText(commandFilePath);
            Console::WriteLine("[Debug] Read command: " + command);
            Console::Out->Flush();

            // Delete command file
            System::IO::File::Delete(commandFilePath);
            Console::WriteLine("[Debug] Deleted command file");
            Console::Out->Flush();

            // Write to completion file
            System::IO::File::WriteAllText(completedFilePath, "done");
            Console::WriteLine("[Debug] Wrote completion file");
            Console::Out->Flush();

            return command;
        }
    }
    catch (System::Exception^ ex) {
        Console::WriteLine("[Error] File operation failed: " + ex->Message);
        Console::WriteLine("[Error] Stack trace: " + ex->StackTrace);
        Console::Out->Flush();
    }

    return nullptr;
}

//-----------------------------------------------------------------------------
// main function
//-----------------------------------------------------------------------------
int main(array<System::String^>^ args)
{
    try {
        // Test file writing capability
        System::IO::File::WriteAllText("test_write.txt", "Test write access");
        Console::WriteLine("[Debug] Successfully wrote test file");
        System::IO::File::Delete("test_write.txt");
        Console::WriteLine("[Debug] Successfully deleted test file");
        Console::Out->Flush();
    }
    catch (System::Exception^ ex) {
        Console::WriteLine("[Error] File permission test failed: " + ex->Message);
        Console::Out->Flush();
    }


    Console::WriteLine("[main] in main");
    Console::Out->Flush();
    long lReturn = 0;
    int nLoopFlag = 1;
    char cInput = ' ';

    Console::WriteLine("Portable Ramanspectrometer Application");
    Console::Out->Flush();


    WRRUSB2^ objWrrUSB = nullptr;

    //Create object of WRRUSB2 class
    objWrrUSB = gcnew WRRUSB2();
    if (objWrrUSB == nullptr) {
        return 1;
    }
    Console::WriteLine("[main] created object");
    Console::Out->Flush();



    //Device initialize
    lReturn = InitializeDevice(objWrrUSB);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success))
    {
        return 1;
    }
    Console::WriteLine("initialized device");
    Console::Out->Flush();
    AppDomain::CurrentDomain->ProcessExit += gcnew EventHandler(OnProcessExit);



    // --- Set up the multi-threaded modules ---

    DataCapture^ capture = gcnew DataCapture(objWrrUSB);
    DataSaver^ saver = gcnew DataSaver();
    SetupCapture(capture);

    PrintMenu();
    Console::Out->Flush();

    while (nLoopFlag == 1) {


        // Check for commands from file
        String^ strInput = CheckCommandFile();

        // If no command, wait briefly and continue
        if (strInput == nullptr) {
            System::Threading::Thread::Sleep(100);
            continue;
        }

        Console::WriteLine("[Debug] Received input: '" + strInput + "'");
        Console::Out->Flush();

        // Process input as before
        strInput = strInput->Trim();
        if (String::IsNullOrWhiteSpace(strInput)) continue;

        char cInput = Char::ToUpper(strInput[0]);



        switch (cInput)
        {
        case '0':
            PrintMenu();
            Console::Out->Flush();

            break;

        case '1':
            lReturn = GetDeivceInformation(objWrrUSB);
            break;

        case '2':
            lReturn = GetCalibrationValue(objWrrUSB);
            break;

        case '3':
            lReturn = GetLDPower(objWrrUSB);
            break;

        case '4':
            lReturn = SetLDPower(objWrrUSB);
            break;

        case '5':
            lReturn = GetLDPowerEnable(objWrrUSB);
            break;

        case '6':
            lReturn = SetLDPowerEnable(objWrrUSB);
            break;

        case '7':
            lReturn = GetLDTemperature(objWrrUSB);
            break;

        case '8':
            lReturn = GetLDBoardTemperature(objWrrUSB);
            break;

        case 'F':
            lReturn = StartMeasureForFrequencyMode(objWrrUSB);
            break;

        case 'G':
            lReturn = StartMeasureForContinuousMode(objWrrUSB);
            break;

        case 'H':
            lReturn = StartMeasureForTriggerMode(objWrrUSB);
            break;

        case 'S':
            lReturn = StartMeasureForSingleSpectrum(objWrrUSB);
            break;

        case 'R': // Start Capture
            if (!capturing.load()) {
                capture->StartCapture();
            }
            else {
                Console::WriteLine("Capture is already running.");
                Console::Out->Flush();

            }
            break;

        case 'T': // Stop Capture
            if (capturing.load()) {
                capture->StopCapture();
            }
            else {
                Console::WriteLine("Capture is not running.");
                Console::Out->Flush();
            }
            break;

        case 'C': // Save Data Manually
            if (gCapturedData != nullptr) {
                saver->SaveData(gCapturedData);
            }
            else {
                Console::WriteLine("No data available to save.");
                Console::Out->Flush();

            }
            break;

        case 'Q':
            nLoopFlag = 0;
            break;

        default:
            break;
        }
    }


    //Device uninitialize
    lReturn = UninitializeDevice(objWrrUSB);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success))
    {
        return 1;
    }

    return 0;
}


void OnProcessExit(Object^ sender, EventArgs^ e) {
    if (g_nDeviceId != 0) {
        WRRUSB2^ tempUsb = gcnew WRRUSB2();
        tempUsb->USB2_close(g_nDeviceId);
        tempUsb->USB2_uninitialize();
    }
}


//-----------------------------------------------------------------------------
// Initialize device function
//-----------------------------------------------------------------------------
long InitializeDevice(WRRUSB2^ objWrrUSB)
{
    long    lReturn = 0;
    unsigned short usNum = 0;
    array<short>^ aryDeviceId = nullptr;

    //Argument error check
    if (objWrrUSB == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    //Initialization
    lReturn = (long)objWrrUSB->USB2_initialize();
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: Dll Can not be Initialized.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();
        return lReturn;
    }

    //Device ID storage area allocation
    aryDeviceId = gcnew array<short>(8);
    if (aryDeviceId == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }
    Array::Clear(aryDeviceId, 0, aryDeviceId->Length);

    //Get connected device list
    lReturn = (long)objWrrUSB->USB2_getModuleConnectionList(aryDeviceId, usNum);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        objWrrUSB->USB2_uninitialize();
        Console::WriteLine("Error code 0x{0:x04}: Module Connection list failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();
        return lReturn;
    }

    //Set acquired device ID
    g_nDeviceId = aryDeviceId[0];

    //Open device
    lReturn = (long)objWrrUSB->USB2_open(g_nDeviceId);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        objWrrUSB->USB2_uninitialize();
        Console::WriteLine("Error code 0x{0:x04}: Device can not be open.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();
        return lReturn;
    }

    return lReturn;
}

//-----------------------------------------------------------------------------
// Uninitialize device function
//-----------------------------------------------------------------------------
long UninitializeDevice(WRRUSB2^ objWrrUSB)
{
    long    lReturn = 0;

    //Argument error check
    if (objWrrUSB == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    //Close device
    lReturn = (long)objWrrUSB->USB2_close(g_nDeviceId);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: Device can not be closed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        return lReturn;
    }

    //Uninitialization
    lReturn = (long)objWrrUSB->USB2_uninitialize();
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_uninitialize failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();
    }

    return lReturn;
}

//-----------------------------------------------------------------------------
// Get device information function
//-----------------------------------------------------------------------------
long GetDeivceInformation(WRRUSB2^ objWrrUSB)
{
    long lReturn = 0;
    Usb2Struct::CModuleInformation^ pModuleInfo = nullptr;
    Usb2Struct::CSpectroInformation^ pSpecInfo = nullptr;

    //Argument error check
    if (objWrrUSB == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    try {
        pModuleInfo = gcnew Usb2Struct::CModuleInformation();
        pSpecInfo = gcnew Usb2Struct::CSpectroInformation();
    }
    catch (OutOfMemoryException^) {
        // Debug::Assert(false);
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    lReturn = (long)objWrrUSB->USB2_getModuleInformation(g_nDeviceId, pModuleInfo);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getModuleInformation failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
    }

    lReturn = (long)objWrrUSB->USB2_getSpectroInformation(g_nDeviceId, pSpecInfo);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getSpectroInformation failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
    }

    Console::WriteLine("Deivce information");
    Console::WriteLine("ModelName : {0}", pModuleInfo->product);
    Console::WriteLine("SerialNo  : {0}", pModuleInfo->serial);
    Console::WriteLine("UnitID    : {0}", pSpecInfo->unit);
    Console::WriteLine("FirmVer   : {0}", pModuleInfo->firmware);
    Console::Out->Flush();


    return lReturn;
}

//-----------------------------------------------------------------------------
// Get calibration value function
//-----------------------------------------------------------------------------
long GetCalibrationValue(WRRUSB2^ objWrrUSB)
{
    long lReturn = 0;
    array<double>^ arydblDataArray = nullptr;

    //Argument error check
    if (objWrrUSB == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    try {
        arydblDataArray = gcnew array<double>(6);
    }
    catch (OutOfMemoryException^) {
        //Debug::Assert(false);
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    lReturn = (long)objWrrUSB->USB2_getCalibrationCoefficient(g_nDeviceId, (long)E_CALIBRATIONCOEFFICIENT::D_CALIBRATIONCOEFFICIENT_WAVELENGTH, arydblDataArray);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getCalibrationCoefficient failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    Console::WriteLine("Calibration coefficients");
    Console::WriteLine("  A0 : {0:E}", arydblDataArray[0]);
    Console::WriteLine("  B1 : {0:E}", arydblDataArray[1]);
    Console::WriteLine("  B2 : {0:E}", arydblDataArray[2]);
    Console::WriteLine("  B3 : {0:E}", arydblDataArray[3]);
    Console::WriteLine("  B4 : {0:E}", arydblDataArray[4]);
    Console::WriteLine("  B5 : {0:E}", arydblDataArray[5]);
    Console::WriteLine("");
    Console::Out->Flush();


    return lReturn;
}

//-----------------------------------------------------------------------------
// Get LD Power
//-----------------------------------------------------------------------------
long GetLDPower(WRRUSB2^ objWrrUSB)
{
    long lReturn = 0;
    Int64 lLDPower = 0;

    //Argument error check
    if (objWrrUSB == nullptr) {
        return (long)(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    lReturn = (long)objWrrUSB->USB2_getLDPower(g_nDeviceId, lLDPower);
    if (lReturn != (long)Usb2Struct::Cusb2Err::usb2Success) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getLDPower failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }

        return lReturn;
    }
    Console::WriteLine("LD Power : 0x{0:X08}", lLDPower);
    Console::WriteLine("");
    Console::Out->Flush();


    return lReturn;
}

//-----------------------------------------------------------------------------
// Set LD Power
//-----------------------------------------------------------------------------
long SetLDPower(WRRUSB2^ objWrrUSB)
{
    long lReturn = 0;
    Int64 lLDPower = 0;
    String^ strNum = "";

    //Argument error check
    if (objWrrUSB == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    Console::WriteLine("Input LDPower (long)");
    Console::WriteLine("LOW    : 1");
    Console::WriteLine("MIDDLE : 2");
    Console::WriteLine("HIGH   : 3");
    Console::Out->Flush();

    Console::Write("Set LD Power > ");
    strNum = Console::ReadLine();
    Int64::TryParse(strNum, lLDPower);

    lReturn = (long)objWrrUSB->USB2_setLDPower(g_nDeviceId, lLDPower);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setLDPower failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }
    Console::WriteLine("");
    Console::Out->Flush();


    return lReturn;
}

/// <summary>
/// Get LD Power enable
/// </summary>
/// <param name="objWrrUSB"></param>
/// <returns></returns>
long GetLDPowerEnable(WRRUSB2^ objWrrUSB)
{
    long lReturn = 0;
    Int64 lLDPowerEnable = 0;

    //Argument error check
    if (objWrrUSB == nullptr)
    {
        return (long)Usb2Struct::Cusb2Err::usb2Err_unsuccess;
    }

    lReturn = (long)objWrrUSB->USB2_getLDPowerEnable(g_nDeviceId, lLDPowerEnable);
    if (lReturn != (long)Usb2Struct::Cusb2Err::usb2Success)
    {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getLDPowerEnable failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }
    Console::WriteLine("LD Power Enable : 0x{0:x08}", lLDPowerEnable);
    Console::WriteLine("");
    Console::Out->Flush();


    return lReturn;
}

/// <summary>
/// Set LD Power enable
/// </summary>
/// <param name="objWrrUSB"></param>
/// <returns></returns>
long SetLDPowerEnable(WRRUSB2^ objWrrUSB)
{
    long lReturn = 0;
    Int64 lLDPowerEnable = 0;

    //Argument error check
    if (objWrrUSB == nullptr)
    {
        return (long)Usb2Struct::Cusb2Err::usb2Err_unsuccess;
    }

    try
    {
        Console::WriteLine("Input LDPowerEnable (long)");
        Console::WriteLine("Enable  : 1");
        Console::WriteLine("Disable : Other than 1");
        Console::Out->Flush();

        Console::Write("Set LD Power Enable > ");

        Int64::TryParse(Console::ReadLine(), lLDPowerEnable);
    }
    catch (Exception^)
    {
        //  Debug::Assert(false);
        return (long)Usb2Struct::Cusb2Err::usb2Err_unsuccess;
    }

    lReturn = (long)objWrrUSB->USB2_setLDPowerEnable(g_nDeviceId, lLDPowerEnable);
    if (lReturn != (long)Usb2Struct::Cusb2Err::usb2Success)
    {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setLDPowerEnable failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }
    Console::WriteLine("");
    Console::Out->Flush();


    return lReturn;
}

/// <summary>
/// Get LD temperature
/// </summary>
/// <param name="objWrrUSB"></param>
/// <returns></returns>
long GetLDTemperature(WRRUSB2^ objWrrUSB)
{
    long lReturn = 0;
    Int64 lLDTemp = 0;

    //Argument error check
    if (objWrrUSB == nullptr)
    {
        return (long)Usb2Struct::Cusb2Err::usb2Err_unsuccess;
    }

    lReturn = (long)objWrrUSB->USB2_getLDTemp(g_nDeviceId, lLDTemp);
    if (lReturn != (long)Usb2Struct::Cusb2Err::usb2Success)
    {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getLDTemp failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }
    Console::WriteLine("LDTemp : {0}[degC]", (double)lLDTemp / 100);
    Console::WriteLine("");
    Console::Out->Flush();


    return lReturn;
}

/// <summary>
/// Get LD board temperature
/// </summary>
/// <param name="objWrrUSB"></param>
/// <returns></returns>
long GetLDBoardTemperature(WRRUSB2^ objWrrUSB)
{
    long lReturn = 0;
    Int64 lLDBoardTemp = 0;

    //Argument error check
    if (objWrrUSB == nullptr)
    {
        return (long)Usb2Struct::Cusb2Err::usb2Err_unsuccess;
    }

    lReturn = (long)objWrrUSB->USB2_getLDBoardTemp(g_nDeviceId, lLDBoardTemp);
    if (lReturn != (long)Usb2Struct::Cusb2Err::usb2Success)
    {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getLDBoardTemp failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }
    Console::WriteLine("LDBoardTemp : {0}[degC]", (double)lLDBoardTemp / 100);
    Console::WriteLine("");
    Console::Out->Flush();


    return lReturn;
}

//-----------------------------------------------------------------------------
// Menu
//-----------------------------------------------------------------------------
void PrintMenu(void)
{
    Console::WriteLine("--------------------------------------------------------------------------");
    Console::WriteLine("  Menu               (0) Menu");
    Console::WriteLine("  DeviceInformation  (1) Get");
    Console::WriteLine("  Calibration        (2) Get");
    Console::WriteLine("  LDPower            (3) Get            (4) Set");
    Console::WriteLine("  LDPowerEnable      (5) Get            (6) Set");
    Console::WriteLine("  LDTemperature      (7) Get");
    Console::WriteLine("  LDBoardTemp        (8) Get");
    Console::WriteLine("  Measure            (F) FrequencyMode  (G) ContinuousMode (H) TriggerMode  (S) SingleSpectrum");
    Console::WriteLine("  Capture            (R) StartCapture    (T) StopCapture");
    Console::WriteLine("  SaveData           (C) Save");
    Console::WriteLine("  Quit               (Q) Quit");
    Console::WriteLine("--------------------------------------------------------------------------");
    Console::Out->Flush();

}

//-----------------------------------------------------------------------------
// Measurement start function (Frequency specified meausrement mode)
//-----------------------------------------------------------------------------
long StartMeasureForFrequencyMode(WRRUSB2^ objWrrUSB)
{
    long    lReturn = 0;
    long    lPixelSize = 0;
    int     nRepeatCount = 0;
    Int64   lCaptureFrameCnt = 0;
    Int64   lCurrCaptureIdx = 0;
    long    lOldIndex = 0;
    long    lImageDataSize = 0;
    long    lFrameCnt = FREQUENCYMODE_FRAME_COUNT;
    long    lDataTrasmit = DATA_TRANSMIT_COUNT;
    long    lDataCount = DATA_COUNT_MAX_VALUE;
    Int64   lSizeX = 0;
    Int64   lSizeY = 0;
    long    lGainMode = (long)E_GAINMODE::D_GAINMODE_SENSOR;
    long    lSensorGainMode = (long)E_SENSORGAINMODE::D_SENSORGAINMODE_LOWGAIN;
    array<unsigned short>^ aryusImageData = nullptr;
    array<unsigned short>^ aryusImageHeader = nullptr;
    array<UInt64>^ aryTime = nullptr;
    gCapturedData = nullptr;  // Clear any previous data.

    //Argument error check
    if (objWrrUSB == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    Console::WriteLine("Parameter setting.........");
    Console::Out->Flush();


    //Set CaptureMode 
    lReturn = (long)objWrrUSB->USB2_setCaptureMode(g_nDeviceId, (long)E_CAPTUREMODE::D_CAPTUREMODE_COUNT);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setCaptureMode failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    // Set TriggerMode
    lReturn = (long)objWrrUSB->USB2_setTriggerMode(g_nDeviceId, (long)E_TRIGGERMODE::D_TRIGGERMODE_INTERNAL);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setTriggerMode failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set acquired data count
    lReturn = (long)objWrrUSB->USB2_setDataCount(g_nDeviceId, lDataCount);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setDataCount failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set data transmit count
    lReturn = (long)objWrrUSB->USB2_setDataTransmit(g_nDeviceId, lDataTrasmit);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setDataTransmit failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set exposure cycle
    lReturn = (long)objWrrUSB->USB2_setExposureCycle(g_nDeviceId, CYCLE_TIME);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setExposureCycle failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set exposure time
    lReturn = (long)objWrrUSB->USB2_setExposureTime(g_nDeviceId, EXPOSURE_TIME);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setExposureTime failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set gain value(Set the sensor gain mode, and set low gain or high gain)
    lReturn = (long)objWrrUSB->USB2_setGain(g_nDeviceId, lGainMode, lSensorGainMode);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setGain failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Get image size
    lReturn = (long)objWrrUSB->USB2_getImageSize(g_nDeviceId, lSizeX, lSizeY);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getImageSize failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }
    lPixelSize = (long)(lSizeX * lSizeY);

    Console::WriteLine("Allocate buffer.........");
    Console::Out->Flush();


    //Buffer allocation
    lReturn = (long)objWrrUSB->USB2_allocateBuffer(g_nDeviceId, lFrameCnt);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_allocateBuffer failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Capture start
    Console::WriteLine("Start Capture.............");
    lReturn = (long)objWrrUSB->USB2_captureStart(g_nDeviceId, lFrameCnt);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: Data capture can not be start.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    Console::WriteLine("");
    Console::Out->Flush();

    Console::Write("Image Data ");

    //Data acquisition is reapeated 5 times while determining status of capture
    do {
        //Acquire capture status
        lReturn = (long)objWrrUSB->USB2_getCaptureStatus(g_nDeviceId, lCaptureFrameCnt, lCurrCaptureIdx);
        //Trace::WriteLine("APLLOGC++:CaptureStatus lReturn = " + lReturn.ToString() + " lCaptureFrameCnt = " + lCaptureFrameCnt.ToString() + "lCurrCaptureIdx = " + lCurrCaptureIdx.ToString());
        if (lReturn == safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {

            //Calculate old index
            if (lCaptureFrameCnt > lCurrCaptureIdx + 1) {
                lOldIndex = (long)(OTHERMODE_FRAME_COUNT - (lCaptureFrameCnt - (lCurrCaptureIdx + 1)));
            }
            else {
                lOldIndex = (long)((lCurrCaptureIdx - lCaptureFrameCnt) + 1);
            }

            //Allocate buffer to store header
            aryusImageHeader = gcnew array<unsigned short>((int)(IMAGE_HEADER_SIZE * lCaptureFrameCnt));
            if (aryusImageHeader == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }
            Array::Clear(aryusImageHeader, 0, aryusImageHeader->Length);

            //Allocate a buffer to store data
            lImageDataSize = (long)(lCaptureFrameCnt * lDataTrasmit * lPixelSize);
            aryusImageData = gcnew array<unsigned short>((int)lImageDataSize);
            if (aryusImageData == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }
            Array::Clear(aryusImageData, 0, aryusImageData->Length);

            //Allocate buffer to store time
            aryTime = gcnew array<UInt64>((int)(lDataTrasmit * lCaptureFrameCnt));
            if (aryTime == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }

            //Get data from device
            lReturn = (long)objWrrUSB->USB2_getImageHeaderData(g_nDeviceId, aryusImageHeader, aryusImageData, lOldIndex, lCaptureFrameCnt, aryTime);
            //Trace::WriteLine("APLLOGC++:getImageHeaderData lReturn = " + lReturn.ToString());
            if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
                objWrrUSB->USB2_captureStop(g_nDeviceId);
                objWrrUSB->USB2_releaseBuffer(g_nDeviceId);
                Console::WriteLine("Error code 0x{0:x04}: USB2_getImageHeaderData failed.", lReturn);
                Console::WriteLine("Exit by pressing the key");
                Console::Out->Flush();

                if (Environment::UserInteractive) {
                    Console::ReadKey();
                }
                return lReturn;
            }

            //Display data
            DisplayData(aryusImageData);
            gCapturedData = aryusImageData;

            Console::WriteLine("Stop Capture.........");
            Console::Out->Flush();

            //Stop capture
            lReturn = (long)objWrrUSB->USB2_captureStop(g_nDeviceId);
            if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
                Console::WriteLine("Error code 0x{0:x04}: USB2_captureStop failed.", lReturn);
                Console::WriteLine("Exit by pressing the key");
                Console::Out->Flush();

                if (Environment::UserInteractive) {
                    Console::ReadKey();
                }
                return lReturn;
            }

            //Release allocated buffer
            if (aryusImageData != nullptr) {
                delete aryusImageData;
                aryusImageData = nullptr;
            }
            if (aryusImageHeader != nullptr) {
                delete aryusImageHeader;
                aryusImageHeader = nullptr;
            }
            if (aryTime != nullptr) {
                delete aryTime;
                aryTime = nullptr;
            }

            nRepeatCount++;

            if (nRepeatCount < REPEAT_COUNT) {
                //next Capture start
                Console::WriteLine("Start Capture.............");
                Console::Out->Flush();

                lReturn = (long)objWrrUSB->USB2_captureStart(g_nDeviceId, lFrameCnt);
                if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
                    Console::WriteLine("Error code 0x{0:x04}: Data capture can not be start.", lReturn);
                    Console::WriteLine("Exit by pressing the key");
                    Console::Out->Flush();

                    if (Environment::UserInteractive) {
                        Console::ReadKey();
                    }
                    return lReturn;
                }
            }
        }
        Thread::Sleep(250);
    } while (nRepeatCount < REPEAT_COUNT);

    //Stop capture
    Console::WriteLine("Stop Capture.........");
    lReturn = (long)objWrrUSB->USB2_captureStop(g_nDeviceId);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_captureStop failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    Thread::Sleep(100);

    //Release buffer
    Console::WriteLine("Rleasing Buffer......");
    Console::Out->Flush();

    lReturn = (long)objWrrUSB->USB2_releaseBuffer(g_nDeviceId);
    if (lReturn != (Int32)(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::Write("Error code 0x{0:x04}: USB2_releaseBuffer failed.", lReturn);
        return lReturn;
    }

    Console::WriteLine("End of measurement");
    Console::Out->Flush();


    return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success);
}

//-----------------------------------------------------------------------------
// Measurement start function (Continous measurement mode)
//-----------------------------------------------------------------------------
long StartMeasureForContinuousMode(WRRUSB2^ objWrrUSB)
{
    long    lReturn = 0;
    long    lPixelSize = 0;
    int     nRepeatCount = 0;
    Int64   lCaptureFrameCnt = 0;
    Int64   lCurrCaptureIdx = 0;
    long    lOldIndex = 0;
    long    lImageDataSize = 0;
    long    lFrameCnt = OTHERMODE_FRAME_COUNT;
    long    lDataTrasmit = DATA_TRANSMIT_COUNT;
    long    lDataCount = DATA_COUNT_MAX_VALUE;
    Int64   lSizeX = 0;
    Int64   lSizeY = 0;
    long    lGainMode = (long)E_GAINMODE::D_GAINMODE_SENSOR;
    long    lSensorGainMode = (long)E_SENSORGAINMODE::D_SENSORGAINMODE_LOWGAIN;
    Int64 lCaptureMode = 0;
    Int64 lTriggerMode = 0;
    long lMaxFrameCount = 0;
    array<unsigned short>^ aryusImageData = nullptr;
    array<unsigned short>^ aryusImageHeader = nullptr;
    array<UInt64>^ aryTime = nullptr;
    gCapturedData = nullptr;  // Clear any previous data.


    //Argument error check
    if (objWrrUSB == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    Console::WriteLine("Parameter setting.........");
    Console::Out->Flush();


    //Set CaptureMode 
    lReturn = (long)objWrrUSB->USB2_setCaptureMode(g_nDeviceId, (long)E_CAPTUREMODE::D_CAPTUREMODE_CONTINUOUS);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setCaptureMode failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    // Set TriggerMode
    lReturn = (long)objWrrUSB->USB2_setTriggerMode(g_nDeviceId, (long)E_TRIGGERMODE::D_TRIGGERMODE_INTERNAL);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setTriggerMode failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set acquired data count
    lReturn = (long)objWrrUSB->USB2_setDataCount(g_nDeviceId, lDataCount);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setDataCount failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set data transmit count
    lReturn = (long)objWrrUSB->USB2_setDataTransmit(g_nDeviceId, lDataTrasmit);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setDataTransmit failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set exposure cycle
    lReturn = (long)objWrrUSB->USB2_setExposureCycle(g_nDeviceId, CYCLE_TIME);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setExposureCycle failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set exposure time
    lReturn = (long)objWrrUSB->USB2_setExposureTime(g_nDeviceId, EXPOSURE_TIME);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setExposureTime failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set gain value(Set the sensor gain mode, and set low gain or high gain)
    lReturn = (long)objWrrUSB->USB2_setGain(g_nDeviceId, lGainMode, lSensorGainMode);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setGain failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Get image size
    lReturn = (long)objWrrUSB->USB2_getImageSize(g_nDeviceId, lSizeX, lSizeY);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getImageSize failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }
    lPixelSize = (long)(lSizeX * lSizeY);

    Console::WriteLine("Allocate buffer.........");
    Console::Out->Flush();


    //Buffer allocation
    lReturn = (long)objWrrUSB->USB2_allocateBuffer(g_nDeviceId, lFrameCnt);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_allocateBuffer failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Capture start
    Console::WriteLine("Start Capture.............");
    Console::Out->Flush();

    lReturn = (long)objWrrUSB->USB2_captureStart(g_nDeviceId, lFrameCnt);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: Data capture can not be start.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    Console::WriteLine("");
    Console::WriteLine("Image Data ");
    Console::Out->Flush();


    //Data acquisition is reapeated 5 times while determining status of capture
    do {
        //Acquire capture status
        lReturn = (long)objWrrUSB->USB2_getCaptureStatus(g_nDeviceId, lCaptureFrameCnt, lCurrCaptureIdx);
        // Trace::WriteLine("APLLOGC++:CaptureStatus lReturn = " + lReturn.ToString() + " lCaptureFrameCnt = " + lCaptureFrameCnt.ToString() + "lCurrCaptureIdx = " + lCurrCaptureIdx.ToString());
        if (lReturn == safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {

            //Calculate old index
            if (lCaptureFrameCnt > lCurrCaptureIdx + 1) {
                lOldIndex = (long)(OTHERMODE_FRAME_COUNT - (lCaptureFrameCnt - (lCurrCaptureIdx + 1)));
            }
            else {
                lOldIndex = (long)((lCurrCaptureIdx - lCaptureFrameCnt) + 1);
            }

            //Allocate buffer to store header
            aryusImageHeader = gcnew array<unsigned short>((int)(IMAGE_HEADER_SIZE * lCaptureFrameCnt));
            if (aryusImageHeader == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }
            Array::Clear(aryusImageHeader, 0, aryusImageHeader->Length);

            //Allocate a buffer to store data
            lImageDataSize = (long)(lCaptureFrameCnt * lDataTrasmit * lPixelSize);
            aryusImageData = gcnew array<unsigned short>((int)lImageDataSize);
            if (aryusImageData == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }
            Array::Clear(aryusImageData, 0, aryusImageData->Length);

            //Allocate buffer to store time
            aryTime = gcnew array<UInt64>((int)(lDataTrasmit * lCaptureFrameCnt));
            if (aryTime == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }

            //Get data from device
            lReturn = (long)objWrrUSB->USB2_getImageHeaderData(g_nDeviceId, aryusImageHeader, aryusImageData, lOldIndex, lCaptureFrameCnt, aryTime);
            // Trace::WriteLine("APLLOGC++:getImageHeaderData lReturn = " + lReturn.ToString());
            if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
                objWrrUSB->USB2_captureStop(g_nDeviceId);
                objWrrUSB->USB2_releaseBuffer(g_nDeviceId);
                Console::WriteLine("Error code 0x{0:x04}: USB2_getImageHeaderData failed.", lReturn);
                Console::WriteLine("Exit by pressing the key");
                Console::Out->Flush();

                if (Environment::UserInteractive) {
                    Console::ReadKey();
                }
                return lReturn;
            }

            //Display data
            DisplayData(aryusImageData);
            gCapturedData = aryusImageData;

            //Release allocated buffer
            if (aryusImageData != nullptr) {
                delete aryusImageData;
                aryusImageData = nullptr;
            }
            if (aryusImageHeader != nullptr) {
                delete aryusImageHeader;
                aryusImageHeader = nullptr;
            }
            if (aryTime != nullptr) {
                delete aryTime;
                aryTime = nullptr;
            }

            nRepeatCount++;
        }
        Thread::Sleep(250);
    } while (nRepeatCount < REPEAT_COUNT);

    //Stop capture
    Console::WriteLine("Stop Capture.........");
    Console::Out->Flush();

    lReturn = (long)objWrrUSB->USB2_captureStop(g_nDeviceId);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_captureStop failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    Thread::Sleep(100);

    //Release buffer
    Console::WriteLine("Rleasing Buffer......");
    Console::Out->Flush();

    lReturn = (long)objWrrUSB->USB2_releaseBuffer(g_nDeviceId);
    if (lReturn != (Int32)(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::Write("Error code 0x{0:x04}: USB2_releaseBuffer failed.", lReturn);
        return lReturn;
    }

    Console::WriteLine("End of measurement");
    Console::Out->Flush();


    return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success);
}

//-----------------------------------------------------------------------------
// Measurement start function (Trigger mode)
//-----------------------------------------------------------------------------
long StartMeasureForTriggerMode(WRRUSB2^ objWrrUSB)
{
    long    lReturn = 0;
    long    lPixelSize = 0;
    int     nRepeatCount = 0;
    Int64   lCaptureFrameCnt = 0;
    Int64   lCurrCaptureIdx = 0;
    long    lOldIndex = 0;
    long    lImageDataSize = 0;
    long    lFrameCnt = OTHERMODE_FRAME_COUNT;
    long    lDataTrasmit = DATA_TRANSMIT_COUNT;
    long    lDataCount = DATA_COUNT_MAX_VALUE;
    Int64   lSizeX = 0;
    Int64   lSizeY = 0;
    long    lGainMode = (long)E_GAINMODE::D_GAINMODE_SENSOR;
    long lSensorGainMode = (long)E_SENSORGAINMODE::D_SENSORGAINMODE_LOWGAIN;
    array<unsigned short>^ aryusImageData = nullptr;
    array<unsigned short>^ aryusImageHeader = nullptr;
    array<UInt64>^ aryTime = nullptr;
    gCapturedData = nullptr;  // Clear any previous data.

    //Argument error check
    if (objWrrUSB == nullptr) {
        return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    Console::WriteLine("Parameter setting.........");
    Console::Out->Flush();


    //Set CaptureMode 
    lReturn = (long)objWrrUSB->USB2_setCaptureMode(g_nDeviceId, (long)E_CAPTUREMODE::D_CAPTUREMODE_TRIGGER);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setCaptureMode failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    // Set TriggerMode
    lReturn = (long)objWrrUSB->USB2_setTriggerMode(g_nDeviceId, (long)E_TRIGGERMODE::D_TRIGGERMODE_SOFTSYNC);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setTriggerMode failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set acquired data count
    lReturn = (long)objWrrUSB->USB2_setDataCount(g_nDeviceId, lDataCount);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setDataCount failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set data transmit count
    lReturn = (long)objWrrUSB->USB2_setDataTransmit(g_nDeviceId, lDataTrasmit);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setDataTransmit failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set exposure cycle
    lReturn = (long)objWrrUSB->USB2_setExposureCycle(g_nDeviceId, CYCLE_TIME);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setExposureCycle failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set exposure time
    lReturn = (long)objWrrUSB->USB2_setExposureTime(g_nDeviceId, EXPOSURE_TIME);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setExposureTime failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Set gain value(Set the sensor gain mode, and set low gain or high gain)
    lReturn = (long)objWrrUSB->USB2_setGain(g_nDeviceId, lGainMode, lSensorGainMode);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_setGain failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Get image size
    lReturn = (long)objWrrUSB->USB2_getImageSize(g_nDeviceId, lSizeX, lSizeY);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getImageSize failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }
    lPixelSize = (long)(lSizeX * lSizeY);

    Console::WriteLine("Allocate buffer.........");
    Console::Out->Flush();


    //Buffer allocation
    lReturn = (long)objWrrUSB->USB2_allocateBuffer(g_nDeviceId, lFrameCnt);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_allocateBuffer failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    //Capture start
    Console::WriteLine("Start Capture.............");
    lReturn = (long)objWrrUSB->USB2_captureStart(g_nDeviceId, lFrameCnt);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: Data capture can not be start.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    // Fire trigger
    Console::WriteLine("Fire Trigger.........");
    Console::Out->Flush();

    lReturn = (long)objWrrUSB->USB2_fireTrigger(g_nDeviceId);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_fireTrigger failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    Console::WriteLine("");
    Console::WriteLine("Image Data ");
    Console::Out->Flush();


    //Data acquisition is reapeated 5 times while determining status of capture
    do {
        //Acquire capture status
        lReturn = (long)objWrrUSB->USB2_getCaptureStatus(g_nDeviceId, lCaptureFrameCnt, lCurrCaptureIdx);
        //Trace::WriteLine("APLLOGC++:CaptureStatus lReturn = " + lReturn.ToString() + " lCaptureFrameCnt = " + lCaptureFrameCnt.ToString() + "lCurrCaptureIdx = " + lCurrCaptureIdx.ToString());
        if (lReturn == safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {

            //Calculate old index
            if (lCaptureFrameCnt > lCurrCaptureIdx + 1) {
                lOldIndex = (long)(OTHERMODE_FRAME_COUNT - (lCaptureFrameCnt - (lCurrCaptureIdx + 1)));
            }
            else {
                lOldIndex = (long)((lCurrCaptureIdx - lCaptureFrameCnt) + 1);
            }

            //Allocate buffer to store header
            aryusImageHeader = gcnew array<unsigned short>((int)(IMAGE_HEADER_SIZE * lCaptureFrameCnt));
            if (aryusImageHeader == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }
            Array::Clear(aryusImageHeader, 0, aryusImageHeader->Length);

            //Allocate a buffer to store data
            lImageDataSize = (long)(lCaptureFrameCnt * lDataTrasmit * lPixelSize);
            aryusImageData = gcnew array<unsigned short>((int)lImageDataSize);
            if (aryusImageData == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }
            Array::Clear(aryusImageData, 0, aryusImageData->Length);

            //Allocate buffer to store time
            aryTime = gcnew array<UInt64>((int)(lDataTrasmit * lCaptureFrameCnt));
            if (aryTime == nullptr) {
                return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
            }

            //Get data from device
            lReturn = (long)objWrrUSB->USB2_getImageHeaderData(g_nDeviceId, aryusImageHeader, aryusImageData, lOldIndex, lCaptureFrameCnt, aryTime);
            //Trace::WriteLine("APLLOGC++:getImageHeaderData lReturn = " + lReturn.ToString());
            if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
                objWrrUSB->USB2_captureStop(g_nDeviceId);
                objWrrUSB->USB2_releaseBuffer(g_nDeviceId);
                Console::WriteLine("Error code 0x{0:x04}: USB2_getImageHeaderData failed.", lReturn);
                Console::WriteLine("Exit by pressing the key");
                Console::Out->Flush();

                if (Environment::UserInteractive) {
                    Console::ReadKey();
                }
                return lReturn;
            }

            //Display data
            DisplayData(aryusImageData);
            gCapturedData = aryusImageData;

            //Release allocated buffer
            if (aryusImageData != nullptr) {
                delete aryusImageData;
                aryusImageData = nullptr;
            }
            if (aryusImageHeader != nullptr) {
                delete aryusImageHeader;
                aryusImageHeader = nullptr;
            }
            if (aryTime != nullptr) {
                delete aryTime;
                aryTime = nullptr;
            }

            nRepeatCount++;

            if (nRepeatCount < REPEAT_COUNT) {
                // Fire trigger
                Console::WriteLine("Fire Trigger.........");
                Console::Out->Flush();

                lReturn = (long)objWrrUSB->USB2_fireTrigger(g_nDeviceId);
                if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
                    Console::WriteLine("Error code 0x{0:x04}: USB2_fireTrigger failed.", lReturn);
                    Console::WriteLine("Exit by pressing the key");
                    Console::Out->Flush();

                    if (Environment::UserInteractive) {
                        Console::ReadKey();
                    }
                    return lReturn;
                }
            }
        }
        Thread::Sleep(250);
    } while (nRepeatCount < REPEAT_COUNT);

    //Stop capture
    Console::WriteLine("Stop Capture.........");
    Console::Out->Flush();

    lReturn = (long)objWrrUSB->USB2_captureStop(g_nDeviceId);
    if (lReturn != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:x04}: USB2_captureStop failed.", lReturn);
        Console::WriteLine("Exit by pressing the key");
        Console::Out->Flush();

        if (Environment::UserInteractive) {
            Console::ReadKey();
        }
        return lReturn;
    }

    Thread::Sleep(100);

    //Release buffer
    Console::WriteLine("Rleasing Buffer......");
    Console::Out->Flush();

    lReturn = (long)objWrrUSB->USB2_releaseBuffer(g_nDeviceId);
    if (lReturn != (Int32)(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::Write("Error code 0x{0:x04}: USB2_releaseBuffer failed.", lReturn);
        return lReturn;
    }

    Console::WriteLine("End of measurement");
    Console::Out->Flush();


    return safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success);
}
//-----------------------------------------------------------------------------
// Modified Single Spectrum Capture Function 
//-----------------------------------------------------------------------------
long StartMeasureForSingleSpectrum(WRRUSB2^ objWrrUSB) {
    long lReturn = 0;
    Int64 lSizeX = 0, lSizeY = 0, lPixelSize = 0;
    Int64 lCaptureFrameCnt = 0, lCurrCaptureIdx = 0;
    long lOldIndex = 0;
    long lImageDataSize = 0;
    long lFrameCnt = OTHERMODE_FRAME_COUNT;
    long lDataTransmit = DATA_TRANSMIT_COUNT;
    long lDataCount = DATA_COUNT_MAX_VALUE;
    long lGainMode = static_cast<long>(E_GAINMODE::D_GAINMODE_SENSOR);
    long lSensorGainMode = static_cast<long>(E_SENSORGAINMODE::D_SENSORGAINMODE_LOWGAIN);
    int iSleepTime = 250; // Adjust sleep time as needed for device specifics
    array<unsigned short>^ aryusImageData = nullptr;
    array<unsigned short>^ aryusImageHeader = nullptr;
    array<UInt64>^ aryTime = nullptr;
    gCapturedData = nullptr;  // Clear any previous data.


    if (objWrrUSB == nullptr) {
        return static_cast<long>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
    }

    Console::WriteLine("Parameter setting...");
    Console::Out->Flush();


    lReturn = objWrrUSB->USB2_setCaptureMode(g_nDeviceId, static_cast<long>(E_CAPTUREMODE::D_CAPTUREMODE_COUNT));
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:X}: USB2_setCaptureMode failed.", lReturn);
        Console::Out->Flush();

        return lReturn;
    }

    lReturn = objWrrUSB->USB2_setTriggerMode(g_nDeviceId, static_cast<long>(E_TRIGGERMODE::D_TRIGGERMODE_INTERNAL));
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:X}: USB2_setTriggerMode failed.", lReturn);
        Console::Out->Flush();

        return lReturn;
    }

    lReturn = objWrrUSB->USB2_setDataCount(g_nDeviceId, lDataCount);
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:X}: USB2_setDataCount failed.", lReturn);
        Console::Out->Flush();

        return lReturn;
    }

    lReturn = objWrrUSB->USB2_setDataTransmit(g_nDeviceId, lDataTransmit);
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:X}: USB2_setDataTransmit failed.", lReturn);
        Console::Out->Flush();

        return lReturn;
    }

    lReturn = objWrrUSB->USB2_setGain(g_nDeviceId, lGainMode, lSensorGainMode);
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:X}: USB2_setGain failed.", lReturn);
        Console::Out->Flush();

        return lReturn;
    }

    lReturn = objWrrUSB->USB2_getImageSize(g_nDeviceId, lSizeX, lSizeY);
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:X}: USB2_getImageSize failed.", lReturn);
        Console::Out->Flush();

        return lReturn;
    }
    lPixelSize = lSizeX * lSizeY;

    Console::WriteLine("Allocate buffer...");
    Console::Out->Flush();


    lReturn = objWrrUSB->USB2_allocateBuffer(g_nDeviceId, lFrameCnt);
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:X}: USB2_allocateBuffer failed.", lReturn);
        Console::Out->Flush();

        return lReturn;
    }

    Console::WriteLine("Start Capture...");
    Console::Out->Flush();

    lReturn = objWrrUSB->USB2_captureStart(g_nDeviceId, lFrameCnt);
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error code 0x{0:X}: Data capture cannot be started.", lReturn);
        Console::Out->Flush();

        return lReturn;
    }

    Thread::Sleep(250); // Wait for data to be ready

    Console::WriteLine("");
    Console::Out->Flush();

    Console::Write("Image Data ");

    //Acquire capture status
    lReturn = objWrrUSB->USB2_getCaptureStatus(g_nDeviceId, lCaptureFrameCnt, lCurrCaptureIdx);
    if (lReturn == static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {

        //Calculate old index
        if (lCaptureFrameCnt > lCurrCaptureIdx + 1) {
            lOldIndex = (long)(OTHERMODE_FRAME_COUNT - (lCaptureFrameCnt - (lCurrCaptureIdx + 1)));
        }
        else {
            lOldIndex = (long)((lCurrCaptureIdx - lCaptureFrameCnt) + 1);
        }

        //Allocate buffer to store header
        aryusImageHeader = gcnew array<unsigned short>((int)(IMAGE_HEADER_SIZE * lCaptureFrameCnt));
        if (aryusImageHeader == nullptr) {
            return static_cast<long>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
        }
        Array::Clear(aryusImageHeader, 0, aryusImageHeader->Length);

        //Allocate a buffer to store data
        lImageDataSize = (long)(lCaptureFrameCnt * lDataTransmit * lPixelSize);
        aryusImageData = gcnew array<unsigned short>((int)lImageDataSize);
        if (aryusImageData == nullptr) {
            return static_cast<long>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
        }
        Array::Clear(aryusImageData, 0, aryusImageData->Length);

        //Allocate buffer to store time
        aryTime = gcnew array<UInt64>((int)(lDataTransmit * lCaptureFrameCnt));
        if (aryTime == nullptr) {
            return static_cast<long>(Usb2Struct::Cusb2Err::usb2Err_unsuccess);
        }

        //Get data from device
        lReturn = objWrrUSB->USB2_getImageHeaderData(g_nDeviceId, aryusImageHeader, aryusImageData, lOldIndex, lCaptureFrameCnt, aryTime);
        if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
            Console::WriteLine("Error code 0x{0:x04}: USB2_getImageHeaderData failed.", lReturn);
            Console::Out->Flush();

            objWrrUSB->USB2_captureStop(g_nDeviceId);
            objWrrUSB->USB2_releaseBuffer(g_nDeviceId);
            return lReturn;
        }

        //Display data
        DisplayData(aryusImageData);
        gCapturedData = aryusImageData;

        Console::WriteLine("Stop Capture...");
        Console::Out->Flush();

        lReturn = objWrrUSB->USB2_captureStop(g_nDeviceId);
        if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
            Console::WriteLine("Error code 0x{0:x04}: USB2_captureStop failed.", lReturn);
            Console::Out->Flush();

            return lReturn;
        }

        //Release allocated buffer
        if (aryusImageData != nullptr) {
            delete aryusImageData;
            aryusImageData = nullptr;
        }
        if (aryusImageHeader != nullptr) {
            delete aryusImageHeader;
            aryusImageHeader = nullptr;
        }
        if (aryTime != nullptr) {
            delete aryTime;
            aryTime = nullptr;
        }
    }
    else {
        Console::WriteLine("Error code 0x{0:x04}: USB2_getCaptureStatus failed.", lReturn);
        Console::Out->Flush();

        objWrrUSB->USB2_captureStop(g_nDeviceId);
        return lReturn;
    }

    Console::WriteLine("Releasing Buffer...");
    Console::Out->Flush();

    lReturn = objWrrUSB->USB2_releaseBuffer(g_nDeviceId);
    if (lReturn != static_cast<long>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::Write("Error code 0x{0:x04}: USB2_releaseBuffer failed.", lReturn);
        return lReturn;
    }

    Console::WriteLine("End of measurement");
    Console::Out->Flush();

    return static_cast<long>(Usb2Struct::Cusb2Err::usb2Success);
}
//-----------------------------------------------------------------------------
// SaveData function
//-----------------------------------------------------------------------------
bool SaveData(array<unsigned short>^ aryusImageData)
{
    // Prompt the user for a CSV file name.
    Console::Write("Enter file name to save (with .csv extension): ");
    String^ fileName = Console::ReadLine();
    if (String::IsNullOrEmpty(fileName))
    {
        fileName = "savedData.csv";
    }

    try
    {
        // Create a StreamWriter to write to the specified file.
        System::IO::StreamWriter^ sw = gcnew System::IO::StreamWriter(fileName);

        // Optionally write a CSV header.
        sw->WriteLine("Index,Value");

        // Write each element of the array as a new CSV row.
        for (int i = 0; i < aryusImageData->Length; i++)
        {
            sw->WriteLine("{0},{1}", i, aryusImageData[i]);
        }
        sw->Close();

        Console::WriteLine("Data successfully saved to {0}", fileName);
        Console::Out->Flush();

        return true;
    }
    catch (Exception^ ex)
    {
        Console::WriteLine("Error saving file: {0}", ex->Message);
        Console::Out->Flush();

        return false;
    }
}






//-----------------------------------------------------------------------------
// Display data function
//-----------------------------------------------------------------------------
bool DisplayData(array<unsigned short>^ aryusImageData)
{
    String^ strData = nullptr;
    int     nData = 0;
    //Output data to the console
    for (int nIdx = 0; nIdx < aryusImageData->Length; nIdx++) {
        nData = aryusImageData[nIdx];
        strData = String::Format("{0:X04} ", nData);
        Console::Write(strData);
        strData = "";
    }
    Console::WriteLine("");
    Console::Out->Flush();

    return true;
}
// Function to continuously capture data
long ContinuousMeasurement(WRRUSB2_DLL::WRRUSB2^ objWrrUSB) {
    Console::WriteLine("Continuous Measurement Running...");
    Console::Out->Flush();

    int errorCount = 0;

    // Ensure device is connected
    if (GetDeivceInformation(objWrrUSB) != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
        Console::WriteLine("Error: Device is not connected!");
        Console::Out->Flush();

        return -1;
    }

    objWrrUSB->USB2_captureStop(g_nDeviceId);
    Thread::Sleep(500);

    while (capturing.load()) {  // Capture continues while capturing == true
        long result = StartMeasureForSingleSpectrum(objWrrUSB);

        if (result != safe_cast<System::Int32>(Usb2Struct::Cusb2Err::usb2Success)) {
            Console::WriteLine("Error occurred: {0:X}. Retrying...", result);
            Console::Out->Flush();

            errorCount++;

            if (errorCount >= 3) {
                Console::WriteLine("Too many failures, stopping measurement.");
                Console::Out->Flush();

                capturing.store(false);  // Ensure capture stops after failures
                return result;
            }

            objWrrUSB->USB2_captureStop(g_nDeviceId);
            Thread::Sleep(500);
        }
        else {
            errorCount = 0;  // Reset error count if successful
        }

        Thread::Sleep(1000);
    }

    Console::WriteLine("Capture Stopped.");
    Console::Out->Flush();

    return 0;
}




// Function to assign capture function
void SetupCapture(DataCapture^ capture) {
    capture->SetCaptureFunction(&ContinuousMeasurement);
}