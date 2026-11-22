extends Control
## WOTR-1.1-K4: the readout panel of the Remaster world.
##
## Structure is Godot Control / Container / Theme (docs/tooling/ART-PIPELINE.md);
## the rows are built in code because their number follows the state the core
## reported. Every string is a catalog id with an English fallback; the eight
## product locales are card WOTR-1.1-K5.

const Palette := preload("res://wotr_remaster/world/wotr_palette.gd")
const Text := preload("res://wotr_remaster/world/wotr_text.gd")

var world = null

var _panel: PanelContainer = null
var _rows: VBoxContainer = null
var _labels: Dictionary = {}
var _buttons: Dictionary = {}


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_build()


func _build() -> void:
	_panel = PanelContainer.new()
	_panel.name = "Panel"
	_panel.position = Vector2(16.0, 16.0)
	_panel.custom_minimum_size = Vector2(460.0, 0.0)
	_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(_panel)

	_rows = VBoxContainer.new()
	_rows.name = "Rows"
	_rows.add_theme_constant_override("separation", 3)
	_panel.add_child(_rows)

	for key in ["title", "status", "selection", "regions", "battle", "result"]:
		var label := Label.new()
		label.name = key
		label.add_theme_font_size_override("font_size", 14)
		label.add_theme_color_override("font_color", Palette.TEXT)
		label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		label.custom_minimum_size = Vector2(440.0, 0.0)
		_rows.add_child(label)
		_labels[key] = label

	var buttons := HBoxContainer.new()
	buttons.name = "Buttons"
	buttons.add_theme_constant_override("separation", 8)
	_rows.add_child(buttons)

	var move_button := _button("move", Text.ID_MOVE, "Move")
	var save_button := _button("save", Text.ID_SAVE, "Save")
	var load_button := _button("load", Text.ID_LOAD, "Load")
	buttons.add_child(move_button)
	buttons.add_child(save_button)
	buttons.add_child(load_button)


func _button(key: String, catalog_id: String, english: String) -> Button:
	var button := Button.new()
	button.name = key
	button.text = Text.t(catalog_id, english)
	button.custom_minimum_size = Vector2(96.0, 32.0)
	button.focus_mode = Control.FOCUS_ALL
	button.pressed.connect(_on_pressed.bind(key))
	_buttons[key] = button
	return button


func _on_pressed(key: String) -> void:
	if world == null:
		return
	match key:
		"move":
			world.submit_move()
		"save":
			world.save_campaign()
		"load":
			world.reload_last_save()


## Fills the panel from the world's report.
func update(report: Dictionary) -> void:
	if _labels.is_empty():
		return
	_labels["title"].text = str(report.get("title", ""))
	_labels["status"].text = str(report.get("status", ""))
	_labels["selection"].text = str(report.get("selection", ""))
	_labels["regions"].text = str(report.get("regions", ""))
	_labels["battle"].text = str(report.get("battle", ""))
	_labels["result"].text = str(report.get("result", ""))
	var move_button: Button = _buttons.get("move")
	if move_button != null:
		move_button.disabled = not bool(report.get("can_move", false))
