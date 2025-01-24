#include "InitD3D9.h"

#include <d3d9.h>

LPDIRECT3D9 d3d;    
LPDIRECT3DDEVICE9 d3ddev;

void initD3D(HWND hWnd)
{
    d3d = Direct3DCreate9(D3D_SDK_VERSION);   

    D3DPRESENT_PARAMETERS d3dpp;    

    ZeroMemory(&d3dpp, sizeof(d3dpp));    
    d3dpp.Windowed = TRUE;   
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;   
    d3dpp.hDeviceWindow = hWnd;  

    d3d->CreateDevice(D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp,
        &d3ddev);

}

void render_frame(void)
{
    d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 40, 100), 1.0f, 0);
          d3ddev->BeginScene();   


          d3ddev->EndScene();   
    d3ddev->Present(NULL, NULL, NULL, NULL);    
}

void cleanD3D(void)
{
    d3ddev->Release();   
    d3d->Release();   
}