#include <windows.h>
#include <format>
#include <filesystem>
#include <toml++/toml.hpp>

#include "nya_commonhooklib.h"
#include "nfsmw.h"

// fixes a crash when totaling your car after quitting from a race
// in this case GRacerInfo is valid, mRaceParms is not
void __thiscall TotalVehicleFixed(GRacerInfo* pThis) {
	if (!GRaceStatus::fObj->mRaceParms) return;
	GRacerInfo::TotalVehicle(pThis);
}
void __thiscall BlowEngineFixed(GRacerInfo* pThis) {
	if (!GRaceStatus::fObj->mRaceParms) return;
	GRacerInfo::BlowEngine(pThis);
}

// fixes a crash when hitting objects using a player-driven AI car
// the result of this function is not null checked at 4F0E70
EAX_CarState* GetClosestPlayerCarFixed(bVector3* a1) {
	if (auto car = GetClosestPlayerCar(a1)) return car;
	static EAX_CarState temp;
	memset(&temp,0,sizeof(temp));
	return &temp;
}

// always unlock all cars in the career garage
auto IsCarUnlockedOrig = (bool(__cdecl*)(void* a1, uint32_t a2, int a3))nullptr;
bool IsCarUnlockedFixed(void* a1, uint32_t a2, int a3) {
	auto cars = &FEDatabase->mUserProfile->PlayersCarStable;
	if (auto rec = FEPlayerCarDB::GetCarRecordByHandle(cars, a2)) {
		if (FEPlayerCarDB::GetCareerRecordByHandle(cars, rec->CareerHandle) != nullptr) return true;
	}
	return IsCarUnlockedOrig(a1, a2, a3);
}

// return ntsc or pal based on what videos are in the movies folder
bool IsPALFixed() {
	const char* movies[] = {
		"attract_movie_english",
		"blacklist_01_english",
		"blacklist_02_english",
		"blacklist_03_english",
		"blacklist_04_english",
		"blacklist_05_english",
		"blacklist_06_english",
		"blacklist_07_english",
		"blacklist_08_english",
		"blacklist_09_english",
		"blacklist_10_english",
		"blacklist_11_english",
		"blacklist_12_english",
		"blacklist_13_english",
		"blacklist_14_english",
		"blacklist_15_english",
		"bounty_tutorial_english",
		"drag_tutorial_english",
		"eahd_bumper_english",
		"ealogo_english",
		"psa_english",
		"pursuit_tutorial_english",
		"speedtrap_tutorial_english",
		"storyfmv_bla134_english",
		"storyfmv_bus12_english",
		"storyfmv_cro06_coh06a_english",
		"storyfmv_her136_english",
		"storyfmv_pin11_english",
		"storyfmv_rac01_english",
		"storyfmv_rap30_english",
		"storyfmv_raz08_english",
		"storyfmv_saf25_english",
		"tollbooth_tutorial_english",
	};

	for (auto& filename : movies) {
		if (std::filesystem::exists(std::format("MOVIES/{}_ntsc.vp6", filename))) return false;
		if (std::filesystem::exists(std::format("MOVIES/{}_pal.vp6", filename))) return true;
	}
	return BuildRegion::IsPal();
}

HRESULT __stdcall GetSaveFolderNew(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath) {
	std::filesystem::create_directory("SAVE");
	strcpy_s(pszPath, 256, std::filesystem::absolute("SAVE").string().c_str());
	return 0;
}

class DOMotionBlurEnable : public FEToggleWidget {
public:
	void* operator new(size_t size) {
		return GAME_malloc(size);
	}

	DOMotionBlurEnable(bool state) : FEToggleWidget(state) {}

	void Act(const char* a2, uint32_t a3) override {
		if (a3 == 0x9120409E || a3 == 0xB5971BF1) {
			g_MotionBlurEnable = !g_MotionBlurEnable;
		}

		bMovedLastUpdate = true;
		BlinkArrows(a3);
		Draw();
	}
	void Draw() override {
		if (auto title = pTitle) {
			title->LabelHash = 0x1C7D9D8D;
			title->Flags |= 0x400000;
			title->Flags = title->Flags & 0xFFBFFFFD | 0x400000;
		}
		if (auto title = pData) {
			title->LabelHash = g_MotionBlurEnable ? 0x16FDEF36 : 0xF6BBD534;
			title->Flags |= 0x400000;
			title->Flags = title->Flags & 0xFFBFFFFD | 0x400000;
		}
	}
};

void __thiscall MotionBlurHooked(UIWidgetMenu* pThis, FEToggleWidget* widget, bool a3) {
	UIWidgetMenu::AddToggleOption(pThis, widget, a3);
	UIWidgetMenu::AddToggleOption(pThis, new DOMotionBlurEnable(true), a3);
}

int nResX = 0;
int nResY = 0;
void __stdcall GetRacingResolution_New(int* x, int* y) {
	if (nResX <= 0 || nResY <= 0) {
		RECT rect = {};
		GetClientRect(GetDesktopWindow(), &rect);
		nResX = rect.right - rect.left;
		nResY = rect.bottom - rect.top;
	}

	*x = nResX;
	*y = nResY;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			if (NyaHookLib::GetEntryPoint() != 0x3C4040) {
				MessageBoxA(nullptr, "Unsupported game version! Make sure you're using v1.3 (.exe size of 6029312 bytes)", "nya?!~", MB_ICONERROR);
				return TRUE;
			}

			bool bSeamlessUG2 = true;
			bool bGameFolderSaves = false;
			bool bOverrideResolution = false;
			static float fSchedulerTimestep = 60.0;
			if (std::filesystem::exists("NFSMWCwoeeQOL_gcp.toml")) {
				auto config = toml::parse_file("NFSMWCwoeeQOL_gcp.toml");
				bSeamlessUG2 = config["seamless_ug2"].value_or(bSeamlessUG2);
				bGameFolderSaves = config["move_savegames"].value_or(bGameFolderSaves);
				bOverrideResolution = config["override_resolution"].value_or(bOverrideResolution);
				fSchedulerTimestep = config["sim_rate"].value_or(fSchedulerTimestep);
				if (fSchedulerTimestep < 1.0) fSchedulerTimestep = 1.0;
			}

			// NIS motion blur override - this is required for 360 stuff to be compatible with the motion blur toggle
			// currently one frame behind, todo hook in a few more places to set the bool quickly enough for that to not happen
			static bool bNewNISMotionBlur = true;
			NyaHooks::WorldServiceHook::Init();
			NyaHooks::WorldServiceHook::aFunctions.push_back([](){ *(bool*)0x8F9B28 = bNewNISMotionBlur && g_MotionBlurEnable; });

			NyaHookLib::Patch(0x46E302, &bNewNISMotionBlur);
			NyaHookLib::Patch(0x473D31, &bNewNISMotionBlur);
			NyaHookLib::Patch(0x4830DC, &bNewNISMotionBlur);
			NyaHookLib::Patch(0x622EFD, &bNewNISMotionBlur);
			NyaHookLib::Patch(0x62E353, &bNewNISMotionBlur);
			NyaHookLib::Patch(0x62E3AD, &bNewNISMotionBlur);
			NyaHookLib::Patch(0x6DBB40, &bNewNISMotionBlur);
			NyaHookLib::Patch(0x6F75DA, &bNewNISMotionBlur);

			// using LateInitHookAlternate here to make sure LateInitHook can override this later
			NyaHooks::LateInitHookAlternate::Init();
			NyaHooks::LateInitHookAlternate::aFunctions.push_back([]() { Scheduler::fgScheduler->fTimeStep = 1.0 / fSchedulerTimestep; });

			if (bOverrideResolution) {
				NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x6C27D0, &GetRacingResolution_New);
				NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x52978B, 0x5297C8); // remove resolution from the basic menu
				NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x5297DC, 0x529818); // remove resolution from the advanced menu
			}

			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x63C093, &TotalVehicleFixed);
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x63839A, &BlowEngineFixed);
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x4F0E70, &GetClosestPlayerCarFixed);
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x516137, &IsPALFixed);

			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x529BC7, &MotionBlurHooked);

			if (bGameFolderSaves) {
				static auto func = &GetSaveFolderNew;
				NyaHookLib::Patch(0x6CBF17 + 2, &func);
			}

			if (bSeamlessUG2) {
				// always give 10k but don't pop up the message anymore
				NyaHookLib::Patch<uint8_t>(0x51857C, 0xEB);
				NyaHookLib::Patch<uint16_t>(0x56D7CA, 0x9090);
			}

			IsCarUnlockedOrig = (bool(__cdecl*)(void* a1, uint32_t a2, int a3))NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x7BF8A2, &IsCarUnlockedFixed);
		} break;
		default:
			break;
	}
	return TRUE;
}