// SPDX-License-Identifier: MIT
#include "Editor/Inspector/ShadowSettingsPanel.h"
#include "Engine/Runtime/Engine/SceneState.h"
#include "Engine/Rendering/Renderer/Core/Renderer.h"
#include "Engine/Rendering/RHI/OpenGL/OpenGLNativeBridge.h"
#include <imgui.h>
#include <algorithm>
void DrawShadowSettingsControls(SceneState& scene, Renderer& renderer)
{
    if (!ImGui::CollapsingHeader("Shadows")) return;
    auto& settings = scene.shadowSettings;
    ImGui::Checkbox("Multiple shadow lights", &settings.multiLight);
    if (!settings.multiLight) ImGui::TextWrapped("Legacy scope: only the main light casts shadows.");
    int budget = static_cast<int>(settings.maxLights);
    if (ImGui::SliderInt("Shadow light budget", &budget,1,4)) settings.maxLights = budget;
    const auto resolution = [](const char* label, unsigned& value) {
        const unsigned sizes[] = {256,512,1024,2048};
        int selected = 0;
        for (int i=0;i<4;++i) if (value==sizes[i]) selected=i;
        if (ImGui::Combo(label,&selected,"256\0" "512\0" "1024\0" "2048\0")) value=sizes[selected];
    };
    resolution("Directional / Spot resolution",settings.resolution);
    resolution("Point face resolution",settings.pointResolution);
    const auto& frame = renderer.GetShadowFrame();
    ImGui::Text("Shadow lights: %u / %u eligible; views: %u",
        static_cast<unsigned>(frame.lights.size()),frame.eligibleLights,static_cast<unsigned>(frame.views.size()));
    for (const auto& light : scene.lights)
    {
        const auto found = std::find_if(frame.lights.begin(),frame.lights.end(),[&](const ShadowLightFrame& entry){return entry.lightId==light.proxy.id;});
        if (!light.proxy.visible || !scene.world.IsVisible(light.actor)) continue;
        if (light.proxy.castsShadow)
            ImGui::TextWrapped("%s: %s",light.name.c_str(),found!=frame.lights.end()?"shadow active":"no shadow allocation (scope, budget or invalid projection)");
    }
    auto& debug = renderer.ShadowDebug();
    ImGui::Checkbox("Cascade colors (debug)",&debug.cascades);
    ImGui::Checkbox("Shadow map preview",&debug.preview);
    if (debug.preview)
    {
        const int maxLight = std::max(0,static_cast<int>(frame.lights.size())-1);
        ImGui::SliderInt("Preview light slot",&debug.light,0,maxLight);
        debug.light=std::clamp(debug.light,0,maxLight);
        int maxSlice = 0;
        if (debug.light < static_cast<int>(frame.lights.size()))
        {
            const auto& light=frame.lights[debug.light];
            maxSlice=light.type==LightType::Point ? 5 : static_cast<int>(light.cascadeCount)-1;
            if (const auto* source=FindSceneLight(scene.lights,light.lightId)) ImGui::TextUnformatted(source->name.c_str());
        }
        ImGui::SliderInt("Face / cascade",&debug.slice,0,maxSlice);
        debug.slice=std::clamp(debug.slice,0,maxSlice);
        ImGui::Checkbox("Reflection cascades",&debug.reflection);
        const auto texture=renderer.GetShadowPreviewTexture();
        if (texture) ImGui::Image(static_cast<ImTextureID>(GetOpenGLTexture(renderer.Device(),texture)),{256,256},{0,1},{1,0});
        else ImGui::TextDisabled("No completed shadow preview.");
        ImGui::TextWrapped("Depth: white is far/empty. Point faces: +X, -X, +Y, -Y, +Z, -Z. Debug controls are not saved.");
    }
}
