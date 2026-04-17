#pragma once

#include <math.h>

namespace pescado_core {

template <typename T>
class Vector3 {
 public:
  T x;
  T y;
  T z;

  Vector3() : x(0), y(0), z(0) {}
  Vector3(T ax, T ay, T az) : x(ax), y(ay), z(az) {}

  T SqrMagnitude() const {
    return x * x + y * y + z * z;
  }

  T Magnitude() const {
    return sqrt(static_cast<float>(SqrMagnitude()));
  }

  void Normalize() {
    const T length = Magnitude();
    if (length != 0) {
      x /= length;
      y /= length;
      z /= length;
    }
  }

  Vector3<T> Normalized() const {
    Vector3<T> result = *this;
    result.Normalize();
    return result;
  }

  Vector3<T> operator+(const Vector3<T>& other) const {
    return Vector3<T>(x + other.x, y + other.y, z + other.z);
  }

  Vector3<T> operator-(const Vector3<T>& other) const {
    return Vector3<T>(x - other.x, y - other.y, z - other.z);
  }

  Vector3<T> operator-() const {
    return Vector3<T>(-x, -y, -z);
  }

  Vector3<T> operator*(const T& scalar) const {
    return Vector3<T>(x * scalar, y * scalar, z * scalar);
  }

  Vector3<T>& operator+=(const Vector3<T>& other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
  }
};

template <typename T>
inline T DotProduct(const Vector3<T>& a, const Vector3<T>& b) {
  return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

template <typename T>
inline Vector3<T> CrossProduct(const Vector3<T>& a, const Vector3<T>& b) {
  return Vector3<T>(
      a.y * b.z - a.z * b.y,
      -(a.x * b.z) + (a.z * b.x),
      a.x * b.y - a.y * b.x);
}

class Matrix3x3 {
 public:
  float m[3][3] = {
      {1, 0, 0},
      {0, 1, 0},
      {0, 0, 1},
  };

  Matrix3x3() {}
  Matrix3x3(float values[3][3]) {
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        m[r][c] = values[r][c];
      }
    }
  }

  static Matrix3x3 Multiply(const Matrix3x3& a, const Matrix3x3& b) {
    Matrix3x3 result;
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        result.m[r][c] = a.m[r][0] * b.m[0][c] +
                         a.m[r][1] * b.m[1][c] +
                         a.m[r][2] * b.m[2][c];
      }
    }
    return result;
  }

  static Matrix3x3 RotX(float theta) {
    const float c = cosf(theta);
    const float s = sinf(theta);
    float values[3][3] = {
        {1, 0, 0},
        {0, c, -s},
        {0, s, c},
    };
    return Matrix3x3(values);
  }

  static Matrix3x3 RotY(float theta) {
    const float c = cosf(theta);
    const float s = sinf(theta);
    float values[3][3] = {
        {c, 0, s},
        {0, 1, 0},
        {-s, 0, c},
    };
    return Matrix3x3(values);
  }

  static Matrix3x3 RotZ(float theta) {
    const float c = cosf(theta);
    const float s = sinf(theta);
    float values[3][3] = {
        {c, -s, 0},
        {s, c, 0},
        {0, 0, 1},
    };
    return Matrix3x3(values);
  }
};

inline Matrix3x3 operator*(const Matrix3x3& a, const Matrix3x3& b) {
  return Matrix3x3::Multiply(a, b);
}

inline Vector3<float> operator*(const Matrix3x3& matrix, const Vector3<float>& v) {
  return Vector3<float>(
      matrix.m[0][0] * v.x + matrix.m[0][1] * v.y + matrix.m[0][2] * v.z,
      matrix.m[1][0] * v.x + matrix.m[1][1] * v.y + matrix.m[1][2] * v.z,
      matrix.m[2][0] * v.x + matrix.m[2][1] * v.y + matrix.m[2][2] * v.z);
}

using Vec3 = Vector3<float>;

}  // namespace pescado_core
