#pragma once
#include "Engine/Source/Graphics/Particle/ParticleEmitter.h"

#include <Editor/Source/EditorInterface.h>
#include "Engine/Source/Math/KittyMath.h"
#include "Engine/Source/Graphics/PostProcessing.h"
#include "Engine/Source/Graphics/Renderers/BasicRenderer.h"
#include "Engine/Source/Graphics/FX/VFXResources.h"

namespace KE
{
	
	class VFXManager
	{
		KE_EDITOR_FRIEND
	private:
		Graphics* myGraphics;
		SpriteManager* mySpriteManager;
		std::vector<VFXSequence> myVFXSequences;
		PostProcessing myVFXPostProcessing;
		BasicRenderer myVFXRenderer;
		CBuffer myVFXCBuffer;

		//render data
		std::vector<VFXSequencePlayerData> myRenderQueue;
		std::vector<VFXSequenceRenderPackage> myRenderPackages;
		std::vector<SpriteBatch*> mySpriteBatches;
		//
	public:


		void Init(Graphics* aGraphics);
		void RenderVFX(const VFXBufferData& aFxBuffer, const ModelData* aModelData);
		void Render(eRenderLayers aLayer, ID3D11DepthStencilView* aDSV);
		void Update(float aDeltaTime);
		void Resize(int aWidth, int aHeight);

		void EndFrame();

		void PrepareRenderData(VFXSequencePlayerData& aPlayerData);

		void InitializeVFXMesh(VFXMeshInstance& aMeshToInitialize) const;
		void InitializeParticleEmitter(ParticleEmitter& anEmitterToInitialize) const;
		
		inline PostProcessing& GetPostProcessing() { return myVFXPostProcessing; }

		void TriggerVFXSequence(int aVFXSequenceIndex, const VFXRenderInput& aRenderInput);
		void StopVFXSequence(int aVFXSequenceIndex, const VFXRenderInput& aRenderInput);

		static void SaveVFXSequence(VFXSequence* aSequence);
		void LoadVFXSequence(int aVFXSequenceIndex, const std::string& aFilePath);

		int CreateVFXSequence(const std::string& aName);

		int GetVFXSequenceFromName(const std::string& aName);
		VFXSequence* GetVFXSequence(int aVFXSequenceIndex);
		void ClearVFX();

		void RenderVFXDirect(int aVFXIndex, int aCurrentFrame, Transform* aTransform, eRenderLayers aLayer);
	};

	struct VFXPlayerInterface
	{
		VFXManager* manager;

		std::vector<int> myVFXSequenceIndices;

		void TriggerVFXSequence(int aVFXSequenceIndex, const VFXRenderInput& aRenderInput) const
		{
			manager->TriggerVFXSequence(myVFXSequenceIndices[aVFXSequenceIndex], aRenderInput);
		}

		void StopVFXSequence(int aVFXSequenceIndex, const VFXRenderInput& aRenderInput) const
		{
			manager->StopVFXSequence(myVFXSequenceIndices[aVFXSequenceIndex], aRenderInput);
		}

		void AddVFX(const std::string& aVFXName)
		{
			myVFXSequenceIndices.push_back(manager->GetVFXSequenceFromName(aVFXName));
		}
	};




}
