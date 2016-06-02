all:
	python ~/Projects/emscripten/emcc -O1 -s EXPORTED_FUNCTIONS="['_tessellate', '_malloc', '_free']" dict.c mesh.c render.c tess.c geom.c memalloc.c normal.c priorityq.c sweep.c tessmono.c tessellate.c -o tessellate.asm.js
	cp tessellate.asm.js ~/Workspaces/Shumway/src/swf/gl/lib/
	cp tessellate.js ~/Workspaces/Shumway/src/swf/gl/lib/

	gcc dict.c mesh.c render.c tess.c geom.c memalloc.c normal.c priorityq.c sweep.c tessmono.c tessellate.c main.c -o tessellate
