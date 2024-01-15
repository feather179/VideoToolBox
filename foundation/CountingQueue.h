#ifndef COUNTING_QUEUE_H
#define COUNTING_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class CountingQueue {
public:
    explicit CountingQueue(size_t maxCount) : mMaxCount(maxCount) {
        mAbort = false;
    }
    ~CountingQueue() {
        abort();
    }

    void push(const T& value) {
        std::unique_lock<std::mutex> lock(mMutex);
        mCv.wait(lock, [&]() -> bool { return (mQueue.size() < mMaxCount) || mAbort; });
        if (!mAbort) {
            mQueue.push(value);
            mCv.notify_all();
        }
    }

    bool pop(T &value) {
        std::unique_lock<std::mutex> lock(mMutex);
        mCv.wait(lock, [&]() -> bool { return (!mQueue.empty()) || mAbort; });
        if (!mAbort) {
            value = mQueue.front();
            mQueue.pop();
            mCv.notify_all();
            return true;
        }
        return false;
    }

    //void flush() {
    //    std::unique_lock<std::mutex> lock(mMutex);
    //    // clear queue
    //    while (!mQueue.empty()) {
    //        mQueue.pop();
    //    }
    //    mAbort = true;
    //    mCv.notify_all();
    //    mAbort = false;
    //}

    bool empty() {
        std::unique_lock<std::mutex> lock(mMutex);
        return mQueue.empty();
    }

    size_t size() {
        std::unique_lock<std::mutex> lock(mMutex);
        return mQueue.size();
    }

    void init() {
        std::unique_lock<std::mutex> lock(mMutex);
        mAbort = false;
    }

    void abort() {
        std::unique_lock<std::mutex> lock(mMutex);
        while (!mQueue.empty()) {
            mQueue.pop();
        }
        mAbort = true;
        mCv.notify_all();
    }

    bool isAbort() {
        std::unique_lock<std::mutex> lock(mMutex);
        return mAbort;
    }


private:
    size_t mMaxCount;
    std::queue<T> mQueue;
    std::mutex mMutex;
    std::condition_variable mCv;
    bool mAbort;

};


#endif // COUNTINGQUEUE_H
