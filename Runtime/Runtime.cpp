#include <Lumos/Core/Application.h>
#include <Lumos/Core/EntryPoint.h>
#include <Lumos/Core/OS/Window.h>
#include <Lumos/Core/OS/Input.h>
#include <Lumos/Core/Engine.h>
#include <Lumos/Utilities/StringUtilities.h>

#include <imgui/imgui.h>

using namespace Lumos;

class Runtime : public Application
{
    friend class Application;

public:
    explicit Runtime()
        : Application()
    {
    }

    ~Runtime()
    {
    }

    void OnUpdate(const TimeStep& dt) override
    {
        using namespace Lumos;
        Application::OnUpdate(dt);
    }

    void OnEvent(Event& e) override
    {
        Application::OnEvent(e);

        if(Input::Get().GetKeyPressed(Lumos::InputCode::Key::Escape))
            Application::SetAppState(AppState::Closing);
    }

    void Init() override
    {
        std::vector<std::string> projectLocations = {
            OS::Instance()->GetExecutablePath() + "../../../../../ExampleProject/",
            "/Users/jmorton/dev/Lumos/ExampleProject/Example.lmproj",
            "ExampleProject/Example.lmproj",
            "../ExampleProject/Example.lmproj",
            OS::Instance()->GetExecutablePath() + "/ExampleProject/Example.lmproj",
            OS::Instance()->GetExecutablePath() + "/../ExampleProject/Example.lmproj",
            OS::Instance()->GetExecutablePath() + "/../../ExampleProject/Example.lmproj"
        };

        bool fileFound = false;
        std::string filePath;
        for(auto& path : projectLocations)
        {
            if(FileSystem::FileExists(path))
            {
                LUMOS_LOG_INFO("Loaded Project {0}", path);
                m_ProjectSettings.m_ProjectRoot = StringUtilities::GetFileLocation(path);
                m_ProjectSettings.m_ProjectName = "Example";
                break;
            }
        }

        Application::Init();
        Application::SetEditorState(EditorState::Play);
        Application::Get().GetWindow()->SetWindowTitle("Runtime");
        Application::Get().GetWindow()->SetEventCallback(BIND_EVENT_FN(Runtime::OnEvent));
    }

    void OnImGui() override
    {
        ImGui::Begin("Metrics");
        ImGui::Text("FPS : %.2f", (float)Lumos::Engine::Get().Statistics().FramesPerSecond);
        ImGui::End();
        Application::OnImGui();
    }
};

Lumos::Application* Lumos::CreateApplication()
{
    return new ::Runtime();
}
