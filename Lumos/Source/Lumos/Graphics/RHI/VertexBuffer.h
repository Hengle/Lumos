#pragma once
#include "Core/Core.h"
#include "RHIDefinitions.h"

namespace Lumos
{
    namespace Graphics
    {
        class LUMOS_EXPORT VertexBuffer
        {
        public:
            virtual ~VertexBuffer()                                                                  = default;
            virtual void Resize(uint32_t size)                                                       = 0;
            virtual void SetData(uint32_t size, const void* data, bool addBarrier = false)           = 0;
            virtual void SetDataSub(uint32_t size, const void* data, uint32_t offset)                = 0;
            virtual void ReleasePointer()                                                            = 0;
            virtual void Bind(CommandBuffer* commandBuffer, Pipeline* pipeline, uint8_t binding = 0) = 0;
            virtual void Unbind()                                                                    = 0;
            virtual uint32_t GetSize() { return 0; }

            template <typename T>
            T* GetPointer()
            {
                return static_cast<T*>(GetPointerInternal());
            }

        protected:
            static VertexBuffer* (*CreateFunc)(const BufferUsage&);
            static VertexBuffer* (*CreateWithDataFunc)(uint32_t, const void*, const BufferUsage&);
            virtual void* GetPointerInternal() = 0;

        public:
            static VertexBuffer* Create(const BufferUsage& usage = BufferUsage::STATIC);
            static VertexBuffer* Create(uint32_t size, const void* data, const BufferUsage& usage = BufferUsage::STATIC);
        };
    }
}
