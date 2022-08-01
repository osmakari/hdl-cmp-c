#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hdl-parse.h"
#include <math.h>
#include "hdl-cmp.h"

// Unknown file format
#define HDL_COMPILER_OUTPUT_FORMAT_UNKNOWN 0xFF
// Binary file
#define HDL_COMPILER_OUTPUT_FORMAT_BIN  0
// C source file
#define HDL_COMPILER_OUTPUT_FORMAT_C    1

// Maximum output file buffer size
#define HDL_COMPILER_OUTPUT_BUFFER_SIZE 4096


#define HDL_COMPILER_VERSION_MAJOR  0
#define HDL_COMPILER_VERSION_MINOR  1

// Input file path
char input_file_path[128];

const char *tagnames[] = {
    "box",
    "text"
};
enum HDL_AttrIndex {
    HDL_ATTR_X          = 0, // X position
    HDL_ATTR_Y          = 1, // Y position
    HDL_ATTR_WIDTH      = 2, // Width
    HDL_ATTR_HEIGHT     = 3, // Height
    HDL_ATTR_FLEX       = 4, // Flex 
    HDL_ATTR_FLEX_DIR   = 5, // Flex dir
    HDL_ATTR_BIND       = 6, // Bindings
    HDL_ATTR_IMG        = 7, // Bitmap image
    HDL_ATTR_PADDING    = 8, // Padding
    HDL_ATTR_ALIGN      = 9, // Content alignment
    HDL_ATTR_SIZE       = 10, // Bitmap/font size
    HDL_ATTR_DISABLED   = 11  // Disabled
};

const char *attrnames[] = {
    "x",
    "y",
    "width",
    "height",
    "flex",
    "flexdir",
    "bind",
    "img",
    "padding",
    "align",
    "size",
    "disabled"
};

const char *alignment_x[] = {
    "center",
    "left",
    "right"
};

const char *alignment_y[] = {
    "middle",
    "top",
    "bottom"
};

uint8_t findTag (char *tagname) {
    for(int x = 0; x < sizeof(tagnames) / sizeof(const char *); x++) {
        if(strcmp(tagname, tagnames[x]) == 0) {
            return x;
        }
    }
    return 0xFF;
}

uint8_t findAttr (char *attrname) {
    for(int x = 0; x < sizeof(attrnames) / sizeof(const char *); x++) {
        if(strcmp(attrname, attrnames[x]) == 0) {
            return x;
        }
    }
    return 0xFF;
}

int compileElement (struct HDL_Document *doc, struct HDL_Element *element, uint8_t *buffer, int *pc) {
    
    uint8_t tagc = findTag(element->tag);
    if(tagc == 0xFF) {
        printf("Tag '%s' not found\r\n", element->tag);
        return 1;
    }
    buffer[(*pc)++] = tagc;
    int str_len = 0;
    if(element->content != NULL) {
        str_len = strlen(element->content);
    }
    if(str_len == 0) {
        buffer[(*pc)] = 0;
    }
    else {
        memcpy(&buffer[(*pc)], element->content, str_len + 1);
    }
    (*pc) += str_len + 1;

    // Save address count position
    int attrCountAddr = (*pc);
    buffer[(*pc)++] = element->attrCount;
    for(int i = 0; i < element->attrCount; i++) {
        uint8_t attr = findAttr(element->attrs[i].key);
        if(attr == 0xFF) {
            // Attribute not defined
            buffer[attrCountAddr]--;
            printf("Skipping attribute '%s' - not defined\r\n", element->attrs[i].key);
        }
        else {
            buffer[(*pc)++] = attr;
            void *val = element->attrs[i].value;
            float ftemp = 0;
            if(attr == HDL_ATTR_FLEX_DIR) {
                // Flex direction attribute
                if(element->attrs[i].type == HDL_TYPE_STRING) {
                    val = &ftemp;
                    ftemp = 1;
                    element->attrs[i].type = HDL_TYPE_FLOAT;
                    if(strcmp((char*)element->attrs[i].value, "col") == 0) {
                        ftemp = 1;
                    }
                    else if(strcmp((char*)element->attrs[i].value, "row") == 0) {
                        ftemp = 2;
                    }
                    else {
                        printf("Unknown value '%s' given for 'flexdir'\r\n");
                    }
                }
            }
            else if(attr == HDL_ATTR_ALIGN) {
                // Alignment
                // 2 part string in format "yalign xalign"
                // Example "middle center", "top right", "bottom center"
                if(element->attrs[i].type == HDL_TYPE_STRING) {
                    char *y_string = element->attrs[i].value;
                    char *x_string = NULL;
                    int slen = strlen(y_string);
                    element->attrs[i].type = HDL_TYPE_FLOAT;
                    ftemp = 0;
                    val = &ftemp;
                    for(int i = 0; slen; i++) {
                        if(y_string[i] == ' ') {
                            y_string[i] = 0;
                            x_string = &y_string[i + 1];
                            break;
                        }
                    }
                    if(x_string != NULL) {
                        uint8_t y_align = 0;
                        uint8_t x_align = 0;
                        int found = 0;
                        for(int i = 0; i < sizeof(alignment_y) / sizeof(const char *); i++) {
                            if(strcmp(alignment_y[i], y_string) == 0) {
                                y_align = i;
                                found = 1;
                                break;
                            }
                        }
                        if(!found) {
                            printf("Error: Unknown Y axis value given for 'align'\r\n");
                        }
                        else {
                            found = 0;
                            for(int i = 0; i < sizeof(alignment_x) / sizeof(const char *); i++) {
                                if(strcmp(alignment_x[i], x_string) == 0) {
                                    x_align = i;
                                    found = 1;
                                    break;
                                }
                            }
                            if(!found) {
                                printf("Error: Unknown X axis value given for 'align'\r\n");
                            }
                            else {
                                ftemp = (uint8_t)((y_align) | ((x_align) << 4));
                            }
                        }
                    }
                    else {
                        printf("Error: 'align' requires vertical and horizontal alignment ex. 'middle center'\r\n");
                    }
                }

            }
            int type_addr = (*pc);
            buffer[(*pc)++] = element->attrs[i].type;
            buffer[(*pc)++] = element->attrs[i].count;
            switch(element->attrs[i].type) {
                case HDL_TYPE_NULL:
                {
                    buffer[(*pc)++] = 0;
                    break;
                }
                case HDL_TYPE_IMG:
                case HDL_TYPE_BOOL:
                case HDL_TYPE_BIND:
                {
                    buffer[(*pc)++] = *(uint8_t*)val;
                    break;
                }
                case HDL_TYPE_FLOAT:
                {
                    // Optimize value
                    float _mod = fmodf(*(float*)&buffer[*pc], 1);
                    uint8_t ntype = HDL_TYPE_FLOAT;
                    if(_mod == 0) {
                        int32_t nval = (int32_t)*(float*)&buffer[*pc];
                        // Integer
                        if(nval < 0x80 && nval > -0x80) {
                            ntype = HDL_TYPE_I8;
                        }
                        else if(nval < 0x8000 && nval > -0x8000) {
                            ntype = HDL_TYPE_I16;
                        }
                        else {
                            ntype = HDL_TYPE_I32;
                        }
                    }
                    for(int z = 0; z < element->attrs[i].count; z++) {
                        switch(ntype) {
                            case HDL_TYPE_FLOAT:
                            {
                                *(float*)&buffer[*pc] = (float)((float*)val)[z];
                                (*pc) += sizeof(float);
                                break;
                            }
                            case HDL_TYPE_I8:
                            {
                                *(int8_t*)&buffer[*pc] = (int8_t)((float*)val)[z];
                                (*pc) += sizeof(int8_t);
                                break;
                            }
                            case HDL_TYPE_I16:
                            {
                                *(int16_t*)&buffer[*pc] = (int16_t)((float*)val)[z];
                                (*pc) += sizeof(int16_t);
                                break;
                            }
                            case HDL_TYPE_I32:
                            {
                                *(int32_t*)&buffer[*pc] = (int32_t)((float*)val)[z];
                                (*pc) += sizeof(int32_t);
                                break;
                            }
                        }
                    }
                    buffer[type_addr] = ntype;
                    break;
                }
                case HDL_TYPE_STRING:
                {
                    int len = strlen((char*)val);
                    memcpy((char*)&buffer[*pc], (char*)val, len + 1);
                    (*pc) += len + 1;
                    break;
                }
            }
        }
    }
    buffer[(*pc)++] = element->childCount;
    for(int i = 0; i < element->childCount; i++) {
        compileElement(doc, &doc->elements[element->children[i]], buffer, pc);
    }

    return 0;
}

int compileBitmap (struct HDL_Document *doc, struct HDL_Bitmap *bmp, uint8_t *buffer, int *pc) {
    *(uint16_t*)&buffer[*pc] = bmp->size;
    (*pc) += 2;
    *(uint16_t*)&buffer[*pc] = bmp->width;
    (*pc) += 2;
    *(uint16_t*)&buffer[*pc] = bmp->height;
    (*pc) += 2;

    buffer[*pc] = bmp->colorMode;
    (*pc) += 1;

    memcpy(&buffer[*pc], bmp->data, bmp->size);

    (*pc) += bmp->size;

    return 0;
}

int compile (struct HDL_Document *doc, uint8_t *buffer, int *pc) {

    if(doc == NULL) {
        return 1;
    }

    memset(buffer, 0, 16);

    // Major and minor versions
    buffer[(*pc)++] = (uint8_t)HDL_COMPILER_VERSION_MAJOR;
    buffer[(*pc)++] = (uint8_t)HDL_COMPILER_VERSION_MINOR;

    // Bitmap count
    buffer[(*pc)++] = doc->bitmapCount;

    // Vartable count
    buffer[(*pc)++] = 0;

    // Element count
    *(uint16_t*)&buffer[(*pc)] = doc->elementCount;
    (*pc) += 2;

    // Padding
    (*pc) = 16;

    // Bitmaps...
    for(int i = 0; i < doc->bitmapCount; i++) {
        if(compileBitmap(doc, &doc->bitmaps[i], buffer, pc)) {
            printf("ERROR: Failed to compile bitmap\r\n");
            return 1;
        }
    }

    // Vartables...

    // Elements
    if(compileElement(doc, &doc->elements[0], buffer, pc)) {
        // Fail
        printf("ERROR: Failed to compile element\r\n");
        return 1;
    }


    return 0;
}

uint8_t output_buffer[HDL_COMPILER_OUTPUT_BUFFER_SIZE];

void writeBinFile (struct HDL_Document *doc, FILE *file, int original_size) {
    
    int len = 0;

    if(compile(doc, output_buffer, &len)) {
        // Error
        printf("Failed to compile\r\n");
    }
    else {
        printf("Original: %iB, Compiled: %iB\r\n", original_size, len);
        fwrite(output_buffer, 1, len, file);
    }

}

void writeCFile (struct HDL_Document *doc, FILE *file, int original_size, int comment) {
    int len = 0;

    if(compile(doc, output_buffer, &len)) {
        // Error
        printf("Failed to compile\r\n");
    }
    else {
        printf("Original: %iB, Compiled: %iB\r\n", original_size, len);
        
        fprintf(file, "// HDL output file\n// Original size: %iB, Compiled size: %iB\n\n", original_size, len);

        fprintf(file, "// Output\nunsigned char HDL_PAGE_OUTPUT[%i] = {\n", len);

        if(!comment) {
            for(int i = 0; i < len; i++) {
                fprintf(file, "0x%02X", output_buffer[i]);
                if(i != len - 1) {
                    fputs(", ", file);
                }
                if((i + 1) % 16 == 0) {
                    // Newline after every 16 bytes
                    fputc('\n', file);
                }
            }
        }
        else {
            // Commented version
            int i = 0;
            // File format version
            fprintf(file, "0x%02X, 0x%02X, // File format version (major, minor)\n", output_buffer[i], output_buffer[i + 1]);
            i += 2;
            // Bitmap, vartable, element count
            uint8_t bitmapCount = output_buffer[i];
            uint8_t vartableCount = output_buffer[i + 1];
            uint16_t elementCount = *(uint16_t*)&output_buffer[i + 2];
            fprintf(file, "0x%02X, 0x%02X, 0x%02X, 0x%02X,// Bitmap(1B), Vartable(1B), Element(2B) count\n", bitmapCount, vartableCount, output_buffer[i + 2], output_buffer[i + 3]);
            i += 4;
            // Reserved until 0x10
            for(i; i < 0x10; i++) {
                fprintf(file, "0x%02X, ", output_buffer[i]);
            }
            fprintf(file, " // Padding until 0x10\n");
            fprintf(file, "// Bitmaps\n");
            // Bitmap data 
            for(int x = 0; x < bitmapCount; x++) {
                fprintf(file, "// Bitmap %i\n", x);
                // Bitmap size
                uint16_t bmapSize = *(uint16_t*)&output_buffer[i];
                fprintf(file, "0x%02X, 0x%02X, // Bitmap size\n", output_buffer[i], output_buffer[i + 1]);
                i += 2;
                // Bitmap width, height
                fprintf(file, "0x%02X, 0x%02X, 0x%02X, 0x%02X, // Bitmap width (2B), height (2B)\n", 
                        output_buffer[i], output_buffer[i + 1], output_buffer[i + 2], output_buffer[i + 3]);
                i += 4;
                // Color mode
                fprintf(file, "0x%02X, // Color mode\n", output_buffer[i]);
                i++;
                // Image data
                fprintf(file, "// Image data (%iB)\n", bmapSize);
                for(int z = 0; z < bmapSize; z++) {
                    fprintf(file, "0x%02X, ", output_buffer[i]);
                    if((z + 1) % 16 == 0) {
                        // Newline after every 16 bytes
                        fputc('\n', file);
                    }
                    i++;
                }
                fputc('\n', file);
                // Vartable
                // TODO:
                fprintf(file, "// Vartable\n");
                for(int z = 0; z < vartableCount; z++) {

                }
                // Elements
                fprintf(file, "// Elements\n");
                for(int z = 0; z < elementCount; z++) {
                    // Tag
                    fprintf(file, "0x%02X, // Tag\n", output_buffer[i]);
                    i++;
                    // Content
                    do {
                        fprintf(file, "0x%02X, ", output_buffer[i]);
                    }
                    while(output_buffer[i++] != 0);

                    fprintf(file, " // Content\n");
                    // Attribute count
                    uint8_t attrCount = output_buffer[i];
                    fprintf(file, "0x%02X, // Attribute count\n", attrCount);
                    i++;
                    // Attributes
                    fprintf(file, "// Attributes\n");
                    for(int z = 0; z < attrCount; z++) {
                        fprintf(file, "// Attribute %i\n", z);
                        // Key, type, count
                        uint8_t attrType = output_buffer[i + 1];
                        uint8_t e_attrCount = output_buffer[i + 2];
                        fprintf(file, "0x%02X, 0x%02X, 0x%02X, // Key, Type, Count\n", output_buffer[i], attrType, e_attrCount);
                        i += 3;
                        fprintf(file, "// Attribute value\n");
                        int _attrlen = HDL_TYPE_SIZES[attrType] * e_attrCount;
                        if(attrType == HDL_TYPE_STRING) {
                            _attrlen = strlen((const char*)&output_buffer[i]);
                        }
                        for(int y = 0; y < _attrlen; y++) {
                            fprintf(file, "0x%02X, ", output_buffer[i]);
                            if((y + 1) % 16 == 0) {
                                // Newline after every 16 bytes
                                fputc('\n', file);
                            }
                            i++;
                        }
                        fputc('\n', file);
                    }
                    // Child count
                    fprintf(file, "0x%02X", output_buffer[i]);
                    i++;
                    if(i < len) {
                        fprintf(file, ", ");
                    }
                    fprintf(file, " // Child count\n");
                }
            }
        }

        fputs("\n};\n\n", file);

    }
}

/**
 * @brief Prints help
 * 
 */
void printHelp () {
    printf("HDL-CMP - HDL Compiler\r\n");
    printf("Usage: \r\n");
    printf("\thdl-cmp [options] <file>\r\n");
    printf("Options:\r\n");
    printf("\t-h\t\tPrint this help\r\n");
    printf("\t-o <file>\t\tOutput file path\r\n");
    printf("\t-f <format>\t\tForce output format: 'bin'(binary file) or 'c'(C source file)\r\n");
    printf("\t-c\t\tComment the output file\r\n");

}


int main (int argc, char *argv[]) {

    if(argc < 2) {
        printf("Usage: \r\n\thdl-cmp [options] <file>\r\n\tSee all options with -h\r\n");
        return 1;
    }

    char *filename = NULL;

    // Output file path
    char *argf_fpath = NULL;
    // Output file format
    uint8_t argf_format = HDL_COMPILER_OUTPUT_FORMAT_UNKNOWN;
    // Comment output file
    uint8_t arg_comment = 0;

    /*
        0: expect file or option
        1: expect output file path
        2: expect file format
    */
    uint8_t arg_state = 0;
    for(int i = 1; i < argc; i++) {
        switch(arg_state) {
            case 0:
            {
                // Expect file or option
                if(argv[i][0] == '-') {
                    // Option
                    switch(argv[i][1]) {
                        case 'h':
                        {
                            // Print help
                            printHelp();
                            return 0;
                        }
                        case 'o':
                        {
                            // Output file
                            arg_state = 1;
                            break;
                        }
                        case 'c':
                        {
                            // Comment output file
                            arg_comment = 1;
                            break;
                        }
                        case 'f':
                        {   
                            // Output file format
                            arg_state = 2;
                            break;
                        }
                    }
                }
                else {
                    // File
                    if(filename != NULL) {
                        printf("Error: Compiler expects only single input file\r\n");
                        return 1;
                    }
                    else {
                        filename = argv[i];
                    }
                }
                break;
            }
            case 1:
            {
                // Expect output path
                if(argv[i][0] == '-') {
                    printf("Error: expected filename after -o option\r\n");
                    return 1;
                }
                argf_fpath = argv[i];
                arg_state = 0;
                break;
            }
            case 2:
            {
                // Force output format
                // Expect output format
                if(argv[i][0] == '-') {
                    printf("Error: expected file format after -f option\r\n");
                    return 1;
                }
                if(strcmp(argv[i], "bin") == 0) {
                    // Binary file
                    argf_format = HDL_COMPILER_OUTPUT_FORMAT_BIN;
                }
                else if(strcmp(argv[i], "c") == 0) {
                    // C source file
                    argf_format = HDL_COMPILER_OUTPUT_FORMAT_C;
                }
                else {
                    printf("Error: Unknown output format: '%s'\r\n", argv[i]);
                    return 1;
                }
                arg_state = 0;
                break;
            }
        }
    }

    if(arg_state != 0) {
        printf("Error: Expected a value after %s\r\n", argv[argc - 1]);
        return 1;
    }

    if(filename == NULL) {
        printf("Error: Expected an input file\r\n");
        return 1;
    }
    input_file_path[0] = 0;
    // Set filename path
    for(int i = strlen(filename) - 1; i > 0; i--) {
        if(filename[i] == '/') {
            memcpy(input_file_path, filename, i + 1);
            input_file_path[i + 1] = 0;
        }
    }
    
    FILE *f = fopen(filename, "r");

    if(f == NULL) {
        printf("Failed to open file %s\r\n", filename);
        return 1;
    }

    // Seek to end to find the length of the file
    fseek(f, 0, SEEK_END);

    size_t filesize = ftell(f);

    // Seek back to start
    fseek(f, 0, SEEK_SET);

    // Allocate buffer
    char *buffer = malloc(filesize + 1);

    if(buffer == NULL) {
        printf("Failed to allocate enough memory\r\n");
        fclose(f);
        return 1;
    }

    // Read file into buffer
    fread(buffer, 1, filesize, f);

    // Zero terminate
    buffer[filesize] = 0;

    fclose(f);

    // Parse file
    struct HDL_Document doc;

    int err = HDL_Parse(buffer, &doc);
    if(!err) {
        int depth = 0;
        //_HDL_PrintBlocks();
        //HDL_PrintVars(&doc);
        //HDL_PrintElement(&doc, &doc.elements[0], depth);
    }
    else {
        printf("Parse failed\r\n");
        free(buffer);

        return 1;
    }

    free(buffer);

    // Detect format from file extension
    if(argf_format == HDL_COMPILER_OUTPUT_FORMAT_UNKNOWN && argf_fpath != NULL) {
        char *extension = NULL;
        char *inputFile = argf_fpath;
        for(int i = strlen(inputFile) - 1; i > 0; i--) {
            if(inputFile[i] == '.') {
                extension = (char*)inputFile + i;
            }
        }

        if(strcmp(extension, ".bin") == 0) {
            argf_format = HDL_COMPILER_OUTPUT_FORMAT_BIN;
        }
        else if(strcmp(extension, ".c") == 0) {
            argf_format = HDL_COMPILER_OUTPUT_FORMAT_C;
        }
    }


    // Write output file
    if(argf_fpath != NULL) {

        if(argf_format == HDL_COMPILER_OUTPUT_FORMAT_UNKNOWN) {
            printf("Unknown file output format\r\n");
            return 1;
        }

        FILE *fo = fopen(argf_fpath, "w");

        if(fo == NULL) {
            printf("Could not open '%s' for writing\r\n", argf_fpath);
            return 1;
        }

        if(argf_format == HDL_COMPILER_OUTPUT_FORMAT_BIN) {
            writeBinFile(&doc, fo, filesize);
        }
        else {
            writeCFile(&doc, fo, filesize, arg_comment);
        }

        fclose(fo);
    }
    else {
        // TODO: Output file not set
        printf("Output file not set\r\n");
    }

    return 0;
}