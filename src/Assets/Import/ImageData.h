// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace AssetImport
{
struct ImageData
{
    static constexpr std::uint32_t RgbaChannelCount = 4;

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t channels = RgbaChannelCount;
    std::vector<std::uint8_t> pixels;

    bool IsValid() const
    {
        if (width == 0 || height == 0 || channels != RgbaChannelCount)
            return false;

        if (width > std::numeric_limits<std::size_t>::max() / channels)
            return false;

        const std::size_t rowSize =
            static_cast<std::size_t>(width) * channels;
        return height <= std::numeric_limits<std::size_t>::max() / rowSize &&
               pixels.size() == rowSize * height;
    }
};
}
