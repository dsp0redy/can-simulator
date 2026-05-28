#include "db-reader.hpp"
#include "can-receive.hpp"
#include "can-send.hpp"
#include "j1939-db.hpp"
#include "can-setup.hpp"
#include "process-data.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

namespace
{
using Clock = std::chrono::steady_clock;

constexpr uint32_t kListenPgn = 0x00EE00;
constexpr uint32_t kMinCycleMs = 5;
constexpr uint32_t kMaxCycleMs = 60000;
constexpr uint32_t kSessionInactivityTimeoutMs = 0;
constexpr size_t kMaxActiveSessions = 512;

struct TransmissionKey
{
    uint32_t requestedPgn{0};
    uint8_t targetAddress{0xFF};

    bool operator<(const TransmissionKey& other) const
    {
        if (requestedPgn != other.requestedPgn)
            return requestedPgn < other.requestedPgn;
        return targetAddress < other.targetAddress;
    }
};

uint32_t ClampCycleMs(uint32_t raw)
{
    if (raw < kMinCycleMs)
        return kMinCycleMs;
    if (raw > kMaxCycleMs)
        return kMaxCycleMs;
    return raw;
}

class PeriodicTransmissionSession
{
public:
    using SendCallback = std::function<void(const TransmissionKey&)>;

    PeriodicTransmissionSession(const TransmissionKey& key,
                                std::chrono::milliseconds period,
                                std::chrono::milliseconds inactivityTimeout,
                                SendCallback sendCallback,
                                std::function<void(const std::string&)> log)
        : m_key(key)
        , m_period(period)
        , m_inactivityTimeout(inactivityTimeout)
        , m_sendCallback(std::move(sendCallback))
        , m_log(std::move(log))
        , m_lastRefresh(Clock::now())
    {
        m_worker = std::thread([this]() { Run(); });
    }

    ~PeriodicTransmissionSession()
    {
        Stop();
    }

    void Refresh(std::chrono::milliseconds newPeriod)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_period = newPeriod;
            m_lastRefresh = Clock::now();
        }
        m_cv.notify_all();
    }

    bool IsExpired(Clock::time_point now) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_inactivityTimeout.count() <= 0)
            return false;
        return (now - m_lastRefresh) >= m_inactivityTimeout;
    }

    void Stop()
    {
        const bool wasStopping = m_stopRequested.exchange(true);
        if (!wasStopping)
            m_cv.notify_all();

        if (m_worker.joinable())
            m_worker.join();
    }

private:
    void Run()
    {
        auto nextSend = Clock::now();

        while (!m_stopRequested.load())
        {
            std::chrono::milliseconds period{0};
            Clock::time_point lastRefresh;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                period = m_period;
                lastRefresh = m_lastRefresh;
            }

            if (period.count() <= 0)
                period = std::chrono::milliseconds(kMinCycleMs);

            if (nextSend <= Clock::now())
                nextSend = Clock::now() + period;

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait_until(lock, nextSend, [&]() {
                    return m_stopRequested.load();
                });
            }

            if (m_stopRequested.load())
                break;

            const auto now = Clock::now();
            if (m_inactivityTimeout.count() > 0 && (now - lastRefresh) >= m_inactivityTimeout)
            {
                m_log("Stopping periodic thread for PGN=0x" +
                      [&]() {
                          std::ostringstream oss;
                          oss << std::hex << m_key.requestedPgn << std::dec;
                          return oss.str();
                      }() +
                      " SA=0x" + std::to_string(m_key.targetAddress) +
                      " due to inactivity timeout");
                break;
            }

            try
            {
                m_sendCallback(m_key);
            }
            catch (const std::exception& ex)
            {
                m_log(std::string("Periodic TX error: ") + ex.what());
            }

            nextSend += period;
        }
    }

    TransmissionKey m_key;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;
    std::atomic<bool> m_stopRequested{false};
    std::chrono::milliseconds m_period{0};
    std::chrono::milliseconds m_inactivityTimeout{0};
    SendCallback m_sendCallback;
    std::function<void(const std::string&)> m_log;
    Clock::time_point m_lastRefresh;
};

class PeriodicTransmissionManager
{
public:
    using SendCallback = PeriodicTransmissionSession::SendCallback;

    PeriodicTransmissionManager(size_t maxSessions,
                                std::chrono::milliseconds inactivityTimeout,
                                std::function<void(const std::string&)> log)
        : m_maxSessions(maxSessions)
        , m_inactivityTimeout(inactivityTimeout)
        , m_log(std::move(log))
    {
        m_reaper = std::thread([this]() { ReaperLoop(); });
    }

    ~PeriodicTransmissionManager()
    {
        StopAll();
    }

    bool StartOrRefresh(const TransmissionKey& key,
                        std::chrono::milliseconds period,
                        SendCallback sendCallback)
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);

        const auto existing = m_sessions.find(key);
        if (existing != m_sessions.end())
        {
            existing->second->Refresh(period);
            return true;
        }

        if (m_sessions.size() >= m_maxSessions)
            return false;

        m_sessions[key] = std::make_unique<PeriodicTransmissionSession>(
            key, period, m_inactivityTimeout, std::move(sendCallback), m_log);
        return true;
    }

    void Stop(const TransmissionKey& key)
    {
        std::unique_ptr<PeriodicTransmissionSession> session;
        {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);
            auto it = m_sessions.find(key);
            if (it == m_sessions.end())
                return;

            session = std::move(it->second);
            m_sessions.erase(it);
        }

        session->Stop();
    }

    void StopAll()
    {
        const bool wasStopping = m_stopping.exchange(true);
        if (!wasStopping && m_reaper.joinable())
            m_reaper.join();

        std::vector<std::unique_ptr<PeriodicTransmissionSession>> sessions;
        {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);
            for (auto& [key, session] : m_sessions)
            {
                (void)key;
                sessions.push_back(std::move(session));
            }
            m_sessions.clear();
        }

        for (auto& session : sessions)
            session->Stop();
    }

private:
    void ReaperLoop()
    {
        while (!m_stopping.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (m_stopping.load())
                break;

            std::vector<TransmissionKey> expired;
            {
                std::lock_guard<std::mutex> lock(m_sessionsMutex);
                const auto now = Clock::now();
                for (const auto& [key, session] : m_sessions)
                {
                    if (session->IsExpired(now))
                        expired.push_back(key);
                }
            }

            for (const auto& key : expired)
                Stop(key);
        }
    }

    size_t m_maxSessions{0};
    std::chrono::milliseconds m_inactivityTimeout{0};
    std::function<void(const std::string&)> m_log;
    std::map<TransmissionKey, std::unique_ptr<PeriodicTransmissionSession>> m_sessions;
    std::mutex m_sessionsMutex;
    std::atomic<bool> m_stopping{false};
    std::thread m_reaper;
};
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: Can-Simulator <path-to-j1939-xml-file> [can-interface]\n";
        return 1;
    }

    const std::string interfaceName = (argc >= 3) ? argv[2] : "vcan0";

    auto isRequestPayload = [](const std::vector<uint8_t>& payload) {
        if (payload.size() < 3)
            return false;
        return std::all_of(payload.begin() + 3, payload.end(), [](uint8_t byte) {
            return byte == 0xFF;
        });
    };

    auto                           database  = std::make_shared<DbReader>(argv[1]);
    std::shared_ptr<J1939Database> contents = database->ParseXmlAndStoreInContainer();
    std::mutex sendMutex;
    std::mutex logMutex;

    int socketId = -1;
    try
    {
        CanSetUp canSetUp(interfaceName);
        socketId = canSetUp.CanSocketSetup();
        canSetUp.ApplyPgnFilter(socketId, kListenPgn);

        const CanReceive receiver;
        const CanSend sender;
        const ProcessData processData;
        auto logLine = [&](const std::string& message) {
            const std::lock_guard<std::mutex> lock(logMutex);
            std::cout << message << "\n";
        };

        {
            std::ostringstream oss;
            oss << "Listening on " << interfaceName << " for PGN=0x"
                << std::hex << kListenPgn << std::dec << "...";
            logLine(oss.str());
        }

        auto sendResponseForRequestedPgn = [&](const TransmissionKey& key) {
            const uint32_t requestedPgn = key.requestedPgn;
            const uint8_t targetAddress = key.targetAddress;
            const auto it = contents->find(requestedPgn);
            if (it == contents->end())
                return;

            if (it->second.dlc == 0)
                throw std::runtime_error("Cannot transmit PGN with DLC=0");

            if (it->second.signals.empty())
                throw std::runtime_error("Cannot build payload: PGN has no SPNs");

            std::vector<uint8_t> responsePayload = processData.BuildRandomPayload(it->second);
            const J1939TxConfig responseConfig{
                .name = 0,
                .pgn = requestedPgn,
                .addr = targetAddress,
            };

            const std::lock_guard<std::mutex> lock(sendMutex);
            const ssize_t sentBytes = sender.Send(socketId, responsePayload, responseConfig);
            {
                std::ostringstream oss;
                oss << "Sent response bytes=" << sentBytes
                    << " for PGN=0x" << std::hex << requestedPgn
                    << " to SA=0x" << static_cast<uint32_t>(targetAddress)
                    << std::dec;
                logLine(oss.str());
            }
        };

        PeriodicTransmissionManager transmissionManager(
            kMaxActiveSessions,
            std::chrono::milliseconds(kSessionInactivityTimeoutMs),
            logLine);

        for (;;)
        {
            const J1939RxMessage request = receiver.Receive(socketId);

            if (!isRequestPayload(request.payload))
            {
                std::ostringstream oss;
                oss << "Ignoring non-request frame (len=" << request.payload.size() << ")";
                logLine(oss.str());
                continue;
            }

            // J1939 request payload carries requested PGN in first 3 bytes.
            const uint32_t requestedPgn =
                static_cast<uint32_t>(request.payload[0]) |
                (static_cast<uint32_t>(request.payload[1]) << 8) |
                (static_cast<uint32_t>(request.payload[2]) << 16);

            const TransmissionKey key{requestedPgn, request.addr};

            {
                std::ostringstream oss;
                oss << "RX PGN=0x" << std::hex << request.pgn
                    << " SA=0x" << static_cast<uint32_t>(request.addr)
                    << " RequestedPGN=0x" << requestedPgn << std::dec;
                logLine(oss.str());
            }

            const auto it = contents->find(requestedPgn);
            if (it == contents->end())
            {
                std::ostringstream oss;
                oss << "No XML entry for requested PGN 0x"
                    << std::hex << requestedPgn << std::dec;
                logLine(oss.str());
                transmissionManager.Stop(key);
                continue;
            }

            {
                std::ostringstream oss;
                oss << "Found requested PGN: DLC=" << static_cast<uint32_t>(it->second.dlc)
                    << " signals=" << it->second.signals.size()
                    << " cycle_ms=" << it->second.cycle_time_ms;
                logLine(oss.str());
            }

            try
            {
                sendResponseForRequestedPgn(key);
            }
            catch (const std::exception& ex)
            {
                logLine(std::string("Immediate TX error: ") + ex.what());
                transmissionManager.Stop(key);
                continue;
            }

            const uint32_t cycleMs = it->second.cycle_time_ms;
            if (cycleMs > 0)
            {
                const uint32_t clampedCycleMs = ClampCycleMs(cycleMs);
                const bool accepted = transmissionManager.StartOrRefresh(
                    key,
                    std::chrono::milliseconds(clampedCycleMs),
                    sendResponseForRequestedPgn);

                if (!accepted)
                {
                    logLine("Max active periodic sessions reached, rejecting new periodic request");
                    continue;
                }

                std::ostringstream oss;
                oss << "Periodic session active for RequestedPGN=0x"
                    << std::hex << requestedPgn << std::dec
                    << " every " << clampedCycleMs << " ms";
                if (clampedCycleMs != cycleMs)
                    oss << " (clamped from " << cycleMs << " ms)";
                logLine(oss.str());
            }
            else
            {
                transmissionManager.Stop(key);
                std::ostringstream oss;
                oss << "RequestedPGN=0x" << std::hex << requestedPgn << std::dec
                    << " has cycle_ms=0, one-shot response only";
                logLine(oss.str());
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        if (socketId >= 0)
            close(socketId);
        return 1;
    }

    if (socketId >= 0)
        close(socketId);

    return 0;
}