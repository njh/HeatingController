style_css.h: style.css node_modules/csso
	node css2cpp.js $< $@

node_modules/csso:
	npm install csso
