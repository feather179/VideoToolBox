#ifndef VIDEO_D3D11_WIDGET_H
#define VIDEO_D3D11_WIDGET_H

#include <QtWidgets/QWidget>

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>

#include <wrl/client.h>

#include <memory>
#include <mutex>

using Microsoft::WRL::ComPtr;

class AVFrameBuffer;

class VideoD3D11Widget : public QWidget {
    Q_OBJECT

private:
    ComPtr<ID3D11Device> mD3D11Device;
    ComPtr<ID3D11DeviceContext> mD3D11DeviceContext;
    ComPtr<IDXGISwapChain> mSwapChain;
    ComPtr<ID3D11RenderTargetView> mRenderTargetView;
    ComPtr<ID3D11Texture2D> mTexture;
    ComPtr<ID3D11ShaderResourceView> mShaderResourceView;
    ComPtr<ID3D11VertexShader> mVertexShader;
    ComPtr<ID3D11PixelShader> mPixelShader;
    ComPtr<ID3D11InputLayout> mInputLayout;
    ComPtr<ID3D11Buffer> mVertexBuffer;
    ComPtr<ID3D11Buffer> mIndexBuffer;
    ComPtr<ID3D11SamplerState> mSamplerState;
    D3D11_VIEWPORT mViewport;

    int mFrameWidth, mFrameHeight;

    std::mutex mRenderMutex;

    void init();
    void resetRenderTarget();

public:
    VideoD3D11Widget(QWidget *parent = nullptr);
    ~VideoD3D11Widget();

    QPaintEngine *paintEngine() const override { return nullptr; }
    void render(std::shared_ptr<AVFrameBuffer> frame);
    void render(void *buffer);

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
};

#endif
