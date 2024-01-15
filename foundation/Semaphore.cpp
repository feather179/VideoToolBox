#include "Semaphore.h"

Semaphore::Semaphore() {
    mCount = 0;
}

Semaphore::Semaphore(int count) : mCount(count) {}

void Semaphore::acquire() {
    std::unique_lock<std::mutex> lock(mMutex);
    mCv.wait(lock, [=]() { return mCount > 0; });
    mCount--;
}

void Semaphore::release() {
    std::unique_lock<std::mutex> lock(mMutex);
    mCount++;
    mCv.notify_one();
}
