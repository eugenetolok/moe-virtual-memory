#include <algorithm>
#include "llama.h"
#include "ggml-cpu/ggml-cpu-impl.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <sys/resource.h>
extern "C" bool llama_moe_paging_direct_mul_mat_id(ggml_compute_params *, ggml_tensor *);
struct alignas(64) worker_metrics { uint64_t chunks=0,fallbacks=0,conversion_bytes=0; };
static std::array<worker_metrics,16> metrics;
static bool shadow = false;
static bool forced_inverse=false;
static uint64_t decode_index=0;
static void need(bool ok,const char *message);
static FILE *reference=nullptr;
static bool compare_reference=false;
static uint64_t exact_tensor_bytes=0,exact_tensor_events=0;
static void reference_check(ggml_compute_params *p,ggml_tensor *op,int layer,int part) {
    if(!reference) return;
    ggml_barrier(p->threadpool);
    if(p->ith) return;
    const uint64_t descriptor[4]={decode_index,uint64_t(layer),uint64_t(part),uint64_t(op->ne[0])};
    const size_t bytes=size_t(op->ne[0])*8*4;
    need(op->ne[1]==8 && op->ne[2]==1 && op->nb[1]==size_t(op->ne[0])*4 && bytes<=65536,
         "reference tensor stride");
    if(compare_reference) {
        uint64_t expected[4];std::array<uint8_t,65536> buffer;
        need(std::fread(expected,1,sizeof(expected),reference)==sizeof(expected) &&
             !std::memcmp(expected,descriptor,sizeof(expected)),"reference event order");
        need(std::fread(buffer.data(),1,bytes,reference)==bytes,"reference payload missing");
        if(std::memcmp(buffer.data(),op->data,bytes)) {
            const auto *actual=static_cast<const uint8_t *>(op->data);
            size_t offset=0;while(offset<bytes && buffer[offset]==actual[offset])++offset;
            std::cerr<<"first mismatch step="<<decode_index<<" layer="<<layer<<" part="<<part
                     <<" byte="<<offset<<" expected="<<unsigned(buffer[offset])<<" actual="<<unsigned(actual[offset])<<'\n';
            std::exit(2);
        }
    } else {
        need(std::fwrite(descriptor,1,sizeof(descriptor),reference)==sizeof(descriptor) &&
             std::fwrite(op->data,1,bytes,reference)==bytes,"reference write");
    }
    exact_tensor_bytes+=bytes;++exact_tensor_events;
}
static uint64_t observed_ops = 0, rejected_ops = 0;

struct route_record { uint64_t step; int layer; std::array<int32_t,8> ids; };
static void need(bool ok, const char *message);
static std::vector<route_record> routes;
static std::array<route_record,40> current_routes;
static std::array<bool,40> route_valid{};
static bool route_callback(ggml_tensor *t, bool ask, void *) {
    const char *prefix="ffn_moe_topk-"; int layer=-1, suffix=0;
    const size_t n=std::strlen(prefix);
    const bool wanted=!std::strncmp(t->name,prefix,n) && std::sscanf(t->name+n,"%d%n",&layer,&suffix)==1 &&
                      !t->name[n+suffix];
    if (ask) return wanted;
    if (!wanted) return true;
    need(layer>=0 && layer<40 && t->type==GGML_TYPE_I32 && t->ne[0]==8 && t->ne[1]==1,
         "unexpected native topk callback");
    route_record r{decode_index,layer,{}};
    for(int i=0;i<8;++i) {
        std::memcpy(&r.ids[i],static_cast<const char *>(t->data)+i*t->nb[0],4);
        need(r.ids[i]>=0 && r.ids[i]<256,"native route callback expert out of range");
    }
    current_routes[layer]=r; route_valid[layer]=true;
    return true;
}
static void need(bool ok, const char *message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(2); }
}
static bool hook(ggml_compute_params *p, ggml_tensor *op) {
    if(op->op!=GGML_OP_MUL_MAT_ID) return false;
    int part=-1,layer=-1,suffix=0;
    const char *names[3]={"ffn_moe_gate-","ffn_moe_up-","ffn_moe_down-"};
    for(int i=0;i<3;++i) {
        size_t n=std::strlen(names[i]);
        if(!std::strncmp(op->name,names[i],n) && std::sscanf(op->name+n,"%d%n",&layer,&suffix)==1 &&
           !op->name[n+suffix]) part=i;
    }
    need(part>=0 && layer>=0 && layer<40,"unsupported routed node");
    need(route_valid[layer] && current_routes[layer].step==decode_index,"missing authoritative route");
    need(llama_moe_paging_direct_mul_mat_id(p,op),"production direct_sparse rejected node");
    if(!p->ith) {++observed_ops;if(!part) routes.push_back(current_routes[layer]);}
    reference_check(p,op,layer,part);
    return true;
}
int main(int argc,char **argv) {
    need(argc==6,"usage: integrated-tile-model model prompt-file generated-count shadow0or1 output.json");
    const int n=std::stoi(argv[3]); shadow=std::stoi(argv[4])!=0;
    need(n>0 && n<=600,"bounded generated count");
    forced_inverse=shadow && std::getenv("RESEARCH_FORCE_INVERSE");
    const char *write_ref=std::getenv("RESEARCH_REFERENCE_WRITE"),*read_ref=std::getenv("RESEARCH_REFERENCE_READ");
    need(!(write_ref && read_ref),"one reference mode only");
    if(write_ref || read_ref) {compare_reference=read_ref;reference=std::fopen(read_ref?read_ref:write_ref,read_ref?"rb":"wbx");need(reference,"reference stream open");}
    std::ifstream f(argv[2]); need(bool(f),"prompt file missing");
    const std::string prompt((std::istreambuf_iterator<char>(f)),{});
    ggml_backend_load_all();
    ggml_backend_dev_t devices[]={nullptr};
    auto mp=llama_model_default_params(); mp.devices=devices; mp.n_gpu_layers=0; mp.load_mode=LLAMA_LOAD_MODE_MMAP;
    const llama_model_tensor_buft_override overrides[]={{"_exps",ggml_backend_cpu_buffer_type()},{nullptr,nullptr}};
    mp.tensor_buft_overrides=overrides;
    auto *model=llama_model_load_from_file(argv[1],mp); need(model,"load model");
    auto cp=llama_context_default_params(); cp.n_ctx=1024; cp.n_batch=2; cp.n_ubatch=1;
    cp.offload_kqv=false; cp.op_offload=false;
    cp.cb_eval=route_callback; cp.cb_eval_user_data=nullptr;
    const char *thread_env=std::getenv("RESEARCH_THREADS");
    const int threads=thread_env?std::stoi(thread_env):4;need(threads>=1 && threads<=16,"threads range");
    cp.n_threads=threads; cp.n_threads_batch=threads; cp.no_perf=false;
    auto *ctx=llama_init_from_model(model,cp); need(ctx,"context");
    ggml_cpu_set_moe_mul_mat_id_hook(hook);
    const auto *vocab=llama_model_get_vocab(model);
    int count=-llama_tokenize(vocab,prompt.data(),prompt.size(),nullptr,0,true,true);
    need(count>0 && count+n<1024,"prompt size");
    std::vector<llama_token> input(count), generated;
    need(llama_tokenize(vocab,prompt.data(),prompt.size(),input.data(),count,true,true)==count,"tokenize");
    auto *sampler=llama_sampler_init_greedy();
    // Optional measurement-only settle, after model admission and before prefill.
    // Default0 keeps the normal runtime unchanged. No model work runs during this wait.
    const char * settle_env=std::getenv("RESEARCH_SETTLE_SECONDS");
    const int settle=settle_env?std::stoi(settle_env):0;need(settle>=0 && settle<=60,"settle bounds");
    if(settle) std::this_thread::sleep_for(std::chrono::seconds(settle));
    const auto prefill0=std::chrono::steady_clock::now();
    for (auto token:input) {
        need(!llama_decode(ctx,llama_batch_get_one(&token,1)),"prompt decode"); ++decode_index;
    }
    const double prefill_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-prefill0).count();
    auto cpu_ms=[](){struct rusage r{};need(!getrusage(RUSAGE_SELF,&r),"CPU resource");return 1000.0*(r.ru_utime.tv_sec+r.ru_stime.tv_sec)+.001*(r.ru_utime.tv_usec+r.ru_stime.tv_usec);};
    const char *phase_path=std::getenv("RESEARCH_PHASE_FILE");
    if(phase_path){std::ofstream phase(phase_path);phase << "generation";}
    const double cpu0=cpu_ms();
    const auto t0=std::chrono::steady_clock::now();
    for (int i=0;i<n;++i) {
        auto token=llama_sampler_sample(sampler,ctx,-1); generated.push_back(token);
        // Include EOS as an ordinary token for this bounded observation workload.
        need(!llama_decode(ctx,llama_batch_get_one(&token,1)),"generation decode"); ++decode_index;
    }
    const double wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
    const double generation_cpu_ms=cpu_ms()-cpu0;
    if(phase_path){std::ofstream phase(phase_path);phase << "complete";}
    need(routes.size()==decode_index*40 && !rejected_ops,"route coverage incomplete");
    if (shadow) need(observed_ops==decode_index*120,"Q8 operation coverage incomplete");
    if(reference) {
        need(exact_tensor_events==decode_index*120,"reference event coverage");
        if(compare_reference) need(std::fgetc(reference)==EOF,"trailing reference bytes");
        need(!std::fclose(reference),"reference stream close");reference=nullptr;
    }
    std::ofstream out(argv[5]); need(bool(out),"output");
    out << "{\"shadow\":" << (shadow?"true":"false") << ",\"generated_tokens\":[";
    for(size_t i=0;i<generated.size();++i) out << (i?",":"") << generated[i];
    out << "],\"prompt_tokens\":" << count << ",\"decode_calls\":" << decode_index
        << ",\"observed_ops\":" << observed_ops
        << ",\"generation_cpu_ms\":" << generation_cpu_ms << ",\"prefill_wall_ms\":" << prefill_ms
        << ",\"generation_loop_wall_ms\":" << wall << ",\"routes\":[";
    for(size_t i=0;i<routes.size();++i) {
        const auto &r=routes[i]; out << (i?",":"") << "[" << r.step << ',' << r.layer;
        for(auto id:r.ids) out << ',' << id; out << ']';
    }
    uint64_t chunks=0,fallbacks=0,converted=0;
    for(const auto &m:metrics){chunks+=m.chunks;fallbacks+=m.fallbacks;converted+=m.conversion_bytes;}
    out << "],\"threads\":" << threads << ",\"tiled_chunks\":" << chunks
        << ",\"inverse_fallback_chunks\":" << fallbacks << ",\"canonical_conversion_bytes\":" << converted
        << ",\"online_pack_bytes\":0,\"row_scratch_bytes_per_worker\":1152,\"worker_counter_bytes\":" << sizeof(metrics)
        << ",\"forced_inverse_test\":" << (forced_inverse?"true":"false") << ",\"exact_tensor_bytes\":" << exact_tensor_bytes << ",\"exact_tensor_events\":" << exact_tensor_events
        << ",\"candidate_drives_outputs\":" << (shadow?"true":"false") << "}" << '\n';
    llama_sampler_free(sampler); llama_free(ctx); llama_model_free(model); llama_backend_free();
}
