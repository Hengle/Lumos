#pragma once
#include "EditorPanel.h"
#include "Editor.h"
#include <Lumos/Maths/Frustum.h>
#include <Lumos/Maths/Transform.h>
#include <Lumos/Graphics/Camera/Camera.h>
#include <Lumos/Utilities/StringUtilities.h>
#include <Lumos/ImGui/ImGuiUtilities.h>
#include <Lumos/Graphics/Renderers/SceneRenderer.h>
#include <imgui/imgui.h>

namespace Lumos
{
    class GameViewPanel : public EditorPanel
    {
        friend class Editor;

    public:
        GameViewPanel();
        ~GameViewPanel() = default;

        void OnImGui() override;
        void OnNewScene(Scene* scene) override;
        void OnRender() override;
        void DrawGizmos(float width, float height, float xpos, float ypos, Scene* scene);

        void Resize(uint32_t width, uint32_t height);
        void ToolBar();

    private:
        SharedPtr<Graphics::Texture2D> m_GameViewTexture = nullptr;
        Scene* m_CurrentScene                            = nullptr;
        uint32_t m_Width, m_Height;
        UniquePtr<Graphics::SceneRenderer> m_SceneRenderer;
        bool m_GameViewVisible = false;
        bool m_ShowStats       = false;
    };
}
