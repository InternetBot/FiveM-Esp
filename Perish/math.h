#pragma once
#include <Windows.h>

#include "../../ext/SimpleMath.h"
using namespace DirectX::SimpleMath;

class Vec2 {
public:
	constexpr Vec2(
		const float x = 0.f,
		const float y = 0.f) noexcept :
		x(x), y(y) { }

	float x, y;
};
class Vector
{
public:
	constexpr Vector(
		const float x = 0.f,
		const float y = 0.f,
		const float z = 0.f) noexcept :
		x(x), y(y), z(z) {
	}

	constexpr const Vector& operator-(const Vector& other) const noexcept;
	constexpr const Vector& operator+(const Vector& other) const noexcept;
	constexpr const Vector& operator/(const float factor) const noexcept;
	constexpr const Vector& operator*(const float factor) const noexcept;

	// 3d -> 2d, explanations already exist.
	const bool world_to_screen(const DirectX::SimpleMath::Matrix& view_matrix, Vec2& out);

	const bool IsZero();

	float x, y, z;
};

using Vec3 = Vector;