#include "stdafx.h"
#include "Engine/Source/Graphics/ModelData.h"
#include "Engine/Source/Graphics/Graphics.h"
#include "VFXManager.h"

#include <External/Include/nlohmann/json.hpp>

#include "Utility/Logging.h"

namespace KE
{

	void VFXManager::Init(Graphics* aGraphics)
	{
		myVFXRenderer.Init(aGraphics);
		myGraphics = aGraphics;
		mySpriteManager = &aGraphics->GetSpriteManager();
		
		myVFXPostProcessing.Init(
			myGraphics->GetDevice().Get(),
			aGraphics,
			aGraphics->GetShaderLoader().GetVertexShader(SHADER_LOAD_PATH "UpDownSample_VS.cso"),
			aGraphics->GetShaderLoader().GetPixelShader(SHADER_LOAD_PATH "DownSample_PS.cso"),
			aGraphics->GetShaderLoader().GetPixelShader(SHADER_LOAD_PATH "UpSample_PS.cso"),
			aGraphics->GetShaderLoader().GetPixelShader(SHADER_LOAD_PATH "Gaussian_PS.cso")
		);

		{
			D3D11_BUFFER_DESC cbd = {};
			cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cbd.Usage = D3D11_USAGE_DYNAMIC;
			cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			cbd.MiscFlags = 0u;
			cbd.ByteWidth = sizeof(VFXBufferData);
			cbd.StructureByteStride = 0u;

			myVFXCBuffer.Init(myGraphics->GetDevice(), &cbd);
		}

		myVFXPostProcessing.SetPreProcessPS(aGraphics->GetShaderLoader().GetPixelShader(SHADER_LOAD_PATH "BloomPreProcess_PS.cso"));
		myVFXPostProcessing.SetPSShader(aGraphics->GetShaderLoader().GetPixelShader(SHADER_LOAD_PATH "PostProcessing_VFX_PS.cso"));
		myVFXPostProcessing.SetVSShader(aGraphics->GetShaderLoader().GetVertexShader(SHADER_LOAD_PATH "PostProcessing_VFX_VS.cso"));

		myVFXPostProcessing.ConfigureDownSampleRTs(aGraphics, aGraphics->GetRenderWidth(), aGraphics->GetRenderHeight());

		PostProcessAttributes& postProcessAttributes = myVFXPostProcessing.GetAttributes();
		postProcessAttributes.bloomBlending = 0.2f;
		postProcessAttributes.bloomTreshold = 0.25f;

		KE_GLOBAL::blackboard.Register(this);
	}

	void VFXManager::RenderVFX(const VFXBufferData& aFxBuffer, const ModelData* aModelData)
	{
		auto* graphicsContext = myGraphics->GetContext().Get();
		myVFXCBuffer.MapBuffer(&aFxBuffer, sizeof(aFxBuffer), graphicsContext);
		myVFXCBuffer.BindForPS(6, graphicsContext);
		myVFXCBuffer.BindForVS(6, graphicsContext);

		myGraphics->SetRasterizerState(KE::eRasterizerStates::FrontfaceCulling);
		myVFXRenderer.RenderModel(
			{
				nullptr,
				myGraphics->GetView(),
				myGraphics->GetProjection(),
				0,
				0
			},
			*aModelData
		);

		myGraphics->SetRasterizerState(KE::eRasterizerStates::BackfaceCulling);
		myVFXRenderer.RenderModel(
			{
				nullptr,
				myGraphics->GetView(),
				myGraphics->GetProjection(),
				0,
				0
			},
			*aModelData
		);
	}

	void VFXManager::Render(eRenderLayers aLayer, ID3D11DepthStencilView* aDSV)
	{
		myGraphics->SetDepthStencilState(KE::eDepthStencilStates::ReadOnlyLess);
		myGraphics->SetBlendState(KE::eBlendStates::VFXBlend);

		for (auto& playedVFX: myRenderQueue)
		{
			for (auto& emitter : playedVFX.myEmitters)
			{
				auto* batch = emitter.myEmitter.GetSpriteBatch();
				mySpriteManager->BindBuffers(*batch, myGraphics->GetCameraManager().GetHighlightedCamera());
				mySpriteManager->RenderBatch(*batch);
			}
		}

		for (VFXSequenceRenderPackage& package : myRenderPackages)
		{
			if (package.layer != aLayer) { continue; }

			VFXBufferData fxb{};

			fxb.colour = package.attributes.colour;
			fxb.uvOffset = package.attributes.uvOffset;
			fxb.uvScale = package.attributes.uvScale;

			fxb.bloomAttributes = {
				package.bloom ? 1.0f : 0.0f,
				0.0f,
				0.0f,
				0.0f
			};

			auto* modelData = package.modelData;
			modelData->myTransform = &package.instanceTransform.GetMatrix();

			if (package.customBuffer.constantBuffer != nullptr)
			{
				const auto& buffer = package.customBuffer;
				buffer.constantBuffer->MapBuffer(buffer.bufferData, buffer.bufferSize, myGraphics->GetContext().Get());
				buffer.constantBuffer->BindForPS(buffer.bufferSlot, myGraphics->GetContext().Get());
			}

			RenderVFX(fxb, modelData);
		}

		myGraphics->SetDepthStencilState(KE::eDepthStencilStates::Write);
		myGraphics->SetBlendState(KE::eBlendStates::Disabled);
	}
	
	void VFXManager::Update(float aDeltaTime)
	{
		for (int i = 0; i < myRenderQueue.size(); i++)
		{
			VFXSequencePlayerData& playerData = myRenderQueue[i];
			playerData.myTimer += aDeltaTime * VFX_SEQUENCE_FRAME_RATE;
			if (playerData.myTimer > myVFXSequences[playerData.mySequenceIndex].myDuration)
			{
				if (playerData.myIsLooping)
				{
					playerData.myTimer = 0.0f;
				}
				else
				{
					myRenderQueue.erase(myRenderQueue.begin() + i);
					i--;
				}
			}
			playerData.myFrame = (int)(playerData.myTimer);
			for (auto& emitter : playerData.myEmitters)
			{
				emitter.myEmitter.Update(
					*playerData.myRenderInput.myTransform, 
					playerData.myFrame >= emitter.myStartFrame && playerData.myFrame <= emitter.myEndFrame
				);
			}
		}

		for (auto& playerData : myRenderQueue)
		{
			PrepareRenderData(playerData);
		}

		Vector3f cameraPos = myGraphics->GetCameraManager().GetHighlightedCamera()->transform.GetPosition();
		std::ranges::sort(myRenderPackages, [&cameraPos](const VFXSequenceRenderPackage& a, const VFXSequenceRenderPackage& b)
		{
			const float distA = (a.instanceTransform.GetPosition() - cameraPos).LengthSqr();
			const float distB = (b.instanceTransform.GetPosition() - cameraPos).LengthSqr();
			return distA > distB;
		});
	}

	void VFXManager::Resize(int aWidth, int aHeight)
	{
		myVFXPostProcessing.ConfigureDownSampleRTs(myGraphics, aWidth, aHeight);
	}

	void VFXManager::EndFrame()
	{
		myRenderPackages.clear();

	}

	void VFXManager::PrepareRenderData(VFXSequencePlayerData& aPlayerData)
	{
		KE::VFXSequence& sq = myVFXSequences[aPlayerData.mySequenceIndex];
		for (int ts = 0; ts < sq.myTimestamps.size(); ts++)
		{
			VFXTimeStamp& vfxTS = sq.myTimestamps[ts];
			if (vfxTS.myStartpoint > aPlayerData.myFrame || vfxTS.myEndpoint < aPlayerData.myFrame)
			{
				continue;
			}
			if (vfxTS.myType == VFXType::VFXMeshInstance)
			{

				VFXSequenceRenderPackage& renderPackage = myRenderPackages.emplace_back();

				renderPackage.layer = aPlayerData.myLayer;
				renderPackage.modelData = sq.myVFXMeshes[vfxTS.myEffectIndex].GetModelData();

				renderPackage.bloom = aPlayerData.myRenderInput.bloom;

				renderPackage.instanceTransform = aPlayerData.myRenderInput.GetTransform() * sq.myVFXMeshes[vfxTS.myEffectIndex].myTransform;

				for (auto& modifier : vfxTS.myCurveDataSets)
				{
					if (!modifier.IsValid()) { continue; }
					const float mod = modifier.GetEvaluatedValue(aPlayerData.myFrame, vfxTS.myStartpoint, vfxTS.myEndpoint);
					float* base = (float*)&renderPackage.attributes;
					base += (int)modifier.myType;
					*base = mod;
				}


				Vector3f scl = renderPackage.instanceTransform.GetScale();
				if (aPlayerData.myRenderInput.scaleOverride.x > -1.0f)
				{
					scl = aPlayerData.myRenderInput.scaleOverride;
				}

				Vector3f rot = {
					KE::DegToRad(renderPackage.attributes.rotation.x),
					KE::DegToRad(renderPackage.attributes.rotation.y),
					KE::DegToRad(renderPackage.attributes.rotation.z)
				};

				renderPackage.instanceTransform.TranslateLocal(renderPackage.attributes.translation);

				DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
				renderPackage.instanceTransform = rotationMatrix * renderPackage.instanceTransform.GetMatrix();

				renderPackage.instanceTransform.SetScale({
					scl.x * renderPackage.attributes.scale.x,
					scl.y * renderPackage.attributes.scale.y,
					scl.z * renderPackage.attributes.scale.z
					});

				renderPackage.customBuffer = aPlayerData.myRenderInput.customBufferInput;
			}
		}
	}

	void VFXManager::InitializeVFXMesh(VFXMeshInstance& aMeshToInitialize) const
	{
		ModelData& modelData = *aMeshToInitialize.GetModelData();
		modelData.myTransform = &aMeshToInitialize.GetTransform()->GetMatrix();
		modelData.myMeshList = &myGraphics->GetModelLoader().Load("Data/InternalAssets/cylinder.fbx");
		auto& rr = modelData.myRenderResources.emplace_back();
		rr.myMaterial = myGraphics->GetTextureLoader().GetDefaultMaterial();
		rr.myPixelShader = myGraphics->GetShaderLoader().GetPixelShader(SHADER_LOAD_PATH "Model_VFX_PS.cso");
		rr.myVertexShader = myGraphics->GetShaderLoader().GetVertexShader(SHADER_LOAD_PATH "Model_VFX_VS.cso");
	}

	void VFXManager::InitializeParticleEmitter(ParticleEmitter& anEmitterToInitialize) const
	{
		anEmitterToInitialize.Init(myGraphics, 1024, "Data/InternalAssets/defaultTexture.png");
	}

	void VFXManager::TriggerVFXSequence(int aVFXSequenceIndex, const VFXRenderInput& aRenderInput)
	{
		VFXSequencePlayerData& sqData = myRenderQueue.emplace_back(aRenderInput);
		sqData.mySequenceIndex = aVFXSequenceIndex;
		sqData.myRenderInput = aRenderInput;
		sqData.myTimer = 0.0f;
		sqData.myIsLooping = aRenderInput.looping;
		sqData.myLayer = aRenderInput.myLayer;

		sqData.myTimestampTriggerInfo.resize(
			myVFXSequences[aVFXSequenceIndex].myTimestamps.size(),
			false
		);

		const size_t emitterCount = myVFXSequences[aVFXSequenceIndex].myParticleEmitters.size();
		sqData.myEmitters.reserve(emitterCount);
		for (int i = 0; i < emitterCount; ++i)
		{
			sqData.myEmitters.push_back(myVFXSequences[aVFXSequenceIndex].myParticleEmitters[i]);
		}
	}

	void VFXManager::StopVFXSequence(int aVFXSequenceIndex, const VFXRenderInput& aRenderInput)
	{
		//this is stupid but fuck it, we look for a playerData with the same index and transform pointer
		for (int i = 0; i < myRenderQueue.size(); i++)
		{
			if (myRenderQueue[i].mySequenceIndex == aVFXSequenceIndex &&
				myRenderQueue[i].myRenderInput.myTransform == aRenderInput.myTransform)
			{
				myRenderQueue.erase(myRenderQueue.begin() + i);
				i--;
			}
		}
	}

	void VFXManager::SaveVFXSequence(VFXSequence* aSequence)
	{
		nlohmann::json output;
		VFXSequence& sq = *aSequence;
		
		output["name"] = sq.myName;
		output["duration"] = sq.myDuration;
		
		output["meshes"] = nlohmann::json::array();
		output["particleEmitters"] = nlohmann::json::array();
		output["timestamps"] = nlohmann::json::array();

		for (auto& VFXMesh : sq.myVFXMeshes)
		{
			nlohmann::json mesh;
			mesh["name"]	 = VFXMesh.myModelData.myMeshList->myFilePath;

			mesh["albedo"]   = VFXMesh.myModelData.myRenderResources[0].myMaterial->myTextures[0]->myMetadata.myFilePath;
			mesh["normal"]   = VFXMesh.myModelData.myRenderResources[0].myMaterial->myTextures[1]->myMetadata.myFilePath;
			mesh["material"] = VFXMesh.myModelData.myRenderResources[0].myMaterial->myTextures[2]->myMetadata.myFilePath;
			mesh["effects"]  = VFXMesh.myModelData.myRenderResources[0].myMaterial->myTextures[3]->myMetadata.myFilePath;

			mesh["vertexShader"] = VFXMesh.myModelData.myRenderResources[0].myVertexShader->GetName();
			mesh["pixelShader"] = VFXMesh.myModelData.myRenderResources[0].myPixelShader->GetName();

			output["meshes"].push_back(mesh);
		}

		for (auto& emitter : sq.myParticleEmitters)
		{
			nlohmann::json em;
			em["particleCapacity"] = emitter.myEmitter.GetSpriteBatch()->myInstances.size();
			em["particleTexture"] = emitter.myEmitter.GetSpriteBatch()->myData.myTexture->myMetadata.myFilePath;
			em["particleMode"] = (int)emitter.myEmitter.GetSpriteBatch()->myData.myMode;

			nlohmann::json spa;
			auto& attr = emitter.myEmitter.GetSharedAttributes();

			//
			spa["burstTimeMin"] = attr.burstTimeMin;
			spa["burstTimeMax"] = attr.burstTimeMax;
			spa["burstCountMin"] = attr.burstCountMin;
			spa["burstCountMax"] = attr.burstCountMax;

			spa["velocityMin"] = attr.velocityMin;
			spa["velocityMax"] = attr.velocityMax;

			spa["accelerationMin"] = attr.accelerationMin;
			spa["accelerationMax"] = attr.accelerationMax;

			spa["velocityDegradation"] = attr.velocityDegradation;
			spa["accelerationDegradation"] = attr.accelerationDegradation;

			spa["lifeTimeMin"] = attr.lifeTimeMin;
			spa["lifeTimeMax"] = attr.lifeTimeMax;
			spa["lifeTimeMidPoint"] = attr.lifeTimeMidPoint;

			spa["angleMin"] = attr.angleMin;
			spa["angleMax"] = attr.angleMax;

			spa["horizontalVelocityFactor"] = attr.horizontalVelocityFactor;
			spa["verticalVelocityFactor"] = attr.verticalVelocityFactor;

			spa["startColor"] = { attr.startColor.x, attr.startColor.y, attr.startColor.z, attr.startColor.w };
			spa["midColor"] = { attr.midColor.x, attr.midColor.y, attr.midColor.z, attr.midColor.w };
			spa["endColor"] = { attr.endColor.x, attr.endColor.y, attr.endColor.z, attr.endColor.w };

			spa["startSize"] = attr.startSize;
			spa["midSize"] = attr.midSize;
			spa["endSize"] = attr.endSize;

			//

			em["sharedParticleAttributes"] = spa;

			output["particleEmitters"].push_back(em);
		}

		for (auto& timestamp : sq.myTimestamps)
		{
			nlohmann::json ts;
			ts["type"] = (int)timestamp.myType;
			ts["start"] = timestamp.myStartpoint;
			ts["end"] = timestamp.myEndpoint;
			ts["effectIndex"] = timestamp.myEffectIndex;
			
			ts["curves"] = nlohmann::json::array();
			for (VFXCurveDataSet& curve : timestamp.myCurveDataSets)
			{
				nlohmann::json cd;
				cd["curveAttribute"] = (int)curve.myType;
				cd["curveProfile"] = (int)curve.myCurveProfile;
				cd["minValue"] = curve.myMinValue;
				cd["maxValue"] = curve.myMaxValue;
				cd["points"] = nlohmann::json::array();
				
				for (Vector2f& point : curve.myData)
				{
					nlohmann::json p;
					p["x"] = point.x;
					p["y"] = point.y;
					cd["points"].push_back(p);
				}
				
				ts["curves"].push_back(cd);
			}
			output["timestamps"].push_back(ts);
		}
		
		std::string jsonStr = output.dump(4);

		std::string out = VFX_SEQUENCE_FILE_LOCATION + sq.myName + ".kittyVFX";
		std::ofstream file(out);

		if (!file.is_open())
		{
			KE_ERROR("Failed to save sequence %s (%i)", sq.myName.c_str(), sq.myIndex);
			int i = 0;
			for (i = 0; i < 128 && !file.is_open(); i++)
			{
				out = VFX_SEQUENCE_FILE_LOCATION + sq.myName + std::format("backupSave_{}.kittyVFX", i);
				file.open(out);
			}
		}

		file << jsonStr;
		file.close();
	}

	void VFXManager::LoadVFXSequence(int aVFXSequenceIndex, const std::string& aFilePath)
	{
		VFXSequence& sq = myVFXSequences[aVFXSequenceIndex];

		sq.myVFXMeshes.clear();
		sq.myParticleEmitters.clear();
		sq.myTimestamps.clear();

		nlohmann::json input;
		std::string fileToLoad = VFX_SEQUENCE_FILE_LOCATION + aFilePath + ".kittyVFX";
		std::ifstream file(fileToLoad);

		if (!file.is_open())
		{
			//File we're trying to load does not exist. This is currently okay, we create it using the default template.
			std::filesystem::copy("Data/InternalAssets/VFXSequences/default.kittyVFX", fileToLoad);
			file.open(fileToLoad);

			if (!file.is_open())
			{
				assert(false && "default.kittyVFX not found! attempted to load %s");
			}
		}

		file >> input;
		file.close();
		
		sq.myDuration = input["duration"];
		
		sq.myVFXMeshes.reserve(32); //temp!? TODO: fix

		

		//load meshes
		for (auto& mesh : input["meshes"])
		{
			VFXMeshInstance& meshData = sq.myVFXMeshes.emplace_back();
			meshData.myModelData.myMeshList = &myGraphics->GetModelLoader().Load(mesh["name"]);
			meshData.myModelData.myRenderResources.emplace_back();
			meshData.myModelData.myRenderResources[0].myMaterial = myGraphics->GetTextureLoader().GetCustomMaterial(
				mesh["albedo"],
				mesh["normal"],
				mesh["material"],
				mesh["effects"]
			);

			meshData.myModelData.myRenderResources[0].myVertexShader = myGraphics->GetShaderLoader().GetVertexShader(mesh["vertexShader"]);
			meshData.myModelData.myRenderResources[0].myPixelShader = myGraphics->GetShaderLoader().GetPixelShader(mesh["pixelShader"]);
			meshData.myModelData.myTransform = &meshData.myTransform.GetMatrix();
		}

		//load particle emitters
		for (auto& emitter : input["particleEmitters"])
		{
			VFXEmitter& emit = sq.myParticleEmitters.emplace_back();
			auto& em = emit.myEmitter;

			em.Init(myGraphics, emitter["particleCapacity"], emitter["particleTexture"]);

			em.GetSpriteBatch()->myData.myMode = (SpriteBatchMode)emitter["particleMode"];

			auto& attr = em.GetSharedAttributes();
			auto& spa = emitter["sharedParticleAttributes"];

			attr.burstTimeMin = spa["burstTimeMin"];
			attr.burstTimeMax = spa["burstTimeMax"];
			attr.burstCountMin = spa["burstCountMin"];
			attr.burstCountMax = spa["burstCountMax"];

			attr.velocityMin = spa["velocityMin"];
			attr.velocityMax = spa["velocityMax"];

			attr.accelerationMin = spa["accelerationMin"];
			attr.accelerationMax = spa["accelerationMax"];

			attr.velocityDegradation = spa["velocityDegradation"];
			attr.accelerationDegradation = spa["accelerationDegradation"];

			attr.lifeTimeMin = spa["lifeTimeMin"];
			attr.lifeTimeMax = spa["lifeTimeMax"];
			attr.lifeTimeMidPoint = spa["lifeTimeMidPoint"];

			attr.angleMin = spa["angleMin"];
			attr.angleMax = spa["angleMax"];

			attr.horizontalVelocityFactor = spa["horizontalVelocityFactor"];
			attr.verticalVelocityFactor = spa["verticalVelocityFactor"];

			attr.startColor = Vector4f(spa["startColor"][0], spa["startColor"][1], spa["startColor"][2], spa["startColor"][3]);
			attr.midColor = Vector4f(spa["midColor"][0], spa["midColor"][1], spa["midColor"][2], spa["midColor"][3]);
			attr.endColor = Vector4f(spa["endColor"][0], spa["endColor"][1], spa["endColor"][2], spa["endColor"][3]);

			attr.startSize = spa["startSize"];
			attr.midSize = spa["midSize"];
			attr.endSize = spa["endSize"];
		}

		//load timestamps
		for (auto& timestamp : input["timestamps"])
		{
			VFXTimeStamp& ts = sq.myTimestamps.emplace_back();
			ts.myType = (VFXType)timestamp["type"];
			ts.myStartpoint = timestamp["start"];
			ts.myEndpoint = timestamp["end"];
			ts.myEffectIndex = timestamp["effectIndex"];

			if (ts.myType == VFXType::ParticleEmitter)
			{
				sq.myParticleEmitters[ts.myEffectIndex].myStartFrame = ts.myStartpoint;
				sq.myParticleEmitters[ts.myEffectIndex].myEndFrame = ts.myEndpoint;
			}

			for (auto& curve : timestamp["curves"])
			{
				int index = (int)curve["curveAttribute"];
				if (index >= (int)VFXAttributeTypes::Count) { continue; }

				VFXCurveDataSet& cd = ts.myCurveDataSets[index];
				cd.myType = (VFXAttributeTypes)curve["curveAttribute"];
				cd.myCurveProfile = (VFXCurveProfiles)curve["curveProfile"];
				cd.myMinValue = curve["minValue"];
				cd.myMaxValue = curve["maxValue"];

				for (auto& point : curve["points"])
				{
					cd.myData.push_back(Vector2f(point["x"], point["y"]));
				}
			}
		}
	}

	int VFXManager::CreateVFXSequence(const std::string& aName)
	{
		VFXSequence& sq = myVFXSequences.emplace_back();

		sq.myIndex = (int)myVFXSequences.size() - 1;
		sq.myManager = this;
		LoadVFXSequence(sq.myIndex, aName);

		sq.myName = aName;

		return sq.myIndex;
	}

	int VFXManager::GetVFXSequenceFromName(const std::string& aName)
	{
		for (auto& seq : myVFXSequences)
		{
			if (seq.myName == aName)
			{
				return seq.myIndex;
			}
		}

		return CreateVFXSequence(aName);
	}

	VFXSequence* VFXManager::GetVFXSequence(int aVFXSequenceIndex)
	{
		return &myVFXSequences[aVFXSequenceIndex];
	}

	void VFXManager::ClearVFX()
	{
		myRenderQueue.clear();
	}

	void VFXManager::RenderVFXDirect(int aVFXIndex, int aCurrentFrame, Transform* aTransform, eRenderLayers aLayer)
	{
		const VFXRenderInput in(*aTransform, false, false);

		//create a custom player data
		VFXSequencePlayerData playerData = {in};
		playerData.mySequenceIndex = aVFXIndex;
		playerData.myFrame = aCurrentFrame;
		playerData.myRenderInput.myTransform = aTransform;
		playerData.myTimer = 0.0f;
		playerData.myLayer = aLayer;

		PrepareRenderData(playerData);
	}
}
