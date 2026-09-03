// DXVK Native triangle example using SDL3 WSI + D3D11/DXGI.
//
// Build note: must define DXVK_WSI_SDL3 before including dxvk/wsi/native_wsi.h
// so that HWND maps to SDL_Window*.

#define DXVK_WSI_SDL3 1

#include <windows.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <wsi/native_wsi.h>

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstddef>
#include <cstdint>

#include "triangle_vs.h"
#include "triangle_ps.h"

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while (0)

#define HR_CHECK(expr)                                                       \
    do {                                                                     \
        HRESULT _hr = (expr);                                                \
        if (FAILED(_hr)) {                                                   \
            std::fprintf(stderr, "HRESULT failed at %s:%d code=0x%08lx\n",   \
                         __FILE__, __LINE__, (unsigned long)_hr);            \
            return 1;                                                        \
        }                                                                    \
    } while (0)

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

int main(int /*argc*/, char** /*argv*/) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const int W = 1024;
    const int H = 768;
    SDL_Window* window = SDL_CreateWindow("DXVK Native Triangle (SDL3 + D3D11)",
                                           W, H, 0);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // ---- DXGI / D3D11 setup -----------------------------------------------
    IDXGIFactory1* factory1 = nullptr;
    HR_CHECK(CreateDXGIFactory1(IID_IDXGIFactory1, (void**)&factory1));

    IDXGIFactory2* factory2 = nullptr;
    HR_CHECK(factory1->QueryInterface(IID_IDXGIFactory2, (void**)&factory2));

    // Use the first adapter. Passing nullptr to D3D11CreateDevice is also fine,
    // but we keep this explicit so the swapchain and device share the adapter.
    IDXGIAdapter1* adapter = nullptr;
    if (factory1->EnumAdapters1(0, &adapter) != S_OK) {
        std::fprintf(stderr, "No DXGI adapter found.\n");
        return 1;
    }

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT createFlags = 0;

    ID3D11Device*         device  = nullptr;
    ID3D11DeviceContext*  ctx     = nullptr;
    D3D_FEATURE_LEVEL     chosen  = D3D_FEATURE_LEVEL_9_1;
    HR_CHECK(D3D11CreateDevice(
        adapter,
        D3D_DRIVER_TYPE_UNKNOWN, // explicit adapter: driver type is ignored
        nullptr,
        createFlags,
        featureLevels,
        (UINT)(sizeof(featureLevels) / sizeof(featureLevels[0])),
        D3D11_SDK_VERSION,
        &device,
        &chosen,
        &ctx));

    // ---- Swapchain --------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width              = 0;   // use window size
    scDesc.Height             = 0;
    scDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.SampleDesc.Count   = 1;
    scDesc.SampleDesc.Quality = 0;
    scDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount        = 2;
    scDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.Scaling            = DXGI_SCALING_STRETCH;
    scDesc.AlphaMode          = DXGI_ALPHA_MODE_IGNORE;
    scDesc.Flags              = 0;

    IDXGISwapChain1* swapChain = nullptr;
    HR_CHECK(factory2->CreateSwapChainForHwnd(
        device,
        dxvk::wsi::toHwnd(window),
        &scDesc,
        nullptr,
        nullptr,
        &swapChain));

    // ---- Render target ----------------------------------------------------
    ID3D11Texture2D* backBuffer = nullptr;
    HR_CHECK(swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer));

    ID3D11RenderTargetView* rtv = nullptr;
    HR_CHECK(device->CreateRenderTargetView(backBuffer, nullptr, &rtv));
    SAFE_RELEASE(backBuffer);

    // ---- Input layout -----------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(Vertex, r),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* inputLayout = nullptr;
    HR_CHECK(device->CreateInputLayout(
        layout,
        (UINT)(sizeof(layout) / sizeof(layout[0])),
        g_triVSByteCode,  sizeof(g_triVSByteCode),
        &inputLayout));

    // ---- Shaders ----------------------------------------------------------
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader*  ps = nullptr;
    HR_CHECK(device->CreateVertexShader(
        g_triVSByteCode, sizeof(g_triVSByteCode), nullptr, &vs));
    HR_CHECK(device->CreatePixelShader(
        g_triPSByteCode, sizeof(g_triPSByteCode), nullptr, &ps));

    // ---- Vertex buffer ----------------------------------------------------
    // Clockwise on screen (top -> bottom-right -> bottom-left): D3D11 default
    // rasterizer culls back faces (CCW), so winding matters!
    Vertex verts[] = {
        {  0.0f,  0.75f, 0.0f,   1.0f, 0.0f, 0.0f, 1.0f }, // top    - red
        {  0.75f,-0.75f, 0.0f,   0.0f, 1.0f, 0.0f, 1.0f }, // bottom right - green
        { -0.75f,-0.75f, 0.0f,   0.0f, 0.0f, 1.0f, 1.0f }, // bottom left  - blue
    };
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth      = sizeof(verts);
    vbDesc.Usage          = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA vbInit = {};
    vbInit.pSysMem = verts;
    ID3D11Buffer* vb = nullptr;
    HR_CHECK(device->CreateBuffer(&vbDesc, &vbInit, &vb));

    // ---- Viewport ---------------------------------------------------------
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = (float)W;
    vp.Height   = (float)H;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    // ---- Main loop --------------------------------------------------------
    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (ev.type == SDL_EVENT_KEY_DOWN &&
                       ev.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }

        const float clearColor[4] = { 0.1f, 0.2f, 0.4f, 1.0f };
        ctx->ClearRenderTargetView(rtv, clearColor);

        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->RSSetViewports(1, &vp);
        ctx->IASetInputLayout(inputLayout);

        UINT stride = (UINT)sizeof(Vertex);
        UINT off    = 0;
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &off);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);

        ctx->Draw(3, 0);

        HRESULT hr = swapChain->Present(1, 0); // vsync on
        if (FAILED(hr)) {
            std::fprintf(stderr, "Present failed: 0x%08lx\n", (unsigned long)hr);
            running = false;
        }
    }

    // ---- Cleanup ----------------------------------------------------------
    ctx->ClearState();
    ctx->Flush();

    SAFE_RELEASE(vb);
    SAFE_RELEASE(ps);
    SAFE_RELEASE(vs);
    SAFE_RELEASE(inputLayout);
    SAFE_RELEASE(rtv);
    SAFE_RELEASE(swapChain);
    SAFE_RELEASE(ctx);
    SAFE_RELEASE(device);
    SAFE_RELEASE(adapter);
    SAFE_RELEASE(factory2);
    SAFE_RELEASE(factory1);

    SDL_DestroyWindow(window);
    SDL_Quit();
    std::printf("Triangle example: finished cleanly.\n");
    return 0;
}
