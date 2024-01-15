#include "VideoD3D11Widget.h"

#include "foundation/Log.h"
#include "foundation/FFBuffer.h"

// https://zhuanlan.zhihu.com/p/587747958

extern "C" {
#include "libavutil/frame.h"
}

struct Vertex {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT2 Tex;
};

static const char *vertexShader = "struct VSIn\
{\
    float3 pos : POSITION;\
    float2 tex : TEXCOORD;\
};\
struct VSOut\
{\
    float4 pos : SV_Position;\
    float2 tex : TEXCOORD;\
};\
VSOut main(VSIn vIn)\
{\
    VSOut vsOut;\
    vsOut.pos = float4(vIn.pos.x, vIn.pos.y, vIn.pos.z, 1.0);\
    vsOut.tex = vIn.tex;\
    return vsOut;\
}";

static const char *pixelShader = "struct VSOut\
{\
    float4 pos : SV_Position;\
    float2 tex : TEXCOORD;\
};\
Texture2D tex : register(t0);\
SamplerState samplerLinear : register(s0);\
float4 main(VSOut pIn) : SV_Target\
{\
    return tex.Sample(samplerLinear, pIn.tex);\
}";

VideoD3D11Widget::VideoD3D11Widget(QWidget *parent) : QWidget(parent) {
    QWidget::setAttribute(Qt::WA_PaintOnScreen);

    mFrameWidth = 0;
    mFrameHeight = 0;

    init();
}

VideoD3D11Widget::~VideoD3D11Widget() {}

void VideoD3D11Widget::init() {
    HRESULT hr;

    DXGI_MODE_DESC bufferDesc;
    ZeroMemory(&bufferDesc, sizeof(DXGI_MODE_DESC));
    bufferDesc.Width = 0;
    bufferDesc.Height = 0;
    bufferDesc.RefreshRate.Numerator = 60;
    bufferDesc.RefreshRate.Denominator = 1;
    bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
    swapChainDesc.BufferDesc = bufferDesc;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = (HWND)winId();
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                       createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION,
                                       &swapChainDesc, mSwapChain.ReleaseAndGetAddressOf(),
                                       mD3D11Device.ReleaseAndGetAddressOf(), &featureLevel,
                                       mD3D11DeviceContext.ReleaseAndGetAddressOf());
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2,
            D3D11_SDK_VERSION, &swapChainDesc, mSwapChain.ReleaseAndGetAddressOf(),
            mD3D11Device.ReleaseAndGetAddressOf(), &featureLevel,
            mD3D11DeviceContext.ReleaseAndGetAddressOf());
    }

    {
        ID3DBlob *pBlob = nullptr;
        UINT sharderFlag = 0;
        // sharderFlag |= D3DCOMPILE_DEBUG;
        // sharderFlag |= D3DCOMPILE_SKIP_OPTIMIZATION;
        hr = D3DCompile(vertexShader, strlen(vertexShader), nullptr, nullptr, nullptr, "main",
                        "vs_4_0", sharderFlag, 0, &pBlob, nullptr);
        hr = mD3D11Device->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
                                              nullptr, mVertexShader.ReleaseAndGetAddressOf());
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
             D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        hr = mD3D11Device->CreateInputLayout(layout, 2, pBlob->GetBufferPointer(),
                                             pBlob->GetBufferSize(),
                                             mInputLayout.ReleaseAndGetAddressOf());
        mD3D11DeviceContext->IASetInputLayout(mInputLayout.Get());
    }

    {
        ID3DBlob *pBlob = nullptr;
        UINT sharderFlag = 0;
        // sharderFlag |= D3DCOMPILE_DEBUG;
        // sharderFlag |= D3DCOMPILE_SKIP_OPTIMIZATION;
        hr = D3DCompile(pixelShader, strlen(pixelShader), nullptr, nullptr, nullptr, "main",
                        "ps_4_0", sharderFlag, 0, &pBlob, nullptr);
        hr = mD3D11Device->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
                                             nullptr, mPixelShader.ReleaseAndGetAddressOf());
    }

    {
        Vertex vertices[] = {
            {DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f)},
            {DirectX::XMFLOAT3(-1.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f)},
            {DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f)},
            {DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f)},
        };

        D3D11_BUFFER_DESC verticesDesc;
        ZeroMemory(&verticesDesc, sizeof(D3D11_BUFFER_DESC));
        verticesDesc.ByteWidth = sizeof(vertices);
        verticesDesc.Usage = D3D11_USAGE_IMMUTABLE;
        verticesDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        verticesDesc.CPUAccessFlags = 0;
        verticesDesc.MiscFlags = 0;
        verticesDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA resourceDate;
        ZeroMemory(&resourceDate, sizeof(D3D11_SUBRESOURCE_DATA));
        resourceDate.pSysMem = vertices;
        resourceDate.SysMemPitch = 0;
        resourceDate.SysMemSlicePitch = 0;

        hr = mD3D11Device->CreateBuffer(&verticesDesc, &resourceDate,
                                        mVertexBuffer.ReleaseAndGetAddressOf());

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        mD3D11DeviceContext->IASetVertexBuffers(0, 1, mVertexBuffer.GetAddressOf(), &stride,
                                                &offset);
    }

    {
        DirectX::XMUINT3 index[] = {
            {0, 1, 2},
            {0, 2, 3},
        };

        D3D11_BUFFER_DESC indexDesc;
        ZeroMemory(&indexDesc, sizeof(D3D11_BUFFER_DESC));
        indexDesc.ByteWidth = sizeof(index);
        indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        indexDesc.CPUAccessFlags = 0;
        indexDesc.MiscFlags = 0;
        indexDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA resourceDate;
        ZeroMemory(&resourceDate, sizeof(D3D11_SUBRESOURCE_DATA));
        resourceDate.pSysMem = index;
        resourceDate.SysMemPitch = 0;
        resourceDate.SysMemSlicePitch = 0;

        hr = mD3D11Device->CreateBuffer(&indexDesc, &resourceDate,
                                        mIndexBuffer.ReleaseAndGetAddressOf());
        mD3D11DeviceContext->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    }

    D3D11_SAMPLER_DESC sampleDesc;
    ZeroMemory(&sampleDesc, sizeof(D3D11_SAMPLER_DESC));
    sampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampleDesc.MinLOD = 0;
    sampleDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = mD3D11Device->CreateSamplerState(&sampleDesc, mSamplerState.ReleaseAndGetAddressOf());
    mD3D11DeviceContext->PSSetSamplers(0, 1, mSamplerState.GetAddressOf());

    mD3D11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    resetRenderTarget();
}

void VideoD3D11Widget::resetRenderTarget() {
    HRESULT hr;

    int width = this->width();
    int height = this->height();

    // MUST release render target view firstly
    // otherwise resize buffer will fail
    mRenderTargetView.Reset();

    hr = mSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    ComPtr<ID3D11Texture2D> backBuffer;
    mSwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.ReleaseAndGetAddressOf()));
    mD3D11Device->CreateRenderTargetView(backBuffer.Get(), nullptr,
                                         mRenderTargetView.ReleaseAndGetAddressOf());
}

void VideoD3D11Widget::render(std::shared_ptr<AVFrameBuffer> frame) {
    std::lock_guard<std::mutex> lock(mRenderMutex);

    if (frame->get()->width != mFrameWidth || frame->get()->height != mFrameHeight || !mTexture) {
        mFrameWidth = frame->get()->width;
        mFrameHeight = frame->get()->height;

        if (mFrameWidth <= 0 || mFrameHeight <= 0) return;

        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width = mFrameWidth;
        texDesc.Height = mFrameHeight;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        mD3D11Device->CreateTexture2D(&texDesc, nullptr, mTexture.ReleaseAndGetAddressOf());
    }

    unsigned char *data = frame->get()->data[0];
    int linesize = frame->get()->linesize[0];

    mD3D11DeviceContext->UpdateSubresource(mTexture.Get(), 0, nullptr, data, linesize, 0);

    update();
}

void VideoD3D11Widget::render(void *buffer /* HANDLE */) {
    HRESULT hr;
    ComPtr<ID3D11Device1> d3dDevice1;
    HANDLE handle = buffer;

    std::lock_guard<std::mutex> lock(mRenderMutex);

    hr = mD3D11Device->QueryInterface<ID3D11Device1>(d3dDevice1.ReleaseAndGetAddressOf());
    hr = d3dDevice1->OpenSharedResource1(
        handle, __uuidof(ID3D11Texture2D),
        reinterpret_cast<void **>(mTexture.ReleaseAndGetAddressOf()));

    D3D11_TEXTURE2D_DESC desc;
    mTexture->GetDesc(&desc);

    mFrameWidth = desc.Width;
    mFrameHeight = desc.Height;

    update();
}

void VideoD3D11Widget::resizeEvent(QResizeEvent *event) {
    resetRenderTarget();
}

void VideoD3D11Widget::paintEvent(QPaintEvent *event) {
    std::lock_guard<std::mutex> lock(mRenderMutex);

    int x = 0, y = 0;
    int width = this->width(), height = this->height();
    if (mFrameWidth > 0 && mFrameHeight > 0 && width > 0 && height > 0) {
        float widthRatio = (float)this->width() / (float)mFrameWidth;
        float heightRatio = (float)this->height() / (float)mFrameHeight;
        float ratio = widthRatio < heightRatio ? widthRatio : heightRatio;
        width = (int)(mFrameWidth * ratio);
        height = (int)(mFrameHeight * ratio);
        x = (this->width() - width) / 2;
        y = (this->height() - height) / 2;
    }

    ZeroMemory(&mViewport, sizeof(D3D11_VIEWPORT));
    mViewport.TopLeftX = x;
    mViewport.TopLeftY = y;
    mViewport.Width = width;
    mViewport.Height = height;
    mViewport.MinDepth = 0.0f;
    mViewport.MaxDepth = 1.0f;
    mD3D11DeviceContext->RSSetViewports(1, &mViewport);

    mD3D11DeviceContext->OMSetRenderTargets(1, mRenderTargetView.GetAddressOf(), nullptr);
    const float bgcolor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    mD3D11DeviceContext->ClearRenderTargetView(mRenderTargetView.Get(), bgcolor);

    D3D11_TEXTURE2D_DESC desc;
    if (mTexture) {
        mTexture->GetDesc(&desc);

        // ID3D11ShaderResourceView *shaderResourceView = nullptr;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        mD3D11Device->CreateShaderResourceView(mTexture.Get(), &srvDesc,
                                               mShaderResourceView.ReleaseAndGetAddressOf());

        mD3D11DeviceContext->VSSetShader(mVertexShader.Get(), nullptr, 0);
        mD3D11DeviceContext->PSSetShader(mPixelShader.Get(), nullptr, 0);
        mD3D11DeviceContext->PSSetShaderResources(0, 1, mShaderResourceView.GetAddressOf());

        mD3D11DeviceContext->DrawIndexed(6, 0, 0);
    }

    mSwapChain->Present(1, 0);
}