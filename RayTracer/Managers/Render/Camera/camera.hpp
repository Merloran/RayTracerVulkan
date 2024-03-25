#pragma once
class Camera
{
public:
	Void initialize(const FVector3& position);
	Void move_forward(Float32 dt);
	Void move_right(Float32 dt);
	Void move_up(Float32 dt);
	Void rotate(Float32 xOffset, Float32 yOffset);

	[[nodiscard]]
	const FMatrix4& get_view() const;
	[[nodiscard]]
	const FMatrix4& get_projection(Float32 aspectRatio) const;

	[[nodiscard]]
	const FVector3& get_forward() const;
	[[nodiscard]]
	const FVector3& get_right() const;
	[[nodiscard]]
	const FVector3& get_up() const;
	[[nodiscard]]
	const FVector3& get_position() const;
	[[nodiscard]]
	const FVector2& get_view_bounds() const;
	[[nodiscard]]
	Float32 get_sensitivity() const;
	[[nodiscard]]
	Float32 get_speed() const;
	[[nodiscard]]
	Float32 get_fov() const;
	[[nodiscard]]
	Bool has_changed() const;

	Void set_position(const FVector3& position);
	Void set_view_bounds(const FVector2& viewBounds);
	Void set_sensitivity(Float32 sensitivity);
	Void set_speed(Float32 speed);
	Void set_fov(Float32 fov);
	Void set_camera_changed(Bool hasChanged);

	Void catch_input(Float32 deltaTime);

	Void update_camera_vectors();

private:
	FVector3 forward, up, right;
	FVector3 position;
	FVector2 lastPosition, viewBounds;
	Float32  yaw, pitch, fov;
	Float32  speed, sensitivity;
			 
	Bool	 isInactive, hasChanged;
};

