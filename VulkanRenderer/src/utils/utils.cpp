#include "utils.h"

#include <Windows.h>

void utils::runSubProcess(const std::filesystem::path& executablePath, const std::vector<std::string>& args)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    HANDLE childOutRead, childOutWrite;
    
    SECURITY_ATTRIBUTES securityAttributes;
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES); 
    securityAttributes.bInheritHandle = TRUE; 
    securityAttributes.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&childOutRead, &childOutWrite, &securityAttributes, 0))
        return;
    if (!SetHandleInformation(childOutRead, HANDLE_FLAG_INHERIT, 0))
        return;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = childOutWrite;

    std::string argString = executablePath.filename().string();
    for (const auto& arg : args)
        argString += " " + arg;

    constexpr DWORD kFlags = MB_ERR_INVALID_CHARS;
    i32 wideLength = MultiByteToWideChar(CP_UTF8, kFlags, argString.data(), (i32)argString.size(), nullptr, 0);
    std::wstring wArgString;
    wArgString.resize(wideLength);
    MultiByteToWideChar(CP_UTF8, kFlags, argString.data(), (i32)argString.size(), wArgString.data(), wideLength);
    
    auto status = CreateProcess(
        executablePath.relative_path().c_str(),
        LPWSTR(wArgString.c_str()),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (status != 0)
    {
        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        CloseHandle(childOutWrite);
        
        DWORD bytesRead; 
        CHAR buffer[1024];
        while (ReadFile(childOutRead, buffer, 1024, &bytesRead, nullptr) && bytesRead > 0) 
        {
            LOG("{}: {}", executablePath.stem().string(), std::string{buffer + 0, buffer + bytesRead});
        }
        
        CloseHandle(childOutRead);
    }

}
