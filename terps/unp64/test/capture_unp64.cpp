/* Stands in for unp64.cpp in a capture build of an interpreter (capture.sh):
 * the real unp64() is compiled under another name, and the one here writes
 * every program it is handed, with its switches, into $UNP64_CAPTURE_DIR
 * before passing it on. That is how the samples of the regression tests are
 * taken from the game images of the Scott and Taylor test suites.
 *
 *   <hash>.prg   the program, as unp64() got it
 *   <hash>.sw    the switch string (no file when there are none)
 *   log.tsv      one line per call: hash, and the file the interpreter was
 *                started on
 *
 * The hash is FNV-1a over the switches and the program, so that the same
 * program with other switches is another sample. */

#define unp64 unp64_real
#include "../unp64.cpp"
#undef unp64

#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <unistd.h>

#ifdef __APPLE__
#include <crt_externs.h>
#endif

static uint64_t fnv1a(uint64_t hash, const uint8_t *data, size_t length) {
	for (size_t i = 0; i < length; i++) {
		hash ^= data[i];
		hash *= 0x100000001b3ULL;
	}
	return hash;
}

static void writeFile(const std::string &path, const void *data, size_t length) {
	/* O_EXCL: the suites run their probes in parallel, and the first one to
	 * see a sample writes it. */
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (fd < 0)
		return;
	if (write(fd, data, length) != (ssize_t)length)
		perror(path.c_str());
	close(fd);
}

extern "C" int unp64(const uint8_t *compressed, size_t length, uint8_t *destinationBuffer, size_t *finalLength, const char *settings) {
	const char *dir = getenv("UNP64_CAPTURE_DIR");
	if (dir != nullptr) {
		std::string switches = settings ? settings : "";
		uint64_t hash = fnv1a(0xcbf29ce484222325ULL, (const uint8_t *)switches.data(), switches.size() + 1);
		hash = fnv1a(hash, compressed, length);
		char name[17];
		snprintf(name, sizeof name, "%016llx", (unsigned long long)hash);

		std::string base = std::string(dir) + "/" + name;
		writeFile(base + ".prg", compressed, length);
		if (!switches.empty())
			writeFile(base + ".sw", switches.data(), switches.size());

		std::string source = "?";
#ifdef __APPLE__
		int argc = *_NSGetArgc();
		if (argc > 1)
			source = (*_NSGetArgv())[argc - 1];
#endif
		size_t slash = source.rfind('/');
		if (slash != std::string::npos)
			source = source.substr(slash + 1);
		std::string line = std::string(name) + "\t" + source + "\n";
		int fd = open((std::string(dir) + "/log.tsv").c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
		if (fd >= 0) {
			if (write(fd, line.data(), line.size()) != (ssize_t)line.size())
				perror("log.tsv");
			close(fd);
		}
	}
	return unp64_real(compressed, length, destinationBuffer, finalLength, settings);
}
