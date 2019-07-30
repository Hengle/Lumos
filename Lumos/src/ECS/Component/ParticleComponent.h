#pragma once
#include "LM.h"
#include "LumosComponent.h"
#include "Maths/Vector3.h"

namespace Lumos
{
    class ParticleEmitter;

    class LUMOS_EXPORT ParticleComponent : public LumosComponent
    {
    public:
        ParticleComponent();
        explicit ParticleComponent(std::shared_ptr<ParticleEmitter>& emitter);

        void Init() override;
        void OnUpdateComponent(float dt) override;

		void OnIMGUI() override;
		nlohmann::json Serialise() override { return nullptr; };
		void Deserialise(nlohmann::json& data) override {};
    private:
        std::shared_ptr<ParticleEmitter> m_ParticleEmitter;
        Maths::Vector3 m_PositionOffset;
    };
}