extends Node3D
## WOTR-1.1-K4: one region of the tiny map, drawn as a 2.5D hex plate.
##
## Presentation only: the view never holds campaign state, it only shows what
## the strategic core reported.

const Palette := preload("res://wotr_remaster/world/wotr_palette.gd")

const PLATE_RADIUS := 1.55
const TOP_RADIUS := 1.40
const PLATE_HEIGHT := 0.70

var region_id := ""
var _top: MeshInstance3D = null
var _selection: MeshInstance3D = null
var _conflict: MeshInstance3D = null
var _label: Label3D = null
var _conflict_active := false
var _pulse := 0.0


func setup(p_region_id: String, p_position: Vector3) -> void:
	region_id = p_region_id
	position = p_position
	name = "Cell_" + p_region_id


func _ready() -> void:
	var plate := MeshInstance3D.new()
	plate.name = "Plate"
	var plate_mesh := CylinderMesh.new()
	plate_mesh.top_radius = PLATE_RADIUS
	plate_mesh.bottom_radius = PLATE_RADIUS
	plate_mesh.height = PLATE_HEIGHT
	plate_mesh.radial_segments = 6
	plate_mesh.rings = 1
	plate.mesh = plate_mesh
	var plate_material := StandardMaterial3D.new()
	plate_material.albedo_color = Palette.CELL_BASE
	plate_material.roughness = 0.95
	plate.material_override = plate_material
	plate.position = Vector3(0.0, -PLATE_HEIGHT * 0.5, 0.0)
	add_child(plate)

	var top := MeshInstance3D.new()
	top.name = "Top"
	var top_mesh := CylinderMesh.new()
	top_mesh.top_radius = TOP_RADIUS
	top_mesh.bottom_radius = TOP_RADIUS
	top_mesh.height = 0.12
	top_mesh.radial_segments = 6
	top_mesh.rings = 1
	top.mesh = top_mesh
	var top_material := StandardMaterial3D.new()
	top_material.albedo_color = Palette.CELL_TOP
	top_material.roughness = 0.8
	top.material_override = top_material
	top.position = Vector3(0.0, 0.06, 0.0)
	add_child(top)
	_top = top

	_selection = _ring("Selection", TOP_RADIUS + 0.08, 0.05, 0.20, Palette.SELECTED, 1.4)
	_conflict = _ring("Conflict", TOP_RADIUS - 0.12, 0.07, 0.26, Palette.CONFLICT, 1.1)
	_conflict.visible = false

	_label = Label3D.new()
	_label.name = "Label"
	_label.text = region_id
	_label.font_size = 48
	_label.pixel_size = 0.0045
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.modulate = Palette.TEXT
	_label.outline_size = 12
	_label.outline_modulate = Color(0.0, 0.0, 0.0, 0.85)
	_label.position = Vector3(0.0, 1.25, 0.0)
	_label.no_depth_test = false
	add_child(_label)


func _ring(node_name: String, radius: float, height: float, y: float, color: Color, energy: float) -> MeshInstance3D:
	var ring := MeshInstance3D.new()
	ring.name = node_name
	var ring_mesh := CylinderMesh.new()
	ring_mesh.top_radius = radius
	ring_mesh.bottom_radius = radius
	ring_mesh.height = height
	ring_mesh.radial_segments = 32
	ring_mesh.rings = 1
	ring.mesh = ring_mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(color.r, color.g, color.b, 0.35)
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = energy
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	ring.material_override = material
	ring.position = Vector3(0.0, y, 0.0)
	add_child(ring)
	return ring


## The region's owner colour, or the unowned colour.
func set_owner_color(color: Color) -> void:
	if _top != null:
		var material := _top.material_override as StandardMaterial3D
		if material != null:
			material.albedo_color = color


func set_selected(p_selected: bool) -> void:
	if _selection != null:
		_selection.visible = p_selected


func set_conflict(p_conflict: bool) -> void:
	_conflict_active = p_conflict
	if _conflict != null:
		_conflict.visible = p_conflict
	set_process(p_conflict)


func _process(delta: float) -> void:
	if not _conflict_active or _conflict == null:
		return
	_pulse = fmod(_pulse + delta * 2.0, TAU)
	var material := _conflict.material_override as StandardMaterial3D
	if material != null:
		material.emission_energy_multiplier = 0.7 + 0.8 * (0.5 + 0.5 * sin(_pulse))
