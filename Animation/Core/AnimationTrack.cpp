#include "AnimationTrack.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace Phantom::Animation {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

using Quat = std::array<double, 4>;  // w, x, y, z

Quat mul(const Quat& a, const Quat& b) {
	return {
		a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
		a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
		a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
		a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0],
	};
}

Quat normalized(Quat q) {
	const double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
	if (n < 1e-12) return { 1.0, 0.0, 0.0, 0.0 };
	for (double& c : q) c /= n;
	return q;
}

// Shortest-path spherical interpolation.
Quat slerp(Quat a, Quat b, double t) {
	double d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
	if (d < 0.0) { for (double& c : b) c = -c; d = -d; }
	if (d > 0.9995) {
		Quat r;
		for (int i = 0; i < 4; ++i) r[i] = a[i] + (b[i] - a[i]) * t;
		return normalized(r);
	}
	const double theta = std::acos(std::min(1.0, d));
	const double s = std::sin(theta);
	const double wa = std::sin((1.0 - t) * theta) / s;
	const double wb = std::sin(t * theta) / s;
	Quat r;
	for (int i = 0; i < 4; ++i) r[i] = a[i] * wa + b[i] * wb;
	return normalized(r);
}

float lerpf(float a, float b, double t) { return static_cast<float>(a + (b - a) * t); }

// Index of the first key whose frame is greater than `frame`.
size_t upperIndex(const std::vector<TrackKey>& keys, double frame) {
	return static_cast<size_t>(std::upper_bound(keys.begin(), keys.end(), frame,
		[](double f, const TrackKey& k) { return f < static_cast<double>(k.frame); }) - keys.begin());
}

void evaluateChannel(const std::vector<TrackKey>& keys, double frame, bool rotationQuat,
                     std::array<float, 3>& out) {
	if (keys.empty()) return;
	if (frame <= keys.front().frame || keys.size() == 1) { out = keys.front().value; return; }
	if (frame >= keys.back().frame) { out = keys.back().value; return; }
	const size_t hi = upperIndex(keys, frame);
	const TrackKey& k0 = keys[hi - 1];
	const TrackKey& k1 = keys[hi];
	if (k0.interp == KeyInterp::Step) { out = k0.value; return; }
	const double u = (frame - k0.frame) / static_cast<double>(k1.frame - k0.frame);
	const double t = k0.interp == KeyInterp::Bezier ? bezierTime(k0.bezier, u) : easeTime(k0.interp, u);
	if (t <= 0.0) { out = k0.value; return; }
	if (rotationQuat) {
		out = quaternionToEuler(slerp(eulerToQuaternion(k0.value), eulerToQuaternion(k1.value), t));
		return;
	}
	for (int i = 0; i < 3; ++i) out[i] = lerpf(k0.value[i], k1.value[i], t);
}

} // namespace

const char* interpName(KeyInterp i) {
	switch (i) {
	case KeyInterp::Step: return "step";
	case KeyInterp::Linear: return "linear";
	case KeyInterp::EaseIn: return "easein";
	case KeyInterp::EaseOut: return "easeout";
	case KeyInterp::EaseInOut: return "easeinout";
	case KeyInterp::Bezier: return "bezier";
	}
	return "linear";
}

bool parseInterpName(const std::string& name, KeyInterp& out) {
	std::string s;
	for (char ch : name)
		if (ch != '-' && ch != '_') s += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	for (KeyInterp i : { KeyInterp::Step, KeyInterp::Linear, KeyInterp::EaseIn, KeyInterp::EaseOut, KeyInterp::EaseInOut, KeyInterp::Bezier })
		if (s == interpName(i)) { out = i; return true; }
	return false;
}

double easeTime(KeyInterp i, double t) {
	switch (i) {
	case KeyInterp::EaseIn: return t * t;
	case KeyInterp::EaseOut: return 1.0 - (1.0 - t) * (1.0 - t);
	case KeyInterp::EaseInOut: return t * t * (3.0 - 2.0 * t);
	case KeyInterp::Bezier: return bezierTime(kDefaultBezier, t);
	default: return t;
	}
}

bool validBezier(const BezierHandles& h) {
	for (float v : h) if (!std::isfinite(v)) return false;
	return h[0] >= 0.f && h[0] <= 1.f && h[2] >= 0.f && h[2] <= 1.f;
}

double bezierTime(const BezierHandles& h, double t) {
	if (t <= 0.0) return 0.0;
	if (t >= 1.0) return 1.0;
	const double x1 = h[0], y1 = h[1], x2 = h[2], y2 = h[3];
	// B(s) = 3(1-s)^2 s p1 + 3(1-s) s^2 p2 + s^3 with p0 = 0 and p3 = 1. x is monotonic for x1, x2 in [0,1]: bisect for s.
	const auto coord = [](double p1, double p2, double s) {
		const double m = 1.0 - s;
		return 3.0 * m * m * s * p1 + 3.0 * m * s * s * p2 + s * s * s;
	};
	double lo = 0.0, hi = 1.0, s = t;
	for (int i = 0; i < 40; ++i) {
		s = 0.5 * (lo + hi);
		if (coord(x1, x2, s) < t) lo = s; else hi = s;
	}
	return coord(y1, y2, s);
}

bool AnimTracks::empty() const {
	for (const auto& c : channels) if (!c.empty()) return false;
	return true;
}

int AnimTracks::keyCount() const {
	int n = 0;
	for (const auto& c : channels) n += static_cast<int>(c.size());
	return n;
}

std::vector<int> AnimTracks::frames() const {
	std::set<int> all;
	for (const auto& c : channels) for (const auto& k : c) all.insert(k.frame);
	return { all.begin(), all.end() };
}

const TrackKey* AnimTracks::find(AnimChannel c, int frame) const {
	for (const auto& k : track(c)) if (k.frame == frame) return &k;
	return nullptr;
}

const char* channelName(AnimChannel c) {
	switch (c) {
	case AnimChannel::Translation: return "translation";
	case AnimChannel::Rotation: return "rotation";
	case AnimChannel::Scale: return "scale";
	case AnimChannel::Fov: return "fov";
	}
	return "";
}

bool parseChannelName(const std::string& name, AnimChannel& out) {
	std::string s;
	for (char ch : name) s += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	if (s == "translation" || s == "move" || s == "position" || s == "location") out = AnimChannel::Translation;
	else if (s == "rotation" || s == "rotate") out = AnimChannel::Rotation;
	else if (s == "scale") out = AnimChannel::Scale;
	else if (s == "fov") out = AnimChannel::Fov;
	else return false;
	return true;
}

bool parseChannelMask(const std::string& text, unsigned& mask, std::string* error) {
	mask = 0;
	size_t pos = 0;
	while (pos <= text.size()) {
		size_t end = text.find(',', pos);
		if (end == std::string::npos) end = text.size();
		const std::string item = text.substr(pos, end - pos);
		std::string lowered;
		for (char ch : item) lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		if (lowered == "all") mask |= kAllChannels;
		else if (lowered == "transform") mask |= kTransformChannels;
		else {
			AnimChannel c;
			if (!parseChannelName(item, c)) {
				if (error) *error = "unknown channel '" + item + "' (translation, rotation, scale, fov, transform, all)";
				return false;
			}
			mask |= channelBit(c);
		}
		pos = end + 1;
	}
	if (mask == 0) { if (error) *error = "no channel given"; return false; }
	return true;
}

std::array<double, 4> eulerToQuaternion(const std::array<float, 3>& e) {
	// R = Ry * Rx * Rz, the same order as the model matrix (eulerAngleYXZ).
	const double hx = e[0] * kDegToRad * 0.5, hy = e[1] * kDegToRad * 0.5, hz = e[2] * kDegToRad * 0.5;
	const Quat qx = { std::cos(hx), std::sin(hx), 0.0, 0.0 };
	const Quat qy = { std::cos(hy), 0.0, std::sin(hy), 0.0 };
	const Quat qz = { std::cos(hz), 0.0, 0.0, std::sin(hz) };
	return normalized(mul(mul(qy, qx), qz));
}

std::array<float, 3> quaternionToEuler(const std::array<double, 4>& qin) {
	const Quat q = normalized(qin);
	const double w = q[0], x = q[1], y = q[2], z = q[3];
	// Rotation matrix elements needed for the YXZ decomposition.
	const double r12 = 2.0 * (y * z - w * x);
	const double r02 = 2.0 * (x * z + w * y);
	const double r22 = 1.0 - 2.0 * (x * x + y * y);
	const double r10 = 2.0 * (x * y + w * z);
	const double r11 = 1.0 - 2.0 * (x * x + z * z);
	double ex, ey, ez;
	ex = std::asin(std::max(-1.0, std::min(1.0, -r12)));
	if (std::abs(r12) < 0.999999) {
		ey = std::atan2(r02, r22);
		ez = std::atan2(r10, r11);
	} else {  // gimbal lock: fold the roll into yaw
		const double r20 = 2.0 * (x * z - w * y);
		const double r00 = 1.0 - 2.0 * (y * y + z * z);
		ey = std::atan2(-r20, r00);
		ez = 0.0;
	}
	return { static_cast<float>(ex * kRadToDeg), static_cast<float>(ey * kRadToDeg), static_cast<float>(ez * kRadToDeg) };
}

void evaluateTracks(const AnimTracks& tracks, double frame, TrackTransform& inout, float* fovDeg) {
	evaluateChannel(tracks.track(AnimChannel::Translation), frame, false, inout.translation);
	evaluateChannel(tracks.track(AnimChannel::Rotation), frame, tracks.rotationMode == RotationMode::Quaternion,
	                inout.rotationEulerDeg);
	evaluateChannel(tracks.track(AnimChannel::Scale), frame, false, inout.scale);
	if (fovDeg && tracks.animated(AnimChannel::Fov)) {
		std::array<float, 3> v = { *fovDeg, 0.f, 0.f };
		evaluateChannel(tracks.track(AnimChannel::Fov), frame, false, v);
		*fovDeg = v[0];
	}
}

void insertKey(std::vector<TrackKey>& track, const TrackKey& key) {
	auto it = std::lower_bound(track.begin(), track.end(), key.frame,
		[](const TrackKey& k, int f) { return k.frame < f; });
	if (it != track.end() && it->frame == key.frame) *it = key;
	else track.insert(it, key);
}

bool removeKeyAt(std::vector<TrackKey>& track, int frame) {
	auto it = std::lower_bound(track.begin(), track.end(), frame,
		[](const TrackKey& k, int f) { return k.frame < f; });
	if (it == track.end() || it->frame != frame) return false;
	track.erase(it);
	return true;
}

bool shiftKeys(AnimTracks& tracks, unsigned mask, const std::vector<int>& frames, int delta, bool copy,
               bool overwrite, std::string* error, int* count) {
	if (count) *count = 0;
	const std::set<int> moving(frames.begin(), frames.end());
	// Diagnose everything first so a refusal leaves the tracks untouched.
	std::array<std::vector<TrackKey>, kAnimChannelCount> picked;
	for (int c = 0; c < kAnimChannelCount; ++c) {
		if (!(mask & (1u << c))) continue;
		const auto& track = tracks.channels[c];
		for (const auto& k : track) {
			if (!moving.count(k.frame)) continue;
			const long long dest = static_cast<long long>(k.frame) + delta;
			if (dest < 0 || dest > 100000000LL) {
				if (error) *error = "destination frame " + std::to_string(dest) + " is out of range";
				return false;
			}
			if (!overwrite && delta != 0) {
				const TrackKey* hit = tracks.find(static_cast<AnimChannel>(c), static_cast<int>(dest));
				const bool hitMoves = hit && !copy && moving.count(hit->frame) > 0;
				if (hit && !hitMoves) {
					if (error) *error = std::string("a ") + channelName(static_cast<AnimChannel>(c)) +
						" key already exists at frame " + std::to_string(dest) + " (use overwrite=1)";
					return false;
				}
			}
			picked[c].push_back(k);
		}
	}
	int placed = 0;
	for (int c = 0; c < kAnimChannelCount; ++c) {
		auto& track = tracks.channels[c];
		if (!copy) for (const auto& k : picked[c]) removeKeyAt(track, k.frame);
		for (auto k : picked[c]) {
			k.frame += delta;
			insertKey(track, k);
			++placed;
		}
	}
	if (count) *count = placed;
	return true;
}

double clipLocalTime(const ClipPlacement& p, double sceneSeconds, double fps, double clipDuration) {
	const double end = (p.trimOut < 0.0 || p.trimOut > clipDuration) ? clipDuration : p.trimOut;
	const double length = end - p.trimIn;
	if (!(length > 0.0) || !(fps > 0.0)) return p.trimIn;
	double t = (sceneSeconds - p.startFrame / fps) * p.speed;
	if (t <= 0.0) return p.trimIn;
	if (p.loop) t = std::fmod(t, length);
	else if (t > length) t = length;
	return p.trimIn + t;
}

bool convertRotationToQuaternion(AnimTracks& tracks) {
	if (tracks.rotationMode == RotationMode::Quaternion) return false;
	tracks.rotationMode = RotationMode::Quaternion;
	return true;
}

} // namespace Phantom::Animation
