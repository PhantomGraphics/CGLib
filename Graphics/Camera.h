#pragma once

#include "../Math/Vector3d.h"
#include "../Math/Matrix3d.h"
#include "../Math/Matrix4d.h"

namespace Phantom {
	namespace Graphics {

/// @brief 3Dカメラの位置・姿勢・投影設定を管理するクラスです。
class Camera
{
public:
	/// @brief デフォルト設定でカメラを初期化します。
	Camera();

	/// @brief 視点・注視点・上方向・クリップ面を指定してカメラを初期化します。
	/// @param eye 視点位置。
	/// @param target 注視点位置。
	/// @param up 上方向ベクトル。
	/// @param near_ ニアクリップ面。
	/// @param far_ ファークリップ面。
	Camera(const Math::Vector3df& eye, const Math::Vector3df& target, const Math::Vector3df& up, const float near_, const float far_);

	/// @brief 直交投影モードに切り替えます。
	void setOrtho() { this->isOrtho = true; }

	/// @brief 透視投影モードに切り替えます。
	void setPerspective() { this->isOrtho = false; }

	/// @brief 視点を相対移動します。
	/// @param v 移動ベクトル。
	void moveEye(const Math::Vector3df& v);

	/// @brief 視点位置を設定します。
	/// @param p 視点位置。
	void setEye(const Math::Vector3df& p);

	/// @brief 注視点を設定します。
	/// @param target 注視点位置。
	void setTarget(const Math::Vector3df& target);

	/// @brief 注視点を取得します。
	/// @return 注視点位置。
	Math::Vector3df getTarget() const { return target; }

	/// @brief 視点・注視点・上方向をまとめて設定します。
	/// @param eye 視点位置。
	/// @param target 注視点位置。
	/// @param up 上方向ベクトル。
	void lookAt(const Math::Vector3df& eye, const Math::Vector3df& target, const Math::Vector3df& up);

	/// @brief 視点位置を取得します。
	/// @return 視点位置。
	Math::Vector3df getEye() const { return eye; }

	/// @brief モデル行列を取得します。
	/// @return モデル行列。
	Math::Matrix4df getModelMatrix() const;

	/// @brief ビュー行列を取得します。
	/// @return ビュー行列。
	Math::Matrix4df getViewMatrix() const;

	/// @brief モデルビュー行列を取得します。
	/// @return モデルビュー行列。
	Math::Matrix4df getModelViewMatrix() const;

	/// @brief ファークリップ面を設定します。
	/// @param f ファー値。
	void setFar(const float f) { this->far_ = f; }

	/// @brief ニアクリップ面を設定します。
	/// @param n ニア値。
	void setNear(const float n) { this->near_ = n; }

	/// @brief ファークリップ面を取得します。
	/// @return ファー値。
	float getFar() const { return far_; }

	/// @brief ニアクリップ面を取得します。
	/// @return ニア値。
	float getNear() const { return near_; }

	/// @brief 投影行列を取得します。
	/// @return 投影行列。
	Math::Matrix4df getProjectionMatrix() const;

	/// @brief ズーム倍率を更新します。
	/// @param s 変化率。
	void zoom(const float s) { this->scale *= (1.0f + s); }

	/// @brief カメラ右方向ベクトルを取得します。
	/// @return 右方向ベクトル。
	Math::Vector3df getRight() const;

	/// @brief カメラ上方向ベクトルを取得します。
	/// @return 上方向ベクトル。
	Math::Vector3df getUp() const;

	/// @brief カメラ前方向ベクトルを取得します。
	/// @return 前方向ベクトル。
	Math::Vector3df getForward() const;

	/// @brief 回転成分の行列を取得します。
	/// @return 回転行列。
	Math::Matrix4df getRotationMatrix() const;

	/// @brief カメラを回転させます。
	/// @param matrix 適用する回転行列。
	void rotate(const Math::Matrix3df& matrix);

private:
	Math::Vector3df eye;
	Math::Vector3df target;
	Math::Vector3df up;
	float near_;
	float far_;
	float scale;
	bool isOrtho;
};

	}
}