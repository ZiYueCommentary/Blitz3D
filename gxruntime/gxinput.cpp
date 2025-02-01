#include "std.h"
#include "gxinput.h"
#include "gxruntime.h"

#include <dinput.h>
#include <xinput.h>

static const int QUE_SIZE = 32;

XInputGetStatePtr XInputGetStateFunc = nullptr;
XInputSetStatePtr XInputSetStateFunc = nullptr;
HMODULE xinputLibrary = nullptr;

void UnloadXInput() {
	if (xinputLibrary) {
		FreeLibrary(xinputLibrary);
		xinputLibrary = nullptr;
		XInputGetStateFunc = nullptr;
		XInputSetStateFunc = nullptr;
	}
}

bool LoadXInput() {
	// Try to load xinput1_4.dll (Windows 8 and later) for analysis
	xinputLibrary = LoadLibraryW(L"xinput1_4.dll");
	if (!xinputLibrary) {
		// Fall back to xinput9_1_0.dll (Windows 7 and earlier) for analysis
		xinputLibrary = LoadLibraryW(L"xinput9_1_0.dll");
	}

	// Once we're able to get the dll loaded, check for specific function pointer addresses
	if (xinputLibrary) {
		XInputGetStateFunc = (XInputGetStatePtr)GetProcAddress(xinputLibrary, "XInputGetState");
		XInputSetStateFunc = (XInputSetStatePtr)GetProcAddress(xinputLibrary, "XInputSetState");
		if (XInputGetStateFunc && XInputSetStateFunc) {
			// All good and defined, return success
			return true;
		}
		// Only unload here since it wouldn't succeed it if we didn't get here in the first place
		UnloadXInput();
	}
	return false;
}

class Device : public gxDevice {
public:
	bool acquired;
	gxInput* input;
	IDirectInputDevice8* device;

	Device(gxInput* i, IDirectInputDevice8* d) :input(i), acquired(false), device(d) {
	}
	virtual ~Device() {
		if(device) device->Release();
	}
	bool acquire() {
		if(device) return acquired = device->Acquire() >= 0;
		return false;
	}
	void unacquire() {
		if(device) device->Unacquire();
		acquired = false;
	}
};

class Keyboard : public Device {
public:
	Keyboard(gxInput* i, IDirectInputDevice8* d) :Device(i, d) {
	}
	void update() {

		if(!device) return;

		if(!acquired) {
			input->runtime->idle();
			return;
		}
		int k, cnt = 32;
		DIDEVICEOBJECTDATA data[32], * curr;
		if(device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), data, (DWORD*)&cnt, 0) < 0) return;
		curr = data;
		for(k = 0; k < cnt; ++curr, ++k) {
			int n = curr->dwOfs; if(!n || n > 255) continue;
			if(curr->dwData & 0x80) downEvent(n);
			else upEvent(n);
		}
	}
};

class Mouse : public Device {
public:
	Mouse(gxInput* i, IDirectInputDevice8* d) :Device(i, d) {
	}
	void update() {

		if(!device) return;

		if(!acquired) {
			input->runtime->idle();
			return;
		}
		DIMOUSESTATE state;
		if(device->GetDeviceState(sizeof(state), &state) < 0) return;
		if(gxGraphics* g = input->runtime->graphics) {
			int mx = axis_states[0] + state.lX;
			int my = axis_states[1] + state.lY;
			if(mx < 0) mx = 0;
			else if(mx >= g->getWidth()) mx = g->getWidth() - 1;
			if(my < 0) my = 0;
			else if(my >= g->getHeight()) my = g->getHeight() - 1;
			axis_states[0] = mx;
			axis_states[1] = my;
			axis_states[2] += state.lZ;
		}
		for(int k = 0; k < 3; ++k) {
			setDownState(k + 1, state.rgbButtons[k] & 0x80 ? true : false);
		}
	}
};

class Joystick : public Device {
public:
	int type, poll_time;
	int mins[12], maxs[12];
	Joystick(gxInput* i, IDirectInputDevice8* d, int t) :Device(i, d), type(t), poll_time(0) {
		for(int k = 0; k < 12; ++k) {
			//initialize joystick axis ranges (d'oh!)
			DIPROPRANGE range;
			range.diph.dwSize = sizeof(DIPROPRANGE);
			range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			range.diph.dwObj = k * 4 + 12;
			range.diph.dwHow = DIPH_BYOFFSET;
			if(d->GetProperty(DIPROP_RANGE, &range.diph) < 0) {
				mins[k] = 0;
				maxs[k] = 65535;
				continue;
			}
			mins[k] = range.lMin;
			maxs[k] = range.lMax - range.lMin;
		}
	}
	void update() {
		unsigned tm = timeGetTime();
		if(tm - poll_time < 3) return;
		if(device->Poll() < 0) {
			acquired = false;
			input->runtime->idle();
			acquire(); if(device->Poll() < 0) return;
		}
		poll_time = tm;
		DIJOYSTATE state;
		if(device->GetDeviceState(sizeof(state), &state) < 0) return;
		axis_states[0] = (state.lX - mins[0]) / (float)maxs[0] * 2 - 1;
		axis_states[1] = (state.lY - mins[1]) / (float)maxs[1] * 2 - 1;
		axis_states[2] = (state.lZ - mins[2]) / (float)maxs[2] * 2 - 1;
		axis_states[3] = (state.rglSlider[0] - mins[6]) / (float)maxs[6] * 2 - 1;
		axis_states[4] = (state.rglSlider[1] - mins[7]) / (float)maxs[7] * 2 - 1;
		axis_states[5] = (state.lRx - mins[3]) / (float)maxs[3] * 2 - 1;
		axis_states[6] = (state.lRy - mins[4]) / (float)maxs[4] * 2 - 1;
		axis_states[7] = (state.lRz - mins[5]) / (float)maxs[5] * 2 - 1;
		if((state.rgdwPOV[0] & 0xffff) == 0xffff) axis_states[8] = -1;
		else axis_states[8] = floor(state.rgdwPOV[0] / 100.0f + .5f);

		for(int k = 0; k < 31; ++k) {
			setDownState(k + 1, state.rgbButtons[k] & 0x80 ? true : false);
		}
	}
};

static Keyboard* keyboard;
static Mouse* mouse;
static std::vector<Joystick*> joysticks;
static std::vector<int> chars;

static Keyboard* createKeyboard(gxInput* input) {

	return new Keyboard(input, 0);

	IDirectInputDevice8* dev;
	if(input->dirInput->CreateDevice(GUID_SysKeyboard, (IDirectInputDevice8**)&dev, 0) >= 0) {

		if(dev->SetCooperativeLevel(input->runtime->hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE) >= 0) {

			if(dev->SetDataFormat(&c_dfDIKeyboard) >= 0) {
				DIPROPDWORD dword;
				memset(&dword, 0, sizeof(dword));
				dword.diph.dwSize = sizeof(DIPROPDWORD);
				dword.diph.dwHeaderSize = sizeof(DIPROPHEADER);
				dword.diph.dwObj = 0;
				dword.diph.dwHow = DIPH_DEVICE;
				dword.dwData = 32;
				if(dev->SetProperty(DIPROP_BUFFERSIZE, &dword.diph) >= 0) {
					return new Keyboard(input, dev);
				}
			}

			return new Keyboard(input, dev);

		}
		else {
			input->runtime->debugInfo("keyboard: SetCooperativeLevel failed");
		}
		dev->Release();
	}
	else {
		input->runtime->debugInfo("keyboard: CreateDevice failed");
	}
	return 0;
}

static Mouse* createMouse(gxInput* input) {

	return new Mouse(input, 0);

	IDirectInputDevice8* dev;
	if(input->dirInput->CreateDevice(GUID_SysMouse, (IDirectInputDevice8**)dev, 0) >= 0) {

		if(dev->SetCooperativeLevel(input->runtime->hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE) >= 0) {	//crashes on dinput8!

			return new Mouse(input, dev);

			if(dev->SetDataFormat(&c_dfDIMouse) >= 0) {
				return new Mouse(input, dev);
			}

			return new Mouse(input, dev);

		}
		else {
			input->runtime->debugInfo("mouse: SetCooperativeLevel failed");
		}
		dev->Release();
	}
	else {
		input->runtime->debugInfo("mouse: CreateDevice failed");
	}
	return 0;
}

static Joystick* createJoystick(gxInput* input, LPCDIDEVICEINSTANCE devinst) {
	IDirectInputDevice8* dev;
	if(input->dirInput->CreateDevice(devinst->guidInstance, (IDirectInputDevice8**)&dev, 0) >= 0) {
		if(dev->SetCooperativeLevel(input->runtime->hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE) >= 0) {
			if(dev->SetDataFormat(&c_dfDIJoystick) >= 0) {
				int t = ((devinst->dwDevType >> 8) & 0xff) == DI8DEVTYPE_GAMEPAD ? 1 : 2;
				return new Joystick(input, dev, t);
			}
		}
		dev->Release();
	}
	return 0;
}

static BOOL CALLBACK enumJoystick(LPCDIDEVICEINSTANCE devinst, LPVOID pvRef) {

	if((devinst->dwDevType & 0xff) != DI8DEVTYPE_JOYSTICK) return DIENUM_CONTINUE;

	if(Joystick* joy = createJoystick((gxInput*)pvRef, devinst)) {
		joysticks.push_back(joy);
	}
	return DIENUM_CONTINUE;
}

gxInput::gxInput(gxRuntime* rt, IDirectInput8* di) :
	runtime(rt), dirInput(di) {
	keyboard = createKeyboard(this);
	mouse = createMouse(this);
	joysticks.clear();
	dirInput->EnumDevices(DI8DEVTYPE_JOYSTICK, enumJoystick, this, DIEDFL_ATTACHEDONLY);

	if (LoadXInput()) {
		for (int i = 0; i < XUSER_MAX_COUNT; i++) {
			if (XInputGetStateFunc) {
				XINPUT_STATE state;
				if (XInputGetStateFunc(i, &state) == ERROR_SUCCESS) {
					xinput_controllers.push_back(new XInputController(i));
				}
			}
		}
	}
}

gxInput::~gxInput() {
	for (size_t i = 0; i < xinput_controllers.size(); ++i) {
		delete xinput_controllers[i];
	}
	xinput_controllers.clear();

	for(int k = 0; k < joysticks.size(); ++k) delete joysticks[k];
	joysticks.clear();
	delete mouse;
	delete keyboard;

	dirInput->Release();
	UnloadXInput();
}

void gxInput::wm_keydown(int key) {
	if(keyboard) keyboard->downEvent(key);
}

void gxInput::wm_keyup(int key) {
	if(keyboard) keyboard->upEvent(key);
}

void gxInput::wm_mousedown(int key) {
	if(mouse) mouse->downEvent(key);
}

void gxInput::wm_mouseup(int key) {
	if(mouse) mouse->upEvent(key);
}

void gxInput::wm_mousemove(int x, int y) {
	if(mouse) {
		mouse->axis_states[0] = x;
		mouse->axis_states[1] = y;
	}
}

void gxInput::wm_mousewheel(int dz) {
	if(mouse) mouse->axis_states[2] += dz;
}

void gxInput::wm_char(int wParam, int lParam) {
	int repeats = lParam & 0xffff;
	for(int i = 0; i < repeats; i++) {
		chars.push_back(wParam);
	}
}

void gxInput::reset() {
	if(mouse) mouse->reset();
	if(keyboard) keyboard->reset();
	for(int k = 0; k < joysticks.size(); ++k) joysticks[k]->reset();
}

bool gxInput::acquire() {
	bool m_ok = true, k_ok = true;
	if(mouse) m_ok = mouse->acquire();
	if(keyboard) k_ok = keyboard->acquire();
	if(m_ok && k_ok) return true;
	if(k_ok) keyboard->unacquire();
	if(m_ok) mouse->unacquire();
	return false;
}

void gxInput::unacquire() {
	if(keyboard) keyboard->unacquire();
	if(mouse) mouse->unacquire();
}

void gxInput::moveMouse(int x, int y) {
	if(!mouse) return;
	mouse->axis_states[0] = x;
	mouse->axis_states[1] = y;
	runtime->moveMouse(x, y);
}

gxDevice* gxInput::getMouse()const {
	return mouse;
}

gxDevice* gxInput::getKeyboard()const {
	return keyboard;
}

bool gxInput::getControllerConnected(int port) {
	if (port < xinput_controllers.size()) {
		XINPUT_STATE state;
		if (XInputGetStateFunc) {
			return XInputGetStateFunc(xinput_controllers[port]->index, &state) == ERROR_SUCCESS;
		}
	}

	port -= xinput_controllers.size();
	if (port >= 0 && port < joysticks.size()) {
		return true;
	}
	return false;
}

gxDevice* gxInput::getJoystick(int n)const {
	if(n < xinput_controllers.size()) return xinput_controllers[n];
	n -= xinput_controllers.size();
	return n >= 0 && n < joysticks.size() ? joysticks[n] : 0;
}

std::vector<int> gxInput::getChars() {
	std::vector<int> chrs = chars;
	chars.clear();
	return chrs;
}

int gxInput::getJoystickType(int n)const {
	if (n < xinput_controllers.size()) return 3;
	n -= xinput_controllers.size();
	return n >= 0 && n < joysticks.size() ? joysticks[n]->type : 0;
}

int gxInput::numJoysticks()const {
	return (xinput_controllers.size() + joysticks.size());
}

int gxInput::toAscii(int scan)const {
	switch(scan) {
		case DIK_INSERT:return ASC_INSERT;
		case DIK_DELETE:return ASC_DELETE;
		case DIK_HOME:return ASC_HOME;
		case DIK_END:return ASC_END;
		case DIK_PGUP:return ASC_PAGEUP;
		case DIK_PGDN:return ASC_PAGEDOWN;
		case DIK_UP:return ASC_UP;
		case DIK_DOWN:return ASC_DOWN;
		case DIK_LEFT:return ASC_LEFT;
		case DIK_RIGHT:return ASC_RIGHT;
	}
	scan &= 0x7f;
	int virt = MapVirtualKey(scan, 1);
	if(!virt) return 0;

	static unsigned char mat[256];
	mat[VK_LSHIFT] = keyboard->keyDown(DIK_LSHIFT) ? 0x80 : 0;
	mat[VK_RSHIFT] = keyboard->keyDown(DIK_RSHIFT) ? 0x80 : 0;
	mat[VK_SHIFT] = mat[VK_LSHIFT] | mat[VK_RSHIFT];
	mat[VK_LCONTROL] = keyboard->keyDown(DIK_LCONTROL) ? 0x80 : 0;
	mat[VK_RCONTROL] = keyboard->keyDown(DIK_RCONTROL) ? 0x80 : 0;
	mat[VK_CONTROL] = mat[VK_LCONTROL] | mat[VK_RCONTROL];
	mat[VK_LMENU] = keyboard->keyDown(DIK_LMENU) ? 0x80 : 0;
	mat[VK_RMENU] = keyboard->keyDown(DIK_RMENU) ? 0x80 : 0;
	mat[VK_MENU] = mat[VK_LMENU] | mat[VK_RMENU];

	WORD ch;
	if(ToAscii(virt, scan, mat, &ch, 0) != 1) return 0;
	return ch & 255;
}

XInputController::XInputController(int idx) : index(idx) {
	memset(&state, 0, sizeof(XINPUT_STATE));
	memset(&prev_state, 0, sizeof(XINPUT_STATE));
	reset();
}

void XInputController::update() {
	prev_state = state;
	if (XInputGetStateFunc) {
		if (XInputGetStateFunc(index, &state) != ERROR_SUCCESS) {
			memset(&state, 0, sizeof(XINPUT_STATE));
			return;
		}
	}

	// This based off mapping Xbox One Controller keys, unsure if 1:1 to other controllers
	// For Thumbstick support
	axis_states[0] = state.Gamepad.sThumbLX / 32767.0f;  // Left Thumbstick X
	axis_states[1] = state.Gamepad.sThumbLY / 32767.0f;  // Left Thumbstick Y
	axis_states[3] = state.Gamepad.sThumbRX / 32767.0f;  // Right Thumbstick X
	axis_states[4] = state.Gamepad.sThumbRY / 32767.0f;  // Right Thumbstick Y

	//Controller Triggers
	axis_states[5] = state.Gamepad.bLeftTrigger / 255.0f;   // Left Trigger, goes from (0 to 1) based on controller pressure
	axis_states[6] = state.Gamepad.bRightTrigger / 255.0f;  // Right Trigger, goes from (0 to 1) based on controller pressure

	// Differential Trigger (for backward compatibility :P with Z)
	axis_states[2] = axis_states[5] - axis_states[6];  // -1 to 1 range

	// Update button states
	const WORD buttons = state.Gamepad.wButtons;
	for (int i = 0; i < 23; ++i) {
		setDownState(i, (buttons & (1 << i)) ? true : false);
	}

	// D-PAD/POV Hat hook
	int pov = -1;
	if (buttons & XINPUT_GAMEPAD_DPAD_UP) {
		if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) pov = 45;
		else if (buttons & XINPUT_GAMEPAD_DPAD_LEFT) pov = 315;
		else pov = 0;
	}
	else if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) {
		if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) pov = 135;
		else if (buttons & XINPUT_GAMEPAD_DPAD_LEFT) pov = 225;
		else pov = 180;
	}
	else if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) {
		pov = 90;
	}
	else if (buttons & XINPUT_GAMEPAD_DPAD_LEFT) {
		pov = 270;
	}
	axis_states[8] = pov; // Store for JoyHat support later
}