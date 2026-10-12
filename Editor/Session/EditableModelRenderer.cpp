// SPDX-License-Identifier: MIT
#include "Editor/Session/EditableModel.h"

#include "Engine/Rendering/Renderer/Core/Renderer.h"

void ApplyEditableModelTransform(EditableModel& model, Renderer& renderer)
{
    const cy::Matrix4f root = model.transform.ToMatrix();
    for (const EditableModelSection& section : model.sections)
    {
        renderer.UpdatePrimitiveTransform(
            section.primitiveId, root * section.localTransform);
    }
}
