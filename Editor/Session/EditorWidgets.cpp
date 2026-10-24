// SPDX-License-Identifier: MIT
#include "Editor/Session/EditorWidgets.h"
#include "Editor/Session/EditorTheme.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cfloat>

namespace EditorWidgets
{
void DrawIcon(Icon icon, ImVec2 position, float size)
{
    auto* draw = ImGui::GetWindowDrawList();
    const auto& palette = EditorTheme::GetPalette();
    const ImU32 color = ImGui::GetColorU32(icon == Icon::Folder ? palette.folder : palette.icon);
    const auto p = [&](float x, float y) { return ImVec2(position.x+x*size,position.y+y*size); };
    const float stroke = std::max(1.0f, size/14.0f);
    const auto line = [&](float x,float y,float u,float v) { draw->AddLine(p(x,y),p(u,v),color,stroke); };
    switch (icon)
    {
    case Icon::Folder:
        draw->AddRectFilled(p(.05f,.25f),p(.45f,.46f),color,size*.07f);
        draw->AddRectFilled(p(.05f,.36f),p(.95f,.88f),color,size*.06f);
        break;
    case Icon::Mesh:
        line(.5f,.05f,.9f,.27f); line(.9f,.27f,.9f,.73f); line(.9f,.73f,.5f,.95f);
        line(.5f,.95f,.1f,.73f); line(.1f,.73f,.1f,.27f); line(.1f,.27f,.5f,.05f);
        line(.1f,.27f,.5f,.5f); line(.9f,.27f,.5f,.5f); line(.5f,.5f,.5f,.95f);
        break;
    case Icon::Light:
        draw->AddCircle(p(.5f,.38f),size*.24f,color,16,stroke);
        line(.35f,.57f,.35f,.77f); line(.65f,.57f,.65f,.77f);
        line(.35f,.77f,.65f,.77f); line(.4f,.9f,.6f,.9f);
        break;
    case Icon::Camera:
        draw->AddRect(p(.07f,.25f),p(.7f,.8f),color,size*.07f,0,stroke);
        line(.7f,.4f,.95f,.25f); line(.95f,.25f,.95f,.8f); line(.95f,.8f,.7f,.65f);
        break;
    case Icon::Move:
        line(.1f,.5f,.9f,.5f); line(.5f,.1f,.5f,.9f);
        line(.1f,.5f,.27f,.34f); line(.1f,.5f,.27f,.66f);
        line(.9f,.5f,.73f,.34f); line(.9f,.5f,.73f,.66f);
        line(.5f,.1f,.34f,.27f); line(.5f,.1f,.66f,.27f);
        line(.5f,.9f,.34f,.73f); line(.5f,.9f,.66f,.73f);
        break;
    case Icon::Rotate:
        draw->PathArcTo(p(.5f,.5f),size*.34f,.2f,5.2f,20); draw->PathStroke(color,0,stroke);
        line(.67f,.09f,.64f,.35f); line(.67f,.09f,.87f,.25f);
        break;
    case Icon::Scale:
        draw->AddRect(p(.12f,.43f),p(.57f,.88f),color,0,0,stroke);
        line(.4f,.6f,.9f,.1f); line(.63f,.1f,.9f,.1f); line(.9f,.1f,.9f,.37f);
        break;
    case Icon::World:
        draw->AddCircle(p(.5f,.5f),size*.4f,color,20,stroke);
        draw->AddEllipse(p(.5f,.5f),ImVec2(size*.18f,size*.4f),color,0,20,stroke);
        line(.1f,.5f,.9f,.5f);
        break;
    case Icon::Local:
        line(.2f,.8f,.2f,.15f); line(.2f,.8f,.9f,.8f); line(.2f,.8f,.7f,.3f);
        break;
    case Icon::File:
        draw->AddRect(p(.2f,.08f),p(.8f,.92f),color,size*.03f,0,stroke);
        line(.33f,.42f,.67f,.42f); line(.33f,.6f,.67f,.6f);
        break;
    }
}

bool IconButton(const char* id, Icon icon, const char* tooltip, bool selected, const char* label)
{
    const float scale = EditorTheme::GetScale();
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::GetPalette().selected);
    const float height = std::max(26*scale, ImGui::GetFrameHeight());
    const float width = label ? height + ImGui::CalcTextSize(label).x + 8*scale : height;
    const bool pressed = ImGui::Button(id,ImVec2(width,height));
    if (selected) ImGui::PopStyleColor();
    const auto p = ImGui::GetItemRectMin();
    DrawIcon(icon,ImVec2(p.x+5*scale,p.y+(height-16*scale)*.5f),16*scale);
    if (label)
        ImGui::GetWindowDrawList()->AddText(ImVec2(p.x+height,p.y+(height-ImGui::GetFontSize())*.5f),
            ImGui::GetColorU32(ImGuiCol_Text),label);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s",tooltip);
    return pressed;
}

bool IconSelectable(const char* id, Icon icon, const char* label, bool selected)
{
    const float scale = EditorTheme::GetScale();
    const bool pressed = ImGui::Selectable(id,selected,0,ImVec2(0,25*scale));
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    DrawIcon(icon,ImVec2(min.x+5*scale,min.y+4*scale),17*scale);
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(min,max,true);
    draw->AddText(ImVec2(min.x+30*scale,min.y+4*scale),ImGui::GetColorU32(ImGuiCol_Text),label);
    if (selected) draw->AddRectFilled(min,ImVec2(min.x+2*scale,max.y),
        ImGui::ColorConvertFloat4ToU32(EditorTheme::GetPalette().blue));
    draw->PopClipRect();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",label);
    return pressed;
}

bool ObjectHeader(Icon icon, const char* name, const char* type, bool* visible)
{
    const float scale = EditorTheme::GetScale();
    bool changed = false;
    const bool inlineVisible = visible && ImGui::GetContentRegionAvail().x >= 255*scale;
    if (ImGui::BeginTable("##ObjectHeader", inlineVisible ? 3 : 2))
    {
        ImGui::TableSetupColumn("Icon",ImGuiTableColumnFlags_WidthFixed,30*scale);
        ImGui::TableSetupColumn("Identity",ImGuiTableColumnFlags_WidthStretch);
        if (inlineVisible)
            ImGui::TableSetupColumn("Visible",ImGuiTableColumnFlags_WidthFixed,
                ImGui::GetFrameHeight()+ImGui::CalcTextSize("Visible").x+ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawIcon(icon,ImGui::GetCursorScreenPos(),26*scale);
        ImGui::Dummy(ImVec2(26*scale,32*scale));
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s",name);
        ImGui::TextDisabled("%s",type);
        if (inlineVisible)
        {
            ImGui::TableNextColumn();
            changed = ImGui::Checkbox("Visible",visible);
        }
        ImGui::EndTable();
    }
    if (visible && !inlineVisible) changed = ImGui::Checkbox("Visible",visible);
    return changed;
}

bool BeginProperties(const char* id)
{
    if (!ImGui::BeginTable(id,2,ImGuiTableFlags_SizingStretchProp)) return false;
    ImGui::TableSetupColumn("Label",ImGuiTableColumnFlags_WidthFixed,85*EditorTheme::GetScale());
    ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void PropertyLabel(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool Section(const char* label, bool defaultOpen)
{
    ImGui::PushStyleColor(ImGuiCol_Header,EditorTheme::GetPalette().header);
    const bool open = ImGui::CollapsingHeader(label,defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleColor();
    return open;
}

bool DragVector3(const char* label, float* values, float speed, float minimum, float maximum, const char* format)
{
    bool changed = false;
    const float scale = EditorTheme::GetScale();
    const float width = ImGui::GetContentRegionAvail().x / scale;
    const bool labelAbove = width < 310.0f;
    const bool axesStacked = width < 210.0f;
    const int columns = axesStacked ? 2 : labelAbove ? 3 : 4;
    ImGui::PushID(label);
    if (labelAbove) ImGui::TextUnformatted(label);
    if (ImGui::BeginTable("##Vector", columns, ImGuiTableFlags_SizingStretchSame))
    {
        if (!labelAbove || axesStacked)
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, (axesStacked ? 18 : 85) * scale);
        for (int index = 0; index < (axesStacked ? 1 : 3); ++index)
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        if (!labelAbove)
        {
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
        }
        const ImU32 axes[] = { IM_COL32(200,90,100,255), IM_COL32(90,170,120,255), IM_COL32(95,145,210,255) };
        for (int index = 0; index < 3; ++index)
        {
            if (axesStacked)
            {
                if (index != 0) ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(axes[index]), "%c", "XYZ"[index]);
            }
            ImGui::TableNextColumn();
            ImGui::PushID(index);
            // Axes stay explicit even when the numeric value consumes the full field.
            if (!axesStacked)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(axes[index]), "%c", "XYZ"[index]);
                ImGui::SameLine(0,3*scale);
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            auto& fonts = ImGui::GetIO().Fonts->Fonts;
            if (fonts.Size > 1) ImGui::PushFont(fonts[1]);
            changed |= ImGui::DragFloat("##Value", values + index, speed, minimum, maximum, format);
            if (fonts.Size > 1) ImGui::PopFont();
            const auto p = ImGui::GetItemRectMin();
            const auto q = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 2 * scale, q.y), axes[index]);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s / %c", label, "XYZ"[index]);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
    return changed;
}

void DrawDockTabAccents()
{
    // Internal API is intentionally isolated here and in EditorWorkspace.
    auto& nodes = ImGui::GetCurrentContext()->DockContext.Nodes;
    for (const auto& entry : nodes.Data)
    {
        auto* node = static_cast<ImGuiDockNode*>(entry.val_p);
        if (!node || !node->HostWindow || !node->TabBar || !node->IsVisible) continue;
        auto* tab = ImGui::TabBarFindTabByID(node->TabBar,node->TabBar->SelectedTabId);
        if (!tab) continue;
        const auto bar = node->TabBar->BarRect;
        const bool central = (tab->Flags & ImGuiTabItemFlags_SectionMask_) == 0;
        const float x = bar.Min.x + ImTrunc(tab->Offset - (central ? node->TabBar->ScrollingAnim : 0.0f));
        auto* draw = node->HostWindow->DrawList;
        const ImVec2 clipMin(central ? std::max(bar.Min.x,node->TabBar->ScrollingRectMinX) : bar.Min.x,bar.Min.y);
        const ImVec2 clipMax(central ? std::min(bar.Max.x,node->TabBar->ScrollingRectMaxX) : bar.Max.x,bar.Max.y);
        draw->PushClipRect(clipMin,clipMax,true);
        draw->AddLine(ImVec2(x,bar.Max.y-1),ImVec2(x+tab->Width,bar.Max.y-1),
            ImGui::ColorConvertFloat4ToU32(EditorTheme::GetPalette().blue),2*EditorTheme::GetScale());
        draw->PopClipRect();
    }
}
}
