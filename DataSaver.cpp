#include "stdafx.h"
#include "DataSaver.h"
#include "GlobalData.h"

using namespace System;
using namespace System::Threading;
using namespace System::IO;
using namespace System::Collections::Concurrent;

DataSaver::DataSaver() {}

void DataSaver::SaveData(array<unsigned short>^ data) {
    if (data == nullptr || data->Length == 0) {
        Console::WriteLine("No data available to save.");
        return;
    }

    Console::Write("Enter filename to save (with .csv extension): ");
    String^ filename = Console::ReadLine();
    if (String::IsNullOrEmpty(filename)) {
        filename = "CapturedData.csv";
    }

    try {
        System::IO::StreamWriter^ sw = gcnew System::IO::StreamWriter(filename);
        sw->WriteLine("Index,Value");
        for (int i = 0; i < data->Length; i++) {
            sw->WriteLine("{0},{1}", i, data[i]);
        }
        sw->Close();
        Console::WriteLine("Data successfully saved to {0}", filename);
    }
    catch (Exception^ ex) {
        Console::WriteLine("Error saving file: {0}", ex->Message);
    }
}