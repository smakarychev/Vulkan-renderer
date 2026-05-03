#pragma once

#include <functional>

namespace lux 
{
struct AssetImportRequest
{
    std::function<void()> ImportFn = [](){};
};

class AssetImportQueue
{
public:
    void Init();
    void Shutdown();
    void SignalStop();
    bool AddRequest(AssetImportRequest&& request);
    /* this function blocks and should be called from secondary thread */
    void ProcessRequests();

private:
    std::queue<AssetImportRequest> m_Requests;
    std::condition_variable m_Cv;
    std::mutex m_CvMutex;
    std::atomic_bool m_Exit{false};
    std::thread m_WorkerThread;
};
}
