#pragma once
#include "Engine/Source/Graphics/ModelData.h"
#include "Engine/Source/Graphics/Particle/ParticleEmitter.h"

namespace KE
{
	class CBuffer;
	constexpr int VFX_SEQUENCE_FRAME_RATE = 120;
	constexpr const char* VFX_SEQUENCE_FILE_LOCATION = "Data/InternalAssets/VFXSequences/";

	class ParticleEmitter;
	class SpriteManager;

	struct MeshList;
	class VFXMeshInstance;
	class VFXManager;

	struct VFXBufferData
	{
		Vector4f colour;
		Vector2f uvOffset;

		Vector2f uvScale;
		Vector4f bloomAttributes;
	};

	enum class VFXType : int
	{
		ParticleEmitter,
		VFXMeshInstance,

		Count
	};

	enum class VFXAttributeTypes
	{
		UV_X_SCROLL,
		UV_Y_SCROLL,

		TRANSLATION_X,
		TRANSLATION_Y,
		TRANSLATION_Z,

		ROTATION_X,
		ROTATION_Y,
		ROTATION_Z,

		SCALE_X,
		SCALE_Y,
		SCALE_Z,

		COLOUR_R,
		COLOUR_G,
		COLOUR_B,
		COLOUR_A,

		UV_X_SCALE,
		UV_Y_SCALE,
		//

		Count
	};

	static const char* VFXAttributeTypeNames[(int)VFXAttributeTypes::Count] =
	{
		"UV Offset: X",
		"UV Offset: Y",

		"Translation: X",
		"Translation: Y",
		"Translation: Z",

		"Rotation: X",
		"Rotation: Y",
		"Rotation: Z",

		"Scale: X",
		"Scale: Y",
		"Scale: Z",

		"Colour: R",
		"Colour: G",
		"Colour: B",
		"Colour: A",

		"UV Scale: X",
		"UV Scale: Y"
	};

	static const Vector3f VFXCurveDefaults[(int)VFXAttributeTypes::Count] =
	{
		{0.0f, 1.0f, 0.5f  }, //UV_X_SCROLL
		{0.0f, 1.0f, 0.5f  }, //UV_Y_SCROLL

		{-1.0f, 1.0f, 0.5f }, //TRANSLATION_X
		{-1.0f, 1.0f, 0.5f }, //TRANSLATION_Y
		{-1.0f, 1.0f, 0.5f }, //TRANSLATION_Z

		{0.0f, 360.0f, 0.5f }, //ROTATION_X
		{0.0f, 360.0f, 0.5f }, //ROTATION_Y
		{0.0f, 360.0f, 0.5f }, //ROTATION_Z

		{-1.0f, 1.0f, 0.5f }, //SCALE_X
		{-1.0f, 1.0f, 0.5f }, //SCALE_Y
		{-1.0f, 1.0f, 0.5f }, //SCALE_Z

		{0.0f, 1.0f, 0.5f  }, //COLOUR_R
		{0.0f, 1.0f, 0.5f  }, //COLOUR_G
		{0.0f, 1.0f, 0.5f  }, //COLOUR_B
		{0.0f, 1.0f, 0.5f  }, //COLOUR_A

		{0.0f, 1.0f, 0.5f  }, //UV_X_SCALE
		{0.0f, 1.0f, 0.5f  }, //UV_Y_SCALE

	};

	enum class VFXCurveProfiles
	{
		None,
		Discrete,
		Linear,
		Smooth,
		Count
	};

	static const char* VFXCurveProfileNames[(int)VFXCurveProfiles::Count] =
	{
		"None",
		"Discrete",
		"Linear",
		"Smooth",
	};

	struct VFXCustomBufferInput
	{
		CBuffer* constantBuffer = nullptr;
		void* bufferData = nullptr;

		int bufferSlot = 0;
		int bufferSize = 0;
	};

	struct VFXCurveDataSet
	{
		VFXAttributeTypes myType = VFXAttributeTypes::Count;
		VFXCurveProfiles myCurveProfile = VFXCurveProfiles::Smooth;

		float myMinValue = 0.0f;
		float myMaxValue = 2.0f;

		std::vector<Vector2f> myData;
		bool visible = true;

		const char* GetName() { return VFXAttributeTypeNames[(int)myType]; }
		bool IsValid() const { return myType != VFXAttributeTypes::Count; }
		float GetEvaluatedValue(int aFrameIndex, int aFirstFrameIndex, int aLastFrameIndex) const;
	};

	struct VFXTimeStamp
	{
		int myStartpoint = 0;
		int myEndpoint = 0;

		int myEffectIndex = 0;
		VFXType myType = VFXType::Count;

		bool myIsOpened = false;
		std::array<KE::VFXCurveDataSet, (size_t)KE::VFXAttributeTypes::Count> myCurveDataSets;
	};

	struct VFXRenderInput
	{
		union
		{
			Transform* myTransform;
			Transform myStationaryTransform;
		};

		eRenderLayers myLayer = eRenderLayers::Main;

		bool looping = false;
		bool isStationary = false;
		bool bloom = true;
		Vector3f scaleOverride = { -1.0f, -1.0f, -1.0f };

		VFXCustomBufferInput customBufferInput;

		Transform& GetTransform() { if (isStationary) { return myStationaryTransform; } return *myTransform; }

		explicit VFXRenderInput(Transform& aTransform, bool aIsLooping = false, bool aIsStationary = false);
	};

	struct VFXEmitter
	{
		ParticleEmitter myEmitter;
		int myStartFrame = 0;
		int myEndFrame = 0;
	};

	struct VFXSequence
	{
		std::string myName = "New Sequence";
		int myDuration = 1 * VFX_SEQUENCE_FRAME_RATE;
		int myIndex = -1;

		std::vector<VFXMeshInstance> myVFXMeshes;
		std::vector<VFXEmitter> myParticleEmitters;
		std::vector<VFXTimeStamp> myTimestamps;

		VFXManager* myManager = nullptr;

		void AddVFXMeshInstance();
		void AddParticleEmitter();
	};

	struct VFXSequencePlayerData
	{
		VFXRenderInput myRenderInput;
		eRenderLayers myLayer;

		std::vector<bool> myTimestampTriggerInfo;
		std::vector<VFXEmitter> myEmitters;

		int mySequenceIndex;
		float myTimer;
		int myFrame;
		bool myIsLooping;
		bool myIsWaitingOnParticles = false;
	};

	struct VFXSequenceRenderPackage
	{
		ModelData* modelData;
		Transform instanceTransform;
		eRenderLayers layer;
		VFXCustomBufferInput customBuffer;

		struct
		{
			Vector2f uvOffset = { 0.0f,0.0f };
			Vector3f translation = { 0.0f,0.0f,0.0f };
			Vector3f rotation = { 0.0f,0.0f,0.0f };
			Vector3f scale = { 1.0f,1.0f,1.0f };
			Vector4f colour = { 1.0f,1.0f,1.0f,1.0f };
			Vector2f uvScale = { 1.0f,1.0f };
		} attributes;

		bool bloom = true;
	};

	class VFXMeshInstance
	{
		friend class VFXManager;
	private:
		Transform myTransform;

		ModelData myModelData;
	public:
		VFXMeshInstance() = default;
		~VFXMeshInstance() = default;

		inline void SetModelData(const ModelData& aModelData) { myModelData = aModelData; }
		inline ModelData* GetModelData() { return &myModelData; }
		inline Transform* GetTransform() { return &myTransform; }
	};
}
