#ifndef BUFFERED_CHANNEL_H_
#define BUFFERED_CHANNEL_H_

#include <windows.h>
#include <queue>
#include <stdexcept>
#include <utility>

template<class T>
class BufferedChannel {
public:
    explicit BufferedChannel(int size)
        : buffer_size(size > 0 ? size : 1),  
        closed(false) {

        InitializeCriticalSection(&cs);

        has_space = CreateEvent(NULL, TRUE, TRUE, NULL);    
        has_data = CreateEvent(NULL, TRUE, FALSE, NULL);    
    }

    ~BufferedChannel() {
        Close();
        DeleteCriticalSection(&cs);
        CloseHandle(has_space);
        CloseHandle(has_data);
    }

    void Send(T value) {
        EnterCriticalSection(&cs);

        try {
            if (closed) {
                LeaveCriticalSection(&cs);
                throw std::runtime_error("Cannot send to closed channel");
            }

            while (queue.size() >= buffer_size) {
                ResetEvent(has_space);
                LeaveCriticalSection(&cs);

                WaitForSingleObject(has_space, INFINITE);

                EnterCriticalSection(&cs);
                if (closed) {
                    LeaveCriticalSection(&cs);
                    throw std::runtime_error("Cannot send to closed channel");
                }
            }

            queue.push(std::move(value));

            SetEvent(has_data);

            if (queue.size() < buffer_size) {
                SetEvent(has_space);
            }

        }
        catch (...) {
            LeaveCriticalSection(&cs);
            throw;
        }

        LeaveCriticalSection(&cs);
    }

    std::pair<T, bool> Recv() {
        EnterCriticalSection(&cs);

        if (closed && queue.empty()) {
            LeaveCriticalSection(&cs);
            return std::make_pair(T(), false);
        }

        while (queue.empty() && !closed) {
            ResetEvent(has_data);
            LeaveCriticalSection(&cs);

            WaitForSingleObject(has_data, INFINITE);

            EnterCriticalSection(&cs);
        }

        if (queue.empty()) {
            LeaveCriticalSection(&cs);
            return std::make_pair(T(), false);
        }

        T value = std::move(queue.front());
        queue.pop();

        SetEvent(has_space);

        if (!queue.empty()) {
            SetEvent(has_data);
        }
        else if (closed) {
            SetEvent(has_data);
        }

        LeaveCriticalSection(&cs);
        return std::make_pair(std::move(value), true);
    }

    void Close() {
        EnterCriticalSection(&cs);

        if (!closed) {
            closed = true;

            SetEvent(has_space);
            SetEvent(has_data);
        }

        LeaveCriticalSection(&cs);
    }

private:
    std::queue<T> queue;
    const size_t buffer_size;
    bool closed;

    CRITICAL_SECTION cs;
    HANDLE has_space;  
    HANDLE has_data; 
};

#endif // BUFFERED_CHANNEL_H_