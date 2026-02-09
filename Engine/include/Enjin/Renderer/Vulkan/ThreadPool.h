#pragma once

#include "Enjin/Platform/Platform.h"
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <algorithm>

namespace Enjin {
namespace Renderer {

// Simple thread pool for parallel command buffer recording
class ENJIN_API ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool() { Shutdown(); }

    void Initialize(u32 threadCount = 0) {
        if (!m_Workers.empty()) return; // Already initialized
        if (threadCount == 0) {
            threadCount = std::max(2u, std::min(7u, std::thread::hardware_concurrency() - 1));
        }
        m_Running = true;
        m_Workers.reserve(threadCount);
        for (u32 i = 0; i < threadCount; ++i) {
            m_Workers.emplace_back([this] { WorkerLoop(); });
        }
    }

    void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Running = false;
        }
        m_Condition.notify_all();
        for (auto& worker : m_Workers) {
            if (worker.joinable()) worker.join();
        }
        m_Workers.clear();
    }

    template<typename F>
    std::future<void> Submit(F&& task) {
        auto packaged = std::make_shared<std::packaged_task<void()>>(std::forward<F>(task));
        auto future = packaged->get_future();
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Tasks.push([packaged]() { (*packaged)(); });
        }
        m_Condition.notify_one();
        return future;
    }

    u32 GetThreadCount() const { return static_cast<u32>(m_Workers.size()); }

private:
    void WorkerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                m_Condition.wait(lock, [this] { return !m_Running || !m_Tasks.empty(); });
                if (!m_Running && m_Tasks.empty()) return;
                task = std::move(m_Tasks.front());
                m_Tasks.pop();
            }
            task();
        }
    }

    std::vector<std::thread> m_Workers;
    std::queue<std::function<void()>> m_Tasks;
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
    bool m_Running = false;
};

} // namespace Renderer
} // namespace Enjin
