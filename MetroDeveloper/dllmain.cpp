#define _CRT_SECURE_NO_WARNINGS 1 // MSVS dno

#include <stdio.h>
#include <io.h>
#include <windows.h>
#include "MinHook.h"
#include "uconsole.h"

#include <dinput.h>

#define PSAPI_VERSION 1
#include <psapi.h>
#pragma comment (lib, "psapi.lib")

#include "MetroDeveloper.h"
#include "wpn_bobbing_la.h"

bool g_navmap_enabled;
bool g_log_enabled;

bool isLL;

bool g_unlock_3rd_person_camera;
typedef void(__thiscall* _base_npc_cameras_cam_set)(void* _this, int camera_style, float speed, int preserve_attach);
typedef void(__cdecl* _set_camera_2033)(...); // this function has weird calling convention; 'this' passed in EDI, and two parameters passed through stack
_base_npc_cameras_cam_set base_npc_cameras_cam_set = nullptr;
_set_camera_2033 set_camera_2033 = nullptr;


enum enpc_cameras // 2033, Last Light, Redux (changed a bit in Arktika.1)
{
	enc_first_eye = 0,
	enc_ladder    = 1,
	enc_look_at   = 2,
	enc_free_look = 3,
	enc_station   = 4,
	enc_locked    = 5,
	enc_max_cam   = 6
};

bool g_fly;
typedef void(__fastcall*** _track)(void*);
#ifdef _WIN64
typedef _track(__fastcall* _cflycam_cflycam)(void* _this, const char* name);
typedef LPCRITICAL_SECTION(__fastcall* _memory)();
typedef void* (__fastcall* _tlsf_memalign)(DWORD64 tlsf, DWORD align, DWORD size);
typedef void* (__fastcall* _camera_manager_play_track)(DWORD64 _this, void* t, float accrue, float start_pos, void* unused3, void* e, void* unused4, void* unused5, void* owner);
_tlsf_memalign tlsf_memalign = nullptr;
DWORD64 g_game = NULL;
#else
typedef _track(__thiscall* _cflycam_cflycam)(void* _this, const char* name);
typedef LPCRITICAL_SECTION(__cdecl* _memory)();
typedef void* (__thiscall* _camera_manager_play_track)(DWORD _this, void* t, double hz, void* owner);
void* tlsf_memalign = nullptr;
DWORD g_game = NULL;
#endif
_cflycam_cflycam cflycam_cflycam = nullptr;
_memory memory = nullptr;
_camera_manager_play_track camera_manager_play_track = nullptr;

bool g_unlock_dev_console;
bool g_quicksave;
bool g_vs_signals_enabled;

// signature scanner
MODULEINFO mi;

MODULEINFO GetModuleData(const char* moduleName)
{
	MODULEINFO currentModuleInfo = { 0 };
	HMODULE moduleHandle = GetModuleHandle(moduleName);
	if (moduleHandle == NULL)
	{
		return currentModuleInfo;
	}
	GetModuleInformation(GetCurrentProcess(), moduleHandle, &currentModuleInfo, sizeof(MODULEINFO));
	return currentModuleInfo;
}

bool DataCompare(const BYTE* pData, const BYTE* pattern, const char* mask)
{
	for (; *mask; mask++, pData++, pattern++)
		if (*mask == 'x' && *pData != *pattern)
			return false;
	return (*mask) == NULL;
}

#ifndef _WIN64
DWORD FindPattern(DWORD start_address, DWORD length, BYTE* pattern, char* mask)
{
	for (DWORD i = 0; i < length; i++)
		if (DataCompare((BYTE*)(start_address + i), pattern, mask))
			return (DWORD)(start_address + i);
	return NULL;
}

DWORD FindPatternInEXE(BYTE* pattern, char* mask)
{
	return FindPattern((DWORD)mi.lpBaseOfDll, mi.SizeOfImage, pattern, mask);
}
#else
DWORD64 FindPattern(DWORD64 start_address, DWORD64 length, BYTE* pattern, char* mask)
{
	for (DWORD64 i = 0; i < length; i++)
		if (DataCompare((BYTE*)(start_address + i), pattern, mask))
			return (DWORD64)(start_address + i);
	return NULL;
}

DWORD64 FindPatternInEXE(BYTE* pattern, char* mask)
{
	return FindPattern((DWORD64)mi.lpBaseOfDll, mi.SizeOfImage, pattern, mask);
}
#endif

void BadQuitReset()
{
	HKEY hKey;
	DWORD disposition;
	if (RegCreateKeyEx(HKEY_CURRENT_USER,
#ifndef _WIN64
		!isLL ? "Software\\4A-Games\\Metro2033" : "Software\\4A-Games\\Metro2034"
#else
		"Software\\4A-Games\\Metro Redux"
#endif
		, 0, NULL, 0, KEY_SET_VALUE, 0, &hKey,
		&disposition) == ERROR_SUCCESS)
	{
		RegDeleteValue(hKey, "BadQuit");
		RegCloseKey(hKey);
	}
}

typedef uconsole_server** (__stdcall* _getConsole)();
_getConsole getConsole = nullptr;

// log part
FILE* fLog;
CRITICAL_SECTION logCS;

typedef void(__thiscall* _slog)(const char* s);
_slog slog_Orig = nullptr;

void __fastcall slog_Hook(const char* s)
{
#ifndef _WIN64
	static bool isNavMapThreadCreated = false;
	if (g_navmap_enabled && !isNavMapThreadCreated)
	{
		if (strstr(s, "* [loader] map loaded in "))
		{
			StartNavmapThread();
			isNavMapThreadCreated = true;
		}
	}
#endif

	if (g_log_enabled)
	{
		EnterCriticalSection(&logCS);

		fprintf(fLog, s);
		fputc('\n', fLog);
		fflush(fLog);

		LeaveCriticalSection(&logCS);
	}

	slog_Orig(s);
}

void ASMWrite(void* address, BYTE* code, size_t size)
{
	DWORD OldProtect = NULL;
	VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &OldProtect);
	memcpy(address, code, size);
	VirtualProtect(address, size, OldProtect, &OldProtect);
}

void getString(const char* section_name, const char* str_name, const char* default_str, char* result, DWORD size)
{
	GetPrivateProfileString(section_name, str_name, default_str, result, size, ".\\MetroDeveloper.ini");
}

bool getBool(const char* section_name, const char* bool_name, bool default_bool)
{
	string256 str;
	getString(section_name, bool_name, (default_bool ? "true" : "false"), str, sizeof(str));
	return (strcmp(str, "true") == 0) || (strcmp(str, "yes") == 0) || (strcmp(str, "on") == 0) || (strcmp(str, "1") == 0);
}

float getFloat(const char* section_name, const char* param_name, float param_default)
{
	string256 str;
	float param;
	
	getString(section_name, param_name, "", str, sizeof(str));
	if(!str[0] || sscanf(str, "%f", &param) != 1)
		return param_default;
	
	return param;
}

void* clevel_r_on_key_press_Orig = nullptr;

#ifndef _WIN64
void __fastcall clevel_r_on_key_press_Hook2033(void* _this, void* _unused, int action, int key, int state)
{
	printf("action = %d, key = %d, state = %d\n", action, key, state);

	// console
	if (g_unlock_dev_console)
	{
		if (key == DIK_GRAVE)
		{
			uconsole_server** console = getConsole();
			(*console)->show(console);
		}
	}
	
	// quick save on F5
	if (g_quicksave)
	{
		if (key == DIK_F5)
		{
			uconsole_server** console = getConsole();
			(*console)->execute_deferred(console, "gamesave auto_save");
		}
	}

	// fly on F7
	if (g_fly)
	{
		if (key == DIK_F7)
		{
			uconsole_server** console = getConsole();
			(*console)->execute_deferred(console, "fly 1");
		}
	}

	// 3rd person camera
	if (g_unlock_3rd_person_camera && key <= DIK_F3 && key >= DIK_F1)
	{
		// _this == g_level + 0x4 (+0x4 due to multiple inheritance)

		// which one is better to use here ??
		void* control_entity = *((void**)((char*)_this + 0x10));
		void* view_entity = *((void**)((char*)_this + 0x14));

		void *base_npc_cameras = *((void**)((char*)view_entity + 0x348));

		if (key == DIK_F1) // F1
		{
			__asm
			{
				push 3F800000h              // speed = 1.f
				push enc_first_eye          // camera_style = enc_first_eye
				mov edi, [base_npc_cameras] // 'this' pointer passed in EDI
				call [set_camera_2033]
			}
		}

		if (key == DIK_F2) // F2
		{
			__asm
			{
				push 3F800000h              // speed = 1.f
				push enc_look_at            // camera_style = enc_look_at
				mov edi, [base_npc_cameras] // 'this' pointer passed in EDI
				call [set_camera_2033]
			}
		}

		if (key == DIK_F3) // F3
		{
			__asm
			{
				push 3F800000h              // speed = 1.f
				push enc_free_look          // camera_style = enc_free_look
				mov edi, [base_npc_cameras] // 'this' pointer passed in EDI
				call [set_camera_2033]
			}
		}
	}

	// VisualScript signals
	if (g_vs_signals_enabled)
		process_vs_signal(key, action);

	typedef void(__thiscall* _clevel_r_on_key_press_2033)(void* _this, int action, int key, int state);
	((_clevel_r_on_key_press_2033)clevel_r_on_key_press_Orig)(_this, action, key, state);
}
#endif

#ifndef _WIN64
void __fastcall clevel_r_on_key_press_Hook(void* _this, void* _unused, int action, int key, int state, int resending)
#else
void __fastcall clevel_r_on_key_press_Hook(void* _this, int action, int key, int state, int resending)
#endif
{
	//printf("action = %d, key = %d, state = %d, resending = %d\n", action, key, state, resending);

	// console
	if (g_unlock_dev_console)
	{
		if (key == DIK_GRAVE)
		{
			uconsole_server** console = getConsole();
			(*console)->show(console);
		}
	}

	// quick save on F5
	if (g_quicksave)
	{
		if (key == DIK_F5)
		{
			uconsole_server** console = getConsole();
			(*console)->execute_deferred(console, "gamesave");
		}
	}

#ifdef _WIN64
	// Redux
	
	// 3rd person camera
	if (g_unlock_3rd_person_camera && key <= DIK_F3 && key >= DIK_F1)
	{
		// _this == g_level + 0x8 (+0x8 due to multiple inheritance)

		// which one is better to use here ??
		void* startup_entity = *((void**)((char*)_this + 0x28));
		void* control_entity = *((void**)((char*)_this + 0x30));
		void* view_entity = *((void**)((char*)_this + 0x38));

		void *base_npc_cameras = *((void**)((char*)view_entity + 0x640));

		if (key == DIK_F1) // F1
			base_npc_cameras_cam_set(base_npc_cameras, enc_first_eye, 1.f, 1);
		if (key == DIK_F2) // F2
			base_npc_cameras_cam_set(base_npc_cameras, enc_look_at, 1.f, 1);
		if (key == DIK_F3) // F3
			base_npc_cameras_cam_set(base_npc_cameras, enc_free_look, 1.f, 1);
	}

	// fly on F7
	if (g_fly)
	{
		if (key == DIK_F7)
		{
			LPCRITICAL_SECTION mem = memory();
			++*(DWORD*)((DWORD64)mem + 0x100);
			EnterCriticalSection(mem);
			DWORD64 tlsf = *(DWORD64*)((DWORD64)mem + 0x38);
			// судя по всему, в редуксе выравнивание 0x10. Точный размер памяти хз, выставил такой-же как в арктике
			void* cflycam_this = tlsf_memalign(tlsf, 0x10, 0x120);
			LeaveCriticalSection(mem);

			_track track = cflycam_cflycam(cflycam_this, "1");

			if (g_game == NULL)
			{
				// читаем адрес инструкции mov rax, cs:g_game
				// 48 8B 05 ? ? ? ? 4C 8D 85 ? ? ? ? 48 8D 95 ? ? ? ? 48 8B 58 10 48 8D 4C 24 ? E8 ? ? ? ? 48 8B CB 48 89 7C 24 ? 0F 57 DB 0F 57 D2 48 8B D0 E8 ? ? ? ? 48 8D 8D ? ? ? ? E9
				DWORD64 mov = FindPattern(
					(DWORD64)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x48\x8B\x05\x00\x00\x00\x00\x4C\x8D\x85\x00\x00\x00\x00\x48\x8D\x95\x00\x00\x00\x00\x48\x8B\x58\x10\x48\x8D\x4C\x24\x00\xE8\x00\x00\x00\x00\x48\x8B\xCB\x48\x89\x7C\x24\x00\x0F\x57\xDB\x0F\x57\xD2\x48\x8B\xD0\xE8\x00\x00\x00\x00\x48\x8D\x8D\x00\x00\x00\x00\xE9",
					"xxx????xxx????xxx????xxxxxxxx?x????xxxxxxx?xxxxxxxxxx????xxx????x");

				// вычисняем адрес и получаем g_game
				g_game = *(DWORD64*)(mov + 7 + *(DWORD*)(mov + 3));
			}
			DWORD64 _cameras = *(DWORD64*)(g_game + 0x10);

			// я так и не понял для чего этот конструктор, т.к. работает и без его вызова, ну пусть будет...
			(**track)(track); // _constructor

			void* owner = nullptr;
			camera_manager_play_track(_cameras, track, 0.0, 0.0, nullptr, nullptr, nullptr, nullptr, &owner);
		}
	}
#else
	// Last Light

	// 3rd person camera
	if (g_unlock_3rd_person_camera && key <= DIK_F3 && key >= DIK_F1)
	{
		// _this == g_level + 0x4 (+0x4 due to multiple inheritance)

		// which one is better to use here ??
		void* startup_entity = *((void**)((char*)_this + 0x18));
		void* control_entity = *((void**)((char*)_this + 0x1C));
		void* view_entity = *((void**)((char*)_this + 0x20));

		void *base_npc_cameras = *((void**)((char*)view_entity + 0x3A4));

		if (key == DIK_F1) // F1
			base_npc_cameras_cam_set(base_npc_cameras, enc_first_eye, 1.f, 1);
		if (key == DIK_F2) // F2
			base_npc_cameras_cam_set(base_npc_cameras, enc_look_at, 1.f, 1);
		if (key == DIK_F3) // F3
			base_npc_cameras_cam_set(base_npc_cameras, enc_free_look, 1.f, 1);
	}

	// fly on F7
	if (g_fly)
	{
		if (key == DIK_F7)
		{
			LPCRITICAL_SECTION mem = memory();
			++*(DWORD*)((DWORD)mem + 0x84);
			EnterCriticalSection(mem);
			DWORD tlsf = *(DWORD*)((DWORD)mem + 0x20);
			// судя по всему, в редуксе выравнивание 0x10. Точный размер памяти хз, выставил такой-же как в арктике
			void* cflycam_this = nullptr;

			__asm
			{
				mov esi, tlsf
				push 0x10
				push esi
				mov eax, 0x120
				call tlsf_memalign
				mov cflycam_this, eax
				add esp, 8
			}
			LeaveCriticalSection(mem);

			_track track = cflycam_cflycam(cflycam_this, "1");

			if (g_game == NULL)
			{
				// читаем адрес инструкции mov eax, g_game
				// A1 ? ? ? ? 0F 57 C0 56
				DWORD mov = FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\xA1\x00\x00\x00\x00\x0F\x57\xC0\x56",
					"x????xxxx");

				// вычисняем адрес и получаем g_game
				g_game = *(DWORD*)(*(DWORD*)(mov + 1));
			}
			DWORD _cameras = *(DWORD*)(g_game + 0x8);
			
			// я так и не понял для чего этот конструктор, т.к. работает и без его вызова, ну пусть будет...
			(**track)(track); // _constructor
			
			void* owner = nullptr;
			camera_manager_play_track(_cameras, track, 0, &owner);
		}
	}
#endif

	typedef void(__thiscall* _clevel_r_on_key_press)(void* _this, int action, int key, int state, int resending);
	((_clevel_r_on_key_press)clevel_r_on_key_press_Orig)(_this, action, key, state, resending);
}

typedef void (__stdcall* _cmd_register_commands)();
_cmd_register_commands cmd_register_commands_Orig = nullptr;

void* cmd_mask_Address = nullptr;
unsigned int* ps_actor_flags_Address = nullptr;
void* cmd_mask_vftable_Address = nullptr;

cmd_mask_struct g_god;
cmd_mask_struct g_global_god;
cmd_mask_struct g_kill_everyone;
cmd_mask_struct g_notarget;
cmd_mask_struct g_unlimitedammo;
cmd_mask_struct g_unlimitedfilters;
cmd_mask_struct g_autopickup;

#ifndef _WIN64
void cmd_register_commands_Hook()
{
	uconsole cu = uconsole::uconsole(getConsole(), cmd_mask_Address);

	cu.cmd_mask(&g_god, "g_god", ps_actor_flags_Address, 1, false);
	cu.command_add(&g_god);

	cu.cmd_mask(&g_global_god, "g_global_god", ps_actor_flags_Address, 2, false);
	cu.command_add(&g_global_god);

	cu.cmd_mask(&g_kill_everyone, "g_kill_everyone", ps_actor_flags_Address, 16, false);
	cu.command_add(&g_kill_everyone);

	cu.cmd_mask(&g_notarget, "g_notarget", ps_actor_flags_Address, 4, false);
	cu.command_add(&g_notarget);

	cu.cmd_mask(&g_unlimitedammo, "g_unlimitedammo", ps_actor_flags_Address, 8, false);
	cu.command_add(&g_unlimitedammo);

	cu.cmd_mask(&g_unlimitedfilters, "g_unlimitedfilters", ps_actor_flags_Address, 128, false);
	cu.command_add(&g_unlimitedfilters);

	cu.cmd_mask(&g_autopickup, "g_autopickup", ps_actor_flags_Address, 32, false);
	cu.command_add(&g_autopickup);

	cmd_register_commands_Orig();
}
#else
void cmd_register_commands_Hook()
{
	// 0. call original function first to ensure that g_toggle_aim is initialized and we can find it
	cmd_register_commands_Orig();
	
	// 1. find constant string
	const char *str_g_toggle_aim = (const char *)FindPattern(
		(DWORD64)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)"g_toggle_aim\0",
		"xxxxxxxxxxxxx");
		
	// 2. find reference to that string
	const char **xref = (const char **)FindPattern(
		(DWORD64)mi.lpBaseOfDll,
		mi.SizeOfImage,
		(BYTE*)&str_g_toggle_aim,
		"xxxxxxxx");
	
	// 3. find pointer to existing command object based on xref
	cmd_mask_struct * pCmd = (cmd_mask_struct*)(((char*)xref) - offsetof(cmd_mask_struct, _name));
	
	ps_actor_flags_Address = pCmd->value;
	cmd_mask_vftable_Address = pCmd->__vftable;
	
	// 4. register new commands
	uconsole cu = uconsole::uconsole(getConsole(), NULL);
	
	g_god.construct(cmd_mask_vftable_Address, "g_god", ps_actor_flags_Address, 1);
	cu.command_add(&g_god);
	
	g_global_god.construct(cmd_mask_vftable_Address, "g_global_god", ps_actor_flags_Address, 2);
	cu.command_add(&g_global_god);

	g_kill_everyone.construct(cmd_mask_vftable_Address, "g_kill_everyone", ps_actor_flags_Address, 16);
	cu.command_add(&g_kill_everyone);

	g_notarget.construct(cmd_mask_vftable_Address, "g_notarget", ps_actor_flags_Address, 4);
	cu.command_add(&g_notarget);

	g_unlimitedammo.construct(cmd_mask_vftable_Address, "g_unlimitedammo", ps_actor_flags_Address, 8);
	cu.command_add(&g_unlimitedammo);

	g_unlimitedfilters.construct(cmd_mask_vftable_Address, "g_unlimitedfilters", ps_actor_flags_Address, 128);
	cu.command_add(&g_unlimitedfilters);

	g_autopickup.construct(cmd_mask_vftable_Address, "g_autopickup", ps_actor_flags_Address, 32);
	cu.command_add(&g_autopickup);
}
#endif


BOOL APIENTRY DllMain(HINSTANCE hInstDLL, DWORD reason, LPVOID reserved)
{
	if(reason == DLL_PROCESS_ATTACH)
	{
		AllocConsole();
		freopen("CONOUT$", "w", stdout);

		if (getBool("other", "beep", true))
		{
			Beep(1000, 200);
		}

		mi = GetModuleData(NULL);

		bool minhook = (MH_Initialize() == MH_OK);
		if (!minhook)
		{
			MessageBox(NULL, "MinHook not initialized!", "MinHook", MB_OK | MB_ICONERROR);
		}

#ifndef _WIN64
		isLL = (GetModuleHandle("MetroLL.exe") != NULL);
#else
		isLL = true;
#endif

		g_log_enabled = getBool("log", "enabled", false);
		if(g_log_enabled)
		{
			string256 logFilename;

			getString("log", "filename", "uengine.log", logFilename, sizeof(logFilename));
			fLog = fopen(logFilename, "w");
			if (fLog != NULL)
			{
				InitializeCriticalSection(&logCS);
			}
		}

		g_navmap_enabled = (!isLL && strstr(GetCommandLine(), "-navmap"));

		// setup log hook
		if (g_log_enabled || g_navmap_enabled)
		{
			LPVOID slog_Address;

#ifndef _WIN64
			if (!isLL)
			{
				// B8 ? ? ? ? E8 ? ? ? ? 53 33 DB
				slog_Address = (LPVOID)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\xB8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x53\x33\xDB",
					"x????x????xxx");
			}
			else
			{
				// B8 ? ? ? ? E8 ? ? ? ? 53 33 DB 56 33 C0
				slog_Address = (LPVOID)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\xB8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x53\x33\xDB\x56\x33\xC0",
					"x????x????xxxxxx");
			}
#else
			// 40 53 B8 ? ? ? ? E8 ? ? ? ? 48 2B E0 33 C0
			slog_Address = (LPVOID)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x40\x53\xB8\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x2B\xE0\x33\xC0",
				"xxx????x????xxxxx");
#endif

			if (minhook) {
				if (MH_CreateHook(slog_Address, &slog_Hook, reinterpret_cast<LPVOID*>(&slog_Orig)) == MH_OK) {
					if (MH_EnableHook(slog_Address) != MH_OK) {
						MessageBox(NULL, "MH_EnableHook() != MH_OK", "slog Hook", MB_OK | MB_ICONERROR);
					}
				}
				else {
					MessageBox(NULL, "MH_CreateHook() != MH_OK", "slog Hook", MB_OK | MB_ICONERROR);
				}
			}
		}

		if (isLL && getBool("other", "allow_dds", false))
		{
#ifndef _WIN64
			// 75 31 8B 3F
			LPVOID jne = (LPVOID)FindPattern(
				(DWORD)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x75\x31\x8B\x3F",
				"xxxx");
#else
			// 75 ? 49 8B 45 ? 48 8D 50 ? 48 85 C0 75 ? 48 8B D6 48 8D 0D
			LPVOID jne = (LPVOID)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x75\x00\x49\x8B\x45\x00\x48\x8D\x50\x00\x48\x85\xC0\x75\x00\x48\x8B\xD6\x48\x8D\x0D",
				"x?xxx?xxx?xxxx?xxxxxx");
#endif

			BYTE JMP[] = { 0xEB };
			ASMWrite(jne, JMP, sizeof(JMP));
		}

		if (getBool("other", "unlock_content_folder", false))
			install_vfs_hooks(isLL);

		if (getBool("other", "badquit_reset", false))
		{
			BadQuitReset();
		}

#ifndef _WIN64
		if (!isLL && getBool("other", "no_videocard_msg", false)) // Only metro 2033
		{
			// 68 ? ? ? ? BF ? ? ? ? 8D 74 24 ? E8 ? ? ? ? 83 C4 ? 80 7C 24 ? ? 75 - для версии с dlc
			LPVOID VideoMsgAddress = (LPVOID)FindPattern(
				(DWORD)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x68\x00\x00\x00\x00\xBF\x00\x00\x00\x00\x8D\x74\x24\x00\xE8\x00\x00\x00\x00\x83\xC4\x00\x80\x7C\x24\x00\x00\x75",
				"x????x????xxx?x????xx?xxx??x");
			BYTE VideoMsgJMP[] = { 0xEB, 0x5B };
			if (VideoMsgAddress == NULL)
			{
				// 68 ? ? ? ? BF ? ? ? ? 8D 74 24 ? E8 ? ? ? ? 83 C4 ? 80 7C 24 ? ? 74 ? 8D 7C 24 - для версии без dlc
				VideoMsgAddress = (LPVOID)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x68\x00\x00\x00\x00\xBF\x00\x00\x00\x00\x8D\x74\x24\x00\xE8\x00\x00\x00\x00\x83\xC4\x00\x80\x7C\x24\x00\x00\x74\x00\x8D\x7C\x24",
					"x????x????xxx?x????xx?xxx??x?xxx");
				VideoMsgJMP[1] = 0x78;
			}
			ASMWrite(VideoMsgAddress, VideoMsgJMP, sizeof(VideoMsgJMP));
		}
#endif

		if (g_navmap_enabled || getBool("other", "nointro", false))
		{
			LPVOID IntroAddress;
#ifndef _WIN64
			if (!isLL)
			{
				// 51 0F B7 05
				IntroAddress = (LPVOID)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x51\x0F\xB7\x05",
					"xxxx");
			}
			else
			{
				// 66 A1 ? ? ? ? 66 83 F8 06 73 15
				IntroAddress = (LPVOID)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x66\xA1\x00\x00\x00\x00\x66\x83\xF8\x06\x73\x15",
					"xx????xxxxxx");
			}
			BYTE ret[] = { 0xC3 };
			ASMWrite(IntroAddress, ret, sizeof(ret));
#else
			// 73 1D 33 C9
			IntroAddress = (LPVOID)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x73\x1D\x33\xC9",
				"xxxx");
			BYTE jmp[] = { 0xEB };
			ASMWrite(IntroAddress, jmp, sizeof(jmp));
#endif
		}

		g_unlock_3rd_person_camera = getBool("other", "unlock_3rd_person_camera", false);
		if (g_unlock_3rd_person_camera)
		{
#ifdef _WIN64
			base_npc_cameras_cam_set = (_base_npc_cameras_cam_set)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x41\x56\x48\x83\xEC\x30\x48\x63\x41\x68\x48\x8B\xF1\x0F\x29\x74\x24\x00\x4C\x8B\x74\xC1\x00\x89\x51\x68\x48\x63\xC2\x0F\x28\xF2\x48\x8B\x5C\xC1\x00\x49\x8B\x06\x49\x8B\xCE\x41\x8B\xF9\xFF\x50\x28\x48\x8B\x03\x44\x8B\xC7\x49\x8B\xD6\x48\x8B\xCB\xFF\x50\x20\x8B\x05\x00\x00\x00\x00\xA8\x01\x75\x20",
				"xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxxx?xxxx?xxxxxxxxxxxxx?xxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxx");
#else
			if (isLL)
			{
				base_npc_cameras_cam_set = (_base_npc_cameras_cam_set)FindPattern(
					(DWORD64)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x53\x56\x8B\xF1\x8B\x46\x4C\x57\x8B\x7C\x86\x34\x8B\x44\x24\x10\x89\x46\x4C\x8B\x17\x8B\x5C\x86\x34\x8B\x42\x14\x8B\xCF",
					"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
			}
			else
			{
				set_camera_2033 = (_set_camera_2033)FindPattern(
					(DWORD64)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\xB8\x00\x00\x00\x00\x84\x05\x00\x00\x00\x00\x56\x75\x1D\x09\x05\x00\x00\x00\x00\x68\x00\x00\x00\x00\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x83\xC4\x04\x8B\x47\x6C",
					"x????xx????xxxxx????x????xx????????x????xxxxxx");
			}
#endif
		}

		g_unlock_dev_console = getBool("other", "unlock_dev_console", false);
		g_quicksave = getBool("other", "quicksave", false);

#ifdef _WIN64
		bool restore_deleted_commands = getBool("other", "restore_deleted_commands", false);
#else
		bool restore_deleted_commands = getBool("other", "restore_deleted_commands", false) && isLL;
#endif

		if (g_unlock_dev_console || restore_deleted_commands || g_navmap_enabled)
		{
#ifndef _WIN64
			if (!isLL)
			{
				// 55 8B EC 83 E4 ? A1 ? ? ? ? 85 C0 56 57 75 ? E8 ? ? ? ? 85 C0 74 ? 8B F8 E8 ? ? ? ? EB ? 33 C0 8B F0 A3 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 5F
				getConsole = (_getConsole)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x55\x8B\xEC\x83\xE4\x00\xA1\x00\x00\x00\x00\x85\xC0\x56\x57\x75\x00\xE8\x00\x00\x00\x00\x85\xC0\x74\x00\x8B\xF8\xE8\x00\x00\x00\x00\xEB\x00\x33\xC0\x8B\xF0\xA3\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x5F",
					"xxxxx?x????xxxxx?x????xxx?xxx????x?xxxxx????x????x????x");
			}
			else
			{
				// 55 8B EC 83 E4 ? A1 ? ? ? ? 85 C0 75 ? E8 ? ? ? ? 8B C8 A3 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 8B E5
				getConsole = (_getConsole)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x55\x8B\xEC\x83\xE4\x00\xA1\x00\x00\x00\x00\x85\xC0\x75\x00\xE8\x00\x00\x00\x00\x8B\xC8\xA3\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xA1\x00\x00\x00\x00\x8B\xE5",
					"xxxxx?x????xxx?x????xxx????x????x????xx");
			}
#else
			// 48 83 ec ? 48 8b 05 ? ? ? ? 48 85 c0 75 ? e8 ? ? ? ? 48 8b 05
			getConsole = (_getConsole)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x48\x83\xec\x00\x48\x8b\x05\x00\x00\x00\x00\x48\x85\xc0\x75\x00\xe8\x00\x00\x00\x00\x48\x8b\x05",
				"xxx?xxx????xxxx?x????xxx");
#endif
		}

		g_fly = getBool("other", "fly", false);

		if(g_unlock_dev_console || g_unlock_3rd_person_camera || g_quicksave || g_fly)
		{
#ifndef _WIN64
			if (minhook) {
				// 51 ? 8B ? 8B 0D ? ? ? ? 85
				LPVOID clevel_r_on_key_press_Address = (LPVOID)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x51\x00\x8B\x00\x8B\x0D\x00\x00\x00\x00\x85",
					"x?x?xx????x");

				MH_STATUS status = MH_CreateHook(clevel_r_on_key_press_Address, 
					(isLL ? (LPVOID)&clevel_r_on_key_press_Hook : (LPVOID)&clevel_r_on_key_press_Hook2033),
					reinterpret_cast<LPVOID*>(&clevel_r_on_key_press_Orig));

				if (status == MH_OK) {
					if (MH_EnableHook(clevel_r_on_key_press_Address) != MH_OK) {
						MessageBox(NULL, "MH_EnableHook() != MH_OK", "clevel_r_on_key_press Hook", MB_OK | MB_ICONERROR);
					}
				} else {
					MessageBox(NULL, "MH_CreateHook() != MH_OK", "clevel_r_on_key_press Hook", MB_OK | MB_ICONERROR);
				}
			}
#else
			if (minhook) {
				// 40 53 55 56 57 48 83 EC ? 48 8B F1
				LPVOID clevel_r_on_key_press_Address = (LPVOID)FindPattern(
					(DWORD64)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x40\x53\x55\x56\x57\x48\x83\xEC\x00\x48\x8B\xF1",
					"xxxxxxxx?xxx");

				MH_STATUS status = MH_CreateHook(clevel_r_on_key_press_Address, (LPVOID)&clevel_r_on_key_press_Hook,
					reinterpret_cast<LPVOID*>(&clevel_r_on_key_press_Orig));

				if (status == MH_OK) {
					if (MH_EnableHook(clevel_r_on_key_press_Address) != MH_OK) {
						MessageBox(NULL, "MH_EnableHook() != MH_OK", "clevel_r_on_key_press Hook", MB_OK | MB_ICONERROR);
					}
				} else {
					MessageBox(NULL, "MH_CreateHook() != MH_OK", "clevel_r_on_key_press Hook", MB_OK | MB_ICONERROR);
				}
			}
#endif
		}

		if (restore_deleted_commands)
		{
#ifndef _WIN64
			// 8A 54 24 10 8B C1
			cmd_mask_Address = (LPVOID)FindPattern(
				(DWORD)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x8A\x54\x24\x10\x8B\xC1",
				"xxxxxx");

			// c7 05 ? ? ? ? ? ? ? ? 89 1d ? ? ? ? 89 1d ? ? ? ? 89 1d ? ? ? ? e8 ? ? ? ? 83 c4 ? e8
			ps_actor_flags_Address = (unsigned int*) *(DWORD*)(FindPattern(
				(DWORD)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\xc7\x05\x00\x00\x00\x00\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x83\xc4\x00\xe8",
				"xx????????xx????xx????xx????x????xx?x") + 6);

			if (minhook) {
				// B8 ? ? ? ? 53 BB
				LPVOID cmd_register_commands_Address = (LPVOID)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\xB8\x00\x00\x00\x00\x53\xBB",
					"x????xx");

				MH_STATUS status = MH_CreateHook(cmd_register_commands_Address, (LPVOID)&cmd_register_commands_Hook,
					reinterpret_cast<LPVOID*>(&cmd_register_commands_Orig));

				if (status == MH_OK) {
					if (MH_EnableHook(cmd_register_commands_Address) != MH_OK) {
						MessageBox(NULL, "MH_EnableHook() != MH_OK", "cmd_register_commands Hook", MB_OK | MB_ICONERROR);
					}
				}
				else {
					MessageBox(NULL, "MH_CreateHook() != MH_OK", "cmd_register_commands Hook", MB_OK | MB_ICONERROR);
				}
			}
#else
			if (minhook) {
				// 48 89 5C 24 ? 57 48 83 EC 20 8B 05 ? ? ? ? 48 8D 1D ? ? ? ? 48 8D 3D ? ? ? ? A8 01 75 60
				LPVOID cmd_register_commands_Address = (LPVOID)FindPattern(
					(DWORD64)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x8B\x05\x00\x00\x00\x00\x48\x8D\x1D\x00\x00\x00\x00\x48\x8D\x3D\x00\x00\x00\x00\xA8\x01\x75\x60",
					"xxxx?xxxxxxx????xxx????xxx????xxxx");

				MH_STATUS status = MH_CreateHook(cmd_register_commands_Address, (LPVOID)&cmd_register_commands_Hook,
					reinterpret_cast<LPVOID*>(&cmd_register_commands_Orig));

				if (status == MH_OK) {
					if (MH_EnableHook(cmd_register_commands_Address) != MH_OK) {
						MessageBox(NULL, "MH_EnableHook() != MH_OK", "cmd_register_commands Hook", MB_OK | MB_ICONERROR);
					}
				}
				else {
					MessageBox(NULL, "MH_CreateHook() != MH_OK", "cmd_register_commands Hook", MB_OK | MB_ICONERROR);
				}
			}
#endif
		}

		if (g_fly)
		{
#ifdef _WIN64
			// 48 8B C4 48 89 58 10 48 89 68 18 56 57 41 54 41 56 41 57 48 81 EC ? ? ? ? 0F 29 70 C8 0F 29 78 B8 44 0F 29 40
			cflycam_cflycam = (_cflycam_cflycam)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x48\x8B\xC4\x48\x89\x58\x10\x48\x89\x68\x18\x56\x57\x41\x54\x41\x56\x41\x57\x48\x81\xEC\x00\x00\x00\x00\x0F\x29\x70\xC8\x0F\x29\x78\xB8\x44\x0F\x29\x40",
				"xxxxxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx");

			// 48 83 EC 28 48 8B 05 ? ? ? ? 48 85 C0 75 7F 8B 05 ? ? ? ? 48 89 5C 24 ? A8 01 75 1A 83 C8 01 89 05 ? ? ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? E8
			memory = (_memory)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x48\x83\xEC\x28\x48\x8B\x05\x00\x00\x00\x00\x48\x85\xC0\x75\x7F\x8B\x05\x00\x00\x00\x00\x48\x89\x5C\x24\x00\xA8\x01\x75\x1A\x83\xC8\x01\x89\x05\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8D\x0D\x00\x00\x00\x00\xE8",
				"xxxxxxx????xxxxxxx????xxxx?xxxxxxxxx????x????xxx????x");

			// 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 49 8D 40 FF 41 B9 ? ? ? ? 33 F6 48 8B FA 48 8B E9 49 3B C1 77 14
			tlsf_memalign = (_tlsf_memalign)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x49\x8D\x40\xFF\x41\xB9\x00\x00\x00\x00\x33\xF6\x48\x8B\xFA\x48\x8B\xE9\x49\x3B\xC1\x77\x14",
				"xxxx?xxxx?xxxx?xxxxxxxxxxx????xxxxxxxxxxxxx");

			// 48 89 5C 24 ? 48 89 54 24 ? 57 48 83 EC 60 48 8B 02 48 8B DA 48 8B 94 24 ? ? ? ? 0F 29 74 24 ? 0F 29 7C 24 ? 0F 28 F3 48 8B F9 48 8B CB 0F 28 FA FF 90
			camera_manager_play_track = (_camera_manager_play_track)FindPattern(
				(DWORD64)mi.lpBaseOfDll,
				mi.SizeOfImage,
				(BYTE*)"\x48\x89\x5C\x24\x00\x48\x89\x54\x24\x00\x57\x48\x83\xEC\x60\x48\x8B\x02\x48\x8B\xDA\x48\x8B\x94\x24\x00\x00\x00\x00\x0F\x29\x74\x24\x00\x0F\x29\x7C\x24\x00\x0F\x28\xF3\x48\x8B\xF9\x48\x8B\xCB\x0F\x28\xFA\xFF\x90",
				"xxxx?xxxx?xxxxxxxxxxxxxxx????xxxx?xxxx?xxxxxxxxxxxxxx");
#else
			if (isLL)
			{
				// 55 8B EC 83 E4 F0 81 EC ? ? ? ? 53 56 8B F1 57 33 FF 89 7E 04 89 7E 08 81 66 ? ? ? ? ? C7 06 ? ? ? ? C7 46
				cflycam_cflycam = (_cflycam_cflycam)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x55\x8B\xEC\x83\xE4\xF0\x81\xEC\x00\x00\x00\x00\x53\x56\x8B\xF1\x57\x33\xFF\x89\x7E\x04\x89\x7E\x08\x81\x66\x00\x00\x00\x00\x00\xC7\x06\x00\x00\x00\x00\xC7\x46",
					"xxxxxxxx????xxxxxxxxxxxxxxx?????xx????xx");

				// 55 8B EC 83 E4 F8 A1 ? ? ? ? 85 C0 75 48 B8 ? ? ? ? 84 05 ? ? ? ? 75 18 09 05 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 04
				memory = (_memory)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x55\x8B\xEC\x83\xE4\xF8\xA1\x00\x00\x00\x00\x85\xC0\x75\x48\xB8\x00\x00\x00\x00\x84\x05\x00\x00\x00\x00\x75\x18\x09\x05\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x68\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x83\xC4\x04",
					"xxxxxxx????xxxxx????xx????xxxx????x????x????x????xxx");

				// 53 8B 5C 24 0C 55 56 33 ED 57 85 C0 74 19 3D ? ? ? ? 73 12 83 C0 03 83 E0 FC 8B E8 83 F8 0C 77 05 BD
				tlsf_memalign = (void*)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x53\x8B\x5C\x24\x0C\x55\x56\x33\xED\x57\x85\xC0\x74\x19\x3D\x00\x00\x00\x00\x73\x12\x83\xC0\x03\x83\xE0\xFC\x8B\xE8\x83\xF8\x0C\x77\x05\xBD",
					"xxxxxxxxxxxxxxx");

				// 83 EC 10 53 56 8B 74 24 1C 8B 06 8B 50 44 57 8B F9 8B 4C 24 2C 51 8B CE FF D2 F3 0F 10 44 24 ? 8B 06 8B 50 6C 83 EC 08 F3 0F 11 44 24
				camera_manager_play_track = (_camera_manager_play_track)FindPattern(
					(DWORD)mi.lpBaseOfDll,
					mi.SizeOfImage,
					(BYTE*)"\x83\xEC\x10\x53\x56\x8B\x74\x24\x1C\x8B\x06\x8B\x50\x44\x57\x8B\xF9\x8B\x4C\x24\x2C\x51\x8B\xCE\xFF\xD2\xF3\x0F\x10\x44\x24\x00\x8B\x06\x8B\x50\x6C\x83\xEC\x08\xF3\x0F\x11\x44\x24",
					"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx?xxxxxxxxxxxxx");
			}
#endif
		}

#ifndef _WIN64
		g_vs_signals_enabled = getBool("vs_signals", "enabled", false);
		if (g_vs_signals_enabled && !isLL)
			install_vs_signals();
#endif

#ifndef _WIN64
		if (getBool(BOBBING_SECT, "enabled", false) && !isLL)
			install_wpn_bobbing();
#endif
	}
	
	return TRUE;
}