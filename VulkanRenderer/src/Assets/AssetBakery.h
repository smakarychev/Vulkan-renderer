#pragma once

#include <functional>

namespace lux 
{
struct AssetBakeRequest
{
    std::function<void()> BakeFn = [](){};
};

class AssetBakery
{
public:
    void Init();
    void Shutdown();
    void SignalStop();
    bool AddRequest(AssetBakeRequest&& request);
    /* this function blocks and should be called from secondary thread */
    void ProcessRequests();

private:
    std::queue<AssetBakeRequest> m_Requests;
    std::condition_variable m_Cv;
    std::mutex m_CvMutex;
    std::atomic_bool m_Exit{false};
    std::thread m_WorkerThread;
};
}
