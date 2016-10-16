#include "UuidGenerator.hpp"

int main() {
	UuidGenerator ug;

	ug.generate();
	puts(ug.getValue().c_str());
}
