﻿/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/

#include "Base/Platform.h"

#if defined(__DAVAENGINE_WIN_UAP__)

#include "Core/Core.h"
#include "Render/Renderer.h"
#include "Render/2D/Systems/VirtualCoordinatesSystem.h"
#include "Render/2D/Systems/RenderSystem2D.h"
#include "UI/UIScreenManager.h"

#include "Platform/SystemTimer.h"
#include "Platform/TemplateWin32/CorePlatformWinUAP.h"
#include "Platform/TemplateWin32/WinUAPXamlApp.h"
#include "Platform/TemplateWin32/DispatcherWinUAP.h"
#include "Platform/DeviceInfo.h"

#include "FileSystem/Logger.h"

#include "Utils/Utils.h"

#include "WinUAPXamlApp.h"

extern void FrameworkDidLaunched();
extern void FrameworkWillTerminate();

using namespace ::Windows::System;
using namespace ::Windows::Foundation;
using namespace ::Windows::UI::Core;
using namespace ::Windows::UI::Xaml;
using namespace ::Windows::UI::Xaml::Controls;
using namespace ::Windows::UI::Input;
using namespace ::Windows::UI::ViewManagement;
using namespace ::Windows::Devices::Input;
using namespace ::Windows::ApplicationModel;
using namespace ::Windows::Graphics::Display;
using namespace ::Windows::ApplicationModel::Core;
using namespace ::Windows::UI::Xaml::Media;
using namespace ::Windows::System::Threading;
using namespace ::Windows::Phone::UI::Input;

namespace DAVA
{


WinUAPXamlApp::WinUAPXamlApp()
    : core(static_cast<CorePlatformWinUAP*>(Core::Instance()))
{}

WinUAPXamlApp::~WinUAPXamlApp() {}

DisplayOrientations WinUAPXamlApp::GetDisplayOrientation()
{
    return displayOrientation;
}

ApplicationViewWindowingMode WinUAPXamlApp::GetScreenMode()
{
    return isFullscreen ? ApplicationViewWindowingMode::FullScreen :
                          ApplicationViewWindowingMode::PreferredLaunchViewSize;
}

void WinUAPXamlApp::SetScreenMode(ApplicationViewWindowingMode screenMode)
{
    // Note: must run on UI thread
    bool fullscreen = ApplicationViewWindowingMode::FullScreen == screenMode;
    SetFullScreen(fullscreen);
}

Windows::Foundation::Size WinUAPXamlApp::GetCurrentScreenSize()
{
    return Windows::Foundation::Size(windowWidth, windowHeight);
}

void WinUAPXamlApp::SetCursorPinning(bool isPinning)
{
    // should be started on UI thread
    Logger::FrameworkDebug("[CorePlatformWinUAP] CursorPinning %d", static_cast<int32>(isPinning));
    if (isPhoneApiDetected)
    {
        return;
    }
    isPinning = isCursorPinning;
}

void WinUAPXamlApp::SetCursorVisible(bool isVisible)
{
    // should be started on UI thread
    if (isPhoneApiDetected)
    {
        return;
    }
    Logger::FrameworkDebug("[CorePlatformWinUAP] CursorState %d", static_cast<int32>(isVisible));
    if (isVisible != isMouseCursorShown)
    {
        Window::Current->CoreWindow->PointerCursor = (isVisible ? ref new CoreCursor(CoreCursorType::Arrow, 0) : nullptr);
        isMouseCursorShown = isVisible;
    }
}

void WinUAPXamlApp::OnLaunched(::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ args)
{
    CoreWindow^ coreWindow = Window::Current->CoreWindow;
    // need initialize device info on UI thread
    // // it's a temporary decision
    float32 landscapeWidth = Max(coreWindow->Bounds.Width, coreWindow->Bounds.Height);
    float32 landscapeHeight = Min(coreWindow->Bounds.Width, coreWindow->Bounds.Height);
    DeviceInfo::InitializeScreenInfo(static_cast<int32>(landscapeWidth), static_cast<int32>(landscapeHeight));
    uiThreadDispatcher = coreWindow->Dispatcher;

    CreateBaseXamlUI();

    UpdateScreenSize(coreWindow->Bounds.Width, coreWindow->Bounds.Height);
    InitRender();

    WorkItemHandler^ workItemHandler = ref new WorkItemHandler([this](Windows::Foundation::IAsyncAction^ action) { Run(); });
    renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

    Window::Current->Activate();
}

void WinUAPXamlApp::AddUIElement(Windows::UI::Xaml::UIElement^ uiElement)
{
    // Note: must be called from UI thread
    canvas->Children->Append(uiElement);
}

void WinUAPXamlApp::RemoveUIElement(Windows::UI::Xaml::UIElement^ uiElement)
{
    // Note: must be called from UI thread
    unsigned int index = 0;
    for (auto x = canvas->Children->First();x->HasCurrent;x->MoveNext(), ++index)
    {
        if (x->Current == uiElement)
        {
            canvas->Children->RemoveAt(index);
            break;
        }
    }
}

void WinUAPXamlApp::PositionUIElement(Windows::UI::Xaml::UIElement^ uiElement, float32 x, float32 y)
{
    // Note: must be called from UI thread
    canvas->SetLeft(uiElement, x);
    canvas->SetTop(uiElement, y);
}

void WinUAPXamlApp::UnfocusUIElement()
{
    // XAML controls cannot be unfocused programmatically, this is especially useful for text fields
    // So use dummy offscreen control that steals focus
    controlThatTakesFocus->Focus(FocusState::Pointer);
}

void WinUAPXamlApp::Run()
{
    dispatcher = std::make_unique<DispatcherWinUAP>();

    Core::Instance()->CreateSingletons();
    
#if RHI_COMLETE // init renderer in FrameworkDidLaunched and SystemAppStarted
    RenderManager::Instance()->BindToCurrentThread();
	ReInitRender();
#endif

	InitCoordinatesSystem();
    // View size and orientation option should be configured in FrameowrkDidLaunched
    FrameworkDidLaunched();

    core->RunOnUIThreadBlocked([this]() {
        SetupEventHandlers();
        SetTitleName();
        InitInput();
        PrepareScreenSize();
        SetDisplayOrientations();
    });

    Core::Instance()->SetIsActive(true);

    Core::Instance()->SystemAppStarted();
    while (!quitFlag)
    {
        dispatcher->ProcessTasks();

        DAVA::uint64 startTime = DAVA::SystemTimer::Instance()->AbsoluteMS();
        
        Core::Instance()->SystemProcessFrame();
        
        uint32 elapsedTime = (uint32)(SystemTimer::Instance()->AbsoluteMS() - startTime);
        int32 sleepMs = 1;
        int32 fps = Renderer::GetDesiredFPS();
        if (fps > 0)
        {
            sleepMs = (1000 / fps) - elapsedTime;
            if (sleepMs > 0)
            {
                Thread::Sleep(sleepMs);
            }
        }
    }

    ApplicationCore* appCore = Core::Instance()->GetApplicationCore();
    if (appCore != nullptr && appCore->OnQuit())
    {
        Application::Current->Exit();
    }

    Core::Instance()->SystemAppFinished();
    FrameworkWillTerminate();
    Core::Instance()->ReleaseSingletons();

    Application::Current->Exit();
}

void WinUAPXamlApp::OnSuspending(::Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args)
{
    isWindowVisible = false;
    Core::Instance()->SetIsActive(isWindowVisible);
}

void WinUAPXamlApp::OnResuming(::Platform::Object^ sender, ::Platform::Object^ args)
{
    isWindowVisible = true;
    Core::Instance()->SetIsActive(isWindowVisible);
}

void WinUAPXamlApp::OnWindowActivationChanged(::Windows::UI::Core::CoreWindow^ sender, ::Windows::UI::Core::WindowActivatedEventArgs^ args)
{
    CoreWindowActivationState state = args->WindowActivationState;
    switch (state)
    {
    case CoreWindowActivationState::CodeActivated:
    case CoreWindowActivationState::PointerActivated:
        Core::Instance()->SetIsActive(true);
        break;
    case CoreWindowActivationState::Deactivated:
        Core::Instance()->SetIsActive(false);
        break;
    default:
        break;
    }
}

void WinUAPXamlApp::OnWindowVisibilityChanged(::Windows::UI::Core::CoreWindow^ sender, ::Windows::UI::Core::VisibilityChangedEventArgs^ args)
{
    // Propagate to main thread
    isWindowVisible = args->Visible;
    Core::Instance()->SetIsActive(isWindowVisible);
}

void WinUAPXamlApp::OnWindowSizeChanged(::Windows::UI::Core::CoreWindow^ sender, ::Windows::UI::Core::WindowSizeChangedEventArgs^ args)
{
    // Propagate to main thread
    float32 w = args->Size.Width;
    float32 h = args->Size.Height;
    core->RunOnMainThread([this, w, h]() {
        UpdateScreenSize(w, h);
        ReInitRender();
        ReInitCoordinatesSystem();
        UIScreenManager::Instance()->ScreenSizeChanged();
    });
}

void WinUAPXamlApp::OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
    PointerPoint^ pointPtr = args->CurrentPoint;
    PointerPointProperties^ pointProperties = pointPtr->Properties;
    PointerDeviceType type = pointPtr->PointerDevice->PointerDeviceType;
    // will be started on main thread
    if ((PointerDeviceType::Mouse == type) || (PointerDeviceType::Pen == type))
    {
        //update state before create dava event
        isLeftButtonPressed = pointProperties->IsLeftButtonPressed;
        isRightButtonPressed = pointProperties->IsRightButtonPressed;;
        isMiddleButtonPressed = pointProperties->IsMiddleButtonPressed;
    }

    float32 x = pointPtr->Position.X;
    float32 y = pointPtr->Position.Y;
    int32 id = pointPtr->PointerId;
    core->RunOnMainThread([this, x, y, id]() {
        DAVATouchEvent(UIEvent::PHASE_BEGAN, x, y, id);
    });
}

void WinUAPXamlApp::OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
    // will be started on main thread
    float32 x = args->CurrentPoint->Position.X;
    float32 y = args->CurrentPoint->Position.Y;
    int32 id = args->CurrentPoint->PointerId;

    auto fn = [this, x, y, id]() { DAVATouchEvent(UIEvent::PHASE_ENDED, x, y, id); };

    PointerDeviceType type = args->CurrentPoint->PointerDevice->PointerDeviceType;
    if ((PointerDeviceType::Mouse == type) || (PointerDeviceType::Pen == type))
    {
        if (isLeftButtonPressed || isMiddleButtonPressed || isRightButtonPressed)
        {
            core->RunOnMainThread(fn);

            PointerPointProperties^ pointProperties = args->CurrentPoint->Properties;
            // update state after create davaEvent
            isLeftButtonPressed = pointProperties->IsLeftButtonPressed;
            isRightButtonPressed = pointProperties->IsRightButtonPressed;
            isMiddleButtonPressed = pointProperties->IsMiddleButtonPressed;
        }
        else
        {
            DVASSERT(isLeftButtonPressed || isMiddleButtonPressed || isRightButtonPressed);
        }
    }
    else //  PointerDeviceType::Touch == args->CurrentPoint->PointerDevice->PointerDeviceType
    {
        core->RunOnMainThread(fn);
    }
}

void WinUAPXamlApp::OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
    // will be started on main thread
    float32 x = args->CurrentPoint->Position.X;
    float32 y = args->CurrentPoint->Position.Y;
    int32 id = args->CurrentPoint->PointerId;
    UIEvent::eInputPhase phase = UIEvent::PHASE_DRAG;

    PointerDeviceType type = args->CurrentPoint->PointerDevice->PointerDeviceType;
    if ((PointerDeviceType::Mouse == type) || (PointerDeviceType::Pen == type))
    {
        if (!(isLeftButtonPressed || isMiddleButtonPressed || isRightButtonPressed))
        {
            phase = UIEvent::PHASE_MOVE;
        }
    }

    core->RunOnMainThread([this, phase, x, y, id]() {
        DAVATouchEvent(phase, x, y, id);
    });
}

void WinUAPXamlApp::OnPointerEntered(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
    // will be started on main thread
    Logger::FrameworkDebug("[CorePlatformWinUAP] OnPointerEntered");
    PointerDeviceType type = args->CurrentPoint->PointerDevice->PointerDeviceType;
    if (PointerDeviceType::Mouse == type && isCursorPinning)
    {
        SetCursorVisible(false);
    }
}

void WinUAPXamlApp::OnPointerExited(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
    // will be started on main thread
    float32 x = args->CurrentPoint->Position.X;
    float32 y = args->CurrentPoint->Position.Y;
    int32 id = args->CurrentPoint->PointerId;

    Logger::FrameworkDebug("[CorePlatformWinUAP] OnPointerExited");
    PointerDeviceType type = args->CurrentPoint->PointerDevice->PointerDeviceType;
    if ((PointerDeviceType::Mouse == type) || PointerDeviceType::Pen == type)
    {
        if (isLeftButtonPressed || isMiddleButtonPressed || isRightButtonPressed)
        {
            core->RunOnMainThread([this, x, y, id]() {
                DAVATouchEvent(UIEvent::PHASE_ENDED, x, y, id);
            });
            PointerPointProperties^ pointProperties = args->CurrentPoint->Properties;
            // update state after create davaEvent
            isLeftButtonPressed = pointProperties->IsLeftButtonPressed;
            isRightButtonPressed = pointProperties->IsRightButtonPressed;
            isMiddleButtonPressed = pointProperties->IsMiddleButtonPressed;
        }
        SetCursorVisible(true);
    }
    else //  PointerDeviceType::Touch == type
    {
        core->RunOnMainThread([this, x, y, id]() {
            DAVATouchEvent(UIEvent::PHASE_DRAG, x, y, id);
        });
    }
}

void WinUAPXamlApp::OnPointerWheel(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
    // will be started on main thread
    Logger::FrameworkDebug("[CorePlatformWinUAP] OnPointerWheel");
    PointerPoint^ point = args->CurrentPoint;
    PointerPointProperties^ pointProperties = point->Properties;
    int32 wheelDelta = pointProperties->MouseWheelDelta;

    core->RunOnMainThread([this, wheelDelta]() {
        Vector<DAVA::UIEvent> touches;
        UIEvent newTouch;
        newTouch.tid = 0;
        newTouch.physPoint.x = 0;
        newTouch.physPoint.y = static_cast<float32>(wheelDelta / WHEEL_DELTA);
        newTouch.phase = UIEvent::PHASE_WHEEL;
        touches.push_back(newTouch);
        UIControlSystem::Instance()->OnInput(UIEvent::PHASE_WHEEL, touches, allTouches);
    });
}

void WinUAPXamlApp::OnHardwareBackButtonPressed(Platform::Object^ sender, Windows::Phone::UI::Input::BackPressedEventArgs ^args)
{
    core->RunOnMainThread([this]() {
        InputSystem::Instance()->GetKeyboard().OnKeyPressed(static_cast<int32>(DVKEY_BACK));
        UIEvent ev;
        ev.keyChar = 0;
        ev.tapCount = 1;
        ev.phase = UIEvent::PHASE_KEYCHAR;
        ev.tid = DVKEY_BACK;
        Vector<UIEvent> touches = { ev };
        UIControlSystem::Instance()->OnInput(0, touches, allTouches);
        touches.pop_back();
        UIControlSystem::Instance()->OnInput(0, touches, allTouches);
        InputSystem::Instance()->GetKeyboard().OnKeyUnpressed(static_cast<int32>(DVKEY_BACK));
    });
    args->Handled = true;
}

void WinUAPXamlApp::OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
{
    CoreWindow^ window = CoreWindow::GetForCurrentThread();
    CoreVirtualKeyStates menuStatus = window->GetKeyState(VirtualKey::Menu);
    CoreVirtualKeyStates tabStatus = window->GetKeyState(VirtualKey::Tab);
    bool isPressOrLock = static_cast<bool>((menuStatus & CoreVirtualKeyStates::Down) & (tabStatus & CoreVirtualKeyStates::Down));
    if (isPressOrLock)
    {
        __DAVAENGINE_WIN_UAP_INCOMPLETE_IMPLEMENTATION__
    }

    VirtualKey key = args->VirtualKey;
    // Note: should be propagated to main thread
    core->RunOnMainThread([this, key]() {
        UIEvent ev;
        ev.keyChar = 0;
        ev.tapCount = 1;
        ev.phase = UIEvent::PHASE_KEYCHAR;
        ev.tid = InputSystem::Instance()->GetKeyboard().GetDavaKeyForSystemKey(static_cast<int32>(key));

        Vector<UIEvent> touches = { ev };
        UIControlSystem::Instance()->OnInput(0, touches, allTouches);
        touches.pop_back();
        UIControlSystem::Instance()->OnInput(0, touches, allTouches);
        InputSystem::Instance()->GetKeyboard().OnSystemKeyPressed(static_cast<int32>(key));
    });

}

void WinUAPXamlApp::OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
{
    // Note: should be propagated to main thread
    VirtualKey key = args->VirtualKey;
    core->RunOnMainThread([this, key]() {
        InputSystem::Instance()->GetKeyboard().OnSystemKeyUnpressed(static_cast<int32>(key));
    });
}

void WinUAPXamlApp::OnMouseMoved(MouseDevice^ mouseDevice, MouseEventArgs^ args)
{
    // Note: must run on main thread
    if (!isCursorPinning || isMouseCursorShown)
    {
        return;
    }
    Point position(static_cast<float32>(args->MouseDelta.X), static_cast<float32>(args->MouseDelta.Y));
    int32 button = 0;
    if (isLeftButtonPressed)
    {
        button = 1;
    }
    else if (isRightButtonPressed)
    {
        button = 2;
    }
    else if (isMiddleButtonPressed)
    {
        button = 3;
    }

    float32 x = position.X;
    float32 y = position.Y;

    core->RunOnMainThread([this, x, y, button]() {
        if (isLeftButtonPressed || isMiddleButtonPressed || isRightButtonPressed)
        {
            DAVATouchEvent(UIEvent::PHASE_DRAG, x, y, button);
        }
        else
        {
            DAVATouchEvent(UIEvent::PHASE_MOVE, x, y, button);
        }
    });
}

void WinUAPXamlApp::DAVATouchEvent(UIEvent::eInputPhase phase, float32 x, float32 y, int32 id)
{
    Logger::FrameworkDebug("[CorePlatformWinUAP] DAVATouchEvent phase = %d, ID = %d, position.X = %f, position.Y = %f", phase, id, x, y);
    Vector<DAVA::UIEvent> touches;
    bool isFind = false;
    for (auto it = allTouches.begin(), end = allTouches.end(); it != end; ++it)
    {
        if (it->tid == id)
        {
            isFind = true;
            it->physPoint.x = x;
            it->physPoint.y = y;
            it->phase = phase;
            break;
        }
    }
    if (!isFind)
    {
        UIEvent newTouch;
        newTouch.tid = id;
        newTouch.physPoint.x = x;
        newTouch.physPoint.y = y;
        newTouch.phase = phase;
        allTouches.push_back(newTouch);
    }
    for (auto it = allTouches.begin(), end = allTouches.end(); it != end; ++it)
    {
        touches.push_back(*it);
    }
    if (phase == UIEvent::PHASE_ENDED)
    {
        for (Vector<DAVA::UIEvent>::iterator it = allTouches.begin(); it != allTouches.end(); ++it)
        {
            if (it->tid == id)
            {
                allTouches.erase(it);
                break;
            }
        }
    }
    UIControlSystem::Instance()->OnInput(phase, touches, allTouches);
}

void WinUAPXamlApp::SetupEventHandlers()
{
    Suspending += ref new SuspendingEventHandler(this, &WinUAPXamlApp::OnSuspending);
    Resuming += ref new EventHandler<::Platform::Object^>(this, &WinUAPXamlApp::OnResuming);

    CoreWindow^ coreWindow = Window::Current->CoreWindow;
    coreWindow->Activated += ref new TypedEventHandler<CoreWindow^, WindowActivatedEventArgs^>(this, &WinUAPXamlApp::OnWindowActivationChanged);
    coreWindow->SizeChanged += ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &WinUAPXamlApp::OnWindowSizeChanged);
    coreWindow->VisibilityChanged += ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &WinUAPXamlApp::OnWindowVisibilityChanged);

    coreWindow->PointerPressed += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &WinUAPXamlApp::OnPointerPressed);
    coreWindow->PointerMoved += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &WinUAPXamlApp::OnPointerMoved);
    coreWindow->PointerReleased += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &WinUAPXamlApp::OnPointerReleased);
    coreWindow->PointerEntered += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &WinUAPXamlApp::OnPointerEntered);
    coreWindow->PointerExited += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &WinUAPXamlApp::OnPointerExited);
    coreWindow->PointerWheelChanged += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &WinUAPXamlApp::OnPointerWheel);

    coreWindow->KeyDown += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &WinUAPXamlApp::OnKeyDown);
    coreWindow->KeyUp += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &WinUAPXamlApp::OnKeyUp);
    MouseDevice::GetForCurrentView()->MouseMoved += ref new TypedEventHandler<MouseDevice^, MouseEventArgs^>(this, &WinUAPXamlApp::OnMouseMoved);
    if (Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.Phone.UI.Input.HardwareButtons"))
    {
        HardwareButtons::BackPressed += ref new EventHandler<BackPressedEventArgs^>(this, &WinUAPXamlApp::OnHardwareBackButtonPressed);
        isPhoneApiDetected = true;
        Logger::FrameworkDebug("[CorePlatformWinUAP] Detected Phone Mode!");
    }
}

void WinUAPXamlApp::CreateBaseXamlUI()
{
    swapChainPanel = ref new Controls::SwapChainPanel();
    canvas = ref new Controls::Canvas();
    swapChainPanel->Children->Append(canvas);
    Window::Current->Content = swapChainPanel;

    // Windows UAP doesn't allow to unfocus UI control programmatically
    // It only permits to set focus at another control
    // So create dummy offscreen button that steals focus when there is
    // a need to unfocus native control, especially useful for text fields
    controlThatTakesFocus = ref new Button();
    controlThatTakesFocus->Content = L"I steal your focus";
    controlThatTakesFocus->Width = 30;
    controlThatTakesFocus->Height = 20;
    AddUIElement(controlThatTakesFocus);
    PositionUIElement(controlThatTakesFocus, -100, -100);
}

void WinUAPXamlApp::SetTitleName()
{
    // Note: must run on UI thread
    Logger::FrameworkDebug("[CorePlatformWinUAP] SetTitleName");
    KeyedArchive* options = Core::Instance()->GetOptions();
    if (nullptr != options)
    {
        WideString title = StringToWString(options->GetString("title", "[set application title using core options property 'title']"));
        ApplicationView::GetForCurrentView()->Title = ref new ::Platform::String(title.c_str());
    }
}

void WinUAPXamlApp::SetDisplayOrientations()
{
    // Note: must run on UI thread
    Logger::FrameworkDebug("[CorePlatformWinUAP] SetDisplayOrientations");
    Core::eScreenOrientation orientMode = Core::Instance()->GetScreenOrientation();
    switch (orientMode)
    {
    case DAVA::Core::SCREEN_ORIENTATION_TEXTURE:
        break;
    case DAVA::Core::SCREEN_ORIENTATION_LANDSCAPE_RIGHT:
        displayOrientation = DisplayOrientations::Landscape;
        break;
    case DAVA::Core::SCREEN_ORIENTATION_LANDSCAPE_LEFT:
        displayOrientation = DisplayOrientations::LandscapeFlipped;
        break;
    case DAVA::Core::SCREEN_ORIENTATION_PORTRAIT:
        displayOrientation = DisplayOrientations::Portrait;
        break;
    case DAVA::Core::SCREEN_ORIENTATION_PORTRAIT_UPSIDE_DOWN:
        displayOrientation = DisplayOrientations::PortraitFlipped;
        break;
    case DAVA::Core::SCREEN_ORIENTATION_LANDSCAPE_AUTOROTATE:
        displayOrientation = DisplayOrientations::Landscape | DisplayOrientations::LandscapeFlipped;
        break;
    case DAVA::Core::SCREEN_ORIENTATION_PORTRAIT_AUTOROTATE:
        displayOrientation = DisplayOrientations::Portrait | DisplayOrientations::PortraitFlipped;
        break;
    }
    DisplayInformation::GetForCurrentView()->AutoRotationPreferences = displayOrientation;
}

void WinUAPXamlApp::InitInput()
{
    // Detect touch
    Logger::FrameworkDebug("[CorePlatformWinUAP] InitInput");
    TouchCapabilities^ touchCapabilities = ref new TouchCapabilities();
    isTouchDetected = (1 == touchCapabilities->TouchPresent);   // Touch is always present in MSVS simulator

    // Detect mouse
    MouseCapabilities^ mouseCapabilities = ref new MouseCapabilities();
    isMouseDetected = (1 == mouseCapabilities->MousePresent);
}

void WinUAPXamlApp::InitRender()
{
#if RHI_COMPLETE_WIN10
    Logger::FrameworkDebug("[CorePlatformWinUAP] InitRender");
    RenderManager::Create(Core::RENDERER_OPENGL_ES_2_0);
    RenderManager::Instance()->Create(swapChainPanel);
#endif RHI_COMPLETE_WIN10
}

void WinUAPXamlApp::ReInitRender()
{
    Logger::FrameworkDebug("[CorePlatformWinUAP] ReInitRender");
	rhi::ResetParam params;
	params.width = static_cast<int32>(windowWidth);
	params.height = static_cast<int32>(windowHeight);
	Renderer::Reset(params);
    //RenderSystem2D::Instance()->Init(); //RHI_COMPLETE
}

void WinUAPXamlApp::InitCoordinatesSystem()
{
    Logger::FrameworkDebug("[CorePlatformWinUAP] InitCoordinatesSystem");
    VirtualCoordinatesSystem::Instance()->SetInputScreenAreaSize(static_cast<int32>(windowWidth), static_cast<int32>(windowHeight));
    VirtualCoordinatesSystem::Instance()->SetPhysicalScreenSize(static_cast<int32>(windowWidth), static_cast<int32>(windowHeight));
    VirtualCoordinatesSystem::Instance()->EnableReloadResourceOnResize(true);
}

void WinUAPXamlApp::ReInitCoordinatesSystem()
{
    Logger::FrameworkDebug("[CorePlatformWinUAP] ReInitCoordinatesSystem");
    VirtualCoordinatesSystem::Instance()->SetInputScreenAreaSize(static_cast<int32>(windowWidth), static_cast<int32>(windowHeight));
    VirtualCoordinatesSystem::Instance()->UnregisterAllAvailableResourceSizes();
    VirtualCoordinatesSystem::Instance()->RegisterAvailableResourceSize(static_cast<int32>(windowWidth), static_cast<int32>(windowHeight), "Gfx");
    VirtualCoordinatesSystem::Instance()->SetPhysicalScreenSize(static_cast<int32>(windowWidth), static_cast<int32>(windowHeight));
    VirtualCoordinatesSystem::Instance()->SetVirtualScreenSize(static_cast<int32>(windowWidth), static_cast<int32>(windowHeight));
    VirtualCoordinatesSystem::Instance()->ScreenSizeChanged();
}

void WinUAPXamlApp::PrepareScreenSize()
{
    // Note: must run on UI thread
    KeyedArchive* options = Core::Instance()->GetOptions();
    if (nullptr != options)
    {
        windowedMode.width = options->GetInt32("width", DisplayMode::DEFAULT_WIDTH);
        windowedMode.height = options->GetInt32("height", DisplayMode::DEFAULT_HEIGHT);
        windowedMode.bpp = options->GetInt32("bpp", DisplayMode::DEFAULT_BITS_PER_PIXEL);

        fullscreenMode.width = options->GetInt32("fullscreen.width", fullscreenMode.width);
        fullscreenMode.height = options->GetInt32("fullscreen.height", fullscreenMode.height);
        fullscreenMode.bpp = windowedMode.bpp;
        isFullscreen = (0 != options->GetInt32("fullscreen", 0));
    }
    Logger::FrameworkDebug("[PlatformWin32] best display fullscreen mode matched: %d x %d x %d refreshRate: %d",
                           fullscreenMode.width, fullscreenMode.height, fullscreenMode.bpp, fullscreenMode.refreshRate);
    SetFullScreen(isFullscreen);
    if (isFullscreen)
    {
        currentMode = fullscreenMode;
    }
    else
    {
        currentMode = windowedMode;
        SetPreferredSize(windowedMode.width, windowedMode.height);
    }
}

void WinUAPXamlApp::UpdateScreenSize(float32 width, float32 height)
{
    windowWidth = width;
    windowHeight = height;
    Logger::FrameworkDebug("[CorePlatformWinUAP] UpdateScreenSize windowWidth = %f, windowHeight = %f.", windowWidth, windowHeight);
}

void WinUAPXamlApp::SetFullScreen(bool isFullscreen_)
{
    // Note: must run on UI thread
    Logger::FrameworkDebug("[CorePlatformWinUAP] SetFullScreen %d", (int32)isFullscreen_);
    if (isPhoneApiDetected)
    {
        return;
    }
    ApplicationView^ view = ApplicationView::GetForCurrentView();
    bool isFull = view->IsFullScreenMode;
    if (isFull == isFullscreen_)
    {
        return;
    }
    if (isFullscreen_)
    {
        isFullscreen = view->TryEnterFullScreenMode();
    }
    else
    {
        view->ExitFullScreenMode();
        isFullscreen = false;
    }
}

void WinUAPXamlApp::SetPreferredSize(int32 width, int32 height)
{
    // Note: must run on UI thread
    Logger::FrameworkDebug("[CorePlatformWinUAP] SetPreferredSize width = %d, height = %d", width, height);
    if (isPhoneApiDetected)
    {
        return;
    }
    // MSDN::This property only has an effect when the app is launched on a desktop device that is not in tablet mode.
    ApplicationView::GetForCurrentView()->PreferredLaunchViewSize = Windows::Foundation::Size(static_cast<float32>(width), static_cast<float32>(height));
}

}   // namespace DAVA

#endif  // __DAVAENGINE_WIN_UAP__
