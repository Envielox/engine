#include "geom.h"

#include <math.h>

Point3f vectMul(Point3f a, Point3f b)
{
    return (Point3f) { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

Point3f vectMulScalar(Point3f a, Point3f b, float c)
{
    return (Point3f) { a.x + b.x * c, a.y + b.y * c, a.z + b.z * c };
}

Point3f vectSum(Point3f a, Point3f b, Point3f c)
{
    return (Point3f) { a.x + b.x + c.x, a.y + b.y + c.y, a.z + b.z + c.z };
}

Point3f vectDiv(Point3f a, float c)
{
    return (Point3f) { a.x / c, a.y / c, a.z / c };
}

Point3f vectScale(Point3f a, Point3f b)
{
    return (Point3f) { a.x * b.x, a.y * b.y, a.z * b.z };
}

Point3f vectNormalize(Point3f a)
{
    float len = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
    return vectDiv(a, len);
}

Point3f multiplyVectMatrix(Point3f v, Matrix3f m)
{
    Point3f res;
    res.x = v.x * m.v[0][0] + v.y * m.v[1][0] + v.z * m.v[2][0];
    res.y = v.x * m.v[0][1] + v.y * m.v[1][1] + v.z * m.v[2][1];
    res.z = v.x * m.v[0][2] + v.y * m.v[1][2] + v.z * m.v[2][2];
    return res;
}

Matrix3f createRotationMatrix(Point3f axis, float angle)
{
    Matrix3f ret;
    float l = axis.x;
    float m = axis.y;
    float n = axis.z;
    float c = cosf(angle);
    float s = sinf(angle);
    float c_ = 1 - c;

    ret.v[0][0] = l * l * c_ + c;
    ret.v[0][1] = m * l * c_ - n * s;
    ret.v[0][2] = n * l * c_ + m * s;

    ret.v[1][0] = l * m * c_ + n * s;
    ret.v[1][1] = m * m * c_ + c;
    ret.v[1][2] = n * m * c_ - l * s;

    ret.v[2][0] = l * n * c_ - m * s;
    ret.v[2][1] = m * n * c_ + l * s;
    ret.v[2][2] = n * n * c_ + c;

    return ret;
}


int cmpDistData(const void * a, const void * b)
{
    return (*(struct dist_data*)a).dist < (*(struct dist_data*)b).dist ? -1 : 1;
}

const float eps = 0.000001f;
static int epsEq(const float a, const float b)
{
    return fabsf(a - b) < eps; // Not necessarily great numerically
}
static int epsGteF(const float a, const float b)
{
    return a > b || epsEq(a, b); // Not necessarily great numerically
}
static int epsLteF(const float a, const float b)
{
    return b > a || epsEq(a, b); // Not necessarily great numerically
}
static int epsGteP(const Point3f a, const Point3f b)
{
    return epsGteF(a.x, b.x) && epsGteF(a.y, b.y) && epsGteF(a.z, b.z);
}
static int epsLteP(const Point3f a, const Point3f b)
{
    return epsLteF(a.x, b.x) && epsLteF(a.y, b.y) && epsLteF(a.z, b.z);
}

// check intersection of vector [origin; direction] with plane limited
// by value and versor plane (1 on planeth coordinate, 0 on the rest)
int vectPlaneIntersection(Point3f origin, Point3f direction, int plane, Point3f value)
{
    float dist;
    float* origin_a = (float*)(&origin);
    float* value_a = (float*)(&value);
    float* direction_a = (float*)(&direction);

    if (plane < 0 || plane >= 3) return 0;

    if (direction_a[plane] == 0
        || (origin_a[plane] < value_a[plane] && direction_a[plane] < 0)
        || (origin_a[plane] > value_a[plane] && direction_a[plane] > 0))
        return 0;
    dist = (value_a[plane] - origin_a[plane]) / direction_a[plane];

    Point3f intersect = vectMulScalar(origin, direction, dist);
    float* intersect_a = (float*)(&intersect);

    float i_1, v_1, i_2, v_2;
    i_1 = intersect_a[(plane + 1) % 3];
    v_1 = value_a[(plane + 1) % 3];
    i_2 = intersect_a[(plane + 2) % 3];
    v_2 = value_a[(plane + 2) % 3];

    if (v_1 < 0) {
        v_1 *= -1.0f;
        i_1 *= -1.0f;
    }
    if (v_2 < 0) {
        v_2 *= -1.0f;
        i_2 *= -1.0f;
    }
    return epsGteF(i_1, 0) && epsLteF(i_1, v_1) && epsGteF(i_2, 0) && epsLteF(i_2, v_1);
}

float MAX(float a, float b) {
    return a > b ? a : b;
}

void clamp(Point3f *p, float radius) {
    if (p->x < -radius)
        p->x = -radius;
    else if (p->x > radius)
        p->x = radius;
    if (p->y < -radius)
        p->y = -radius;
    else if (p->y > radius)
        p->y = radius;
    if (p->z < -radius)
        p->z = -radius;
    else if (p->z > radius)
        p->z = radius;
}
