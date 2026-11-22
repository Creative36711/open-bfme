extends Node3D
## WOTR-1.1-K4: one army on the tiny map, drawn as a 2.5D banner.
##
## Presentation only: the banner shows the army the core reported, with its
## faction colour and its surviving strength.

const Palette := preload("res://wotr_remaster/world/wotr_palette.gd")

var army_id := ""
var _flag: MeshInstance3D = null
var _label: Label3D = null
var _strength := 0
var _selected := false
var _label_offset := 0.0


func setup(p_army_id: String, p_position: Vector3, p_slot: int) -> void:
	army_id = p_army_id
	position = p_position
	name = "Army_" + p_army_id
	_label_offset = 0.55 * float(p_slot)


func _ready() -> void:
	var pole := MeshInstance3D.new()
	pole.name = "Pole"
	var pole_mesh := CylinderMesh.new()
	pole_mesh.top_radius = 0.06
	pole_mesh.bottom_radius = 0.08
	pole_mesh.height = 1.30
	pole_mesh.radial_segments = 8
	pole_mesh.rings = 1
	pole.mesh = pole_mesh
	var pole_material := StandardMaterial3D.new()
	pole_material.albedo_color = Color(0.30, 0.28, 0.25)
	pole_material.roughness = 0.9
	pole.material_override = pole_material
	pole.position = Vector3(0.0, 0.65, 0.0)
	add_child(pole)

	_flag = MeshInstance3D.new()
	_flag.name = "Flag"
	var flag_mesh := BoxMesh.new()
	flag_mesh.size = Vector3(0.95, 0.60, 0.06)
	_flag.mesh = flag_mesh
	var flag_material := StandardMaterial3D.new()
	flag_material.albedo_color = Palette.MUTED
	flag_material.roughness = 0.7
	flag_material.emission_enabled = true
	flag_material.emission_energy_multiplier = 0.25
	_flag.material_override = flag_material
	_flag.position = Vector3(0.46, 1.10, 0.0)
	add_child(_flag)

	_label = Label3D.new()
	_label.name = "Label"
	_label.font_size = 34
	_label.pixel_size = 0.0036
	_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label.modulate = Palette.TEXT
	_label.outline_size = 12
	_label.outline_modulate = Color(0.0, 0.0, 0.0, 0.85)
	_label.position = Vector3(0.0, 1.75 + _label_offset, 0.0)
	add_child(_label)


## The army's faction colour and its strength.
func set_army(faction_color: Color, strength: int, caption: String) -> void:
	_strength = strength
	if _flag != null:
		var material := _flag.material_override as StandardMaterial3D
		if material != null:
			material.albedo_color = faction_color
			material.emission = faction_color
			material.emission_energy_multiplier = 1.4 if _selected else 0.25
	if _label != null:
		_label.text = caption


## The army the player has selected.
func set_selected(p_selected: bool) -> void:
	_selected = p_selected
	if _flag != null:
		var material := _flag.material_override as StandardMaterial3D
		if material != null:
			material.emission_energy_multiplier = 1.4 if _selected else 0.25
