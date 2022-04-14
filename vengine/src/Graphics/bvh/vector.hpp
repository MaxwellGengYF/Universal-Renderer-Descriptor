#ifndef BVH_VECTOR_HPP
#define BVH_VECTOR_HPP

#include <cstddef>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

#include <Graphics/bvh/platform.hpp>
#include <Eigen/Eigen>
namespace bvh {

template<typename, size_t>
struct Vector;

/// Helper class to set the elements of a vector.
template<size_t I, typename Scalar, size_t N>
struct VectorSetter {
	template<typename... Args>
	bvh_always_inline static void set(Vector<Scalar, N>& v, Scalar s, Args... args) {
		v[I] = s;
		VectorSetter<I + 1, Scalar, N>::set(v, args...);
	}
};

template<typename Scalar, size_t N>
struct VectorSetter<N, Scalar, N> {
	bvh_always_inline static void set(Vector<Scalar, N>&) {}
};
/// An N-dimensional vector class.
template<typename Scalar, size_t N>
struct Vector {
	Eigen::Matrix<Scalar, N, 1> value;

	Vector() = default;
	bvh_always_inline Vector(Scalar s) {
		value.fill(s);
	}
	bvh_always_inline Vector(Eigen::Matrix<Scalar, N, 1> const& value)
		: value(value) {}
	template<size_t M, std::enable_if_t<(M > N), int> = 0>
	bvh_always_inline explicit Vector(const Vector<Scalar, M>& other) {
		memcpy(&value, &other, sizeof(Vector<Scalar, M>));
	}

	template<typename... Args>
	bvh_always_inline Vector(Scalar first, Scalar second, Args... args) {
		value = {first, second, args...};
	}

	template<typename... Args>
	bvh_always_inline void set(Args... args) {
		value = {std::forward<Args>(args)...};
	}

	bvh_always_inline Vector operator-() const {
		return -value;
	}

	bvh_always_inline Vector inverse() const {
		return 1.0 / value;
	}

	bvh_always_inline Vector& operator+=(const Vector& other) {
		return *this = *this + other;
	}

	bvh_always_inline Vector& operator-=(const Vector& other) {
		return *this = *this - other;
	}

	bvh_always_inline Vector& operator*=(const Vector& other) {
		return *this = *this * other;
	}

	bvh_always_inline Scalar& operator[](size_t i) { return value[i]; }
	bvh_always_inline Scalar operator[](size_t i) const { return value[i]; }
	Vector operator+(Vector const& b) const {
		return {value + b.value};
	}
	Vector operator-(Vector const& b) const {
		return {value - b.value};
	}
	Vector operator*(Vector const& b) const {
		return {value.array() * b.value.array()};
	}
	Vector operator/(Vector const& b) const {
		return {value / b.value};
	}
};
template<typename Scalar, size_t N>
bvh_always_inline inline Vector<Scalar, N> min(const Vector<Scalar, N>& a, const Vector<Scalar, N>& b) {
	return {a.value.cwiseMin(b.value)};
}

template<typename Scalar, size_t N>
bvh_always_inline inline Vector<Scalar, N> max(const Vector<Scalar, N>& a, const Vector<Scalar, N>& b) {
	return {a.value.cwiseMax(b.value)};
}

template<typename Scalar, size_t N>
bvh_always_inline inline Scalar dot(const Vector<Scalar, N>& a, const Vector<Scalar, N>& b) {
	return a.value.dot(b.value);
}

template<typename Scalar, size_t N>
bvh_always_inline inline Scalar length(const Vector<Scalar, N>& v) {
	return std::sqrt(a.value.dot(b.value));
}

template<typename Scalar, size_t N>
bvh_always_inline inline Vector<Scalar, N> normalize(const Vector<Scalar, N>& v) {
	return v.value / length(v);
}

template<typename Scalar>
using Vector3 = Vector<Scalar, 3>;

template<typename Scalar>
bvh_always_inline inline Vector3<Scalar> cross(const Vector3<Scalar>& a, const Vector3<Scalar>& b) {
	return a.value.cross(b.value);
}

template<typename Scalar>
bvh_always_inline inline Vector3<Scalar> abs(const Vector3<Scalar>& a) {
	return {a.value.cwiseAbs()};
}

}// namespace bvh
#endif
