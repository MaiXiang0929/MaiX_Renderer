// SPDX-License-Identifier: MIT
#pragma once

#include "cyMatrix.h"

struct InstanceTransformData
{
    cy::Matrix4f modelView = cy::Matrix4f::Identity();
};

static_assert(sizeof(InstanceTransformData) == 64,
    "InstanceTransformData must match the std140 shader structure.");
