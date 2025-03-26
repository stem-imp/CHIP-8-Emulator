#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.19041.4355' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "Chip8\chip8.h"
#include "winlayout.h"

#define NOMINMAX
#include <Windows.h>
#include <tchar.h>
#include <CommCtrl.h>

#include <SDL2\SDL.h>
#include <SDL2\SDL_syswm.h>

#include <gl\GL.h>

#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <utility>
#include <sstream>
#include <string>

using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::ifstream;
using std::queue;
using std::stringstream;
using std::string;

void create_console_window();

SDL_Window* sdlWnd = nullptr;
SDL_GLContext glContext = nullptr;
int fps = 60;

GLint windowW, windowH;
void resize_window(int w, int h);

void on_key_down(Chip8& chip8, const SDL_Event& event);
void on_key_up(Chip8& chip8, const SDL_Event& event);

GLuint texture[] = { 0 };
void init_GL();
void setup_chip8_image_vertices(void);
void draw_chip8_image_buffer(Chip8& chip8);

#define WAV_CREATION
#ifdef WAV_CREATION
#define SAMPLE_RATE 44100	// Samples per second
#define DURATION	0.25	// Duration in seconds
#define FREQUENCY	392.0	// frequency of the beep (G4 note)

// WAV file header structure
typedef struct {
	char chunkID[4];       // "RIFF"
	uint32_t chunkSize;    // File size - 8 bytes
	char format[4];        // "WAVE"

	char subchunk1ID[4];   // "fmt "
	uint32_t subchunk1Size;// Size of the fmt chunk (16 for PCM)
	uint16_t audioFormat;  // Audio format (1 for PCM)
	uint16_t numChannels;  // Number of channels (1 for mono)
	uint32_t sampleRate;   // Samples per second
	uint32_t byteRate;     // Bytes per second
	uint16_t blockAlign;   // Bytes per sample * number of channels
	uint16_t bitsPerSample;// Bits per sample

	char subchunk2ID[4];   // "data"
	uint32_t subchunk2Size;// Size of the data chunk
} WavHeader;

void write_wav_header(uint8_t* buffer, int sampleRate, int bitsPerSample, int numChannels, int numSamples) {
	WavHeader header;

	// Fill the header
	memcpy(header.chunkID, "RIFF", 4);
	header.chunkSize = 36 + numSamples * numChannels * (bitsPerSample / 8);
	memcpy(header.format, "WAVE", 4);
	memcpy(header.subchunk1ID, "fmt ", 4);
	header.subchunk1Size = 16;
	header.audioFormat = 1; // PCM
	header.numChannels = numChannels;
	header.sampleRate = sampleRate;
	header.byteRate = sampleRate * numChannels * (bitsPerSample / 8);
	header.blockAlign = numChannels * (bitsPerSample / 8);
	header.bitsPerSample = bitsPerSample;
	memcpy(header.subchunk2ID, "data", 4);
	header.subchunk2Size = numSamples * numChannels * (bitsPerSample / 8);

	// Write the header
	std::copy((uint8_t*)&header, (uint8_t*)&header + sizeof(WavHeader), buffer);
}
#endif

void char_to_tchar(TCHAR* dst, const char* src, size_t dstLen);

#define ID_FILE_LOAD_ROM					512
#define ID_FILE_EXIT						WM_DESTROY
#define ID_SETTING_CONFIG					1024
#define ID_QUIRK_PARENT						1025
#define ID_QUIRK_RESET_VF					1026
#define ID_QUIRK_SET_VX_TO_VY				1027
#define ID_QUIRK_INCREMENT_I				1028

#define ID_VIDEO_PARENT						1536
#define ID_VIDEO_FPS_EDIT					1537
#define ID_VIDEO_SKIP_ON_SPRITE_COLLISION	1538

typedef struct ConfigTemp {
	ConfigTemp(Chip8* chip8=nullptr) : chip8(chip8), fps(0), quirks()
	{}

	Chip8* chip8;
	HWND dialog;
	HWND quirksGroup;
	HWND videoGroup;
	HWND fpsEdit;
	int fps;
	Chip8Quirks quirks;
	WNDPROC originalQuriksProc;
} ConfigTemp;

void add_menu(HWND hwnd);
bool open_chip8_file(HWND hwnd, TCHAR* filepath);

INT_PTR CALLBACK dialog_proc(HWND, UINT, WPARAM, LPARAM);
HWND create_config_dialog(HWND hParent, ConfigTemp& config);
void confirm_modal_dialog(ConfigTemp& config);
void close_modal_dialog(HWND hwnd);
HFONT get_scaled_font(HWND hwnd, int fontSize);
LRESULT CALLBACK fps_edit_subclass_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
void limit_fps(HWND fpsEdit);

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR pCmdLine, int nCmdShow)
{
	create_console_window();

	SDL_Init(SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	sdlWnd = SDL_CreateWindow(
		"Chip8 Interpreter",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		640, 320+20,
		SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);

	SDL_SysWMinfo systemInfo;
	SDL_VERSION(&systemInfo.version);
	SDL_GetWindowWMInfo(sdlWnd, &systemInfo);
	HWND hwnd = systemInfo.info.win.window;
	add_menu(hwnd);
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	ShowWindow(hwnd, SW_SHOWMINIMIZED);
	ShowWindow(hwnd, SW_RESTORE);

	glContext = SDL_GL_CreateContext(sdlWnd);
	SDL_GL_SetSwapInterval(0);
	init_GL();

	Chip8 chip8;
	ConfigTemp config(&chip8);

#ifdef WAV_CREATION
	int numSamples = static_cast<int>(SAMPLE_RATE * DURATION);
	vector<uint8_t> audioBuf(sizeof(WavHeader) + sizeof(int16_t)*numSamples, 0);
	write_wav_header(audioBuf.data(), SAMPLE_RATE, 16, 1, numSamples);
	uint16_t* data = (uint16_t*)(audioBuf.data() + sizeof(WavHeader));
	for (int i = 0; i < numSamples; i++) {
		double t = (double)i / SAMPLE_RATE; // Time in seconds
		data[i] = (int16_t)(32767 * sin(2 * M_PI * FREQUENCY * t));
	}
#endif

	bool quit = false;
	SDL_Event e;

	LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&startingTime);

	bool readyToDraw = false;
	uint8_t lastSoundTimer = 0;
	while (!quit) {
		while (SDL_PollEvent(&e) != 0)
		{
			switch (e.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_SYSWMEVENT:
				if (e.syswm.msg->msg.win.msg == WM_COMMAND)
				{
					switch (LOWORD(e.syswm.msg->msg.win.wParam)) {
						case ID_FILE_EXIT:
							quit = true;
						break;
						case ID_FILE_LOAD_ROM:{
							TCHAR filepath[MAX_PATH] = { 0 };
							if (open_chip8_file(hwnd, filepath)) {
								chip8.load_rom(filepath);
							}
						}
						break;
						case ID_SETTING_CONFIG:
							create_config_dialog(hwnd, config);
						break;
					}
				}
			case SDL_WINDOWEVENT:
				if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
					resize_window(e.window.data1, e.window.data2);
				} else if (e.window.event == SDL_WINDOWEVENT_SHOWN) {
					int w, h;
					SDL_GetWindowSize(sdlWnd, &w, &h);
					resize_window(w, h);
				}
				break;
			case SDL_KEYDOWN:
				on_key_down(chip8, e);
				break;
			case SDL_KEYUP:
				on_key_up(chip8, e);
				break;
			}
		}

		if (chip8.is_ROM_opened()) {
			uint16_t code = chip8.fetch_code();
			if (!readyToDraw) {
				chip8.execute_code(code);
				readyToDraw = chip8.is_draw_code(code) && !(chip8.is_sprites_overlapped() && chip8.skip_on_sprites_overlap());
			}

			QueryPerformanceCounter(&endingTime);
			elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
			elapsedMicroseconds.QuadPart *= 1000000;
			elapsedMicroseconds.QuadPart /= frequency.QuadPart;
			if (elapsedMicroseconds.QuadPart > 1000000 / fps) {
				QueryPerformanceCounter(&startingTime);
				if (lastSoundTimer == 0 && chip8.get_sound_timer() > 0) {
					lastSoundTimer = chip8.get_sound_timer();
#ifdef WAV_CREATION
					PlaySound((LPCTSTR)audioBuf.data(), GetModuleHandle(nullptr), SND_MEMORY | SND_ASYNC | SND_LOOP);
#endif
				}
				else if (lastSoundTimer > 0 && chip8.get_sound_timer() == 0) {
					lastSoundTimer = 0;
					PlaySound(nullptr, GetModuleHandle(nullptr), SND_SYNC);
				}
				chip8.countdown();
				if (readyToDraw) {
					readyToDraw = false;
					chip8.execute_code(code);
				}
				draw_chip8_image_buffer(chip8);
				SDL_GL_SwapWindow(sdlWnd);
			}
		}
	}

	SDL_DestroyWindow(sdlWnd);
	sdlWnd = nullptr;

	SDL_Quit();
#ifdef DEBUG
	FreeConsole();
#endif

	return 0;
}

void create_console_window()
{
#ifdef DEBUG
	AllocConsole();

	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	freopen_s(&fp, "CONIN$", "r", stdin);

	cout << "Console window is ready!" << endl;
#endif
}

void on_key_down(Chip8& chip8, const SDL_Event& event)
{
	switch (event.key.keysym.sym)
	{
	case SDLK_1: chip8.on_key_down(0x1); break;
	case SDLK_2: chip8.on_key_down(0x2); break;
	case SDLK_3: chip8.on_key_down(0x3); break;
	case SDLK_q: chip8.on_key_down(0x4); break;
	case SDLK_w: chip8.on_key_down(0x5); break;
	case SDLK_e: chip8.on_key_down(0x6); break;
	case SDLK_a: chip8.on_key_down(0x7); break;
	case SDLK_s: chip8.on_key_down(0x8); break;
	case SDLK_d: chip8.on_key_down(0x9); break;
	case SDLK_z: chip8.on_key_down(0xA); break;
	case SDLK_x: chip8.on_key_down(0x0); break;
	case SDLK_c: chip8.on_key_down(0xB); break;
	case SDLK_4: chip8.on_key_down(0xC); break;
	case SDLK_r: chip8.on_key_down(0xD); break;
	case SDLK_f: chip8.on_key_down(0xE); break;
	case SDLK_v: chip8.on_key_down(0xF); break;
	default: break;
	}
}

void on_key_up(Chip8& chip8, const SDL_Event& event)
{
	switch (event.key.keysym.sym)
	{
	case SDLK_1: chip8.on_key_up(0x1); break;
	case SDLK_2: chip8.on_key_up(0x2); break;
	case SDLK_3: chip8.on_key_up(0x3); break;
	case SDLK_q: chip8.on_key_up(0x4); break;
	case SDLK_w: chip8.on_key_up(0x5); break;
	case SDLK_e: chip8.on_key_up(0x6); break;
	case SDLK_a: chip8.on_key_up(0x7); break;
	case SDLK_s: chip8.on_key_up(0x8); break;
	case SDLK_d: chip8.on_key_up(0x9); break;
	case SDLK_z: chip8.on_key_up(0xA); break;
	case SDLK_x: chip8.on_key_up(0x0); break;
	case SDLK_c: chip8.on_key_up(0xB); break;
	case SDLK_4: chip8.on_key_up(0xC); break;
	case SDLK_r: chip8.on_key_up(0xD); break;
	case SDLK_f: chip8.on_key_up(0xE); break;
	case SDLK_v: chip8.on_key_up(0xF); break;
	default: break;
	}
}

void resize_window(int w, int h)
{
	windowW = w;
	windowH = h;
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, (GLdouble)w, 0.0, (GLdouble)h, -1.0, 1.0);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	setup_chip8_image_vertices();
	SDL_GL_SwapWindow(sdlWnd);
}

void init_GL()
{
	glClearColor(1.f, 1.f, 1.f, 1.f);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &texture[0]);
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void setup_chip8_image_vertices(void)
{
	static GLint vertices[] = {
		0, windowH, windowW, windowH, windowW, 0, 0, 0
	};
	static GLfloat texCoords[] = {
		0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f
	};
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_INT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
}

void draw_chip8_image_buffer(Chip8& chip8)
{
	glClear(GL_COLOR_BUFFER_BIT);
	const uint8_t* bytes = chip8.get_display_buffer();
	glTexImage2D(GL_TEXTURE_2D, 0,
		GL_LUMINANCE,
		64, 32, 0,
		GL_LUMINANCE,
		GL_UNSIGNED_BYTE,
		bytes);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glFinish();
}

void char_to_tchar(TCHAR* dst, const char* src, size_t dstLen)
{
	if (!src || !dst) return;
#ifdef UNICODE
	int size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, src, -1, NULL, 0);
	if (size > 0 && size <= dstLen) {
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, src, -1, dst, size);
	}
#else
	size_t srcLen = strlen(src);
	if (srcLen == 0) return;
	if (srcLen <= dstLen) {
		_tcsncpy(dst, src, srcLen);
		int nullIdx = srcLen < dstLen ? srcLen : srcLen-1;
		dst[nullIdx] = 0;
	}
	else {
		_tcsncpy(dst, src, dstLen);
		dst[dstLen-1] = 0;
	}
#endif
}

void add_menu(HWND hwnd)
{
	HMENU menuBar = CreateMenu();

	HMENU fileMenu = CreateMenu();
	AppendMenu(fileMenu, MF_STRING, ID_FILE_LOAD_ROM, _T("Load ROM"));
	AppendMenu(fileMenu, MF_STRING, ID_FILE_EXIT, _T("Exit"));

	HMENU settingsMenu = CreateMenu();
	AppendMenu(settingsMenu, MF_STRING, ID_SETTING_CONFIG, _T("Configs"));

	AppendMenu(menuBar, MF_POPUP, (UINT_PTR)fileMenu, _T("&File"));
	AppendMenu(menuBar, MF_POPUP, (UINT_PTR)settingsMenu, _T("&Settings"));

	SetMenu(hwnd, menuBar);
}

bool open_chip8_file(HWND hwnd, TCHAR* filepath)
{
	TCHAR fileDirectory[MAX_PATH] = { 0 };
	DWORD res = GetCurrentDirectory(MAX_PATH, fileDirectory);
	OPENFILENAME ofn = { 0 };
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = filepath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = _T("Chip8 Files\0*.ch8\0");
	ofn.nFilterIndex = 1; // 1-based
	ofn.lpstrInitialDir = fileDirectory;
	ofn.Flags = OFN_READONLY | OFN_PATHMUSTEXIST| OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn)) {
		cout << "Selected file: " << filepath << std::endl;
		return true;
	}
	else {
		// User canceled or an error occurred
		DWORD error = CommDlgExtendedError();
		if (error != 0) {
			cerr << "Error in GetOpenFileName: " << error << endl;
		}
		else {
			cout << "File selection canceled." << endl;
		}
		return false;
	}
}

INT_PTR CALLBACK dialog_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ConfigTemp& config = *reinterpret_cast<ConfigTemp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	
	WORD lowordWParam;
	switch (msg) {
	case WM_COMMAND:
		lowordWParam = LOWORD(wParam);
		switch (lowordWParam) {
		case IDOK:
			confirm_modal_dialog(config);
			return TRUE;
		case IDCANCEL:
			close_modal_dialog(hwnd);
			return TRUE;
		default: break;
		}
		break;
	case WM_CLOSE:
		close_modal_dialog(hwnd);
		return TRUE;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND create_config_dialog(HWND hParent, ConfigTemp& config)
{
	RECT rcParent;
	GetWindowRect(hParent, &rcParent);
	int windowW = rcParent.right - rcParent.left;
	int windowH = rcParent.bottom - rcParent.top;
	int dialogW = std::min(536, std::max(windowW / 2, 268));
	int dialogH = std::min(336, std::max(windowH / 2, 168));

	HWND hDlg = CreateWindowEx(
		WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
		WC_DIALOG,									// System dialog class
		_T("Settings"),
		WS_VISIBLE | WS_POPUPWINDOW | WS_CAPTION,
		0, 0, dialogW, dialogH,
		hParent,
		NULL,
		NULL,
		config.chip8);
	config.dialog = hDlg;
	if (hDlg) {
		SetWindowLongPtr(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&config));
		SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)dialog_proc);

		RECT dialogClientRect;
		GetClientRect(hDlg, &dialogClientRect);
		int dialogCW = dialogClientRect.right - dialogClientRect.left;
		int dialogCH = dialogClientRect.bottom - dialogClientRect.top;
		int dlgPadding = std::max(dialogCH / 32, 4);

		HWND quirksGroupbox = CreateWindow(_T("button"), _T("Quirks"),
			WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
			0, 0,
			0,
			0, hDlg, (HMENU)ID_QUIRK_PARENT, NULL, NULL);
		config.quirksGroup = quirksGroupbox;

		RECT quirksGroupboxRect;
		GetWindowRect(quirksGroupbox, &quirksGroupboxRect);
		int quirksGroupboxW = quirksGroupboxRect.right - quirksGroupboxRect.left;
		RECT quirksGroupboxClientRect;
		GetClientRect(quirksGroupbox, &quirksGroupboxClientRect);
		int quirksGroupboxCW = quirksGroupboxClientRect.right - quirksGroupboxClientRect.left;
		int quirksGroupboxCH = quirksGroupboxClientRect.bottom - quirksGroupboxClientRect.top;
		LOGFONT lf = { 0 };
		GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), &lf);
		lf.lfHeight = -MulDiv(12, GetDeviceCaps(GetDC(hDlg), LOGPIXELSY), 72); // 12 point font
		HFONT guiFont = CreateFontIndirect(&lf);
		HFONT guiFont1 = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		SendMessage(quirksGroupbox, WM_SETFONT, WPARAM(guiFont), FALSE);

		HWND resetVFHwnd = CreateWindow(_T("button"), _T("Reset VF"),
			WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0,
			quirksGroupbox, (HMENU)ID_QUIRK_RESET_VF, NULL, NULL);
		SendMessage(resetVFHwnd, WM_SETFONT, WPARAM(guiFont1), FALSE);
		UINT resetVFChecked = config.chip8->get_reset_VF() ? BST_CHECKED : BST_UNCHECKED;
		CheckDlgButton(quirksGroupbox, ID_QUIRK_RESET_VF, resetVFChecked);

		HWND setVXToVYHwnd = CreateWindow(_T("button"), _T("Set VX to VY"),
			WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0,
			quirksGroupbox, (HMENU)ID_QUIRK_SET_VX_TO_VY, NULL, NULL);
		SendMessage(setVXToVYHwnd, WM_SETFONT, WPARAM(guiFont1), FALSE);
		UINT setVXToVYChecked = config.chip8->is_VX_set_to_VY() ? BST_CHECKED : BST_UNCHECKED;
		CheckDlgButton(quirksGroupbox, ID_QUIRK_SET_VX_TO_VY, setVXToVYChecked);

		HWND incrementIHwnd = CreateWindow(_T("button"), _T("Increment I"),
			WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0,
			quirksGroupbox, (HMENU)ID_QUIRK_INCREMENT_I, NULL, NULL);
		SendMessage(incrementIHwnd, WM_SETFONT, WPARAM(guiFont1), FALSE);
		UINT incrementIChecked = config.chip8->is_increment_I() ? BST_CHECKED : BST_UNCHECKED;
		CheckDlgButton(quirksGroupbox, ID_QUIRK_INCREMENT_I, incrementIChecked);

		HWND okBtnHwnd = CreateWindow(_T("button"), _T("OK"),
			WS_VISIBLE | WS_CHILD | BS_FLAT,
			0, 0, 0, 0,
			hDlg, (HMENU)IDOK, NULL, NULL);
		SendMessage(okBtnHwnd, WM_SETFONT, WPARAM(guiFont1), FALSE);

		HWND cancelBtnHwnd = CreateWindow(_T("button"), _T("Cancel"),
			WS_VISIBLE | WS_CHILD | BS_FLAT,
			0, 0, 0, 0,
			hDlg, (HMENU)IDCANCEL, NULL, NULL);
		SendMessage(cancelBtnHwnd, WM_SETFONT, WPARAM(guiFont1), FALSE);

		HWND videoGroupbox = CreateWindow(_T("button"), _T("Video"),
			WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
			0, 0, 0, 0, hDlg, (HMENU)0, NULL, NULL);
		SendMessage(videoGroupbox, WM_SETFONT, WPARAM(guiFont), FALSE);
		config.videoGroup = videoGroupbox;

		HWND fpsTitle = CreateWindow(_T("static"), _T("FPS"),
			WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 0, 0, 0, 0,
			videoGroupbox, (HMENU)0, NULL, NULL);
		SendMessage(fpsTitle, WM_SETFONT, WPARAM(guiFont1), FALSE);
		HWND fpsEdit = CreateWindow(_T("edit"), _T(""),
			WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 0, 0, 0, 0,
			videoGroupbox, (HMENU)ID_VIDEO_FPS_EDIT, NULL, NULL);
		SetWindowSubclass(fpsEdit, fps_edit_subclass_proc, 0, 0);
		config.fpsEdit = fpsEdit;

		HFONT sysFont = get_scaled_font(fpsTitle, 12);
		SendMessage(fpsEdit, WM_SETFONT, WPARAM(sysFont), FALSE);
		RECT textRect = { 0, 0, 0, 0 };
		HDC hDC = GetDC(fpsEdit);
		HFONT hOldFont = (HFONT)SelectObject(hDC, sysFont);
		TCHAR text[2] = { '8' };
		SetWindowText(fpsEdit, text);
		DrawText(hDC, text, lstrlen(text), &textRect, DT_CALCRECT);
		SelectObject(hDC, hOldFont);
		ReleaseDC(fpsEdit, hDC);
		
		stringstream ss;
		ss << fps;
		TCHAR fpsValue[16] = { 0 };
		char_to_tchar(fpsValue, ss.str().c_str(), sizeof(fpsValue) / sizeof(fpsValue[0]));
		SetWindowText(fpsEdit, fpsValue);

		hDC = GetDC(videoGroupbox);
		DrawText(hDC, _T("Video"), lstrlen(_T("Video")), &textRect, DT_CALCRECT);
		ReleaseDC(videoGroupbox, hDC);

		HWND videoHSeparator = CreateWindow(_T("static"), NULL,
			WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
			0, 0, 0, 2, // Thin horizontal line
			videoGroupbox, (HMENU)0, NULL, NULL);
		
		HWND skipOnSpriteCollision = CreateWindow(_T("button"), _T("Skip on Sprite Collision"),
			WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0,
			videoGroupbox, (HMENU)ID_VIDEO_SKIP_ON_SPRITE_COLLISION,
			NULL, NULL);
		SendMessage(skipOnSpriteCollision, WM_SETFONT, WPARAM(guiFont1), FALSE);
		UINT skipOnSpriteCollisionChecked = config.chip8->skip_on_sprites_overlap() ? BST_CHECKED : BST_UNCHECKED;
		CheckDlgButton(videoGroupbox, ID_VIDEO_SKIP_ON_SPRITE_COLLISION, skipOnSpriteCollisionChecked);

		HWND hSeparator = CreateWindow(_T("static"), NULL,
			WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
			0, 0, 0, 2, // Thin horizontal line
			hDlg, (HMENU)0, NULL, NULL);

		// layout widgets
		winlayout::WidgetsContainer wc2(quirksGroupbox);
		winlayout::SequentialLayout quirksGroupboxLayout(SBS_VERT);
		quirksGroupboxLayout.set_padding(std::min(10, dlgPadding * 2), dlgPadding, dlgPadding, std::min(24, dlgPadding * 6));
		quirksGroupboxLayout.set_gap(0, dlgPadding);
		quirksGroupboxLayout.use_reletive_coordinates(true);
		wc2.set_layout(&quirksGroupboxLayout);
		winlayout::Widget resetVFWidget(resetVFHwnd), setVXToVYWidget(setVXToVYHwnd), incrementIWidget(incrementIHwnd);
		int fpsTitleHeight = textRect.bottom - textRect.top;
		resetVFWidget.set_box(0, 0, INT_MAX, fpsTitleHeight);
		setVXToVYWidget.set_box(0, 0, INT_MAX, fpsTitleHeight);
		incrementIWidget.set_box(0, 0, INT_MAX, fpsTitleHeight);
		wc2.add(&resetVFWidget);
		wc2.add(&setVXToVYWidget);
		wc2.add(&incrementIWidget);

		winlayout::Widget fpsTitleWidget(fpsTitle), fpsEditWidget(fpsEdit);
		winlayout::WidgetsContainer fpsContainer;
		winlayout::EvenLayout fpsLayout;
		fpsLayout.set_gap(2, 0);
		fpsContainer.set_layout(&fpsLayout);
		fpsContainer.add(&fpsTitleWidget);
		fpsContainer.add(&fpsEditWidget);
		fpsContainer.set_box(0, 0, INT_MAX, 0);
		fpsContainer.set_fixed_h(fpsTitleHeight); // 16

		winlayout::Widget videoHSeparatorWidget(videoHSeparator);
		videoHSeparatorWidget.set_box(0, 0, INT_MAX, 0);
		videoHSeparatorWidget.set_fixed_h(2);

		winlayout::Widget skipOnSpriteCollisionWidget(skipOnSpriteCollision);
		skipOnSpriteCollisionWidget.set_box(0, 0, INT_MAX, 0);
		skipOnSpriteCollisionWidget.set_fixed_h(fpsTitleHeight);
		
		winlayout::WidgetsContainer videoGroupboxContainer(videoGroupbox);
		winlayout::RatioLayout videoGroupboxLayout(SBS_VERT);
		videoGroupboxLayout.set_padding(std::min(10, dlgPadding*2), 0, dlgPadding, fpsTitleHeight/2+1);
		videoGroupboxLayout.set_gap(0, dlgPadding);
		videoGroupboxLayout.use_reletive_coordinates(true);
		videoGroupboxLayout.set_percentages({ 25, 0, 25, 0, 25, 0, 25});
		videoGroupboxContainer.set_layout(&videoGroupboxLayout);
		winlayout::Widget space;
		videoGroupboxContainer.add(&space);
		videoGroupboxContainer.add(&fpsContainer);
		videoGroupboxContainer.add(&space);
		videoGroupboxContainer.add(&videoHSeparatorWidget);
		videoGroupboxContainer.add(&space);
		videoGroupboxContainer.add(&skipOnSpriteCollisionWidget);
		videoGroupboxContainer.add(&space);

		winlayout::WidgetsContainer wc3;
		wc3.set_fixed_h(24);
		winlayout::EvenLayout el2;
		el2.set_gap(dlgPadding, 0);
		wc3.set_layout(&el2);
		winlayout::Widget okBtnWidget(okBtnHwnd), cancelBtnWidget(cancelBtnHwnd);
		wc3.add(&okBtnWidget);
		wc3.add(&cancelBtnWidget);

		winlayout::WidgetsContainer wc1;
		winlayout::RatioLayout rl(SBS_VERT);
		rl.set_percentages({ 100, 00, 00 });
		rl.set_gap(0, dlgPadding);
		wc1.set_layout(&rl);
		winlayout::Widget hSeparatorWidget(hSeparator);
		hSeparatorWidget.set_fixed_h(2);
		wc1.add(&videoGroupboxContainer);
		wc1.add(&hSeparatorWidget);
		wc1.add(&wc3);

		winlayout::WidgetsContainer wc;
		winlayout::EvenLayout el(SBS_HORZ);
		el.set_padding(dlgPadding, dlgPadding, dlgPadding, dlgPadding);
		el.set_gap(dlgPadding, dlgPadding);
		wc.set_layout(&el);
		wc.add(&wc2);
		wc.add(&wc1);
		wc.set_box(0, 0, dialogClientRect.right - dialogClientRect.left, dialogClientRect.bottom - dialogClientRect.top);

		// Center dialog on parent
		RECT rcDlg;
		GetWindowRect(hDlg, &rcDlg);
		int x = rcParent.left + (windowW - (rcDlg.right - rcDlg.left)) / 2;
		int y = rcParent.top + (windowH - (rcDlg.bottom - rcDlg.top)) / 2;
		SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);

		// Disable parent and focus dialog
		EnableWindow(hParent, FALSE);
		SetFocus(hDlg);
	}
	return hDlg;
}

void confirm_modal_dialog(ConfigTemp& config)
{
	UINT resetVFChecked = IsDlgButtonChecked(config.quirksGroup, ID_QUIRK_RESET_VF);
	Chip8Quirks quirks = {
		IsDlgButtonChecked(config.quirksGroup, ID_QUIRK_RESET_VF) == BST_CHECKED,
		IsDlgButtonChecked(config.quirksGroup, ID_QUIRK_SET_VX_TO_VY) == BST_CHECKED,
		IsDlgButtonChecked(config.quirksGroup, ID_QUIRK_INCREMENT_I) == BST_CHECKED
	};
	config.chip8->set_quirks(quirks);

	limit_fps(config.fpsEdit);

	UINT skipOnSpriteCollisionChecked = IsDlgButtonChecked(config.videoGroup, ID_VIDEO_SKIP_ON_SPRITE_COLLISION);
	config.chip8->set_skip_on_sprite_collision(skipOnSpriteCollisionChecked == BST_CHECKED);

	close_modal_dialog(config.dialog);
}

void close_modal_dialog(HWND hwnd)
{
	HWND hParent = GetParent(hwnd);
	EnableWindow(hParent, TRUE);
	SetFocus(hParent);
	DestroyWindow(hwnd);
}

HFONT get_scaled_font(HWND hwnd, int fontSize)
{
	HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
	LOGFONT lf;
	if (GetObject(hFont, sizeof(LOGFONT), &lf) == 0) {
		return 0;
	}
	lf.lfHeight = fontSize;
	return CreateFontIndirect(&lf);
}

LRESULT CALLBACK fps_edit_subclass_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_RETURN) {
			cout << "Enter\n";
			limit_fps(hwnd);
			return 0; // Prevent default handling
		}
		break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void limit_fps(HWND fpsEdit)
{
	int len = GetWindowTextLength(fpsEdit) + 1;
	vector<TCHAR> fpsText(len, 0);
	GetWindowText(fpsEdit, &fpsText[0], len);
	stringstream ss;
	ss.str(string(fpsText.begin(), fpsText.end()));
	ss >> fps;
	fps = std::min(fps, 300);
	ss.clear();
	ss.str("");
	ss << fps;
	TCHAR fpsValue[8] = { 0 };
	char_to_tchar(fpsValue, ss.str().c_str(), sizeof(fpsValue) / sizeof(fpsValue[0]));
	SetWindowText(fpsEdit, fpsValue);
}
