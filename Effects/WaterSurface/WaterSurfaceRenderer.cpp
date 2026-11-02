#include "WaterSurfaceRenderer.h"
#include "CGLib/VulkanGraphics/VulkanContext.h"
namespace Phantom::Effects::WaterSurface {
bool WaterSurfaceRenderer::create(const VKG::VulkanContext& ctx,VkRenderPass pass,uint32_t frames,
                                  const WaterSurfaceConfig& config,const std::vector<uint32_t>& vert,
                                  const std::vector<uint32_t>& frag) {
    if(!sets_.empty() || frames==0) return false;
    if(!isValidWaterSurfaceConfig(config)) return false;
    config_=config;
    const auto d=ctx.getDevice();
    std::vector<VkDescriptorSetLayoutBinding> bindings{
        {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
    for(uint32_t i=2;i<5;++i) bindings.push_back({i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
    if(!layout_.create(d,bindings) || !pool_.create(d,{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,frames},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,frames},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,frames*3}},frames)) { destroy(d); return false; }
    sets_=pool_.allocateSets(d,std::vector<VkDescriptorSetLayout>(frames,layout_.get()));
    if(sets_.size()!=frames) { destroy(d); return false; }
    uniforms_.resize(frames);
    for(auto& u:uniforms_) if(!u.createMapped(ctx,sizeof(Uniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) { destroy(d); return false; }
    VKG::PipelineConfig c; c.vertSpv=vert; c.fragSpv=frag; c.descriptorSetLayout=layout_.get(); c.cullMode=VK_CULL_MODE_NONE;
    if(!pipeline_.create(ctx,pass,c)) { destroy(d); return false; }
    return true;
}
void WaterSurfaceRenderer::destroy(VkDevice d) {
    pipeline_.destroy(d); pool_.destroy(d); layout_.destroy(d);
    for(auto& u:uniforms_) u.destroy(d);
    uniforms_.clear(); sets_.clear();
}
void WaterSurfaceRenderer::update(VkDevice d,uint32_t f,VkBuffer state,const WaterSurfaceImages& images,
                                  const WaterSurfaceCamera& camera,const WaterSurfaceMaterial& material,float elevation) {
    if(f>=sets_.size()) return;
    Uniform u{camera.viewProjection,camera.inverseViewProjection,camera.reflectionViewProjection,camera.eye,
        {config_.width,config_.depth,elevation,0},material.color,material.absorption,
        {material.reflection?1.f:0.f,material.refraction?1.f:0.f,0,0},{config_.columns,config_.rows,0,0}};
    uniforms_[f].write(&u,sizeof(u));
    VkDescriptorBufferInfo buffers[2]{{state,0,VK_WHOLE_SIZE},{uniforms_[f].get(),0,sizeof(u)}};
    const VkDescriptorImageInfo textures[]{images.reflection,images.color,images.depth};
    VkWriteDescriptorSet writes[5]{};
    for(uint32_t b=0;b<5;++b) {
        auto& w=writes[b]; w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w.dstSet=sets_[f]; w.dstBinding=b; w.descriptorCount=1;
        w.descriptorType=b==0?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:b==1?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        if(b<2) w.pBufferInfo=&buffers[b]; else w.pImageInfo=&textures[b-2];
    }
    vkUpdateDescriptorSets(d,5,writes,0,nullptr);
}
void WaterSurfaceRenderer::recordDraw(VkCommandBuffer cmd,uint32_t f) {
    if(f>=sets_.size()) return;
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_.getPipeline());
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_.getLayout(),0,1,&sets_[f],0,nullptr);
    vkCmdDraw(cmd,(config_.columns-1)*(config_.rows-1)*6,1,0,0);
}
}
