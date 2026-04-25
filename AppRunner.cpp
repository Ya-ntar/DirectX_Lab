#include "AppRunner.h"

#include "SceneConfig.h"
#include "AssetRepository.h"
#include "MeshLoader.h"
#include <iostream>
#include <vector>

#include "ControlSettings.h"
#include "GameController.h"
#include "MaterialResolver.h"
#include "RenderLoop.h"
#include "SceneBootstrap.h"
#include "framework/Framework.h"
#include "framework/InputDevice.h"
#include "framework/Timer.h"
#include "framework/Window.h"

using namespace gfw;

bool RunApplication(Window &window, InputDevice &input_device) {
    Framework framework;
    if (!framework.Initialize(&window)) {
        std::wcerr << L"Failed to initialize Framework!" << std::endl;
        return false;
    }

    // Print all materials in Sponza to console
    // PrintModelMaterials(L"sponza/Sponza-master/Sponza.obj", L"sponza/Sponza-master/Sponza.mtl");

    SceneConfig config = BuildAppConfig();
    framework.SetCamera(BuildInitialCamera(config));

    AssetRepository asset_repository(framework);
    MaterialResolver material_resolver(framework);
    std::vector<RenderObject> objects = BootstrapSceneObjects(config, asset_repository, material_resolver);

    if (objects.empty()) {
        std::wcerr << L"No valid scene objects configured." << std::endl;
        return false;
    }

    Timer timer;
    timer.Reset();
    GameControllerSettings game_settings = ToGameControllerSettings(config.controls);
    GameController game(game_settings);

    RunRenderLoop(window, input_device, framework, game, objects, timer);

    framework.Shutdown();
    return true;
}


