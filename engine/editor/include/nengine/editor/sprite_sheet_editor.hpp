#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <istream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "nengine/assets/asset_guid.hpp"

namespace nengine::editor {

struct SpriteSheetRegion {
    std::uint32_t x{0},y{0},width{0},height{0};
    friend bool operator==(const SpriteSheetRegion&,
                           const SpriteSheetRegion&)=default;
};

// TWOD-005: persistent Editor authoring model for automatic grid slicing and
// individually hand-drawn regions. Rendering the subrect as sprite UVs is a
// separate runtime acceptance requirement, not claimed by this document.
class SpriteSheetEditor {
public:
    bool open(assets::AssetGuid texture,std::uint32_t width,std::uint32_t height) {
        if(!texture.valid()||width==0||height==0||width>16384||height>16384)
            return false;
        texture_=texture;width_=width;height_=height;regions_.clear();
        return true;
    }
    bool add(SpriteSheetRegion region) {
        if(!texture_.valid() || !region.width || !region.height ||
           region.x>=width_ || region.y>=height_ ||
           region.width>width_-region.x || region.height>height_-region.y ||
           regions_.size()>=4096 ||
           std::find(regions_.begin(),regions_.end(),region)!=regions_.end())
            return false;
        regions_.push_back(region);return true;
    }
    bool erase(std::size_t index) {
        if(index>=regions_.size())return false;
        regions_.erase(regions_.begin()+static_cast<std::ptrdiff_t>(index));
        return true;
    }
    bool auto_grid(std::uint32_t cell_w,std::uint32_t cell_h,
                   std::uint32_t margin=0,std::uint32_t spacing=0) {
        if(!texture_.valid()||cell_w==0||cell_h==0||
           margin>=width_||margin>=height_)return false;
        std::vector<SpriteSheetRegion> next;
        // Fixed upper bound; all arithmetic is widened before calculating
        // boundaries to reject malicious dimensions without overflow.
        for(std::uint64_t y=margin;
            y+cell_h<=height_;y+=static_cast<std::uint64_t>(cell_h)+spacing){
            for(std::uint64_t x=margin;
                x+cell_w<=width_;x+=static_cast<std::uint64_t>(cell_w)+spacing){
                if(next.size()>=4096)return false;
                next.push_back({static_cast<std::uint32_t>(x),
                    static_cast<std::uint32_t>(y),cell_w,cell_h});
            }
        }
        if(next.empty())return false;
        regions_=std::move(next);return true;
    }
    assets::AssetGuid texture() const noexcept {return texture_;}
    const std::vector<SpriteSheetRegion>& regions() const noexcept {
        return regions_;
    }
    std::uint32_t width() const noexcept{return width_;}
    std::uint32_t height() const noexcept{return height_;}
    bool write(std::ostream& out) const {
        if(!texture_.valid()||regions_.empty())return false;
        std::string body="NENGINE_SPRITE_SLICES 1\n";
        body+="TEXTURE "+texture_.to_string()+"\n";
        body+="SIZE "+std::to_string(width_)+" "+std::to_string(height_)+"\n";
        body+="REGIONS "+std::to_string(regions_.size())+"\n";
        for(const auto& r:regions_){
            body+="RECT "+std::to_string(r.x)+" "+std::to_string(r.y)+" "+
                  std::to_string(r.width)+" "+std::to_string(r.height)+"\n";
        }
        body+="END_SLICES\n";
        out<<body;
        return out.good();
    }
    bool read(std::istream& input) {
        std::string tag,raw;
        unsigned version=0;
        if(!(input>>tag>>version)||tag!="NENGINE_SPRITE_SLICES"||version!=1)
            return false;
        if(!(input>>tag>>raw)||tag!="TEXTURE")return false;
        const auto guid=assets::AssetGuid::parse(raw);
        if(!guid||!guid->valid())return false;
        std::uint32_t width=0,height=0;
        if(!(input>>tag>>width>>height)||tag!="SIZE")return false;
        SpriteSheetEditor candidate;
        if(!candidate.open(*guid,width,height))return false;
        std::size_t n=0;
        if(!(input>>tag>>n)||tag!="REGIONS"||n==0||n>4096)return false;
        for(std::size_t i=0;i<n;++i){
            SpriteSheetRegion r;
            if(!(input>>tag>>r.x>>r.y>>r.width>>r.height) ||
               tag!="RECT"||!candidate.add(r))return false;
        }
        if(!(input>>tag)||tag!="END_SLICES")return false;
        if(input>>tag)return false;
        *this=std::move(candidate);return true;
    }
private:
    assets::AssetGuid texture_{};
    std::uint32_t width_{0},height_{0};
    std::vector<SpriteSheetRegion> regions_{};
};

} // namespace nengine::editor
