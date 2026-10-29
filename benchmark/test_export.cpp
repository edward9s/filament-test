#include "../src/Benchmark.h"
#include <cassert>
int main(int argc,char** argv) {
    assert(argc==3);
    benchmark::Run run;run.start();run.sample(100,true,{});run.sample(30'000'000'100ll,true,{});
    for(int i=1;i<=600;++i)run.sample(30'000'000'100ll+i*100'000'000ll,true,{0,0,30.+i*.1,.2,.3,.4,.5});
    const std::string identity="\"schema_version\":\"1.0.0\",\"run_id\":\"Filament1\",\"preset_id\":\"standard\",\"valid\":true,\"invalid_reason\":null";
    benchmark::save(run,argv[2],"Filament1",identity,benchmark::read(argv[1]));
    benchmark::write(std::filesystem::path(argv[2])/"quote.json",benchmark::quote("測試\n\"\\\x01"));
}
