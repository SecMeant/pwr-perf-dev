// WindowsProject1.cpp : Definiuje punkt wejścia dla aplikacji.
//

#include "stdafx.h"
#include "WindowsProject1.h"
#include "vfw.h"

#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <limits>
#include <intrin.h>

#pragma comment(lib, "vfw32")

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int, HWND*);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// windows.h shadows max method from numeric limits causing compilation errors
#undef max

using pixel_t = uint8_t;

std::atomic_uint8_t brightness = 1;
std::atomic<uint32_t> gray_scale(0);

constexpr auto avi_output_file = L"asdf.avi";
constexpr decltype(brightness) max_brightness_level = 40;
constexpr decltype(brightness) min_brightness_level = 0;

template<class T> struct always_false : std::false_type {};

template<typename T>
constexpr auto uaddsat(T& t1, T& t2)
{
	unsigned char c;

	if constexpr (sizeof(T) == 1)
		c = _addcarry_u8(0, t1, t2, &t1);
	else if constexpr (sizeof(T) == 2)
		c = _addcarry_u16(0, t1, t2, &t1);
	else if constexpr (sizeof(T) == 4)
		c = _addcarry_u32(0, t1, t2, &t1);
	else if constexpr (sizeof(T) == 8)
		c = _addcarry_u64(0, t1, t2, &t1);
	else
		static_assert(std::always_false<T>);

	if (c)
		t1 = std::numeric_limits<T>::max();

	return t1;
}

void magic(uint8_t *data, size_t size)
{
	for (size_t i = 0; i < size; i += 4)
	{
		pixel_t& v = data[i + 0];
		pixel_t& y1 = data[i + 1];
		pixel_t& u = data[i + 2];
		pixel_t& y0 = data[i + 3];

		y1 = 0x80;
		y0 = 0x80;
	}
}

void videofilter_grayscale(uint8_t *data, size_t size)
{
	for (size_t i = 0; i < size; i += 4)
	{
		pixel_t& v  = data[i + 0];
		pixel_t& y1 = data[i + 1];
		pixel_t& u  = data[i + 2];
		pixel_t& y0 = data[i + 3];

		y1 = 0x80;
		y0 = 0x80;
	}
}

void applybrightness(uint8_t *data, size_t size)
{
	auto brightness_local = brightness.load(std::memory_order::memory_order_relaxed);
	for (SIZE_T i = 0; i < size; i += 4)
	{
		pixel_t& v = data[i + 0];
		pixel_t& y1 = data[i + 1];
		pixel_t& u = data[i + 2];
		pixel_t& y0 = data[i + 3];

		pixel_t delta = 3 * brightness_local;

		uaddsat(v, delta);
		uaddsat(u, delta);
	}
}

LRESULT videoFilter(
	HWND hWnd,
	LPVIDEOHDR lpVHdr
)
{
	videohdr_tag *img_meta = (videohdr_tag *)lpVHdr;
	auto pixel = reinterpret_cast<pixel_t *>(img_meta->lpData);
	SIZE_T pixel_count = img_meta->dwBytesUsed / sizeof(pixel_t);

	if (gray_scale) {
		videofilter_grayscale(pixel, pixel_count);
	}

	magic(pixel, pixel_count);

	return 0;
}

static void printCaps(std::ofstream &s, CAPDRIVERCAPS &caps)
{
	s   << "cap capabilities:\n"
		<< "wDeviceIndex: " << caps.wDeviceIndex << '\n'
		<< "fHasOverlay: " << caps.fHasOverlay << '\n'
		<< "fHasDlgVideoSource: " << caps.fHasDlgVideoSource << '\n'
		<< "fHasDlgVideoFormat: " << caps.fHasDlgVideoFormat << '\n'
		<< "fHasDlgVideoDisplay: " << caps.fHasDlgVideoDisplay << '\n'
		<< "fCaptureInitialized: " << caps.fCaptureInitialized << '\n'
		<< "fDriverSuppliesPalettes: " << caps.fDriverSuppliesPalettes << '\n'
		<< "hVideoIn: " << reinterpret_cast<intptr_t>(caps.hVideoIn) << '\n'
		<< "hVideoOut: " << reinterpret_cast<intptr_t>(caps.hVideoOut) << '\n'
		<< "hVideoExtIn: " << reinterpret_cast<intptr_t>(caps.hVideoExtIn) << '\n'
		<< "hVideoExtOut: " << reinterpret_cast<intptr_t>(caps.hVideoExtOut) << "\n\n";
}

static void printCapParams(std::ofstream &s, CAPTUREPARMS &cparams)
{
	s   << "Capture params:\n"
		<< "dwRequestMicroSecPerFrame: " << cparams.dwRequestMicroSecPerFrame << '\n'
		<< "fMakeUserHitOKToCapture: " << cparams.fMakeUserHitOKToCapture << '\n'
		<< "wPercentDropForError: " << cparams.wPercentDropForError << '\n'
		<< "fYield: " << cparams.fYield << '\n'
		<< "dwIndexSize: " << cparams.dwIndexSize << '\n'
		<< "wChunkGranularity: " << cparams.wChunkGranularity << '\n'
		<< "fUsingDOSMemory: " << cparams.fUsingDOSMemory << '\n'
		<< "wNumVideoRequested: " << cparams.wNumVideoRequested << '\n'
		<< "fCaptureAudio: " << cparams.fCaptureAudio << '\n'
		<< "wNumAudioRequested: " << cparams.wNumAudioRequested << '\n'
		<< "vKeyAbort: " << cparams.vKeyAbort << '\n'
		<< "fAbortLeftMouse: " << cparams.fAbortLeftMouse << '\n'
		<< "fAbortRightMouse: " << cparams.fAbortRightMouse << '\n'
		<< "fLimitEnabled: " << cparams.fLimitEnabled << '\n'
		<< "wTimeLimit: " << cparams.wTimeLimit << '\n'
		<< "fMCIControl: " << cparams.fMCIControl << '\n'
		<< "fStepMCIDevice: " << cparams.fStepMCIDevice << '\n'
		<< "dwMCIStartTime: " << cparams.dwMCIStartTime << '\n'
		<< "dwMCIStopTime: " << cparams.dwMCIStopTime << '\n'
		<< "fStepCaptureAt2x: " << cparams.fStepCaptureAt2x << '\n'
		<< "wStepCaptureAverageFrames: " << cparams.wStepCaptureAverageFrames << '\n'
		<< "dwAudioBufferSize: " << cparams.dwAudioBufferSize << '\n'
		<< "fDisableWriteCache: " << cparams.fDisableWriteCache << '\n'
		<< "AVStreamMaster: " << cparams.AVStreamMaster << "\n\n";
}

static void printCapInfo(std::ofstream &s, BITMAPINFO &bminfo)
{
	s	<< "Cap bminfo:\n"
		<< "biSize: " << bminfo.bmiHeader.biSize << '\n'
		<< "biWidth: " << bminfo.bmiHeader.biWidth << '\n'
		<< "biHeight: " << bminfo.bmiHeader.biHeight << '\n'
		<< "biPlanes: " << bminfo.bmiHeader.biPlanes << '\n'
		<< "biBitCount: " << bminfo.bmiHeader.biBitCount << '\n'
		<< "biCompression: " << bminfo.bmiHeader.biCompression << '\n'
		<< "biSizeImage: " << bminfo.bmiHeader.biSizeImage << '\n'
		<< "biXPelsPerMeter: " << bminfo.bmiHeader.biXPelsPerMeter << '\n'
		<< "biYPelsPerMeter: " << bminfo.bmiHeader.biYPelsPerMeter << '\n'
		<< "biClrUsed: " << bminfo.bmiHeader.biClrUsed << '\n'
		<< "biClrImportant: " << bminfo.bmiHeader.biClrImportant << "\n\n";
}

using uint = unsigned;

class MainWindow
{
	void init_cap_window() noexcept
	{
		capDriverConnect(_cwnd, 0);
		capCaptureSequence(_cwnd);

		CAPDRIVERCAPS captureCapability;
		capDriverGetCaps(_cwnd, &captureCapability, sizeof(CAPDRIVERCAPS));
		printCaps(_log, captureCapability);

		CAPTUREPARMS capParams;
		capCaptureGetSetup(_cwnd, &capParams, sizeof(CAPTUREPARMS));
		printCapParams(_log, capParams);

		BITMAPINFO bminfo;
		capGetVideoFormat(_cwnd, &bminfo, sizeof(BITMAPINFO));
		printCapInfo(_log, bminfo);

		capParams.fYield = true;
		capCaptureSetSetup(_cwnd, &capParams, sizeof(CAPTUREPARMS));

		capPreviewScale(_cwnd, TRUE);
		capFileSetCaptureFile(_cwnd, avi_output_file);
		capCaptureSingleFrameOpen(_cwnd);
	}

public:
	static constexpr DWORD button_space_w = 20;

	static constexpr DWORD mainwindow_width = 805;
	static constexpr DWORD mainwindow_height = 800;

	static constexpr DWORD capture_pos_x = 0;
	static constexpr DWORD capture_pos_y = 0;
	static constexpr DWORD capture_pos_w = 800;
	static constexpr DWORD capture_pos_h = 600;

	static constexpr DWORD capture_button_pos_x = 50;
	static constexpr DWORD capture_button_pos_y = 700;
	static constexpr DWORD capture_button_pos_w = 100;
	static constexpr DWORD capture_button_pos_h = 30;
	static constexpr int capture_button_idx = 1337;

	static constexpr DWORD config_button_pos_x = capture_button_pos_x + capture_button_pos_w + button_space_w;
	static constexpr DWORD config_button_pos_y = 700;
	static constexpr DWORD config_button_pos_w = 100;
	static constexpr DWORD config_button_pos_h = 30;
	static constexpr int config_button_idx = 1338;

	static constexpr DWORD bup_button_pos_x = config_button_pos_x + config_button_pos_w + button_space_w;
	static constexpr DWORD bup_button_pos_y = 700;
	static constexpr DWORD bup_button_pos_w = 100;
	static constexpr DWORD bup_button_pos_h = 30;
	static constexpr int bup_button_idx = 1339;

	static constexpr DWORD bdown_button_pos_x = bup_button_pos_x + bup_button_pos_w + button_space_w;
	static constexpr DWORD bdown_button_pos_y = 700;
	static constexpr DWORD bdown_button_pos_w = 100;
	static constexpr DWORD bdown_button_pos_h = 30;
	static constexpr int bdown_button_idx = 1340;

	static constexpr DWORD gs_button_pos_x = bdown_button_pos_x + bdown_button_pos_w + button_space_w;
	static constexpr DWORD gs_button_pos_y = 700;
	static constexpr DWORD gs_button_pos_w = 100;
	static constexpr DWORD gs_button_pos_h = 30;
	static constexpr int gs_button_idx = 1341;

	MainWindow(HINSTANCE hi, int ncmdshow, DWORD width = 800, DWORD height = 800) noexcept
		: _log("debug.log")
	{
		InitInstance(hi, ncmdshow, &_mwnd);

		_cwnd = capCreateCaptureWindowA(
			"Capture window",
			WS_CHILD | WS_VISIBLE,
			capture_pos_x,
			capture_pos_y,
			capture_pos_w,
			capture_pos_h,
			_mwnd,
			2
		);

		_capbutton = CreateWindow(
			L"BUTTON",
			L"Capture frame",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			capture_button_pos_x,
			capture_button_pos_y,
			capture_button_pos_w,
			capture_button_pos_h,
			_mwnd,
			(HMENU)capture_button_idx,
			(HINSTANCE)GetWindowLong(_mwnd, GWL_HINSTANCE),
			NULL
		);

		_confbutton = CreateWindow(
			L"BUTTON",
			L"Configure",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			config_button_pos_x,
			config_button_pos_y,
			config_button_pos_w,
			config_button_pos_h,
			_mwnd,
			(HMENU)config_button_idx,
			(HINSTANCE)GetWindowLong(_mwnd, GWL_HINSTANCE),
			NULL
		);

		_bupbutton = CreateWindow(
			L"BUTTON",
			L"B+",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			bup_button_pos_x,
			bup_button_pos_y,
			bup_button_pos_w,
			bup_button_pos_h,
			_mwnd,
			(HMENU)bup_button_idx,
			(HINSTANCE)GetWindowLong(_mwnd, GWL_HINSTANCE),
			NULL
		);

		_bdownbutton = CreateWindow(
			L"BUTTON",
			L"B-",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			bdown_button_pos_x,
			bdown_button_pos_y,
			bdown_button_pos_w,
			bdown_button_pos_h,
			_mwnd,
			(HMENU)bdown_button_idx,
			(HINSTANCE)GetWindowLong(_mwnd, GWL_HINSTANCE),
			NULL
		);

		_grayscalebutton = CreateWindow(
			L"BUTTON",
			L"GS",
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
			gs_button_pos_x,
			gs_button_pos_y,
			gs_button_pos_w,
			gs_button_pos_h,
			_mwnd,
			(HMENU)gs_button_idx,
			(HINSTANCE)GetWindowLong(_mwnd, GWL_HINSTANCE),
			NULL
		);

		SetWindowLong(_mwnd, GWL_USERDATA, (LONG)this);
		capSetCallbackOnFrame(_cwnd, videoFilter);

		init_cap_window();

		thclose.test_and_set();
		th = std::thread([this] {
			while (thclose.test_and_set()) {
				capCaptureSingleFrame(_cwnd);
				std::this_thread::sleep_for(std::chrono::milliseconds(80));
			}
		});
	}

	~MainWindow()
	{
		thclose.clear();
		th.join();
		capDriverDisconnect(_cwnd);
	}

	template <typename T>
	void log(T&& s) noexcept
	{
		_log << s << '\n';
	}

	inline auto mwnd() const noexcept
	{
		return _mwnd;
	}

	inline auto cwnd() const noexcept
	{
		return _cwnd;
	}

	inline auto capbutton() const noexcept
	{
		return _capbutton;
	}

	void captureFrame()
	{
		capCaptureSingleFrameClose(_cwnd);
		capFileSaveDIB(_cwnd, L"frame.bmp");
		capCaptureSingleFrameOpen(_cwnd);
	}

	void configure()
	{
		capCaptureSingleFrameClose(_cwnd);
		capDlgVideoFormat(_cwnd);
		capCaptureSingleFrameOpen(_cwnd);
	}

	void bup()
	{
		if (brightness == max_brightness_level)
			return;
		++brightness;
	}

	void bdown()
	{
		if (brightness == min_brightness_level)
			return;
		--brightness;
	}

	void grayscale_toggle()
	{
		gray_scale.fetch_xor(1);
	}

private:
	HWND _mwnd;
	HWND _cwnd;
	HWND _capbutton;
	HWND _confbutton;
	HWND _bupbutton;
	HWND _bdownbutton;
	HWND _grayscalebutton;

	std::atomic_flag thclose;
	std::thread th;

	std::ofstream _log;
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINDOWSPROJECT1, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));

	MainWindow mwnd(hInstance, nCmdShow);

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, HWND* window_ptr)
{
	hInst = hInstance;

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, MainWindow::mainwindow_width, MainWindow::mainwindow_height,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	*window_ptr = hWnd;
	ShowWindow(hWnd, nCmdShow);

   UpdateWindow(hWnd);

   return TRUE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	MainWindow *mwnd = (MainWindow*) GetWindowLong(hWnd, GWL_USERDATA);
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
			int wmEvent = HIWORD(wParam);

            switch (wmId)
            {
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
			case MainWindow::capture_button_idx:
				mwnd->captureFrame();
				break;
			case MainWindow::config_button_idx:
				mwnd->configure();
				break;
			case MainWindow::bup_button_idx:
				mwnd->bup();
				break;
			case MainWindow::bdown_button_idx:
				mwnd->bdown();
				break;
			case MainWindow::gs_button_idx:
				mwnd->grayscale_toggle();
				break;
			default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
