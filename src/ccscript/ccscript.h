#ifndef CCSCRIPT_CCSCRIPT_H
#define CCSCRIPT_CCSCRIPT_H

#include <inttypes.h>

enum {
    CCS_TYPE_NONE,
    CCS_TYPE_INT8,
    CCS_TYPE_INT16,
    CCS_TYPE_INT32,
    CCS_TYPE_INT64,
    CCS_TYPE_UINT8,
    CCS_TYPE_UINT16,
    CCS_TYPE_UINT32,
    CCS_TYPE_UINT64,
    CCS_TYPE_FLOAT32,
    CCS_TYPE_FLOAT64,
    CCS_TYPE_STRING,
};

struct ccs_str {
    unsigned len;
    unsigned size;
    char* str;
};

union ccs_num {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
};

union ccs_data {
    union ccs_num n;
    struct ccs_str s;
};

struct ccs_var {
    char* name;
    int type;
    union {
        union ccs_data data;
        void* arraydata;
    };
    unsigned dim;
    unsigned* sizes;
    unsigned size;
};

struct ccs_command {
    int cmd;
    void* args;
};

struct ccs_program {
    unsigned commands;
    struct ccs_command* commanddata;
};

enum {
    CCS_ERROR_NONE,
};

struct ccs_thread {
    int error;
    char* errinfo;
    unsigned programs;
    struct ccs_program* programdata;
    int currentprogram;
};

struct ccs_state {
    int threads;
    struct ccs_thread* threaddata;
    int vars;
    struct ccs_var* vardata;
};

enum {
    CCS_CMD_NOP,
    CCS_CMD_CALL,
    CCS_CMD_MATH,
    CCS_CMD_VMATH,
    CCS_CMD_MATHTO,
    CCS_CMD_VMATHTO,
    CCS_CMD_STRCAT,
    CCS_CMD_VSTRCAT,
    CCS_CMD_STRCATTO,
    CCS_CMD_VSTRCATTO,
};

typedef void (*ccs_cmd_call_func)(struct ccs_state, int /*thread*/, void* /*data*/);
struct ccs_cmd_call {
    ccs_cmd_call_func func;
    void* data;
};

enum {
    CCS_CMD_MATH_ADD,
    CCS_CMD_MATH_SUB,
    CCS_CMD_MATH_MUL,
    CCS_CMD_MATH_DIV,
};
struct ccs_cmd_math {
    struct ccs_var* var;
    int op;
    union ccs_num num;
    int numtype;
};
struct ccs_cmd_vmath {
    struct ccs_var* var1;
    int op;
    struct ccs_var* var2;
};
struct ccs_cmd_vmathto {
    struct ccs_var* var1;
    int op;
    struct ccs_var* var2;
    struct ccs_var* out;
};

struct ccs_cmd_strcat {
    struct ccs_var* var;
    struct ccs_str str;
};
struct ccs_cmd_vstrcat {
    struct ccs_var* var1;
    struct ccs_var* var2;
};
struct ccs_cmd_vstrcatto {
    struct ccs_var* var1;
    struct ccs_var* var2;
    struct ccs_var* out;
};

#endif
