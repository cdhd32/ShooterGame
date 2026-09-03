#pragma once
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>
#include<wrl.h>
#include <algorithm>

#pragma comment(lib, "Winmm.lib")

class Dx12Wrapper;
class PMDRenderer;
class PMDActor;
class LineRenderer;

class Application
{
private:
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	std::shared_ptr<Dx12Wrapper> _dx12;
	std::shared_ptr<PMDRenderer> _pmdRenderer;
	std::shared_ptr<PMDActor> _pmdActor;
	std::shared_ptr<LineRenderer> _lineRenderer;

	unsigned long _lastTime = ::timeGetTime();

	struct FpsCamera
	{
		DirectX::XMFLOAT3 position = { 0.0f, 15.0f, -8.0f };
		float yaw = 0.0f;
		float pitch = 0.0f;

		float dt = 0.0f;

		void Move(float dt)
		{
			using namespace DirectX;

			const float speed = 5.0f;
			XMVECTOR forward = XMVectorSet(std::sinf(yaw), 0.0f, std::cosf(yaw), 0.0f);
			XMVECTOR right = XMVectorSet(std::cosf(yaw), 0.0f, -std::sinf(yaw), 0.0f);
			XMVECTOR pos = XMLoadFloat3(&position);

			if (GetAsyncKeyState('W') & 0x8000)
				pos = XMVectorAdd(pos, XMVectorScale(forward, speed * dt));
			if (GetAsyncKeyState('S') & 0x8000)
				pos = XMVectorAdd(pos, XMVectorScale(forward, -speed * dt));
			if (GetAsyncKeyState('D') & 0x8000)
				pos = XMVectorAdd(pos, XMVectorScale(right, speed * dt));
			if (GetAsyncKeyState('A') & 0x8000)
				pos = XMVectorAdd(pos, XMVectorScale(right, -speed * dt));

			XMStoreFloat3(&position, pos);
		}

		void AddMouseDelta(float dx, float dy)
		{
			constexpr float sensitivity = 0.002f;

			yaw += dx * sensitivity;
			pitch -= dy * sensitivity;
			pitch = std::clamp<float>(pitch, -1.45f, 1.45f);
		}

		DirectX::XMMATRIX View() const
		{
			using namespace DirectX;

			XMVECTOR eye = XMLoadFloat3(&position);
			XMVECTOR forward = XMVector3Normalize(XMVectorSet(
				std::cosf(pitch) * std::sinf(yaw),
				std::sinf(pitch),
				std::cosf(pitch) * std::cosf(yaw),
				0.0f));

			return DirectX::XMMatrixLookToLH(
				eye,
				forward,
				DirectX::XMVectorSet(0, 1, 0, 0));
		}
	};

	FpsCamera _camera;

private:
	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);
public:
	static Application& Instance();

	bool Init();

	void Run();

	void Terminate();
	SIZE GetWindowSize();
	void AddMouseDelta(float dx, float dy);
	~Application();
};

