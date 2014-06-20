#include "base/closure.h"

void Add() {}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  base::Closure closure = base::Bind(&Add);
  return 0;
}