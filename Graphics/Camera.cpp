#include "Camera.h"

#include "../Math/pi.h"

#include "../ThirdParty/glm-0.9.9.8/glm/gtc/matrix_transform.hpp"

#include <cmath>

using namespace Phantom::Math;
using namespace Phantom::Graphics;

Camera::Camera() :
	near_(1.0f),
	far_(10.0f),
	scale(1.0f),
	isOrtho(false)
{
}

Camera::Camera(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up, const float near_, const float far_) :
	near_(near_),
	far_(far_),
	scale(1.0f),
	isOrtho(false)
{
	lookAt(eye, target, up);
}

void Camera::moveEye(const glm::vec3& v)
{
	this->eye += v;
}

void Camera::setEye(const glm::vec3& p)
{
	this->eye = p;
}

void Camera::setTarget(const Vector3df& target)
{
	this->target = target;
}

void Camera::lookAt(const Vector3df& eye, const Vector3df& target, const Vector3df& up)
{
	this->eye = eye;
	this->target = target;
	this->up = up;
}

Matrix4df Camera::getModelMatrix() const
{
	return glm::scale(glm::mat4(1.0f), glm::vec3(scale));
}

Matrix4df Camera::getViewMatrix() const
{
	return glm::lookAt(eye, target, up);
}

Matrix4df Camera::getModelViewMatrix() const
{
	const auto rotation = getRotationMatrix();
	//glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), eye);
	glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
	return rotation * scaleMatrix;
}

Matrix4df Camera::getProjectionMatrix() const
{
	if (isOrtho) {
		return glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, near_, far_);
	}
	else {
		return glm::perspective(1.0f, 0.5f * (float)PI, near_, far_);
	}
}

Vector3df Camera::getRight() const
{
	const auto& f = getForward();
	return glm::cross(up, f);
}

Vector3df Camera::getUp() const
{
	return up;
}

Vector3df Camera::getForward() const
{
	return glm::normalize(target - eye);
}

Matrix4df Camera::getRotationMatrix() const
{
	return glm::lookAt(eye, target, up);
}

void Camera::rotate(const Matrix3df& matrix)
{
	up = matrix * up;
	eye -= target;
	eye = matrix * eye;
	eye += target;
}