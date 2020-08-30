// dllmain.cpp : DLL 애플리케이션의 진입점을 정의합니다.
#include <iostream>
#include <Windows.h>
#include <cmath>
#include <vector>
#include <time.h>
#include <process.h>
#include <string>
#include "HWBP.h"

#define PLAYER_INFO 0x2AEBC
#define VIEWMATRIX 0x101ae8
#define MyMEM *(int*)(Base + 0x109B74)

using namespace std;

UINT32 Base = 0;

struct ESP_ST
{
	int bottom_x;
	int bottom_y;
	int top_x;
	int top_y;
	string name;
	int mem;
	int dist;
	int view_first_x;
	int view_first_y;
	int view_second_x;
	int view_second_y;
};

struct SelectedTarget
{
	int mem;
	int index;
};

struct Data
{
	int mem;
	string name;
	int health;
	float x;
	float y;
	float z;
	long long tm;
	int team;
};

int WIDTH;
int HEIGHT;
vector<Data> Target;
SelectedTarget SelTarget = { -1, -1 };
bool Aimbot = false;
bool Free = false;
bool TeamMode = false;
bool DamageMode = false;
bool TeleportMode = false;
bool TpMode = false;

pair<float, float> AimCalc(float enemy_x, float enemy_y, float enemy_z);
float AimDist(float yaw, float pitch);
BOOL AllocConsole_t();
LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* e);
void hwbp_init();
void MouseMove(float yaw, float pitch);
void SaveData(int mem, string name, int health, float x, float y, float z, int team);
pair<float, float> TargetSelector();
void AimThread(void* param);
void FreeThread(void* param);
void Beep_Enable(void* param);
void Beep_Disable(void* param);
void tpall();
bool WorldToScreen(float in_x, float in_y, float in_z, float& out_x, float& out_y);
void ESP(void* param);
void DrawString(HDC hWnd, vector<ESP_ST> list);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Base = (INT32)GetModuleHandleW(L"ac_client.exe");
		//DisableThreadLibraryCalls(hModule);
		AllocConsole_t();
		hwbp_init();
		_beginthread(AimThread, 0, 0);
		_beginthread(ESP, 0, 0);
		break;
	}
	return TRUE;
}

#define ExceptionAddress e->ExceptionRecord->ExceptionAddress
LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* e)
{
	CONTEXT* ctx = e->ContextRecord;
	if (ExceptionAddress == (PVOID)(Base + PLAYER_INFO)) // player info
	{
		string name = string((char*)(ctx->Esi + 0x225));
		int health = *(int*)(ctx->Esi + 0xF8);
		float x = *(float*)(ctx->Esi + 0x4);
		float y = *(float*)(ctx->Esi + 0x8);
		float z = *(float*)(ctx->Esi + 0xC);
		int team = *(int*)(ctx->Esi + 0x32C);

		//cout << "yaw: " << *(float*)(ctx->Esi + 0x40) << " pitch: " << *(float*)(ctx->Esi + 0x44) << " x: " << x << " y: " << y << " z: " << z << endl;

		SaveData(ctx->Esi, name, health, x, y, z, team);

		*(int*)(ctx->Esi + 0x3C) = ctx->Eax;
		ctx->Eip += 0x3;
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}

pair<float, float> TargetSelector()
{
	pair<float, float> AimCalc_tmp;
	float min_dist = 10000, AimDist_tmp, yaw = -1, pitch = -1;
	if (SelTarget.mem != -1 && Target.size() && Target[SelTarget.index].mem == SelTarget.mem && /* TargetHealth */*(int*)(Target[SelTarget.index].mem + 0xF8) > 0)
	{
		AimCalc_tmp = AimCalc(Target[SelTarget.index].x, Target[SelTarget.index].y, Target[SelTarget.index].z);
		return { AimCalc_tmp.first, AimCalc_tmp.second };
	}
	else if (SelTarget.mem != -1)
	{
		SelTarget.mem = -1;
		SelTarget.index = -1;
	}
	for (int i = 0; i < (int)Target.size(); i++)
	{
		if (*(int*)( /* TargetHealth */ Target[i].mem + 0xF8) <= 0)
		{
			continue;
		}
		AimCalc_tmp = AimCalc(Target[i].x, Target[i].y, Target[i].z);
		AimDist_tmp = AimDist(AimCalc_tmp.first, AimCalc_tmp.second);
		if (min_dist > AimDist_tmp)
		{
			min_dist = AimDist_tmp;
			yaw = AimCalc_tmp.first;
			pitch = AimCalc_tmp.second;
			SelTarget.mem = Target[i].mem;
			SelTarget.index = i;
		}
	}
	return { yaw, pitch };
}

void SaveData(int mem, string name, int health, float x, float y, float z, int team)
{
	bool inserted = false;
	for (int i = 0; i < (int)Target.size(); i++)
	{
		if (Target[i].mem == mem)
		{
			Target[i].name = name;
			Target[i].health = health;
			Target[i].x = x;
			Target[i].y = y;
			Target[i].z = z;
			Target[i].tm = clock();
			Target[i].team = team;
			inserted = true;
		}
		else if (clock() - Target[i].tm > 0.5 * CLOCKS_PER_SEC)
		{
			if (SelTarget.index > i)
			{
				SelTarget.index--;
			}
			if (SelTarget.index == i)
			{
				SelTarget.index = -1;
				SelTarget.mem = -1;
			}
			Target.erase(Target.begin() + i);
		}
	}
	if (!inserted)
	{
		Target.push_back({ mem, name, health, x, y, z, clock() });
	}
}

void MouseMove(float yaw, float pitch)
{
	*(float*)(MyMEM + 0x40) = yaw;
	*(float*)(MyMEM + 0x44) = pitch;
}

pair<float, float> AimCalc(float enemy_x, float enemy_y, float enemy_z)
{
	float my_x = *(float*)(MyMEM + 0x4);
	float my_y = *(float*)(MyMEM + 0x8);
	float my_z = *(float*)(MyMEM + 0xC);

	float x = enemy_x - my_x;
	float y = enemy_y - my_y;
	float z = enemy_z - my_z;

	float dist = sqrt(pow(x, 2) + pow(y, 2));

	float pitch = (float)(atan2((double)z, (double)dist) / 3.141592 * 180.0);
	float yaw = (float)(atan2((double)y, (double)x) / 3.141592 * 180.0 + 90.0);

	return { yaw, pitch };
}

float AimDist(float yaw_e, float pitch_e)
{
	float yaw_n = *(float*)(MyMEM + 0x40);
	float pitch_n = *(float*)(MyMEM + 0x44);

	float n1 = (float)sqrt(pow((double)yaw_e - (double)yaw_n, 2) + pow((double)pitch_e - (double)pitch_n, 2));
	float n2 = (float)sqrt(pow((double)yaw_e + 360 - (double)yaw_n, 2) + pow((double)pitch_e - (double)pitch_n, 2));
	float n3 = (float)sqrt(pow((double)yaw_e - 360 - (double)yaw_n, 2) + pow((double)pitch_e - (double)pitch_n, 2));

	return min(n1, min(n2, n3));
}

PVOID ExceptionHandlerHandle = NULL;
void hwbp_init()
{
	HANDLE hMainThread = HWBP->GetMainThread();
	ExceptionHandlerHandle = AddVectoredExceptionHandler(1, ExceptionHandler);
	CONTEXT c{};
	c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	SuspendThread(hMainThread);
	c.Dr0 = Base + PLAYER_INFO;
	c.Dr7 = (1 << 0);
	SetThreadContext(hMainThread, &c);
	ResumeThread(hMainThread);
}

BOOL AllocConsole_t()
{
	BOOL bAlloc = AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
	SetConsoleTitleA("");
	return bAlloc;
}

#define PI 3.1415926535897
double getRadian(int rotate) {
	return rotate * (PI / 180);
}
void front(float& x, float& y, int rotate, float length) {
	x += length * cos(getRadian(rotate - 90));
	y += length * sin(getRadian(rotate - 90));
	//printf("x = %f, y = %f\n", x, y);
}
void left(float& x, float& y, int rotate, float length) {
	x += length * cos(getRadian(rotate + 180));
	y += length * sin(getRadian(rotate + 180));

	//printf("x = %f, y = %f\n", x, y);
}
void right(float& x, float& y, int rotate, float length) {
	x += length * cos(getRadian(rotate));
	y += length * sin(getRadian(rotate));

	//printf("x = %f, y = %f\n", x, y);
}
void behind(float& x, float& y, int rotate, float length) {
	x += length * cos(getRadian(rotate + 90));
	y += length * sin(getRadian(rotate + 90));
	//printf("x = %f, y = %f\n", x, y);
}

void FreeThread(void* param) {
	/**(float*)(MyMEM + 0x34) = *(float*)(Target[SelTarget.index].mem + 0x34);
	*(float*)(MyMEM + 0x38) = *(float*)(Target[SelTarget.index].mem + 0x38);
	*(float*)(MyMEM + 0x3C) = *(float*)(Target[SelTarget.index].mem + 0x3C);*/
	float x = *(float*)(MyMEM + 0x34);
	float y = *(float*)(MyMEM + 0x38);
	float z = *(float*)(MyMEM + 0x3C);
	float speed = 0.025;
	float len = 1;
	while (Free) {
		float yaw = *(float*)(MyMEM + 0x40);
		float pitch = *(float*)(MyMEM + 0x44);

		if (GetAsyncKeyState('W')) {
			front(x, y, yaw, speed);
		}
		if (GetAsyncKeyState('A')) {
			left(x, y, yaw, speed);
		}
		if (GetAsyncKeyState('S')) {
			behind(x, y, yaw, speed);
		}
		if (GetAsyncKeyState('D')) {
			right(x, y, yaw, speed);
		}
		if (GetAsyncKeyState(VK_SPACE)) {
			z += speed;
		}
		if (GetAsyncKeyState(VK_LSHIFT) || GetAsyncKeyState(VK_RSHIFT) || GetAsyncKeyState(VK_SHIFT)) {
			z -= speed;
		}
		
		*(float*)(MyMEM + 0x34) = x;
		*(float*)(MyMEM + 0x38) = y;
		*(float*)(MyMEM + 0x3C) = z;
		Sleep(1);
	}
}

void AimThread(void* param)
{
	pair<float, float> tmp;
	Aimbot = true;
	while (true)
	{
		__try
		{
			if (GetAsyncKeyState(VK_F1) & 0x0001)
			{
				Free = !Free;
				if (Free)
				{
					_beginthread(FreeThread, 0, 0);
					_beginthread(Beep_Enable, 0, 0);
				}
				else
				{
					_beginthread(Beep_Disable, 0, 0);
				}
			}
			/*if (GetAsyncKeyState(VK_F2) & 0x0001)
			{
				DamageMode = !DamageMode;
				if (DamageMode)
				{
					_beginthread(Beep_Enable, 0, 0);
				}
				else
				{
					_beginthread(Beep_Disable, 0, 0);
				}
			}
			if (GetAsyncKeyState(VK_F3) & 0x0001)
			{
				TeleportMode = !TeleportMode;
				if (TeleportMode)
				{
					_beginthread(Beep_Enable, 0, 0);
				}
				else
				{
					_beginthread(Beep_Disable, 0, 0);
				}
			}
			if (GetAsyncKeyState(VK_F4) & 0x0001)
			{
				TpMode = !TpMode;
				if (TpMode)
				{
					_beginthread(Beep_Enable, 0, 0);
				}
				else
				{
					_beginthread(Beep_Disable, 0, 0);
				}
			}
			if (GetAsyncKeyState(VK_F5) & 0x0001)
			{
				TeamMode = !TeamMode;
				if (TeamMode)
				{
					_beginthread(Beep_Enable, 0, 0);
				}
				else
				{
					_beginthread(Beep_Disable, 0, 0);
				}
			}*/

			if (TpMode) // tpAll
			{
				tpall();
			}

			if (GetAsyncKeyState(VK_RBUTTON) && TeleportMode) // teleport
			{
				if (SelTarget.mem != -1)
				{
					*(float*)(MyMEM + 0x34) = *(float*)(Target[SelTarget.index].mem + 0x34);
					*(float*)(MyMEM + 0x38) = *(float*)(Target[SelTarget.index].mem + 0x38);
					*(float*)(MyMEM + 0x3C) = *(float*)(Target[SelTarget.index].mem + 0x3C);
				}
			}

			if (GetAsyncKeyState(VK_LBUTTON) || GetAsyncKeyState(VK_RBUTTON)) // aimbot
			{
				tmp = TargetSelector();
				if (tmp.first != -1 && tmp.second != -1)
				{
					if (Aimbot)
					{
						MouseMove(tmp.first, tmp.second);
					}
				}
			}
			else
			{
				SelTarget.mem = -1;
				SelTarget.index = -1;
			}
		}
		__finally
		{
			Sleep(1);
		}
	}
}

void Beep_Enable(void* param)
{
	Beep(2000, 200);
}

void Beep_Disable(void* param)
{
	Beep(1000, 200);
}

void tpall()
{
	float my_x = *(float*)(MyMEM + 0x34);
	float my_y = *(float*)(MyMEM + 0x38);
	float my_z = *(float*)(MyMEM + 0x3C);

	for (int i = 0; i < (int)Target.size(); i++)
	{
		*(float*)(Target[i].mem + 0x34) = my_x;
		*(float*)(Target[i].mem + 0x38) = my_y;
		*(float*)(Target[i].mem + 0x3C) = my_z;
	}
}

bool WorldToScreen(float in_x, float in_y, float in_z, float& out_x, float& out_y)
{
	float* viewMatrix = (float*)(Base + VIEWMATRIX);
	out_x = in_x * viewMatrix[0] + in_y * viewMatrix[4] + in_z * viewMatrix[8] + viewMatrix[12];
	out_y = in_x * viewMatrix[1] + in_y * viewMatrix[5] + in_z * viewMatrix[9] + viewMatrix[13];
	float w = in_x * viewMatrix[3] + in_y * viewMatrix[7] + in_z * viewMatrix[11] + viewMatrix[15];

	if (w < 0.0)
	{
		return false;
	}

	out_x /= w;
	out_x *= WIDTH / 2.0f;
	out_x += WIDTH / 2.0f;
	out_y /= w;
	out_y *= -HEIGHT / 2.0f;
	out_y += HEIGHT / 2.0f;
	return true;
}

HPEN hPen1 = CreatePen(PS_SOLID, 2, RGB(255, 0, 0)); // non-targeting
HPEN hPen2 = CreatePen(PS_SOLID, 2, RGB(0, 255, 0)); // targeting
HPEN hPen3 = CreatePen(PS_SOLID, 10, RGB(255, 0, 0));
HPEN hPen4 = CreatePen(PS_SOLID, 10, RGB(0, 255, 0));
void DrawString(HDC hdc, vector<ESP_ST> list) //draws string to HDC.
{
	for (int i = 0; i < (int)list.size(); i++)
	{
		if (SelTarget.mem == list[i].mem) // Box ESP
		{
			SelectObject(hdc, hPen2);
			MoveToEx(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y, NULL);
			LineTo(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y);
			MoveToEx(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].bottom_y, NULL);
			LineTo(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].bottom_y);
			MoveToEx(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y, NULL);
			LineTo(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].bottom_y);
			MoveToEx(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y, NULL);
			LineTo(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].bottom_y);
		}
		else
		{
			SelectObject(hdc, hPen1);
			MoveToEx(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y, NULL);
			LineTo(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y);
			MoveToEx(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].bottom_y, NULL);
			LineTo(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].bottom_y);
			MoveToEx(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y, NULL);
			LineTo(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].bottom_y);
			MoveToEx(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y, NULL);
			LineTo(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].bottom_y);
		}

		SelectObject(hdc, hPen2);
		MoveToEx(hdc, list[i].view_first_x, list[i].view_first_y, NULL);
		LineTo(hdc, list[i].view_second_x, list[i].view_second_y);

		SelectObject(hdc, hPen1); // Line ESP
		MoveToEx(hdc, WIDTH / 2, HEIGHT, NULL);
		LineTo(hdc, list[i].top_x, list[i].bottom_y);

		int health = *(int*)(Target[i].mem + 0xF8); // HP Bar
		//SelectObject(hdc, hPen3);
		//MoveToEx(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y - HEIGHT / 70, NULL);
		//LineTo(hdc, list[i].top_x + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y - HEIGHT / 70);
		SelectObject(hdc, hPen4);
		MoveToEx(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4), list[i].top_y - HEIGHT / 70, NULL);
		LineTo(hdc, list[i].top_x - (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 4) + (int)(((double)list[i].bottom_y - (double)list[i].top_y) / 2) * health / 100, list[i].top_y - HEIGHT / 70);
	}
	//DeleteObject(hPen1);
	//DeleteObject(hPen2);
	//DeleteObject(hdc);
}

void ESP(void* param)
{
	HWND hWnd = FindWindowA(NULL, "AssaultCube");
	HDC hdc = GetDC(hWnd);
	while (true)
	{
		vector<ESP_ST> list;
		RECT rect;
		float out_bottom_x, out_bottom_y, out_top_x, out_top_y;
		GetWindowRect(hWnd, &rect);
		WIDTH = rect.right - rect.left - 6;
		HEIGHT = rect.bottom - rect.top - 29;
		for (int i = 0; i < (int)Target.size(); i++)
		{
			if (WorldToScreen(Target[i].x, Target[i].y, Target[i].z + 0.7, out_bottom_x, out_bottom_y) && /* TargetHealth */*(int*)(Target[i].mem + 0xF8) > 0)
			{
				WorldToScreen(*(float*)(Target[i].mem + 0x34), *(float*)(Target[i].mem + 0x38), *(float*)(Target[i].mem + 0x3c), out_top_x, out_top_y);
				float my_x = *(float*)(MyMEM + 0x34);
				float my_y = *(float*)(MyMEM + 0x38);
				float my_z = *(float*)(MyMEM + 0x3C);
				float dist = (float)sqrt(pow((double)Target[i].x - (double)my_x, 2.0) + pow((double)Target[i].y - (double)my_y, 2.0) + pow((double)Target[i].z - (double)my_z, 2.0));

				float yaw = *(float*)(Target[i].mem + 0x40);
				float pitch = *(float*)(Target[i].mem + 0x44);
				float x = Target[i].x, y = Target[i].y, z = Target[i].z;
				float len = 3.0;

				float view_first_x, view_first_y, view_second_x, view_second_y;
				WorldToScreen(Target[i].x, Target[i].y, Target[i].z + 0.5, view_first_x, view_first_y);
				if (yaw < 90) {
					y += -(90 - yaw) / 90 * len;
					x += yaw / 90 * len;
				}
				else if (yaw < 180) {
					y += (yaw - 90) / 90 * len;
					x += (90 - (yaw - 90)) / 90 * len;
				}
				else if (yaw < 270) {
					y += (90 - (yaw - 180)) / 90 * len;
					x += -(yaw - 180) / 90 * len;
				}
				else {
					y += -(yaw - 270) / 90 * len;
					x += -(90 - (yaw - 270)) / 90 * len;
				}

				double R = len;
				double RADIAN = pitch * (3.1415926535 / 180);
				double X = cos(RADIAN) * R;
				double Y = sin(RADIAN) * R;

				//x /= R / X;
				//y /= R / X;

				//cout << x << " " << y << " " << z << " " << z + Y << endl;
				z += Y;

				WorldToScreen(x, y, z + 0.5, view_second_x, view_second_y);

				list.push_back({ (int)out_top_x, (int)out_top_y, (int)out_bottom_x, (int)out_bottom_y, Target[i].name, Target[i].mem, (int)dist, (int)view_first_x, (int)view_first_y, (int)view_second_x, (int)view_second_y });
			}
		}
		DrawString(hdc, list);
	}
}