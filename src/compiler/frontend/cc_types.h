/* cc_types.h — C type system */
#ifndef M68K_CC_TYPES_H
#define M68K_CC_TYPES_H

#include "common/types.h"

/*
 * MC68000 type sizes:
 *   char    = 1 byte
 *   short   = 2 bytes
 *   int     = 2 bytes  (16-bit target)  — or 4 bytes (configurable)
 *   long    = 4 bytes
 *   pointer = 4 bytes  (32-bit, only 24 bits used)
 *   float   = 4 bytes  (software emulated)
 *   double  = 8 bytes  (software emulated)
 */

typedef enum {
    CC_TYPE_VOID,
    CC_TYPE_CHAR,
    CC_TYPE_SHORT,
    CC_TYPE_INT,
    CC_TYPE_LONG,
    CC_TYPE_FLOAT,
    CC_TYPE_DOUBLE,
    CC_TYPE_POINTER,
    CC_TYPE_ARRAY,
    CC_TYPE_STRUCT,
    CC_TYPE_UNION,
    CC_TYPE_ENUM,
    CC_TYPE_FUNCTION
} CcTypeKind;

#define CC_TYPE_MAX_MEMBERS 64
#define CC_TYPE_MAX_PARAMS  32
#define CC_TYPE_MAX_NAME    128

typedef struct CcType {
    CcTypeKind    kind;
    bool          isUnsigned;
    bool          isConst;
    bool          isVolatile;
    bool          isStatic;
    bool          isExtern;

    /* Size info */
    u32           size;        /* in bytes */
    u32           alignment;

    /* Pointer / Array */
    struct CcType *base;       /* pointed-to / element type */
    u32           arraySize;   /* element count for arrays  */

    /* Struct / Union */
    char          name[CC_TYPE_MAX_NAME];
    struct {
        char           name[CC_TYPE_MAX_NAME];
        struct CcType *type;
        u32            offset;
    } members[CC_TYPE_MAX_MEMBERS];
    int           memberCount;

    /* Function */
    struct CcType *returnType;
    struct CcType *params[CC_TYPE_MAX_PARAMS];
    int            paramCount;
    bool           isVariadic;
} CcType;

/* Constructors */
CcType *ccTypeCreate(CcTypeKind kind);
CcType *ccTypePointerTo(CcType *base);
CcType *ccTypeArrayOf(CcType *base, u32 size);
void    ccTypeDestroy(CcType *t);

/* Primitives (singletons) */
CcType *ccTypeVoid(void);
CcType *ccTypeChar(void);
CcType *ccTypeShort(void);
CcType *ccTypeInt(void);
CcType *ccTypeLong(void);

/* Size calculation */
u32 ccTypeGetSize(CcType *t);
u32 ccTypeGetAlignment(CcType *t);

/* Compatibility check */
bool ccTypeIsCompatible(CcType *a, CcType *b);
bool ccTypeIsIntegral(CcType *t);
bool ccTypeIsArithmetic(CcType *t);
bool ccTypeIsPointer(CcType *t);

#endif /* M68K_CC_TYPES_H */
