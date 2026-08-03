#include "EnjinTest.h"
#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Quaternion.h"

using namespace Enjin;
using namespace Enjin::Math;

// ===========================================================================
// MathScalar
// ===========================================================================

ENJIN_TEST(MathScalar, RadiansDegreesRoundTrip) {
    f32 deg = 90.0f;
    ENJIN_EXPECT_FLOAT_EQ(Degrees(Radians(deg)), deg);
    ENJIN_EXPECT_FLOAT_EQ(Radians(180.0f), PI);
}

ENJIN_TEST(MathScalar, ClampInRange) {
    ENJIN_EXPECT_FLOAT_EQ(Clamp(5.0f, 0.0f, 10.0f), 5.0f);
}

ENJIN_TEST(MathScalar, ClampUnder) {
    ENJIN_EXPECT_FLOAT_EQ(Clamp(-1.0f, 0.0f, 10.0f), 0.0f);
}

ENJIN_TEST(MathScalar, ClampOver) {
    ENJIN_EXPECT_FLOAT_EQ(Clamp(15.0f, 0.0f, 10.0f), 10.0f);
}

ENJIN_TEST(MathScalar, LerpEndpoints) {
    ENJIN_EXPECT_FLOAT_EQ(Lerp(1.0f, 5.0f, 0.0f), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(Lerp(1.0f, 5.0f, 1.0f), 5.0f);
}

ENJIN_TEST(MathScalar, LerpMidpoint) {
    ENJIN_EXPECT_FLOAT_EQ(Lerp(0.0f, 10.0f, 0.5f), 5.0f);
}

ENJIN_TEST(MathScalar, SmoothStep) {
    ENJIN_EXPECT_FLOAT_EQ(SmoothStep(0.0f, 1.0f, 0.0f), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(SmoothStep(0.0f, 1.0f, 1.0f), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(SmoothStep(0.0f, 1.0f, 0.5f), 0.5f);
}

ENJIN_TEST(MathScalar, InverseLerp) {
    ENJIN_EXPECT_FLOAT_EQ(InverseLerp(0.0f, 10.0f, 5.0f), 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(InverseLerp(0.0f, 10.0f, 0.0f), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(InverseLerp(0.0f, 10.0f, 10.0f), 1.0f);
}

ENJIN_TEST(MathScalar, Remap) {
    ENJIN_EXPECT_FLOAT_EQ(Remap(5.0f, 0.0f, 10.0f, 0.0f, 100.0f), 50.0f);
}

ENJIN_TEST(MathScalar, MoveTowards) {
    ENJIN_EXPECT_FLOAT_EQ(MoveTowards(0.0f, 10.0f, 3.0f), 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(MoveTowards(8.0f, 10.0f, 5.0f), 10.0f);
}

ENJIN_TEST(MathScalar, NormalizeAngle) {
    ENJIN_EXPECT_FLOAT_EQ(NormalizeAngle(0.0f), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(NormalizeAngle(360.0f), 0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(NormalizeAngle(270.0f), -90.0f, 0.01f);
}

ENJIN_TEST(MathScalar, MinMax) {
    ENJIN_EXPECT_FLOAT_EQ(Min(3.0f, 7.0f), 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(Max(3.0f, 7.0f), 7.0f);
}

ENJIN_TEST(MathScalar, Sign) {
    ENJIN_EXPECT_FLOAT_EQ(Sign(5.0f), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(Sign(-3.0f), -1.0f);
    ENJIN_EXPECT_FLOAT_EQ(Sign(0.0f), 0.0f);
}

ENJIN_TEST(MathScalar, IsNearZero) {
    ENJIN_EXPECT_TRUE(IsNearZero(0.0f));
    ENJIN_EXPECT_FALSE(IsNearZero(1.0f));
}

// ===========================================================================
// Vector2
// ===========================================================================

ENJIN_TEST(Vector2, DefaultConstruction) {
    Vector2 v;
    ENJIN_EXPECT_VEC2_EQ(v, 0.0f, 0.0f);
}

ENJIN_TEST(Vector2, ExplicitConstruction) {
    Vector2 v(3.0f, 4.0f);
    ENJIN_EXPECT_VEC2_EQ(v, 3.0f, 4.0f);
}

ENJIN_TEST(Vector2, ScalarConstruction) {
    Vector2 v(5.0f);
    ENJIN_EXPECT_VEC2_EQ(v, 5.0f, 5.0f);
}

ENJIN_TEST(Vector2, Addition) {
    Vector2 a(1.0f, 2.0f), b(3.0f, 4.0f);
    Vector2 c = a + b;
    ENJIN_EXPECT_VEC2_EQ(c, 4.0f, 6.0f);
}

ENJIN_TEST(Vector2, Subtraction) {
    Vector2 a(5.0f, 7.0f), b(2.0f, 3.0f);
    Vector2 c = a - b;
    ENJIN_EXPECT_VEC2_EQ(c, 3.0f, 4.0f);
}

ENJIN_TEST(Vector2, ScalarMultiply) {
    Vector2 v(2.0f, 3.0f);
    Vector2 r = v * 2.0f;
    ENJIN_EXPECT_VEC2_EQ(r, 4.0f, 6.0f);
}

ENJIN_TEST(Vector2, ScalarDivide) {
    Vector2 v(6.0f, 8.0f);
    Vector2 r = v / 2.0f;
    ENJIN_EXPECT_VEC2_EQ(r, 3.0f, 4.0f);
}

ENJIN_TEST(Vector2, Length) {
    Vector2 v(3.0f, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.Length(), 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.LengthSquared(), 25.0f);
}

ENJIN_TEST(Vector2, Dot) {
    Vector2 a(1.0f, 0.0f), b(0.0f, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(a.Dot(b), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(a.Dot(a), 1.0f);
}

ENJIN_TEST(Vector2, Normalize) {
    Vector2 v(3.0f, 4.0f);
    Vector2 n = v.Normalized();
    ENJIN_EXPECT_FLOAT_NEAR(n.Length(), 1.0f, 0.001f);
}

ENJIN_TEST(Vector2, CompoundAssignment) {
    Vector2 v(1.0f, 2.0f);
    v += Vector2(3.0f, 4.0f);
    ENJIN_EXPECT_VEC2_EQ(v, 4.0f, 6.0f);
    v *= 2.0f;
    ENJIN_EXPECT_VEC2_EQ(v, 8.0f, 12.0f);
}

ENJIN_TEST(Vector2, Equality) {
    Vector2 a(1.0f, 2.0f), b(1.0f, 2.0f), c(1.0f, 3.0f);
    ENJIN_EXPECT_TRUE(a == b);
    ENJIN_EXPECT_TRUE(a != c);
}

// ===========================================================================
// Vector3
// ===========================================================================

ENJIN_TEST(Vector3, DefaultConstruction) {
    Vector3 v;
    ENJIN_EXPECT_VEC3_EQ(v, 0.0f, 0.0f, 0.0f);
}

ENJIN_TEST(Vector3, ExplicitConstruction) {
    Vector3 v(1.0f, 2.0f, 3.0f);
    ENJIN_EXPECT_VEC3_EQ(v, 1.0f, 2.0f, 3.0f);
}

ENJIN_TEST(Vector3, ScalarConstruction) {
    Vector3 v(5.0f);
    ENJIN_EXPECT_VEC3_EQ(v, 5.0f, 5.0f, 5.0f);
}

ENJIN_TEST(Vector3, Addition) {
    Vector3 a(1.0f, 2.0f, 3.0f), b(4.0f, 5.0f, 6.0f);
    Vector3 c = a + b;
    ENJIN_EXPECT_VEC3_EQ(c, 5.0f, 7.0f, 9.0f);
}

ENJIN_TEST(Vector3, Subtraction) {
    Vector3 a(5.0f, 7.0f, 9.0f), b(1.0f, 2.0f, 3.0f);
    Vector3 c = a - b;
    ENJIN_EXPECT_VEC3_EQ(c, 4.0f, 5.0f, 6.0f);
}

ENJIN_TEST(Vector3, ScalarMultiply) {
    Vector3 v(1.0f, 2.0f, 3.0f);
    Vector3 r = v * 3.0f;
    ENJIN_EXPECT_VEC3_EQ(r, 3.0f, 6.0f, 9.0f);
}

ENJIN_TEST(Vector3, ScalarDivide) {
    Vector3 v(6.0f, 9.0f, 12.0f);
    Vector3 r = v / 3.0f;
    ENJIN_EXPECT_VEC3_EQ(r, 2.0f, 3.0f, 4.0f);
}

ENJIN_TEST(Vector3, Length) {
    Vector3 v(1.0f, 2.0f, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.Length(), 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(v.LengthSquared(), 9.0f);
}

ENJIN_TEST(Vector3, Dot) {
    Vector3 x(1.0f, 0.0f, 0.0f), y(0.0f, 1.0f, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(x.Dot(y), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(x.Dot(x), 1.0f);
}

ENJIN_TEST(Vector3, Cross) {
    Vector3 x(1.0f, 0.0f, 0.0f), y(0.0f, 1.0f, 0.0f);
    Vector3 z = x.Cross(y);
    ENJIN_EXPECT_VEC3_EQ(z, 0.0f, 0.0f, 1.0f);
}

ENJIN_TEST(Vector3, Normalize) {
    Vector3 v(3.0f, 0.0f, 4.0f);
    Vector3 n = v.Normalized();
    ENJIN_EXPECT_FLOAT_NEAR(n.Length(), 1.0f, 0.001f);
}

ENJIN_TEST(Vector3, NormalizeZero) {
    Vector3 v(0.0f, 0.0f, 0.0f);
    Vector3 n = v.Normalized();
    ENJIN_EXPECT_VEC3_EQ(n, 0.0f, 0.0f, 0.0f);
}

ENJIN_TEST(Vector3, Negation) {
    Vector3 v(1.0f, -2.0f, 3.0f);
    Vector3 n = -v;
    ENJIN_EXPECT_VEC3_EQ(n, -1.0f, 2.0f, -3.0f);
}

// ===========================================================================
// Vector4
// ===========================================================================

ENJIN_TEST(Vector4, DefaultConstruction) {
    Vector4 v;
    ENJIN_EXPECT_VEC4_EQ(v, 0.0f, 0.0f, 0.0f, 0.0f);
}

ENJIN_TEST(Vector4, ExplicitConstruction) {
    Vector4 v(1.0f, 2.0f, 3.0f, 4.0f);
    ENJIN_EXPECT_VEC4_EQ(v, 1.0f, 2.0f, 3.0f, 4.0f);
}

ENJIN_TEST(Vector4, FromVector3) {
    Vector3 v3(1.0f, 2.0f, 3.0f);
    Vector4 v4(v3, 1.0f);
    ENJIN_EXPECT_VEC4_EQ(v4, 1.0f, 2.0f, 3.0f, 1.0f);
}

ENJIN_TEST(Vector4, Dot) {
    Vector4 a(1.0f, 0.0f, 0.0f, 0.0f), b(0.0f, 1.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(a.Dot(b), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(a.Dot(a), 1.0f);
}

// ===========================================================================
// Matrix4
// ===========================================================================

ENJIN_TEST(Matrix4, Identity) {
    Matrix4 m = Matrix4::Identity();
    ENJIN_EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m(1, 1), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m(3, 3), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(m(0, 1), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(m(1, 0), 0.0f);
}

ENJIN_TEST(Matrix4, Translation) {
    Matrix4 t = Matrix4::Translation(Vector3(3.0f, 4.0f, 5.0f));
    Vector4 p(0.0f, 0.0f, 0.0f, 1.0f);
    Vector4 result = t * p;
    ENJIN_EXPECT_FLOAT_EQ(result.x, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(result.y, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(result.z, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(result.w, 1.0f);
}

ENJIN_TEST(Matrix4, Scale) {
    Matrix4 s = Matrix4::Scale(Vector3(2.0f, 3.0f, 4.0f));
    Vector4 p(1.0f, 1.0f, 1.0f, 1.0f);
    Vector4 result = s * p;
    ENJIN_EXPECT_FLOAT_EQ(result.x, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(result.y, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(result.z, 4.0f);
}

ENJIN_TEST(Matrix4, MultiplyIdentity) {
    Matrix4 a = Matrix4::Translation(Vector3(1.0f, 2.0f, 3.0f));
    Matrix4 i = Matrix4::Identity();
    Matrix4 result = a * i;
    for (int j = 0; j < 16; j++) {
        ENJIN_EXPECT_FLOAT_NEAR(result.Data()[j], a.Data()[j], 0.001f);
    }
}

ENJIN_TEST(Matrix4, InverseIdentity) {
    Matrix4 m = Matrix4::Identity();
    Matrix4 inv = m.Inverse();
    for (int j = 0; j < 16; j++) {
        ENJIN_EXPECT_FLOAT_NEAR(inv.Data()[j], m.Data()[j], 0.001f);
    }
}

ENJIN_TEST(Matrix4, InverseRoundTrip) {
    Matrix4 t = Matrix4::Translation(Vector3(5.0f, -3.0f, 7.0f));
    Matrix4 inv = t.Inverse();
    Matrix4 result = t * inv;
    Matrix4 id = Matrix4::Identity();
    for (int j = 0; j < 16; j++) {
        ENJIN_EXPECT_FLOAT_NEAR(result.Data()[j], id.Data()[j], 0.001f);
    }
}

ENJIN_TEST(Matrix4, Transpose) {
    Matrix4 m = Matrix4::Identity();
    // Set an off-diagonal element
    m(0, 1) = 5.0f;
    Matrix4 t = m.Transposed();
    ENJIN_EXPECT_FLOAT_EQ(t(1, 0), 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(t(0, 1), 0.0f);
}

// ===========================================================================
// Quaternion
// ===========================================================================

ENJIN_TEST(Quaternion, Identity) {
    Quaternion q = Quaternion::Identity();
    ENJIN_EXPECT_FLOAT_EQ(q.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(q.y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(q.z, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(q.w, 1.0f);
}

ENJIN_TEST(Quaternion, Normalize) {
    Quaternion q(1.0f, 2.0f, 3.0f, 4.0f);
    Quaternion n = q.Normalized();
    ENJIN_EXPECT_FLOAT_NEAR(n.Length(), 1.0f, 0.001f);
}

ENJIN_TEST(Quaternion, FromEulerToEulerRoundTrip) {
    Vector3 euler(Radians(30.0f), Radians(45.0f), Radians(60.0f));
    Quaternion q = Quaternion::FromEuler(euler);
    Vector3 result = q.ToEuler();
    ENJIN_EXPECT_FLOAT_NEAR(result.x, euler.x, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(result.y, euler.y, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(result.z, euler.z, 0.01f);
}

ENJIN_TEST(Quaternion, RotateVector90Y) {
    // 90-degree rotation around Y axis: X(1,0,0) -> Z(0,0,-1) (right-hand rule)
    Quaternion q(Vector3(0.0f, 1.0f, 0.0f), Radians(90.0f));
    Vector3 v(1.0f, 0.0f, 0.0f);
    Vector3 result = q.Rotate(v);
    ENJIN_EXPECT_FLOAT_NEAR(result.x, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(result.y, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(result.z, -1.0f, 0.01f);
}

ENJIN_TEST(Quaternion, SlerpEndpoints) {
    Quaternion a = Quaternion::Identity();
    Quaternion b = Quaternion(Vector3(0.0f, 1.0f, 0.0f), Radians(90.0f));
    Quaternion start = Quaternion::Slerp(a, b, 0.0f);
    Quaternion end = Quaternion::Slerp(a, b, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(start.x, a.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(start.y, a.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(start.z, a.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(start.w, a.w, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(end.x, b.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(end.y, b.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(end.z, b.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(end.w, b.w, 0.001f);
}

ENJIN_TEST(Quaternion, ConjugateTimesOriginal) {
    Quaternion q(Vector3(0.0f, 1.0f, 0.0f), Radians(45.0f));
    Quaternion conj = q.Conjugate();
    Quaternion product = q * conj;
    ENJIN_EXPECT_FLOAT_NEAR(product.x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(product.y, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(product.z, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(product.w, 1.0f, 0.001f);
}

// Regression: FromMatrix must return the SAME rotation that produced the
// matrix, not its conjugate/inverse. Round-tripping an asymmetric rotation
// through ToMatrix -> FromMatrix and applying it to a vector must match the
// original rotation applied to that vector. The pre-fix code extracted the
// transposed antisymmetric terms and returned the inverse rotation, which
// this test would have caught (rotated vector would come out mirrored).
ENJIN_TEST(Quaternion, FromMatrixRoundTripPreservesRotation) {
    // Arrange: an asymmetric rotation (skew axis, non-trivial angle)
    Quaternion q = Quaternion(Vector3(0.3f, 0.7f, -0.5f), Radians(50.0f)).Normalized();
    Vector3 v(0.4f, -0.2f, 0.9f);
    Vector3 expected = q.Rotate(v);

    // Act: round-trip through the matrix and re-apply
    Matrix4 m = q.ToMatrix();
    Quaternion r = Quaternion::FromMatrix(m);
    Vector3 got = r.Rotate(v);

    // Assert: same rotation action (±q both rotate identically)
    ENJIN_EXPECT_FLOAT_NEAR(got.x, expected.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(got.y, expected.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(got.z, expected.z, 0.001f);
}

// Sign anchor: a hand-built 90-degrees-about-+Y matrix (active rotation,
// column-major) must extract to a quaternion that maps +X to -Z, NOT +Z.
// The pre-fix (conjugate) code would have mapped +X to +Z here.
ENJIN_TEST(Quaternion, FromMatrixSignMatchesActiveRotation) {
    // Arrange: 90deg about +Y. Columns are the rotated basis axes.
    //   +X (1,0,0) -> (0,0,-1);  +Y unchanged;  +Z (0,0,1) -> (1,0,0)
    Matrix4 m; // starts as identity
    m.m[0] = 0.0f; m.m[1] = 0.0f; m.m[2] = -1.0f; // column 0
    m.m[4] = 0.0f; m.m[5] = 1.0f; m.m[6] = 0.0f;  // column 1
    m.m[8] = 1.0f; m.m[9] = 0.0f; m.m[10] = 0.0f; // column 2

    // Act
    Quaternion q = Quaternion::FromMatrix(m);
    Vector3 rotatedX = q.Rotate(Vector3(1.0f, 0.0f, 0.0f));

    // Assert: +X rotates to -Z (right-hand rule), proving no conjugation
    ENJIN_EXPECT_FLOAT_NEAR(rotatedX.x, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(rotatedX.y, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(rotatedX.z, -1.0f, 0.01f);
}

ENJIN_TEST(Quaternion, GetForwardRightUpIdentity) {
    Quaternion q = Quaternion::Identity();
    Vector3 fwd = q.GetForward();
    Vector3 right = q.GetRight();
    Vector3 up = q.GetUp();
    ENJIN_EXPECT_FLOAT_NEAR(fwd.x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(fwd.y, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(fwd.z, -1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(right.x, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(right.y, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(right.z, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(up.x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(up.y, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(up.z, 0.0f, 0.001f);
}

ENJIN_TEST_MAIN()
