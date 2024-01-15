#include "WGCRecorder.h"

#include <foundation/Log.h>

#include <dxgi1_2.h>

extern "C" {
HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(::IDXGIDevice *dxgiDevice,
                                                       ::IInspectable **graphicsDevice);

HRESULT __stdcall CreateDirect3D11SurfaceFromDXGISurface(::IDXGISurface *dgxiSurface,
                                                         ::IInspectable **graphicsSurface);
}

struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDirect3DDxgiInterfaceAccess
    : ::IUnknown {
    virtual HRESULT __stdcall GetInterface(GUID const &id, void **object) = 0;
};

template <typename T>
static auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const &object) {
    auto access = object.as<IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<T> result;
    winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
    return result;
}

WGCRecorder::WGCRecorder() {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
}

WGCRecorder::~WGCRecorder() {
    mFrameArrived.revoke();
    mCaptureFramePool.Close();
    mCaptureSession.Close();
    winrt::uninit_apartment();
}

bool WGCRecorder::init() {
    HRESULT hr;

    auto isCaptureSupported =
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
    if (!isCaptureSupported) return false;

    // winrt::com_ptr<ID3D11Device> d3dDevice;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                           D3D11_SDK_VERSION, mD3D11Device.put(), nullptr, nullptr);
    if (DXGI_ERROR_UNSUPPORTED == hr) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0,
                               D3D11_SDK_VERSION, mD3D11Device.put(), nullptr, nullptr);
    }

    auto dxgiDevice = mD3D11Device.as<IDXGIDevice>();

    winrt::com_ptr<IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put());

    mDirect3DDevice =
        inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    mD3D11Device->GetImmediateContext(mD3D11DeviceContext.put());

    return true;
}

void WGCRecorder::start(HWND hwnd) {
    HRESULT hr;

    HMONITOR hMonitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    auto activationFactory =
        winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
    auto interopFactory = activationFactory.as<IGraphicsCaptureItemInterop>();
    // winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = {nullptr};
    // hr = interopFactory->CreateForWindow(
    //     GetDesktopWindow(),
    //     winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
    //     reinterpret_cast<void **>(winrt::put_abi(mCaptureItem)));

    hr = interopFactory->CreateForMonitor(
        hMonitor, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        reinterpret_cast<void **>(winrt::put_abi(mCaptureItem)));

    auto size = mCaptureItem.Size();

    // winrt::DirectXPixelFormat
    mCaptureFramePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
        mDirect3DDevice,
        static_cast<winrt::Windows::Graphics::DirectX::DirectXPixelFormat>(
            DXGI_FORMAT_R8G8B8A8_UNORM),
        2, size);
    mCaptureSession = mCaptureFramePool.CreateCaptureSession(mCaptureItem);
    mFrameArrived =
        mCaptureFramePool.FrameArrived(winrt::auto_revoke, {this, &WGCRecorder::onFrameArrived});
    mCaptureSession.StartCapture();
}

void WGCRecorder::onFrameArrived(
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const &sender,
    winrt::Windows::Foundation::IInspectable const &) {

    // static bool dump = true;
    // LOGD("%s\n", __PRETTY_FUNCTION__);
    HRESULT hr;
    HANDLE handle;

    auto frame = sender.TryGetNextFrame();
    auto frameContentSize = frame.ContentSize();
    auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

    D3D11_TEXTURE2D_DESC desc;
    frameTexture->GetDesc(&desc);

    winrt::com_ptr<IDXGIResource1> dxgiResource;
    hr = frameTexture->QueryInterface(__uuidof(IDXGIResource1), dxgiResource.put_void());
    // hr = dxgiResource->GetSharedHandle(&handle);
    hr = dxgiResource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &handle);

    if (mFrameArrivedCallback) {
        mFrameArrivedCallback((void *)handle);
    }

#if 0

    // D3D11_BIND_SHADER_RESOURCE
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    winrt::com_ptr<ID3D11Texture2D> texture;
    hr = mD3D11Device->CreateTexture2D(&desc, nullptr, texture.put());
    mD3D11DeviceContext->CopyResource(texture.get(), frameSurface.get());

    D3D11_MAPPED_SUBRESOURCE map;
    hr = mD3D11DeviceContext->Map(texture.get(), 0, D3D11_MAP_READ, 0, &map);

    // if (dump) {
    //     dump = false;
    //     FILE *file = fopen("D:/Desktop/VideoToolBox/build/windows/x64/debug/dump.bin", "wb");
    //     fwrite(map.pData, 1, map.RowPitch * desc.Height, file);
    //     fclose(file);
    // }

    mD3D11DeviceContext->Unmap(texture.get(), 0);
#endif
}