#pragma once

#include "Signals/Signal.h"

#include <expected>
#include <filesystem>
#include <memory>

enum class FileWatcherWatchError : u8
{
    FileNotFound,
    AlreadyWatching,
    WatchFailed,
};
enum class FileWatcherError : u8
{
    NotWatchingDirectory,
};

struct FileWatcherSettings
{
    static constexpr std::chrono::milliseconds DEFAULT_DEBOUNCE_DURATION = std::chrono::milliseconds(85);
    std::chrono::milliseconds DebounceDuration{DEFAULT_DEBOUNCE_DURATION};
};

struct FileWatcherEvent
{
    enum class ActionType : u8
    {
        Modify, Rename, Create, Delete
    };
    std::string Name;
    std::string OldName;
    ActionType Action;
    std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
};

template <typename Error>
using FileWatcherResult = std::expected<void, Error>;

class FileWatcherHandler;

class FileWatcher
{
    friend class FileWatcherHandler;
public:
    FileWatcher();
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

    ~FileWatcher();

    FileWatcherResult<FileWatcherWatchError> Watch(const std::filesystem::path& path,
        const FileWatcherSettings& settings = {}) const;
    FileWatcherResult<FileWatcherError> StopWatching() const;
    FileWatcherResult<FileWatcherError> Subscribe(FileWatcherHandler& handler) const;

    static std::string ErrorDescription(FileWatcherWatchError error);
    static std::string ErrorDescription(FileWatcherError error);
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

class FileWatcherHandler
{
    friend class FileWatcher;
public:
    FileWatcherHandler(std::function<void(const FileWatcherEvent&)>&& handlerFn);
    ~FileWatcherHandler();
    
    const FileWatcher* m_Watcher{nullptr};
    SignalHandler<FileWatcherEvent> m_Handler;
};
