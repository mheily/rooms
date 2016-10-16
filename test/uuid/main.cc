#include "UuidGenerator.hpp"

void testSanitizer() {
	UuidGenerator ug;
	bool error = true;

	try {
		ug.setValue("invalid string");
	} catch (...) {
		error = false;
	}
	assert(!error);

	try {
		ug.setValue("");
	} catch (...) {
		error = false;
	}
	assert(!error);
}

int main() {
	UuidGenerator ug;

	ug.generate();
	puts(ug.getValue().c_str());
	testSanitizer();
	std::cout << "done\n";
}
