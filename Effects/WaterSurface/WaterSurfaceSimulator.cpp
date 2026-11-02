#include "WaterSurfaceSimulator.h"
#include "CGLib/VulkanGraphics/VulkanContext.h"

namespace Phantom::Effects::WaterSurface {
namespace {
void barrier(VkCommandBuffer cmd) {
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask = b.srcAccessMask;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 1, &b, 0, nullptr, 0, nullptr);
}
}
bool WaterSurfaceSimulator::create(const VKG::VulkanContext& ctx, const VKG::VulkanCommandPool& commands,
                                  const WaterSurfaceConfig& c, const std::vector<uint32_t>& spv) {
    if (isValid()) return false;
    if (spv.empty() || !isValidWaterSurfaceConfig(c)) return false;
    const size_t count=size_t(c.columns)*c.rows;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(ctx.getPhysicalDevice(), &properties);
    if (count*sizeof(WaterCell)>properties.limits.maxStorageBufferRange ||
        (c.columns+15)/16>properties.limits.maxComputeWorkGroupCount[0] ||
        (c.rows+15)/16>properties.limits.maxComputeWorkGroupCount[1]) return false;
    const auto device = ctx.getDevice();
    config_ = c; step_ = waterSurfaceStepSize(c); current_ = 0; accumulator_ = 0;
    std::vector<WaterCell> initial(count);
    for (auto& state : states_)
        if (!state.create(ctx, commands, count*sizeof(WaterCell),
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          initial.data())) { destroy(device); return false; }
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (uint32_t i=0;i<2;++i) bindings.push_back({i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr});
    if (!layout_.create(device, bindings) || !pool_.create(device, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4}},2)) {
        destroy(device); return false;
    }
    sets_=pool_.allocateSets(device,{layout_.get(),layout_.get()});
    if (sets_.size()!=2) { destroy(device); return false; }
    for(uint32_t s=0;s<2;++s) for(uint32_t b=0;b<2;++b) {
        VkDescriptorBufferInfo info{states_[s^b].get(),0,VK_WHOLE_SIZE};
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet=sets_[s]; w.dstBinding=b; w.descriptorCount=1;
        w.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w.pBufferInfo=&info;
        vkUpdateDescriptorSets(device,1,&w,0,nullptr);
    }
    VKG::ComputePipelineConfig cfg;
    cfg.compSpv=spv; cfg.descriptorSetLayout=layout_.get();
    cfg.pushConstantRange={VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Push)};
    if(!pipeline_.create(ctx,cfg)) { destroy(device); return false; }
    return true;
}
void WaterSurfaceSimulator::destroy(VkDevice device) {
    pipeline_.destroy(device); pool_.destroy(device); layout_.destroy(device);
    for(auto& s:states_) s.destroy(device);
    sets_.clear(); impulses_.clear(); current_=0; accumulator_=0; reset_=false;
}
bool WaterSurfaceSimulator::addImpulse(const WaterImpulse& p) {
    if (!isValid() || impulses_.size()>=256 || !std::isfinite(p.x) || !std::isfinite(p.z) ||
        !std::isfinite(p.radius) || p.radius<=0 || !std::isfinite(p.velocity)) return false;
    impulses_.push_back(p); return true;
}
void WaterSurfaceSimulator::dispatch(VkCommandBuffer cmd,const Push& p) {
    barrier(cmd);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_.getPipeline());
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_.getLayout(),0,1,&sets_[current_],0,nullptr);
    vkCmdPushConstants(cmd,pipeline_.getLayout(),VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(p),&p);
    vkCmdDispatch(cmd,(config_.columns+15)/16,(config_.rows+15)/16,1);
    current_^=1;
}
void WaterSurfaceSimulator::recordUpdate(VkCommandBuffer cmd,double elapsed,bool paused,bool singleStep) {
    if(!isValid() || !cmd) return;
    Push p{config_.columns,config_.rows,0,0,config_.width,config_.depth,config_.waveSpeed,config_.damping,
           static_cast<float>(step_),0,0,0,0,{0,0,0}};
    if(reset_) {
        barrier(cmd);
        for(auto& s:states_) vkCmdFillBuffer(cmd,s.get(),0,VK_WHOLE_SIZE,0);
        reset_=false;
    }
    p.operation=1;
    for(const auto& i:impulses_) { p.x=i.x; p.z=i.z; p.radius=i.radius; p.velocity=i.velocity; dispatch(cmd,p); }
    impulses_.clear(); p.operation=0;
    if(singleStep) dispatch(cmd,p);
    else if(!paused && std::isfinite(elapsed) && elapsed>0) {
        accumulator_=std::min(accumulator_+elapsed,16*step_);
        for(int i=0;i<16 && accumulator_+step_*1e-9>=step_;++i) {
            dispatch(cmd,p); accumulator_=std::max(0.0,accumulator_-step_);
        }
    }
    barrier(cmd);
}
}
