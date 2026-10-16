#pragma once
// Per-channel animation tracks of one object (moved from PhantomStudio, docs/todo/PLAN_phantomstudio_library_extraction.md Phase 1).
// Pure data + math: no Vulkan, no scene types, so it is unit-testable on its own.
#include <array>
#include <map>
#include <string>
#include <vector>

namespace Phantom::Animation {

	struct TrackTransform {
		std::array<float, 3> translation = { 0.f, 0.f, 0.f };
		std::array<float, 3> rotationEulerDeg = { 0.f, 0.f, 0.f };
		std::array<float, 3> scale = { 1.f, 1.f, 1.f };
	};

	// Fov (vertical field of view in degrees, value[0]) exists only on camera objects.
	enum class AnimChannel { Translation = 0, Rotation = 1, Scale = 2, Fov = 3 };
	constexpr int kAnimChannelCount = 4;
	constexpr unsigned kAllChannels = 0xFu;
	constexpr unsigned kTransformChannels = 0x7u;  // translation | rotation | scale: what every object has
	constexpr unsigned channelBit(AnimChannel c) { return 1u << static_cast<int>(c); }

	// Ease*: the segment's normalised time t is remapped before the blend (EaseIn t^2, EaseOut 1-(1-t)^2, EaseInOut smoothstep),
	// so the motion starts slowly, ends slowly, or both. Rotation still takes the shortest quaternion path.
	// Bezier: the segment time is remapped by a cubic Bezier timing curve through (0,0) and (1,1) whose two handles are stored
	// on the key that starts the segment (TrackKey::bezier = x1,y1,x2,y2, like CSS cubic-bezier: x in [0,1] is the handle's
	// share of the segment's time, y is its share of the value change and may leave [0,1] to overshoot). Because the handles
	// are normalised to the segment, every component of the key follows the same timing and moving the keys keeps the shape.
	enum class KeyInterp { Step, Linear, EaseIn, EaseOut, EaseInOut, Bezier };

	using BezierHandles = std::array<float, 4>;
	constexpr BezierHandles kDefaultBezier = { 0.42f, 0.f, 0.58f, 1.f };   // CSS "ease-in-out"
	bool validBezier(const BezierHandles& h);                         // finite, x1 and x2 within [0,1]

	const char* interpName(KeyInterp i);                              // "step" / "linear" / "easein" / "easeout" / "easeinout" / "bezier"
	bool parseInterpName(const std::string& name, KeyInterp& out);    // case-insensitive; also accepts "ease-in" etc.
	// Remaps segment time t in [0,1] by the key's easing (Linear and Step return t unchanged). Bezier uses the default handles;
	// pass the key's own with the overload below.
	double easeTime(KeyInterp i, double t);
	double bezierTime(const BezierHandles& h, double t);              // y of the timing curve at x = t

	// LegacyEuler: rotation keys are interpolated per Euler component (what files before .cgstudio v10 did).
	// Quaternion : rotation keys are Euler angles (YXZ order, degrees) converted to quaternions and blended along
	//              the shortest path. A quaternion path cannot express a turn of 180 degrees or more by itself.
	enum class RotationMode { LegacyEuler, Quaternion };

	struct TrackKey {
		int frame = 0;                       // integer frame, one key per frame per channel
		std::array<float, 3> value = { 0.f, 0.f, 0.f };
		KeyInterp interp = KeyInterp::Linear; // how the segment that STARTS at this key is evaluated
		BezierHandles bezier = kDefaultBezier; // timing handles, used only while interp == Bezier (kept otherwise so a switch back restores them)
	};

	struct AnimTracks {
		std::array<std::vector<TrackKey>, kAnimChannelCount> channels;  // each sorted ascending by frame
		RotationMode rotationMode = RotationMode::Quaternion;

		bool empty() const;                                // no key on any channel
		bool animated(AnimChannel c) const { return !channels[static_cast<int>(c)].empty(); }
		const std::vector<TrackKey>& track(AnimChannel c) const { return channels[static_cast<int>(c)]; }
		std::vector<TrackKey>& track(AnimChannel c) { return channels[static_cast<int>(c)]; }
		int keyCount() const;                              // keys over all channels
		std::vector<int> frames() const;                   // distinct key frames over all channels, ascending
		const TrackKey* find(AnimChannel c, int frame) const;
	};

	const char* channelName(AnimChannel c);
	bool parseChannelName(const std::string& name, AnimChannel& out);
	// "translation,rotation" / "all" (every channel) / "transform" (translation, rotation, scale) -> mask. False (error filled) on an unknown name.
	bool parseChannelMask(const std::string& text, unsigned& mask, std::string* error);

	// Writes only the channels that have keys; the others keep what `inout` already holds (the base value).
	// Outside the keyed range the end value is held. `frame` may be fractional.
	// `fovDeg` receives the fov channel when it is keyed (nullptr = ignore the fov channel).
	void evaluateTracks(const AnimTracks& tracks, double frame, TrackTransform& inout, float* fovDeg = nullptr);

	// Rotation helpers (exposed for tests): Euler YXZ degrees <-> quaternion (w,x,y,z).
	std::array<double, 4> eulerToQuaternion(const std::array<float, 3>& eulerDeg);
	std::array<float, 3> quaternionToEuler(const std::array<double, 4>& q);

	// Inserts a key, replacing an existing key at the same frame. Keeps the track sorted.
	void insertKey(std::vector<TrackKey>& track, const TrackKey& key);
	bool removeKeyAt(std::vector<TrackKey>& track, int frame);

	// Moves (copy=false) or copies the keys of the masked channels that sit on `frames` by `delta` frames.
	// Nothing is changed unless the whole operation can be done: a destination below frame 0, or a destination that
	// already holds a key which is not itself moving, is refused unless `overwrite`. `count` receives the keys placed.
	bool shiftKeys(AnimTracks& tracks, unsigned mask, const std::vector<int>& frames, int delta, bool copy,
	               bool overwrite, std::string* error, int* count);

	// Re-expresses legacy per-component Euler rotation keys as quaternion keys: the evaluated pose is kept at every key
	// frame, the path between keys becomes the shortest rotation. Returns false if already in quaternion mode.
	bool convertRotationToQuaternion(AnimTracks& tracks);

	// ---- Characters (docs/todo/PLAN_phantomstudio_animation_authoring.md Phase 5) ----
	// How a character's imported clip (SceneObject::activeAnimationIndex) is placed on the scene timeline. The clip itself is
	// never edited (the GltfDocument is shared); this only maps scene time to clip time:
	//   t = (sceneSeconds - startFrame / fps) * speed;  t < 0 holds the first pose;
	//   clip time = trimIn + t, clamped to the trimmed range or (loop) wrapped inside it.
	// trimOut < 0 means "to the end of the clip". The defaults reproduce the pre-Phase-5 behaviour exactly.
	struct ClipPlacement {
		int    startFrame = 0;
		double speed = 1.0;      // 0.01 .. 100
		double trimIn = 0.0;     // seconds into the clip
		double trimOut = -1.0;   // seconds into the clip, < 0 = clip end
		bool   loop = false;
		bool isDefault() const { return startFrame == 0 && speed == 1.0 && trimIn == 0.0 && trimOut < 0.0 && !loop; }
	};

	// Hand-authored pose of one bone, as a DELTA applied on top of the clip (or bind) pose, in the bone's local space:
	//   local = clipLocal * T(translation) * R(rotation) * S(scale)
	// `pose` is the static delta; keyed channels of `tracks` override it (identity where neither exists: translation 0,
	// rotation 0, scale 1). A bone is a glTF node index (stable for a given source file); `name` is kept to re-find it.
	struct BoneTrack {
		int node = -1;
		std::string name;
		TrackTransform pose;
		AnimTracks tracks;
		bool isIdentityPose() const {
			return pose.translation == std::array<float, 3>{ 0.f, 0.f, 0.f } &&
			       pose.rotationEulerDeg == std::array<float, 3>{ 0.f, 0.f, 0.f } &&
			       pose.scale == std::array<float, 3>{ 1.f, 1.f, 1.f };
		}
	};

	// Everything authored on a SkinnedCharacter beyond the imported data. Composition order (see CharacterPose.h):
	// bind pose -> placed clip -> bone deltas (static pose, then keys); morph weights: clip -> manual override -> morph keys.
	// A second clip cross-faded over the first: the weight of `clip` rises linearly from 0 to 1 over
	// [fadeStart, fadeStart + fadeFrames] (scene frames); before that only the main clip plays, after it only this one.
	// Poses are mixed per bone (translation / scale lerp, rotation slerp). Inactive when clip < 0 or fadeFrames == 0.
	struct ClipBlend {
		int clip = -1;               // index into the character's animations
		ClipPlacement placement;     // where the blended clip sits on the timeline
		int fadeStart = 0;
		int fadeFrames = 0;
		bool active() const { return clip >= 0 && fadeFrames > 0; }
		// Weight of the blended clip at `frame` (0 before the fade, 1 after).
		double weightAt(double frame) const {
			if (!active()) return 0.0;
			const double w = (frame - fadeStart) / fadeFrames;
			return w < 0.0 ? 0.0 : (w > 1.0 ? 1.0 : w);
		}
	};

	struct CharacterAnimation {
		ClipPlacement clip;
		ClipBlend blend;
		std::vector<BoneTrack> bones;
		std::map<int, AnimTracks> morphs;   // flat morph index -> weight track (stored in the Translation channel, value[0])
		bool authored() const { return !clip.isDefault() || blend.active() || !bones.empty() || !morphs.empty(); }
	};

	// Which track set of an object an edit addresses: the object itself, one bone (glTF node) or one morph target.
	struct TrackTarget {
		int bone = -1;
		int morph = -1;
		bool isObject() const { return bone < 0 && morph < 0; }
	};

	// Scene time -> clip time under `placement` (seconds). `clipDuration` is the clip's length in seconds.
	double clipLocalTime(const ClipPlacement& placement, double sceneSeconds, double fps, double clipDuration);

} // namespace Phantom::Animation
