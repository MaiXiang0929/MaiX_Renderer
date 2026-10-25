// SPDX-License-Identifier: MIT
#include "Engine/Asset/Importer/TextureSource.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>
namespace AssetImport
{
namespace
{
bool BoundedSize(unsigned width,unsigned height)
{
    return width>0 && height>0 && width<=16384 && height<=16384 &&
        static_cast<std::uint64_t>(width)*height*4<=256u*1024u*1024u;
}
std::uint8_t SampleChannel(const ImageData& image,unsigned channel,unsigned x,unsigned y,unsigned width,unsigned height)
{
    const double sx=std::clamp((x+0.5)*image.width/width-0.5,0.0,static_cast<double>(image.width-1));
    const double sy=std::clamp((y+0.5)*image.height/height-0.5,0.0,static_cast<double>(image.height-1));
    const unsigned x0=static_cast<unsigned>(sx),y0=static_cast<unsigned>(sy);
    const unsigned x1=std::min(x0+1,image.width-1),y1=std::min(y0+1,image.height-1);
    const auto pixel=[&](unsigned px,unsigned py) { return image.pixels[(static_cast<std::size_t>(py)*image.width+px)*4+channel]; };
    const double tx=sx-x0,ty=sy-y0;
    const double a=pixel(x0,y0)*(1.0-tx)+pixel(x1,y0)*tx;
    const double b=pixel(x0,y1)*(1.0-tx)+pixel(x1,y1)*tx;
    return static_cast<std::uint8_t>(std::clamp(std::lround(a*(1.0-ty)+b*ty),0l,255l));
}
}
bool ValidateTextureSource(const TextureSource& source,std::string& error)
{
    error.clear();
    if(source.kind!=TextureSourceKind::Image && source.kind!=TextureSourceKind::Normal && source.kind!=TextureSourceKind::Orm)
        error="Unknown texture reconstruction kind.";
    else if(source.kind==TextureSourceKind::Orm)
    {
        if(!source.image.path.empty() || source.image.embeddedId || source.flipGreen)
            error="ORM reconstruction contains irrelevant image/normal fields.";
        else if(!BoundedSize(source.width,source.height))
            error="ORM dimensions must be nonzero, at most 16384 per axis and 256 MiB RGBA.";
        else
        {
            bool any=false;
            for(const auto& input:source.ormInputs) if(input)
            {
                any=true;
                if(input->image.path.empty() || input->channel>3)
                    error="ORM input requires an image path and RGBA channel in [0,3].";
            }
            if(!any) error="ORM reconstruction requires at least one input.";
        }
    }
    else
    {
        if(source.image.path.empty()) error="Image reconstruction requires a source path.";
        else if(source.width || source.height || std::any_of(source.ormInputs.begin(),source.ormInputs.end(),[](const auto& i){return i.has_value();}))
            error="Image/normal reconstruction contains irrelevant ORM fields.";
        else if(source.kind==TextureSourceKind::Image && source.flipGreen)
            error="Raw image reconstruction cannot flip a normal channel.";
    }
    return error.empty();
}
bool BuildTextureSourceImage(const TextureSource& source,const TextureImageLoader& loader,ImageData& output,std::string& error)
{
    if(!ValidateTextureSource(source,error)) return false;
    if(!loader) { error="Texture reconstruction requires an image loader."; return false; }
    try
    {
        ImageData candidate;
        const auto load=[&](const TextureSourceImage& image,ImageData& result)
        {
            if(!loader(image,result,error)) { if(error.empty()) error="Texture reconstruction input could not be loaded."; return false; }
            if(!result.IsValid() || !BoundedSize(result.width,result.height))
            { error="Texture reconstruction input is invalid or exceeds the decoded size limit."; return false; }
            return true;
        };
        if(source.kind!=TextureSourceKind::Orm)
        {
            if(!load(source.image,candidate)) return false;
            if(source.flipGreen) for(std::size_t i=1;i<candidate.pixels.size();i+=4) candidate.pixels[i]=255-candidate.pixels[i];
        }
        else
        {
            std::array<ImageData,3> inputs;
            for(unsigned i=0;i<3;++i) if(source.ormInputs[i] && !load(source.ormInputs[i]->image,inputs[i])) return false;
            candidate.width=source.width; candidate.height=source.height;
            candidate.pixels.assign(static_cast<std::size_t>(source.width)*source.height*4,255);
            for(unsigned y=0;y<source.height;++y) for(unsigned x=0;x<source.width;++x)
                for(unsigned i=0;i<3;++i) if(source.ormInputs[i])
                    candidate.pixels[(static_cast<std::size_t>(y)*source.width+x)*4+i]=SampleChannel(inputs[i],source.ormInputs[i]->channel,x,y,source.width,source.height);
        }
        output=std::move(candidate); error.clear(); return true;
    }
    catch(const std::exception& e) { error=std::string("Texture reconstruction failed: ")+e.what(); return false; }
}
}
