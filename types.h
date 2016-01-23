#pragma once

#define ARR_IDX(x, y, col) ((((y) * width + (x)) * 3) + (col))
#define ARR_IDX4(x, y, col) ((((y) * width + (x)) * 4) + (col))

typedef struct Point3f {
    float x;
    float y;
    float z;
} Point3f;

typedef struct Color4f {
    float r;
    float g;
    float b;
    float a;
} Color4f;

//enum OctTreeNodeType { Empty, Solid, Partial };
#define Empty 0
#define Solid 1
#define Partial 2

typedef struct OctTreeNode {
    char x, y, z;
    char type;
    int parent;
    union {
        Color4f color;
        int nodes[2][2][2];
    };
} OctTreeNode;

struct dist_data {
    float dist;
    int plane;
};

typedef enum RenderMethod { Stacking, Stackless, TracerCL } RenderMethod;
