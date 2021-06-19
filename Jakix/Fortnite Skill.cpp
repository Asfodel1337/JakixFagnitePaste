#include "win_utils.h"
#include <dwmapi.h>
#include "Main.h"
#include <random>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <Lmcons.h>
#include <Windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <tchar.h>

#define OFFSET_UWORLD 0x985c930

ImFont* m_pFont;


#include <winternl.h>
#include "Imgui/imgui_internal.h"
#include "xorstr.hpp"
typedef NTSTATUS(NTAPI* pdef_NtRaiseHardError)(NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask OPTIONAL, PULONG_PTR Parameters, ULONG ResponseOption, PULONG Response);
typedef NTSTATUS(NTAPI* pdef_RtlAdjustPrivilege)(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);

DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
DWORD_PTR PlayerState;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Persistentlevel;
DWORD_PTR Ulevel;


std::string name = ("");
std::string ownerid = ("");
std::string secret = ("");
std::string version = ("1.0");

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

Vector3 localactorpos;

uint64_t TargetPawn;
int localplayerID;

bool isaimbotting;

RECT GameRect = { NULL };
D3DPRESENT_PARAMETERS d3dpp;

DWORD ScreenCenterX;
DWORD ScreenCenterY;
DWORD ScreenCenterZ;

static void xCreateWindow();
static void xInitD3d();
static void xMainLoop();
static LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND Window = NULL;
IDirect3D9Ex* p_Object = NULL;
static LPDIRECT3DDEVICE9 D3dDevice = NULL;
static LPDIRECT3DVERTEXBUFFER9 TriBuf = NULL;

FTransform GetBoneIndex(DWORD_PTR mesh, int index){
	DWORD_PTR bonearray = read<DWORD_PTR>(DriverHandle, processID, mesh + 0x4A8);
	if (bonearray == NULL)	{
		bonearray = read<DWORD_PTR>(DriverHandle, processID, mesh + 0x4A8 + 0x10);
	}
	return read<FTransform>(DriverHandle, processID, bonearray + (index * 0x30));
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id) {
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = read<FTransform>(DriverHandle, processID, mesh + 0x1C0);
	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());
	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

D3DMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0)) {
	float radPitch = (rot.x * float(M_PI) / 180.f);
	float radYaw = (rot.y * float(M_PI) / 180.f);
	float radRoll = (rot.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

extern Vector3 CameraEXT(0, 0, 0);

Vector3 ProjectWorldToScreen(Vector3 WorldLocation) {
	Vector3 Screenlocation = Vector3(0, 0, 0);
	Vector3 Camera;

	auto chain69 = read<uintptr_t>(DriverHandle, processID, Localplayer + 0xa8);
	uint64_t chain699 = read<uintptr_t>(DriverHandle, processID, chain69 + 8);

	Camera.x = read<float>(DriverHandle, processID, chain699 + 0x7F8);
	Camera.y = read<float>(DriverHandle, processID, Rootcomp + 0x12C);

	float test = asin(Camera.x);
	float degrees = test * (180.0 / M_PI);
	Camera.x = degrees;

	if (Camera.y < 0)
		Camera.y = 360 + Camera.y;

	D3DMATRIX tempMatrix = Matrix(Camera);
	Vector3 vAxisX, vAxisY, vAxisZ;

	vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	uint64_t chain = read<uint64_t>(DriverHandle, processID, Localplayer + 0x70);
	uint64_t chain1 = read<uint64_t>(DriverHandle, processID, chain + 0x98);
	uint64_t chain2 = read<uint64_t>(DriverHandle, processID, chain1 + 0x130);

	Vector3 vDelta = WorldLocation - read<Vector3>(DriverHandle, processID, chain2 + 0x10);
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	float zoom = read<float>(DriverHandle, processID, chain699 + 0x590);

	float FovAngle = 80.0f / (zoom / 1.19f);

	float ScreenCenterX = Width / 2;
	float ScreenCenterY = Height / 2;
	float ScreenCenterZ = Height / 2;

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.z = ScreenCenterZ - vTransformed.z * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;

	return Screenlocation;
}

DWORD Menuthread(LPVOID in) {
	while (1) {
		if (GetAsyncKeyState(VK_F8) & 1) {
			item.Show_Menu = !item.Show_Menu;
		}
		Sleep(2);
	}
}

Vector3 AimbotCorrection(float bulletVelocity, float bulletGravity, float targetDistance, Vector3 targetPosition, Vector3 targetVelocity) {
	Vector3 recalculated = targetPosition;
	float gravity = fabs(bulletGravity);
	float time = targetDistance / fabs(bulletVelocity);
	/* Bullet drop correction */
	float bulletDrop = (gravity / 250) * time * time;
	recalculated.z += bulletDrop * 120;
	/* Player movement correction */
	recalculated.x += time * (targetVelocity.x);
	recalculated.y += time * (targetVelocity.y);
	recalculated.z += time * (targetVelocity.z);
	return recalculated;
}

void ShadowRGBText(int x, int y, ImColor color, const char* str)
{
	ImFont a;
	std::string utf_8_1 = std::string(str);
	std::string utf_8_2 = string_To_UTF8(utf_8_1);
	ImGui::GetOverlayDrawList()->AddText(ImVec2(x + 1, y + 2), ImGui::ColorConvertFloat4ToU32(ImColor(0, 0, 0, 0)), utf_8_2.c_str());
	ImGui::GetOverlayDrawList()->AddText(ImVec2(x + 1, y + 1), ImGui::ColorConvertFloat4ToU32(ImColor(0, 0, 0, 200)), utf_8_2.c_str());
	ImGui::GetOverlayDrawList()->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(color)), utf_8_2.c_str());
}

void aimbot(float x, float y, float z) {
	float ScreenCenterX = (Width / 2);
	float ScreenCenterY = (Height / 2);
	float ScreenCenterZ = (Depth / 2);
	int AimSpeed = item.Aim_Speed;
	float TargetX = 0;
	float TargetY = 0;
	float TargetZ = 0;

	if (x != 0) {
		if (x > ScreenCenterX) {
			TargetX = -(ScreenCenterX - x);
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX > ScreenCenterX * 2) TargetX = 0;
		}

		if (x < ScreenCenterX) {
			TargetX = x - ScreenCenterX;
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX < 0) TargetX = 0;
		}
	}

	if (y != 0) {
		if (y > ScreenCenterY) {
			TargetY = -(ScreenCenterY - y);
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY > ScreenCenterY * 2) TargetY = 0;
		}

		if (y < ScreenCenterY) {
			TargetY = y - ScreenCenterY;
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY < 0) TargetY = 0;
		}
	}

	if (z != 0) {
		if (z > ScreenCenterZ) {
			TargetZ = -(ScreenCenterZ - z);
			TargetZ /= AimSpeed;
			if (TargetZ + ScreenCenterZ > ScreenCenterZ * 2) TargetZ = 0;
		}

		if (z < ScreenCenterZ) {
			TargetZ = z - ScreenCenterZ;
			TargetZ /= AimSpeed;
			if (TargetZ + ScreenCenterZ < 0) TargetZ = 0;
		}
	}

	//WriteAngles(TargetX / 3.5f, TargetY / 3.5f, TargetZ / 3.5f);
	mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(TargetX), static_cast<DWORD>(TargetY), NULL, NULL);
	if (item.Auto_Fire) {
		mouse_event(MOUSEEVENTF_LEFTDOWN, DWORD(NULL), DWORD(NULL), DWORD(0x0002), ULONG_PTR(NULL));
		mouse_event(MOUSEEVENTF_LEFTUP, DWORD(NULL), DWORD(NULL), DWORD(0x0004), ULONG_PTR(NULL));
	}

	
	return;
}

double GetCrossDistance(double x1, double y1, double z1, double x2, double y2, double z2) {
	return sqrt(pow((x2 - x1), 2) + pow((y2 - y1), 2));
}

typedef struct _FNlEntity {
	uint64_t Actor;
	int ID;
	uint64_t mesh;
}FNlEntity;

std::vector<FNlEntity> entityList;

void cache()
{
	while (true) {
		std::vector<FNlEntity> tmpList;

		Uworld = read<DWORD_PTR>(DriverHandle, processID, base_address + OFFSET_UWORLD);
		DWORD_PTR Gameinstance = read<DWORD_PTR>(DriverHandle, processID, Uworld + 0x180);
		DWORD_PTR LocalPlayers = read<DWORD_PTR>(DriverHandle, processID, Gameinstance + 0x38);
		Localplayer = read<DWORD_PTR>(DriverHandle, processID, LocalPlayers);
		PlayerController = read<DWORD_PTR>(DriverHandle, processID, Localplayer + 0x30);
		LocalPawn = read<DWORD_PTR>(DriverHandle, processID, PlayerController + 0x2A0);

		PlayerState = read<DWORD_PTR>(DriverHandle, processID, LocalPawn + 0x240);
		Rootcomp = read<DWORD_PTR>(DriverHandle, processID, LocalPawn + 0x130);

		if (LocalPawn != 0) {
			localplayerID = read<int>(DriverHandle, processID, LocalPawn + 0x18);
		}

		Persistentlevel = read<DWORD_PTR>(DriverHandle, processID, Uworld + 0x30);
		DWORD ActorCount = read<DWORD>(DriverHandle, processID, Persistentlevel + 0xA0);
		DWORD_PTR AActors = read<DWORD_PTR>(DriverHandle, processID, Persistentlevel + 0x98);

		for (int i = 0; i < ActorCount; i++) {
			uint64_t CurrentActor = read<uint64_t>(DriverHandle, processID, AActors + i * 0x8);

			int curactorid = read<int>(DriverHandle, processID, CurrentActor + 0x18);

			if (curactorid == localplayerID || curactorid == localplayerID + 765) {
				FNlEntity fnlEntity{ };
				fnlEntity.Actor = CurrentActor;
				fnlEntity.mesh = read<uint64_t>(DriverHandle, processID, CurrentActor + 0x280);
				fnlEntity.ID = curactorid;
				tmpList.push_back(fnlEntity);
			}
		}
		entityList = tmpList;
		Sleep(1);
	}
}

void AimAt(DWORD_PTR entity) {
	uint64_t currentactormesh = read<uint64_t>(DriverHandle, processID, entity + 0x280);
	auto rootHead = GetBoneWithRotation(currentactormesh, item.hitbox);
	//Vector3 rootHeadOut = ProjectWorldToScreen(rootHead);

	if (item.Aim_Prediction) {
		float distance = localactorpos.Distance(rootHead) / 250;
		uint64_t CurrentActorRootComponent = read<uint64_t>(DriverHandle, processID, entity + 0x130);
		Vector3 vellocity = read<Vector3>(DriverHandle, processID, CurrentActorRootComponent + 0x140);
		Vector3 Predicted = AimbotCorrection(30000, -504, distance, rootHead, vellocity);
		Vector3 rootHeadOut = ProjectWorldToScreen(Predicted);
		if (rootHeadOut.x != 0 || rootHeadOut.y != 0 || rootHeadOut.z != 0) {
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z, Width / 2, Height / 2, Depth / 2) <= item.AimFOV * 1)) {
				aimbot(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z);				
			}
		}
	}
	else {
		Vector3 rootHeadOut = ProjectWorldToScreen(rootHead);
		if (rootHeadOut.x != 0 || rootHeadOut.y != 0 || rootHeadOut.z != 0) {
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z, Width / 2, Height / 2, Depth / 2) <= item.AimFOV * 1)) {
				aimbot(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z);
			}
		}
	}
}

void AimAt2(DWORD_PTR entity) {
	uint64_t currentactormesh = read<uint64_t>(DriverHandle, processID, entity + 0x280);
	auto rootHead = GetBoneWithRotation(currentactormesh, item.hitbox);

	if (item.Aim_Prediction) {
		float distance = localactorpos.Distance(rootHead) / 250;
		uint64_t CurrentActorRootComponent = read<uint64_t>(DriverHandle, processID, entity + 0x130);
		Vector3 vellocity = read<Vector3>(DriverHandle, processID, CurrentActorRootComponent + 0x140);
		Vector3 Predicted = AimbotCorrection(30000, -504, distance, rootHead, vellocity);
		Vector3 rootHeadOut = ProjectWorldToScreen(Predicted);
		if (rootHeadOut.x != 0 || rootHeadOut.y != 0 || rootHeadOut.z != 0) {
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z, Width / 2, Height / 2, Depth / 2) <= item.AimFOV * 1)) {
				if (item.Lock_Line) {
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2), ImVec2(rootHeadOut.x, rootHeadOut.y), IM_COL32(143, 0, 255, 255), item.Thickness);
				}
			}
		}
	}
	else {
		Vector3 rootHeadOut = ProjectWorldToScreen(rootHead);
		if (rootHeadOut.x != 0 || rootHeadOut.y != 0 || rootHeadOut.z != 0) {
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, rootHeadOut.z, Width / 2, Height / 2, Depth / 2) <= item.AimFOV * 1)) {
				if (item.Lock_Line) {
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2), ImVec2(rootHeadOut.x, rootHeadOut.y), IM_COL32(143, 0, 255, 255), item.Thickness);
				}
			}
		}
	}
}

void DrawESP() {
	if (item.Draw_FOV_Circle) {
		ImGui::GetOverlayDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), float(item.AimFOV), IM_COL32(0, 0, 0, 255), item.Shape, item.Thickness);
	}
	if (item.Cross_Hair) {
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2 - 12, Height / 2), ImVec2(Width / 2 - 2, Height / 2), ImGui::GetColorU32({ 14, 14, 14, 255.f }), 2.0f);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2 + 13, Height / 2), ImVec2(Width / 2 + 3, Height / 2), ImGui::GetColorU32({ 14, 14, 14, 255.f }), 2.0f);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2 - 12), ImVec2(Width / 2, Height / 2 - 2), ImGui::GetColorU32({ 14, 14, 14, 255.f }), 2.0f);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 2 + 13), ImVec2(Width / 2, Height / 2 + 3), ImGui::GetColorU32({ 14, 14, 14, 255.f }), 2.0f);
	}

	char dist[64];
	//sprintf_s(dist, "%.1f Fps\n", ImGui::GetIO().Framerate);
	//ImGui::GetOverlayDrawList()->AddText(ImVec2(15, 15), ImGui::GetColorU32({ color.Black[0], color.Black[1], color.Black[2], 4.0f }), dist);

	auto entityListCopy = entityList;
	float closestDistance = FLT_MAX;
	DWORD_PTR closestPawn = NULL;

	DWORD_PTR AActors = read<DWORD_PTR>(DriverHandle, processID, Ulevel + 0x98);

	if (AActors == (DWORD_PTR)nullptr)
		return;

	for (unsigned long i = 0; i < entityListCopy.size(); ++i) {
		FNlEntity entity = entityListCopy[i];

		uint64_t CurrentActor = read<uint64_t>(DriverHandle, processID, AActors + i * 0x8);

		uint64_t CurActorRootComponent = read<uint64_t>(DriverHandle, processID, entity.Actor + 0x130);
		if (CurActorRootComponent == (uint64_t)nullptr || CurActorRootComponent == -1 || CurActorRootComponent == NULL)
			continue;

		Vector3 actorpos = read<Vector3>(DriverHandle, processID, CurActorRootComponent + 0x11C);
		Vector3 actorposW2s = ProjectWorldToScreen(actorpos);

		DWORD64 otherPlayerState = read<uint64_t>(DriverHandle, processID, entity.Actor + 0x240);
		if (otherPlayerState == (uint64_t)nullptr || otherPlayerState == -1 || otherPlayerState == NULL)
			continue;

		localactorpos = read<Vector3>(DriverHandle, processID, Rootcomp + 0x11C);

		Vector3 bone66 = GetBoneWithRotation(entity.mesh, 66);
		Vector3 bone0 = GetBoneWithRotation(entity.mesh, 0);

		Vector3 top = ProjectWorldToScreen(bone66);
		Vector3 aimbotspot = ProjectWorldToScreen(bone66);
		Vector3 bottom = ProjectWorldToScreen(bone0);

		Vector3 Head = ProjectWorldToScreen(Vector3(bone66.x, bone66.y, bone66.z + 15));

		float distance = localactorpos.Distance(bone66) / 100.f;
		float BoxHeight = (float)(Head.y - bottom.y);
		float CornerHeight = abs(Head.y - bottom.y);
		float CornerWidth = BoxHeight / 2.0f;


		int MyTeamId = read<int>(DriverHandle, processID, PlayerState + 0xED0);
		int ActorTeamId = read<int>(DriverHandle, processID, otherPlayerState + 0xED0);
		int curactorid = read<int>(DriverHandle, processID, CurrentActor + 0x18);

		if (MyTeamId != ActorTeamId) {
			if (distance < item.VisDist) {
				if (item.Esp_line) {
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 100), ImVec2(bottom.x, bottom.y), IM_COL32(143, 0, 255, 255), item.Thickness);
				}
				if (item.Head_dot) {
					ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(Head.x, Head.y), float(BoxHeight / 25), ImGui::GetColorU32({ item.Headdot[0], item.Headdot[1], item.Headdot[2], item.Transparency }), 50);
				}
				if (item.Triangle_ESP_Filled) {
					ImGui::GetOverlayDrawList()->AddTriangleFilled(ImVec2(Head.x, Head.y - 45), ImVec2(bottom.x - (BoxHeight / 2), bottom.y), ImVec2(bottom.x + (BoxHeight / 2), bottom.y), ImGui::GetColorU32({ item.TriangleESPFilled[0], item.TriangleESPFilled[1], item.TriangleESPFilled[2], item.Transparency }));
				}
				if (item.Triangle_ESP) {
					ImGui::GetOverlayDrawList()->AddTriangle(ImVec2(Head.x, Head.y - 50), ImVec2(bottom.x - (BoxHeight / 2), bottom.y), ImVec2(bottom.x + (BoxHeight / 2), bottom.y), ImGui::GetColorU32({ item.TriangleESP[0], item.TriangleESP[1], item.TriangleESP[2], 1.0f }), item.Thickness);
				}
				if (item.Esp_box_fill) {
					ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), ImGui::GetColorU32({ item.Espboxfill[0], item.Espboxfill[1], item.Espboxfill[2], item.Transparency }));
				}
				if (item.Esp_box) {
					ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), IM_COL32(0, 0, 0, 150));
					ImGui::GetOverlayDrawList()->AddRect(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), IM_COL32(143, 0, 255, 255));
				}
				if (item.Esp_Corner_Box) {
					DrawCornerBox(Head.x - (CornerWidth / 2), Head.y, CornerWidth, CornerHeight, ImGui::GetColorU32({ item.BoxCornerESP[0], item.BoxCornerESP[1], item.BoxCornerESP[2], 1.0f }), item.Thickness);
				}
				if (item.Esp_Circle_Fill) {
					ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(Head.x, Head.y), float(BoxHeight), ImGui::GetColorU32({ item.EspCircleFill[0], item.EspCircleFill[1], item.EspCircleFill[2], item.Transparency }), item.Shape);
				}
				if (item.Esp_Circle) {
					ImGui::GetOverlayDrawList()->AddCircle(ImVec2(Head.x, Head.y), float(BoxHeight), ImGui::GetColorU32({ item.EspCircle[0], item.EspCircle[1], item.EspCircle[2], 1.0f }), item.Shape, item.Thickness);
				}
				if (item.Distance_Esp) {
					char dist[64];
					sprintf_s(dist, "[%.fm]", distance);
					ShadowRGBText(bottom.x - 15, bottom.y, IM_COL32(143, 0, 255, 255), dist);
				}
				if (item.Aimbot) {
					auto dx = aimbotspot.x - (Width / 2);
					auto dy = aimbotspot.y - (Height / 2);
					auto dz = aimbotspot.z - (Depth / 2);
					auto dist = sqrtf(dx * dx + dy * dy + dz * dz) / 100.0f;
					if (dist < item.AimFOV && dist < closestDistance) {
						closestDistance = dist;
						closestPawn = entity.Actor;
					}
				}
			}			
		}
		else if (CurActorRootComponent != CurrentActor) {
			if (distance > 2) {
				if (item.TeamESP)
				{
					if (item.Esp_line) {
						ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 100), ImVec2(bottom.x, bottom.y), IM_COL32(143, 0, 255, 255), item.Thickness);
					}
					if (item.Head_dot) {
						ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(Head.x, Head.y), float(BoxHeight / 25), ImGui::GetColorU32({ item.Headdot[0], item.Headdot[1], item.Headdot[2], item.Transparency }), 50);
					}
					if (item.Triangle_ESP_Filled) {
						ImGui::GetOverlayDrawList()->AddTriangleFilled(ImVec2(Head.x, Head.y - 45), ImVec2(bottom.x - (BoxHeight / 2), bottom.y), ImVec2(bottom.x + (BoxHeight / 2), bottom.y), ImGui::GetColorU32({ item.TriangleESPFilled[0], item.TriangleESPFilled[1], item.TriangleESPFilled[2], item.Transparency }));
					}
					if (item.Triangle_ESP) {
						ImGui::GetOverlayDrawList()->AddTriangle(ImVec2(Head.x, Head.y - 50), ImVec2(bottom.x - (BoxHeight / 2), bottom.y), ImVec2(bottom.x + (BoxHeight / 2), bottom.y), ImGui::GetColorU32({ item.TriangleESP[0], item.TriangleESP[1], item.TriangleESP[2], 1.0f }), item.Thickness);
					}
					if (item.Esp_box_fill) {
						ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), ImGui::GetColorU32({ item.Espboxfill[0], item.Espboxfill[1], item.Espboxfill[2], item.Transparency }));
					}
					if (item.Esp_box) {
						ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), IM_COL32(0, 0, 0, 150));
						ImGui::GetOverlayDrawList()->AddRect(ImVec2(Head.x - (CornerWidth / 2), Head.y), ImVec2(bottom.x + (CornerWidth / 2), bottom.y), IM_COL32(143, 0, 255, 255));
					}
					if (item.Esp_Corner_Box) {
						DrawCornerBox(Head.x - (CornerWidth / 2), Head.y, CornerWidth, CornerHeight, ImGui::GetColorU32({ item.BoxCornerESP[0], item.BoxCornerESP[1], item.BoxCornerESP[2], 1.0f }), item.Thickness);
					}
					if (item.Esp_Circle_Fill) {
						ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(Head.x, Head.y), float(BoxHeight), ImGui::GetColorU32({ item.EspCircleFill[0], item.EspCircleFill[1], item.EspCircleFill[2], item.Transparency }), item.Shape);
					}
					if (item.Esp_Circle) {
						ImGui::GetOverlayDrawList()->AddCircle(ImVec2(Head.x, Head.y), float(BoxHeight), ImGui::GetColorU32({ item.EspCircle[0], item.EspCircle[1], item.EspCircle[2], 1.0f }), item.Shape, item.Thickness);
					}
					if (item.Distance_Esp) {
						char dist[64];
						sprintf_s(dist, "[%.fm]", distance);
						ShadowRGBText(bottom.x - 15, bottom.y, IM_COL32(143, 0, 255, 255), dist);
					}
				}
				if (item.Team_Aimbot) {					
					auto dx = aimbotspot.x - (Width / 2);
					auto dy = aimbotspot.y - (Height / 2);
					auto dz = aimbotspot.z - (Depth / 2);
					auto dist = sqrtf(dx * dx + dy * dy + dz * dz) / 100.0f;
					if (dist < item.AimFOV && dist < closestDistance) {
						closestDistance = dist;
						closestPawn = entity.Actor;
					}
				}
			}			
		}	
		else if (curactorid == 18391356) {
			ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height / 1), ImVec2(bottom.x, bottom.y), ImGui::GetColorU32({ item.TeamLineESP[0], item.TeamLineESP[1], item.TeamLineESP[2], 1.0f }), item.Thickness);
		}
	}

	if (item.Aimbot) {
		if (closestPawn != 0) {
			if (item.Aimbot && closestPawn && GetAsyncKeyState(item.aimkey) < 0) {
				AimAt(closestPawn);					
				if (item.Auto_Bone_Switch) {

					item.boneswitch += 1;
					if (item.boneswitch == 700) {
						item.boneswitch = 0;
					}

					if (item.boneswitch == 0) {
						item.hitboxpos = 0; 
					}
					else if (item.boneswitch == 50) {
						item.hitboxpos = 1; 
					}
					else if (item.boneswitch == 100) {
						item.hitboxpos = 2; 
					}
					else if (item.boneswitch == 150) {
						item.hitboxpos = 3; 
					}
					else if (item.boneswitch == 200) {
						item.hitboxpos = 4; 
					}
					else if (item.boneswitch == 250) {
						item.hitboxpos = 5;
					}
					else if (item.boneswitch == 300) {
						item.hitboxpos = 6; 
					}
					else if (item.boneswitch == 350) {
						item.hitboxpos = 7; 
					}
					else if (item.boneswitch == 400) {
						item.hitboxpos = 6;
					}
					else if (item.boneswitch == 450) {
						item.hitboxpos = 5;
					}
					else if (item.boneswitch == 500) {
						item.hitboxpos = 4;
					}
					else if (item.boneswitch == 550) {
						item.hitboxpos = 3;
					}
					else if (item.boneswitch == 600) {
						item.hitboxpos = 2;
					}
					else if (item.boneswitch == 650) {
						item.hitboxpos = 1;
					}
				}
			}
			else {
				isaimbotting = false;
				AimAt2(closestPawn);
			}
		}
	}
}

void GetKey() {
	if (item.hitboxpos == 0) {
		item.hitbox = 66; //head
	}
	else if (item.hitboxpos == 1) {
		item.hitbox = 65; //neck
	}
	else if (item.hitboxpos == 2) {
		item.hitbox = 64; //CHEST_TOP
	}
	else if (item.hitboxpos == 3) {
		item.hitbox = 36; //CHEST_TOP
	}
	else if (item.hitboxpos == 4) {
		item.hitbox = 7; //chest
	}
	else if (item.hitboxpos == 5) {
		item.hitbox = 6; //CHEST_LOW
	}
	else if (item.hitboxpos == 6) {
		item.hitbox = 5; //TORSO
	}
	else if (item.hitboxpos == 7) {
		item.hitbox = 2; //pelvis
	}

	if (item.aimkeypos == 0) {
		item.aimkey = VK_RBUTTON;//left mouse button
	}
	else if (item.aimkeypos == 1) {
		item.aimkey = VK_LBUTTON;//right mouse button
	}
	else if (item.aimkeypos == 2) {
		item.aimkey = VK_CAPITAL;//right mouse button
	}

	DrawESP();
}

void Active() {
	ImGuiStyle* Style = &ImGui::GetStyle();
	Style->Colors[ImGuiCol_Button] = ImColor(55, 55, 55);
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(55, 55, 55);
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(0, 0, 0);
}
void Hovered() { 
	ImGuiStyle* Style = &ImGui::GetStyle(); 
	Style->Colors[ImGuiCol_Button] = ImColor(0, 0, 0); 
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(0, 0, 0); 
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(55, 55, 55); 
}

void Active1() { 
	ImGuiStyle* Style = &ImGui::GetStyle(); 
	Style->Colors[ImGuiCol_Button] = ImColor(0, 55, 0); 
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(0, 55, 0); 
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(55, 0, 0); 
}
void Hovered1() { 
	ImGuiStyle* Style = &ImGui::GetStyle(); 
	Style->Colors[ImGuiCol_Button] = ImColor(55, 0, 0); 
	Style->Colors[ImGuiCol_ButtonActive] = ImColor(55, 0, 0); 
	Style->Colors[ImGuiCol_ButtonHovered] = ImColor(0, 55, 0); 
}

void ToggleButton(const char* str_id, bool* v)
{
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	float height = 16.0f;
	float width = height * 1.60f;
	float radius = height * 0.50f;

	ImGui::InvisibleButton(str_id, ImVec2(width, height));
	if (ImGui::IsItemClicked())
		*v = !*v;

	float t = *v ? 1.0f : 0.0f;

	ImGuiContext& g = *GImGui;
	float ANIM_SPEED = 0.20f;
	if (g.LastActiveId == g.CurrentWindow->GetID(str_id))
	{
		float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
		t = *v ? (t_anim) : (1.0f - t_anim);
	}

	ImU32 col_bg;

	col_bg = ImGui::GetColorU32(ImLerp(ImVec4(1.f, 0.0f, 0.0f, 1.0f), ImVec4(0.0f, 1.f, 0.0f, 1.0f), t));

	draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
	draw_list->AddCircleFilled(ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius), radius + 0.80f, IM_COL32(128, 128, 128, 255));
}


void render() {
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (item.Show_Menu) {

		static int tab = 1;
		ImGui::SetNextWindowSize({ 536.f,621.f });
		ImGui::Begin(("Jakix FN [F8 To toggle menu]"), 0, ImGuiWindowFlags_::ImGuiWindowFlags_NoResize | ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse);
		ImGui::SetCursorPos({ 181.f,28.f });
		if (ImGui::Button("Aimbot", { 75.f,36.f }))
		{
			tab = 1;
		}
		ImGui::SetCursorPos({ 274.f,28.f });
		if (ImGui::Button("Visuals", { 75.f,36.f }))
		{
			tab = 2;
		}

		if (tab == 1)
		{
			ImGui::BeginChild("child", { 520.f,530.f }, true);
			ImGui::Text("Enable Aimbot"); ImGui::SameLine();ToggleButton("Aimbot", &item.Aimbot);
			if (item.Aim_Speed)
			{
				ImGui::SliderFloat("Aimbot Smoothing", &item.Aim_Speed, 1.f, 25.f);
			}
			ImGui::Combo("Aim Bone", &item.hitboxpos, hitboxes, sizeof(hitboxes) / sizeof(*hitboxes));
			ImGui::Text("Draw FOV cirlce"); ImGui::SameLine();ToggleButton("circle", &item.Draw_FOV_Circle);
			if (item.Draw_FOV_Circle)
			{
				ImGui::SliderFloat("Aim FOV", &item.AimFOV, 15.f, 600.f);
			}
			ImGui::Text("Crosshair"); ImGui::SameLine();ToggleButton("crosshair", &item.Cross_Hair);
			ImGui::Text("Aim At Teammates"); ImGui::SameLine();ToggleButton("teamaim", &item.Team_Aimbot); 

			ImGui::EndChild();
		}

		if (tab == 2)
		{
			ImGui::BeginChild("child", { 520.f,530.f }, true);
			ImGui::Text("Box ESP"); ImGui::SameLine(); ToggleButton("ESP", &item.Esp_box);
			ImGui::Text("Distance ESP"); ImGui::SameLine(); ToggleButton("Distance", &item.Distance_Esp);
			ImGui::Text("Snaplines"); ImGui::SameLine(); ToggleButton("Snaplines", &item.Esp_line);
			ImGui::Text("Teammate ESP"); ImGui::SameLine(); ToggleButton("TeamESP", &item.TeamESP);
			ImGui::EndChild();
		}


		ImGui::End();


	}
	GetKey();

	ImGui::EndFrame();
	D3dDevice->SetRenderState(D3DRS_ZENABLE, false);
	D3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	D3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	D3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	if (D3dDevice->BeginScene() >= 0) {
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		D3dDevice->EndScene();
	}
	HRESULT result = D3dDevice->Present(NULL, NULL, NULL, NULL);

	if (result == D3DERR_DEVICELOST && D3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
		ImGui_ImplDX9_InvalidateDeviceObjects();
		D3dDevice->Reset(&d3dpp);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
}

void xInitD3d()
{
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &p_Object)))
		exit(3);

	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.BackBufferWidth = Width;
	d3dpp.BackBufferHeight = Height;
	d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
	d3dpp.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.hDeviceWindow = Window;
	d3dpp.Windowed = TRUE;

	p_Object->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &D3dDevice);

	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	(void)io;

	ImGui_ImplWin32_Init(Window);
	ImGui_ImplDX9_Init(D3dDevice);

	auto* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.31f, 0.00f, 0.58f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.99f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.56f, 0.00f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.33f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.20f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.29f, 0.29f, 0.29f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_Column] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	colors[ImGuiCol_ColumnHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	colors[ImGuiCol_ColumnActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);


	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(8.00f, 8.00f);
	style.FramePadding = ImVec2(4.00f, 3.00f);
	style.ItemSpacing = ImVec2(8.00f, 4.00f);
	style.ItemInnerSpacing = ImVec2(4.00f, 4.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 21.00f;
	style.ScrollbarSize = 14.00f;
	style.GrabMinSize = 10.00f;
	style.WindowBorderSize = 0.00f;
	style.ChildBorderSize = 1.00f;
	style.PopupBorderSize = 1.00f;
	style.FrameBorderSize = 1.00f;
	style.TabBorderSize = 0.00f;
	style.WindowRounding = 0.00f;
	style.ChildRounding = 0.00f;
	style.FrameRounding = 6.00f;
	style.PopupRounding = 0.00f;
	style.ScrollbarRounding = 9.00f;
	style.GrabRounding = 7.00f;
	style.TabRounding = 4.00f;
	style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
	style.DisplaySafeAreaPadding = ImVec2(3.00f, 3.00f);

	p_Object->Release();
}

LPCSTR RandomStringx(int len) {
	std::string str = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	std::string newstr;
	std::string builddate = __DATE__;
	std::string buildtime = __TIME__;
	int pos;
	while (newstr.size() != len) {
		pos = ((rand() % (str.size() - 1)));
		newstr += str.substr(pos, 1);
	}
	return newstr.c_str();
}

int main(int argc, const char* argv[]) {
	

	printf("Niggers");

	CreateThread(NULL, NULL, Menuthread, NULL, NULL, NULL);
	DriverHandle = CreateFileW(XorStr(L"\\\\.\\a1SSLSubWo32").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	while (DriverHandle == INVALID_HANDLE_VALUE) {
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		exit(0);
	}

	while (hwnd == NULL) {
		DWORD dLastError = GetLastError();
		LPCTSTR strErrorMessage = NULL;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, dLastError, 0, (LPSTR)&strErrorMessage, 0, NULL);
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
		printf(XorStr("\n Start FN").c_str());
		Sleep(450);

		hwnd = FindWindowA(0, XorStr("Fortnite  ").c_str());
		GetWindowThreadProcessId(hwnd, &processID);

		RECT rect;
		if (GetWindowRect(hwnd, &rect)) {
			info_t Input_Output_Data;
			Input_Output_Data.pid = processID;
			unsigned long int Readed_Bytes_Amount;

			DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
			base_address = (unsigned long long int)Input_Output_Data.data;
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);

			xCreateWindow();
			xInitD3d();

			HANDLE handle = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cache), nullptr, NULL, nullptr);
			CloseHandle(handle);

			xMainLoop();
			
		}



	}
	return 0;
}

void SetWindowToTarget()
{
	while (true)
	{
		if (hwnd)
		{
			ZeroMemory(&GameRect, sizeof(GameRect));
			GetWindowRect(hwnd, &GameRect);
			Width = GameRect.right - GameRect.left;
			Height = GameRect.bottom - GameRect.top;
			DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

			if (dwStyle & WS_BORDER)
			{
				GameRect.top += 32;
				Height -= 39;
			}
			ScreenCenterX = Width / 2;
			ScreenCenterY = Height / 2;
			MoveWindow(Window, GameRect.left, GameRect.top, Width, Height, true);
		}
		else
		{
			exit(0);
		}
	}
}

const MARGINS Margin = { -1 };

void xCreateWindow()
{
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)SetWindowToTarget, 0, 0, 0);

	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.lpszClassName = "Gideion";
	wc.lpfnWndProc = WinProc;
	RegisterClassEx(&wc);

	if (hwnd)
	{
		GetClientRect(hwnd, &GameRect);
		POINT xy;
		ClientToScreen(hwnd, &xy);
		GameRect.left = xy.x;
		GameRect.top = xy.y;

		Width = GameRect.right;
		Height = GameRect.bottom;
	}
	else
		exit(2);

	Window = CreateWindowEx(NULL, "Gideion", "Gideion1", WS_POPUP | WS_VISIBLE, 0, 0, Width, Height, 0, 0, 0, 0);

	DwmExtendFrameIntoClientArea(Window, &Margin);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);
}

void WriteAngles(float TargetX, float TargetY, float TargetZ) {
	float x = TargetX / 6.666666666666667f;
	float y = TargetY / 6.666666666666667f;
	float z = TargetZ / 6.666666666666667f;
	y = -(y);

	writefloat(PlayerController + 0x418, y);
	writefloat(PlayerController + 0x418 + 0x4, x);
	writefloat(PlayerController + 0x418, z);
}

MSG Message = { NULL };

void xMainLoop()
{
	static RECT old_rc;
	ZeroMemory(&Message, sizeof(MSG));

	while (Message.message != WM_QUIT)
	{
		if (PeekMessage(&Message, Window, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}

		HWND hwnd_active = GetForegroundWindow();

		if (hwnd_active == hwnd) {
			HWND hwndtest = GetWindow(hwnd_active, GW_HWNDPREV);
			SetWindowPos(Window, hwndtest, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		if (GetAsyncKeyState(0x23) & 1)
			exit(8);

		RECT rc;
		POINT xy;

		ZeroMemory(&rc, sizeof(RECT));
		ZeroMemory(&xy, sizeof(POINT));
		GetClientRect(hwnd, &rc);
		ClientToScreen(hwnd, &xy);
		rc.left = xy.x;
		rc.top = xy.y;

		ImGuiIO& io = ImGui::GetIO();
		io.ImeWindowHandle = hwnd;
		io.DeltaTime = 1.0f / 60.0f;

		POINT p;
		GetCursorPos(&p);
		io.MousePos.x = p.x - xy.x;
		io.MousePos.y = p.y - xy.y;

		if (GetAsyncKeyState(VK_LBUTTON)) {
			io.MouseDown[0] = true;
			io.MouseClicked[0] = true;
			io.MouseClickedPos[0].x = io.MousePos.x;
			io.MouseClickedPos[0].x = io.MousePos.y;
		}
		else
			io.MouseDown[0] = false;

		if (rc.left != old_rc.left || rc.right != old_rc.right || rc.top != old_rc.top || rc.bottom != old_rc.bottom)
		{
			old_rc = rc;

			Width = rc.right;
			Height = rc.bottom;

			d3dpp.BackBufferWidth = Width;
			d3dpp.BackBufferHeight = Height;
			SetWindowPos(Window, (HWND)0, xy.x, xy.y, Width, Height, SWP_NOREDRAW);
			D3dDevice->Reset(&d3dpp);
		}
		render();
	}
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	DestroyWindow(Window);
}

LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam))
		return true;

	switch (Message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		exit(4);
		break;
	case WM_SIZE:
		if (D3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			d3dpp.BackBufferWidth = LOWORD(lParam);
			d3dpp.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = D3dDevice->Reset(&d3dpp);
			if (hr == D3DERR_INVALIDCALL)
				IM_ASSERT(0);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
		break;
	default:
		return DefWindowProc(hWnd, Message, wParam, lParam);
		break;
	}
	return 0;
}
