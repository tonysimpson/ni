#ifndef _COMPAT2TO3_H
#define _COMPAT2TO3_H

#include <Python.h>

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#define NiCompatStr_Type PyUnicode_Type
#define NiCompatStr_AsString PyUnicode_AsUTF8
#define NiCompatStr_AS_STRING PyUnicode_AsUTF8
#define NiCompatStr_Check PyUnicode_Check
#define NiCompatStr_CheckExact PyUnicode_CheckExact
#define NiCompatStr_CHECK_INTERNED PyUnicode_CHECK_INTERNED
#define NiCompatStr_Concat PyUnicode_Concat
#define NiCompatStr_Format PyUnicode_Format
#define NiCompatStr_FromFormat PyUnicode_FromFormat
#define NiCompatStr_FromFormatV PyUnicode_FromFormatV
#define NiCompatStr_FromString PyUnicode_FromString
#define NiCompatStr_FromStringAndSize PyUnicode_FromStringAndSize
#define NiCompatStr_GET_SIZE PyUnicode_GET_SIZE
#define NiCompatStr_InternFromString PyUnicode_InternFromString
#define NiCompatStr_InternInPlace PyUnicode_InternInPlace
#define NiCompatStr_Size PyUnicode_Size
#define NiCompatStr_Type PyUnicode_Type
#define NiCompatStr_HASH_FIELD(o) ((PyUnicodeObject*)o)->hash
#else
#include <bytesobject.h>
#define NiCompatStr_Type PyString_Type
#define NiCompatStr_AsString PyString_AsString
#define NiCompatStr_AS_STRING PyString_AS_STRING
#define NiCompatStr_Check PyString_Check
#define NiCompatStr_CheckExact PyString_CheckExact
#define NiCompatStr_CHECK_INTERNED PyString_CHECK_INTERNED
#define NiCompatStr_Concat PyString_Concat
#define NiCompatStr_Format PyString_Format
#define NiCompatStr_FromFormat PyString_FromFormat
#define NiCompatStr_FromFormatV PyString_FromFormatV
#define NiCompatStr_FromString PyString_FromString
#define NiCompatStr_FromStringAndSize PyString_FromStringAndSize
#define NiCompatStr_GET_SIZE PyString_GET_SIZE
#define NiCompatStr_InternFromString PyString_InternFromString
#define NiCompatStr_InternInPlace PyString_InternInPlace
#define NiCompatStr_Size PyString_Size
#define NiCompatStr_Type PyString_Type
#define NiCompatStr_HASH_FIELD(o) ((PyStringObject*)o)->ob_shash
#endif

#endif /* _COMPAT2TO3_H */
