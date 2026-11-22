extends RefCounted
## WOTR-1.1-K4: the text of the Remaster world.
##
## Every string is a catalog id with an English fallback. The eight product
## locales live in tools/l10n/catalog (card WOTR-1.1-K5); until an id is in
## the catalog the English source below is shown. Nothing here changes a
## command, a state byte or a hash.

const L10n := preload("res://scripts/l10n/l10n.gd")

const ID_MOVE := "game.wotr.remaster.hud.move"
const ID_SAVE := "game.wotr.remaster.hud.save"
const ID_LOAD := "game.wotr.remaster.hud.load"


## The text of a catalog id, or the English source when the catalog has none.
static func t(id: String, english: String) -> String:
	if L10n.has(id):
		return L10n.t(id)
	return english
