/* Comprehend engine glue: saved games, #savestate/#loadstate and undo. */

#include "comprehend.h"
#include "graphics_magician.h"
#include "comprehend_internal.h"

namespace Glk {
namespace Comprehend {

// Thin wrappers so the Serializer can drive a GLK file stream directly.
struct GlkWriteStream : public Common::WriteStream {
	strid_t _str;
	explicit GlkWriteStream(strid_t str) : _str(str) {}
	void writeByte(byte b) override { glk_put_char_stream(_str, (unsigned char)b); }
	uint32 write(const void *p, uint32 n) override {
		glk_put_buffer_stream(_str, (char *)p, n);
		return n;
	}
};

struct GlkReadStream : public Common::SeekableReadStream {
	strid_t _str;
	int64 _size;
	bool _eos = false;
	GlkReadStream(strid_t str, int64 size) : _str(str), _size(size) {}
	byte readByte() override {
		glsi32 c = glk_get_char_stream(_str);
		if (c < 0) { _eos = true; return 0; }
		return (byte)c;
	}
	uint32 read(void *p, uint32 n) override {
		glui32 got = glk_get_buffer_stream(_str, (char *)p, n);
		if (got < n) _eos = true;
		return got;
	}
	bool eos() const override { return _eos; }
	int64 pos() const override { return (int64)glk_stream_get_position(_str); }
	int64 size() const override { return _size; }
	bool seek(int64 o, int w) override {
		glk_stream_set_position(_str, (glsi32)o, (glui32)w);
		_eos = false;
		return true;
	}
};

// Saved-game file header. Lets a restore reject a file that isn't one of this
// story's saves, instead of feeding garbage into the live game state (which
// then crashes when an out-of-range room/object is dereferenced).
static const char kSaveMagic[4] = { 'C', 'M', 'S', 'V' };
// v2 added the live '@'-replacement selector (_currentReplaceWord and friends)
// to the payload; v1 saves are rejected (the layout is otherwise unchanged).
static const byte kSaveVersion = 2;

// Serialize the live game state out to an already-created fileref. Shared by
// the slot-based save (autosave/launcher) and the SAVE-verb prompt path.
Common::Error Comprehend::saveToFileref(frefid_t fref) {
	if (!fref) return Common::Error(Common::kNoGameDataFoundError);

	strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
	if (!str) return Common::Error(Common::kNoGameDataFoundError);

	GlkWriteStream ws(str);
	// Header: magic, version, then the game id (length-prefixed) so a restore
	// can confirm the file belongs to the running story.
	ws.write(kSaveMagic, sizeof(kSaveMagic));
	ws.writeByte(kSaveVersion);
	byte idLen = (byte)MIN(_gameId.size(), (size_t)255);
	ws.writeByte(idLen);
	ws.write(_gameId.c_str(), idLen);

	Common::Serializer s(nullptr, &ws);
	_game->synchronizeSave(s);

	glk_stream_close(str, nullptr);
	return Common::Error(Common::kNoError);
}

// Restore live game state from an already-created fileref. Validates the header
// and rolls back to the pre-restore state if the payload turns out to be bad,
// so a wrong or truncated file can never leave the game in a crashing state.
Common::Error Comprehend::loadFromFileref(frefid_t fref) {
	if (!fref) return Common::Error(Common::kNoGameDataFoundError);

	strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
	if (!str) return Common::Error(Common::kNoGameDataFoundError);

	// Seek to end to determine file size, then rewind.
	glk_stream_set_position(str, 0, seekmode_End);
	int64 size = (int64)glk_stream_get_position(str);
	glk_stream_set_position(str, 0, seekmode_Start);

	GlkReadStream rs(str, size);

	// Validate the header before touching live state.
	char magic[4] = { 0 };
	rs.read(magic, sizeof(magic));
	byte ver = rs.readByte();
	byte idLen = rs.readByte();
	char idBuf[256] = { 0 };
	if (idLen)
		rs.read(idBuf, idLen);
	bool headerOk = !rs.eos() &&
					memcmp(magic, kSaveMagic, sizeof(kSaveMagic)) == 0 &&
					ver == kSaveVersion &&
					_gameId == Common::String(idBuf, idLen);
	if (!headerOk) {
		glk_stream_close(str, nullptr);
		print("That is not a saved game for this story.\n");
		return Common::Error(Common::kNoGameDataFoundError);
	}

	// Read the remaining bytes as the state payload.
	int64 payloadLen = size - (int64)(sizeof(magic) + 2 + idLen);
	std::vector<byte> payload(payloadLen > 0 ? (size_t)payloadLen : 0);
	if (!payload.empty())
		rs.read(payload.data(), (uint32)payload.size());
	glk_stream_close(str, nullptr);

	// The state layout is fixed-size for a given story, so the live state's
	// serialized length is exactly how long a genuine save's payload must be.
	// A truncated or otherwise wrong-sized file is rejected here, before it can
	// reach synchronizeSave() (which asserts on a mismatched room/item count).
	std::vector<byte> snapshot;
	bool haveSnapshot = serializeGameState(snapshot);
	if (!haveSnapshot || payload.size() != snapshot.size()) {
		print("That saved game appears to be damaged.\n");
		return Common::Error(Common::kNoGameDataFoundError);
	}

	deserializeGameState(payload);

	// A right-sized but internally inconsistent payload still shouldn't be able
	// to strand the player in a non-existent room: roll back if so.
	if (!_game->saveStateLooksValid()) {
		deserializeGameState(snapshot);
		print("That saved game appears to be damaged.\n");
		return Common::Error(Common::kNoGameDataFoundError);
	}

	clearUndo();  // can't undo across a restore

	// Repaint the room description, item list and picture for the restored
	// state. Without this the screen keeps showing the room the player restored
	// *from* until something else happens to dirty the graphics: the in-game
	// RESTORE verb (and the slot-based loadGameState) otherwise leave the flags
	// untouched. The launcher autorestore path already forces this in playGame().
	_game->_updateFlags = UPDATE_ALL;
	return Common::Error(Common::kNoError);
}

Common::Error Comprehend::saveGameState(int slot, const Common::String & /*desc*/) {
	Common::String name = Common::String::format("%s_save_%d", _gameId.c_str(), slot);
	frefid_t fref = glk_fileref_create_by_name(
		fileusage_SavedGame | fileusage_BinaryMode, (char *)name.c_str(), 0);
	Common::Error err = saveToFileref(fref);
	if (fref) glk_fileref_destroy(fref);
	return err;
}

Common::Error Comprehend::loadGameState(int slot) {
	Common::String name = Common::String::format("%s_save_%d", _gameId.c_str(), slot);
	frefid_t fref = glk_fileref_create_by_name(
		fileusage_SavedGame | fileusage_BinaryMode, (char *)name.c_str(), 0);
	if (!fref) return Common::Error(Common::kNoGameDataFoundError);

	if (!glk_fileref_does_file_exist(fref)) {
		glk_fileref_destroy(fref);
		return Common::Error(Common::kNoGameDataFoundError);
	}

	Common::Error err = loadFromFileref(fref);
	glk_fileref_destroy(fref);
	return err;
}

// SAVE verb: prompt the player for a file via the standard Glk save dialog.
Common::Error Comprehend::saveGamePrompt() {
	frefid_t fref = glk_fileref_create_by_prompt(
		fileusage_SavedGame | fileusage_BinaryMode, filemode_Write, 0);
	if (!fref) return Common::Error(Common::kNoGameDataFoundError);

	Common::Error err = saveToFileref(fref);
	glk_fileref_destroy(fref);
	return err;
}

// RESTORE verb: prompt the player for a file via the standard Glk restore dialog.
Common::Error Comprehend::loadGamePrompt() {
	frefid_t fref = glk_fileref_create_by_prompt(
		fileusage_SavedGame | fileusage_BinaryMode, filemode_Read, 0);
	if (!fref) return Common::Error(Common::kNoGameDataFoundError);

	Common::Error err = loadFromFileref(fref);
	glk_fileref_destroy(fref);
	return err;
}

// #savestate/#loadstate RNG trailer: 1 byte use-native flag + 4 little-endian
// 32-bit words of the xoshiro128** table (see common_utils/randomness.c).
#define RNG_TRAILER_SIZE (1 + 4 * 4)

static void serializeRngState(byte *out) {
	int usenative = 0;
	glui32 *table = nullptr;
	int count = 0;
	erkyrath_random_get_detstate(&usenative, &table, &count);
	out[0] = (byte)(usenative ? 1 : 0);
	for (int i = 0; i < 4; ++i) {
		glui32 w = (i < count && table) ? table[i] : 0;
		out[1 + i * 4 + 0] = (byte)(w & 0xff);
		out[1 + i * 4 + 1] = (byte)((w >> 8) & 0xff);
		out[1 + i * 4 + 2] = (byte)((w >> 16) & 0xff);
		out[1 + i * 4 + 3] = (byte)((w >> 24) & 0xff);
	}
}

static void deserializeRngState(const byte *in) {
	int usenative = in[0] ? 1 : 0;
	glui32 table[4];
	for (int i = 0; i < 4; ++i) {
		table[i] = (glui32)in[1 + i * 4 + 0]
				 | ((glui32)in[1 + i * 4 + 1] << 8)
				 | ((glui32)in[1 + i * 4 + 2] << 16)
				 | ((glui32)in[1 + i * 4 + 3] << 24);
	}
	erkyrath_random_set_detstate(usenative, table, 4);
}

bool Comprehend::saveStateToPath(const char *path) {
	std::vector<byte> snapshot;
	if (!serializeGameState(snapshot))
		return false;
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;
	// Trailer: a full snapshot of the shared erkyrath RNG state (the use-native
	// flag plus the xoshiro128** table), so a restore resumes the exact same
	// stream position -- some game logic is rand-gated (e.g. Talisman maze walls).
	byte rng[RNG_TRAILER_SIZE];
	serializeRngState(rng);
	size_t n = snapshot.empty() ? 0 : fwrite(snapshot.data(), 1, snapshot.size(), f);
	n += fwrite(rng, 1, sizeof(rng), f);
	fclose(f);
	return n == snapshot.size() + sizeof(rng);
}

bool Comprehend::loadStateFromPath(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	std::vector<byte> payload;
	byte buf[4096];
	for (;;) {
		size_t n = fread(buf, 1, sizeof(buf), f);
		payload.insert(payload.end(), buf, buf + n);
		if (n < sizeof(buf))
			break;
	}
	bool readError = ferror(f);
	fclose(f);
	if (readError)
		return false;

	// Split off the RNG-state trailer.
	if (payload.size() < RNG_TRAILER_SIZE)
		return false;
	byte rng[RNG_TRAILER_SIZE];
	memcpy(rng, payload.data() + payload.size() - RNG_TRAILER_SIZE, RNG_TRAILER_SIZE);
	payload.resize(payload.size() - RNG_TRAILER_SIZE);

	std::vector<byte> snapshot;
	if (!serializeGameState(snapshot) || payload.size() != snapshot.size())
		return false;
	deserializeGameState(payload);
	if (_game && !_game->saveStateLooksValid()) {
		deserializeGameState(snapshot);
		return false;
	}

	// Restore the exact RNG stream position captured at #savestate time.
	deserializeRngState(rng);
	clearUndo();
	if (_game)
		_game->_updateFlags = UPDATE_ALL;
	return true;
}

bool Comprehend::serializeGameState(std::vector<byte> &out) {
	if (!_game) return false;
	Common::MemoryReadWriteStream mem(Common::YES);
	mem.reserve(_snapshotSizeHint);
	Common::Serializer s(nullptr, &mem);
	_game->synchronizeSave(s);
	size_t sz = (size_t)mem.size();
	out.assign(mem.getData(), mem.getData() + sz);
	_snapshotSizeHint = sz;  // next turn's snapshot is the same size; reserve it
	return true;
}

void Comprehend::deserializeGameState(const std::vector<byte> &in) {
	if (!_game) return;
	// Let the game swap in a different static database before the payload is
	// applied (Talisman loads its part-2 desert tables when restoring a desert
	// save). Must run before synchronizeSave reads the rooms/items into them.
	_game->prepareRestore(in);
	Common::MemoryReadWriteStream mem(Common::YES);
	mem.write(in.data(), (uint32)in.size());
	mem.seek(0, SEEK_SET);
	Common::Serializer s(&mem, nullptr);
	_game->synchronizeSave(s);
	gmCMHourglassReset();  // undo jumps the sand back; snap, don't animate
	_hourglassFallPending = false;
}

void Comprehend::pushUndo() {
	std::vector<byte> snapshot;
	if (!serializeGameState(snapshot))
		return;
	// Don't record an identical, back-to-back snapshot (e.g. when a turn is
	// re-run via REDO_TURN), so each entry is a distinct turn boundary.
	if (!_undoStack.empty() && _undoStack.back() == snapshot)
		return;
	_undoStack.push_back(snapshot);
	if (_undoStack.size() > kMaxUndo)
		_undoStack.erase(_undoStack.begin());
}

bool Comprehend::undo() {
	// back() mirrors the current state; we need at least one earlier state to
	// revert to.
	if (_undoStack.size() < 2)
		return false;
	_undoStack.pop_back();
	deserializeGameState(_undoStack.back());
	return true;
}

bool Comprehend::undoTurn(uint turns) {
	if (_undoStack.empty())
		return false;
	// back() is the start of the fatal turn (one turn back). Each additional
	// turn drops one more boundary off the top, but always keep one snapshot to
	// restore. After restoring, back() mirrors the restored state, matching the
	// invariant the rest of the undo machinery relies on.
	while (turns > 1 && _undoStack.size() > 1) {
		_undoStack.pop_back();
		--turns;
	}
	deserializeGameState(_undoStack.back());
	return true;
}

} // namespace Comprehend
} // namespace Glk
