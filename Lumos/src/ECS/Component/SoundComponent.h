#pragma once
#include "LM.h"
#include "LumosComponent.h"

namespace Lumos
{
	class SoundNode;

	class LUMOS_EXPORT SoundComponent : public LumosComponent
	{
	public:
        SoundComponent();
		explicit SoundComponent(std::shared_ptr<SoundNode>& sound);

		void OnUpdateComponent(float dt) override;
		void Init() override;

		void OnIMGUI() override;
        
        SoundNode* GetSoundNode() const { return m_SoundNode.get(); }

		nlohmann::json Serialise() override { return nullptr; };
		void Deserialise(nlohmann::json& data) override {};
        
    private:
        std::shared_ptr<SoundNode> m_SoundNode;
	};
}