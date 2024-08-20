#include "ApplicationInfoPanel.h"

#include <Lumos/Core/Application.h>
#include <Lumos/Scene/SceneManager.h>
#include <Lumos/Scene/Scene.h>
#include <Lumos/Core/Asset/AssetManager.h>

#include "Editor.h"

#include <Lumos/Core/Engine.h>
#include <Lumos/Core/OS/Window.h>
#include <Lumos/Graphics/Renderers/SceneRenderer.h>
#include <Lumos/ImGui/ImGuiUtilities.h>
#include <Lumos/Utilities/StringUtilities.h>
#include <imgui/imgui.h>
#include <imgui/Plugins/implot/implot.h>

namespace Lumos
{
    struct ScrollingBuffer
    {
        int MaxSize;
        int Offset;
        ImVector<ImVec2> Data;
        ScrollingBuffer(int max_size = 2000)
        {
            MaxSize = max_size;
            Offset  = 0;
            Data.reserve(MaxSize);
        }
        void AddPoint(float x, float y)
        {
            if(Data.size() < MaxSize)
                Data.push_back(ImVec2(x, y));
            else
            {
                Data[Offset] = ImVec2(x, y);
                Offset       = (Offset + 1) % MaxSize;
            }
        }
        void Erase()
        {
            if(Data.size() > 0)
            {
                Data.shrink(0);
                Offset = 0;
            }
        }
    };

    ApplicationInfoPanel::ApplicationInfoPanel()
    {
        m_Name       = "Application Info###appinfo";
        m_SimpleName = "Application Info";
    }

    static float MaxFrameTime = 0;
    void ApplicationInfoPanel::OnImGui()
    {
        auto flags = ImGuiWindowFlags_NoCollapse;
        if(ImGui::Begin(m_Name.c_str(), &m_Active, flags))
        {
            ImGuiUtilities::PushID();

            // m_FPSData.push_back(Lumos::Engine::Get().Statistics().FramesPerSecond);
            // MaxFrameTime = Maths::Max(MaxFrameTime, m_FPSData.back());

            static ScrollingBuffer rdata(40000), rdata1(40000);
            static float t = 0;
            t += ImGui::GetIO().DeltaTime;
            static int frame = 0;
            frame++;

            // if (frame > (int)(ImGui::GetIO().Framerate / 60))
            {
                rdata.AddPoint(t, ImGui::GetIO().Framerate);
                rdata1.AddPoint(t, (float)Lumos::Engine::GetTimeStep().GetMillis()); // 1000.0f / ImGui::GetIO().Framerate);
            }

            static ImPlotAxisFlags rt_axis = ImPlotAxisFlags_NoTickLabels;
            static bool PlotFrameTime      = false;
            static bool PlotFramerate      = false;

            ImGui::Checkbox("Plot Frame Time", &PlotFrameTime);
            ImGui::Checkbox("Plot Frame Rate", &PlotFramerate);

            if(PlotFramerate && ImPlot::BeginPlot("Framerate", ImVec2(-1, 350), 0))
            {
                ImPlot::SetupAxis(ImAxis_X1, nullptr, rt_axis);
                ImPlot::SetupAxis(ImAxis_Y1, "FPS", 0);
                ImPlot::SetupAxisLimits(ImAxis_X1, t - 10.0f, t, ImGuiCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 120);

                ImPlot::PlotLine("##Framerate", &rdata.Data[0].x, &rdata.Data[0].y, rdata.Data.size(), 0, rdata.Offset, 2 * sizeof(float));

                ImPlot::EndPlot();
            }

            if(PlotFrameTime && ImPlot::BeginPlot("Frametime", ImVec2(-1, 350), 0))
            {
                ImPlot::SetupAxis(ImAxis_X1, nullptr, rt_axis);
                ImPlot::SetupAxis(ImAxis_Y1, "Frame (ms)", 0);
                ImPlot::SetupAxisLimits(ImAxis_X1, t - 10.0f, t, ImGuiCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 60);

                ImPlot::PlotLine("##Framerate", &rdata1.Data[0].x, &rdata1.Data[0].y, rdata1.Data.size(), 0, rdata1.Offset, 2 * sizeof(float));

                ImPlot::EndPlot();
            }

            if(ImGui::TreeNodeEx("Application", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto systems = Application::Get().GetSystemManager();

                if(ImGui::TreeNode("Quality Settings"))
                {
                    QualitySettings& qs = Application::Get().GetQualitySettings();
                    int shadowQuality   = (int)qs.ShadowQuality;
                    int shadowRes       = (int)qs.ShadowResolution;
                    float resScale      = qs.RendererScale;

                    ImGui::Columns(2);

                    if(ImGuiUtilities::Property("Render Scale", resScale, 0.01f, 3.0f, 0.1f, ImGuiUtilities::PropertyFlag::DragValue))
                        qs.RendererScale = resScale;

                    if(ImGuiUtilities::Property("Shadow Quality", shadowQuality, 0, 3))
                        qs.ShadowQuality = (ShadowQualitySetting)shadowQuality;

                    if(ImGuiUtilities::Property("Shadow Resolution", shadowRes, 0, 3))
                        qs.ShadowResolution = (ShadowResolutionSetting)shadowRes;

                    ImGui::Columns(1);

                    ImGui::TreePop();
                }

                if(ImGui::TreeNode("Systems"))
                {
                    systems->OnImGui();
                    ImGui::TreePop();
                }

                auto SceneRenderer = Application::Get().GetSceneRenderer();
                if(ImGui::TreeNode("SceneRenderer"))
                {
                    SceneRenderer->OnImGui();
                    ImGui::TreePop();
                }

                ImGui::NewLine();
                ImGui::Columns(2);
                bool VSync = Application::Get().GetWindow()->GetVSync();
                if(ImGuiUtilities::Property("VSync", VSync))
                {
                    auto editor = m_Editor;
                    Application::Get().QueueEvent([VSync, editor]
                                                  {
                                                      Application::Get().GetWindow()->SetVSync(VSync);
                                                      Application::Get().GetWindow()->GetSwapChain()->SetVSync(VSync);
                                                      Graphics::Renderer::GetRenderer()->OnResize(Application::Get().GetWindow()->GetWidth(), Application::Get().GetWindow()->GetHeight()); });
                }

                ImGui::Columns(1);
                ImGui::Text("FPS : %5.2i", Engine::Get().Statistics().FramesPerSecond);
                ImGui::Text("Frame Time : %5.2f ms", Engine::Get().Statistics().FrameTime);
                ImGui::Text("Arena Count : %i", GetArenaCount());
                ImGui::Text("Num Draw Calls  %u", Engine::Get().Statistics().NumDrawCalls);
                ImGui::Text("Total Triangles  %u", Engine::Get().Statistics().TriangleCount);
                ImGui::Text("Num Rendered Objects %u", Engine::Get().Statistics().NumRenderedObjects);
                ImGui::Text("Num Shadow Objects %u", Engine::Get().Statistics().NumShadowObjects);
                ImGui::Text("Bound Pipelines %u", Engine::Get().Statistics().BoundPipelines);
                ImGui::Text("Bound SceneRenderer %u", Engine::Get().Statistics().BoundSceneRenderer);
                if(ImGui::TreeNodeEx("Arenas", 0))
                {
                    uint64_t totalAllocated = 0;
                    for(int i = 0; i < GetArenaCount(); i++)
                    {
                        auto arena = GetArena(i);
                        totalAllocated += arena->Size;
                        float percentageFull = (float)arena->Position / (float)arena->Size;
                        ImGui::ProgressBar(percentageFull);
                        Lumos::ImGuiUtilities::Tooltip((Lumos::StringUtilities::BytesToString(arena->Position) + " / " + Lumos::StringUtilities::BytesToString(arena->Size)).c_str());
                    }
                    ImGui::Text("Total %s", Lumos::StringUtilities::BytesToString(totalAllocated).c_str());
                    ImGui::TreePop();
                }

                ImGui::Text("Scene : %s", Application::Get().GetSceneManager()->GetCurrentScene()->GetSceneName().c_str());
                ImGui::TreePop();
            }

            ImGuiUtilities::PopID();
        }
        ImGui::End();
    }
}
