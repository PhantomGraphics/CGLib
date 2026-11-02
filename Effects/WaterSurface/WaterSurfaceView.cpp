#include "WaterSurfaceRenderer.h"
#include "CGLib/VkAppBase/VkAppBase.h"
#include "CGLib/VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "CGLib/VulkanGraphics/SpvResolver.h"
#include "CGLib/VulkanGraphics/VulkanOffscreen.h"
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <cstdio>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

using namespace Phantom;
using namespace Phantom::Effects::WaterSurface;
class WaterSurfaceApp : public VKG::VkAppBase, public IScenarioDispatcher {
public:
    WaterSurfaceApp():VkAppBase(1280,800,"Water Surface"){}
    bool loadScenario(const std::string& path) { scenario_=true; return runner_.load(path); }
    int exitCode() const { return failed_ || runner_.hasFailed()?1:0; }
    void dispatch(const std::string& command) override {
        std::istringstream s(command); std::string op; s>>op;
        bool ok=true;
        if(op=="reset") sim_.requestReset();
        else if(op=="pause") { int v=0; s>>v; paused_=v!=0; }
        else if(op=="step") single_=true;
        else if(op=="impulse") { WaterImpulse i; s>>i.x>>i.z>>i.radius>>i.velocity; ok=sim_.addImpulse(i); }
        else if(op=="resize") { int w=0,h=0; s>>w>>h; ok=w>=320 && h>=240; if(ok) glfwSetWindowSize(getWindow().get(),w,h); }
        else if(op=="status") { responses_.push_back(failed_?"failed":"ready"); return; }
        else if(op=="verify") ok=verifyGpu();
        else if(op=="reflection" || op=="refraction") {
            int value=0; s>>value;
            if(op=="reflection") material_.reflection=value!=0; else material_.refraction=value!=0;
        }
        else if(op=="screenshot") { std::string path; s>>path; requestScreenshot(path); }
        else if(op!="wait") ok=false;
        responses_.push_back(ok?"ok":"error");
    }
    std::vector<std::string> collectResponses() override { auto r=std::move(responses_); responses_.clear(); return r; }
protected:
    void onInit() override {
        failed_=!sim_.create(getContext(),getCommandPool(),config_,spv("water_update.comp"));
        VkSamplerCreateInfo ci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        ci.magFilter=ci.minFilter=VK_FILTER_NEAREST;
        ci.addressModeU=ci.addressModeV=ci.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.maxLod=0;
        if(vkCreateSampler(getDevice(),&ci,nullptr,&sampler_)!=VK_SUCCESS) failed_=true;
        setExitCondition([this]{ return failed_ || (scenario_ && runner_.isFinished()); });
        getWindow().onMouseButton=[this](int button,int action,int) {
            if(button!=GLFW_MOUSE_BUTTON_LEFT || action!=GLFW_PRESS) return;
            double x,y; glfwGetCursorPos(getWindow().get(),&x,&y);
            int w,h; glfwGetWindowSize(getWindow().get(),&w,&h);
            if(w<=0 || h<=0) return;
            glm::vec4 a=camera_.inverseViewProjection*glm::vec4(float(x)/w*2-1,float(y)/h*2-1,0,1);
            glm::vec4 b=camera_.inverseViewProjection*glm::vec4(float(x)/w*2-1,float(y)/h*2-1,1,1);
            glm::vec3 origin=glm::vec3(a)/a.w, direction=glm::normalize(glm::vec3(b)/b.w-origin);
            if(std::abs(direction.y)<1e-6f) return;
            float t=-origin.y/direction.y;
            glm::vec3 p=origin+t*direction;
            if(t>0 && p.x>=0 && p.x<=sim_.config().width && p.z>=0 && p.z<=sim_.config().depth)
                sim_.addImpulse({p.x,p.z,radius_,strength_});
        };
    }
    void onSwapChainCreated() override {
        if(failed_) return;
        const auto extent=getExtent();
        for(auto& frame:targets_) for(auto& target:frame)
            if(!target.create(getContext(),extent.width,extent.height,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_D32_SFLOAT)) failed_=true;
        if(failed_) return;
        const auto vert=spv("scene.vert");
        for(int i=0;i<3;++i) {
            VKG::PipelineConfig c; c.vertSpv=vert;
            c.fragSpv=spv(i==0?"scene.frag":i==1?"scene_reflection.frag":"scene_refraction.frag");
            c.cullMode=VK_CULL_MODE_NONE; c.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
            c.pushConstantRanges={{VK_SHADER_STAGE_FRAGMENT_BIT,0,128}};
            if(!scene_[i].create(getContext(),i==0?getRenderPass():targets_[0][i-1].getRenderPass(),c)) failed_=true;
        }
        if(!water_.create(getContext(),getRenderPass(),MAX_FRAMES_IN_FLIGHT,config_,spv("water.vert"),spv("water.frag"))) failed_=true;
    }
    void onSwapChainDestroying() override { destroyTargets(); }
    void onUpdate(uint32_t) override {
        if(scenario_ && runner_.isActive()) runner_.tick(*this,collectResponses());
        double now=glfwGetTime(); dt_=scenario_?1.0/60:std::min(now-last_,0.1); last_=now;
        auto* window=getWindow().get(); double x,y; glfwGetCursorPos(window,&x,&y);
        if(!scenario_ && !ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS) {
            azimuth_+=float(x-oldX_)*0.005f; elevation_=std::clamp(elevation_+float(y-oldY_)*0.005f,0.1f,1.4f);
        }
        oldX_=x; oldY_=y;
        glm::vec3 eye=glm::vec3(5,0,5)+distance_*glm::vec3(std::cos(elevation_)*std::sin(azimuth_),std::sin(elevation_),std::cos(elevation_)*std::cos(azimuth_));
        auto extent=getExtent();
        glm::mat4 projection=glm::perspective(glm::radians(48.f),float(extent.width)/extent.height,0.1f,100.f);
        projection[1][1]*=-1;
        camera_.viewProjection=projection*glm::lookAt(eye,glm::vec3(5,0,5),glm::vec3(0,1,0));
        camera_.inverseViewProjection=glm::inverse(camera_.viewProjection); camera_.eye=glm::vec4(eye,1);
        eye.y=-eye.y;
        camera_.reflectionViewProjection=projection*glm::lookAt(eye,glm::vec3(5,0,5),glm::vec3(0,-1,0));
    }
    void sceneDraw(VkCommandBuffer cmd,int mode) {
        struct Push { glm::mat4 inverseVP, vp; } p;
        p.vp=mode==1?camera_.reflectionViewProjection:camera_.viewProjection; p.inverseVP=glm::inverse(p.vp);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,scene_[mode].getPipeline());
        vkCmdPushConstants(cmd,scene_[mode].getLayout(),VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(p),&p);
        vkCmdDraw(cmd,3,1,0,0);
    }
    void onPreRender(VkCommandBuffer cmd,uint32_t frame) override {
        if(failed_) return;
        sim_.recordUpdate(cmd,dt_,paused_,single_); single_=false;
        for(int i=0;i<2;++i) { targets_[frame][i].beginRenderPass(cmd); sceneDraw(cmd,i+1); targets_[frame][i].endRenderPass(cmd); }
        WaterSurfaceImages images{
            {sampler_,targets_[frame][0].getColorImageView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {sampler_,targets_[frame][1].getColorImageView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {sampler_,targets_[frame][1].getDepthImageView(),VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL}};
        water_.update(getDevice(),frame,sim_.buffer(),images,camera_,material_);
    }
    void onRender(VkCommandBuffer cmd,uint32_t frame,uint32_t) override {
        if(failed_) return;
        sceneDraw(cmd,0); water_.recordDraw(cmd,frame);
    }
    void onImGui() override {
        ImGui::SetNextWindowPos({16,16},ImGuiCond_Once);
        ImGui::SetNextWindowSize({310,420},ImGuiCond_Once);
        ImGui::Begin("Water Surface");
        ImGui::TextUnformatted("Left click: ripple / Right drag: orbit");
        ImGui::Checkbox("Pause",&paused_);
        if(ImGui::Button("Single step")) single_=true;
        ImGui::SameLine(); if(ImGui::Button("Reset")) sim_.requestReset();
        ImGui::SliderFloat("Radius",&radius_,0.05f,2.f);
        ImGui::SliderFloat("Strength",&strength_,-4.f,4.f);
        ImGui::SliderFloat("Distance",&distance_,6.f,25.f);
        ImGui::Checkbox("Reflection",&material_.reflection); ImGui::Checkbox("Refraction",&material_.refraction);
        ImGui::ColorEdit3("Water color",&material_.color.x);
        ImGui::SliderFloat3("Absorption",&material_.absorption.x,0,2);
        ImGui::SliderFloat("Distortion",&material_.absorption.w,0,0.1f);
        ImGui::SliderFloat("Wave speed",&config_.waveSpeed,0,5);
        ImGui::SliderFloat("Damping",&config_.damping,0,3);
        ImGui::SliderInt("Grid",&grid_,32,512);
        ImGui::SliderFloat("Width",&config_.width,2,20);
        ImGui::SliderFloat("Depth",&config_.depth,2,20);
        if(ImGui::Button("Apply simulation settings")) {
            vkDeviceWaitIdle(getDevice());
            sim_.destroy(getDevice()); water_.destroy(getDevice()); config_.columns=config_.rows=uint32_t(grid_);
            failed_=!sim_.create(getContext(),getCommandPool(),config_,spv("water_update.comp"));
            if(!water_.create(getContext(),getRenderPass(),MAX_FRAMES_IN_FLIGHT,config_,spv("water.vert"),spv("water.frag"))) failed_=true;
        }
        ImGui::End();
    }
    void destroyTargets() {
        water_.destroy(getDevice()); for(auto& p:scene_) p.destroy(getDevice());
        for(auto& frame:targets_) for(auto& target:frame) target.destroy(getContext());
    }
    void onCleanup() override {
        destroyTargets(); sim_.destroy(getDevice());
        if(sampler_) vkDestroySampler(getDevice(),sampler_,nullptr); sampler_=VK_NULL_HANDLE;
        if(runner_.hasFailed()) std::fprintf(stderr,"%s\n",runner_.failMessage().c_str());
    }
private:
    bool verifyGpu() {
        WaterSurfaceConfig c; c.columns=23; c.rows=17; c.waveSpeed=2;
        WaterSurfaceReference reference;
        WaterSurfaceSimulator gpu;
        VKG::VulkanBuffer readback;
        bool ok=reference.create(c) && gpu.create(getContext(),getCommandPool(),c,spv("water_update.comp"));
        const VkDeviceSize bytes=reference.cells().size()*sizeof(WaterCell);
        if(ok) ok=readback.createMapped(getContext(),bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        auto compare=[&]() {
            auto cmd=getCommandPool().beginSingleTimeCommands();
            if(!cmd) return false;
            VkBufferCopy copy{0,0,bytes}; vkCmdCopyBuffer(cmd,gpu.buffer(),readback.get(),1,&copy);
            if(!getCommandPool().endSingleTimeCommands(cmd)) return false;
            if(vmaInvalidateAllocation(getContext().getAllocator(),readback.getVmaAllocation(),0,VK_WHOLE_SIZE)!=VK_SUCCESS) return false;
            const auto* cells=static_cast<const WaterCell*>(readback.getMapped());
            for(size_t i=0;i<reference.cells().size();++i) {
                if(!std::isfinite(cells[i].height) || !std::isfinite(cells[i].velocity) ||
                   std::abs(cells[i].height-reference.cells()[i].height)>1e-5f ||
                   std::abs(cells[i].velocity-reference.cells()[i].velocity)>1e-5f) return false;
            }
            return true;
        };
        if(ok) {
            for(int frame=0;frame<60 && ok;++frame) {
                if(frame==0 || frame==10) {
                    WaterImpulse impulse{frame==0?5.f:3.f,5,1.2f,1};
                    reference.addImpulse(impulse); gpu.addImpulse(impulse);
                }
                if(frame==40) { reference.reset(); gpu.requestReset(); }
                const bool pause=frame>=20 && frame<25;
                reference.advance(1.0/60,pause);
                auto cmd=getCommandPool().beginSingleTimeCommands();
                if(!cmd) { ok=false; break; }
                gpu.recordUpdate(cmd,1.0/60,pause);
                ok=getCommandPool().endSingleTimeCommands(cmd) && compare();
            }
        }
        vkDeviceWaitIdle(getDevice()); readback.destroy(getDevice()); gpu.destroy(getDevice());
        std::fprintf(stdout,"WaterSurface GPU reference comparison: %s\n",ok?"PASS":"FAIL");
        return ok;
    }
    static std::vector<uint32_t> spv(const char* name) { return VKG::loadSPVRepo(std::string("shaders/water/")+name+".spv"); }
    WaterSurfaceConfig config_;
    WaterSurfaceSimulator sim_;
    WaterSurfaceRenderer water_;
    WaterSurfaceCamera camera_;
    WaterSurfaceMaterial material_;
    VKG::VulkanOffscreen targets_[MAX_FRAMES_IN_FLIGHT][2];
    VKG::VulkanPipeline scene_[3];
    VkSampler sampler_=VK_NULL_HANDLE;
    ScenarioRunner runner_;
    std::vector<std::string> responses_;
    bool failed_=false, scenario_=false, paused_=false, single_=false;
    float radius_=0.3f,strength_=1.5f,azimuth_=0.4f,elevation_=0.7f,distance_=13;
    int grid_=256;
    double last_=0,dt_=0,oldX_=0,oldY_=0;
};
int main(int argc,char** argv) {
    WaterSurfaceApp app;
    for(int i=1;i+1<argc;++i) if(std::string(argv[i])=="--run-scenario")
        if(!app.loadScenario(argv[++i])) return 1;
    return app.run(argc,argv)?app.exitCode():1;
}
