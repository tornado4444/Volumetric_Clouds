#include "Init.hpp"

int main() {
	Init init;

	init.initialize();

	while (!init.shouldClose()) {
		init.processInput(init.getWindow());
		init.render();
		init.swapBuffersAndPollEvents();
	}
}