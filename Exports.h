#pragma once

#ifdef GAMEFRAMEWORK_EXPORTS
#define GAMEFRAMEWORK_API __declspec(dllexport)   
#elif defined(GAMEFRAMEWORK_STATIC)
#define GAMEFRAMEWORK_API
#else
#define GAMEFRAMEWORK_API __declspec(dllimport)   
#endif
