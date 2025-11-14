#include "stdafx.h"

#include "Editor/Source/Editor.h"
#ifndef KITTYENGINE_NO_EDITOR

#include "VFXEditor.h"

#include "Editor/Source/EditorUtils.h"
#include "Editor/Source/ImGui/ImGuiHandler.h"
#include "Graphics/Graphics.h"
#include "imgui/imgui.h"

const char* KE_EDITOR::VFXEditor::GetWindowName() const
{
	return myVFXSequence->myName.c_str();
}

void KE_EDITOR::VFXEditor::Init()
{
	myVFXManager = &KE_GLOBAL::blackboard.Get<KE::Graphics>()->GetVFXManager();
}

void KE_EDITOR::VFXEditor::Update()
{
}

void KE_EDITOR::VFXEditor::Render()
{
	if (!myVFXSequence)
	{
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && ImGui::IsWindowFocused())
	{
		myState.playing = !myState.playing;
	}

	myState.currentFloat += ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? -1.0f : ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? 1.0f : 0.0f;
	myState.currentFloat += KE_GLOBAL::deltaTime * KE::VFX_SEQUENCE_FRAME_RATE * (float)myState.playing;
	myState.currentFloat = KE::Wrap(myState.currentFloat, (float)mySequenceInterface.GetFrameMin(), (float)mySequenceInterface.GetFrameMax() - 1);
	myState.current = (int)myState.currentFloat;
	
	ImGui::PushItemWidth(130);
	ImGui::InputInt("Duration (VFX Frames)", &myVFXSequence->myDuration);
	ImGui::SameLine();
	float ds = static_cast<float>(myVFXSequence->myDuration) / static_cast<float>(KE::VFX_SEQUENCE_FRAME_RATE);
	if (ImGui::InputFloat("Duration (Seconds)", &ds))
	{
		myVFXSequence->myDuration = static_cast<int>(ds * static_cast<float>(KE::VFX_SEQUENCE_FRAME_RATE));
	}

	ImGui::SameLine();
	ImGui::Checkbox("Preview", &myState.preview);
	
	if (ImGui::Button("Preview Settings")) { ImGui::OpenPopup("vfxPreviewAttr"); }
	if (ImGui::BeginPopup("vfxPreviewAttr"))
	{
		ImGui::Checkbox("Preview", &myState.preview);
		ImGui::DragFloat3("Position", &myState.previewTransform.GetPositionRef().x, 0.1f);
		ImGui::SliderInt(
			"Layer",
			&myState.previewLayer,
			static_cast<int>(KE::eRenderLayers::Back),
			static_cast<int>(KE::eRenderLayers::Count) - 1,
			EnumToString(static_cast<KE::eRenderLayers>(myState.previewLayer))
		);
		ImGui::EndPopup();
	}

	if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) { KE::VFXManager::SaveVFXSequence(myVFXSequence); }
	ImGui::SameLine();
	if (ImGui::Button("Save")) { KE::VFXManager::SaveVFXSequence(myVFXSequence); }
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		myVFXManager->LoadVFXSequence(mySequenceIndex, myVFXSequence->myName);
		SetSequenceIndex(mySequenceIndex);
	}

	ImGui::PopItemWidth();

	if (ImGui::BeginTable("VFX Sequence Table", 3, ImGuiTableFlags_Resizable))
	{
		ImGui::TableNextColumn();
		if (ImGui::BeginChild("Sequencer Child"))
		{
			const int currentCache = myState.current;
			ImSequencer::Sequencer(&mySequenceInterface, &myState.current, 0, &myState.selected, &myState.firstTime,
				ImSequencer::SEQUENCER_EDIT_STARTEND |
				ImSequencer::SEQUENCER_ADD |
				//ImSequencer::SEQUENCER_DEL | 
				ImSequencer::SEQUENCER_CHANGE_FRAME
			);
			if (currentCache != myState.current) { myState.currentFloat = (float)myState.current; }
		}
		ImGui::EndChild();

		ImGui::TableNextColumn();
		if (ImGui::BeginChild("Attribute Child") && myVFXSequence->myTimestamps.size() > myState.selected)
		{
			const KE::VFXTimeStamp& selectedStamp = myVFXSequence->myTimestamps[myState.selected];
			ImGui::Text("Selected: %d", myState.selected);

			switch (selectedStamp.myType)
			{
			case KE::VFXType::VFXMeshInstance:
			{
				KE::ModelData* modelData = myVFXSequence->myVFXMeshes[selectedStamp.myEffectIndex].GetModelData();
				ImGuiHandler::DisplayModelData(modelData);
				break;
			}
			case KE::VFXType::ParticleEmitter:
			{
				KE::ParticleEmitter* emitter = &myVFXSequence->myParticleEmitters[selectedStamp.myEffectIndex].myEmitter;
				ImGuiHandler::DisplayParticleEmitter(emitter);
				break;
			}
			default:
				break;
			}
		}
		ImGui::EndChild();

		ImGui::TableNextColumn();

		ImGuiHandler::DisplayPostProcessing(&myVFXManager->GetPostProcessing());

		ImGui::EndTable();
	}

	if (myState.preview)
	{
		if (myState.playing)
		{
			for (auto& em : myVFXSequence->myParticleEmitters)
			{
				em.myEmitter.Update(myState.previewTransform);
			}

		}
		myVFXManager->RenderVFXDirect(mySequenceIndex, myState.current, &myState.previewTransform, static_cast<KE::eRenderLayers>(myState.previewLayer));
	}
}

void KE_EDITOR::VFXEditor::SetSequenceIndex(int aSequenceIndex)
{
	KE::VFXSequence* sequence = myVFXManager->GetVFXSequence(aSequenceIndex);
	SetSequence(sequence);
}

void KE_EDITOR::VFXEditor::SetSequence(KE::VFXSequence* aVFXSequence)
{
	mySequenceIndex = aVFXSequence->myIndex;
	myVFXSequence = aVFXSequence;
	mySequenceInterface.Link(aVFXSequence);
}
#endif
