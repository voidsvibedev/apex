#include "memory/data.hpp"

std::mutex Mutex;

void Initialize() {
    PVMMDLL_MAP_MODULEENTRY base;

    Overlay* overlay = new Overlay(L"Consolas", 18.0);
    Helper* helper = new Helper();

    Cache* cache = new Cache();
    APEX apex;

    if (!overlay->init()) std::println("failed to init");
    if (!overlay->startup_d2d()) std::println("failed to startup_d2d");

	auto CacheGameData = std::make_shared<DelayFunction>(1000, [&]{
	
		auto LocalPlayer = mem.Read<std::uint64_t>(base->vaBase + 0x225a8a8);
        auto ViewRender = mem.Read<std::uint64_t>(base->vaBase + 0x74dd028);
        auto ViewMatrix = mem.Read<std::uint64_t>(ViewRender + 0x11a350);

        std::lock_guard<std::mutex> lock(Mutex);
        apex.LocalPlayer = LocalPlayer;
        apex.ViewRender = ViewRender;
        apex.ViewMatrixPTR = ViewMatrix;

	});

	auto CacheEntities = std::make_shared<DelayFunction>(10000, [&]{

        std::vector<std::uint64_t> Temp;

        auto Entities = helper->ScatterReadArray<std::uint64_t>(base->vaBase + 0x1eabd08, 500);
        auto ClassIdentifier = helper->ScatterOffset<std::uint64_t>(Entities, 0x468);
        
        auto sh = mem.ScatterCreate();
        
        for (int i = 0; i < Entities.size() - 1; ++i)
        {
            mem.ScatterPrepareRead(sh, (std::uint64_t*)ClassIdentifier[i]);
        }
        
        mem.ScatterExecute(sh);
        
        for (int i = 0; i < Entities.size() - 1; ++i)
        {
            char buffer[128] = { 0 };
        
            mem.ScatterRead(sh, (std::uint64_t*)ClassIdentifier[i], &buffer, 64);

            // std::println("plr: {0}", buffer);
        
            if (strcmp(std::string(buffer).c_str(), "player") == 0)
            {
                // std::println("plr: {0}", buffer);
                Temp.push_back(Entities[i]);
            }
        }

        std::lock_guard<std::mutex> lock(Mutex);
        apex.Entities = Temp;

	});

    auto CameraData = std::make_shared<DelayFunction>(11, [&]{
        
        auto PlayerLocations = helper->ScatterOffset<Vector3, std::uint64_t>(apex.Entities, 0x17c);

        for (auto en : PlayerLocations)
        {
            // std::println("pos: {0} {1} {2}", en.x, en.y, en.z);
        }

        std::lock_guard<std::mutex> lock(Mutex);
        apex.PlayerLocations = PlayerLocations;
        apex.ViewMatrix = mem.Read<Matrix>(apex.ViewMatrixPTR);

	});

	auto Render = std::make_shared<DelayFunction>(8, [&]{

        overlay->begin_scene();
        overlay->clear_scene();

        std::lock_guard<std::mutex> lock(Mutex);
        for (int i = 0; i < apex.PlayerLocations.size(); ++i)
        {
            auto Head = world_to_screen(Vector3(apex.PlayerLocations[i].x, apex.PlayerLocations[i].y, apex.PlayerLocations[i].z + 60), apex.ViewMatrix);
            auto Feet = world_to_screen(Vector3(apex.PlayerLocations[i].x, apex.PlayerLocations[i].y, apex.PlayerLocations[i].z), apex.ViewMatrix);

            // std::println("Head + playerlocations.size, apex entities size: {0} {1} {2} {3} {4}", Head.x, Head.y, Head.z, apex.PlayerLocations.size(), apex.Entities.size());

            float height = Feet.y - Head.y;
            float width = height / 2.0f;

            float healthBarLeft = Head.x + width / 2 + 5;
            float healthBarTop = Head.y;

            float healthBarRight = healthBarLeft + 1;
            float healthBarBottom = Feet.y;

            overlay->draw_rectangle(Head.x - width / 2, Head.y, Head.x + width / 2, Feet.y, D2D1::ColorF(D2D1::ColorF::White));
            overlay->draw_filled_rectangle(healthBarLeft, healthBarTop, healthBarRight, healthBarBottom, D2D1::ColorF(D2D1::ColorF::LightGreen));

        }

        overlay->end_scene();

    });


	auto ConnectTest = std::make_shared<DelayFunction>(100, [&]{
		mem.Connect();
	});

    auto UpdateBase = std::make_shared<DelayFunction>(10000, [&]{
		base = mem.GetModule(Process);
	});

    while (true)
    {
        if (!mem.Connect())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            std::println("{0}", xorstr_("[VERBOSE] Couldn't connect"));
        }
		base = mem.GetModule(Process);


		CacheGameData->Execute();

		CacheEntities->Execute();
        CameraData->Execute();

		Render->Execute();

        if(GetAsyncKeyState(VK_F6))
        {
            delete overlay;
            delete base;
            delete helper;

            break;
        }

    }

    std::println("{0}", xorstr_("[INFO] Thread exiting"));
    VMMDLL_Close(VmmHandle);

    delete overlay;
    ExitThread(0);
}

DWORD WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        std::println("{0}", xorstr_("[INFO] library loaded"));
        std::thread(Initialize).detach();
    }

    return TRUE;
}