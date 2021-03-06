
#include <VEngineConfig.h>
//
// Created by Mike Smith on 2021/3/14.
//

#include <ir/api/basic_types.h>

namespace toolhub {

template struct Vector<bool, 2>;
template struct Vector<bool, 3>;
template struct Vector<bool, 4>;
template struct Vector<float, 2>;
template struct Vector<float, 3>;
template struct Vector<float, 4>;
template struct Vector<int, 2>;
template struct Vector<int, 3>;
template struct Vector<int, 4>;
template struct Vector<uint, 2>;
template struct Vector<uint, 3>;
template struct Vector<uint, 4>;

}// namespace toolhub
