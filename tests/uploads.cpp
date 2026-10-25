#include "UploadPool.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace {
struct Payload {
    static inline std::atomic<int> alive=0;
    std::vector<int> values;
    Payload() {++alive;}
    ~Payload() {--alive;}
};
using Pool=forest::UploadPool<Payload>;
}
int main() {
    {
        Pool pool;
        void* pending=nullptr;
        const int* submitted=nullptr;
        {
            auto first=pool.acquire();
            first.data().values.assign(100,42);submitted=first.data().values.data();
            pending=first.retainForUpload();
            // One callback can complete synchronously while generation still holds the lease.
            Pool::uploadComplete(nullptr,0,first.retainForUpload());
            auto second=pool.acquire();
            second.data().values.assign(100,7);
            assert(second.data().values.data()!=submitted);
        }
        {
            auto next=pool.acquire();
            next.data().values.assign(100,9);
            assert(next.data().values.data()!=submitted);
            for(int i=0;i<100;++i)assert(submitted[i]==42);
        }
        Pool::uploadComplete(nullptr,0,pending);
        {
            auto recycled=pool.acquire();
            assert(recycled.data().values.data()==submitted);
            assert(recycled.data().values.capacity()>=100);
        }
        // More simultaneous frames than the retained cache limit must still finish safely.
        std::vector<void*> callbacks;
        for(int i=0;i<20;++i) {
            auto lease=pool.acquire();lease.data().values.assign(100,i);
            for(int batch=0;batch<6;++batch)callbacks.push_back(lease.retainForUpload());
        }
        std::vector<std::thread> threads;
        for(int worker=0;worker<4;++worker)threads.emplace_back([&,worker] {
            for(size_t i=size_t(worker);i<callbacks.size();i+=4)Pool::uploadComplete(nullptr,0,callbacks[i]);
        });
        // Acquiring on the producer thread can overlap callback delivery.
        for(int i=0;i<1000;++i) {auto lease=pool.acquire();lease.data().values.assign(100,i);}
        for(auto& thread:threads)thread.join();
        assert(Payload::alive<=4);
    }
    assert(Payload::alive==0);
    {
        auto pool=std::make_unique<Pool>();
        void* callback=nullptr;
        {
            auto lease=pool->acquire();lease.data().values={1,2,3};
            callback=lease.retainForUpload();
        }
        pool.reset(); // Callbacks must not dereference a destroyed Effects object.
        assert(Payload::alive==1);
        Pool::uploadComplete(nullptr,0,callback);
        assert(Payload::alive==0);
    }
    std::cout<<"Upload storage: delayed, inline, concurrent, cache overflow and owner destruction passed\n";
}
