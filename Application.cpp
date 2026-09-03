#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"
#include "LineRenderer.h"

//윈도우상수
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_INPUT:
	{
		UINT size = 0;

		const UINT queryResult = GetRawInputData(
			reinterpret_cast<HRAWINPUT>(lparam),
			RID_INPUT,
			nullptr,
			&size,
			sizeof(RAWINPUTHEADER));

		if (queryResult == 0 && size > 0)
		{
			std::vector<BYTE> buffer(size);

			const UINT copiedSize = GetRawInputData(
				reinterpret_cast<HRAWINPUT>(lparam),
				RID_INPUT,
				buffer.data(),
				&size,
				sizeof(RAWINPUTHEADER));

			if (copiedSize == size)
			{
				const RAWINPUT* input =
					reinterpret_cast<const RAWINPUT*>(buffer.data());

				if (input->header.dwType == RIM_TYPEMOUSE)
				{
					const RAWMOUSE& mouse = input->data.mouse;

					// FPS 카메라에서는 상대 좌표 입력만 사용
					if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
					{
						Application::Instance().AddMouseDelta(
							static_cast<float>(mouse.lLastX),
							static_cast<float>(mouse.lLastY));
					}
				}
			}
		}

		// Raw Input 시스템 정리
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

Application& Application::Instance()
{
    static Application instance;
    return instance;
}

bool Application::Init()
{
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	CreateGameWindow(_hwnd, _windowClass);

	RAWINPUTDEVICE mouse = {};
	mouse.usUsagePage = 0x01;
	mouse.usUsage = 0x02;
	mouse.dwFlags = 0;
	mouse.hwndTarget = _hwnd;

	if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse))) {
		return false;
	}

	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));
	_pmdActor.reset(new PMDActor(L"model/cylinder_512.pmd", *_pmdRenderer));
	//_pmdActor->LoadVMDFile(L"motion/pose.vmd", "pose");

	_lineRenderer.reset(new LineRenderer(*_dx12));

	return true;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW);
	float angle = 0.0f;
	MSG msg = {};
	unsigned int frame = 0;

	while (true)
	{
		unsigned long currentTime = ::timeGetTime();
		float deltaTime = (float)(currentTime - _lastTime) / 1000.0f;
		_lastTime = currentTime;

		_camera.Move(deltaTime);

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				return;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//전체의렌더링준비
		_dx12->BeginDraw();

		//PMD용의렌더링파이프라인에 맞춘다
		_dx12->CommandList()->SetPipelineState(_pmdRenderer->GetPipelineState());
		//루트 시그니처도 PMD용에 맞춘다
		_dx12->CommandList()->SetGraphicsRootSignature(_pmdRenderer->GetRootSignature());

		_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_dx12->SetScene(_camera.View(), _camera.position);
		_dx12->BindScene();

		_pmdActor->Update();
		_pmdActor->Draw();

		_lineRenderer->Draw();

		_dx12->EndDraw();

		//프레젠트
		_dx12->Swapchain()->Present(1, 0);
	}
}

void Application::Terminate()
{
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

SIZE Application::GetWindowSize()
{
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

void Application::AddMouseDelta(float dx, float dy)
{
	_camera.AddMouseDelta(dx, dy);
}

Application::Application()
{
}

Application::~Application()
{
}

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;
	windowClass.lpszClassName = _T("DX12Test");
	windowClass.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&windowClass);

	RECT wrc = { 0, 0, window_width, window_height };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	_hwnd = CreateWindow(windowClass.lpszClassName,
		_T("DX12 Test"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		windowClass.hInstance,
		nullptr);
}
