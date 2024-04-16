#include "utils.h"

#include <Windows.h>

#include "Core/core.h"

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

    CloseHandle(childOutWrite);

    if (status != 0)
    {
        std::string lineTail = {};
        for (;;)
        {
            DWORD bytesRead; 
            CHAR buffer[256];
            
            if (!ReadFile(childOutRead, buffer, sizeof(buffer), &bytesRead, nullptr) || bytesRead == 0)
            {
                DWORD exitCode;
                if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
                    break;
            }

            std::string bufferString = std::string{buffer + 0, buffer + bytesRead};
            std::vector<std::string_view> lines = splitStringTransient(bufferString, "\n");

            bool unfinishedLine = !bufferString.ends_with("\r\n");

            if (!lineTail.empty() && (!unfinishedLine || lines.size() > 1))
            {
                LOG("{}: {}", executablePath.stem().string(), lineTail + std::string{lines.front()});
                lineTail = {};
                lines.erase(lines.begin());
            }
            
            if (unfinishedLine)
            {
                lineTail += lines.back();
                lines.pop_back();
            }

            for (auto& line : lines)
                LOG("{}: {}", executablePath.stem().string(), line);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        CloseHandle(childOutRead);
    }

}

std::vector<std::string_view> utils::splitStringTransient(std::string_view string, std::string_view delimiter)
{
    std::vector<std::string_view> result;

    u32 delimiterLength = (u32)delimiter.length();
    u64 offset = 0;
    u64 pos = string.find(delimiter);
    while (pos != std::string::npos)
    {
        result.push_back(string.substr(offset, pos - offset));
        offset = pos + delimiterLength;
        pos = string.find(delimiter, offset);
    }

    if (offset < string.length())
        result.push_back(string.substr(offset));

    return result;
}
