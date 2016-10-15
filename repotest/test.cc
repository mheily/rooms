#include <assert.h>

#include "json.hpp"

#include "repoParser.hpp"

int main() {
	RepoParser rp;
	rp.parse("fixtures/repository.json");
	RepoRoom* rr = rp.getRoom("FreeBSD-10.3");
	assert(rr);
	std::cout << rr->getSummary() << "\n";	
	rp.installRoom("FreeBSD-10.3");
	rp.printSearchResult("f");
	std::cout << "done\n";	
}
