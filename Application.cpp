#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"
#include "Plane.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cwchar>

namespace
{
	constexpr float enemyScale = 0.55f;
	constexpr float enemyHitRadius = 1.25f;
	constexpr float enemyModelCenterY = 15.0f;
	HWND gCrosshairHud = nullptr;

	DirectX::XMFLOAT3 GetEnemyRenderPosition(
		const DirectX::XMFLOAT3& logicalPosition)
	{
		DirectX::XMFLOAT3 renderPosition = logicalPosition;
		renderPosition.y -= enemyModelCenterY * enemyScale;
		return renderPosition;
	}

	void DebugLog(const char* format, ...)
	{
#ifdef _DEBUG
		char message[512] = {};
		va_list arguments;
		va_start(arguments, format);
		vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
		va_end(arguments);

		OutputDebugStringA(message);
		std::fputs(message, stdout);
		std::fflush(stdout);
#else
		(void)format;
#endif
	}
}

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

	case WM_CTLCOLORSTATIC:
		SetTextColor(reinterpret_cast<HDC>(wparam),
			reinterpret_cast<HWND>(lparam) == gCrosshairHud
				? RGB(20, 20, 20)
				: RGB(255, 245, 210));
		SetBkMode(reinterpret_cast<HDC>(wparam), TRANSPARENT);
		return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
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
	DebugLog("[INIT] ShooterGame initialized. enemies=%zu arenaPlanes=%zu\n",
		_enemies.size(), _arenaPlanes.size());

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
	DebugLog("[GAME] RESET hp=%d time=%.1f camera=(%.2f, %.2f, %.2f)\n",
		_health, _timeRemaining,
		_camera.position.x, _camera.position.y, _camera.position.z);
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
	const auto renderPosition = GetEnemyRenderPosition(slot->position);
	slot->actor->SetTransform(renderPosition, enemyScale, 0.0f);
	const float visibleCenterY =
		renderPosition.y + enemyModelCenterY * enemyScale;
	DebugLog(
		"[SPAWN] enemy=%zu logicalCenter=(%.2f, %.2f, %.2f) "
		"renderPivotY=%.2f visibleCenterY=%.2f centerDeltaY=%.2f speed=%.2f\n",
		static_cast<size_t>(std::distance(_enemies.begin(), slot)),
		slot->position.x, slot->position.y, slot->position.z,
		renderPosition.y, visibleCenterY,
		visibleCenterY - slot->position.y, slot->speed);
}

void Application::Shoot()
{
	const auto forward = _camera.Forward();
	float nearestDistance = 1000.0f;
	float nearestRayDistance = 1000.0f;
	float nearestRayAlong = -1.0f;
	float nearestVisibleRayDistance = 1000.0f;
	float nearestVisibleRayAlong = -1.0f;
	float nearestLogicalCenterY = 0.0f;
	float nearestVisibleCenterY = 0.0f;
	size_t nearestEnemyIndex = static_cast<size_t>(-1);
	Enemy* hitEnemy = nullptr;
	int activeEnemyCount = 0;

	DebugLog("[SHOT] eye=(%.2f, %.2f, %.2f) dir=(%.3f, %.3f, %.3f)\n",
		_camera.position.x, _camera.position.y, _camera.position.z,
		forward.x, forward.y, forward.z);

	for (size_t enemyIndex = 0; enemyIndex < _enemies.size(); ++enemyIndex)
	{
		auto& enemy = _enemies[enemyIndex];
		if (!enemy.active) continue;
		++activeEnemyCount;
		const float x = enemy.position.x - _camera.position.x;
		const float y = enemy.position.y - _camera.position.y;
		const float z = enemy.position.z - _camera.position.z;
		const float alongRay = x * forward.x + y * forward.y + z * forward.z;
		if (alongRay <= 0.0f) continue;

		const float distanceSquared = x * x + y * y + z * z;
		const float closestSquared = (std::max)(
			0.0f, distanceSquared - alongRay * alongRay);
		const float rayDistance = std::sqrt(closestSquared);
		if (rayDistance < nearestRayDistance)
		{
			nearestRayDistance = rayDistance;
			nearestRayAlong = alongRay;
			nearestLogicalCenterY = enemy.position.y;
			nearestEnemyIndex = enemyIndex;
		}

		const float visibleCenterY =
			GetEnemyRenderPosition(enemy.position).y +
			enemyModelCenterY * enemyScale;
		const float visibleY = visibleCenterY - _camera.position.y;
		const float visibleAlongRay =
			x * forward.x + visibleY * forward.y + z * forward.z;
		if (visibleAlongRay > 0.0f)
		{
			const float visibleDistanceSquared =
				x * x + visibleY * visibleY + z * z;
			const float visibleClosestSquared = (std::max)(
				0.0f,
				visibleDistanceSquared - visibleAlongRay * visibleAlongRay);
			const float visibleRayDistance = std::sqrt(visibleClosestSquared);
			if (visibleRayDistance < nearestVisibleRayDistance)
			{
				nearestVisibleRayDistance = visibleRayDistance;
				nearestVisibleRayAlong = visibleAlongRay;
				nearestVisibleCenterY = visibleCenterY;
			}
		}

		if (alongRay < nearestDistance &&
			closestSquared <= enemyHitRadius * enemyHitRadius)
		{
			nearestDistance = alongRay;
			hitEnemy = &enemy;
		}
	}

	if (hitEnemy)
	{
		const size_t enemyIndex = static_cast<size_t>(hitEnemy - _enemies.data());
		--hitEnemy->health;
		if (hitEnemy->health <= 0)
		{
			hitEnemy->active = false;
			hitEnemy->actor->SetVisible(false);
			_score += 100;
			DebugLog("[HIT] enemy=%zu distance=%.2f score=%d\n",
				enemyIndex, nearestDistance, _score);
		}
	}
	else
	{
		DebugLog(
			"[MISS] active=%d nearestEnemy=%zu logicalY=%.2f "
			"logicalRayDistance=%.2f logicalAlong=%.2f visibleY=%.2f "
			"visibleRayDistance=%.2f visibleAlong=%.2f hitRadius=%.2f\n",
			activeEnemyCount, nearestEnemyIndex, nearestLogicalCenterY,
			nearestRayDistance, nearestRayAlong, nearestVisibleCenterY,
			nearestVisibleRayDistance, nearestVisibleRayAlong, enemyHitRadius);
	}
}

void Application::UpdateGame(float deltaTime)
{
	static bool restartWasDown = false;
	static bool fireWasDown = false;
	const bool restartDown = (GetAsyncKeyState('R') & 0x8000) != 0;
	const bool fireDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	if (_gameState != GameState::Playing)
	{
		if (fireDown && !fireWasDown)
		{
			DebugLog("[SHOT_IGNORED] gameState=%s\n",
				_gameState == GameState::Won ? "Won" : "Lost");
		}
		if (restartDown && !restartWasDown) ResetGame();
		restartWasDown = restartDown;
		fireWasDown = fireDown;
		return;
	}
	restartWasDown = restartDown;
	if (fireDown && !fireWasDown)
	{
		DebugLog("[INPUT] LMB_DOWN gameState=Playing shotCooldown=%.3f\n",
			(std::max)(0.0f, _shotCooldown));
	}
	fireWasDown = fireDown;

	_timeRemaining -= deltaTime;
	_spawnCooldown -= deltaTime;
	_shotCooldown -= deltaTime;
	_damageCooldown -= deltaTime;

	if (_timeRemaining <= 0.0f)
	{
		_timeRemaining = 0.0f;
		_gameState = GameState::Won;
		DebugLog("[GAME] WON score=%d hp=%d\n", _score, _health);
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

	if (fireDown && _shotCooldown <= 0.0f)
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
			DebugLog("[DAMAGE] hp=%d enemyDistance=%.2f\n", _health, distance);
		}

		const float yaw = std::atan2(dx, dz);
		enemy.actor->SetTransform(
			GetEnemyRenderPosition(enemy.position), enemyScale, yaw);
	}

	if (_health <= 0)
	{
		_health = 0;
		_gameState = GameState::Lost;
		DebugLog("[GAME] LOST score=%d timeRemaining=%.1f\n",
			_score, _timeRemaining);
	}

#ifdef _DEBUG
	static float debugStatusTimer = 0.0f;
	debugStatusTimer -= deltaTime;
	if (debugStatusTimer <= 0.0f)
	{
		const int activeCount = static_cast<int>(std::count_if(
			_enemies.begin(), _enemies.end(),
			[](const Enemy& enemy) { return enemy.active; }));
		DebugLog("[STATE] hp=%d score=%d time=%.1f activeEnemies=%d shotCooldown=%.2f\n",
			_health, _score, _timeRemaining, activeCount,
			(std::max)(0.0f, _shotCooldown));
		debugStatusTimer = 1.0f;
	}
#endif
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
	UpdateHud();
}

void Application::CreateHud()
{
	_statusHudFont = CreateFontW(
		-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		FIXED_PITCH | FF_MODERN, L"Consolas");
	_resultHudFont = CreateFontW(
		-40, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	_crosshairHudFont = CreateFontW(
		-32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		FIXED_PITCH | FF_MODERN, L"Consolas");

	_statusHud = CreateWindowExW(
		WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
		L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
		20, 18, 760, 32, _hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
	_resultHud = CreateWindowExW(
		WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
		L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
		100, 210, 1080, 210, _hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
	_crosshairHud = CreateWindowExW(
		WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
		L"STATIC", L"+", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
		620, 338, 40, 44, _hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
	gCrosshairHud = _crosshairHud;

	if (_statusHud && _statusHudFont)
	{
		SendMessageW(_statusHud, WM_SETFONT,
			reinterpret_cast<WPARAM>(_statusHudFont), TRUE);
	}
	if (_resultHud && _resultHudFont)
	{
		SendMessageW(_resultHud, WM_SETFONT,
			reinterpret_cast<WPARAM>(_resultHudFont), TRUE);
	}
	if (_crosshairHud && _crosshairHudFont)
	{
		SendMessageW(_crosshairHud, WM_SETFONT,
			reinterpret_cast<WPARAM>(_crosshairHudFont), TRUE);
	}
}

void Application::UpdateHud()
{
	if (!_statusHud || !_resultHud) return;

	wchar_t statusText[128] = {};
	wchar_t resultText[160] = {};
	if (_gameState == GameState::Playing)
	{
		std::swprintf(statusText, 128,
			L"HP %03d   SCORE %05d   TIME %02d   |   LMB FIRE   R RESTART",
			_health, _score, static_cast<int>(std::ceil(_timeRemaining)));
	}
	else if (_gameState == GameState::Won)
	{
		std::swprintf(statusText, 128, L"HP %03d   SCORE %05d", _health, _score);
		std::swprintf(resultText, 160,
			L"YOU SURVIVED!\r\nSCORE %d\r\nPRESS R TO RESTART", _score);
	}
	else
	{
		std::swprintf(statusText, 128, L"SCORE %05d", _score);
		std::swprintf(resultText, 160,
			L"GAME OVER\r\nSCORE %d\r\nPRESS R TO RESTART", _score);
	}

	SetWindowTextW(_statusHud, statusText);
	SetWindowTextW(_resultHud, resultText);
	InvalidateRect(_statusHud, nullptr, TRUE);
	InvalidateRect(_resultHud, nullptr, TRUE);
}

void Application::DestroyHud()
{
	if (_statusHud)
	{
		DestroyWindow(_statusHud);
		_statusHud = nullptr;
	}
	if (_resultHud)
	{
		DestroyWindow(_resultHud);
		_resultHud = nullptr;
	}
	if (_crosshairHud)
	{
		DestroyWindow(_crosshairHud);
		_crosshairHud = nullptr;
		gCrosshairHud = nullptr;
	}
	if (_statusHudFont)
	{
		DeleteObject(_statusHudFont);
		_statusHudFont = nullptr;
	}
	if (_resultHudFont)
	{
		DeleteObject(_resultHudFont);
		_resultHudFont = nullptr;
	}
	if (_crosshairHudFont)
	{
		DeleteObject(_crosshairHudFont);
		_crosshairHudFont = nullptr;
	}
}

void Application::Terminate()
{
	DestroyHud();
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
	CreateHud();
}
