#pragma once
#include "../benchmark/Protocol.h"
#include "../benchmark/Build.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace benchmark {
inline std::atomic<int> command{0},status{0},repetition{1};
inline int64_t now() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline int64_t unixMs() {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
inline std::string quote(const std::string& input) {
    std::ostringstream out;out<<'"';
    for(unsigned char c:input) {
        if(c=='"'||c=='\\')out<<'\\'<<char(c);
        else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;
        else out<<char(c);
    }
    out<<'"';return out.str();
}
inline std::string read(const std::string& path) {std::ifstream in(path);if(!in)throw std::runtime_error("Cannot read "+path);return {std::istreambuf_iterator<char>(in),{}};}
inline void write(const std::filesystem::path& path,const std::string& text) {std::ofstream out(path);out<<text;out.flush();if(!out)throw std::runtime_error("Cannot write "+path.string());}
inline std::string boolean(bool v){return v?"true":"false";}
inline std::string summary(const Run& r,const std::string& identity) {
    std::ostringstream out;out<<std::setprecision(12)<<"{"<<identity<<",\"presentation_fps\":null,\"application\":";
    if(r.frames.empty())out<<"null";
    else {
        std::vector<double> intervals;intervals.reserve(r.frames.size());for(auto f:r.frames)intervals.push_back(f.interval);
        auto s=calculate(std::move(intervals));
        out<<"{\"sample_count\":"<<s.sample_count<<",\"duration_ms\":"<<s.duration_ms<<",\"average_fps\":"<<s.average_fps
            <<",\"low_1_fps\":"<<s.low_1_fps<<",\"mean_ms\":"<<s.mean_ms<<",\"p50_ms\":"<<s.p50_ms
            <<",\"p95_ms\":"<<s.p95_ms<<",\"p99_ms\":"<<s.p99_ms<<",\"max_ms\":"<<s.max_ms<<"}";
    }
    out<<"}";return out.str();
}
inline void save(const Run& run,const std::string& root,const std::string& id,const std::string& identity,const std::string& metadata) {
    auto path=std::filesystem::path(root)/id;std::filesystem::create_directories(path);write(path/".pending","incomplete");
    std::ostringstream frames,metrics;frames<<std::setprecision(12)<<"frame_index,elapsed_ms,interval_ms,animation_seconds,cpu_scene_ms,cpu_effects_ms,cpu_submit_ms,cpu_wait_ms\n";
    for(size_t i=0;i<run.frames.size();++i){auto f=run.frames[i];frames<<i<<','<<f.elapsed<<','<<f.interval<<','<<f.animation<<','<<f.scene<<','<<f.effects<<','<<f.submit<<','<<f.wait<<'\n';}
    metrics<<std::setprecision(12)<<"sample_id,observed_frame,metric,value_ms\n";
    for(auto m:run.metrics)metrics<<m.sample<<','<<m.observed<<','<<m.name<<','<<m.ms<<'\n';
    write(path/"metadata.json",metadata);write(path/"summary.json",summary(run,identity));write(path/"frames.csv",frames.str());write(path/"metrics.csv",metrics.str());std::filesystem::remove(path/".pending");
}
inline std::string metric(const char* kind,const char* source,const char* scope,const char* id) {
    return "{\"unit\":\"ms\",\"kind\":"+quote(kind)+",\"source\":"+quote(source)+",\"scope\":"+quote(scope)+",\"scope_id\":"+quote(id)+"}";
}
}
