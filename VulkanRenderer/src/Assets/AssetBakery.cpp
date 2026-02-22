#include "rendererpch.h"
#include "AssetBakery.h"

#include "AssetManager.h"

namespace lux
{
void AssetBakery::Init()
{
    m_WorkerThread = std::thread([this]()
    {
        ProcessRequests();
    });
}

void AssetBakery::Shutdown()
{
    SignalStop();
    if (m_WorkerThread.joinable())
        m_WorkerThread.join();
}

void AssetBakery::SignalStop()
{
    {
        std::lock_guard lock(m_CvMutex);
        m_Exit = true;
    }
    m_Cv.notify_one();
}

bool AssetBakery::AddRequest(AssetBakeRequest&& request)
{
    {
        std::lock_guard lock(m_CvMutex);
        m_Requests.push(std::move(request));
    }
    m_Cv.notify_one();

    return true;
}

void AssetBakery::ProcessRequests()
{
    for (;;)
    {
        std::unique_lock lock(m_CvMutex);
        m_Cv.wait(lock, [this](){ return m_Exit || !m_Requests.empty(); });
        if (m_Exit)
            return;

        AssetBakeRequest request = std::move(m_Requests.front());
        m_Requests.pop();
        lock.unlock();

        request.BakeFn();
    }
}
}
