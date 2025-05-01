#include "FileWatcher.h"

#include <mutex>
#include <queue>
#include <thread>
#include <efsw/efsw.hpp>

namespace
{
    FileWatcherEvent::ActionType actionFromEfswAction(efsw::Action action)
    {
        switch (action)
        {
        case efsw::Action::Add:
            return FileWatcherEvent::ActionType::Create;
        case efsw::Action::Delete:
            return FileWatcherEvent::ActionType::Delete;
        case efsw::Action::Modified:
            return FileWatcherEvent::ActionType::Modify;
        case efsw::Action::Moved:
            return FileWatcherEvent::ActionType::Rename;
        default:
            ASSERT(false, "Unknown action")
        }
        std::unreachable();
    }

    struct DebounceListener final : efsw::FileWatchListener
    {
        using FileEvent = FileWatcherEvent;

        DebounceListener(const FileWatcherSettings& settings);

        void handleFileAction(efsw::WatchID watchId, const std::string& directory, const std::string& filename,
            efsw::Action action, std::string oldFilename) override;

        void StartDebounceThread();

        void StopDebounceThread();

        /* some editors (like vscode) do something strange when a file is updated
         * e.g., it is updated twice on save, in order for us not to bake resource more times
         * than necessary, we order baking only after some delay
         */
        std::deque<FileEvent> FilesToProcess;

        FileWatcherSettings Settings{};
        
        Signal<FileEvent> FilesUpdateSignal;

        std::mutex Mutex;
        std::condition_variable ConditionVariable;
        std::atomic_bool Exited{false};

        std::thread DebounceThread;
        
        std::mutex SignalMutex;
        std::optional<FileEvent> PendingFileUpdate;
    };

    DebounceListener::DebounceListener(const FileWatcherSettings& settings): Settings(settings)
    {
        StartDebounceThread();
    }

    void DebounceListener::handleFileAction(efsw::WatchID watchId, const std::string& directory,
        const std::string& filename, efsw::Action action, std::string oldFilename)
    {
        {
            std::scoped_lock lock(Mutex);
            FilesToProcess.push_back({
                .Name = std::filesystem::weakly_canonical(directory + filename).generic_string(),
                .OldName = std::filesystem::weakly_canonical(directory + oldFilename).generic_string(),
                .Action = actionFromEfswAction(action),
                .TimePoint = std::chrono::high_resolution_clock::now()});
        }
        ConditionVariable.notify_one();
    }

    void DebounceListener::StartDebounceThread()
    {
        using namespace std::chrono;
        using namespace std::chrono_literals;

        DebounceThread = std::thread([this]()
        {
            auto replacedWithNewerUpdate = [](const FileEvent& file, std::deque<FileEvent>& otherFiles,
                milliseconds debounce) -> bool
            {
                auto equivalent = [&file](const FileEvent& a)
                {
                    return a.Action == file.Action && a.Name == file.Name;
                };
                    
                if (file.Action == FileWatcherEvent::ActionType::Delete ||
                    file.Action == FileWatcherEvent::ActionType::Create)
                    return false;

                for (auto it = otherFiles.rbegin(); it != otherFiles.rend(); ++it)
                {
                    if (it->TimePoint - file.TimePoint >= debounce)
                        continue;

                    if (equivalent(*it))
                    {
                        auto last = std::prev(it.base());
                        otherFiles.erase(std::remove_if(otherFiles.begin(), last, equivalent), last);
                        return true;
                    }
                }

                return false;
            };

            for (;;)
            {
                if (PendingFileUpdate)
                {
                    const milliseconds debounce = Settings.DebounceDuration;
                    const FileEvent& file = *PendingFileUpdate;
                    std::this_thread::sleep_for(debounce - (high_resolution_clock::now() - file.TimePoint));
                    if (Exited)
                        return;

                    {
                        std::unique_lock lock(Mutex);
                        if (replacedWithNewerUpdate(file, FilesToProcess, debounce))
                        {
                            PendingFileUpdate = std::nullopt;
                            continue;
                        }
                    }

                    std::lock_guard signalLock(SignalMutex);
                    FilesUpdateSignal.Emit(file);
                    PendingFileUpdate = std::nullopt;
                }
                else
                {
                    std::unique_lock lock(Mutex);
                    ConditionVariable.wait(lock, [&]() { return !FilesToProcess.empty() || Exited; });
                    if (Exited)
                        return;
                        
                    PendingFileUpdate = FilesToProcess.front();
                    FilesToProcess.pop_front();
                }
            }
        });
    }

    void DebounceListener::StopDebounceThread()
    {
        {
            std::lock_guard lock(Mutex);
            Exited = true;
        }
        ConditionVariable.notify_one();
    
        if (DebounceThread.joinable())
            DebounceThread.join();
    }
}

struct FileWatcher::Impl
{
    static constexpr efsw::WatchID INVALID_ID = ~0;
    efsw::WatchID WatchId{INVALID_ID};
    efsw::FileWatcher Watcher;
    std::shared_ptr<DebounceListener> Listener;
};

FileWatcher::FileWatcher()
{
    m_Impl = std::make_unique<Impl>();
}

FileWatcher::~FileWatcher()
{
    const FileWatcherResult<FileWatcherError> res = StopWatching();
    if (res.has_value())
        return;

    if (res.error() == FileWatcherError::NotWatchingDirectory)
        return;

    LOG("Unexpected error in FileWatcher::~FileWatcher() ({})", (u8)res.error());
}

FileWatcherResult<FileWatcherWatchError> FileWatcher::Watch(const std::filesystem::path& path,
    const FileWatcherSettings& settings) const
{
    static constexpr bool IS_RECURSIVE = true;

    if (m_Impl->WatchId != Impl::INVALID_ID)
        return std::unexpected(FileWatcherWatchError::AlreadyWatching);
    
    m_Impl->Listener = std::make_shared<DebounceListener>(settings);
    m_Impl->WatchId = m_Impl->Watcher.addWatch(path.string(), m_Impl->Listener.get(), IS_RECURSIVE);
    if (m_Impl->WatchId > 0)
    {
        m_Impl->Watcher.watch();
        return {}; 
    }

    m_Impl->Listener->StopDebounceThread();
    
    switch ((efsw::Error)m_Impl->WatchId)
    {
    case efsw::Error::FileNotFound:
        return std::unexpected(FileWatcherWatchError::FileNotFound);
    case efsw::Error::FileRepeated:
    case efsw::Error::FileOutOfScope:
    case efsw::Error::FileNotReadable:
    case efsw::Error::FileRemote:
    case efsw::Error::WatcherFailed:
    case efsw::Error::Unspecified:
    case efsw::Error::NoError:
    default: 
        return std::unexpected(FileWatcherWatchError::WatchFailed);
    }
}

FileWatcherResult<FileWatcherError> FileWatcher::StopWatching() const
{
    if (m_Impl->WatchId == Impl::INVALID_ID)
        return std::unexpected(FileWatcherError::NotWatchingDirectory);

    m_Impl->WatchId = Impl::INVALID_ID;
    m_Impl->Listener->StopDebounceThread();
    m_Impl->Watcher.removeWatch(m_Impl->WatchId);

    return {};
}

FileWatcherResult<FileWatcherError> FileWatcher::Subscribe(FileWatcherHandler& handler) const
{
    if (m_Impl->WatchId == Impl::INVALID_ID)
        return std::unexpected(FileWatcherError::NotWatchingDirectory);

    handler.m_Watcher = this;
    std::lock_guard signalLock(m_Impl->Listener->SignalMutex);
    handler.m_Handler.Connect(m_Impl->Listener->FilesUpdateSignal);

    return {};
}

std::string FileWatcher::ErrorDescription(FileWatcherWatchError error)
{
    switch (error)
    {
    case FileWatcherWatchError::FileNotFound:
        return "File not found";
    case FileWatcherWatchError::AlreadyWatching:
        return "Already watching";
    case FileWatcherWatchError::WatchFailed:
        return "Watch failed";
    default:
        return "Unknown error";
    }
}

std::string FileWatcher::ErrorDescription(FileWatcherError error)
{
    switch (error)
    {
    case FileWatcherError::NotWatchingDirectory:
        return "No directory is being watched";
    default:
        return "Unknown error";
    }
}

FileWatcherHandler::FileWatcherHandler(std::function<void(const FileWatcherEvent&)>&& handlerFn)
    : m_Handler(std::move(handlerFn))
{
}

FileWatcherHandler::~FileWatcherHandler()
{
    if (!m_Watcher)
        return;

    std::lock_guard signalLock(m_Watcher->m_Impl->Listener->SignalMutex);
    m_Handler.Disconnect();
}
