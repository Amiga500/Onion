X buttons for the GameSwitcher's "X DELETE" hint, one per theme, named
after the theme's folder. Each was made from the theme's own B button
(skin/icon-B-54.png): the letter replaced by an X in the same style. All
credit for the art goes to the themes' authors (github.com/OnionUI/Themes):
Hippo Stock; Oransje by evolve; Onionboy HD by Jeltron; CyberOnion by
Aemiii91; TechDweeb by TechDweeb; ONION PS by hanessh4; Silky by DiMo;
RetroRama by TooGeekCreations.

A theme's own skin/icon-X-54.png (or skin/extra/icon-X-54.png) always
comes first. Themes whose B button is empty (hints drawn in the footer art)
get no X button (Kids Mode shows the plain X there). Themes with no X here
get one made from their B button the first time it shows
(src/common/theme/button_x.h), or the plain X
(.tmp_update/res/icon-X-54.png) for a B it can't use.
