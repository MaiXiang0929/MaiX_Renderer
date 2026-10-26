// SPDX-License-Identifier: MIT
#pragma once
#include "Engine/Runtime/Scene/WorldContentDocument.h"
// Exact authored-value comparison for validated, GUID-canonical regional documents.
// Regional global settings are required defaults by ValidateContentDocument.
// Keep serialized fields aligned with SceneDocument v7; runtime IDs/derived values are excluded.
bool RegionalContentEqual(const ContentDocument&,const ContentDocument&);
