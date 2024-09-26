/******************************************************************
 * File automatically generated by rebuild_wrappers_32.py (v0.0.1.1) *
 ******************************************************************/
#ifndef __wrappedfreetypeTYPES32_H_
#define __wrappedfreetypeTYPES32_H_

#ifndef LIBNAME
#error You should only #include this file inside a wrapped*.c file
#endif
#ifndef ADDED_FUNCTIONS
#define ADDED_FUNCTIONS() 
#endif

typedef int32_t (*iFp_t)(void*);
typedef void (*vFpp_t)(void*, void*);
typedef int32_t (*iFpu_t)(void*, uint32_t);
typedef uint32_t (*uFpL_t)(void*, uintptr_t);
typedef int32_t (*iFpui_t)(void*, uint32_t, int32_t);
typedef int32_t (*iFpplp_t)(void*, void*, intptr_t, void*);
typedef int32_t (*iFplluu_t)(void*, intptr_t, intptr_t, uint32_t, uint32_t);

#define SUPER() ADDED_FUNCTIONS() \
	GO(FT_Done_Face, iFp_t) \
	GO(FT_Outline_Get_CBox, vFpp_t) \
	GO(FT_Render_Glyph, iFpu_t) \
	GO(FT_Get_Char_Index, uFpL_t) \
	GO(FT_Load_Glyph, iFpui_t) \
	GO(FT_New_Face, iFpplp_t) \
	GO(FT_Set_Char_Size, iFplluu_t)

#endif // __wrappedfreetypeTYPES32_H_