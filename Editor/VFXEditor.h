#pragma once
#include "Editor/Source/EditorGraphics.h"

#ifndef KITTYENGINE_NO_EDITOR

namespace KE
{

}

namespace KE_EDITOR
{
	class VFXEditor : public EditorWindowBase
	{
		KE_EDITOR_FRIEND
	private:

		int mySequenceIndex = -1;
		VFXSequenceInterface mySequenceInterface;
		KE::VFXManager* myVFXManager = nullptr;
		KE::VFXSequence* myVFXSequence = nullptr;

		struct VFXEditorState
		{
			int selected = 0;
			int firstTime = 0;
			int current = 0;
			float currentFloat = 0.0f;
			bool playing = false;
			bool preview = true;
			Transform previewTransform;
			int previewLayer = static_cast<int>(KE::eRenderLayers::Main);
		} myState;

	public:
		VFXEditor(EditorWindowInput aStartupData = {}) : EditorWindowBase(aStartupData) {}

		const char* GetWindowName() const override;
		void Init() override;
		void Update() override;
		void Render() override;

		void SetSequenceIndex(int aSequenceIndex);
		void SetSequence(KE::VFXSequence* aVFXSequence);
		inline int GetSequenceIndex() const { return mySequenceIndex; }
	};
}

#endif