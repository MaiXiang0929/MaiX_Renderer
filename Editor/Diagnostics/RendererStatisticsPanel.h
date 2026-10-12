// SPDX-License-Identifier: MIT
#pragma once

class Renderer;

/// @brief 显示渲染统计并通过 Renderer 公共接口修改调试参数。
class RendererStatisticsPanel
{
public:
    void Draw(Renderer& renderer);
};
