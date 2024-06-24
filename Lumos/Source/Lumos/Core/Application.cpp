#ifndef LUMOS_PLATFORM_MACOS
#include "Precompiled.h"
#endif
#include "Application.h"

#include "Scene/Scene.h"
#include "Engine.h"
#include "Utilities/Timer.h"

#include "Graphics/RHI/Renderer.h"
#include "Graphics/RHI/GraphicsContext.h"
#include "Graphics/Renderers/SceneRenderer.h"
#include "Graphics/Camera/Camera.h"
#include "Graphics/Material.h"
#include "Graphics/Renderers/DebugRenderer.h"
#include "Graphics/Renderers/GridRenderer.h"
#include "Graphics/Font.h"
#include "Maths/Transform.h"

#include "Scene/EntityFactory.h"
#include "Utilities/LoadImage.h"
#include "Core/OS/Input.h"
#include "Core/OS/Window.h"
#include "Core/Profiler.h"
#include "Core/OS/FileSystem.h"
#include "Core/JobSystem.h"
#include "Core/CoreSystem.h"
#include "Utilities/StringUtilities.h"
#include "Core/OS/FileSystem.h"
#include "Core/String.h"
#include "Core/DataStructures/TDArray.h"
#include "Core/CommandLine.h"
#include "Core/Asset/AssetManager.h"
#include "Scripting/Lua/LuaManager.h"
#include "ImGui/ImGuiManager.h"
#include "Events/ApplicationEvent.h"
#include "Audio/AudioManager.h"
#include "Audio/Sound.h"
#include "Physics/B2PhysicsEngine/B2PhysicsEngine.h"
#include "Physics/LumosPhysicsEngine/LumosPhysicsEngine.h"
#include "Embedded/EmbedAsset.h"
#include "Core/Asset/AssetRegistry.h"
#include "Core/DataStructures/Map.h"

#define SERIALISATION_INCLUDE_ONLY
#include "Scene/Serialisation/SerialisationImplementation.h"
#include "Scene/Serialisation/SerialiseApplication.h"

#include "Embedded/splash.inl"

#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

#include "Core/OS/OS.h"

#include <cereal/archives/json.hpp>
#include <imgui/imgui.h>
#include <imgui/Plugins/implot/implot.h>

namespace Lumos
{
    Application* Application::s_Instance = nullptr;

    Application::Application()
        : m_Frames(0)
        , m_Updates(0)
        , m_SceneViewWidth(800)
        , m_SceneViewHeight(600)
    {
        LUMOS_PROFILE_FUNCTION();
        LUMOS_ASSERT(!s_Instance, "Application already exists!");

        s_Instance = this;
    }

    Application::~Application()
    {
        LUMOS_PROFILE_FUNCTION();
        ImGui::DestroyContext();
        ImPlot::DestroyContext();
    }

    void Application::Init()
    {
        LUMOS_PROFILE_FUNCTION();
        m_FrameArena = ArenaAlloc(Megabytes(1));
        m_Arena      = ArenaAlloc(Kilobytes(64));

        m_EventQueue.Reserve(16);

        SetMaxImageDimensions(2048, 2048);

        m_SceneManager = CreateUniquePtr<SceneManager>();
        m_AssetManager = CreateSharedPtr<AssetManager>();

        Deserialise();

        CommandLine* cmdline = Internal::CoreSystem::GetCmdLine();
        if(cmdline->OptionBool(Str8Lit("help")))
        {
            LUMOS_LOG_INFO("Print this help.\n Option 1 : test");
        }

        Engine::Get();
        LuaManager::Get().OnInit();
        LuaManager::Get().OnNewProject(m_ProjectSettings.m_ProjectRoot);

        m_Timer = CreateUniquePtr<Timer>();

        Graphics::GraphicsContext::SetRenderAPI(static_cast<Graphics::RenderAPI>(m_ProjectSettings.RenderAPI));

        WindowDesc windowDesc;
        windowDesc.Width       = m_ProjectSettings.Width;
        windowDesc.Height      = m_ProjectSettings.Height;
        windowDesc.RenderAPI   = m_ProjectSettings.RenderAPI;
        windowDesc.Fullscreen  = m_ProjectSettings.Fullscreen;
        windowDesc.Borderless  = m_ProjectSettings.Borderless;
        windowDesc.ShowConsole = m_ProjectSettings.ShowConsole;
        windowDesc.Title       = m_ProjectSettings.Title;
        windowDesc.VSync       = m_ProjectSettings.VSync;

        if(m_ProjectSettings.DefaultIcon)
        {
            windowDesc.IconPaths = { "//Assets/Textures/icon.png", "//Assets/Textures/icon32.png" };
        }

        // Initialise the Window
        m_Window = UniquePtr<Window>(Window::Create(windowDesc));
        if(!m_Window->HasInitialised())
            OnQuit();

        m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));

        m_EditorState = EditorState::Play;

        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui::StyleColorsDark();

        bool loadEmbeddedShaders = true;
        if(FileSystem::FolderExists(m_ProjectSettings.m_EngineAssetPath + "Shaders"))
            loadEmbeddedShaders = false;

        if(!loadEmbeddedShaders)
        {
            std::string coreDataPath;
            auto shaderPath = std::filesystem::path(m_ProjectSettings.m_EngineAssetPath + "Shaders/CompiledSPV/");
            int shaderCount = 0;
            if(std::filesystem::is_directory(shaderPath))
            {
                for(auto entry : std::filesystem::directory_iterator(shaderPath))
                {
                    auto extension = StringUtilities::GetFilePathExtension(entry.path().string());
                    if(extension == "spv")
                    {
                        EmbedShader(entry.path().string());
                        shaderCount++;
                    }
                }
            }
            LUMOS_LOG_INFO("Embedded {0} shaders.", shaderCount);
        }
        Graphics::Renderer::Init(loadEmbeddedShaders, m_ProjectSettings.m_EngineAssetPath);

        if(m_ProjectSettings.Fullscreen)
            m_Window->Maximise();

        // Draw Splash Screen
        {
            auto desc          = Graphics::TextureDesc(Graphics::TextureFilter::LINEAR, Graphics::TextureFilter::LINEAR, Graphics::TextureWrap::REPEAT);
            desc.flags         = Graphics::TextureFlags::Texture_Sampled;
            auto splashTexture = Graphics::Texture2D::CreateFromSource(splashWidth, splashHeight, (void*)splash, desc);
            Graphics::Renderer::GetRenderer()->Begin();
            Graphics::Renderer::GetRenderer()->DrawSplashScreen(splashTexture);
            Graphics::Renderer::GetRenderer()->Present();
            // To Display the window
            m_Window->ProcessInput();
            m_Window->OnUpdate();

            delete splashTexture;
        }

        uint32_t screenWidth  = m_Window->GetWidth();
        uint32_t screenHeight = m_Window->GetHeight();
        m_SystemManager       = CreateUniquePtr<SystemManager>();

        System::JobSystem::Context context;

        System::JobSystem::Execute(context, [](JobDispatchArgs args)
                                   { Lumos::Input::Get(); });

        System::JobSystem::Execute(context, [this](JobDispatchArgs args)
                                   {
                auto audioManager = AudioManager::Create();
                if (audioManager)
                {
                    m_SystemManager->RegisterSystem<AudioManager>(audioManager);
                } });

        System::JobSystem::Execute(context, [this](JobDispatchArgs args)
                                   {
                        m_SystemManager->RegisterSystem<LumosPhysicsEngine>();
                        m_SystemManager->RegisterSystem<B2PhysicsEngine>();
                        LUMOS_LOG_INFO("Initialised Physics Manager"); });

        System::JobSystem::Execute(context, [this](JobDispatchArgs args)
                                   { m_SceneManager->LoadCurrentList(); });

        m_ImGuiManager = CreateUniquePtr<ImGuiManager>(false);
        m_ImGuiManager->OnInit();
        LUMOS_LOG_INFO("Initialised ImGui Manager");

        m_SceneRenderer = CreateUniquePtr<Graphics::SceneRenderer>(screenWidth, screenHeight);

        System::JobSystem::Wait(context);

        m_CurrentState = AppState::Running;

        Graphics::Material::InitDefaultTexture();
        Graphics::Font::InitDefaultFont();
        m_SceneRenderer->EnableDebugRenderer(true);

        // updateThread = std::thread(Application::UpdateSystems);
    }

    void Application::OnQuit()
    {
        LUMOS_PROFILE_FUNCTION();
        Serialise();

        ArenaRelease(m_FrameArena);
        ArenaRelease(m_Arena);

        Graphics::Material::ReleaseDefaultTexture();
        Graphics::Font::ShutdownDefaultFont();
        Engine::Release();
        Input::Release();

        m_AssetManager.reset();
        m_SceneManager.reset();
        m_SceneRenderer.reset();
        m_SystemManager.reset();
        m_ImGuiManager.reset();
        LuaManager::Release();

        Graphics::Pipeline::ClearCache();
        Graphics::RenderPass::ClearCache();
        Graphics::Framebuffer::ClearCache();

        m_Window.reset();

        Graphics::Renderer::Release();
    }

    void Application::OpenProject(const std::string& filePath)
    {
        LUMOS_PROFILE_FUNCTION();
        m_ProjectSettings.m_ProjectName = StringUtilities::GetFileName(filePath);
        m_ProjectSettings.m_ProjectName = StringUtilities::RemoveFilePathExtension(m_ProjectSettings.m_ProjectName);

#ifndef LUMOS_PLATFORM_IOS
        auto projectRoot                = StringUtilities::GetFileLocation(filePath);
        m_ProjectSettings.m_ProjectRoot = projectRoot;

        String8 pathCopy                = PushStr8Copy(m_FrameArena, projectRoot.c_str());
        pathCopy                        = StringUtilities::ResolveRelativePath(m_FrameArena, pathCopy);
        m_ProjectSettings.m_ProjectRoot = (const char*)pathCopy.str;
#endif

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Prefabs"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Prefabs");

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Materials"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Materials");

        m_SceneManager = CreateUniquePtr<SceneManager>();

        Deserialise();

        m_SceneManager->LoadCurrentList();
        m_SceneManager->ApplySceneSwitch();

        LuaManager::Get().OnNewProject(m_ProjectSettings.m_ProjectRoot);
    }

    void Application::OpenNewProject(const std::string& path, const std::string& name)
    {
        LUMOS_PROFILE_FUNCTION();
        m_ProjectSettings.m_ProjectRoot = path + name + "/";
        m_ProjectSettings.m_ProjectName = name;

        String8 pathCopy                = PushStr8Copy(m_FrameArena, m_ProjectSettings.m_ProjectRoot.c_str());
        pathCopy                        = StringUtilities::ResolveRelativePath(m_FrameArena, pathCopy);
        m_ProjectSettings.m_ProjectRoot = (const char*)pathCopy.str;

        std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot);

        m_SceneManager = CreateUniquePtr<SceneManager>();

        MountFileSystemPaths();
        // Set Default values
        m_ProjectSettings.RenderAPI   = 1;
        m_ProjectSettings.Width       = 1200;
        m_ProjectSettings.Height      = 800;
        m_ProjectSettings.Borderless  = false;
        m_ProjectSettings.VSync       = true;
        m_ProjectSettings.Title       = "App";
        m_ProjectSettings.ShowConsole = false;
        m_ProjectSettings.Fullscreen  = false;

#ifdef LUMOS_PLATFORM_MACOS
        // This is assuming Application in bin/Release-macos-x86_64/LumosEditor.app
        LUMOS_LOG_INFO(StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()));
        m_ProjectSettings.m_EngineAssetPath = StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()) + "../../../../../Lumos/Assets/";
#else
        m_ProjectSettings.m_EngineAssetPath = StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()) + "../../Lumos/Assets/";
#endif

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets");

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Scripts"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Scripts");

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Scenes"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Scenes");

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Textures"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Textures");

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Meshes"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Meshes");

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Sounds"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Sounds");

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Prefabs"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Prefabs");

        if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Materials"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Materials");

        MountFileSystemPaths();

        m_SceneManager->EnqueueScene(new Scene("Empty Scene"));
        m_SceneManager->SwitchScene(0);

        // Set Default values
        m_ProjectSettings.Title      = "App";
        m_ProjectSettings.Fullscreen = false;

        m_SceneManager->ApplySceneSwitch();

        m_ProjectLoaded = true;

        Serialise();

        LuaManager::Get().OnNewProject(m_ProjectSettings.m_ProjectRoot);
    }

    void Application::MountFileSystemPaths()
    {
        FileSystem::Get().SetAssetRoot(PushStr8Copy(m_Arena, (m_ProjectSettings.m_ProjectRoot + std::string("Assets")).c_str()));
    }

    Scene* Application::GetCurrentScene() const
    {
        LUMOS_PROFILE_FUNCTION_LOW();
        return m_SceneManager->GetCurrentScene();
    }

    glm::vec2 Application::GetWindowSize() const
    {
        if(!m_Window)
            return glm::vec2(0.0f, 0.0f);
        return glm::vec2(static_cast<float>(m_Window->GetWidth()), static_cast<float>(m_Window->GetHeight()));
    }

    float Application::GetWindowDPI() const
    {
        if(!m_Window)
            return 1.0f;

        return m_Window->GetDPIScale();
    }

    bool Application::OnFrame()
    {
        LUMOS_PROFILE_FUNCTION();
        LUMOS_PROFILE_FRAMEMARKER();

        ArenaClear(m_FrameArena);

        if(m_SceneManager->GetSwitchingScene())
        {
            LUMOS_PROFILE_SCOPE("Application::SceneSwitch");
            Graphics::Renderer::GetGraphicsContext()->WaitIdle();
            m_SceneManager->ApplySceneSwitch();
            return m_CurrentState != AppState::Closing;
        }

        double now  = m_Timer->GetElapsedSD();
        auto& stats = Engine::Get().Statistics();
        auto& ts    = Engine::GetTimeStep();

        static int s_NumContiguousLargeFrames = 0;
        const int maxContiguousLargeFrames    = 2;

        if(ts.GetSeconds() > 5)
        {
            LUMOS_LOG_WARN("Large frame time {0}", ts.GetSeconds());

            s_NumContiguousLargeFrames++;
#ifdef LUMOS_DISABLE_LARGE_FRAME_TIME
            // Added to stop application locking computer
            // Exit if frametime exceeds 5 seconds
            return false;
#endif

            if(s_NumContiguousLargeFrames > maxContiguousLargeFrames)
                return false;
        }
        else
            s_NumContiguousLargeFrames = 0;

        ExecuteMainThreadQueue();

        {
            LUMOS_PROFILE_SCOPE("Application::TimeStepUpdates");
            ts.OnUpdate();

            ImGuiIO& io  = ImGui::GetIO();
            io.DeltaTime = (float)ts.GetSeconds();

            stats.FrameTime = ts.GetMillis();
        }

        // Process Input events before ImGui::NewFrame
        Input::Get().ResetPressed();
        m_Window->ProcessInput();
        ImGui::NewFrame();

        {
            std::scoped_lock<std::mutex> lock(m_EventQueueMutex);

            for(auto& event : m_EventQueue)
            {
                event();
            }
            m_EventQueue.Clear();

            // Process custom event queue
            while(!m_EventQueue.Empty())
            {
                auto& func = m_EventQueue.Back();
                func();
                m_EventQueue.PopBack();
            }
        }

        System::JobSystem::Context context;

        {
            LUMOS_PROFILE_SCOPE("Application::Update");
            OnUpdate(ts);

            System::JobSystem::Execute(context, [](JobDispatchArgs args)
                                       { Application::UpdateSystems(); });

            // UpdateSystems();

            // m_SystemManager->GetSystem<LumosPhysicsEngine>()->SyncTransforms(m_SceneManager->GetCurrentScene());
            // m_SystemManager->GetSystem<B2PhysicsEngine>()->SyncTransforms(m_SceneManager->GetCurrentScene());

            m_Updates++;
        }

        // Exit frame early if escape or close button clicked
        // Prevents a crash with vulkan/moltenvk
        if(m_CurrentState == AppState::Closing)
        {
            System::JobSystem::Wait(context);
            return false;
        }

        if(!m_Minimized)
        {
            LUMOS_PROFILE_SCOPE("Application::Render");
            Engine::Get().ResetStats();

            Graphics::Renderer::GetRenderer()->Begin();
            OnRender();
            m_ImGuiManager->OnNewFrame();
            m_ImGuiManager->OnRender(m_SceneManager->GetCurrentScene());

            // Clears debug line and point lists
            DebugRenderer::Reset((float)ts.GetSeconds());
            OnDebugDraw();

            Graphics::Pipeline::DeleteUnusedCache();
            Graphics::Framebuffer::DeleteUnusedCache();
            Graphics::RenderPass::DeleteUnusedCache();

            m_AssetManager->Update((float)ts.GetElapsedSeconds());
            m_Frames++;
        }
        else
        {
            ImGui::Render();
        }

        {
            LUMOS_PROFILE_SCOPE("Application::UpdateGraphicsStats");
            stats.UsedGPUMemory  = Graphics::Renderer::GetGraphicsContext()->GetGPUMemoryUsed();
            stats.TotalGPUMemory = Graphics::Renderer::GetGraphicsContext()->GetTotalGPUMemory();
        }

        {
            LUMOS_PROFILE_SCOPE("Application::WindowUpdate");
            m_Window->UpdateCursorImGui();
            m_Window->OnUpdate();
        }

        if(now - m_SecondTimer > 1.0f)
        {
            LUMOS_PROFILE_SCOPE("Application::FrameRateCalc");
            m_SecondTimer += 1.0f;

            stats.FramesPerSecond  = m_Frames;
            stats.UpdatesPerSecond = m_Updates;

            m_Frames  = 0;
            m_Updates = 0;
        }

        if(!m_Minimized)
            Graphics::Renderer::GetRenderer()->Present();

        // Sync transforms from physics for the next frame
        {
            System::JobSystem::Wait(context);

            m_SystemManager->GetSystem<LumosPhysicsEngine>()->SyncTransforms(m_SceneManager->GetCurrentScene());
            m_SystemManager->GetSystem<B2PhysicsEngine>()->SyncTransforms(m_SceneManager->GetCurrentScene());
        }

        return m_CurrentState != AppState::Closing;
    }

    void Application::OnRender()
    {
        LUMOS_PROFILE_FUNCTION();
        if(!m_SceneManager->GetCurrentScene())
            return;

        if(!m_DisableMainSceneRenderer)
        {
            m_SceneRenderer->BeginScene(m_SceneManager->GetCurrentScene());
            m_SceneRenderer->OnRender();
        }
    }

    void Application::OnDebugDraw()
    {
        m_SystemManager->OnDebugDraw();
    }

    void Application::OnUpdate(const TimeStep& dt)
    {
        LUMOS_PROFILE_FUNCTION();
        if(!m_SceneManager->GetCurrentScene())
            return;

        if(Application::Get().GetEditorState() != EditorState::Paused
           && Application::Get().GetEditorState() != EditorState::Preview)
        {
            LuaManager::Get().OnUpdate(m_SceneManager->GetCurrentScene());
            m_SceneManager->GetCurrentScene()->OnUpdate(dt);
        }
        m_ImGuiManager->OnUpdate(dt, m_SceneManager->GetCurrentScene());
    }

    void Application::OnEvent(Event& e)
    {
        LUMOS_PROFILE_FUNCTION();
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));

        if(m_ImGuiManager)
            m_ImGuiManager->OnEvent(e);
        if(e.Handled())
            return;

        if(m_SceneRenderer)
            m_SceneRenderer->OnEvent(e);

        if(e.Handled())
            return;

        if(m_SceneManager->GetCurrentScene())
            m_SceneManager->GetCurrentScene()->OnEvent(e);

        Input::Get().OnEvent(e);
    }

    void Application::Run()
    {
        while(OnFrame())
        {
        }

        OnQuit();
    }

    void Application::OnNewScene(Scene* scene)
    {
        LUMOS_PROFILE_FUNCTION();
        m_SceneRenderer->OnNewScene(scene);
    }

    SharedPtr<AssetManager>& Application::GetAssetManager()
    {
        return m_AssetManager;
    }

    void Application::SubmitToMainThread(const std::function<void()>& function)
    {
        LUMOS_PROFILE_FUNCTION();
        std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);

        m_MainThreadQueue.emplace_back(function);
    }

    void Application::ExecuteMainThreadQueue()
    {
        LUMOS_PROFILE_FUNCTION();
        std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);

        for(const auto& func : m_MainThreadQueue)
            func();

        m_MainThreadQueue.clear();
    }

    void Application::OnExitScene()
    {
    }

    void Application::AddDefaultScene()
    {
        if(m_SceneManager->GetScenes().Size() == 0)
        {
            m_SceneManager->EnqueueScene(new Scene("Empty Scene"));
            m_SceneManager->SwitchScene(0);
        }
    }

    bool Application::OnWindowClose(WindowCloseEvent& e)
    {
        m_CurrentState = AppState::Closing;
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& e)
    {
        LUMOS_PROFILE_FUNCTION();
        Graphics::Renderer::GetGraphicsContext()->WaitIdle();

        int width = e.GetWidth(), height = e.GetHeight();

        if(width == 0 || height == 0)
        {
            m_Minimized = true;
            return false;
        }
        m_Minimized = false;

        Graphics::Renderer::GetRenderer()->OnResize(width, height);

        if(m_SceneRenderer)
            m_SceneRenderer->OnResize(width, height);

        Graphics::Renderer::GetGraphicsContext()->WaitIdle();

        return false;
    }

    void Application::OnImGui()
    {
        LUMOS_PROFILE_FUNCTION();
        if(!m_SceneManager->GetCurrentScene())
            return;

        m_SceneManager->GetCurrentScene()->OnImGui();
    }

    void Application::UpdateSystems()
    {
        LUMOS_PROFILE_FUNCTION();
        if(Application::Get().GetEditorState() != EditorState::Paused
           && Application::Get().GetEditorState() != EditorState::Preview)
        {
            auto scene = Application::Get().GetSceneManager()->GetCurrentScene();

            if(!scene)
                return;

            Application::Get().GetSystemManager()->OnUpdate(Engine::GetTimeStep(), scene);
        }
    }

    void Application::OnSceneViewSizeUpdated(uint32_t width, uint32_t height)
    {
        LUMOS_PROFILE_FUNCTION();
        Graphics::Renderer::GetGraphicsContext()->WaitIdle();

        WindowResizeEvent e(width, height);
        if(width == 0 || height == 0)
        {
            m_Minimized = true;
            return;
        }
        m_Minimized = false;
        m_SceneRenderer->OnResize(width, height);
        m_SceneRenderer->OnEvent(e);

        Graphics::Renderer::GetGraphicsContext()->WaitIdle();
    }

    void Application::Serialise()
    {
        LUMOS_PROFILE_FUNCTION();
        {
            std::stringstream storage;
            {
                // output finishes flushing its contents when it goes out of scope
                cereal::JSONOutputArchive output { storage };
                output(*this);
            }
            auto fullPath = m_ProjectSettings.m_ProjectRoot + m_ProjectSettings.m_ProjectName + std::string(".lmproj");
            LUMOS_LOG_INFO("Serialising Application {0}", fullPath);
            FileSystem::WriteTextFile(fullPath, storage.str());
        }

        // Save Asset Registry
        {
            String8 path = PushStr8F(m_FrameArena, "%sAssetRegistry.lmar", m_ProjectSettings.m_ProjectRoot.c_str()); // m_ProjectSettings.m_ProjectRoot + std::string("AssetRegistry.lmar");
            SerialiseAssetRegistry(path, *m_AssetManager->GetAssetRegistry());
        }
    }

    void Application::Deserialise()
    {
        LUMOS_PROFILE_FUNCTION();
        {
            auto filePath = m_ProjectSettings.m_ProjectRoot + m_ProjectSettings.m_ProjectName + std::string(".lmproj");

            MountFileSystemPaths();

            LUMOS_LOG_INFO("Loading Project : {0}", filePath);

            if(!FileSystem::FileExists(filePath))
            {
                LUMOS_LOG_INFO("No saved Project file found {0}", filePath);
                {
                    m_SceneManager = CreateUniquePtr<SceneManager>();

                    // Set Default values
                    m_ProjectSettings.RenderAPI   = 1;
                    m_ProjectSettings.Width       = 1200;
                    m_ProjectSettings.Height      = 800;
                    m_ProjectSettings.Borderless  = false;
                    m_ProjectSettings.VSync       = true;
                    m_ProjectSettings.Title       = "App";
                    m_ProjectSettings.ShowConsole = false;
                    m_ProjectSettings.Fullscreen  = false;

                    m_ProjectLoaded = false;

#ifdef LUMOS_PLATFORM_MACOS
                    // This is assuming Application in bin/Release-macos-x86_64/LumosEditor.app
                    LUMOS_LOG_INFO(StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()));
                    m_ProjectSettings.m_EngineAssetPath = StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()) + "../../../../../Lumos/Assets/";

                    if(!FileSystem::FolderExists(m_ProjectSettings.m_EngineAssetPath))
                    {
                        m_ProjectSettings.m_EngineAssetPath = StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()) + "../../Lumos/Assets/";
                    }
#else
                    m_ProjectSettings.m_EngineAssetPath = StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()) + "../../Lumos/Assets/";
#endif
                    m_SceneManager->EnqueueScene(new Scene("Empty Scene"));
                    m_SceneManager->SwitchScene(0);
                }
                return;
            }

            if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets");

            if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Scripts"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Scripts");

            if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Scenes"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Scenes");

            if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Textures"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Textures");

            if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Meshes"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Meshes");

            if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Sounds"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Sounds");

            if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Prefabs"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Prefabs");

            if(!FileSystem::FolderExists(m_ProjectSettings.m_ProjectRoot + "Assets/Materials"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "Assets/Materials");

            m_ProjectLoaded = true;

            std::string data = FileSystem::ReadTextFile(filePath);
            std::istringstream istr;
            istr.str(data);
            try
            {
                cereal::JSONInputArchive input(istr);
                input(*this);

                // Load Asset Registry
                {
                    String8 path = PushStr8F(m_FrameArena, "%sAssetRegistry.lmar", m_ProjectSettings.m_ProjectRoot.c_str());
                    if(FileSystem::FileExists((const char*)path.str))
                    {
                        DeserialiseAssetRegistry(path, *m_AssetManager->GetAssetRegistry());
                    }
                }
            }
            catch(...)
            {
                // Set Default values
                m_ProjectSettings.RenderAPI   = 1;
                m_ProjectSettings.Width       = 1200;
                m_ProjectSettings.Height      = 800;
                m_ProjectSettings.Borderless  = false;
                m_ProjectSettings.VSync       = true;
                m_ProjectSettings.Title       = "App";
                m_ProjectSettings.ShowConsole = false;
                m_ProjectSettings.Fullscreen  = false;

#ifdef LUMOS_PLATFORM_MACOS
                m_ProjectSettings.m_EngineAssetPath = StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()) + "../../../../../Lumos/Assets/";
#else
                m_ProjectSettings.m_EngineAssetPath = StringUtilities::GetFileLocation(OS::Instance()->GetExecutablePath()) + "../../Lumos/Assets/";
#endif

                m_SceneManager->EnqueueScene(new Scene("Empty Scene"));
                m_SceneManager->SwitchScene(0);

                LUMOS_LOG_ERROR("Failed to load project - {0}", filePath);
            }
        }
    }
}
