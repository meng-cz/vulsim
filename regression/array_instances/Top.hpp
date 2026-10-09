#pragma once
#include "header.hpp"
INTERFACE() {
    SERVICE(submit1, array=8, ARG(uint32_t) data);
    SERVICE(submit2, array=4, ARG(uint32_t) data);
    SERVICE(capture1, array=8, ARG(uint32_t) data);
    SERVICE(capture2, array=4, ARG(uint32_t) data);
}
CHILD_INSTANCE(PE, arr1, dims=[8], COORD(0)=ROW);
CHILD_INSTANCE(PE, arr2, dims=[4]);
CONNECT_S_CS(submit1, arr1[*], submit);
CONNECT_S_CS(submit2, arr2[*], submit);
CONNECT_CR_S(arr1[*], stream, capture1);
CONNECT_CR_S(arr2[*], stream, capture2);
USE_CHILD_QUERY(arr1[*], result, first, uint32_t);
USE_CHILD_QUERY(arr2[*], result, second, uint32_t);
REGISTER(last, uint32_t, dims=[8]);
SERVICE(capture1, array=8, ARG(uint32_t) data) { last.setnext(IDX, data); }
SERVICE(capture2, array=4, ARG(uint32_t) data) { }
QUERY(snapshot, ArraySnapshot) {
    ArraySnapshot s{};
    s.first[0] = first<0>(); s.first[1] = first<1>();
    s.first[2] = first<2>(); s.first[3] = first<3>();
    s.first[4] = first<4>(); s.first[5] = first<5>();
    s.first[6] = first<6>(); s.first[7] = first<7>();
    s.second[0] = second<0>(); s.second[1] = second<1>();
    s.second[2] = second<2>(); s.second[3] = second<3>();
    for (uint32_t i = 0; i < 8; ++i) s.streamed[i] = last[i];
    return s;
}
