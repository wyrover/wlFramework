// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#define BASE_EXPORT __declspec(dllexport)

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define MOVE_ONLY_TYPE_FOR_CPP_03(type, rvalue_type) \
 private: \
struct rvalue_type { \
  explicit rvalue_type(type* object) : object(object) {} \
  type* object; \
}; \
  type(type&); \
  void operator=(type&); \
 public: \
 operator rvalue_type() { return rvalue_type(this); } \
 type Pass() { return type(rvalue_type(this)); } \
 private:

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName();                                    \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

// TODO: reference additional headers your program requires here
