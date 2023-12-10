#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include "offsets.h"
#include "Kernterface.h"
#include <thread>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param))
	{
		return 0L;
	}

	if (message == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0L;
	}

	return DefWindowProc(window, message, w_param, l_param);
}

struct Vector {
	Vector() noexcept : x(), y(), z() {}

	Vector(float x, float y, float z) noexcept : x(x), y(y), z(z) {}

	Vector& operator+(const Vector& v) noexcept
	{
		x += v.x;
		y += v.y;
		z += v.z;

		return *this;
	}

	Vector& operator-(const Vector& v) noexcept
	{
		x -= v.x;
		y -= v.y;
		z -= v.z;

		return *this;
	}

	float x, y, z;
};

struct ViewMatrix {
	ViewMatrix() noexcept : data() {}

	float* operator[](int index) noexcept
	{
		return data[index];
	}

	const float* operator[](int index) const noexcept
	{
		return data[index];
	}

	float data[4][4];
};

static bool world_to_screen(const Vector& world, Vector& screen, const ViewMatrix& vm) noexcept {
	float w = vm[3][0] * world.x + vm[3][1] * world.y + vm[3][2] * world.z + vm[3][3];

	if (w < 0.001f) {
		return false;
	}

	const float x = world.x * vm[0][0] + world.y * vm[0][1] + world.z * vm[0][2] + vm[0][3];
	const float y = world.x * vm[1][0] + world.y * vm[1][1] + world.z * vm[1][2] + vm[1][3];

	w = 1.f / w;
	float nx = x * w;
	float ny = y * w;

	const ImVec2 size = ImGui::GetIO().DisplaySize;
	screen.x = (size.x * 0.5f * nx) + (nx + size.x * 0.5f);
	screen.y = -(size.y * 0.5f * ny) + (ny + size.y * 0.5f);

	return true;
}

INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show)
{
	Kernterface driver("\\\\.\\kestasDriver");

	const auto pid = driver.GetTargetPid();
	const auto client = driver.GetClientModule();

	if (!pid || !client)
	{
		MessageBoxA(nullptr, "CS:GO is not running!", "Error", MB_OK | MB_ICONERROR);
		return EXIT_FAILURE;
	}

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = instance;
	wc.lpszClassName = L"External overlay";

	RegisterClassExW(&wc);

	const auto width = GetSystemMetrics(SM_CXSCREEN);
	const auto height = GetSystemMetrics(SM_CYSCREEN);

	const HWND window = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
		wc.lpszClassName,
		L"External overlay",
		WS_POPUP,
		0, 0, width, height,
		nullptr, nullptr, wc.hInstance, nullptr
	);

	SetLayeredWindowAttributes(window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

	{
		RECT client_area{};
		GetClientRect(window, &client_area);

		RECT window_area{};
		GetWindowRect(window, &window_area);

		POINT diff{};
		ClientToScreen(window, &diff);

		const MARGINS margins{
			window_area.left + (diff.x - window_area.left),
			window_area.top + (diff.y - window_area.top),
			client_area.right,
			client_area.bottom
		};

		DwmExtendFrameIntoClientArea(window, &margins);

	}

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 60U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL levels[2]{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	D3D_FEATURE_LEVEL level{};

	D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		levels,
		2U,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&level,
		&device_context
	);

	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer) {
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	}
	else {
		return 1;
	}

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(device, device_context);

	bool running = true;

	while (running) {
		MSG msg;

		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				running = false;
			}
		}

		if (!running) {
			break;
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();

		ImGui::NewFrame();

		// cheat logic		
		const auto local_player = driver.Read<uintptr_t>(pid, client + offsets::dwLocalPlayer, sizeof(uintptr_t));

		if (local_player) {
			const auto local_team = driver.Read<int>(pid, local_player + offsets::m_iTeamNum, sizeof(int));
			ViewMatrix view_matrix;

			view_matrix[0][0] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x0, sizeof(float));
			view_matrix[0][1] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x4, sizeof(float));
			view_matrix[0][2] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x8, sizeof(float));
			view_matrix[0][3] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0xC, sizeof(float));

			view_matrix[1][0] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x10, sizeof(float));
			view_matrix[1][1] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x14, sizeof(float));
			view_matrix[1][2] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x18, sizeof(float));
			view_matrix[1][3] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x1C, sizeof(float));

			view_matrix[2][0] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x20, sizeof(float));
			view_matrix[2][1] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x24, sizeof(float));
			view_matrix[2][2] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x28, sizeof(float));
			view_matrix[2][3] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x2C, sizeof(float));

			view_matrix[3][0] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x30, sizeof(float));
			view_matrix[3][1] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x34, sizeof(float));
			view_matrix[3][2] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x38, sizeof(float));
			view_matrix[3][3] = driver.Read<float>(pid, client + offsets::dwViewMatrix + 0x3C, sizeof(float));

			// esp 
			if (GetAsyncKeyState(VK_XBUTTON1)) {
			
				for (int i = 1; i < 32; ++i) {
					const auto player = driver.Read<uintptr_t>(pid, client + offsets::dwEntityList + i * 0x10, sizeof(uintptr_t));

					if (!player) {
						continue;
					}

					if (driver.Read<int>(pid, player + offsets::m_iHealth, sizeof(int)) < 1) {
						continue;
					}

					if (driver.Read<bool>(pid, player + offsets::m_bDormant, sizeof(bool))) {
						continue;
					}

					if (driver.Read<int>(pid, player + offsets::m_iTeamNum, sizeof(int)) == local_team) {
						continue;
					}

					const auto bones = driver.Read<uintptr_t>(pid, player + offsets::m_dwBoneMatrix, sizeof(uintptr_t));

					if (!bones) {
						continue;
					}

					Vector head_pos{
						driver.Read<float>(pid, bones + 0x30 * 8 + 0x0C, sizeof(float)),
						driver.Read<float>(pid, bones + 0x30 * 8 + 0x1C, sizeof(float)),
						driver.Read<float>(pid, bones + 0x30 * 8 + 0x2C, sizeof(float))
					};

					auto feet_pos = driver.Read<Vector>(pid, player + offsets::m_vecOrigin, sizeof(Vector));

					Vector top;
					Vector bottom;

					if (world_to_screen(head_pos, top, view_matrix) && world_to_screen(feet_pos, bottom, view_matrix)) {
						const auto height = bottom.y - top.y;
						const auto width = height / 2.0f;

						const auto x = top.x - width / 2.0f;
						const auto y = top.y;

						ImGui::GetBackgroundDrawList()->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), ImColor(255, 0, 0, 255));
					}
				}

			}

			if (GetAsyncKeyState(VK_XBUTTON2)) {
				const auto crosshair_id = driver.Read<int>(pid, local_player + offsets::m_iCrosshairId, sizeof(int));
				const auto flash_duration = driver.Read<float>(pid, local_player + offsets::m_flFlashDuration, sizeof(float));

				if (crosshair_id > 0 && crosshair_id < 64 && flash_duration <= 0.0f) {
					const auto entity = driver.Read<uintptr_t>(pid, client + offsets::dwEntityList + (crosshair_id - 1) * 0x10, sizeof(uintptr_t));

					if (entity) {
						const auto entity_team = driver.Read<int>(pid, entity + offsets::m_iTeamNum, sizeof(int));

						if (entity_team != local_team) {
							mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
							mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
						}
					}
				}
			}
		}	

		//rendering

		ImGui::Render();
		constexpr float color[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
		device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);
		device_context->ClearRenderTargetView(render_target_view, color);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		swap_chain->Present(1U, 0U);
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (swap_chain) {
		swap_chain->Release();
	}

	if (device_context) {
		device_context->Release();
	}

	if (device) {
		device->Release();
	}

	if (render_target_view) {
		render_target_view->Release();
	}

	DestroyWindow(window);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}