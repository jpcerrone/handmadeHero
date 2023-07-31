#pragma once

struct Vector2 {
	float x;
	float y;
};

Vector2 operator+(const Vector2 &A, const Vector2 &B) {
	return { A.x + B.x, A.y + B.y };
}

Vector2 operator-(const Vector2& A, const Vector2& B) {
	return { A.x - B.x, A.y - B.y };
}

Vector2 operator*(const Vector2& A, const float scalar) {
	return { A.x * scalar, A.y * scalar };
}

Vector2 operator*(const float scalar, const Vector2& A) {
	return A * scalar;
}

Vector2 operator/(const Vector2& A, const float scalar) {
	return { A.x / scalar, A.y / scalar };
}

Vector2 operator/(const float scalar, const Vector2& A) {
	return A / scalar;
}

bool operator==(Vector2 &A, Vector2 B) {
	return (A.x == B.x) && (A.y == B.y);
}

Vector2 & operator+=(Vector2& A, Vector2 B) {
	A = A + B;
	return A;
}

Vector2& operator/=(Vector2& A, const float scalar) {
	A = A / scalar;
	return A;
}

//

float square(float value) {
	return (value * value);
}