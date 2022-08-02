#ifndef _HDL_PARSE_H
#define _HDL_PARSE_H
#include <stdint.h>

// Maximum tagname length
#define HDL_TAG_MAX_LENGTH          32
// Maximum string length of an attribute key
#define HDL_ATTR_KEY_MAX_LENGTH     32

// Types
enum HDL_Type {
    HDL_TYPE_NULL       = 0,
    HDL_TYPE_BOOL       = 1,
    HDL_TYPE_FLOAT      = 2,
    HDL_TYPE_STRING     = 3,
    HDL_TYPE_I8         = 4,
    HDL_TYPE_I16        = 5,
    HDL_TYPE_I32        = 6,
    HDL_TYPE_IMG        = 7,
    HDL_TYPE_BIND       = 8,

    // Tell's how many types have been defined
    HDL_TYPE_COUNT
};

extern uint8_t HDL_TYPE_SIZES[HDL_TYPE_COUNT];

// Attribute (key=value)
struct HDL_Attr {

    // Key 
    char key[HDL_ATTR_KEY_MAX_LENGTH];
    // Attribute value
    void *value;
    // Attribute type
    enum HDL_Type type;

    // Count (if array)
    uint8_t count;
};

// Variable/definition
struct HDL_Variable {

    // Name
    char name[HDL_ATTR_KEY_MAX_LENGTH];
    // Value
    void *value;
    // Variable type
    enum HDL_Type type;
    // Count (if array)
    uint8_t count;
    // Is constant
    uint8_t isConst;
};


// Element structure
struct HDL_Element {

    // Tag
    char tag[HDL_TAG_MAX_LENGTH];

    // Content string
    char *content;

    // Attributes
    struct HDL_Attr *attrs;
    uint8_t attrCount;
    uint8_t attrAllocCount;

    // Parent index, -1 if root
    int parent;

    // Array of children indices, can't be a pointer because document elements array might be reallocated
    uint16_t *children;
    uint16_t childCount;
    uint16_t childAllocCount;
    
};

// HDL Colorspace 
enum HDL_ColorSpace {
    HDL_COLORS_UNKNOWN,
    // MONO (black and white)
    HDL_COLORS_MONO,
    // RGB colors
    HDL_COLORS_24BIT,
    // Color pallette
    HDL_COLORS_PALLETTE
};


struct HDL_Bitmap {
    char name[32];
    uint16_t size;
    uint16_t width;
    uint16_t height;
    uint8_t sprite_width;
    uint8_t sprite_height;
    uint8_t colorMode;
    uint8_t *data;
};

// Document structure 
struct HDL_Document {

    // Elements
    struct HDL_Element *elements;
    uint16_t elementCount;
    uint16_t elementAllocCount;

    // Variables
    struct HDL_Variable *vars;
    uint16_t varCount;
    uint16_t varAllocCount;

    // Bitmaps
    struct HDL_Bitmap *bitmaps;
    uint16_t bitmapCount;
    uint16_t bitmapAllocCount;
};

int HDL_Parse (char *data, struct HDL_Document *doc);
void HDL_PrintElement (struct HDL_Document *doc, struct HDL_Element *element, int depth);
void HDL_PrintVars (struct HDL_Document *doc);


#endif