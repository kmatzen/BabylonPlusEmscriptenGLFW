App.html: App.cpp
	em++ App.cpp -sUSE_GLFW=3 -sUSE_WEBGL2=1 -sASYNCIFY -o App.html

all: App.html

clean:
	rm App.html App.wasm App.js

run: App.html
	emrun App.html
