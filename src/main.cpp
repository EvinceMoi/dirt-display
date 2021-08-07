#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/endian.hpp>
#include <boost/signals2.hpp>

#include <d3d11.h>
#include <tchar.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <Windows.h>


#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

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
	ID3D11Device* d3d_device_;
	ID3D11DeviceContext* d3d_device_context_;
	IDXGISwapChain* swap_chain_;
	ID3D11RenderTargetView* main_render_target_view_;
	WNDCLASSEX wc_;
	HWND hwnd_;
	bool abort_;
	telemetry_data_t data_;

	display(): abort_(false) {}
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
		hwnd_ = ::CreateWindow(
			wc_.lpszClassName,
			_T("Dirt Rally 2.0 Telemetry Display"),
			WS_OVERLAPPEDWINDOW,
			100, 100, 640, 240,
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



		return true;
	}
	void show() {
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		data_.speed = 43.4f;
		data_.gear = 5;
		data_.stear = -0.4;
		data_.clutch = 0;
		data_.brake = 0.2;
		data_.throttle = 0.83;
		data_.rpm = 7000.f;
		data_.max_rpm = 7212.f;
		data_.track_length = 16823.f;
		data_.lap_distance = 2438.f;

		data_.speed = 0.f;
		data_.gear = 0.f;
		data_.stear = 0.f;
		data_.clutch = 0.f;
		data_.brake = 0.f;
		data_.throttle = 0.f;
		data_.rpm = 0.f;
		data_.max_rpm = 0.f;
		data_.track_length = 0.f;
		data_.lap_distance = 0.f;

		auto io = ImGui::GetIO();
		auto font1 = io.Fonts->AddFontFromFileTTF("../deps/imgui/misc/fonts/DroidSans.ttf", 28.f);
		auto font2 = io.Fonts->AddFontFromFileTTF("../deps/imgui/misc/fonts/Roboto-Medium.ttf", 80.f);

		while (!abort_) {
			MSG msg;
			while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
					abort_ = true;
			}
			if (abort_) break;

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// draw content
			{
				auto io = ImGui::GetIO();
				ImGui::SetNextWindowPos(ImVec2(0, 0));
				ImGui::SetNextWindowSize(io.DisplaySize);
				// ImGui::Begin("Display", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
				ImGui::Begin("Display", 0, ImGuiWindowFlags_NoDecoration);

				io.WantCaptureMouse = true;

				auto drawList = ImGui::GetWindowDrawList();
				auto win_x = io.DisplaySize.x; auto win_y = io.DisplaySize.y;
				auto color_black = ImColor(0x00, 0x00, 0x00, 0xFF);
				auto color_white = ImColor(0xFF, 0xF0, 0xF0, 0xFF);
				// BG
				{
					drawList->AddRectFilled(ImVec2(), ImVec2(win_x, win_y), color_black);
				}
				// RPM
				{
					auto sy = 0;
					auto ey = win_y * 0.2;
					auto max_rpm_sep = static_cast<uint8_t>(std::floor(data_.max_rpm / 1000)) + 1;
					auto percent = data_.rpm / (max_rpm_sep * 1000);

					auto color_green = ImColor(0x00, 0xFF, 0x00, 0xFF);
					auto label_len = .12f;

					// draw bar
					{
						// mask
						auto sx = win_x * label_len;
						auto ex = win_x;
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
							auto p_tl = ImVec2(sx + (ex - sx) * pos_stop[i], sy);
							auto p_br = ImVec2(sx + (ex - sx) * pos_stop[i + 1], ey);
							drawList->AddRectFilledMultiColor(p_tl, p_br, cl, cr, cr, cl);
						}
						// real mask
						{
							auto ps = ImVec2(sx + (ex - sx) * percent, sy);
							auto pe = ImVec2(ex, ey);
							drawList->AddRectFilled(ps, pe, color_black);
						}
					}
					// daw rpm label
					{
						drawList->AddRectFilled(
							ImVec2(),
							ImVec2(win_x * label_len, ey),
							color_green
						);
						// draw RPM text
						static const char* label = "RPM";
						auto tsize = ImGui::CalcTextSize(label);
						//auto pos = ImVec2((win_x * label_len - tsize.x) / 2, (ey - tsize.y) / 2); // center
						auto pos = ImVec2(tsize.x / 3, (ey - tsize.y) / 2); // left
						drawList->AddText(pos, color_black, label);

					}

					// draw sep
					{
						auto tsize = ImGui::CalcTextSize("0");
						for (auto i = 0; i < max_rpm_sep; i++) {
							if ((i + 1) == max_rpm_sep) break;
							auto p = (i + 1) * 1.f / max_rpm_sep;
							auto x = win_x * label_len + win_x * (1 - label_len) * p - 1 * tsize.x;
							auto pos = ImVec2(x, (ey - tsize.y) / 2);
							auto t = std::to_string(i + 1);
							drawList->AddText(pos, color_black, t.c_str());
						}
					}
				}
				// Speed / Gear / Hand Brake
				{
					auto sy = win_y * 0.2;
					auto ey = win_y * 0.8;
					auto pos_tl = ImVec2(0, sy);
					auto pos_br = ImVec2(win_x, ey);

					// background
					drawList->AddRectFilled(pos_tl, pos_br, color_white);

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
						ImGui::PushFont(font2);
						auto tsize = ImGui::CalcTextSize(speed_kph.c_str());
						auto pos = ImVec2((win_x * p_speed - tsize.x), sy + (ey - sy - tsize.y) / 2);
						drawList->AddText(pos, color_black, speed_kph.c_str());
						ImGui::PopFont();
					}

					// draw km/h
					{
						static const char* kmh = "KM/H";
						auto tsize = ImGui::CalcTextSize(kmh);
						auto pos = ImVec2(win_x * p_speed + win_x * p_kmh * 0.2, sy + (ey - sy) / 2);
						drawList->AddText(pos, color_black, kmh);
					}

					// draw gear
					{
						auto tl = ImVec2(win_x * (p_speed + p_kmh), sy);
						auto br = ImVec2(win_x * (p_speed + p_kmh + p_gear), ey);
						auto color_gray = ImColor(0x69, 0x69, 0x69, 0xFF);
						drawList->AddRectFilled(tl, br, color_gray);

						ImGui::PushFont(font2);
						auto gear = std::to_string(static_cast<uint8_t>(std::floor(data_.gear)));
						auto tsize = ImGui::CalcTextSize("0");
						auto pos = ImVec2(tl.x + (win_x * p_gear - tsize.x) / 2, sy + (ey - sy - tsize.y) / 2);
						drawList->AddText(pos, color_black, gear.c_str());
						ImGui::PopFont();
					}
					
				}
				// Input(Brake/Throttle)
				{
					auto sy = win_y * 0.8;
					auto ey = win_y;
					auto pos_tl = ImVec2(0, sy);
					auto pos_br = ImVec2(win_x, ey);

					auto color_brake = ImColor(0xBD, 0x00, 0x00, 0xFF);
					auto color_throttle = ImColor(0x25, 0xBD, 0x00, 0xFF);

					auto brake_tl = ImVec2((1 - data_.brake) * win_x / 2, sy);
					auto brake_br = ImVec2(win_x / 2, ey);
					drawList->AddRectFilled(brake_tl, brake_br, color_brake);

					auto throttle_tl = ImVec2(win_x / 2, sy);
					auto throttle_br = ImVec2((1 + data_.throttle) * win_x / 2, ey);
					drawList->AddRectFilled(throttle_tl, throttle_br, color_throttle);

					static const char* brakes = " BRAKES ";
					static const char* throttle = " THROTTLE ";
					auto tsize_b = ImGui::CalcTextSize(brakes);
					auto tsize_t = ImGui::CalcTextSize(throttle);
					auto pos_b = ImVec2(win_x / 2 - tsize_b.x, sy + (ey - sy - tsize_b.y) / 2);
					auto pos_t = ImVec2(win_x / 2, sy + (ey - sy - tsize_t.y) / 2);
					drawList->AddText(pos_b, color_white, brakes);
					drawList->AddText(pos_t, color_white, throttle);
				}
				{
					if (io.MouseClicked[1]) {
						//toggle_window_frame();
					}

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
					//for (int i = 0; i < 5; i++) {
					//	auto f = io.MouseDown[i];
					//	if (f) {
					//		auto s = std::to_string(i);
					//		drawList->AddText(ImVec2(), color_white, s.c_str());
					//	}
					//}
				}

				ImGui::End();
				ImGui::PopStyleVar();
			}

			ImGui::Render();
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
	void toggle_window_frame() {
		LONG lStyle = GetWindowLong(hwnd_, GWL_STYLE);
		lStyle ^= WS_OVERLAPPEDWINDOW;
		SetWindowLong(hwnd_, GWL_STYLE, lStyle);

		//LONG lExStyle = GetWindowLong(hwnd_, GWL_EXSTYLE);
		//lExStyle ^= (WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
		//SetWindowLong(hwnd_, GWL_EXSTYLE, lExStyle);
		SetWindowPos(hwnd_, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
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


enum class offset_t : uint16_t {
	TIME_TOTAL = 0,
	TIME_LAP = 4,
	DISTANCE_LAP = 8, // meters
	DISTANCE_TOTAL = 12,
	POSITION_X = 16,
	POSITION_Y = 20,
	POSITION_Z = 24,
	SPEED = 28, // [m/s]
	VELOCITY_X = 32,
	VELOCITY_Y = 36,
	VELOCITY_Z = 40,
	ROLL_VECTOR_X = 44,
	ROLL_VECTOR_Y = 48,
	ROLL_VECTOR_Z = 52,
	PITCH_VECTOR_X = 56,
	PITCH_VECTOR_Y = 60,
	PITCH_VECTOR_Z = 64,
	SUSPENSION_POSITION_RL = 68,
	SUSPENSION_POSITION_RR = 72,
	SUSPENSION_POSITION_FL = 76,
	SUSPENSION_POSITION_FR = 80,
	SUSPENSION_VELOCITY_RL = 84,
	SUSPENSION_VELOCITY_RR = 88,
	SUSPENSION_VELOCITY_FL = 92,
	SUSPENSION_VELOCITY_FR = 96,
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
	CURRENT_LAP = 144, // rx only
	ENGINE_RATE = 148, // [rpm/10]
	TEMPERATURE_BRAKE_RL = 204,
	TEMPERATURE_BRAKE_RR = 208,
	TEMPERATURE_BRAKE_FL = 212,
	TEMPERATURE_BRAKE_FR = 216,
	TOTAL_LAPS = 240, // rx only, rally = 1
	TRACK_LENGTH = 244,
	MAX_RPM = 252 // maximum rpm / 10
};

template <typename E>
constexpr auto to_underlying(E e) noexcept {
	return static_cast<std::underlying_type_t<E>>(e);
}

namespace net = boost::asio;

struct listener {
	net::io_context& ioc_;
	net::ip::udp::socket sock_;
	bool abort_;

	net::ip::udp::endpoint sender_;
	std::array<uint8_t, 576> buf_;

	using sig_telemetry_t = boost::signals2::signal<void(const telemetry_data_t&)>;
	sig_telemetry_t sig_telemetry;
public:
	listener(net::io_context& ioc)
		: ioc_(ioc)
		, sock_(ioc)
		, abort_(false)
	{

	}

	void start() {
		boost::system::error_code ec;
		auto group = net::ip::make_address("239.10.9.8", ec);
		auto local = net::ip::make_address("192.168.10.61", ec);
		net::ip::udp::endpoint endp(local, 31000);

		sock_.open(endp.protocol(), ec);
		sock_.set_option(net::ip::udp::socket::reuse_address(true));
		sock_.bind(endp, ec);
		sock_.set_option(net::ip::multicast::join_group(group));

		net::spawn(ioc_.get_executor(), [this](net::yield_context yield) {
			do_recv(yield);
		});
	}
	void stop() {
		abort_ = true;
	}

	void do_recv(net::yield_context yield) {
		boost::system::error_code ec;
		while (!abort_) {
			auto size = sock_.async_receive_from(net::buffer(buf_), sender_, yield[ec]);
			std::cout << "got data, size: " << size << std::endl;

			if (ec) {
				continue;
			}
			if (size != 264) continue;

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
			
			sig_telemetry(data);
		}
	}

	float unpack(offset_t offset) {
		float data;
		std::memcpy(&data, buf_.data() + to_underlying(offset), 4);
		return data;
	}
};

int main(int argc, char* argv[]) {

	ShowConsole(false);

	net::io_context ioc;

	listener l(ioc);
	l.start();

	display ui;

	l.sig_telemetry.connect([&](const telemetry_data_t& d) {
		//std::cout << "update speed: " << d.speed << std::endl;
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