#include "rendererpch.h"
#include "AssetImportQueue.h"

#include "AssetManager.h"

namespace lux
{
void AssetImportQueue::Init()
{
    m_WorkerThread = std::thread([this]()
    {
        ProcessRequests();
    });
}

void AssetImportQueue::Shutdown()
{
    SignalStop();
    if (m_WorkerThread.joinable())
        m_WorkerThread.join();
}

void AssetImportQueue::SignalStop()
{
    {
        std::lock_guard lock(m_CvMutex);
        m_Exit = true;
    }
    m_Cv.notify_one();
}

bool AssetImportQueue::AddRequest(AssetImportRequest&& request)
{
    {
        std::lock_guard lock(m_CvMutex);
        m_Requests.push(std::move(request));
    }
    m_Cv.notify_one();

    return true;
}

void AssetImportQueue::ProcessRequests()
{
    for (;;)
    {
        std::unique_lock lock(m_CvMutex);
        m_Cv.wait(lock, [this](){ return m_Exit || !m_Requests.empty(); });
        if (m_Exit)
            return;

        AssetImportRequest request = std::move(m_Requests.front());
        m_Requests.pop();
        lock.unlock();

        request.ImportFn();
    }
}
}
