#ifndef RECORDER_H
#define RECORDER_H

#include <functional>

#include <Windows.h>

class Recorder {
private:
protected:
    std::function<void(void *)> mFrameArrivedCallback;

public:
    Recorder() = default;
    virtual ~Recorder() = default;

    virtual bool init() = 0;
    virtual void start(HWND hwnd) = 0;
    void setFrameArrivedCallback(std::function<void(void *)> cb) { mFrameArrivedCallback = cb; }
};

class DXGIRecorder : public Recorder {};

class GDIRecorder : public Recorder {};

class CameraRecorder : public Recorder {};

#endif
