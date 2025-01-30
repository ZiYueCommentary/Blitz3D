#ifndef GXINPUT_H
#define GXINPUT_H

#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include <xinput.h>

#include "gxdevice.h"
#include <vector>

class gxRuntime;

class XInputController : public gxDevice {
public:
	int index;
	XINPUT_STATE state;
	XINPUT_STATE prev_state;

	XInputController(int index);
	void update();
};

class gxInput {
public:
	gxRuntime* runtime;
	IDirectInput8* dirInput;

	gxInput(gxRuntime* runtime, IDirectInput8* di);
	~gxInput();

	void reset();
	bool acquire();
	void unacquire();

	void wm_keydown(int key);
	void wm_keyup(int key);
	void wm_mousedown(int key);
	void wm_mouseup(int key);
	void wm_mousemove(int x, int y);
	void wm_mousewheel(int dz);
	void wm_char(int wParam, int lParam);

private:

	/***** GX INTERFACE *****/
public:
	enum {
		ASC_HOME = 1, ASC_END = 2, ASC_INSERT = 3, ASC_DELETE = 4,
		ASC_PAGEUP = 5, ASC_PAGEDOWN = 6,
		ASC_UP = 28, ASC_DOWN = 29, ASC_RIGHT = 30, ASC_LEFT = 31
	};

	std::vector<XInputController*> xinput_controllers;

	void moveMouse(int x, int y);

	gxDevice* getMouse()const;
	gxDevice* getKeyboard()const;
	gxDevice* getJoystick(int port)const;
	std::vector<int> getChars();
	bool getControllerConnected(int port);
	int getJoystickType(int port)const;
	int numJoysticks()const;
	int toAscii(int key)const;
};

#endif