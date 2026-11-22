extends RefCounted
## WOTR-1.1-K4: the palette of the Remaster world.
##
## Presentation only. The colours never reach the campaign state.

const BACKGROUND := Color(0.055, 0.063, 0.078)
const CELL_BASE := Color(0.286, 0.271, 0.239)
const CELL_TOP := Color(0.353, 0.376, 0.318)
const SELECTED := Color(0.545, 0.827, 1.0)
const CONFLICT := Color(0.902, 0.420, 0.220)
const TEXT := Color(0.878, 0.867, 0.816)
const MUTED := Color(0.639, 0.639, 0.596)

## Faction colours, picked by the faction's position in the sorted faction list.
const FACTION_COLORS := [
	Color(0.851, 0.761, 0.420),
	Color(0.549, 0.239, 0.220),
	Color(0.400, 0.549, 0.749),
	Color(0.400, 0.620, 0.380),
	Color(0.620, 0.447, 0.702),
	Color(0.800, 0.518, 0.278),
]


## The colour of a faction. `order` is the sorted list of faction ids, so the
## same pack always paints the same faction the same colour.
static func faction_color(faction_id: String, order: Array) -> Color:
	var index: int = order.find(faction_id)
	if index < 0:
		index = 0
	return FACTION_COLORS[index % FACTION_COLORS.size()]
