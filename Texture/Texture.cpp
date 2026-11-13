#include "Texture.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <tuple>
#include <thread>
#include <nlohmann/json.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb/stb_image_write.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Phantom::Texture {
using Json = nlohmann::json;
static unsigned integer(const Json& v) {
    // Reject fractional, negative and overflowing integers before conversion.
    if ((!v.is_number_unsigned() && !v.is_number_integer()) ||
        (v.is_number_integer() && v.get<int64_t>() < 0) || v.get<uint64_t>() > UINT32_MAX)
        return UINT32_MAX;
    return v.get<unsigned>();
}
const std::vector<NodeInfo>& catalog() {
    static const std::vector<NodeInfo> items{{NodeType::Solid,"solid",0},{NodeType::Noise,"noise",0},
        {NodeType::Gradient,"gradient",0},{NodeType::Checker,"checker",0},
        {NodeType::Blend,"blend",3},{NodeType::BaseColor,"base_color",1},{NodeType::Paint,"paint",0}};
    return items;
}
static const NodeInfo* info(NodeType type) {
    for (const auto& i : catalog()) if (i.type == type) return &i;
    return nullptr;
}
static bool fail(std::string& d, const char* message) { d = message; return false; }
struct Graph {
    std::map<std::string, size_t> ids;
    std::vector<std::vector<size_t>> inputs;
    std::vector<size_t> order;
};
static bool prepare(const Recipe& r, Graph& g, std::string& d) {
    if ((r.formatVersion != 1 && r.formatVersion != 2) || r.evaluatorVersion != r.formatVersion) return fail(d,"Unsupported recipe/evaluator version");
    if (r.resolution != 512 && r.resolution != 1024 && r.resolution != 2048 && r.resolution != 4096)
        return fail(d,"Resolution must be 512, 1024, 2048 or 4096");
    if (r.nodes.empty() || r.nodes.size() > 256) return fail(d,"Graph must contain 1..256 nodes");
    g.inputs.resize(r.nodes.size());
    size_t sampleCount = 0, strokeCount = 0;
    for (size_t n = 0; n < r.nodes.size(); ++n) {
        const auto& node = r.nodes[n]; const auto* i = info(node.type);
        if (!i || node.id.empty() || !g.ids.emplace(node.id,n).second) return fail(d,"Unknown type, empty or duplicate node ID");
        if (!node.scale || node.scale > 4096 || !std::isfinite(node.x) || !std::isfinite(node.y)) return fail(d,"Invalid node parameters");
        for (auto c : {node.first,node.second}) for (float v : c)
            if (!std::isfinite(v) || v < 0 || v > 1) return fail(d,"Colors must be finite linear RGBA in [0,1]");
        if (node.type == NodeType::Paint && r.formatVersion < 2) return fail(d,"Paint requires recipe version 2");
        if (node.type != NodeType::Paint && !node.strokes.empty()) return fail(d,"Strokes require a paint node");
        strokeCount += node.strokes.size();
        for (const auto& s : node.strokes) {
            if (!std::isfinite(s.radius) || s.radius < 0.00025f || s.radius > 1 ||
                !std::isfinite(s.strength) || s.strength < 0 || s.strength > 1 ||
                !std::isfinite(s.hardness) || s.hardness < 0 || s.hardness > 1 || s.samples.empty()) return fail(d,"Invalid brush parameters");
            for (float v : s.color) if (!std::isfinite(v) || v < 0 || v > 1) return fail(d,"Invalid brush color");
            sampleCount += s.samples.size();
            for (const auto& uv : s.samples) for (float v : uv)
                if (!std::isfinite(v) || v < -1 || v > 2) return fail(d,"Invalid stroke sample");
        }
        if (strokeCount > 10000 || sampleCount > 100000) return fail(d,"Stroke/sample limit exceeded");
        g.inputs[n].assign(i->inputs, r.nodes.size());
    }
    auto out = g.ids.find(r.output);
    if (out == g.ids.end() || r.nodes[out->second].type != NodeType::BaseColor) return fail(d,"Output must reference a Base Color node");
    for (const auto& c : r.connections) {
        auto s = g.ids.find(c.source), t = g.ids.find(c.target);
        if (s == g.ids.end() || t == g.ids.end() || c.input >= g.inputs[t->second].size()) return fail(d,"Unknown node or input socket");
        if (r.nodes[s->second].type == NodeType::BaseColor) return fail(d,"Base Color is a terminal socket, not a color source");
        auto& slot = g.inputs[t->second][c.input];
        if (slot != r.nodes.size()) return fail(d,"Input has multiple connections");
        slot = s->second;
    }
    std::vector<int> state(r.nodes.size());
    std::function<bool(size_t)> visit = [&](size_t n) {
        if (state[n] == 1) return fail(d,"Graph contains a cycle");
        if (state[n] == 2) return true;
        state[n] = 1;
        for (size_t s : g.inputs[n]) {
            if (s == r.nodes.size()) return fail(d,"Required input is disconnected");
            if (!visit(s)) return false;
        }
        state[n] = 2; g.order.push_back(n); return true;
    };
    for (size_t n=0; n<r.nodes.size(); ++n) if (!visit(n)) return false;
    d.clear(); return true;
}
bool validate(const Recipe& r, std::string& d) { Graph g; return prepare(r,g,d); }
static Color mix(Color a, Color b, float t) {
    for (size_t i=0;i<4;++i) a[i] += (b[i]-a[i])*t;
    return a;
}
static uint32_t hash(uint32_t v) { v ^= v>>16; v *= 0x7feb352du; v ^= v>>15; v *= 0x846ca68bu; return v^(v>>16); }
static Color over(Color base, Color paint, float alpha) {
    float a = alpha + base[3] * (1-alpha);
    Color c{0,0,0,a};
    if (a > 0) for (size_t k=0;k<3;++k) c[k]=(paint[k]*alpha+base[k]*base[3]*(1-alpha))/a;
    return c;
}
template<class F> static void parallelBands(unsigned rows, F work) {
    const unsigned count=rows<64 ? 1u : std::clamp(std::thread::hardware_concurrency(),1u,8u);
    std::vector<std::thread> workers;
    for (unsigned i=1;i<count;++i) workers.emplace_back([&,i]{work(rows*i/count,rows*(i+1)/count);});
    work(0,rows/count);
    for (auto& worker:workers) worker.join();
}
static uint8_t encode(float value,bool rgb) {
    value=std::clamp(value,0.0f,1.0f);
    if (rgb) value=value<=0.0031308f ? value*12.92f : 1.055f*std::pow(value,1.0f/2.4f)-0.055f;
    return static_cast<uint8_t>(std::lround(value*255));
}
static bool rasterPaint(const Node& node, unsigned size, Image& image, std::string& d, const std::atomic_bool* cancel,
    size_t firstStroke=0,bool keep=false,unsigned top=0,unsigned bottom=0) {
    image.width = image.height = size;
    if (!keep) image.pixels.assign(size_t(size)*size, Color{0,0,0,0});
    if (!bottom) bottom=size;
    parallelBands(bottom-top,[&](unsigned begin,unsigned end) {
      begin+=top; end+=top;
      for (size_t index=firstStroke;index<node.strokes.size();++index) {
        const auto& s=node.strokes[index];
        auto dab = [&](float u, float v) {
            if (cancel && cancel->load()) return false;
            float cx=u*size, cy=v*size, radius=s.radius*size;
            int left=std::max(0,int(std::floor(cx-radius))), right=std::min(int(size)-1,int(std::ceil(cx+radius)));
            int top=std::max(int(begin),int(std::floor(cy-radius))), bottom=std::min(int(end)-1,int(std::ceil(cy+radius)));
            for (int y=top;y<=bottom;++y) {
                if (cancel && cancel->load()) return false;
                for (int x=left;x<=right;++x) {
                    float dx=(x+0.5f-cx)/radius, dy=(y+0.5f-cy)/radius;
                    float distance=std::sqrt(dx*dx+dy*dy);
                    if (distance >= 1) continue;
                    float coverage=distance <= s.hardness ? 1 : (1-distance)/(1-s.hardness);
                    float alpha=coverage*s.strength*s.color[3];
                    auto& pixel=image.pixels[size_t(y)*size+x];
                    if (s.erase) { pixel[3]*=1-alpha; if (pixel[3]==0) pixel={0,0,0,0}; }
                    else pixel=over(pixel,s.color,alpha);
                }
            }
            return true;
        };
        const float step=std::max(s.radius*0.25f,0.5f/size);
        if (!dab(s.samples.front()[0],s.samples.front()[1])) return;
        // Carry arc length across samples, so adding collinear mouse samples does
        // not increase opacity. The last point is stamped exactly once.
        float until=step;
        auto last=s.samples.front();
        for (size_t i=1;i<s.samples.size();++i) {
            auto from=s.samples[i-1], to=s.samples[i];
            float dx=to[0]-from[0], dy=to[1]-from[1], length=std::sqrt(dx*dx+dy*dy);
            for (float offset=until; offset<=length; offset+=step) {
                float t=offset/length; last={from[0]+dx*t,from[1]+dy*t};
                if (!dab(last[0],last[1])) return;
            }
            if (length >= until) until=step-std::fmod(length-until,step);
            else until-=length;
        }
        auto end=s.samples.back();
        if (std::abs(last[0]-end[0])+std::abs(last[1]-end[1])>1e-6f && !dab(end[0],end[1])) return;
      }
    });
    if (cancel && cancel->load()) return fail(d,"Evaluation cancelled");
    return true;
}
struct Region { unsigned left=0,top=0,right=0,bottom=0; };
static bool evaluateInto(const Recipe& r, Image* image, ByteImage* bytes, bool blueFirst, std::string& d, const std::atomic_bool* cancel,
    const std::vector<Image>* prepared=nullptr,const Region* region=nullptr,std::vector<Image>* retain=nullptr) {
    Graph g; if (!prepare(r,g,d)) return false;
    if (cancel && cancel->load()) return fail(d,"Evaluation cancelled");
    std::vector<Image> generated;
    if (!prepared) {
        generated.resize(r.nodes.size());
        for (size_t n=0;n<r.nodes.size();++n) if (r.nodes[n].type==NodeType::Paint)
            if (!rasterPaint(r.nodes[n],r.resolution,generated[n],d,cancel)) return false;
    }
    const auto& paint=prepared ? *prepared : generated;
    if (image) { image->width=image->height=r.resolution; image->pixels.resize(size_t(r.resolution)*r.resolution); }
    if (bytes && !region) { bytes->width=bytes->height=r.resolution; bytes->pixels.resize(size_t(r.resolution)*r.resolution*4); }
    const size_t output=g.ids.find(r.output)->second;
    const Region area=region ? *region : Region{0,0,r.resolution,r.resolution};
    parallelBands(area.bottom-area.top,[&](unsigned begin,unsigned end) {
      begin+=area.top; end+=area.top;
      std::vector<Color> values(r.nodes.size());
      for (unsigned y=begin;y<end;++y) {
        if (cancel && cancel->load()) return;
        for (unsigned x=area.left;x<area.right;++x) {
            float u=(float(x)+0.5f)/r.resolution;
            for (size_t n : g.order) {
                const auto& node=r.nodes[n]; const auto& in=g.inputs[n]; Color c=node.first;
                switch (node.type) {
                case NodeType::Solid: break;
                case NodeType::Gradient: c=mix(node.first,node.second,u); break;
                case NodeType::Checker: c=((uint64_t(x)*node.scale/r.resolution+uint64_t(y)*node.scale/r.resolution)&1) ? node.second:node.first; break;
                case NodeType::Noise: {
                    auto a=uint32_t(uint64_t(x)*node.scale/r.resolution), b=uint32_t(uint64_t(y)*node.scale/r.resolution);
                    c=mix(node.first,node.second,float(hash(node.seed ^ hash(a) ^ hash(b+0x9e3779b9u))>>8)/16777215.0f); break;
                }
                case NodeType::Blend: {
                    auto base=values[in[0]], over=values[in[1]];
                    float alpha=over[3]*values[in[2]][0], resultAlpha=alpha+base[3]*(1-alpha);
                    c={0,0,0,resultAlpha};
                    if (resultAlpha>0) for (size_t k=0;k<3;++k) c[k]=(over[k]*alpha+base[k]*base[3]*(1-alpha))/resultAlpha;
                    break;
                }
                case NodeType::BaseColor: c=values[in[0]]; break;
                case NodeType::Paint: c=paint[n].pixels[size_t(y)*r.resolution+x]; break;
                }
                values[n]=c;
            }
            const size_t at=size_t(y)*r.resolution+x;
            const auto color=values[output];
            if (image) image->pixels[at]=color;
            else {
                auto* pixel=bytes->pixels.data()+at*4;
                pixel[0]=encode(color[blueFirst ? 2:0],true); pixel[1]=encode(color[1],true);
                pixel[2]=encode(color[blueFirst ? 0:2],true); pixel[3]=encode(color[3],false);
            }
        }
      }
    });
    if (cancel && cancel->load()) return fail(d,"Evaluation cancelled");
    if (retain) *retain=std::move(generated);
    d.clear(); return true;
}
bool evaluate(const Recipe& r, Image& destination, std::string& d, const std::atomic_bool* cancel) {
    Image image;
    if (!evaluateInto(r,&image,nullptr,false,d,cancel)) return false;
    destination=std::move(image); d.clear(); return true;
}
bool evaluateSrgb8(const Recipe& r, ByteImage& destination, std::string& d, bool blueFirst, const std::atomic_bool* cancel) {
    ByteImage image;
    if (!evaluateInto(r,nullptr,&image,blueFirst,d,cancel)) return false;
    destination=std::move(image); d.clear(); return true;
}
struct PreviewEvaluator::State { Recipe recipe; std::vector<Image> paint; ByteImage bytes; };
PreviewEvaluator::PreviewEvaluator()=default;
PreviewEvaluator::~PreviewEvaluator()=default;
static bool sameStroke(const Stroke& a,const Stroke& b) {
    return a.color==b.color && a.radius==b.radius && a.strength==b.strength && a.hardness==b.hardness && a.erase==b.erase && a.samples==b.samples;
}
static bool appendOnly(const Recipe& a,const Recipe& b) {
    if (a.formatVersion!=b.formatVersion || a.evaluatorVersion!=b.evaluatorVersion || a.resolution!=b.resolution || a.output!=b.output ||
        a.nodes.size()!=b.nodes.size() || a.connections.size()!=b.connections.size()) return false;
    for (size_t i=0;i<a.connections.size();++i) {
        const auto& x=a.connections[i]; const auto& y=b.connections[i];
        if (x.source!=y.source || x.target!=y.target || x.input!=y.input) return false;
    }
    for (size_t i=0;i<a.nodes.size();++i) {
        const auto& x=a.nodes[i]; const auto& y=b.nodes[i];
        if (x.id!=y.id || x.type!=y.type || x.first!=y.first || x.second!=y.second || x.seed!=y.seed || x.scale!=y.scale || x.strokes.size()>y.strokes.size()) return false;
        for (size_t k=0;k<x.strokes.size();++k) if (!sameStroke(x.strokes[k],y.strokes[k])) return false;
    }
    return true;
}
static Region strokeBounds(const Node& node,size_t first,unsigned size) {
    float left=1,top=1,right=0,bottom=0;
    for (size_t i=first;i<node.strokes.size();++i) {
        const auto& s=node.strokes[i];
        for (const auto& uv:s.samples) {
            left=std::min(left,uv[0]-s.radius); right=std::max(right,uv[0]+s.radius);
            top=std::min(top,uv[1]-s.radius); bottom=std::max(bottom,uv[1]+s.radius);
        }
    }
    auto lower=[&](float v){return unsigned(std::clamp(std::floor(v*size),0.0f,float(size)));};
    auto upper=[&](float v){return unsigned(std::clamp(std::ceil(v*size)+1,0.0f,float(size)));};
    return {lower(left),lower(top),upper(right),upper(bottom)};
}
bool PreviewEvaluator::evaluate(const Recipe& recipe,ByteImage& destination,std::string& d,const std::atomic_bool* cancel) {
    if (!validate(recipe,d)) return false;
    if (cancel && cancel->load()) return fail(d,"Evaluation cancelled");
    if (state_ && appendOnly(state_->recipe,recipe)) {
        struct Backup { size_t node; Region area; std::vector<Color> pixels; };
        std::vector<Backup> backups;
        Region dirty{recipe.resolution,recipe.resolution,0,0};
        for (size_t n=0;n<recipe.nodes.size();++n) {
            const size_t first=state_->recipe.nodes[n].strokes.size();
            if (first==recipe.nodes[n].strokes.size()) continue;
            const auto area=strokeBounds(recipe.nodes[n],first,recipe.resolution);
            if (area.left>=area.right || area.top>=area.bottom) continue;
            Backup backup{n,area,{}};
            const size_t width=area.right-area.left;
            backup.pixels.reserve(width*(area.bottom-area.top));
            for (unsigned y=area.top;y<area.bottom;++y) {
                const auto* row=state_->paint[n].pixels.data()+size_t(y)*recipe.resolution+area.left;
                backup.pixels.insert(backup.pixels.end(),row,row+width);
            }
            backups.push_back(std::move(backup));
            dirty.left=std::min(dirty.left,area.left); dirty.top=std::min(dirty.top,area.top);
            dirty.right=std::max(dirty.right,area.right); dirty.bottom=std::max(dirty.bottom,area.bottom);
        }
        const bool changed=dirty.left<dirty.right && dirty.top<dirty.bottom;
        std::vector<uint8_t> oldBytes;
        const size_t rowBytes=changed ? size_t(dirty.right-dirty.left)*4 : 0;
        if (changed) {
            oldBytes.reserve(rowBytes*(dirty.bottom-dirty.top));
            for (unsigned y=dirty.top;y<dirty.bottom;++y) {
                const auto* row=state_->bytes.pixels.data()+(size_t(y)*recipe.resolution+dirty.left)*4;
                oldBytes.insert(oldBytes.end(),row,row+rowBytes);
            }
        }
        auto restore=[&] {
            for (const auto& backup:backups) {
                const size_t width=backup.area.right-backup.area.left;
                for (unsigned y=backup.area.top;y<backup.area.bottom;++y)
                    std::copy_n(backup.pixels.data()+size_t(y-backup.area.top)*width,width,
                        state_->paint[backup.node].pixels.data()+size_t(y)*recipe.resolution+backup.area.left);
            }
            if (changed) for (unsigned y=dirty.top;y<dirty.bottom;++y)
                std::copy_n(oldBytes.data()+size_t(y-dirty.top)*rowBytes,rowBytes,
                    state_->bytes.pixels.data()+(size_t(y)*recipe.resolution+dirty.left)*4);
        };
        for (const auto& backup:backups) {
            const size_t first=state_->recipe.nodes[backup.node].strokes.size();
            if (!rasterPaint(recipe.nodes[backup.node],recipe.resolution,state_->paint[backup.node],d,cancel,first,true,backup.area.top,backup.area.bottom)) { restore(); return false; }
        }
        if (changed && !evaluateInto(recipe,nullptr,&state_->bytes,true,d,cancel,&state_->paint,&dirty)) { restore(); return false; }
        ByteImage result=state_->bytes;
        if (cancel && cancel->load()) { restore(); return fail(d,"Evaluation cancelled"); }
        state_->recipe=recipe; destination=std::move(result); d.clear(); return true;
    }
    auto next=std::make_unique<State>();
    if (!evaluateInto(recipe,nullptr,&next->bytes,true,d,cancel,nullptr,nullptr,&next->paint)) return false;
    if (cancel && cancel->load()) return fail(d,"Evaluation cancelled");
    next->recipe=recipe;
    destination=next->bytes;
    state_=std::move(next); d.clear(); return true;
}
bool serialize(const Recipe& r, std::string& result, std::string& d) {
    if (!validate(r,d)) return false;
    Json j={{"format_version",r.formatVersion},{"evaluator_version",r.evaluatorVersion},{"resolution",r.resolution},{"output",r.output},
        {"nodes",Json::array()},{"connections",Json::array()},{"external_dependencies",Json::array()}};
    auto nodes=r.nodes; std::sort(nodes.begin(),nodes.end(),[](const Node& a,const Node& b){return a.id<b.id;});
    for (const auto& n:nodes) {
        Json v={{"id",n.id},{"type",info(n.type)->name},{"first",n.first},{"second",n.second},
            {"seed",n.seed},{"scale",n.scale},{"position",{n.x,n.y}}};
        if (n.type==NodeType::Paint) {
            v["strokes"]=Json::array();
            for (const auto& s:n.strokes) v["strokes"].push_back({{"color",s.color},{"radius",s.radius},{"strength",s.strength},
                {"hardness",s.hardness},{"erase",s.erase},{"samples",s.samples}});
        }
        j["nodes"].push_back(std::move(v));
    }
    auto edges=r.connections; std::sort(edges.begin(),edges.end(),[](const Connection& a,const Connection& b){return std::tie(a.target,a.input,a.source)<std::tie(b.target,b.input,b.source);});
    for (const auto& c:edges) j["connections"].push_back({{"source",c.source},{"target",c.target},{"input",c.input}});
    result=j.dump(2)+'\n'; d.clear(); return true;
}
bool deserialize(const std::string& text, Recipe& destination, std::string& d) {
    auto j=Json::parse(text,nullptr,false);
    if (j.is_discarded()) return fail(d,"Invalid JSON");
    // nlohmann conversions are confined to this external-library boundary.
    try {
        Recipe r;
        r.formatVersion=integer(j.at("format_version")); r.evaluatorVersion=integer(j.at("evaluator_version"));
        r.resolution=integer(j.at("resolution")); r.output=j.at("output").get<std::string>();
        if (!j.at("nodes").is_array() || !j.at("connections").is_array() || !j.at("external_dependencies").is_array() || !j.at("external_dependencies").empty()) return fail(d,"Unsupported recipe structure/dependencies");
        for (const auto& v:j.at("nodes")) {
            Node n; n.id=v.at("id").get<std::string>(); auto name=v.at("type").get<std::string>();
            auto it=std::find_if(catalog().begin(),catalog().end(),[&](const NodeInfo& i){return name==i.name;});
            if (it==catalog().end()) return fail(d,"Unknown node type");
            n.type=it->type;
            if (v.at("first").size()!=4 || v.at("second").size()!=4 || v.at("position").size()!=2) return fail(d,"Invalid parameter dimensions");
            n.first=v.at("first").get<Color>(); n.second=v.at("second").get<Color>();
            if (!v.at("seed").is_number_unsigned() && (!v.at("seed").is_number_integer() || v.at("seed").get<int64_t>()<0)) return fail(d,"Invalid seed");
            if (v.at("seed").get<uint64_t>()>UINT32_MAX) return fail(d,"Invalid seed");
            n.seed=integer(v.at("seed")); n.scale=integer(v.at("scale"));
            n.x=v.at("position").at(0).get<float>(); n.y=v.at("position").at(1).get<float>();
            if (v.contains("strokes")) {
                if (!v.at("strokes").is_array()) return fail(d,"Invalid strokes");
                for (const auto& item:v.at("strokes")) {
                    Stroke s; s.color=item.at("color").get<Color>(); s.radius=item.at("radius").get<float>();
                    s.strength=item.at("strength").get<float>(); s.hardness=item.at("hardness").get<float>(); s.erase=item.at("erase").get<bool>();
                    s.samples=item.at("samples").get<std::vector<std::array<float,2>>>(); n.strokes.push_back(std::move(s));
                }
            }
            r.nodes.push_back(std::move(n));
        }
        for (const auto& c:j.at("connections")) r.connections.push_back({c.at("source").get<std::string>(),c.at("target").get<std::string>(),integer(c.at("input"))});
        if (!validate(r,d)) return false;
        destination=std::move(r); d.clear(); return true;
    } catch (const Json::exception&) { return fail(d,"Invalid recipe field type or missing field"); }
}
bool save(const std::filesystem::path& path, const Recipe& r, std::string& d) {
    std::string text; if (!serialize(r,text,d)) return false;
    auto temp=path; temp += ".tmp";
    { std::ofstream f(temp,std::ios::binary|std::ios::trunc); if (!f) return fail(d,"Cannot open temporary recipe");
      f.write(text.data(),std::streamsize(text.size())); f.close(); if (!f) return fail(d,"Cannot write recipe"); }
#ifdef _WIN32
    bool ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code ec; std::filesystem::rename(temp,path,ec); bool ok=!ec;
#endif
    if (!ok) { std::error_code ignored; std::filesystem::remove(temp,ignored); return fail(d,"Cannot replace recipe"); }
    d.clear(); return true;
}
bool load(const std::filesystem::path& path, Recipe& r, std::string& d) {
    std::ifstream f(path,std::ios::binary); if (!f) return fail(d,"Cannot open recipe");
    std::ostringstream text; text<<f.rdbuf(); if (f.bad()) return fail(d,"Cannot read recipe");
    return deserialize(text.str(),r,d);
}
bool writePng(const std::filesystem::path& path, const Image& image, std::string& d) {
    if (!image.width || image.width>4096 || !image.height || image.height>4096 || image.pixels.size()!=size_t(image.width)*image.height) return fail(d,"Invalid image");
    std::vector<unsigned char> bytes; bytes.reserve(image.pixels.size()*4);
    for (auto c:image.pixels) for (size_t k=0;k<4;++k) {
        if (!std::isfinite(c[k])) return fail(d,"Non-finite image color");
        bytes.push_back(encode(c[k],k<3));
    }
    std::ofstream f(path,std::ios::binary); if (!f) return fail(d,"Cannot open PNG");
    auto callback=[](void* ctx,void* data,int size){static_cast<std::ofstream*>(ctx)->write(static_cast<char*>(data),size);};
    int ok=stbi_write_png_to_func(callback,&f,int(image.width),int(image.height),4,bytes.data(),int(image.width*4));
    f.close(); if (!ok || !f) return fail(d,"Cannot write PNG"); d.clear(); return true;
}
Recipe example() {
    Recipe r; r.resolution=512;
    Node checker; checker.id="pattern"; checker.type=NodeType::Checker; checker.first={0.03f,0.1f,0.3f,1}; checker.second={0.8f,0.5f,0.1f,1};
    Node out; out.id="base"; out.type=NodeType::BaseColor;
    r.nodes={checker,out}; r.connections={{"pattern","base",0}}; r.output="base"; return r;
}
}
