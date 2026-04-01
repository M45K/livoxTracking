#pragma once

#include <array>
#include <cstdint>

struct Vec3f {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

inline Vec3f operator+(const Vec3f& lhs, const Vec3f& rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

inline Vec3f operator-(const Vec3f& lhs, const Vec3f& rhs) {
  return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

inline Vec3f operator*(const Vec3f& lhs, float scalar) {
  return {lhs.x * scalar, lhs.y * scalar, lhs.z * scalar};
}

inline Vec3f operator/(const Vec3f& lhs, float scalar) {
  return {lhs.x / scalar, lhs.y / scalar, lhs.z / scalar};
}

inline Vec3f& operator+=(Vec3f& lhs, const Vec3f& rhs) {
  lhs.x += rhs.x;
  lhs.y += rhs.y;
  lhs.z += rhs.z;
  return lhs;
}

inline float DistanceSquaredXY(const Vec3f& lhs, const Vec3f& rhs) {
  const float dx = lhs.x - rhs.x;
  const float dy = lhs.y - rhs.y;
  return dx * dx + dy * dy;
}

struct Mat4f {
  std::array<float, 16> data{};

  Mat4f() : data{1.0f, 0.0f, 0.0f, 0.0f,
                 0.0f, 1.0f, 0.0f, 0.0f,
                 0.0f, 0.0f, 1.0f, 0.0f,
                 0.0f, 0.0f, 0.0f, 1.0f} {}

  Vec3f TransformPoint(const Vec3f& point) const {
    return {
        data[0] * point.x + data[1] * point.y + data[2] * point.z + data[3],
        data[4] * point.x + data[5] * point.y + data[6] * point.z + data[7],
        data[8] * point.x + data[9] * point.y + data[10] * point.z + data[11],
    };
  }
};

struct PointXYZI {
  Vec3f position{};
  float intensity = 0.0f;
  std::uint32_t source_index = 0;
};
