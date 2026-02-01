#include <windows.h>
#include <format>
#include <filesystem>
#include <toml++/toml.hpp>

#include "nya_commonhooklib.h"
#include "nfsmw.h"

auto SHGetFolderPathA = (void(__stdcall*)(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath))(*(uintptr_t*)0x8902EC);

bool bCustomSavePath = false;

struct ModConfig {
	bool bResolutionSet = false;
	bool bSubtitlesEnabled = false;

	static std::string GetSaveFolder() {
		if (bCustomSavePath) {
			return std::format("{}/NFS Most Wanted/", std::filesystem::absolute("SAVE").string());
		}

		CHAR pszPath[256];
		SHGetFolderPathA(0, 0x8005, 0, 0, pszPath);
		return std::format("{}/NFS Most Wanted/", pszPath);
	}

	void Save() {
		std::ofstream file(GetSaveFolder() + "cwoee.sav", std::iostream::out | std::iostream::binary);
		if (!file.is_open()) return;

		file.write((char*)&bResolutionSet, sizeof(bResolutionSet));
		file.write((char*)&bSubtitlesEnabled, sizeof(bSubtitlesEnabled));
	}

	void Load() {
		std::ifstream file(GetSaveFolder() + "cwoee.sav", std::iostream::in | std::iostream::binary);
		if (!file.is_open()) return;

		file.read((char*)&bResolutionSet, sizeof(bResolutionSet));
		file.read((char*)&bSubtitlesEnabled, sizeof(bSubtitlesEnabled));
	}
} Config;

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
	auto cars = &FEDatabase->CurrentUserProfiles[0]->PlayersCarStable;
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

std::vector<D3DDISPLAYMODE> aDisplayModes;
bool DoesDisplayResolutionExist(int x, int y) {
	for (auto& mode : aDisplayModes) {
		if (mode.Width == x && mode.Height == y) return true;
	}
	return false;
}

void CollectDisplayResolutions() {
	static bool bOnce = false;
	if (bOnce) return;
	bOnce = true;

	auto count = GameD3D->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
	for (int i = 0; i < count; i++) {
		D3DDISPLAYMODE mode;
		GameD3D->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &mode);
		if (DoesDisplayResolutionExist(mode.Width, mode.Height)) continue; // skip duplicates, the game doesn't care about refresh rate differences
		aDisplayModes.push_back(mode);
	}

	// use desktop resolution as a fallback
	if (aDisplayModes.empty()) {
		RECT rect = {};
		GetClientRect(GetDesktopWindow(), &rect);
		D3DDISPLAYMODE mode;
		mode.Width = rect.right - rect.left;
		mode.Height = rect.bottom - rect.top;
		aDisplayModes.push_back(mode);
	}

	if (FirstTime || !Config.bResolutionSet) {
		g_RacingResolution = aDisplayModes.size() - 1;
		Config.bResolutionSet = true;
		Config.Save();
	}
}

void __stdcall GetRacingResolution_New(int* x, int* y) {
	CollectDisplayResolutions();

	auto id = g_RacingResolution;
	if (id < 0 || id >= aDisplayModes.size()) id = 0;
	*x = aDisplayModes[id].Width;
	*y = aDisplayModes[id].Height;
}

class DOMotionBlurEnable : public FEToggleWidget {
public:
	DOMotionBlurEnable(bool state) : FEToggleWidget(state) {}

	void Act(const char* a2, uint32_t a3) override {
		if (a3 == 0x9120409E || a3 == 0xB5971BF1) {
			(*UIOptionsScreen::OptionsToEdit)->g_MotionBlurEnable = !(*UIOptionsScreen::OptionsToEdit)->g_MotionBlurEnable;
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
			title->LabelHash = (*UIOptionsScreen::OptionsToEdit)->g_MotionBlurEnable ? 0x16FDEF36 : 0xF6BBD534;
			title->Flags |= 0x400000;
			title->Flags = title->Flags & 0xFFBFFFFD | 0x400000;
		}
	}
};

int GetSubtitlesOn() {
	return Config.bSubtitlesEnabled;
}

class GOSubtitles : public FEToggleWidget {
public:
	GOSubtitles(bool state) : FEToggleWidget(state) {}

	void Act(const char* a2, uint32_t a3) override {
		if (a3 == 0x9120409E || a3 == 0xB5971BF1) {
			Config.bSubtitlesEnabled = !Config.bSubtitlesEnabled;
			Config.Save();
		}

		bMovedLastUpdate = true;
		BlinkArrows(a3);
		Draw();
	}
	void Draw() override {
		if (auto title = pTitle) {
			title->LabelHash = Attrib::StringHash32("SUBTITLES");
			title->Flags |= 0x400000;
			title->Flags = title->Flags & 0xFFBFFFFD | 0x400000;
		}
		if (auto title = pData) {
			title->LabelHash = Config.bSubtitlesEnabled ? 0x16FDEF36 : 0xF6BBD534;
			title->Flags |= 0x400000;
			title->Flags = title->Flags & 0xFFBFFFFD | 0x400000;
		}
	}
};

void __thiscall VOScreenResolution_Act(FEToggleWidget* pThis, const char* a2, uint32_t a3) {
	int scroll = 0;
	if (a3 == 0x9120409E) scroll = -1;
	if (a3 == 0xB5971BF1) scroll = 1;

	auto& dest = (*UIOptionsScreen::OptionsToEdit)->g_RacingResolution;
	dest += scroll;
	if (dest < 0) dest = aDisplayModes.size()-1;
	if (dest >= aDisplayModes.size()) dest = 0;

	pThis->bMovedLastUpdate = true;
	pThis->BlinkArrows(a3);
	pThis->Draw();
}

void __thiscall VOScreenResolution_Draw(FEToggleWidget* pThis) {
	if (auto title = pThis->pTitle) {
		title->LabelHash = 0xC96D57C9;
		title->Flags |= 0x400000;
		title->Flags = title->Flags & 0xFFBFFFFD | 0x400000;
	}
	if (auto title = pThis->pData) {
		auto modeId = (*UIOptionsScreen::OptionsToEdit)->g_RacingResolution;
		if (modeId < 0 || modeId >= aDisplayModes.size()) modeId = 0;
		FEPrintf(title, "%dx%d", aDisplayModes[modeId].Width, aDisplayModes[modeId].Height);
	}
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			if (NyaHookLib::GetEntryPoint() != 0x3C4040) {
				MessageBoxA(nullptr, "Unsupported game version! Make sure you're using v1.3 (.exe size of 6029312 bytes)", "nya?!~", MB_ICONERROR);
				return TRUE;
			}

			bool bSeamlessUG2 = true;
			bool bOverrideResolution = true;
			static float fSchedulerTimestep = 60.0;
			if (std::filesystem::exists("NFSMWCwoeeQOL_gcp.toml")) {
				auto config = toml::parse_file("NFSMWCwoeeQOL_gcp.toml");
				bSeamlessUG2 = config["seamless_ug2"].value_or(bSeamlessUG2);
				bCustomSavePath = config["move_savegames"].value_or(bCustomSavePath);
				bOverrideResolution = config["fix_resolution"].value_or(bOverrideResolution);
				fSchedulerTimestep = config["sim_rate"].value_or(fSchedulerTimestep);
				if (fSchedulerTimestep < 1.0) fSchedulerTimestep = 1.0;
			}

			NyaHooks::OptionsMenuHook::AddStringRecord(Attrib::StringHash32("SUBTITLES"), "Subtitles");
			NyaHooks::OptionsMenuHook::AddMenuOption(OC_PC_ADV_DISPLAY, [](UIWidgetMenu* pThis){ UIWidgetMenu::AddToggleOption(pThis, new DOMotionBlurEnable(true), true); });
			NyaHooks::OptionsMenuHook::AddMenuOption(OC_AUDIO, [](UIWidgetMenu* pThis){ UIWidgetMenu::AddToggleOption(pThis, new GOSubtitles(true), true); });

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
				NyaHooks::LateInitHook::Init();
				NyaHooks::LateInitHook::aPreFunctions.push_back([]() {
					Config.Load();

					NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x6C27D0, &GetRacingResolution_New);
					NyaHookLib::Patch(0x89BB08, &VOScreenResolution_Act);
					NyaHookLib::Patch(0x89BB10, &VOScreenResolution_Draw);
					//NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x52978B, 0x5297C8); // remove resolution from the basic menu
					//NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x5297DC, 0x529818); // remove resolution from the advanced menu

					// prevent racingresolution from being reset
					NyaHookLib::Patch<uint8_t>(0x6C1954, 0xC3);
					NyaHookLib::Patch<uint8_t>(0x6E6B9E, 0xEB);
				});
			}

			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x63C093, &TotalVehicleFixed);
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x63839A, &BlowEngineFixed);
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x4F0E70, &GetClosestPlayerCarFixed);
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x516137, &IsPALFixed);

			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x542DAE, &GetSubtitlesOn);
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x54EE6C, &GetSubtitlesOn);
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x54F20D, &GetSubtitlesOn);

			if (bCustomSavePath) {
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