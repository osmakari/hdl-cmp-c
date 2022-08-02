#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hdl-parse.h"
#include <math.h>
#include "hdl-util.h"

// Number of bytes to reallocate if out of memory
#define HDL_DATABUFFER_REALLOC_SIZE     256
// Number of block pointers to reallocate if out of memory
#define HDL_BLOCKBUFFER_REALLOC_SIZE    32

// Initial size of document element buffer
#define HDL_DOC_ELEMENTS_INITIAL_SIZE   16
// Initial size of document variable buffer
#define HDL_DOC_VARS_INITIAL_SIZE       16
// Initial size of bitmap buffer
#define HDL_DOC_BITMAPS_INITIAL_SIZE    8
// Initial size of element attribute buffer
#define HDL_ELEMENT_ATTR_INITIAL_SIZE   8
// Initial size of element children
#define HDL_ELEMENT_CHILDREN_INITIAL_SIZE   8


// Buffer for data 
static char *data_buffer = NULL;
// Array of addresses of split blocks
static char **blocks = NULL;
// Count of the blocks
static uint32_t block_count = 0;
// Count allocated (for block reallocation)
static uint32_t blocks_allocated = 0;


// Type sizes
uint8_t HDL_TYPE_SIZES[HDL_TYPE_COUNT] = {
    0, /* HDL_TYPE_NULL */
    1, /* HDL_TYPE_BOOL */
    4, /* HDL_TYPE_FLOAT */
    0, /* HDL_TYPE_STRING */
    1, /* HDL_TYPE_I8 */
    2, /* HDL_TYPE_I16 */
    4, /* HDL_TYPE_I32 */
    1, /* HDL_TYPE_IMG */
    1, /* HDL_TYPE_BIND */
};

// Delimiter characters
static const char delimiters[] = {
    '#',    // Define const or img
    '\n',   // Whitespace
    '\r',   // Whitespace
    '\t',   // Whitespace
    ' ',    // Whitespace
    '<',    // Tag start
    '>',    // Tag end
    '/',    // Short tag delimiter 
    '*',    // Comment delimiter
    '=',    // Delimiter for attributes
    '[',    // Delimiter for array start
    ']',    // Delimiter for array end
    ',',    // Delimiter for array values
    '(',    // Delimiter for image parameter start
    ')',    // Delimiter for image parameter end
    '$',    // Binding value
};


/**
 * @brief Checks if a character is delimiter
 * 
 * @param c Character
 * @return int 0 on fail, 1 on success
 */
static int isDelimiter (char c) {
    for(int i = 0; i < sizeof(delimiters)/sizeof(char); i++) {
        if(c == delimiters[i])
            return 1;
    }
    return 0;
}

static int isWhitespace (char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

// Is numeric
static int isNumeric (char c) {
    return (c >= '0' && c <= '9');
}

// Is alphabetical
static int isAlphabetical (char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// Is number string
static int isNumberString (char *str) {
    uint8_t p = 0;
    uint8_t hasnum = 0;
    for(int i = 0; i < strlen(str); i++) {
        if(i != 0 && str[i] == '-') {
            // Can't have negative sign in other positions than first
            return 0;
        }
        if(isAlphabetical(str[i])) {
            // Can't be a 
            return 0;
        }
        if(str[i] == '.') {
            if(p == 0) {
                p = 1;
                continue;
            }
            else {
                // Can't have multiple points
                return 0;
            }
        }
        if(isNumeric(str[i])) {
            hasnum = 1;
        }
    }

    if(p == 1 && str[strlen(str) - 1] == '.') {
        // period at the end of the string
        return 0;
    }

    if(hasnum) {
        return 1;
    }
    else {
        return 0;
    }
}
/**
 * @brief Is integer string
 * 
 * @param str 
 * @return int 
 */
static int isIntString (char *str) {
    uint8_t hasnum = 0;
    int len = strlen(str);
    for(int i = 0; i < len; i++) {
        if(i != 0 && str[i] == '-') {
            // Can't have negative sign in other positions than first
            return 0;
        }
        if(isNumeric(str[i])) {
            hasnum = 1;
        }
        else if(str[i] == '-') {

        }
        else {
            return 0;
        }
    }
    return hasnum;
}

void _HDL_FreeBlocks () {
    if(data_buffer != NULL)
        free(data_buffer);

    if(blocks != NULL)
        free(blocks);

    block_count = 0;
    blocks_allocated = 0;
}

/**
 * @brief Parses the data in to blocks
 * 
 * @param data 
 * @return int 
 */
int _HDL_ParseDataToBlocks (char *data) {
    // Save length of data
    int len = strlen(data);
    // Data buffer index
    int dbi = 0;
    // Data buffer current allocated size
    int dalloc = 0;

    _HDL_FreeBlocks();

    // Allocate data_buffer to the length of data

    dalloc = sizeof(char) * len * 2;
    // TODO: data_buffer reallocation: Allocate double length, because reallocation does not work yet
    data_buffer = malloc(dalloc);

    if(data_buffer == NULL) {
        // Out of memory
        return 1;
    }

    blocks_allocated = HDL_BLOCKBUFFER_REALLOC_SIZE;
    blocks = malloc(sizeof(char*) * blocks_allocated);

    if(blocks == NULL) {
        // Out of memory
        return 1;
    }

    // Save first block
    blocks[block_count++] = &data_buffer[dbi];

    char lastChar = ' ';
    int blocklen = 0;
    // 0 = not in quotes 1 = in single quotes 2 = in double quotes
    uint8_t inquotes = 0;
    // Loop until termination
    for(int i = 0; i < len; i++) {

        char c = data[i];

        // Check for data buffer size 
        if(dalloc - 2 <= dbi) {
            // Data buffer smaller than current index, must be reallocated
            // TODO:    Can't reallocate because the addresses may change
            //          Add array of relative indices instead of pointers to fix this issue
            //dalloc += HDL_DATABUFFER_REALLOC_SIZE;
            //data_buffer = realloc(data_buffer, dalloc);
            printf("ERROR: Can't reallocate memory (TODO)\r\n");
            return 1;
        }

        // Check quotes
        if(lastChar != '\\') {
            switch(c) {
                case '\'':
                {
                    if(inquotes == 0) {
                        inquotes = 1;
                    }
                    else if(inquotes == 1) {
                        inquotes = 0;
                    }
                    break;
                }
                case '"':
                {
                    if(inquotes == 0) {
                        inquotes = 2;
                    }
                    else if(inquotes == 2) {
                        inquotes = 0;
                    }
                    break;
                }
                case '<':
                {
                    if(inquotes == 3) {
                        inquotes = 0;
                    }
                    break;
                }
            }
        }
        // Skip duplicate whitespace
        if(isWhitespace(c) && isWhitespace(lastChar)) {
            lastChar = c;
            continue;
        }

        // Check for delimiters
        if(isDelimiter(c) && !inquotes) {
            if(blocklen > 0) {
                // Set null terminator
                data_buffer[dbi++] = 0;

                if(blocks_allocated - 2 <= block_count) {
                    // Reallocate blocks
                    blocks_allocated += HDL_BLOCKBUFFER_REALLOC_SIZE;
                    blocks = realloc(blocks, sizeof(char*) * blocks_allocated);
                }
                blocks[block_count++] = &data_buffer[dbi];
            }

            blocklen = 0;
            
            if(!isWhitespace(c)) {
                // Add the delimiter
                data_buffer[dbi++] = c;
                // Add second null terminator
                data_buffer[dbi++] = 0;
                blocks[block_count++] = &data_buffer[dbi];
            }
        }
        else  {
            if(!isWhitespace(c) || (inquotes && c != '\n')) {
                // Add character

                // Replace possible newline/tab
                if(lastChar == '\\') {
                    if(c == 'n') {
                        // Newline
                        dbi--;
                        data_buffer[dbi++] = '\n';
                    }
                    else if(c == 't') {
                        // Tab
                        dbi--;
                        data_buffer[dbi++] = '\t';
                    }
                }
                else {
                    data_buffer[dbi++] = c;
                    blocklen++;
                }
            }
        }

        if(c == '>' && lastChar != '\\') {
            if(inquotes == 0) {
                inquotes = 3;
            }
        }

        lastChar = c;
    }
    data_buffer[dbi] = 0;
    if(blocklen <= 0) {
        block_count--;
    }

    return 0;
}

void _HDL_PrintBlocks () {
    printf("Blocks:\r\n");
    for(int i = 0; i < block_count; i++) {
        printf("\t\"%s\"\r\n", blocks[i]);
    }
}

void _HDL_InitElement (struct HDL_Element *element) {

    memset(element, 0, sizeof(struct HDL_Element));

    element->attrAllocCount = HDL_ELEMENT_ATTR_INITIAL_SIZE;
    element->attrs = malloc(sizeof(struct HDL_Attr) * element->attrAllocCount);

    element->childAllocCount = HDL_ELEMENT_CHILDREN_INITIAL_SIZE;
    element->children = malloc(sizeof(uint16_t) * element->childAllocCount);

}

int _HDL_ParseValue (struct HDL_Document *doc, int *blockIndex, uint8_t *len_out, enum HDL_Type *type_out, void **val_out) {

    if(blocks[*blockIndex][0] == '[') {
        // Array
        uint8_t tmp_len = 0;
        enum HDL_Type tmp_type = 0;
        uint8_t tmp_val[64];

        void *val_addr = tmp_val;

        *len_out = 0;
        *type_out = HDL_TYPE_NULL;
        // Allocated length for array
        int alloc_len = 8;

        (*blockIndex)++;
        while((*blockIndex) < block_count) {
            if(blocks[*blockIndex][0] == ']') {
                // Array done
                break;
            }
            if(_HDL_ParseValue(doc, blockIndex, &tmp_len, &tmp_type, &val_addr)) {
                printf("Failed to parse array values\r\n");
                return 1;
            }
            if(tmp_type == HDL_TYPE_STRING) {
                printf("Arrays do not support strings\r\n");
                return 1;
            }
            
            if(*len_out > 0 && (*type_out != tmp_type)) {
                printf("Array mismatch of types\r\n");
                return 1;
            }

            if(*len_out == 0) {
                // Allocate
                *val_out = malloc(HDL_TYPE_SIZES[tmp_type] * alloc_len);
                *type_out = tmp_type;
            }
            else {
                // Reallocate if needed
                if((*len_out) >= alloc_len) {
                    alloc_len += 8;
                    *val_out = realloc(*val_out, HDL_TYPE_SIZES[tmp_type] * alloc_len);
                }
            }

            memcpy(((uint8_t*)*val_out) + (HDL_TYPE_SIZES[tmp_type] * (*len_out)), tmp_val, HDL_TYPE_SIZES[tmp_type]);
            (*len_out)++;

            (*blockIndex)++;

            if(blocks[*blockIndex][0] == ']') {
                // Array done
                break;
            }
            else if(blocks[*blockIndex][0] == ',') {
                // Next value
                
            }
            else {
                printf("Unexpected value in array\r\n");
                return 1;
            }


            (*blockIndex)++;
        }
    }
    else if(blocks[*blockIndex][0] == '"' || blocks[*blockIndex][0] == '\'') {
        uint8_t quoteType = (blocks[*blockIndex][0] == '"');
        // String
        *len_out = 1;
        *type_out = HDL_TYPE_STRING;
        int slen = strlen(blocks[*blockIndex]);
        // Leave quotes out of the string
        if(*val_out == NULL) {
            *val_out = malloc(slen - 1);
        }
        memcpy(*val_out, (blocks[*blockIndex] + 1), slen - 2);
        ((char*)(*val_out))[slen - 2] = 0;
        
        if(quoteType == 0 && blocks[*blockIndex][slen - 1] == '\'') {
            // OK
        }
        else if(quoteType == 1 && blocks[*blockIndex][slen - 1] == '"') {
            // OK
        }
        else {
            // Should not happen ever
            printf("No enclosing quote\r\n");
            return 1;
        }
    }
    else if(isNumberString(blocks[*blockIndex])) {
        // Save as float, compiler will optimize it to the correct size
        *len_out = 1;
        *type_out = HDL_TYPE_FLOAT;
        if(*val_out == NULL) {
            *val_out = malloc(sizeof(float));
        }
        *(float*)(*val_out) = (float)atof(blocks[*blockIndex]);
    }
    else if(strcmp(blocks[*blockIndex], "true") == 0) {
        // boolean/true
        *len_out = 1;
        *type_out = HDL_TYPE_BOOL;
        if(*val_out == NULL) {
            *val_out = malloc(1);
        }
        *(uint8_t*)(*val_out) = 1;
    }
    else if(strcmp(blocks[*blockIndex], "false") == 0) {
        // boolean/false
        *len_out = 1;
        *type_out = HDL_TYPE_BOOL;
        if(*val_out == NULL) {
            *val_out = malloc(1);
        }
        *(uint8_t*)(*val_out) = 0;
    }
    else if(blocks[*blockIndex][0] == '$') {
        // Binding address
        *len_out = 1;
        *type_out = HDL_TYPE_BIND;
        (*blockIndex)++;
        if(!isIntString(blocks[*blockIndex])) {
            
            uint8_t f = 0;
            for(int i = 0; i < doc->varCount; i++) {
                if(strcmp(doc->vars[i].name, blocks[*blockIndex]) == 0) {

                    // Copy value, should be FLOAT
                    *len_out = doc->vars[i].count;
                    *type_out = HDL_TYPE_BIND;
                    if(*val_out == NULL) {
                        *val_out = malloc(1);
                    }
                    *(uint8_t*)(*val_out) = (uint8_t)*(float*)doc->vars[i].value;

                    f = 1;
                    break;
                }
            }
            if(!f) {
                printf("Waringin: Could not parse binding\r\n");
                if(*val_out == NULL) {
                    *val_out = malloc(1);
                }
                *(uint8_t*)(*val_out) = 0xFF;
            }
        }
        else {
            if(*val_out == NULL) {
                *val_out = malloc(1);
            }
            *(uint8_t*)(*val_out) = atoi(blocks[*blockIndex]);
        }
    }
    else {
        // Check variable table
        uint8_t f = 0;
        for(int i = 0; i < doc->varCount; i++) {
            if(strcmp(doc->vars[i].name, blocks[*blockIndex]) == 0) {

                // TODO: Copy or reference? reference for now
                *len_out = doc->vars[i].count;
                *type_out = doc->vars[i].type;
                if(*val_out == NULL) {
                    *val_out = doc->vars[i].value;
                }
                else {
                    if(*type_out == HDL_TYPE_STRING) {
                        strcpy(*val_out, doc->vars[i].value);
                    }
                    else {
                        memcpy(*val_out, doc->vars[i].value, HDL_TYPE_SIZES[doc->vars[i].type]);
                    }
                }

                f = 1;
                break;
            }
        }
        if(!f) {
            // Check image table
            for(int i = 0; i < doc->bitmapCount; i++) {
                if(strcmp(doc->bitmaps[i].name, blocks[*blockIndex]) == 0) {

                    *len_out = 1;
                    *type_out = HDL_TYPE_IMG;
                    if(*val_out == NULL) {
                        *val_out = malloc(1);
                    }
                    *(uint8_t*)(*val_out) = i;
                    f = 1;
                    break;
                }
            }
        }

        if(!f) {
            printf("Unknown value!\r\n");
            return 1;
        }
    }


    return 0;
}

int _HDL_ParseAttribute (struct HDL_Document *doc, struct HDL_Element *element, int *blockIndex) {

    if(isDelimiter(blocks[*blockIndex][0])) {
        printf("Error: Unexpected delimiter on attribute\r\n");
        return 1;
    }
    // Reallocate attributes if needed
    if(element->attrCount >= element->attrAllocCount) {
        element->attrAllocCount += HDL_ELEMENT_ATTR_INITIAL_SIZE;
        element->attrs = realloc(element->attrs, sizeof(struct HDL_Attr) * element->attrAllocCount);
    }
    
    struct HDL_Attr *attr = &element->attrs[element->attrCount++];
    memset(attr, 0, sizeof(struct HDL_Attr));

    // Read attribute key
    strcpy(attr->key, blocks[*blockIndex]);
    (*blockIndex)++;

    if(!isDelimiter(blocks[*blockIndex][0]) || blocks[*blockIndex][0] == '>' || blocks[*blockIndex][0] == '/') {
        // Nothing assigned, set to true
        attr->count = 1;
        attr->type = HDL_TYPE_BOOL;
        attr->value = malloc(1);
        *(uint8_t*)attr->value = 1;
        //if(blocks[*blockIndex][0] == '>' || blocks[*blockIndex][0] == '/') {
            // Decrement blockIndex if ending tag
        (*blockIndex)--;
        //}
        return 0;
    }

    if(blocks[*blockIndex][0] == '=') {
        (*blockIndex)++;
        if(_HDL_ParseValue(doc, blockIndex, &attr->count, &attr->type, &attr->value)) {
            
            return 1;
        }
    }
    else {
        printf("Unexpected character\r\n");
        return 1;
    }

    return 0;
}

int _HDL_ParseImageFromPath (struct HDL_Bitmap *bmp, struct HDL_Document *doc, int *blockIndex) {
    // Remove quotes
    char nbuff[128];
    strcpy(nbuff, blocks[*blockIndex] + 1);
    nbuff[strlen(nbuff) - 1] = 0;
    return HDL_BitmapFromBMP(nbuff, bmp);
}

int _HDL_ParseImage (struct HDL_Document *doc, int *blockIndex) {
    (*blockIndex)++;
    // Reallocate images
    if(doc->bitmapCount >= doc->bitmapAllocCount) {
        doc->bitmapAllocCount += HDL_DOC_BITMAPS_INITIAL_SIZE;
        doc->bitmaps = realloc(doc->bitmaps, sizeof(struct HDL_Bitmap) * doc->bitmapAllocCount);
    }
    struct HDL_Bitmap *bmp = &doc->bitmaps[doc->bitmapCount++];

    // First block should be the name of the image
    if(isDelimiter(blocks[*blockIndex][0])) {
        printf("Unexpected character while parsing images\r\n");
        return 1;
    }
    
    strcpy(bmp->name, blocks[*blockIndex]);
    bmp->colorMode = HDL_COLORS_MONO;
    (*blockIndex)++;

    // Expecting image name
    if(blocks[*blockIndex][0] == '"') {
        return _HDL_ParseImageFromPath(bmp, doc, blockIndex);
    }
    // Expecting parenthesis with width, height inside
    else if(blocks[*blockIndex][0] != '(') {
        printf("(width, height) or image path expected while defining image\r\n");
        return 1;
    }
    // Width
    (*blockIndex)++;
    // TODO: Check if numerical
    bmp->width = atoi(blocks[*blockIndex]);
    // Comma between values
    (*blockIndex)++;
    if(blocks[*blockIndex][0] != ',') {
        printf("(width, height) expected while defining image\r\n");
        return 1;
    }
    // Height
    (*blockIndex)++;
    bmp->height = atoi(blocks[*blockIndex]);
    // Closing parenthesis
    (*blockIndex)++;
    if(blocks[*blockIndex][0] == ',') {
        // Spritesheet values
        (*blockIndex)++;
        bmp->sprite_width = atoi(blocks[*blockIndex]);
        (*blockIndex)++;
        if(blocks[*blockIndex][0] != ',') {
            printf("(width, height, sprite_width, sprite_height) expected while defining image\r\n");
            return 1;
        }
        (*blockIndex)++;
        bmp->sprite_height = atoi(blocks[*blockIndex]);
        (*blockIndex)++;
    }
    else {
        bmp->sprite_height = bmp->height;
        bmp->sprite_width = bmp->width;
    }

    if(blocks[*blockIndex][0] != ')') {
        printf("Missing parenthesis while defining image\r\n");
        return 1;
    }

    (*blockIndex)++;

    if(blocks[*blockIndex][0] == '"') {
        // Bitmap from .bmp
        return _HDL_ParseImageFromPath(bmp, doc, blockIndex);
    }

    bmp->size = (bmp->width + 7)/8 * bmp->height;
    // Allocate and zero data buffer
    bmp->data = malloc(bmp->size);
    memset(bmp->data, 0, bmp->size);

    int y = 0;
    int x = 0;
    int pad_width = (bmp->width + 7) / 8;
    // Start reading image data until semicolon
    while((*blockIndex) < block_count) {

        if(blocks[*blockIndex][0] == ';') {
            // Done
            break;
        }
        char *block = blocks[*blockIndex];
        int len = strlen(block);
        
        for(int i = 0; i < len; i++) {

            if(y * pad_width + x / 8 >= bmp->size) {
                printf("ERROR: Image data overflow %i\r\n", bmp->size);
                return 1;
            }
            
            if(block[i] == '1') {
                bmp->data[y * pad_width + x / 8] |= 1 << (7 - (x % 8));
            }
            else if(block[i] == '0') {
                
            }
            else {
                printf("ERROR: Expected ; after image data\r\n");
                return 1;
            }
            x++;
            if(x >= bmp->width) {
                x = 0;
                y++;
            }
        }

        (*blockIndex)++;
    }

    printf("Bitmap '%s' built\r\n", bmp->name);

    return 0;
}

int _HDL_ParseVariable (struct HDL_Document *doc, int *blockIndex) {
    (*blockIndex)++;
    if(strcmp(blocks[*blockIndex], "const") == 0) {
        // Define constant
        // Reallocate variables if needed
        if(doc->varCount >= doc->varAllocCount) {
            doc->varAllocCount += HDL_DOC_VARS_INITIAL_SIZE;
            doc->vars = realloc(doc->vars, sizeof(struct HDL_Variable) * doc->varAllocCount);
        }

        (*blockIndex)++;
        if(isDelimiter(blocks[*blockIndex][0])) {
            // Unexpected
            printf("Unexpected delimiter instead of const name\r\n");
            return 1;
        }
        struct HDL_Variable *_var = &doc->vars[doc->varCount++];

        memset(_var, 0, sizeof(struct HDL_Variable));

        _var->isConst = 1;
        // Copy name to variable
        strcpy(_var->name, blocks[*blockIndex]);

        (*blockIndex)++;

        if(_HDL_ParseValue(doc, blockIndex, &_var->count, &_var->type, &_var->value)) {
            printf("Failed to parse value for const\r\n");
            return 1;
        }

        (*blockIndex)++;

    }
    else if(strcmp(blocks[*blockIndex], "img") == 0) {
        // Define bitmap
        if(_HDL_ParseImage(doc, blockIndex)) {
            printf("Failed to parse bitmap image\r\n");
            return 1;
        }

        (*blockIndex)++;
    }
    else {
        printf("Error: Unknown definition %s\r\n", blocks[*blockIndex]);
        return 1;
    }
    return 0;
}

int _HDL_ParseElement (struct HDL_Document *doc, int parentIndex, int *blockIndex) {

    (*blockIndex)++;
    if(isDelimiter(blocks[*blockIndex][0])) {
        // Should be a tagname, not delimiter
        printf("Unexpected delimiter at start\r\n");

        return 1;
    }

    // Reallocate if needed
    if(doc->elementCount >= doc->elementAllocCount) {
        doc->elementAllocCount += HDL_DOC_ELEMENTS_INITIAL_SIZE;
        doc->elements = realloc(doc->elements, sizeof(struct HDL_Element) * doc->elementAllocCount);
    }

    // Element count or element address may change, so save the index here
    int elementIndex = doc->elementCount;

    struct HDL_Element *element = &doc->elements[elementIndex];

    // Increment element count
    doc->elementCount++;

    // Initialize element
    _HDL_InitElement(element);

    // Save the tagname
    strcpy(element->tag, blocks[*blockIndex]);
    (*blockIndex)++;

    // 0 = undefined, 1 = short tag, 2 = long tag (check children too)
    uint8_t tagType = 0;

    // Element tag can be checked here

    // Loop through possible attributes until /> or >
    while((*blockIndex) < block_count) {
        char *block = blocks[*blockIndex];
        if(isDelimiter(block[0])) {
            if(block[0] == '/') {

                // Short tag
                (*blockIndex)++;
                block = blocks[*blockIndex];
                if(block[0] != '>') {
                    // Unexpected character
                    printf("Unexpected delimiter attrs 1\r\n");

                    return 1;
                }
                else {
                    tagType = 1;
                    (*blockIndex)++;
                    break;
                }
            }
            else if(block[0] == '>') {
                // Long tag

                tagType = 2;
                (*blockIndex)++;
                break;
            }
            else {
                printf("Error: Unexpected delimiter %c\r\n", block[0]);
                return 1;
            }
        }
        else {
            // Attribute
            if(_HDL_ParseAttribute(doc, element, blockIndex)) {
                printf("Error: Failed to parse attribute\r\n");
                return 1;
            }
            (*blockIndex)++;
        }
    }


    if(parentIndex < 0) {
        // Root element
        element->parent = -1;
        
    }
    else {
        struct HDL_Element *parent = &doc->elements[parentIndex];
        // Add element to parents children
        parent->children[parent->childCount++] = elementIndex;

        // Reallocate if needed...
        if(parent->childCount >= parent->childAllocCount) {
            parent->childAllocCount += HDL_ELEMENT_CHILDREN_INITIAL_SIZE;
            parent->children = realloc(parent->children, sizeof(struct HDL_Element) * parent->childAllocCount);
        }
    }

    if(tagType == 0) {
        // Unexpected end of input
        return 1;
    }
    else if(tagType == 1) {
        // Element ready
    }
    else if(tagType == 2) {
        // Parse children/content
        
        while((*blockIndex) < block_count) {
            if(blocks[*blockIndex][0] == '<') {
                (*blockIndex)++;
                if(blocks[*blockIndex][0] == '/') {

                    // Expect end tag for this element
                    (*blockIndex)++;
                    // Need to get address of the element again because it may have been changed
                    element = &doc->elements[elementIndex];
                    // Compare tags
                    if(strcmp(blocks[*blockIndex], element->tag) == 0) {
                        (*blockIndex)++;
                        if(blocks[*blockIndex][0] == '>') {
                            // Element OK
                            (*blockIndex)++;
                            break;
                        }
                        else {
                            printf("Error: Unexpected character on closing tag\r\n");
                            return 1;
                        }
                    }
                    else {
                        printf("Error: Mismatch of tags (<%s> vs </%s>)\r\n", blocks[*blockIndex], element->tag);
                        return 1;
                    }
                }
                else {
                    if(isDelimiter(blocks[*blockIndex][0])) {
                        // Unexpected character
                        printf("Unexpected delimiter\r\n");
                        return 1;
                    }
                    else {
                        // Child element
                        // Element parse function increments blockIndex at start so let's back up 1 block
                        (*blockIndex)--;
                        if(_HDL_ParseElement(doc, elementIndex, blockIndex)) {
                            // Failed to parse
                            
                            return 1;
                        }
                    }
                }
            }
            else {
                if(!isDelimiter(blocks[*blockIndex][0])) {
                    // Copy content 
                    // Need to get address of the element again because it may have been changed
                    element = &doc->elements[elementIndex];
                    if(element->content == NULL) {
                        int bLen = strlen(blocks[*blockIndex]);
                        element->content = malloc(bLen + 1);
                        memcpy(element->content, blocks[*blockIndex], bLen + 1);
                    }
                    else {
                        // Prevent multiple allocation
                        printf("Error: Multiple allocation of content\r\n");
                        return 1;
                    }
                }
                else {

                    printf("Error: Unexpected character\r\n");
                    return 1;
                }
                (*blockIndex)++;
            }
        }
    }
    return 0;
}

int _HDL_ParseBlocks (struct HDL_Document *doc) {
    int err = 0;
    int blockIndex = 0;

    // Flag if root is created
    uint8_t rootCreated = 0;

    // Loop until all blocks
    while(blockIndex < block_count) {
        if(blocks[blockIndex][0] == '#') {
            // Variable or image definition
            err = _HDL_ParseVariable(doc, &blockIndex);
            if(err)
                break;
        }
        else if(blocks[blockIndex][0] == '<') {
            // Tag start - parse element
            if(rootCreated) {
                // Multiple root elements, illegal
                err = 1;
                printf("Error: trying to create multiple roots\r\n");
                break;
            }
            rootCreated = 1;
            err = _HDL_ParseElement(doc, -1, &blockIndex);
            if(err) {
                printf("Error while parsing elements\r\n");
                break;
            }
        }
        else if(blocks[blockIndex][0] == '/' && blocks[blockIndex][1] == '*') {
            // Comment
            while(blockIndex < block_count && (blocks[blockIndex][0] != '*' && blocks[blockIndex][1] == '/')) {
                // Wait until out of comment
                blockIndex++;
            }
        }
        else {

            printf("Unexpected block %s\r\n", blocks[blockIndex]);
            err = 1;
            if(err)
                break;
        }
    }

    return err;
}


/**
 * @brief Parses an HDL file
 * 
 * @param data Data to parse
 * @return int 0 on success
 */
int HDL_Parse (char *data, struct HDL_Document *doc) {
    int err = 0;
    // Initialize doc
    // Elements
    doc->elementCount = 0;
    doc->elementAllocCount = HDL_DOC_ELEMENTS_INITIAL_SIZE;
    doc->elements = malloc(sizeof(struct HDL_Element) * doc->elementAllocCount);
    // Variables
    doc->varCount = 0;
    doc->varAllocCount = HDL_DOC_VARS_INITIAL_SIZE;
    doc->vars = malloc(sizeof(struct HDL_Variable) * doc->varAllocCount);

    // Bitmaps
    doc->bitmapCount = 0;
    doc->bitmapAllocCount = HDL_DOC_BITMAPS_INITIAL_SIZE;
    doc->bitmaps = malloc(sizeof(struct HDL_Bitmap) * doc->bitmapAllocCount);

    // Parse the data in to easy access blocks
    err |= _HDL_ParseDataToBlocks(data);

    // Parse blocks
    err |= _HDL_ParseBlocks(doc);

    _HDL_FreeBlocks();

    return err;
}

void HDL_PrintElement (struct HDL_Document *doc, struct HDL_Element *element, int depth) {
    for(int i = 0; i < depth * 2; i++) {
        printf(" ");
    }

    printf("%s: %s ", element->tag, element->content);
    if(element->attrCount > 0) {
        printf("[");
    }
    for(int i = 0; i < element->attrCount; i++) {
        struct HDL_Attr *attr = &element->attrs[i];
        printf("%s = ", attr->key);
        if(attr->count > 1) {
            printf("[");
        }
        switch(attr->type) {
            case HDL_TYPE_NULL:
            {
                printf("NULL");
                break;
            }
            case HDL_TYPE_BOOL:
            {
                printf("%s", (*(uint8_t*)attr->value) ? "true" : "false");
                break;
            }
            case HDL_TYPE_FLOAT:
            case HDL_TYPE_I8:
            case HDL_TYPE_I16:
            case HDL_TYPE_I32:
            {
                for(int i = 0; i < attr->count; i++) {
                    switch(attr->type) {
                        case HDL_TYPE_FLOAT:
                            printf("%.2f", (((float*)attr->value)[i]));
                            break;
                        case HDL_TYPE_I8:
                            printf("%i", (((int8_t*)attr->value)[i]));
                            break;
                        case HDL_TYPE_I16:
                            printf("%i", (((int16_t*)attr->value)[i]));
                            break;
                        case HDL_TYPE_I32:
                            printf("%i", (((int32_t*)attr->value)[i]));
                            break;
                    }
                    if(i < attr->count - 1) {
                        printf(", ");
                    }
                }
                break;
            }
            case HDL_TYPE_STRING:
            {
                printf("\"%s\"", (const char *)attr->value);
                break;
            }
        }
        if(attr->count > 1) {
            printf("]");
        }

        printf(", ");
    }
    if(element->attrCount > 0) {
        printf("]");
    }

    printf("\r\n");

    for(int i = 0; i < element->childCount; i++) {
        HDL_PrintElement(doc, &doc->elements[element->children[i]], depth + 1);
    }
}

void HDL_PrintVars (struct HDL_Document *doc) {
    printf("VARS (%i): \r\n", doc->varCount);
    for(int i = 0; i < doc->varCount; i++) {
        struct HDL_Variable *_var = &doc->vars[i];
        printf("const %s = ", _var->name);
        switch(_var->type) {
            case HDL_TYPE_NULL:
            {
                printf("NULL");
                break;
            }
            case HDL_TYPE_BOOL:
            {
                printf("%s", (*(uint8_t*)_var->value) ? "true" : "false");
                break;
            }
            case HDL_TYPE_FLOAT:
            case HDL_TYPE_I8:
            case HDL_TYPE_I16:
            case HDL_TYPE_I32:
            {
                for(int i = 0; i < _var->count; i++) {
                    switch(_var->type) {
                        case HDL_TYPE_FLOAT:
                            printf("%.2f", (((float*)_var->value)[i]));
                            break;
                        case HDL_TYPE_I8:
                            printf("%i", (((int8_t*)_var->value)[i]));
                            break;
                        case HDL_TYPE_I16:
                            printf("%i", (((int16_t*)_var->value)[i]));
                            break;
                        case HDL_TYPE_I32:
                            printf("%i", (((int32_t*)_var->value)[i]));
                            break;
                    }
                    if(i < _var->count - 1) {
                        printf(", ");
                    }
                }
                break;
            }
            case HDL_TYPE_STRING:
            {
                printf("\"%s\"", (const char *)_var->value);
                break;
            }
        }

        printf(", ");
    }
    printf("\r\n\r\n");
}