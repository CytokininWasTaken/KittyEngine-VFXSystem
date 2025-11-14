#include "stdafx.h"
#include "VFXResources.h"

#include "VFXManager.h"
#include "Math/KittyMath.h"

namespace KE
{
	float VFXCurveDataSet::GetEvaluatedValue(int aFrameIndex, int aFirstFrameIndex, int aLastFrameIndex) const
	{
		const float leastTime = myData.front().x;
		const float mostTime = myData.back().x;

		//scale leastTime and mostTime to be 0->1
		const float frameProgressFraction = (float)(aFrameIndex - aFirstFrameIndex) / (float)(aLastFrameIndex - aFirstFrameIndex);
		const float time = leastTime + (mostTime - leastTime) * frameProgressFraction;

		//find the two closest points
		int lowerClosestPoint = -1;
		int upperClosestPoint = -1;

		for (int i = 0; i < myData.size(); ++i)
		{
			if (myData[i].x <= time)
			{
				lowerClosestPoint = i;
			}
			else
			{
				upperClosestPoint = i;
				break;
			}
		}

		if (lowerClosestPoint == -1)
		{
			return myData[upperClosestPoint].y;
		}
		else if (upperClosestPoint == -1)
		{
			return myData[lowerClosestPoint].y;
		}
		else
		{
			const float lowerTime = myData[lowerClosestPoint].x;
			const float upperTime = myData[upperClosestPoint].x;
			const float lowerValue = myData[lowerClosestPoint].y;
			const float upperValue = myData[upperClosestPoint].y;
			const float timeFraction = (time - lowerTime) / (upperTime - lowerTime);
			float value = 0.0f;

			switch (myCurveProfile)
			{
			case VFXCurveProfiles::Linear:
			{
				value = lowerValue + (upperValue - lowerValue) * timeFraction;
				break;
			}
			case VFXCurveProfiles::Smooth:
			{
				const float smoothStep = Smoothstep(timeFraction);
				value = lowerValue + (upperValue - lowerValue) * smoothStep;
				break;
			}
			case VFXCurveProfiles::Discrete:
			{
				value = lowerValue;
				break;
			}
			default:
				break;
			}

			return myMinValue + (myMaxValue - myMinValue) * value;
		}
	}


	VFXRenderInput::VFXRenderInput(Transform& aTransform, bool aIsLooping, bool aIsStationary)
	{
		looping = aIsLooping; isStationary = aIsStationary;
		if (aIsStationary)
		{
			myStationaryTransform = aTransform;
		}
		else
		{
			myTransform = &aTransform;
		}
	}


	void VFXSequence::AddVFXMeshInstance()
	{
		auto& newMesh = myVFXMeshes.emplace_back();
		myManager->InitializeVFXMesh(newMesh);
	}

	void VFXSequence::AddParticleEmitter()
	{
		auto& newEmitter = myParticleEmitters.emplace_back();
		myManager->InitializeParticleEmitter(newEmitter.myEmitter);
	}
}
