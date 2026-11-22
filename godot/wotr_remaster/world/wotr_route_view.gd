extends Node3D
## WOTR-1.1-K4: the routes of the moves the core accepted.
##
## Presentation only: a route is drawn from the region an army left to the
## region it was ordered to. The world never decides a move itself.

const Palette := preload("res://wotr_remaster/world/wotr_palette.gd")

var _bars: Array = []


func _ready() -> void:
	pass


## Draws the given routes. Each one is {"from": Vector3, "to": Vector3, "color": Color}.
func set_routes(routes: Array) -> void:
	for bar in _bars:
		if is_instance_valid(bar):
			var parent: Node = bar.get_parent()
			if parent != null:
				parent.remove_child(bar)
			bar.queue_free()
	_bars.clear()
	for route in routes:
		var from: Vector3 = route.get("from", Vector3.ZERO)
		var to: Vector3 = route.get("to", Vector3.ZERO)
		var color: Color = route.get("color", Palette.MUTED)
		_bars.append(_build_route(from, to, color))


func _build_route(from: Vector3, to: Vector3, color: Color) -> Node3D:
	var holder := Node3D.new()
	var direction := to - from
	var length := Vector2(direction.x, direction.z).length()
	if length < 0.01:
		return holder

	var material := StandardMaterial3D.new()
	material.albedo_color = Color(color.r, color.g, color.b, 0.85)
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = 0.6

	var bar := MeshInstance3D.new()
	bar.name = "Bar"
	var bar_mesh := BoxMesh.new()
	bar_mesh.size = Vector3(0.16, 0.08, length)
	bar.mesh = bar_mesh
	bar.material_override = material
	bar.position = (from + to) * 0.5 + Vector3(0.0, 0.22, 0.0)
	bar.rotation = Vector3(0.0, atan2(direction.x, direction.z), 0.0)
	holder.add_child(bar)

	var tip := MeshInstance3D.new()
	tip.name = "Tip"
	var tip_mesh := SphereMesh.new()
	tip_mesh.radius = 0.22
	tip_mesh.height = 0.44
	tip_mesh.radial_segments = 12
	tip_mesh.rings = 6
	tip.mesh = tip_mesh
	tip.material_override = material
	tip.position = to + Vector3(0.0, 0.30, 0.0)
	holder.add_child(tip)

	add_child(holder)
	return holder
