#include "std.h"
#include "gxruntime.h"
#include "zmouse.h"
#include "../bbruntime/bbaudio.h"

#include "../gxruntime/gxutf8.h"

struct gxRuntime::GfxMode {
	DDSURFACEDESC2 desc;
};
struct gxRuntime::GfxDriver {
	GUID* guid;
	std::string name;
	std::vector<GfxMode*> modes;
	D3DDEVICEDESC7 d3d_desc;
};

static const int static_ws = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
static const int scaled_ws = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

static std::string app_title;
static std::string app_close;
static gxRuntime* runtime;
static bool busy, suspended;
static volatile bool run_flag;
static DDSURFACEDESC2 desktop_desc;

typedef int(_stdcall* LibFunc)(const void* in, int in_sz, void* out, int out_sz);

struct gxDll {
	HINSTANCE hinst;
	std::map<std::string, LibFunc> funcs;
};

static std::map<std::string, gxDll*> libs;

static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

//current gfx mode
//
//0=NONE
//1=SCALED WINDOW
//2=FIXED SIZE WINDOW
//3=EXCLUSIVE
//
static int gfx_mode;
static int border_mode;
static bool gfx_lost;
static bool auto_suspend;

//for modes 1 and 2
static int mod_cnt;
static MMRESULT timerID;
static IDirectDrawClipper* clipper;
static IDirectDrawSurface7* primSurf;
static Debugger* debugger;

static std::set<gxTimer*> timers;

enum {
	WM_STOP = WM_APP + 1, WM_RUN, WM_END
};

////////////////////
// STATIC STARTUP //
////////////////////
gxRuntime* gxRuntime::openRuntime(HINSTANCE hinst, const std::string& cmd_line, Debugger* d) {
	if(runtime) return 0;

	//create debugger
	debugger = d;

	//create WNDCLASS
	WNDCLASS wndclass;
	memset(&wndclass, 0, sizeof(wndclass));
	wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndclass.lpfnWndProc = ::windowProc;
	wndclass.hInstance = hinst;
	wndclass.lpszClassName = "Blitz Runtime Class";
	wndclass.hCursor = (HCURSOR)LoadCursor(0, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	RegisterClass(&wndclass);

	gfx_mode = GMODE_NONE;
	clipper = 0; primSurf = 0;
	busy = suspended = false;
	run_flag = true;

	const char* app_t = " ";
	int ws = WS_CAPTION, ws_ex = 0;

	HWND hwnd = CreateWindowEx(ws_ex, "Blitz Runtime Class", app_t, ws, 0, 0, 0, 0, 0, 0, 0, 0);

	UpdateWindow(hwnd);

	runtime = new gxRuntime(hinst, cmd_line, hwnd);
	return runtime;
}

void gxRuntime::closeRuntime(gxRuntime* r) {
	if(!runtime || runtime != r) return;

	std::map<std::string, gxDll*>::const_iterator it;
	for(it = libs.begin(); it != libs.end(); ++it) {
		FreeLibrary(it->second->hinst);
	}
	libs.clear();

	delete runtime;
	runtime = 0;
}

//////////////////////////
// RUNTIME CONSTRUCTION //
//////////////////////////
typedef int(_stdcall* SetAppCompatDataFunc)(int x, int y);
typedef void (WINAPI* RtlGetVersionFunc) (OSVERSIONINFO*);

gxRuntime::gxRuntime(HINSTANCE hi, const std::string& cl, HWND hw) :
	hinst(hi), cmd_line(cl), hwnd(hw), curr_driver(0), enum_all(false),
	pointer_visible(true), input(0), graphics(0), fileSystem(0), use_di(false) {

	CoInitialize(0);

	enumGfx();
	TIMECAPS tc;
	timeGetDevCaps(&tc, sizeof(tc));
	timeBeginPeriod(tc.wPeriodMin);

	memset(&osinfo, 0, sizeof(osinfo));
	osinfo.dwOSVersionInfoSize = sizeof(osinfo);

	HMODULE osinfodll = LoadLibraryA("ntdll.dll");
	if(osinfodll) {
		RtlGetVersionFunc RtlGetVersion = (RtlGetVersionFunc)GetProcAddress(osinfodll, "RtlGetVersion");
		if(RtlGetVersion) RtlGetVersion(&osinfo);
		FreeLibrary(osinfodll);
	}

	memset(&statex, 0, sizeof(statex));
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx(&statex);

	HMODULE ddraw = LoadLibraryA("ddraw.dll");
	if(ddraw) {
		SetAppCompatDataFunc SetAppCompatData = (SetAppCompatDataFunc)GetProcAddress(ddraw, "SetAppCompatData");
		if(SetAppCompatData) SetAppCompatData(12, 0);
		FreeLibrary(ddraw);
	}

	memset(&devmode, 0, sizeof(devmode));
	devmode.dmSize = sizeof(devmode);
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
}

gxRuntime::~gxRuntime() {
	while(timers.size()) freeTimer(*timers.begin());
	if(graphics) closeGraphics(graphics);
	if(input) closeInput(input);
	TIMECAPS tc;
	timeGetDevCaps(&tc, sizeof(tc));
	timeEndPeriod(tc.wPeriodMin);
	denumGfx();
	DestroyWindow(hwnd);
	UnregisterClass("Blitz Runtime Class", hinst);

	CoUninitialize();
}

void gxRuntime::pauseAudio() {

}

void gxRuntime::resumeAudio() {

}

void gxRuntime::restoreGraphics() {
	if(auto_suspend) {
		if(!graphics->restore()) gfx_lost = true;
	}
}

void gxRuntime::resetInput() {
	if(input) input->reset();
}

void gxRuntime::acquireInput() {
	if(!input) return;
	if(gfx_mode == GMODE_EXCLUSIVE) {
		if(use_di) {
			use_di = input->acquire();
		}
		else {
		}
	}
	input->reset();
}

void gxRuntime::unacquireInput() {
	if(!input) return;
	if(gfx_mode == GMODE_EXCLUSIVE && use_di) input->unacquire();
	input->reset();
}

/////////////
// SUSPEND //
/////////////
void gxRuntime::suspend() {
	busy = true;
	pauseAudio();
	unacquireInput();
	suspended = true;
	busy = false;

	if(gfx_mode == GMODE_EXCLUSIVE) ShowCursor(1);

	if(debugger) debugger->debugStop();
}

////////////
// RESUME //
////////////
void gxRuntime::resume() {
	if(gfx_mode == GMODE_EXCLUSIVE) ShowCursor(0);
	busy = true;
	acquireInput();
	restoreGraphics();
	resumeAudio();
	suspended = false;
	busy = false;

	if(debugger) debugger->debugRun();
}

///////////////////
// FORCE SUSPEND //
///////////////////
void gxRuntime::forceSuspend() {
	if(gfx_mode == GMODE_EXCLUSIVE) {
		SetForegroundWindow(GetDesktopWindow());
		ShowWindow(GetDesktopWindow(), SW_SHOW);
	}
	else {
		suspend();
	}
}

//////////////////
// FORCE RESUME //
//////////////////
void gxRuntime::forceResume() {
	if(gfx_mode == GMODE_EXCLUSIVE) {
		SetForegroundWindow(hwnd);
		ShowWindow(hwnd, SW_SHOWMAXIMIZED);
	}
	else {
		resume();
	}
}

///////////
// PAINT //
///////////
void gxRuntime::paint() {
	switch(gfx_mode) {
		case GMODE_SCALED:
		case GMODE_FIXED:
		{
			RECT src, dest;
			src.left = src.top = 0;
			GetClientRect(hwnd, &dest);
			src.right = gfx_mode == GMODE_SCALED ? graphics->getWidth() : dest.right;
			src.bottom = gfx_mode == GMODE_SCALED ? graphics->getHeight() : dest.bottom;
			POINT p; p.x = p.y = 0; ClientToScreen(hwnd, &p);
			dest.left += p.x; dest.right += p.x;
			dest.top += p.y; dest.bottom += p.y;
			gxCanvas* f = graphics->getFrontCanvas();
			primSurf->Blt(&dest, f->getSurface(), &src, 0, 0);
			break;
		}
	}
}

//////////
// FLIP //
//////////
void gxRuntime::flip(bool vwait) {
	gxCanvas* b = graphics->getBackCanvas();
	gxCanvas* f = graphics->getFrontCanvas();
	int n;
	switch(gfx_mode) {
		case GMODE_SCALED:
		case GMODE_FIXED:
			if(vwait) graphics->vwait();
			f->setModify(b->getModify());
			if(f->getModify() != mod_cnt) {
				mod_cnt = f->getModify();
				paint();
			}
			break;
		case GMODE_EXCLUSIVE:
			if(vwait) {
				BOOL vb;
				while(graphics->dirDraw->GetVerticalBlankStatus(&vb) >= 0 && vb) {}
				n = f->getSurface()->Flip(0, DDFLIP_WAIT);
			}
			else {
				n = f->getSurface()->Flip(0, DDFLIP_NOVSYNC | DDFLIP_WAIT);
			}
			if(n >= 0) return;
			debugLog(("Flip Failed! Return code:" + itoa(n & 0x7fff)).c_str());
			break;
	}
}

////////////////
// MOVE MOUSE //
////////////////
void gxRuntime::moveMouse(int x, int y) {
	POINT p;
	RECT rect;
	switch(gfx_mode) {
		case GMODE_SCALED:
			GetClientRect(hwnd, &rect);
			x = x * (rect.right - rect.left) / graphics->getWidth();
			y = y * (rect.bottom - rect.top) / graphics->getHeight();
		case GMODE_FIXED:
			p.x = x; p.y = y; ClientToScreen(hwnd, &p); x = p.x; y = p.y;
			break;
		case GMODE_EXCLUSIVE:
			if(use_di) return;
			break;
		default:
			return;
	}
	SetCursorPos(x, y);
}

/////////////////
// WINDOW PROC //
/////////////////
LRESULT gxRuntime::windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	if(busy) {
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	PAINTSTRUCT ps;

	//handle 'special' messages!
	switch(msg) {
		case WM_PAINT:
			if(gfx_mode && !auto_suspend) {
				if(!graphics->restore()) gfx_lost = true;
			}
			BeginPaint(hwnd, &ps);
			paint();
			EndPaint(hwnd, &ps);
			return DefWindowProc(hwnd, msg, wparam, lparam);
		case WM_ERASEBKGND:
			return gfx_mode ? GMODE_SCALED : DefWindowProc(hwnd, msg, wparam, lparam);
		case WM_CLOSE:
			if(app_close.size()) {
				int n = MessageBox(hwnd, app_close.c_str(), app_title.c_str(), MB_OKCANCEL | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
				if(n != IDOK) return 0;
			}
			asyncEnd();
			return 0;
		case WM_SETCURSOR:
			if(!suspended) {
				if(gfx_mode == GMODE_EXCLUSIVE) {
					SetCursor(0);
					return 1;
				}
				else if(!pointer_visible) {
					POINT p;
					GetCursorPos(&p);
					ScreenToClient(hwnd, &p);
					RECT r; GetClientRect(hwnd, &r);
					if(p.x >= 0 && p.y >= 0 && p.x < r.right && p.y < r.bottom) {
						SetCursor(0);
						return 1;
					}
				}
			}
			break;
		case WM_ACTIVATEAPP:
			if(auto_suspend) {
				if(wparam) {
					if(suspended) resume();
				}
				else {
					if(!suspended) suspend();
				}
			}
			break;
	}

	if(!input || suspended) return DefWindowProc(hwnd, msg, wparam, lparam);

	if(gfx_mode == GMODE_EXCLUSIVE && use_di) {
		use_di = input->acquire();
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	static const int MK_ALLBUTTONS = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON;

	//handle input messages
	switch(msg) {
		case WM_LBUTTONDOWN:
			input->wm_mousedown(1);
			SetCapture(hwnd);
			break;
		case WM_LBUTTONUP:
			input->wm_mouseup(1);
			if(!(wparam & MK_ALLBUTTONS)) ReleaseCapture();
			break;
		case WM_RBUTTONDOWN:
			input->wm_mousedown(2);
			SetCapture(hwnd);
			break;
		case WM_RBUTTONUP:
			input->wm_mouseup(2);
			if(!(wparam & MK_ALLBUTTONS)) ReleaseCapture();
			break;
		case WM_MBUTTONDOWN:
			input->wm_mousedown(3);
			SetCapture(hwnd);
			break;
		case WM_MBUTTONUP:
			input->wm_mouseup(3);
			if(!(wparam & MK_ALLBUTTONS)) ReleaseCapture();
			break;
		case WM_MOUSEMOVE:
			if(!graphics) break;
			if(gfx_mode == GMODE_EXCLUSIVE && !use_di) {
				POINT p; GetCursorPos(&p);
				input->wm_mousemove(p.x, p.y);
			}
			else {
				int x = (short)(lparam & 0xffff), y = lparam >> 16;
				if(gfx_mode == GMODE_SCALED) {
					RECT rect; GetClientRect(hwnd, &rect);
					x = x * graphics->getWidth() / (rect.right - rect.left);
					y = y * graphics->getHeight() / (rect.bottom - rect.top);
				}
				if(x < 0) x = 0;
				else if(x >= graphics->getWidth()) x = graphics->getWidth() - 1;
				if(y < 0) y = 0;
				else if(y >= graphics->getHeight()) y = graphics->getHeight() - 1;
				input->wm_mousemove(x, y);
			}
			break;
		case WM_MOUSEWHEEL:
			input->wm_mousewheel((short)HIWORD(wparam));
			break;
		case WM_KEYDOWN:case WM_SYSKEYDOWN:
			if(lparam & 0x40000000) break;
			if(int n = ((lparam >> 17) & 0x80) | ((lparam >> 16) & 0x7f)) input->wm_keydown(n);
			break;
		case WM_KEYUP:case WM_SYSKEYUP:
			if(int n = ((lparam >> 17) & 0x80) | ((lparam >> 16) & 0x7f)) input->wm_keyup(n);
			break;
		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return 0;
}

static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if(runtime) return runtime->windowProc(hwnd, msg, wparam, lparam);
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//////////////////////////////
//STOP FROM EXTERNAL SOURCE //
//////////////////////////////
void gxRuntime::asyncStop() {
	PostMessage(hwnd, WM_STOP, 0, 0);
}

//////////////////////////////
//RUN  FROM EXTERNAL SOURCE //
//////////////////////////////
void gxRuntime::asyncRun() {
	PostMessage(hwnd, WM_RUN, 0, 0);
}

//////////////////////////////
// END FROM EXTERNAL SOURCE //
//////////////////////////////
void gxRuntime::asyncEnd() {
	PostMessage(hwnd, WM_END, 0, 0);
}

//////////
// IDLE //
//////////
bool gxRuntime::idle() {
	for(;;) {
		MSG msg;
		BOOL success = 0;
		if(suspended && run_flag) {
			success = GetMessageW(&msg, 0, 0, 0);
		}
		else {
			if(!PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) return run_flag;
		}
		switch(msg.message) {
			case WM_STOP:
				if(!suspended) forceSuspend();
				break;
			case WM_RUN:
				if(suspended) forceResume();
				break;
			case WM_END:
				debugger = 0;
				run_flag = false;
				break;
			case WM_CHAR:
				input->wm_char(msg.wParam, msg.lParam);
				break;
			default:
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
		}
	}
	return run_flag;
}

///////////
// DELAY //
///////////
bool gxRuntime::delay(int ms) {
	int t = timeGetTime() + ms;
	for(;;) {
		if(!idle()) return false;
		int d = t - timeGetTime();	//how long left to wait
		if(d <= 0) return true;
		if(d > 100) d = 100;
		Sleep(d);
	}
}

///////////////
// DEBUGSTMT //
///////////////
bool gxRuntime::debugStmt(int pos, const char* file) {
	return debugger ? debugger->debugStmt(pos, file) : true;
}

///////////////
// DEBUGSTOP //
///////////////
void gxRuntime::debugStop() {
	if(!suspended) forceSuspend();
}

////////////////
// DEBUGENTER //
////////////////
void gxRuntime::debugEnter(void* frame, void* env, const char* func) {
	if(debugger) debugger->debugEnter(frame, env, func);
}

////////////////
// DEBUGLEAVE //
////////////////
void gxRuntime::debugLeave() {
	if(debugger) debugger->debugLeave();
}

////////////////
// DEBUGERROR //
////////////////
void gxRuntime::debugError(const char* t) {
	if(!debugger) return;
	Debugger* d = debugger;
	asyncEnd();
	if(!suspended) {
		forceSuspend();
	}
	d->debugMsg(UTF8::convertToUtf8(t).c_str(), true);
}

///////////////
// DEBUGINFO //
///////////////
void gxRuntime::debugInfo(const char* t) {
	if(!debugger) return;
	Debugger* d = debugger;
	asyncEnd();
	if(!suspended) {
		forceSuspend();
	}
	d->debugMsg(UTF8::convertToUtf8(t).c_str(), false);
}

//////////////
// DEBUGLOG //
//////////////
void gxRuntime::debugLog(const char* t) {
	if(debugger) debugger->debugLog(t);
}

/////////////////////////
// RETURN COMMAND LINE //
/////////////////////////
std::string gxRuntime::commandLine() {
	return cmd_line;
}

/////////////
// EXECUTE //
/////////////
bool gxRuntime::execute(const std::string& cmd_line) {

	if(!cmd_line.size()) return false;

	//convert cmd_line to cmd and params
	std::string cmd = cmd_line, params;
	while(cmd.size() && cmd[0] == ' ') cmd = cmd.substr(1);
	if(cmd.find('\"') == 0) {
		int n = cmd.find('\"', 1);
		if(n != std::string::npos) {
			params = cmd.substr(n + 1);
			cmd = cmd.substr(1, n - 1);
		}
	}
	else {
		int n = cmd.find(' ');
		if(n != std::string::npos) {
			params = cmd.substr(n + 1);
			cmd = cmd.substr(0, n);
		}
	}
	while(params.size() && params[0] == ' ') params = params.substr(1);
	while(params.size() && params[params.size() - 1] == ' ') params = params.substr(0, params.size() - 1);

	SetForegroundWindow(GetDesktopWindow());
	return (int)ShellExecute(GetDesktopWindow(), 0, cmd.c_str(), params.size() ? params.c_str() : 0, 0, SW_SHOW) > 32;
}

///////////////
// APP TITLE //
///////////////
void gxRuntime::setTitle(const std::string& t, const std::string& e) {
	app_title = t;
	app_close = e;
	SetWindowTextW(hwnd, UTF8::convertToUtf16(app_title).c_str());
}

//////////////////
// GETMILLISECS //
//////////////////
int gxRuntime::getMilliSecs() {
	return timeGetTime() & 0x7FFFFFFF;
}

////////////////
// MEMORYINFO //
////////////////
int gxRuntime::getMemoryLoad() {
	GlobalMemoryStatusEx(&statex);
	return statex.dwMemoryLoad;
}

int gxRuntime::getTotalPhys() {
	return statex.ullTotalPhys / 1024;
}

int gxRuntime::getAvailPhys() {
	return statex.ullAvailPhys / 1024;
}

int gxRuntime::getTotalVirtual() {
	return statex.ullTotalVirtual / 1024;
}

int gxRuntime::getAvailVirtual() {
	return statex.ullAvailVirtual / 1024;
}

/////////////////////
// POINTER VISIBLE //
/////////////////////
void gxRuntime::setPointerVisible(bool vis) {
	if(pointer_visible == vis) return;

	pointer_visible = vis;
	if(gfx_mode == GMODE_EXCLUSIVE) return;

	//force a WM_SETCURSOR
	POINT pt;
	GetCursorPos(&pt);
	SetCursorPos(pt.x, pt.y);
}

/////////////////
// INPUT SETUP //
/////////////////
gxInput* gxRuntime::openInput(int flags) {
	if(input) return 0;

	IDirectInput8* di;
	if(DirectInput8Create(hinst, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, 0) >= 0) {
		input = new gxInput(this, di);
		acquireInput();
	}
	else {
		runtime->debugLog("Failed to create DirectInput.");
	}
	return input;
}

void gxRuntime::closeInput(gxInput* i) {
	if(!input || input != i) return;
	unacquireInput();
	delete input;
	input = 0;
}

/////////////////////////////////////////////////////
// TIMER CALLBACK FOR AUTOREFRESH OF WINDOWED MODE //
/////////////////////////////////////////////////////
static void CALLBACK timerCallback(UINT id, UINT msg, DWORD user, DWORD dw1, DWORD dw2) {
	if(gfx_mode) {
		gxCanvas* f = runtime->graphics->getFrontCanvas();
		if(f->getModify() != mod_cnt) {
			mod_cnt = f->getModify();
			InvalidateRect(runtime->hwnd, 0, false);
		}
	}
}

////////////////////
// GRAPHICS SETUP //
////////////////////
void gxRuntime::backupWindowState() {
	GetWindowRect(hwnd, &t_rect);
	t_style = GetWindowLong(hwnd, GWL_STYLE);
}

void gxRuntime::restoreWindowState() {
	SetWindowLong(hwnd, GWL_STYLE, t_style);
	SetWindowPos(
		hwnd, 0, t_rect.left, t_rect.top,
		t_rect.right - t_rect.left, t_rect.bottom - t_rect.top,
		SWP_NOZORDER | SWP_FRAMECHANGED);
}

bool gxRuntime::setDisplayMode(int w, int h, int d, bool d3d, IDirectDraw7* dirDraw) {

	if(d) return dirDraw->SetDisplayMode(w, h, d, 0, 0) >= 0;

	int best_d = 0;

	if(d3d) {
		int bd = curr_driver->d3d_desc.dwDeviceRenderBitDepth;
		if(bd & DDBD_32) best_d = 32;
		else if(bd & DDBD_24) best_d = 24;
		else if(bd & DDBD_16) best_d = 16;
	}
	else {
		int best_n = 0;
		for(d = 16; d <= 32; d += 8) {
			if(dirDraw->SetDisplayMode(w, h, d, 0, 0) < 0) continue;
			DDCAPS caps = { sizeof(caps) };
			dirDraw->GetCaps(&caps, 0);
			int n = 0;
			if(caps.dwCaps & DDCAPS_BLT) ++n;
			if(caps.dwCaps & DDCAPS_BLTCOLORFILL) ++n;
			if(caps.dwCKeyCaps & DDCKEYCAPS_SRCBLT) ++n;
			if(caps.dwCaps2 & DDCAPS2_WIDESURFACES) ++n;
			if(n == 4) return true;
			if(n > best_n) {
				best_d = d;
				best_n = n;
			}
			dirDraw->RestoreDisplayMode();
		}
	}
	return best_d ? dirDraw->SetDisplayMode(w, h, best_d, 0, 0) >= 0 : false;
}

gxGraphics* gxRuntime::openWindowedGraphics(int w, int h, int d, bool d3d) {

	IDirectDraw7* dd;
	if(DirectDrawCreateEx(curr_driver->guid, (void**)&dd, IID_IDirectDraw7, 0) < 0) return 0;

	//set coop level
	if(dd->SetCooperativeLevel(hwnd, DDSCL_NORMAL) >= 0) {
		//create primary surface
		IDirectDrawSurface7* ps;
		DDSURFACEDESC2 desc = { sizeof(desc) };
		desc.dwFlags = DDSD_CAPS;
		desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		if(dd->CreateSurface(&desc, &ps, 0) >= 0) {
			//create clipper
			IDirectDrawClipper* cp;
			if(dd->CreateClipper(0, &cp, 0) >= 0) {
				//attach clipper
				if(ps->SetClipper(cp) >= 0) {
					//set clipper HWND
					if(cp->SetHWnd(0, hwnd) >= 0) {
						//create front buffer
						IDirectDrawSurface7* fs;
						DDSURFACEDESC2 desc = { sizeof(desc) };
						desc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
						desc.dwWidth = w; desc.dwHeight = h;
						desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

						if(d3d) desc.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE;

						if(dd->CreateSurface(&desc, &fs, 0) >= 0) {
							if(timerID = timeSetEvent(100, 10, timerCallback, 0, TIME_PERIODIC)) {
								//Success!
								clipper = cp;
								primSurf = ps;
								mod_cnt = 0;
								fs->AddRef();
								return new gxGraphics(this, dd, fs, fs, d3d);
							}
							fs->Release();
						}
					}
				}
				cp->Release();
			}
			ps->Release();
		}
	}
	dd->Release();
	return 0;
}

gxGraphics* gxRuntime::openExclusiveGraphics(int w, int h, int d, bool d3d) {

	IDirectDraw7* dd;
	if(DirectDrawCreateEx(curr_driver->guid, (void**)&dd, IID_IDirectDraw7, 0) < 0) return 0;

	//Set coop level
	if(dd->SetCooperativeLevel(hwnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT) >= 0) {
		//Set display mode
		if(setDisplayMode(w, h, d, d3d, dd)) {
			//create primary surface
			IDirectDrawSurface7* ps;
			DDSURFACEDESC2 desc = { sizeof(desc) };
			desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
			desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;

			desc.dwBackBufferCount = 1;
			if(d3d) desc.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE;

			if(dd->CreateSurface(&desc, &ps, 0) >= 0) {
				//find back surface
				IDirectDrawSurface7* bs;
				DDSCAPS2 caps = { sizeof caps };
				caps.dwCaps = DDSCAPS_BACKBUFFER;
				if(ps->GetAttachedSurface(&caps, &bs) >= 0) {
					return new gxGraphics(this, dd, ps, bs, d3d);
				}
				ps->Release();
			}
			dd->RestoreDisplayMode();
		}
	}
	dd->Release();
	return 0;
}

gxGraphics* gxRuntime::openGraphics(int w, int h, int d, int driver, int flags) {
	if(graphics) return 0;

	busy = true;

	bool d3d = flags & gxGraphics::GRAPHICS_3D ? true : false;
	bool windowed = flags & gxGraphics::GRAPHICS_WINDOWED ? true : false;

	curr_driver = drivers[driver];

	if(windowed) {
		if(graphics = openWindowedGraphics(w, h, d, d3d)) {
			gfx_mode = (flags & gxGraphics::GRAPHICS_SCALED) ? GMODE_SCALED : GMODE_FIXED;
			auto_suspend = (flags & gxGraphics::GRAPHICS_AUTOSUSPEND) ? true : false;
			int ws, ww, hh;
			border_mode = (flags & gxGraphics::GRAPHICS_BORDERLESS) ? 1 : 0;
			if(gfx_mode == GMODE_SCALED) {
				ws = scaled_ws;
				RECT c_r;
				GetClientRect(hwnd, &c_r);
				ww = c_r.right - c_r.left;
				hh = c_r.bottom - c_r.top;
			}
			else {
				ws = static_ws;
				ww = w;
				hh = h;
			}

			if(border_mode == 1) {
				SetWindowLong(hwnd, GWL_STYLE, WS_POPUP);
				SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW);
			}
			else {
				SetWindowLong(hwnd, GWL_STYLE, ws);
				SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			}

			RECT w_r, c_r;
			GetWindowRect(hwnd, &w_r);
			GetClientRect(hwnd, &c_r);
			int tw = (w_r.right - w_r.left) - (c_r.right - c_r.left);
			int th = (w_r.bottom - w_r.top) - (c_r.bottom - c_r.top);
			int cx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
			int cy = (GetSystemMetrics(SM_CYSCREEN) - hh) / 2;
			POINT zz = { 0,0 };
			ClientToScreen(hwnd, &zz);
			int bw = zz.x - w_r.left, bh = zz.y - w_r.top;
			int wx = cx - bw, wy = cy - bh; if(wy < 0) wy = 0;		//not above top!
			MoveWindow(hwnd, wx, wy, ww + tw, hh + th, true);
		}
	}
	else {
		backupWindowState();

		SetWindowLong(hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
		SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

		ShowCursor(0);
		if(graphics = openExclusiveGraphics(w, h, d, d3d)) {
			gfx_mode = GMODE_EXCLUSIVE;
			auto_suspend = true;
			SetCursorPos(0, 0);
			acquireInput();
		}
		else {
			ShowCursor(1);
			restoreWindowState();
		}
	}

	if(!graphics) curr_driver = 0;

	gfx_lost = false;

	busy = false;

	return graphics;
}

void gxRuntime::closeGraphics(gxGraphics* g) {
	if(!graphics || graphics != g) return;

	auto_suspend = false;

	busy = true;

	unacquireInput();
	if(timerID) { timeKillEvent(timerID); timerID = 0; }
	if(clipper) { clipper->Release(); clipper = 0; }
	if(primSurf) { primSurf->Release(); primSurf = 0; }
	delete graphics; graphics = 0;

	if(gfx_mode == GMODE_EXCLUSIVE) {
		ShowCursor(1);
		restoreWindowState();
	}
	gfx_mode = GMODE_NONE;

	gfx_lost = false;

	busy = false;
}

bool gxRuntime::graphicsLost() {
	return gfx_lost;
}

bool gxRuntime::focus() {
	//return suspended;
	return GetFocus();
}

int gxRuntime::desktopWidth() {
	//	return GetSystemMetrics(SM_CXSCREEN);
	return devmode.dmPelsWidth;
}

int gxRuntime::desktopHeight() {
	//	return GetSystemMetrics(SM_CYSCREEN);
	return devmode.dmPelsHeight;
}

gxFileSystem* gxRuntime::openFileSystem(int flags) {
	if(fileSystem) return 0;

	fileSystem = new gxFileSystem();
	return fileSystem;
}

void gxRuntime::closeFileSystem(gxFileSystem* f) {
	if(!fileSystem || fileSystem != f) return;

	delete fileSystem;
	fileSystem = 0;
}

////////////////////
// GFX ENUM STUFF //
////////////////////
static HRESULT WINAPI enumMode(DDSURFACEDESC2* desc, void* context) {
	int dp = desc->ddpfPixelFormat.dwRGBBitCount;
	if(dp == 16 || dp == 24 || dp == 32) {
		gxRuntime::GfxMode* m = new gxRuntime::GfxMode;
		m->desc = *desc;
		gxRuntime::GfxDriver* d = (gxRuntime::GfxDriver*)context;
		d->modes.push_back(m);
	}
	return DDENUMRET_OK;
}

static int maxDevType;
static HRESULT CALLBACK enumDevice(char* desc, char* name, D3DDEVICEDESC7* devDesc, void* context) {
	int t = 0;
	GUID guid = devDesc->deviceGUID;
	if(guid == IID_IDirect3DRGBDevice) t = 1;
	else if(guid == IID_IDirect3DHALDevice) t = 2;
	else if(guid == IID_IDirect3DTnLHalDevice) t = 3;
	if(t > 1 && t > maxDevType) {
		maxDevType = t;
		gxRuntime::GfxDriver* d = (gxRuntime::GfxDriver*)context;
		d->d3d_desc = *devDesc;
	}
	return D3DENUMRET_OK;
}

static BOOL WINAPI enumDriver(GUID FAR* guid, LPSTR desc, LPSTR name, LPVOID context, HMONITOR hm) {
	IDirectDraw7* dd;
	if(DirectDrawCreateEx(guid, (void**)&dd, IID_IDirectDraw7, 0) < 0) return 0;

	if(!guid && !desktop_desc.ddpfPixelFormat.dwRGBBitCount) {
		desktop_desc.dwSize = sizeof(desktop_desc);
		dd->GetDisplayMode(&desktop_desc);
	}

	gxRuntime::GfxDriver* d = new gxRuntime::GfxDriver;

	d->guid = guid ? new GUID(*guid) : 0;
	d->name = desc;

	memset(&d->d3d_desc, 0, sizeof(d->d3d_desc));
	IDirect3D7* dir3d;
	if(dd->QueryInterface(IID_IDirect3D7, (void**)&dir3d) >= 0) {
		maxDevType = 0;
		dir3d->EnumDevices(enumDevice, d);
		dir3d->Release();
	}
	std::vector<gxRuntime::GfxDriver*>* drivers = (std::vector<gxRuntime::GfxDriver*>*)context;
	drivers->push_back(d);
	dd->EnumDisplayModes(0, 0, d, enumMode);
	dd->Release();
	return 1;
}

void gxRuntime::enumGfx() {
	denumGfx();
	if(enum_all) {
		DirectDrawEnumerateEx(enumDriver, &drivers, DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_NONDISPLAYDEVICES);
	}
	else {
		DirectDrawEnumerateEx(enumDriver, &drivers, 0);
	}
}

void gxRuntime::denumGfx() {
	for(int k = 0; k < drivers.size(); ++k) {
		gxRuntime::GfxDriver* d = drivers[k];
		for(int j = 0; j < d->modes.size(); ++j) delete d->modes[j];
		delete d->guid;
		delete d;
	}
	drivers.clear();
}

int gxRuntime::numGraphicsDrivers() {
	if(!enum_all) {
		enum_all = true;
		enumGfx();
	}
	return drivers.size();
}

void gxRuntime::graphicsDriverInfo(int driver, std::string* name, int* c) {
	GfxDriver* g = drivers[driver];
	int caps = 0;
	if(g->d3d_desc.dwDeviceRenderBitDepth) caps |= GFXMODECAPS_3D;
	*name = g->name;
	*c = caps;
}

int gxRuntime::numGraphicsModes(int driver) {
	return drivers[driver]->modes.size();
}

void gxRuntime::graphicsModeInfo(int driver, int mode, int* w, int* h, int* d, int* c) {
	GfxDriver* g = drivers[driver];
	GfxMode* m = g->modes[mode];
	int caps = 0;
	int bd = 0;
	switch(m->desc.ddpfPixelFormat.dwRGBBitCount) {
		case 16:bd = DDBD_16; break;
		case 24:bd = DDBD_24; break;
		case 32:bd = DDBD_32; break;
	}
	if(g->d3d_desc.dwDeviceRenderBitDepth & bd) caps |= GFXMODECAPS_3D;
	*w = m->desc.dwWidth;
	*h = m->desc.dwHeight;
	*d = m->desc.ddpfPixelFormat.dwRGBBitCount;
	*c = caps;
}

void gxRuntime::windowedModeInfo(int* c) {
	int caps = 0;
	int bd = 0;
	switch(desktop_desc.ddpfPixelFormat.dwRGBBitCount) {
		case 16:bd = DDBD_16; break;
		case 24:bd = DDBD_24; break;
		case 32:bd = DDBD_32; break;
	}
	if(drivers[0]->d3d_desc.dwDeviceRenderBitDepth & bd) caps |= GFXMODECAPS_3D;
	*c = caps;
}

gxTimer* gxRuntime::createTimer(int hertz) {
	gxTimer* t = new gxTimer(this, hertz);
	timers.insert(t);
	return t;
}

void gxRuntime::freeTimer(gxTimer* t) {
	if(!timers.count(t)) return;
	timers.erase(t);
	delete t;
}

static std::string toDir(std::string t) {
	if(t.size() && t[t.size() - 1] != '\\') t += '\\';
	return t;
}

std::string gxRuntime::systemProperty(const std::string& p) {
	char buff[MAX_PATH + 1];
	std::string t = tolower(p);
	if(t == "os") {
		switch(osinfo.dwMajorVersion) {
			case 6:
				switch(osinfo.dwMinorVersion) {
					case 0:return "Windows Vista";
					case 1:return "Windows 7";
					case 2:return "Windows 8";
					case 3:return "Windows 8.1";
				}
				break;
			case 10:
				if(osinfo.dwBuildNumber >= 22000) return "Windows 11";
				return "Windows 10";
				break;
		}
	}
	else if(t == "cpuname") {
		//Uses the __cpuid intrinsic to get the brand name.
		//-------RESOURCES-------
		//https://en.wikipedia.org/wiki/CPUID#EAX=80000002h,80000003h,80000004h:_Processor_Brand_String
		//https://docs.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-160

		std::string cpuBrand;
		uint32_t regs[4];
		int numberOfExtendedFlags;

		__cpuid((int*)regs, 0x80000000);
		numberOfExtendedFlags = regs[0];

		if(numberOfExtendedFlags >= 0x80000004) {
			__cpuid((int*)regs, 0x80000002);
			cpuBrand += std::string((const char*)&regs[0], 4);
			cpuBrand += std::string((const char*)&regs[1], 4);
			cpuBrand += std::string((const char*)&regs[2], 4);
			cpuBrand += std::string((const char*)&regs[3], 4);

			__cpuid((int*)regs, 0x80000003);
			cpuBrand += std::string((const char*)&regs[0], 4);
			cpuBrand += std::string((const char*)&regs[1], 4);
			cpuBrand += std::string((const char*)&regs[2], 4);
			cpuBrand += std::string((const char*)&regs[3], 4);

			__cpuid((int*)regs, 0x80000004);
			cpuBrand += std::string((const char*)&regs[0], 4);
			cpuBrand += std::string((const char*)&regs[1], 4);
			cpuBrand += std::string((const char*)&regs[2], 4);
			cpuBrand += std::string((const char*)&regs[3], 4);
		}
		else cpuBrand = getenv("PROCESSOR_IDENTIFIER"); //Should never happen, modern CPUs implement the brand name.

		return cpuBrand;
	}
	else if(t == "cpuarch") {
		SYSTEM_INFO systemInfo;
		GetNativeSystemInfo(&systemInfo);

		switch(systemInfo.wProcessorArchitecture) {
			case PROCESSOR_ARCHITECTURE_AMD64:
				return "AMD64";
			case PROCESSOR_ARCHITECTURE_INTEL:
				return "x86";
				//Maybe someone runs the game under the x86 emulation layers of ARM Windows, detect it.
			case PROCESSOR_ARCHITECTURE_ARM:
				return "ARM32";
			case PROCESSOR_ARCHITECTURE_ARM64:
				return "ARM64";
				//-------------------------------------------------------------------------------------
			default:
				return "Unknown";
		}
	}
	else if(t == "osbuild") {
		return itoa((int)osinfo.dwBuildNumber);
	}
	else if(t == "appdir") {
		if(GetModuleFileName(0, buff, MAX_PATH)) {
			std::string t = buff;
			int n = t.find_last_of('\\');
			if(n != std::string::npos) t = t.substr(0, n);
			return toDir(t);
		}
	}
	else if(t == "appfile") {
		if(GetModuleFileName(0, buff, MAX_PATH)) return buff; //without toDir, so we don't have the slash at the end
	}
	else if(t == "apphwnd") {
		return itoa((int)hwnd);
	}
	else if(t == "apphinstance") {
		return itoa((int)hinst);
	}
	else if(t == "windowsdir") {
		if(GetWindowsDirectory(buff, MAX_PATH)) return toDir(buff);
	}
	else if(t == "systemdir") {
		if(GetSystemDirectory(buff, MAX_PATH)) return toDir(buff);
	}
	else if(t == "tempdir") {
		if(GetTempPath(MAX_PATH, buff)) return toDir(buff);
	}
	else if(t == "direct3d7") {
		if(graphics) return itoa((int)graphics->dir3d);
	}
	else if(t == "direct3ddevice7") {
		if(graphics) return itoa((int)graphics->dir3dDev);
	}
	else if(t == "directdraw7") {
		if(graphics) return itoa((int)graphics->dirDraw);
	}
	else if(t == "directinput7") {
		if(input) return itoa((int)input->dirInput);
	}
	else if(t == "blitzversion") {
		return itoa((VERSION & 0xffff) / 1000) + "." + itoa((VERSION & 0xffff) % 1000);
	}
	return "";
}

void gxRuntime::calculateDPI() {
	if ((this->scale_x == .0f) && (this->scale_y == .0f)) {
		HDC hdc = GetDC(GetDesktopWindow());
		this->scale_x = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
		this->scale_y = GetDeviceCaps(hdc, LOGPIXELSY) / 96.0f;
		ReleaseDC(GetDesktopWindow(), hdc);
	}
}

void gxRuntime::enableDirectInput(bool enable) {
	if(use_di = enable) {
		acquireInput();
	}
	else {
		unacquireInput();
	}
}

int gxRuntime::callDll(const std::string& dll, const std::string& func, const void* in, int in_sz, void* out, int out_sz) {

	std::map<std::string, gxDll*>::const_iterator lib_it = libs.find(dll);

	if(lib_it == libs.end()) {
		HINSTANCE h = LoadLibrary(dll.c_str());
		if(!h) return 0;
		gxDll* t = new gxDll;
		t->hinst = h;
		lib_it = libs.insert(make_pair(dll, t)).first;
	}

	gxDll* t = lib_it->second;
	std::map<std::string, LibFunc>::const_iterator fun_it = t->funcs.find(func);

	if(fun_it == t->funcs.end()) {
		LibFunc f = (LibFunc)GetProcAddress(t->hinst, func.c_str());
		if(!f) return 0;
		fun_it = t->funcs.insert(make_pair(func, f)).first;
	}

	static void* save_esp;

	_asm {
		mov[save_esp], esp
	};

	int n = fun_it->second(in, in_sz, out, out_sz);

	_asm {
		mov esp, [save_esp]
	};

	return n;
}