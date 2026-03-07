#pragma once

#include <cmath>
#include <iostream>
#include <string>

inline int expectTrue(bool cond, const std::string &msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << std::endl;
    return 1;
  }
  return 0;
}

inline int expectNear(float a, float b, float eps, const std::string &msg) {
  if (std::fabs(a - b) > eps) {
    std::cerr << "FAIL: " << msg << " expected=" << b << " actual=" << a << std::endl;
    return 1;
  }
  return 0;
}