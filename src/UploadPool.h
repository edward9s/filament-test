#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>

namespace forest {
// CPU upload storage. Only Filament's BufferDescriptor callbacks release submitted data.
// Each lease holds a reference too, so even synchronous callbacks cannot recycle it early.
template<typename T>
class UploadPool {
    struct State;
    struct Node {
        T data;
        std::atomic<unsigned> references{0};
        std::shared_ptr<State> owner;
    };
    struct State {
        std::mutex mutex;
        std::array<std::unique_ptr<Node>,4> ready;
        size_t count=0;
    };
    std::shared_ptr<State> state_=std::make_shared<State>();

    static void release(Node* node) noexcept {
        if(node->references.fetch_sub(1,std::memory_order_acq_rel)!=1)return;
        // Pending callbacks keep State alive even after Effects / UploadPool is destroyed.
        auto state=std::move(node->owner);
        std::unique_ptr<Node> available(node);
        std::lock_guard lock(state->mutex);
        if(state->count<state->ready.size())state->ready[state->count++]=std::move(available);
    }
public:
    class Lease {
    public:
        ~Lease() {release(node_);}
        Lease(const Lease&)=delete;
        Lease& operator=(const Lease&)=delete;
        T& data() {return node_->data;}
        void* retainForUpload() {
            node_->references.fetch_add(1,std::memory_order_relaxed);
            return node_;
        }
    private:
        friend class UploadPool;
        explicit Lease(Node* node):node_(node) {}
        Node* node_;
    };
    Lease acquire() {
        std::unique_ptr<Node> node;
        {
            std::lock_guard lock(state_->mutex);
            if(state_->count)node=std::move(state_->ready[--state_->count]);
        }
        if(!node)node=std::make_unique<Node>();
        node->owner=state_;
        node->references.store(1,std::memory_order_relaxed);
        return Lease(node.release());
    }
    static void uploadComplete(void*,size_t,void* user) noexcept {
        release(static_cast<Node*>(user));
    }
};
}
