#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"
#include "Plane.h"

#include <cmath>
#include <cwchar>

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
	CreateArena();

	constexpr int enemyPoolSize = 12;
	_enemies.reserve(enemyPoolSize);
	for (int i = 0; i < enemyPoolSize; ++i)
	{
		Enemy enemy;
		enemy.actor = std::make_shared<PMDActor>(
			L"model/cylinder_512.pmd", *_pmdRenderer);
		enemy.actor->SetVisible(false);
		_enemies.push_back(std::move(enemy));
	}
	ResetGame();

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
		if (deltaTime > 0.1f) deltaTime = 0.1f;

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				return;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (GetForegroundWindow() == _hwnd)
		{
			if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
			{
				PostMessage(_hwnd, WM_CLOSE, 0, 0);
			}
			_camera.Move(deltaTime);
			UpdateGame(deltaTime);

			RECT clientRect = {};
			GetClientRect(_hwnd, &clientRect);
			POINT center = {
				(clientRect.right - clientRect.left) / 2,
				(clientRect.bottom - clientRect.top) / 2
			};
			ClientToScreen(_hwnd, &center);
			SetCursorPos(center.x, center.y);
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

		for (const auto& plane : _arenaPlanes)
		{
			plane->Draw();
		}

		for (auto& enemy : _enemies)
		{
			if (!enemy.active) continue;
			enemy.actor->Update();
			enemy.actor->Draw();
		}

		_dx12->EndDraw();

		//프레젠트
		_dx12->Swapchain()->Present(1, 0);
	}
}

void Application::CreateArena()
{
	constexpr float halfSize = 20.0f;
	constexpr float halfHeight = 4.0f;
	const DirectX::XMFLOAT4 floorColor = { 0.22f, 0.27f, 0.31f, 1.0f };
	const DirectX::XMFLOAT4 wallColor = { 0.44f, 0.18f, 0.13f, 1.0f };

	_arenaPlanes.emplace_back(std::make_unique<Plane>(
		*_dx12, *_pmdRenderer,
		DirectX::XMFLOAT3{ 0, 0, 0 },
		DirectX::XMFLOAT3{ halfSize, 0, 0 },
		DirectX::XMFLOAT3{ 0, 0, -halfSize }, floorColor));

	_arenaPlanes.emplace_back(std::make_unique<Plane>(
		*_dx12, *_pmdRenderer,
		DirectX::XMFLOAT3{ 0, halfHeight, halfSize },
		DirectX::XMFLOAT3{ -halfSize, 0, 0 },
		DirectX::XMFLOAT3{ 0, halfHeight, 0 }, wallColor));
	_arenaPlanes.emplace_back(std::make_unique<Plane>(
		*_dx12, *_pmdRenderer,
		DirectX::XMFLOAT3{ 0, halfHeight, -halfSize },
		DirectX::XMFLOAT3{ halfSize, 0, 0 },
		DirectX::XMFLOAT3{ 0, halfHeight, 0 }, wallColor));
	_arenaPlanes.emplace_back(std::make_unique<Plane>(
		*_dx12, *_pmdRenderer,
		DirectX::XMFLOAT3{ halfSize, halfHeight, 0 },
		DirectX::XMFLOAT3{ 0, 0, halfSize },
		DirectX::XMFLOAT3{ 0, halfHeight, 0 }, wallColor));
	_arenaPlanes.emplace_back(std::make_unique<Plane>(
		*_dx12, *_pmdRenderer,
		DirectX::XMFLOAT3{ -halfSize, halfHeight, 0 },
		DirectX::XMFLOAT3{ 0, 0, -halfSize },
		DirectX::XMFLOAT3{ 0, halfHeight, 0 }, wallColor));
}

void Application::ResetGame()
{
	_gameState = GameState::Playing;
	_timeRemaining = 90.0f;
	_spawnCooldown = 0.0f;
	_shotCooldown = 0.0f;
	_damageCooldown = 0.0f;
	_health = 100;
	_score = 0;
	_camera.position = { 0.0f, 1.7f, -8.0f };
	_camera.yaw = 0.0f;
	_camera.pitch = 0.0f;

	for (auto& enemy : _enemies)
	{
		enemy.active = false;
		enemy.actor->SetVisible(false);
	}
	SpawnEnemy();
	UpdateWindowTitle();
}

void Application::SpawnEnemy()
{
	auto slot = std::find_if(_enemies.begin(), _enemies.end(),
		[](const Enemy& enemy) { return !enemy.active; });
	if (slot == _enemies.end()) return;

	_randomState = _randomState * 1664525u + 1013904223u;
	const int side = static_cast<int>(_randomState & 3u);
	_randomState = _randomState * 1664525u + 1013904223u;
	const float offset =
		(static_cast<float>((_randomState >> 8) % 3200u) / 100.0f) - 16.0f;

	switch (side)
	{
	case 0: slot->position = { -18.0f, 1.0f, offset }; break;
	case 1: slot->position = { 18.0f, 1.0f, offset }; break;
	case 2: slot->position = { offset, 1.0f, -18.0f }; break;
	default: slot->position = { offset, 1.0f, 18.0f }; break;
	}
	slot->active = true;
	slot->health = 1;
	slot->speed = 1.25f + (90.0f - _timeRemaining) * 0.01f;
	slot->actor->SetVisible(true);
	slot->actor->SetTransform(slot->position, 0.55f, 0.0f);
}

void Application::Shoot()
{
	const auto forward = _camera.Forward();
	float nearestDistance = 1000.0f;
	Enemy* hitEnemy = nullptr;

	for (auto& enemy : _enemies)
	{
		if (!enemy.active) continue;
		const float x = enemy.position.x - _camera.position.x;
		const float y = enemy.position.y - _camera.position.y;
		const float z = enemy.position.z - _camera.position.z;
		const float alongRay = x * forward.x + y * forward.y + z * forward.z;
		if (alongRay <= 0.0f || alongRay >= nearestDistance) continue;

		const float distanceSquared = x * x + y * y + z * z;
		const float closestSquared = distanceSquared - alongRay * alongRay;
		if (closestSquared <= 1.25f * 1.25f)
		{
			nearestDistance = alongRay;
			hitEnemy = &enemy;
		}
	}

	if (hitEnemy)
	{
		--hitEnemy->health;
		if (hitEnemy->health <= 0)
		{
			hitEnemy->active = false;
			hitEnemy->actor->SetVisible(false);
			_score += 100;
		}
	}
}

void Application::UpdateGame(float deltaTime)
{
	static bool restartWasDown = false;
	const bool restartDown = (GetAsyncKeyState('R') & 0x8000) != 0;
	if (_gameState != GameState::Playing)
	{
		if (restartDown && !restartWasDown) ResetGame();
		restartWasDown = restartDown;
		return;
	}
	restartWasDown = restartDown;

	_timeRemaining -= deltaTime;
	_spawnCooldown -= deltaTime;
	_shotCooldown -= deltaTime;
	_damageCooldown -= deltaTime;

	if (_timeRemaining <= 0.0f)
	{
		_timeRemaining = 0.0f;
		_gameState = GameState::Won;
		UpdateWindowTitle();
		return;
	}

	const float elapsed = 90.0f - _timeRemaining;
	const float spawnInterval = (std::max)(1.0f, 3.2f - elapsed * 0.02f);
	if (_spawnCooldown <= 0.0f)
	{
		SpawnEnemy();
		_spawnCooldown = spawnInterval;
	}

	if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && _shotCooldown <= 0.0f)
	{
		Shoot();
		_shotCooldown = 0.18f;
	}

	for (auto& enemy : _enemies)
	{
		if (!enemy.active) continue;
		const float dx = _camera.position.x - enemy.position.x;
		const float dz = _camera.position.z - enemy.position.z;
		const float distance = std::sqrt(dx * dx + dz * dz);

		if (distance > 1.2f)
		{
			enemy.position.x += dx / distance * enemy.speed * deltaTime;
			enemy.position.z += dz / distance * enemy.speed * deltaTime;
		}
		else if (_damageCooldown <= 0.0f)
		{
			_health -= 8;
			_damageCooldown = 0.9f;
		}

		const float yaw = std::atan2(dx, dz);
		enemy.actor->SetTransform(enemy.position, 0.55f, yaw);
	}

	if (_health <= 0)
	{
		_health = 0;
		_gameState = GameState::Lost;
	}
	UpdateWindowTitle();
}

void Application::UpdateWindowTitle()
{
	wchar_t title[160] = {};
	if (_gameState == GameState::Playing)
	{
		std::swprintf(title, 160,
			L"RATION RUN  |  HP %d  |  SCORE %d  |  TIME %02d  |  WASD + MOUSE / LMB",
			_health, _score, static_cast<int>(std::ceil(_timeRemaining)));
	}
	else if (_gameState == GameState::Won)
	{
		std::swprintf(title, 160,
			L"YOU SURVIVED!  SCORE %d  |  Press R to restart", _score);
	}
	else
	{
		std::swprintf(title, 160,
			L"GAME OVER  SCORE %d  |  Press R to restart", _score);
	}
	SetWindowTextW(_hwnd, title);
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
	windowClass.hCursor = LoadCursor(nullptr, IDC_CROSS);

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
