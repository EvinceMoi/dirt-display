#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/endian.hpp>
#include <boost/signals2.hpp>

#include <boost/program_options.hpp>

#include <d3d11.h>
#include <tchar.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <Windows.h>


#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "imgui_internal.h"

#include "font.hpp"

struct telemetry_data_t {
	float speed;
	float gear;
	float stear;
	float clutch;
	float brake;
	float throttle;
	float rpm;
	float max_rpm;
	float track_length;
	float lap_distance;
	float handbrake;
};

// Data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;


void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

bool CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void ShowConsole(bool show) {
	::ShowWindow(::GetConsoleWindow(), show ? SW_SHOWDEFAULT : SW_HIDE);
}

struct display {
	WNDCLASSEX wc_;
	HWND hwnd_;
	bool abort_;
	telemetry_data_t data_;
	ImFont* font_normal_;
	ImFont* font_big_;
	ImFont* font_small_;
	ImColor color_black{ 0x00, 0x00, 0x00, 0xFF };
	ImColor color_white{ 0xFF, 0xF0, 0xF0, 0xFF };
	ImColor color_gray{ 0x69, 0x69, 0x69, 0xFF };
	bool draw_fps_;

	display() : abort_(false), data_{0}, draw_fps_(false) {}
	bool init() {
		abort_ = false;
		wc_ = { 
			sizeof(WNDCLASSEX), 
			CS_CLASSDC, 
			WndProc, 
			0L, 0L, 
			GetModuleHandle(NULL), 
			NULL, NULL, NULL, NULL, 
			_T("ImGui Example"), 
			NULL 
		};
		::RegisterClassEx(&wc_);
		int width = 480; int height = 160;
		RECT rect;
		::GetClientRect(GetDesktopWindow(), &rect);
		int x = rect.right / 2 - width / 2;
		int y = rect.bottom / 2 - height / 2;
		hwnd_ = ::CreateWindow(
			wc_.lpszClassName,
			_T("Dirt Rally 2.0 Telemetry Display"),
			//WS_OVERLAPPEDWINDOW,
			//(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU),
			(WS_POPUP),
			x, y, width, height,
			NULL, NULL, 
			wc_.hInstance, NULL
		);

		if (!CreateDeviceD3D(hwnd_)) {
			CleanupDeviceD3D();
			::UnregisterClass(wc_.lpszClassName, wc_.hInstance);
			return false;
		}

		::ShowWindow(hwnd_, SW_SHOWDEFAULT);
		::UpdateWindow(hwnd_);
		::SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGui_ImplWin32_Init(hwnd_);
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

		//ImGui_ImplWin32_EnableDpiAwareness();
		
		auto io = ImGui::GetIO();

		io.WantCaptureMouse = true;
		io.WantCaptureKeyboard = true;

		font_normal_ = io.Fonts->AddFontFromMemoryCompressedTTF(
			CascadiaMonoPL_compressed_data,
			CascadiaMonoPL_compressed_size,
			28.f
		);
		font_big_ = io.Fonts->AddFontFromMemoryCompressedTTF(
			CascadiaMonoPL_compressed_data,
			CascadiaMonoPL_compressed_size,
			80.f
		);
		font_small_ = io.Fonts->AddFontFromMemoryCompressedTTF(
			CascadiaMonoPL_compressed_data,
			CascadiaMonoPL_compressed_size,
			16.f
		);

		return true;
	}
	void show() {
		auto io = ImGui::GetIO();

		while (!abort_) {
			MSG msg;
			while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
					abort_ = true;
			}
			if (abort_) break;

			handle_input();

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// draw content
			{
				auto io = ImGui::GetIO();
				ImGui::SetNextWindowPos(ImVec2(0, 0));
				ImGui::SetNextWindowSize(io.DisplaySize);

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
				ImGui::Begin("Display", 0, ImGuiWindowFlags_NoDecoration);

				auto win_x = io.DisplaySize.x; 
				auto win_y = io.DisplaySize.y;
				
				auto drawList = ImGui::GetWindowDrawList();
				drawList->AddRectFilled(ImVec2(), ImVec2(win_x, win_y), color_black); // background

				// RPM
				draw_section_rpm(ImVec2(), ImVec2(win_x, win_y * 0.2));
				// Speed / Gear / Hand Brake
				draw_section_speed(ImVec2(0, win_y * 0.2), ImVec2(win_x, win_y * 0.8));
				// Input(Brake/Throttle)
				draw_section_input(ImVec2(0, win_y * 0.8), ImVec2(win_x, win_y));
				// fps
				if (draw_fps_)
					draw_fps(ImVec2(), ImVec2(win_x, win_y));

				ImGui::End();
				ImGui::PopStyleVar();
			}

			ImGui::Render();
			static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
			const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
			g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
			g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			g_pSwapChain->Present(1, 0); // Present with vsync
		}

		if (abort_) {
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();

			CleanupDeviceD3D();
			::DestroyWindow(hwnd_);
			::UnregisterClass(wc_.lpszClassName, wc_.hInstance);
		}
	}
	void handle_input() {
		auto io = ImGui::GetIO();

		if (io.MouseClicked[1]) {
			// right click
			draw_fps_ = !draw_fps_;
		}

		{
			// drag move
			static bool drag_moving = false;
			static ImVec2 pos;
			float cdelta = .00001f;
			if (io.MouseDown[0] && io.MouseDownDuration[0] < cdelta) {
				// mouse left down
				drag_moving = true;
				pos = io.MousePos;
			}
			if (io.MouseReleased[0]) {
				drag_moving = false;
			}
			if (drag_moving) {
				auto p = io.MousePos;
				auto dx = p.x - pos.x;
				auto dy = p.y - pos.y;
				move_window(dx, dy);
			}

		}
		{
			// ESC to quit
			if (io.KeysDown[VK_ESCAPE]) {
				abort_ = true;
			}
		}
	}
	void draw_section_rpm(ImVec2 tl, ImVec2 br) {

		auto drawList = ImGui::GetWindowDrawList();

		auto w = br.x - tl.x;
		auto h = br.y - tl.y;

		auto max_rpm_step = static_cast<uint8_t>(std::floor(data_.max_rpm / 1000)) + 1;
		auto p = data_.rpm / (max_rpm_step * 1000);

		static auto color_green = ImColor(0x00, 0xFF, 0x00, 0xFF);
		auto label_len = .12f;


		// draw rpm label
		{
			drawList->AddRectFilled(
				tl,
				ImVec2(tl.x + w * label_len, tl.y + h),
				color_green
			);
			static const char* label = "RPM";
			auto tsize = ImGui::CalcTextSize(label);
			auto pos = ImVec2(tl.x + (w * label_len - tsize.x) / 2, tl.y + (h - tsize.y) / 2); // center
			drawList->AddText(pos, color_black, label);

		}
		// draw bar
		{
			// gradient background
			auto sx = tl.x + w * label_len;
			auto ex = br.x;
			std::array<float, 4> pos_stop = { 0.f, 0.4f, 0.64f, 1.f };
			std::array<ImColor, 4> color_stop = {
				color_green,
				ImColor(0xE0, 0xFF, 0x00, 0xFF),
				ImColor(0xFF, 0xF8, 0x00, 0xFF),
				ImColor(0xFF, 0x00, 0x00, 0xFF),
			};
			for (int i = 0; i < 3; i++) {
				auto cl = color_stop[i];
				auto cr = color_stop[i + 1];
				auto p_tl = ImVec2(sx + (ex - sx) * pos_stop[i], tl.y);
				auto p_br = ImVec2(sx + (ex - sx) * pos_stop[i + 1], br.y);
				drawList->AddRectFilledMultiColor(p_tl, p_br, cl, cr, cr, cl);
			}
			// mask
			{
				auto ps = ImVec2(sx + (ex - sx) * p, tl.y);
				auto pe = ImVec2(ex, br.y);
				drawList->AddRectFilled(ps, pe, color_black);
			}
		}

		// draw marker
		{
			auto tsize = ImGui::CalcTextSize("0");
			for (auto i = 0; i < max_rpm_step; i++) {
				if (max_rpm_step >= 12 && i % 2 == 0) continue;
				if ((i + 1) == max_rpm_step) break;
				auto p = (i + 1) * 1.f / max_rpm_step;
				auto x = tl.x + w * label_len + w * (1 - label_len) * p - 1 * tsize.x;
				auto pos = ImVec2(x, tl.y + (h - tsize.y) / 2);
				auto t = std::to_string(i + 1);
				drawList->AddText(pos, color_black, t.c_str());
			}
		}
	}
	void draw_section_speed(ImVec2 tl, ImVec2 br) {
		auto drawList = ImGui::GetWindowDrawList();

		auto w = br.x - tl.x;
		auto h = br.y - tl.y;

		// background
		auto color_background = color_white;
		drawList->AddRectFilled(tl, br, color_background);

		auto p_speed = 0.3;
		auto p_kmh = 0.3;
		auto p_gear = 0.2;
		auto p_hb = 0.2;
		// draw speed
		{
			auto speed_kph = std::to_string(static_cast<uint32_t>(std::floor(data_.speed * 3.6)));
			if (speed_kph.size() < 3) {
				auto prefix = 3 - speed_kph.size();
				speed_kph = std::string(2, ' ') + speed_kph;
			}
			ImGui::PushFont(font_big_);
			auto tsize = ImGui::CalcTextSize(speed_kph.c_str());
			auto pos = ImVec2(tl.x + w * p_speed - tsize.x, tl.y + (h - tsize.y) / 2);
			drawList->AddText(pos, color_black, speed_kph.c_str());
			ImGui::PopFont();
		}

		// draw km/h
		{
			static const char* kmh = "km/h";
			auto tsize = ImGui::CalcTextSize(kmh);
			auto pos = ImVec2(tl.x + w * p_speed + w * p_kmh * 0.2, tl.y + h / 2);
			drawList->AddText(pos, color_black, kmh);
		}

		// draw gear
		{
			auto ptl = ImVec2(tl.x + w * (p_speed + p_kmh), tl.y);
			auto pbr = ImVec2(tl.x + w * (p_speed + p_kmh + p_gear), tl.y + h);
			drawList->AddRectFilled(ptl, pbr, color_gray);
			static auto color_dec = ImColor(0xD9, 0x25, 0x25, 0xA0);
			auto border_thickness = 4.f;
			drawList->AddRectFilled(ptl, ImVec2(ptl.x + border_thickness, pbr.y), color_dec);
			drawList->AddRectFilled(ImVec2(pbr.x - border_thickness, ptl.y), pbr, color_dec);

			ImGui::PushFont(font_big_);
			std::string gear;
			if (data_.gear < 0)
				gear = "R";
			else if (data_.gear > 0)
				gear = std::to_string(static_cast<int8_t>(std::floor(data_.gear)));
			else
				gear = "N";
			auto tsize = ImGui::CalcTextSize("0");
			auto pos = ImVec2(ptl.x + (w * p_gear - tsize.x) / 2, tl.y + (h - tsize.y) / 2);
			drawList->AddText(pos, color_black, gear.c_str());
			ImGui::PopFont();
		}

		// draw handbrake
		{
			auto ptl = ImVec2(tl.x + w * (p_speed + p_kmh + p_gear), tl.y);
			auto pbr = ImVec2(tl.x + w, tl.y + h);
			auto pcenter = ImVec2(ptl.x + (pbr.x - ptl.x) / 2, ptl.y + (pbr.y - ptl.y) / 2);
			auto radius = std::min(pbr.x - ptl.x, pbr.y - ptl.y) * 0.23;

			bool on = data_.handbrake > 0.f;
			auto color = on ? ImColor(0xE0, 0x16, 0x16, 0xFF) : color_gray;
			
			auto thickness = 6.f;
			auto num_segment = 36;
			drawList->AddCircle(pcenter, radius, color, num_segment, thickness - 1);
			drawList->PathArcTo(pcenter, radius * 1.3, - IM_PI / 4, IM_PI / 4, num_segment);
			drawList->PathStroke(color, 0, thickness);
			drawList->PathArcTo(pcenter, radius * 1.3, IM_PI / 4 * 3, IM_PI / 4 * 5, num_segment);
			drawList->PathStroke(color, 0, thickness);

			// `!`
			auto mtl = ImVec2(pcenter.x - thickness / 2, pcenter.y - radius * 0.55);
			auto mbr = ImVec2(pcenter.x + thickness / 2, pcenter.y + radius * 0.55);
			drawList->AddRectFilled(mtl, mbr, color);
			drawList->AddRectFilled(
				ImVec2(mtl.x, mbr.y - thickness - thickness / 3),
				ImVec2(mbr.x, mbr.y - thickness),
				color_background
			);
		}
	}
	void draw_section_input(ImVec2 tl, ImVec2 br) {
		auto drawList = ImGui::GetWindowDrawList();

		auto w = br.x - tl.x;
		auto h = br.y - tl.y;

		static auto color_brake = ImColor(0xBD, 0x00, 0x00, 0xFF);
		static auto color_throttle = ImColor(0x25, 0xBD, 0x00, 0xFF);

		auto brake_tl = ImVec2(tl.x + (1 - data_.brake) * w / 2, tl.y);
		auto brake_br = ImVec2(tl.x + w / 2, br.y);
		drawList->AddRectFilled(brake_tl, brake_br, color_brake);

		auto throttle_tl = ImVec2(tl.x + w / 2, tl.y);
		auto throttle_br = ImVec2(tl.x + (1 + data_.throttle) * w / 2, tl.y + h);
		drawList->AddRectFilled(throttle_tl, throttle_br, color_throttle);

		// separator
		static ImColor c_sep{ 1.f, 1.f, 1.f, 0.1f };
		drawList->AddLine(
			ImVec2(tl.x + w / 2, tl.y),
			ImVec2(tl.x + w / 2, tl.y + h),
			c_sep
		);


		static const char* brakes = " BRAKES ";
		static const char* throttle = " THROTTLE ";
		auto tsize_b = ImGui::CalcTextSize(brakes);
		auto tsize_t = ImGui::CalcTextSize(throttle);
		auto pb = ImVec2(tl.x + w / 2 - tsize_b.x, tl.y + (h - tsize_b.y) / 2);
		auto pt = ImVec2(tl.x + w / 2, tl.y + (h - tsize_t.y) / 2);
		drawList->AddText(pb, color_white, brakes);
		drawList->AddText(pt, color_white, throttle);
	}
	void draw_fps(ImVec2 tl, ImVec2 br) {
		// frame rate
		auto io = ImGui::GetIO();
		auto drawList = ImGui::GetWindowDrawList();

		auto w = br.x - tl.x;

		ImGui::PushFont(font_small_);
		auto s = std::to_string(static_cast<int>(io.Framerate));
		if (s.size() < 3) {
			auto prefix = 3 - s.size();
			s = std::string(2, ' ') + s;
		}
		auto tsize = ImGui::CalcTextSize(s.c_str());
		static ImColor c_fps{ 1.f, 1.f, 1.f, 0.8f };
		drawList->AddText(ImVec2(tl.x + w - tsize.x / 4 * 5, tsize.x / 4), c_fps, s.c_str());
		ImGui::PopFont();
	}
	
	void move_window(float dx, float dy) {
		RECT rect;
		GetWindowRect(hwnd_, &rect);
		auto x = rect.left + static_cast<LONG>(dx);
		auto y = rect.top + static_cast<LONG>(dy);
		MoveWindow(hwnd_, x, y, rect.right - rect.left, rect.bottom - rect.top, true);
	}
	void update(const telemetry_data_t& d) {
		data_ = d;
	}
	void stop() {
		abort_ = true;
	}
};

// UDP mode 3
enum class offset_t : uint16_t {
	TIME_TOTAL = 0,
	TIME_LAP = 4,
	
	DISTANCE_LAP = 8, // meters
	DISTANCE_TOTAL = 12,
	
	POSITION_X = 16,
	POSITION_Y = 20,
	POSITION_Z = 24,
	
	SPEED = 28, // the magnitude of the vehicles linear velocity, in [m/s]
	
	VELOCITY_X = 32,
	VELOCITY_Y = 36,
	VELOCITY_Z = 40,
	
	ROLL_VECTOR_X = 44,
	ROLL_VECTOR_Y = 48,
	ROLL_VECTOR_Z = 52,
	
	PITCH_VECTOR_X = 56,
	PITCH_VECTOR_Y = 60,
	PITCH_VECTOR_Z = 64,
	
	// the vertical position of the wheel from rest(in car space), measured in meters
	SUSPENSION_POSITION_RL = 68,
	SUSPENSION_POSITION_RR = 72,
	SUSPENSION_POSITION_FL = 76,
	SUSPENSION_POSITION_FR = 80,

	// the vertical acceleration of the wheel from rest(in car space), measured in m/s^2
	SUSPENSION_VELOCITY_RL = 84,
	SUSPENSION_VELOCITY_RR = 88,
	SUSPENSION_VELOCITY_FL = 92,
	SUSPENSION_VELOCITY_FR = 96,

	// the speed of the contact patch of the wheel, in m/s
	WHEEL_PATCH_SPEED_RL = 100,
	WHEEL_PATCH_SPEED_RR = 104,
	WHEEL_PATCH_SPEED_FL = 108,
	WHEEL_PATCH_SPEED_FR = 112,

	INPUT_THROTTLE = 116,
	INPUT_STEER = 120,
	INPUT_BRAKE = 124,
	INPUT_CLUTCH = 128,
	GEAR = 132, // 0: neutral, 1, 2, ... 10 = Reverse
	GFORCE_LATERAL = 136,
	GFORCE_LONGITUDINAL = 140,
	CURRENT_LAP = 144, // RX only
	ENGINE_RATE = 148, // [rpm/10]

	NATIVE_SLI_SUPPORT = 152,
	RACE_POSITION = 156,
	KERS_LEVEL = 160,
	KERS_LEVEL_MAX = 164,
	DRS = 168,
	TRACTION_CONTROL = 172,
	ABS = 176,
	FUEL_IN_TANK = 180,
	FUEL_CAPACITY = 184,
	IN_PITS = 188,
	RACE_SECTOR = 192, // the number of splits in the lap
	SECTION_TIME_1 = 196,
	SECTION_TIME_2 = 200,

	TEMPERATURE_BRAKE_RL = 204,
	TEMPERATURE_BRAKE_RR = 208,
	TEMPERATURE_BRAKE_FL = 212,
	TEMPERATURE_BRAKE_FR = 216,

	TYRE_PRESSURE_RL = 220,
	TYRE_PRESSURE_RR = 224,
	TYRE_PRESSURE_FL = 228,
	TYRE_PRESSURE_FR = 232,

	LAPS_COMPLETED = 236,

	TOTAL_LAPS = 240, // RX only, rally = 1
	TRACK_LENGTH = 244,

	LAST_LAP_TIME = 248,

	MAX_RPM = 252, // maximum rpm / 10

	IDLE_RPM = 256,
	MAX_GEARS = 260 // data length: 264
};

template <typename E>
constexpr auto to_underlying(E e) noexcept {
	return static_cast<std::underlying_type_t<E>>(e);
}

std::string uint8_to_hex(uint8_t it) {
	std::stringstream ss;
	ss << std::setfill('0') << std::setw(2) << std::hex << it;
	return ss.str();
}

namespace net = boost::asio;

struct listener {
	net::io_context& ioc_;
	net::ip::udp::socket sock_;
	bool abort_;

	net::ip::udp::endpoint sender_;
	std::array<uint8_t, 576> buf_{};

	using sig_telemetry_t = boost::signals2::signal<void(const telemetry_data_t&)>;
	sig_telemetry_t sig_telemetry;

#ifdef ENABLE_MULTICAST
	net::ip::address relay_host_;
	uint16_t relay_port_;
#endif

public:
#ifndef ENABLE_MULTICAST
	explicit listener(net::io_context& ioc)
		: ioc_(ioc)
		, sock_(ioc)
		, abort_(false)
	{

	}
#else
	listener(net::io_context& ioc, const net::ip::address& relay_host, uint16_t relay_port)
		: ioc_(ioc)
		, sock_(ioc)
		, abort_(false)
		, relay_host_(relay_host)
		, relay_port_(relay_port)
	{

	}

#endif 



	void start() {
		boost::system::error_code ec;
#ifndef ENABLE_MULTICAST
		net::ip::udp::endpoint endp(net::ip::address_v4::any(), 31000);
#else
		net::ip::udp::endpoint endp;
		if (relay_host_.is_v4()) {
			endp = { net::ip::address_v4::any(), relay_port_ };
		} else {
			endp = { net::ip::address_v6::any(), relay_port_ };
		}

		(net::ip::address_v4::any(), relay_port_);
#endif
		sock_.open(endp.protocol(), ec);
		sock_.set_option(net::ip::udp::socket::reuse_address(true));
		sock_.bind(endp, ec);
		if (ec) {
			std::cout << "bind " << endp.address().to_string() << " failed: " << ec.message() << std::endl;
		}
		sock_.non_blocking(true);
		net::spawn(ioc_.get_executor(), [this](net::yield_context yield) {
			do_recv(yield);
		});
#ifdef ENABLE_MULTICAST
		// send sub sync byte
		net::ip::udp::endpoint relay(relay_host_, relay_port_);
		buf_[0] = 0x2A;
		sock_.async_send_to(net::buffer(buf_, 1), relay, [](const boost::system::error_code& ec, std::size_t bytes) {
			if (ec) {
				std::cout << "sub error: " << ec.message() << std::endl;
				return;
			}
			std::cout << "sub request sent" << std::endl;
		});
#endif
	}
	void stop() {
		abort_ = true;
	}

	void do_recv(net::yield_context yield) {
		boost::system::error_code ec;

		auto timestamp = []() {
			using namespace std::chrono;
			return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
		};

		auto ts = timestamp();
		float pt = 0.f;
		auto on_stage_start = true;
		while (!abort_) {
			auto size = sock_.async_receive_from(net::buffer(buf_), sender_, yield[ec]);
			if (ec) {
				continue;
			}
#ifndef ENABLE_MULTICAST
			if (size != 264) continue;
#else

			if (sender_.address() != relay_host_) continue;

			if (size != 264) {
				if (size < 10) continue;
				
				std::string group;
				std::string local;

				int pos = 0;
				auto gaddrlen = buf_[pos++];
				if (gaddrlen != 4 && gaddrlen != 16) continue; // invalid len

				auto parse_ip = [this](int pos, uint8_t size) -> net::ip::address {
					if (size == 4) {
						net::ip::address_v4::bytes_type abuf;
						std::copy(std::begin(buf_) + pos, std::begin(buf_) + pos + size, std::begin(abuf));
						return net::ip::make_address_v4(abuf);
					} else {
						net::ip::address_v6::bytes_type abuf;
						std::copy(std::begin(buf_) + pos, std::begin(buf_) + pos + size, std::begin(abuf));
						return net::ip::make_address_v6(abuf);
					}
				};

				auto group_addr = parse_ip(pos, gaddrlen);
				pos += gaddrlen;
				
				auto laddrlen = buf_[pos++];
				if (laddrlen != 4 && laddrlen != 16) continue; // invalid len

				auto local_addr = parse_ip(pos, laddrlen);

				std::cout << "g:" << group_addr.to_string() << ", l:" << local_addr.to_string() << std::endl;

				auto sock_addr = sock_.local_endpoint(ec).address();
				if (group_addr.is_v4()) {
					sock_.set_option(net::ip::multicast::join_group(group_addr.to_v4(), local_addr.to_v4()), ec);
				} else {
					sock_.set_option(net::ip::multicast::join_group(group_addr.to_v6(), 0), ec);
				}
				if (ec) {
					std::cout << "join group: " << ec.message() << std::endl;
				}

				continue;
			}
#endif
		
			auto now = timestamp();
			if (on_stage_start = false && (now - ts) > 15) {
				on_stage_start = true;
			} else {
				on_stage_start = false;
				ts = now;
			}

			auto ct = unpack(offset_t::TIME_TOTAL);
			pt = ct;

			telemetry_data_t data;
			data.speed        = unpack(offset_t::SPEED);
			data.gear         = unpack(offset_t::GEAR);
			data.stear        = unpack(offset_t::INPUT_STEER);
			data.clutch       = unpack(offset_t::INPUT_CLUTCH);
			data.brake        = unpack(offset_t::INPUT_BRAKE);
			data.throttle     = unpack(offset_t::INPUT_THROTTLE);
			data.rpm          = unpack(offset_t::ENGINE_RATE) * 10;
			data.max_rpm      = unpack(offset_t::MAX_RPM) * 10;
			data.track_length = unpack(offset_t::TRACK_LENGTH);
			data.lap_distance = unpack(offset_t::DISTANCE_LAP);

			{
				static float last_brake_time = -1;
				static float last_handlebrake_time = -1;
				static float last_rw_lock_time = -1;
				if (on_stage_start) {
					last_brake_time = last_handlebrake_time = last_rw_lock_time = -1;
				}

				if (data.brake == 0) {
					last_brake_time = -1;
				} else {
					// record the first time brake is on
					if (last_brake_time < 0)
						last_brake_time = pt;
				}

				//static auto handbrake_on = false;

				// handbrake
				auto wps_fl = unpack(offset_t::WHEEL_PATCH_SPEED_FL);
				auto wps_fr = unpack(offset_t::WHEEL_PATCH_SPEED_FR);
				auto wps_rl = unpack(offset_t::WHEEL_PATCH_SPEED_RL);
				auto wps_rr = unpack(offset_t::WHEEL_PATCH_SPEED_RR);

				auto avg_f = (std::fabs(wps_fl) + std::fabs(wps_fr)) / 2;
				auto avg_r = (std::fabs(wps_rl) + std::fabs(wps_rr)) / 2;

				auto speed_min = 0.01; // 1.f / 3.6;
				auto handbrake_on = false;

				if (data.speed > speed_min) { // car stopped with gear greater than 0 still got very small speed input
					auto fw_locked = avg_f == 0;
					auto rw_locked = avg_r == 0;
					if (rw_locked) { // rear locked
						auto first_rw_lock = false;
						if (last_rw_lock_time < 0) {
							last_rw_lock_time = pt; // record the time point of the first rear locked
							first_rw_lock = true;
						}

						if (first_rw_lock) {
							//handbrake_on = !fw_locked;
							if (fw_locked) { // brake caused lock
								handbrake_on = false;
							} else {
								handbrake_on = true;
							}
						} else {
							if (pt > last_rw_lock_time && last_rw_lock_time > 0) { // already locked before
								if (!fw_locked) handbrake_on = true;
								else { // front locked
									// brake on
									if (last_brake_time > 0) {
										if (last_rw_lock_time < last_brake_time)
											handbrake_on = true;
										else {
											// rear wheel lock after brake
											// 1. brake caused lock
											// 2. handbrake while braking
											// cannot be detected only from wheel lock, temporarily set to false
											// FIXME handbrake_on = false ?
										}
									} else {
										handbrake_on = true;
									}
								}
							}
						}
					} else {
						handbrake_on = false;
						last_rw_lock_time = -1;
					}

				} else { // considered still
					handbrake_on = true;
				}

				data.handbrake = handbrake_on ? 1.f : 0.f;
				if (!handbrake_on) {
					last_handlebrake_time = -1.f;
				} else {
					if (last_handlebrake_time < 0)
						last_handlebrake_time = pt;
				}
			}
			
			sig_telemetry(data);
		}
	}

	float unpack(offset_t offset) {
		// data use little endian
		float data;
		std::memcpy(&data, buf_.data() + to_underlying(offset), 4);
		return data;
	}
};

int main(int argc, char* argv[]) {

	ShowConsole(false);

	net::io_context ioc;

#ifdef ENABLE_MULTICAST
	namespace po = boost::program_options;

	std::string relay_host;
	uint16_t relay_port;

	po::options_description opts("options");
	opts.add_options()
		("host,h", po::value<std::string>(&relay_host)->default_value("127.0.0.1"), "relay server host")
		("port,p", po::value<uint16_t>(&relay_port)->default_value(31000), "relay server port")
		;
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, opts), vm);
	po::notify(vm);

	boost::system::error_code ec;
	auto rh = net::ip::make_address(relay_host, ec);
	if (ec) {
		std::cout << "failed to parse relay host: " << ec.message() << std::endl;
		exit(EXIT_FAILURE);
	}

	listener l(ioc, rh, relay_port);
#else
	listener l(ioc);
#endif
	l.start();

	display ui;

	l.sig_telemetry.connect([&](const telemetry_data_t& d) {
		ui.update(d);
	});

	std::thread uit([&]() {
		ui.init();
		ui.show();

		l.stop();
		ioc.stop();
	});

	net::signal_set sigs(ioc);
	sigs.add(SIGINT);
	sigs.add(SIGTERM);
	sigs.async_wait([&](const boost::system::error_code&, int) {
		ui.stop();
		l.stop();
		ioc.stop();
	});

	ioc.run();
	uit.join();
	
	return EXIT_SUCCESS;
}