// SPDX-License-Identifier: MIT
#include "Engine/Runtime/Scene/ContentIdentity.h"
#include <random>
#include <algorithm>

bool IsContentGuid(const std::string& value)
{
    return value.size()==32 && value.find_first_not_of('0')!=std::string::npos &&
        std::all_of(value.begin(),value.end(),[](char c) { return (c>='0'&&c<='9')||(c>='a'&&c<='f'); });
}
std::string NewContentGuid()
{
    std::random_device random;
    std::string result(32,'0');
    constexpr char digits[]="0123456789abcdef";
    do {
        for (std::size_t i=0;i<4;++i) {
            const auto word=static_cast<std::uint32_t>(random());
            for (std::size_t j=0;j<8;++j) result[i*8+j]=digits[(word>>(j*4))&15];
        }
    } while (!IsContentGuid(result));
    return result;
}
