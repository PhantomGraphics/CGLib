#pragma once
// Thread-safe command in / response out queues shared by the viewers' command dispatchers.
//
// Any thread (scenario runner, command window, GUI) submits commands; the render thread drains
// them once per frame from processQueue() and pushes one response per command. The responses
// are collected from another thread by collectResponses(). Every operation locks, so the
// dispatcher needs no mutex of its own for these queues.
//
//   dispatch(cmd):          queue_.submit(cmd)
//   collectResponses():     return queue_.collectResponses()
//   processQueue():         auto local = queue_.takeAll();      // or takeOne(cmd)
//                           for each cmd: queue_.respond(route(cmd))
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

class CommandQueue {
public:
    void submit(std::string command) {
        std::lock_guard<std::mutex> lock(mutex_);
        input_.push(std::move(command));
    }

    // Moves every pending command out, oldest first.
    std::queue<std::string> takeAll() {
        std::queue<std::string> local;
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(local, input_);
        return local;
    }

    // Moves the oldest pending command into `out`; false if there is none.
    bool takeOne(std::string& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (input_.empty()) return false;
        out = std::move(input_.front());
        input_.pop();
        return true;
    }

    // Puts commands back at the END of the input queue (behind anything submitted since
    // takeAll()), draining `rest`.
    void requeue(std::queue<std::string>& rest) {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!rest.empty()) {
            input_.push(std::move(rest.front()));
            rest.pop();
        }
    }

    void respond(std::string response) {
        std::lock_guard<std::mutex> lock(mutex_);
        output_.push(std::move(response));
    }

    // Moves every pending response out, oldest first.
    std::vector<std::string> collectResponses() {
        std::vector<std::string> out;
        std::lock_guard<std::mutex> lock(mutex_);
        while (!output_.empty()) {
            out.push_back(std::move(output_.front()));
            output_.pop();
        }
        return out;
    }

private:
    std::mutex              mutex_;
    std::queue<std::string> input_;
    std::queue<std::string> output_;
};
