// SPDX-License-Identifier: MIT
#include "EditableModel.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace
{
void MergeBounds(PrimitiveBounds& result, bool& initialized,
                 const PrimitiveBounds& bounds)
{
    if (bounds.radius <= 0.0f)
        return;
    if (!initialized)
    {
        result = bounds;
        initialized = true;
        return;
    }
    const cy::Vec3f delta = bounds.center - result.center;
    const float distance = delta.Length();
    if (distance + bounds.radius <= result.radius)
        return;
    if (distance + result.radius <= bounds.radius)
    {
        result = bounds;
        return;
    }
    const float mergedRadius =
        (distance + result.radius + bounds.radius) * 0.5f;
    if (distance > 1.0e-6f)
        result.center += delta * ((mergedRadius - result.radius) / distance);
    result.radius = mergedRadius;
}
}

PrimitiveBounds EditableModel::GetWorldBounds() const
{
    PrimitiveBounds result;
    if (sections.empty())
        return result;

    const cy::Matrix4f root = transform.ToMatrix();
    bool initialized = false;
    for (const EditableModelSection& section : sections)
    {
        const PrimitiveBounds bounds = TransformBounds(
            section.localBounds, root * section.localTransform);
        MergeBounds(result, initialized, bounds);
    }
    return result;
}

EditableModel* FindEditableModel(std::vector<EditableModel>& models, ModelId id)
{
    if (!id.IsValid())
        return nullptr;
    const auto found = std::find_if(models.begin(), models.end(),
        [id](const EditableModel& model) { return model.id == id; });
    return found == models.end() ? nullptr : &*found;
}

const EditableModel* FindEditableModel(
    const std::vector<EditableModel>& models, ModelId id)
{
    if (!id.IsValid())
        return nullptr;
    const auto found = std::find_if(models.begin(), models.end(),
        [id](const EditableModel& model) { return model.id == id; });
    return found == models.end() ? nullptr : &*found;
}

std::string MakeUniqueModelName(
    const std::vector<EditableModel>& models,
    const std::string& requestedName)
{
    const std::string baseName = requestedName.empty() ? "Model" : requestedName;
    const auto isUsed = [&models](const std::string& candidate)
    {
        return std::any_of(models.begin(), models.end(),
            [&candidate](const EditableModel& model)
            {
                return model.name == candidate;
            });
    };
    if (!isUsed(baseName))
        return baseName;

    for (std::uint64_t suffix = 1; suffix != 0; ++suffix)
    {
        std::ostringstream candidate;
        candidate << baseName << '_' << std::setw(3) <<
            std::setfill('0') << suffix;
        if (!isUsed(candidate.str()))
            return candidate.str();
    }
    return {};
}

PrimitiveBounds GetSceneWorldBounds(const std::vector<EditableModel>& models)
{
    PrimitiveBounds result;
    bool initialized = false;
    for (const EditableModel& model : models)
        MergeBounds(result, initialized, model.GetWorldBounds());
    return result;
}
