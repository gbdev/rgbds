#include "cli.hpp"

#include <errno.h>
#include <fstream>
#include <string.h>
#include <string>
#include <vector>

#include "extern/getopt.hpp"
#include "util.hpp" // isBlankSpace

using namespace std::literals;

// Turn an at-file's contents into an argv that `getopt` can handle, appending them to `argPool`.
static std::vector<size_t> readAtFile(
    std::string const &path, std::vector<char> &argPool, void (*fatal)(char const *, ...)
) {
	std::vector<size_t> argvOfs;

	std::filebuf file;
	if (!file.open(path, std::ios_base::in)) {
		std::string msg = "Error reading at-file \""s + path + "\": " + strerror(errno);
		fatal(msg.c_str());
		return argvOfs; // Since we can't mark the `fatal` function pointer as [[noreturn]]
	}

	for (;;) {
		int c = file.sbumpc();

		// First, discard any leading blank space
		while (isBlankSpace(c)) {
			c = file.sbumpc();
		}

		// If it's a comment, discard everything until EOL
		if (c == '#') {
			c = file.sbumpc();
			while (c != EOF && !isNewline(c)) {
				c = file.sbumpc();
			}
		}

		if (c == EOF) {
			return argvOfs;
		} else if (isNewline(c)) {
			continue; // Start processing the next line
		}

		// Alright, now we can parse the line
		do {
			argvOfs.push_back(argPool.size());

			// Read one argument (until the next whitespace char).
			// We know there is one because we already have its first character in `c`.
			for (; c != EOF && !isWhitespace(c); c = file.sbumpc()) {
				argPool.push_back(c);
			}
			argPool.push_back('\0');

			// Discard blank space until the next argument (candidate)
			while (isBlankSpace(c)) {
				c = file.sbumpc();
			}
		} while (c != EOF && !isNewline(c)); // End if we reached EOL
	}
}

void cli_ParseArgs(
    int argc,
    char *argv[],
    char const *shortOpts,
    option const *longOpts,
    void (*parseArg)(int, char *),
    void (*fatal)(char const *, ...)
) {
	struct AtFileStackEntry {
		int parentInd;            // Saved offset into parent argv
		std::vector<char *> argv; // This context's arg pointer vec

		AtFileStackEntry(int parentInd_, std::vector<char *> argv_)
		    : parentInd(parentInd_), argv(argv_) {}
	};
	std::vector<AtFileStackEntry> atFileStack;

	int curArgc = argc;
	char **curArgv = argv;
	std::string optString = "-"s + shortOpts; // Request position arguments with a leading '-'
	std::vector<std::vector<char>> argPools;

	for (;;) {
		char *atFileName = nullptr;
		for (int ch;
		     (ch = musl_getopt_long_only(curArgc, curArgv, optString.c_str(), longOpts, nullptr))
		     != -1;) {
			if (ch == 1 && musl_optarg[0] == '@') {
				atFileName = &musl_optarg[1];
				break;
			} else {
				parseArg(ch, musl_optarg);
			}
		}

		if (atFileName) {
			// We need to allocate a new arg pool for each at-file, so as not to invalidate pointers
			// previous at-files may have generated to their own arg pools.
			// But for the same reason, the arg pool must also outlive the at-file's stack entry!
			std::vector<char> &argPool = argPools.emplace_back();

			// Copy `argv[0]` for error reporting, and because option parsing skips it
			AtFileStackEntry &stackEntry =
			    atFileStack.emplace_back(musl_optind, std::vector{atFileName});

			// It would be nice to compute the char pointers on the fly, but reallocs don't allow
			// that; so we must compute the offsets after the pool is fixed
			std::vector<size_t> offsets = readAtFile(&musl_optarg[1], argPool, fatal);
			stackEntry.argv.reserve(offsets.size() + 2); // Avoid a bunch of reallocs
			for (size_t ofs : offsets) {
				stackEntry.argv.push_back(&argPool.data()[ofs]);
			}
			stackEntry.argv.push_back(nullptr); // Don't forget the arg vector terminator!

			curArgc = stackEntry.argv.size() - 1;
			curArgv = stackEntry.argv.data();
			musl_optind = 1; // Don't use 0 because we're not scanning a different argv per se
		} else {
			if (musl_optind != curArgc) {
				// This happens if `--` is passed, process the remaining arg(s) as positional
				assume(musl_optind < curArgc);
				for (int i = musl_optind; i < curArgc; ++i) {
					parseArg(1, argv[i]); // Positional argument
				}
			}

			// Pop off the top stack entry, or end parsing if none
			if (atFileStack.empty()) {
				break;
			}

			// OK to restore `optind` directly, because `optpos` must be 0 right now.
			// (Providing 0 would be a "proper" reset, but we want to resume parsing)
			musl_optind = atFileStack.back().parentInd;
			atFileStack.pop_back();
			if (atFileStack.empty()) {
				curArgc = argc;
				curArgv = argv;
			} else {
				std::vector<char *> &vec = atFileStack.back().argv;
				curArgc = vec.size();
				curArgv = vec.data();
			}
		}
	}
}
