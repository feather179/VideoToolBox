#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <mutex>
#include <condition_variable>

class Semaphore {
private:
    int mCount;
    std::mutex mMutex;
    std::condition_variable mCv;

public:
    Semaphore();
    explicit Semaphore(int count);
    ~Semaphore() = default;

    void acquire();
    void release();
};

#endif // SEMAPHORE_H
