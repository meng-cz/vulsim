#pragma once

#include <defhelper.hpp>

#include "header.hpp"

// A deliberately software-shaped state machine for backend experiments.  Each
// state applies a different pseudo-random-number-generator style transform to
// the current data register and feeds the boolean register back into that
// transform.  Register writes remain local to the corresponding if branch.
INTERFACE() {
}

REGISTER(state, uint8_t) {
    state = FSM_SEED;
}

REGISTER(f, bool) {
    f = false;
}

REGISTER(d, uint64_t) {
    d = 0x9e3779b97f4a7c15ULL;
}

QUERY(snapshot, FsmSnapshot) {
    FsmSnapshot result;
    result.state = state;
    result.f = f;
    result.d = d;
    return result;
}

TICK_IMPL() {
    if (state == FSM_SEED) {
        uint64_t mixed = d ^ (d >> 27);
        bool next_f = f ^ ((mixed & 1ULL) != 0ULL);
        f.setnext(next_f);
        d.setnext(mixed + (next_f ? 0x3c6ef372fe94f82aULL : 0xbb67ae8584caa73bULL));
        state.setnext(FSM_XORSHIFT_A);
    } else if (state == FSM_XORSHIFT_A) {
        uint64_t x = d ^ (d << 13);
        x = x ^ (x >> 7);
        bool next_f = f ^ (((x >> 31) & 1ULL) != 0ULL);
        f.setnext(next_f);
        d.setnext(x ^ (next_f ? 0x2545f4914f6cdd1dULL : 0x9e3779b97f4a7c15ULL));
        state.setnext(FSM_ADD_MIX);
    } else if (state == FSM_ADD_MIX) {
        uint64_t sum = d + (f ? 0xd1b54a32d192ed03ULL : 0x94d049bb133111ebULL);
        bool next_f = ((sum ^ (sum >> 17)) & 1ULL) != 0ULL;
        f.setnext(next_f);
        d.setnext(sum ^ (sum << (next_f ? 9 : 5)));
        state.setnext(FSM_ROTATE);
    } else if (state == FSM_ROTATE) {
        uint64_t rot = (d << 17) | (d >> 47);
        bool next_f = f ? ((rot & 0x8000000000000000ULL) != 0ULL)
                        : ((rot & 0x0000000000000001ULL) != 0ULL);
        f.setnext(next_f);
        d.setnext(rot + (next_f ? 0x632be59bd9b4e019ULL : 0x8cb92ba72f3d8dd7ULL));
        state.setnext(FSM_MULTIPLY);
    } else if (state == FSM_MULTIPLY) {
        uint64_t product = d * 0x5851f42d4c957f2dULL;
        bool next_f = f ^ (((product >> 40) & 1ULL) != 0ULL);
        f.setnext(next_f);
        d.setnext(product + (next_f ? 0x14057b7ef767814fULL : 0x4f1bbcdcaa4f8d31ULL));
        state.setnext(FSM_FEEDBACK);
    } else if (state == FSM_FEEDBACK) {
        uint64_t feedback = d ^ (d >> 29) ^ (d << 21);
        bool next_f = ((feedback & 0xffULL) == (f ? 0xa5ULL : 0x5aULL));
        f.setnext(next_f);
        d.setnext(feedback + (next_f ? d : ~d));
        state.setnext(FSM_SCRAMBLE);
    } else if (state == FSM_SCRAMBLE) {
        uint64_t x = d + 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        bool next_f = f ^ (((x >> 12) & 1ULL) != 0ULL);
        f.setnext(next_f);
        d.setnext((x ^ (x >> 27)) * (next_f ? 0x94d049bb133111ebULL : 0xd6e8feb86659fd93ULL));
        state.setnext(FSM_WYHASH);
    } else if (state == FSM_WYHASH) {
        uint64_t x = d ^ 0xa0761d6478bd642fULL;
        uint64_t y = (x ^ (x >> 32)) * 0xe7037ed1a0b428dbULL;
        bool next_f = ((y >> 63) != 0ULL) ^ f;
        f.setnext(next_f);
        d.setnext(y ^ (next_f ? 0x8ebc6af09c88c6e3ULL : 0x589965cc75374cc3ULL));
        state.setnext(FSM_AVALANCHE);
    } else if (state == FSM_AVALANCHE) {
        uint64_t x = d ^ (d >> 33);
        x = x * 0xff51afd7ed558ccdULL;
        bool next_f = (((x >> 19) ^ x) & 1ULL) != 0ULL;
        f.setnext(next_f);
        d.setnext((x ^ (x >> 33)) * (next_f ? 0xc4ceb9fe1a85ec53ULL : 0x27d4eb2f165667c5ULL));
        state.setnext(FSM_SELECT);
    } else if (state == FSM_SELECT) {
        uint64_t even_path = (d << 7) ^ (d >> 11) ^ 0x243f6a8885a308d3ULL;
        uint64_t odd_path = (d << 19) + (d >> 3) + 0x13198a2e03707344ULL;
        bool next_f = f ^ (((d >> 8) & 1ULL) != 0ULL);
        f.setnext(next_f);
        d.setnext(next_f ? odd_path : even_path);
        state.setnext(FSM_FINALIZE);
    } else if (state == FSM_FINALIZE) {
        uint64_t folded = d ^ (d >> 16) ^ (d >> 32) ^ (d >> 48);
        bool next_f = ((folded & 0x3ULL) == (f ? 0x1ULL : 0x2ULL));
        f.setnext(next_f);
        d.setnext((folded << 1) | (next_f ? 1ULL : 0ULL));
        state.setnext(FSM_RESEED);
    } else if (state == FSM_RESEED) {
        uint64_t seed = d + (f ? 0x6a09e667f3bcc909ULL : 0xbb67ae8584caa73bULL);
        bool next_f = f ^ (((seed >> 37) & 1ULL) != 0ULL);
        f.setnext(next_f);
        d.setnext(seed ^ (seed >> (next_f ? 23 : 41)));
        state.setnext(FSM_SEED);
    } else {
        // Recover deterministically from an invalid sparse state encoding.
        bool next_f = f ^ ((d & 1ULL) != 0ULL);
        f.setnext(next_f);
        d.setnext((d ^ 0xd6e8feb86659fd93ULL) + (next_f ? 1ULL : 0ULL));
        state.setnext(FSM_SEED);
    }
}
