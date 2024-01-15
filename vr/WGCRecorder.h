#ifndef WGC_RECORDER_H
#define WGC_RECORDER_H

#include "Recorder.h"

#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.System.h>
#include <Windows.Graphics.Capture.Interop.h>
#include <d3d11.h>

class WGCRecorder : public Recorder {
private:
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice mDirect3DDevice = nullptr;
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem mCaptureItem = nullptr;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool mCaptureFramePool = nullptr;
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession mCaptureSession = nullptr;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker
        mFrameArrived;

    winrt::com_ptr<ID3D11Device> mD3D11Device;
    winrt::com_ptr<ID3D11DeviceContext> mD3D11DeviceContext;

    void onFrameArrived(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const &sender,
                        winrt::Windows::Foundation::IInspectable const &args);

public:
    WGCRecorder();
    virtual ~WGCRecorder();

    virtual bool init() override;
    virtual void start(HWND hwnd) override;
};

#endif
