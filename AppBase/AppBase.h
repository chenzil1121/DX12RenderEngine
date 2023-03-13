#pragma once
#include"Utility.h"
#include <WindowsX.h>
#include <string>
#include"GameTimer.h"
#include"RenderDevice.h"
#include"Camera.h"

class AppBase
{
protected:

    AppBase(HINSTANCE hInstance, int windowsWidth, int windowsHeight);
    AppBase(const AppBase& rhs) = delete;
    AppBase& operator=(const AppBase& rhs) = delete;
    virtual ~AppBase();

public:

    static AppBase* GetApp();

    HINSTANCE AppInst()const;
    HWND      MainWnd()const;


    int Run();

    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:

    virtual void OnResize() = 0;
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Render(const GameTimer& gt) = 0;
    virtual void Present() = 0;

    // Convenience overrides for handling mouse input.
    virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
    virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
    virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

protected:

    bool InitMainWindow();
    void CalculateFrameStats();

protected:

    static AppBase* m_App;
    RenderDevice m_Device;
    HINSTANCE m_hAppInst = nullptr; // application instance handle
    HWND      m_hMainWnd = nullptr; // main window handle
    bool      m_AppPaused = false;  // is the application paused?
    bool      m_Minimized = false;  // is the application minimized?
    bool      m_Maximized = false;  // is the application maximized?
    bool      m_Resizing = false;   // are the resize bars being dragged?
    bool      m_FullscreenState = false;// fullscreen enabled
    bool      m_FirstResize = false;

    // Used to keep track of the “delta-time?and game time (?.4).
    GameTimer m_Timer;

	Camera m_Camera;
	POINT m_LastMousePos;

    // Derived class should set these in derived constructor to customize starting values.
    std::wstring m_MainWndCaption = L"d3d App";
};