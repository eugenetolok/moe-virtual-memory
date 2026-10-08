// Focused differential using the production paging implementation, kernels and hook.
#include "../llama.cpp/src/llama-moe-paging.cpp"
#include "traits.h"
#include <random>

static ggml_tensor * ordered_sum(ggml_context * ctx, ggml_tensor * down, ggml_tensor * weights) {
    auto * weighted = ggml_mul(ctx, down, weights);
    auto * out = ggml_view_2d(ctx, weighted, down->ne[0], 1, weighted->nb[2], 0);
    for (int j = 1; j < 8; ++j) {
        out = ggml_add(ctx, out, ggml_view_2d(ctx, weighted, down->ne[0], 1, weighted->nb[2], j * weighted->nb[1]));
    }
    return out;
}

int main() {
    ggml_cpu_set_moe_mul_mat_id_hook(llama_moe_paging_direct_mul_mat_id);
    std::mt19937 rng(18018);
    uint64_t cases = 0, masks_seen[256] = {}, op_compares = 0;
    for (auto geometry : {std::make_pair(256, 256), std::make_pair(2048, 512)}) {
        const int emb = geometry.first, ff = geometry.second, slots = 24;
        ggml_init_params ip{256u * 1024 * 1024, nullptr, false};
        auto * ctx = ggml_init(ip); GGML_ASSERT(ctx);
        g.enabled = true; g.compute_mode = "direct_sparse";
        g.layers.clear(); g.layers.resize(1); auto & l = g.layers[0]; l.il = 0;
        l.slot_gate = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_K, emb, ff, slots);
        l.slot_up = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_K, emb, ff, slots);
        l.slot_down = ggml_new_tensor_3d(ctx, GGML_TYPE_Q6_K, ff, emb, slots);
        for (auto * w : {l.slot_gate, l.slot_up, l.slot_down}) {
            std::vector<float> src(ggml_nelements(w));
            for (auto & v : src) { v = ((int)(rng() % 2001) - 1000) * 0.0002f; }
            GGML_ASSERT(ggml_quantize_chunk(w->type, src.data(), w->data, 0, w->ne[1] * slots, w->ne[0], nullptr) == ggml_nbytes(w));
        }
        std::array<std::vector<uint8_t>,3> raw, packed;
        int component=0;
        for (auto * w : {l.slot_gate,l.slot_up,l.slot_down}) {
            const auto * bytes=(const uint8_t *)w->data; raw[component].assign(bytes,bytes+ggml_nbytes(w));
            packed[component].resize(raw[component].size());
            const size_t expert_bytes=ggml_nbytes(w)/slots;
            for(int slot=0;slot<slots;++slot) {moe_tiled_pack(raw[component].data()+slot*expert_bytes,
                packed[component].data()+slot*expert_bytes,w->ne[1],w->ne[0]/256,16,component==2);}
            ++component;
        }
        auto * input = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, emb, 1, 1); l.early_in = input;
        auto * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, 8, 1);
        auto * weights = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, 1, 8, 1);
        for (int j = 0; j < 8; ++j) { ((float *)weights->data)[j] = (j + 1) / 37.0f; }
        auto * gate = ggml_mul_mat_id(ctx, l.slot_gate, input, ids);
        auto * up = ggml_mul_mat_id(ctx, l.slot_up, input, ids);
        auto * canonical_act = ggml_swiglu_split(ctx, gate, up);
        auto * canonical_down = ggml_mul_mat_id(ctx, l.slot_down, canonical_act, ids);
        auto * canonical_out = ordered_sum(ctx, canonical_down, weights);
        auto * proto_act = ggml_map_custom2(ctx, gate, up, llama_moe_paging_swiglu, GGML_N_TASKS_MAX, &l);
        auto * proto_down = ggml_mul_mat_id(ctx, l.slot_down, proto_act, ids);
        auto * proto_out = ordered_sum(ctx, proto_down, weights);
        auto * cg = ggml_new_graph_custom(ctx, 256, false); ggml_build_forward_expand(cg, canonical_out);
        auto * pg = ggml_new_graph_custom(ctx, 256, false); ggml_build_forward_expand(pg, proto_out);
        auto tp = ggml_threadpool_params_default(4); auto * pool = ggml_threadpool_new(&tp);
        auto cp = ggml_graph_plan(cg, 4, pool), pp = ggml_graph_plan(pg, 4, pool);
        std::vector<uint8_t> cw(cp.work_size), pw(pp.work_size); cp.work_data = cw.data(); pp.work_data = pw.data();
        for (int perm = 0; perm < 4; ++perm) {
            int selected[8];
            for (int j = 0; j < 8; ++j) { selected[j] = perm == 0 ? j : perm == 1 ? 23 - j : (j * 7 + perm) % slots; }
            std::copy(selected, selected + 8, (int *)ids->data);
            for (int input_case = 0; input_case < 3; ++input_case) {
                for (int r = 0; r < emb; ++r) {
                    ((float *)input->data)[r] = input_case == 0 ? ((int)(rng()%2001)-1000)/997.0f :
                        input_case == 1 ? 0.0f : (r % 2 ? 12.0f : -12.0f);
                }
                g.tile16=false;component=0;
                for(auto * w:{l.slot_gate,l.slot_up,l.slot_down}) {memcpy(w->data,raw[component].data(),raw[component].size());++component;}
                g.overlap_enabled = false; std::fill(l.early_ready, l.early_ready + 8, false);
                GGML_ASSERT(ggml_graph_compute(cg, &cp) == GGML_STATUS_SUCCESS);
                std::vector<std::vector<uint8_t>> reference;
                for (auto * t : {gate, up, canonical_act, canonical_down, canonical_out}) {
                    auto * bytes = (uint8_t *) t->data;
                    reference.emplace_back(bytes, bytes + ggml_nbytes(t));
                }
                g.tile16=true;component=0;
                for(auto * w:{l.slot_gate,l.slot_up,l.slot_down}) {memcpy(w->data,packed[component].data(),packed[component].size());++component;}
                for (int threads : {1, 4}) {
                    resident_pool_stop(); g.ov_threads = threads;
                    for (int mask = 0; mask < 256; ++mask) {
                        // Full masks on small geometry; all n_ready and adversarial masks at real dimensions.
                        if (emb == 2048 && !(mask == 0 || mask == 255 || mask == 85 || mask == 170 || ((mask + 1) & mask) == 0)) { continue; }
                        bool ready[8], loading[8] = {}; int hit[8], pf[8];
                        for (int j = 0; j < 8; ++j) {
                            const bool selected_ready=(mask>>j)&1;
                            ready[j]=selected_ready && !(j&1); hit[j]=selected[j];
                            pf[j]=(j&1)?selected[j]:-1; loading[j]=(j&1)&&!selected_ready;
                        }
                        g.overlap_enabled = true; g.overlap_verify = false;
                        early_compute_ready(l, 8, ready, hit, pf, loading);
                        // Poison all graph intermediates: no skipped column may read old data.
                        for (auto * t : {gate, up, proto_act, proto_down, proto_out}) { memset(t->data, 0xff, ggml_nbytes(t)); }
                        GGML_ASSERT(ggml_graph_compute(pg, &pp) == GGML_STATUS_SUCCESS);
                        int op = 0;
                        for (auto * t : {gate, up, proto_act, proto_down, proto_out}) {
                            if (memcmp(t->data, reference[op].data(), ggml_nbytes(t))) {
                                fprintf(stderr, "D1 FAIL emb=%d ff=%d perm=%d input=%d threads=%d mask=%d op=%d\n", emb, ff, perm, input_case, threads, mask, op);
                                resident_pool_stop(); return 1;
                            }
                            ++op; ++op_compares;
                        }
                        GGML_ASSERT(memcmp(ids->data, selected, sizeof(selected)) == 0);
                        ++cases; ++masks_seen[mask];
                    }
                }
            }
        }
        resident_pool_stop(); ggml_threadpool_free(pool); g.layers.clear(); ggml_free(ctx);
    }
    for (int m = 0; m < 256; ++m) { GGML_ASSERT(masks_seen[m] > 0); }
    // Independent event-intersection boundary checks, including work beyond I/O completion.
    llama_moe_paging_layer l; l.early_ready[0] = l.early_ready[1] = true;
    l.early_start[0] = 10; l.early_end[0] = 15; l.early_start[1] = 12; l.early_end[1] = 18;
    GGML_ASSERT(early_interval_overlap(l, 11, 16) == 5);
    GGML_ASSERT(early_interval_overlap(l, 20, 21) == 0);
    printf("TILED OVERLAP D1 PASS cases=%llu op_compares=%llu masks=256 mismatches=0 ready_n=0..8 threads=1,4 geometries=256x256,2048x512 poison=yes canonical_final_order=yes\n", (unsigned long long)cases, (unsigned long long)op_compares);
    return 0;
}
