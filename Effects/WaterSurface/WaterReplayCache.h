#pragma once
#include "WaterSurfaceReference.h"
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

namespace Phantom::Effects::WaterSurface {
// Only checkpoint payloads count towards the budget. Working solvers are transient.
// Shared by every water node in one session; no process-global state.
class WaterReplayCache {
public:
    explicit WaterReplayCache(size_t budget = 256ull * 1024 * 1024) : budget_(budget) {}
    size_t bytes() const { std::lock_guard<std::mutex> lock(mutex_); return bytes_; }
    void clear() { std::lock_guard<std::mutex> lock(mutex_); points_.clear(); bytes_ = 0; }
    bool evaluate(const WaterSurfaceConfig& config, const WaterImpulse& impulse, uint64_t target,
                  WaterSurfaceReference& solver, const std::atomic<bool>* cancel = nullptr,
                  std::atomic<float>* progress = nullptr) {
        if (!solver.create(config) || !solver.addImpulse(impulse)) return false;
        std::string key;
        const auto append = [&key](auto v) { key.append(reinterpret_cast<const char*>(&v), sizeof(v)); };
        append(config.columns); append(config.rows); append(config.width); append(config.depth);
        append(config.waveSpeed); append(config.damping);
        append(impulse.x); append(impulse.z); append(impulse.radius); append(impulse.velocity);
        uint64_t step = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [id, p] : points_)
                if (p.key == key && p.step <= target && p.step > step) {
                    step = p.step;
                    solver.restoreCells(p.cells);
                }
        }
        const uint64_t first = step;
        // A checkpoint at the first fixed step on/after each half-second boundary.
        uint64_t boundary = static_cast<uint64_t>(std::floor(step * solver.stepSize() / .5)) + 1;
        for (; step < target;) {
            if (cancel && cancel->load(std::memory_order_relaxed)) return false;
            solver.singleStep(); ++step;
            if (progress) progress->store(float(double(step-first)/std::max<uint64_t>(1,target-first)));
            if (step * solver.stepSize() + 1e-12 >= boundary * .5) {
                ++boundary;
                const size_t size = solver.cells().size() * sizeof(WaterCell);
                std::lock_guard<std::mutex> lock(mutex_);
                if (size <= budget_) {
                    bool exists = false;
                    for (const auto& [id, p] : points_) if (p.key == key && p.step == step) { exists = true; break; }
                    if (!exists) {
                        while (bytes_ + size > budget_ && !points_.empty()) {
                            bytes_ -= points_.begin()->second.cells.size()*sizeof(WaterCell);
                            points_.erase(points_.begin());
                        }
                        points_.emplace(++serial_, Point{key,step,solver.cells()}); bytes_ += size;
                    }
                }
            }
        }
        if (progress) progress->store(1.f);
        return true;
    }
private:
    struct Point { std::string key; uint64_t step; std::vector<WaterCell> cells; };
    mutable std::mutex mutex_;
    std::map<uint64_t, Point> points_; // oldest insertion is evicted first
    uint64_t serial_ = 0;
    size_t budget_, bytes_ = 0;
};
}
