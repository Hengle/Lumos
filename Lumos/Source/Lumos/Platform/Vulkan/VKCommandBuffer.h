#pragma once
#include "VK.h"
#include "Graphics/RHI/CommandBuffer.h"
#include "VKFence.h"
#include "Core/Reference.h"

namespace Lumos
{
    namespace Graphics
    {
        enum class CommandBufferState : uint8_t
        {
            Idle,
            Recording,
            Ended,
            Submitted
        };

        class RenderPass;
        class Pipeline;
        class VKSemaphore;

        class VKCommandBuffer : public CommandBuffer
        {
        public:
            VKCommandBuffer();
            VKCommandBuffer(VkCommandBuffer commandBuffer);
            ~VKCommandBuffer();

            bool Init(bool primary) override;
            bool Init(bool primary, VkCommandPool commandPool);
            void Unload() override;
            void BeginRecording() override;
            void BeginRecordingSecondary(RenderPass* renderPass, Framebuffer* framebuffer) override;
            void EndRecording() override;
            void Reset();
            bool Flush() override;
            bool Wait();

            void Submit() override;

            void BindPipeline(Pipeline* pipeline) override;
            void BindPipeline(Pipeline* pipeline, uint32_t layer) override;
            void UnBindPipeline() override;
            void EndCurrentRenderPass() override;

            void BeginRenderPass(RenderPass* renderpass, float* clearColour, Framebuffer* framebuffer, uint32_t width, uint32_t height);

            bool Execute(VkPipelineStageFlags flags, VkSemaphore signalSemaphore, bool waitFence);

            void ExecuteSecondary(CommandBuffer* primaryCmdBuffer) override;
            void UpdateViewport(uint32_t width, uint32_t height, bool flipViewport) override;

            VkCommandBuffer GetHandle() const { return m_CommandBuffer; };
            CommandBufferState GetState() const { return m_State; }

            VkSemaphore GetSemaphore() const;

            static void MakeDefault();

        protected:
            static CommandBuffer* CreateFuncVulkan();

        private:
            VkCommandBuffer m_CommandBuffer;
            VkCommandPool m_CommandPool;
            bool m_Primary;
            CommandBufferState m_State;
            SharedPtr<VKFence> m_Fence;
            SharedPtr<VKSemaphore> m_Semaphore;

            uint32_t m_BoundPipelineLayer = 0;
            Pipeline* m_BoundPipeline     = nullptr;

            RenderPass* m_BoundRenderPass    = nullptr;
            uint32_t m_BoundRenderPassWidth  = 0;
            uint32_t m_BoundRenderPassHeight = 0;
            Framebuffer* m_BoundFrameBuffer  = nullptr;
        };
    }
}
