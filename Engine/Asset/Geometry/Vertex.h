// SPDX-License-Identifier: MIT
#pragma once
#include "cyVector.h"

struct Vertex {
	cy::Vec3f Position;
	cy::Vec3f Normal;
	cy::Vec2f TexCoord;
	cy::Vec4f Tangent;
};
