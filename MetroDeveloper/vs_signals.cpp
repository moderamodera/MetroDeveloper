#include <stdio.h>
#include <windows.h>
#define PSAPI_VERSION 1
#include <psapi.h>
#include <dinput.h>

#include "MetroDeveloper.h"

// DirectInput key names
struct DIKeyName { const char* name; int id; };

DIKeyName g_DIKeyNames[] = {
	{ "DIK_ESCAPE", 0x01 },
	{ "DIK_1", 0x02 },
	{ "DIK_2", 0x03 },
	{ "DIK_3", 0x04 },
	{ "DIK_4", 0x05 },
	{ "DIK_5", 0x06 },
	{ "DIK_6", 0x07 },
	{ "DIK_7", 0x08 },
	{ "DIK_8", 0x09 },
	{ "DIK_9", 0x0A },
	{ "DIK_0", 0x0B },
	{ "DIK_MINUS", 0x0C },
	{ "DIK_EQUALS", 0x0D },
	{ "DIK_BACK", 0x0E },
	{ "DIK_TAB", 0x0F },
	{ "DIK_Q", 0x10 },
	{ "DIK_W", 0x11 },
	{ "DIK_E", 0x12 },
	{ "DIK_R", 0x13 },
	{ "DIK_T", 0x14 },
	{ "DIK_Y", 0x15 },
	{ "DIK_U", 0x16 },
	{ "DIK_I", 0x17 },
	{ "DIK_O", 0x18 },
	{ "DIK_P", 0x19 },
	{ "DIK_LBRACKET", 0x1A },
	{ "DIK_RBRACKET", 0x1B },
	{ "DIK_RETURN", 0x1C },
	{ "DIK_LCONTROL", 0x1D },
	{ "DIK_A", 0x1E },
	{ "DIK_S", 0x1F },
	{ "DIK_D", 0x20 },
	{ "DIK_F", 0x21 },
	{ "DIK_G", 0x22 },
	{ "DIK_H", 0x23 },
	{ "DIK_J", 0x24 },
	{ "DIK_K", 0x25 },
	{ "DIK_L", 0x26 },
	{ "DIK_SEMICOLON", 0x27 },
	{ "DIK_APOSTROPHE", 0x28 },
	{ "DIK_GRAVE", 0x29 },
	{ "DIK_LSHIFT", 0x2A },
	{ "DIK_BACKSLASH", 0x2B },
	{ "DIK_Z", 0x2C },
	{ "DIK_X", 0x2D },
	{ "DIK_C", 0x2E },
	{ "DIK_V", 0x2F },
	{ "DIK_B", 0x30 },
	{ "DIK_N", 0x31 },
	{ "DIK_M", 0x32 },
	{ "DIK_COMMA", 0x33 },
	{ "DIK_PERIOD", 0x34 },
	{ "DIK_SLASH", 0x35 },
	{ "DIK_RSHIFT", 0x36 },
	{ "DIK_MULTIPLY", 0x37 },
	{ "DIK_LMENU", 0x38 },
	{ "DIK_SPACE", 0x39 },
	{ "DIK_CAPITAL", 0x3A },
	{ "DIK_F1", 0x3B },
	{ "DIK_F2", 0x3C },
	{ "DIK_F3", 0x3D },
	{ "DIK_F4", 0x3E },
	{ "DIK_F5", 0x3F },
	{ "DIK_F6", 0x40 },
	{ "DIK_F7", 0x41 },
	{ "DIK_F8", 0x42 },
	{ "DIK_F9", 0x43 },
	{ "DIK_F10", 0x44 },
	{ "DIK_NUMLOCK", 0x45 },
	{ "DIK_SCROLL", 0x46 },
	{ "DIK_NUMPAD7", 0x47 },
	{ "DIK_NUMPAD8", 0x48 },
	{ "DIK_NUMPAD9", 0x49 },
	{ "DIK_SUBTRACT", 0x4A },
	{ "DIK_NUMPAD4", 0x4B },
	{ "DIK_NUMPAD5", 0x4C },
	{ "DIK_NUMPAD6", 0x4D },
	{ "DIK_ADD", 0x4E },
	{ "DIK_NUMPAD1", 0x4F },
	{ "DIK_NUMPAD2", 0x50 },
	{ "DIK_NUMPAD3", 0x51 },
	{ "DIK_NUMPAD0", 0x52 },
	{ "DIK_DECIMAL", 0x53 },
	{ "DIK_OEM_102", 0x56 },
	{ "DIK_F11", 0x57 },
	{ "DIK_F12", 0x58 },
	{ "DIK_F13", 0x64 },
	{ "DIK_F14", 0x65 },
	{ "DIK_F15", 0x66 },
	{ "DIK_KANA", 0x70 },
	{ "DIK_ABNT_C1", 0x73 },
	{ "DIK_CONVERT", 0x79 },
	{ "DIK_NOCONVERT", 0x7B },
	{ "DIK_YEN", 0x7D },
	{ "DIK_ABNT_C2", 0x7E },
	{ "DIK_NUMPADEQUALS", 0x8D },
	{ "DIK_PREVTRACK", 0x90 },
	{ "DIK_AT", 0x91 },
	{ "DIK_COLON", 0x92 },
	{ "DIK_UNDERLINE", 0x93 },
	{ "DIK_KANJI", 0x94 },
	{ "DIK_STOP", 0x95 },
	{ "DIK_AX", 0x96 },
	{ "DIK_UNLABELED", 0x97 },
	{ "DIK_NEXTTRACK", 0x99 },
	{ "DIK_NUMPADENTER", 0x9C },
	{ "DIK_RCONTROL", 0x9D },
	{ "DIK_MUTE", 0xA0 },
	{ "DIK_CALCULATOR", 0xA1 },
	{ "DIK_PLAYPAUSE", 0xA2 },
	{ "DIK_MEDIASTOP", 0xA4 },
	{ "DIK_VOLUMEDOWN", 0xAE },
	{ "DIK_VOLUMEUP", 0xB0 },
	{ "DIK_WEBHOME", 0xB2 },
	{ "DIK_NUMPADCOMMA", 0xB3 },
	{ "DIK_DIVIDE", 0xB5 },
	{ "DIK_SYSRQ", 0xB7 },
	{ "DIK_RMENU", 0xB8 },
	{ "DIK_PAUSE", 0xC5 },
	{ "DIK_HOME", 0xC7 },
	{ "DIK_UP", 0xC8 },
	{ "DIK_PRIOR", 0xC9 },
	{ "DIK_LEFT", 0xCB },
	{ "DIK_RIGHT", 0xCD },
	{ "DIK_END", 0xCF },
	{ "DIK_DOWN", 0xD0 },
	{ "DIK_NEXT", 0xD1 },
	{ "DIK_INSERT", 0xD2 },
	{ "DIK_DELETE", 0xD3 },
	{ "DIK_LWIN", 0xDB },
	{ "DIK_RWIN", 0xDC },
	{ "DIK_APPS", 0xDD },
	{ "DIK_POWER", 0xDE },
	{ "DIK_SLEEP", 0xDF },
	{ "DIK_WAKE", 0xE3 },
	{ "DIK_WEBSEARCH", 0xE5 },
	{ "DIK_WEBFAVORITES", 0xE6 },
	{ "DIK_WEBREFRESH", 0xE7 },
	{ "DIK_WEBSTOP", 0xE8 },
	{ "DIK_WEBFORWARD", 0xE9 },
	{ "DIK_WEBBACK", 0xEA },
	{ "DIK_MYCOMPUTER", 0xEB },
	{ "DIK_MAIL", 0xEC },
	{ "DIK_MEDIASELECT", 0xED },

	/*
	 * Alternate names for keys, to facilitate transition from DOS. },
	 */
	{ "DIK_BACKSPACE", 0x0E },
	{ "DIK_NUMPADSTAR", 0x37 },
	{ "DIK_LALT", 0x38 },
	{ "DIK_CAPSLOCK", 0x3A },
	{ "DIK_NUMPADMINUS", 0x4A },
	{ "DIK_NUMPADPLUS", 0x4E },
	{ "DIK_NUMPADPERIOD", 0x53 },
	{ "DIK_NUMPADSLASH", 0xB5 },
	{ "DIK_RALT", 0xB8 },
	{ "DIK_UPARROW", 0xC8 },
	{ "DIK_PGUP", 0xC9 },
	{ "DIK_LEFTARROW", 0xCB },
	{ "DIK_RIGHTARROW", 0xCD },
	{ "DIK_DOWNARROW", 0xD0 },
	{ "DIK_PGDN", 0xD1 },

	{ 0, 0 }
};

// shared strings, kinda like java 
struct shared_string;

typedef shared_string* (__stdcall *pfn_dock_shared_string)(const char *str);
static pfn_dock_shared_string dock_shared_string;

// static function, so no 'this'
// x is used for localus signals (non-interesting for us)
typedef void (__stdcall *pfn_igame_level_signal)(shared_string **signal, int x);
static pfn_igame_level_signal igame_level_signal;

// signals table
shared_string* g_signals_table[256] = { 0 };

static DWORD WINAPI init_signals_thread(LPVOID param)
{
	unsigned i;

	Sleep(3000);

	for (i = 0; g_DIKeyNames[i].name; i++) {
		char str[256];
		getString("vs_signals", g_DIKeyNames[i].name, "", str, sizeof(str));
		if (str[0]) {
			g_signals_table[g_DIKeyNames[i].id] = dock_shared_string(str);
		}
	}

	return 0;
}

void install_vs_signals(MODULEINFO &mi)
{
	printf("initializing VS signals...\n");

	dock_shared_string = (pfn_dock_shared_string)FindPattern(
		(DWORD)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x83\xEC\x14\x53\x55\x56\x8B\x35\x84\x0D\xA1\x00\x57\x8B\x7C\x24\x28\x85\xFF",
		"xx?xxxxx????xxxx?xx"
	);

	printf("dock_shared_string = %p\n", dock_shared_string);

	igame_level_signal = (pfn_igame_level_signal)FindPattern(
		(DWORD)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"\x83\xEC\x14\x53\x55\x56\x8B\x35\xDC\x0D\xA1\x00\x8B\x86\xF0\x01\x00\x00\x57\x8D\xBE\xF0\x01\x00\x00\x0F\xB6\xC8",
		"xx?xxxxx????xx????xxx????xxx"
	);

	printf("igame_level_signal = %p\n", igame_level_signal);

	// cannot make table here because shared string manager is not initialized yet
	DWORD tid;
	HANDLE th;
	th = CreateThread(NULL, 0, init_signals_thread, NULL, 0, &tid);
	CloseHandle(th);
}

bool process_vs_signal(int key, int action)
{
	if (key >= 0 && key < 256 && g_signals_table[key]) {
		igame_level_signal(&g_signals_table[key], 0);
		return true;
	}

	return false;
}