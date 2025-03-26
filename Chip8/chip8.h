#include <string>
#include <stack>
#include <random>
#include <vector>
#include <utility>

using std::string;
using std::wstring;
using std::stack;
using std::random_device;
using std::mt19937;
using std::uniform_int_distribution;
using std::pair;

struct Chip8Quirks {
	Chip8Quirks() : resetVF(false), setVXtoVY(false), increamentI(false)
	{}
	Chip8Quirks(bool resetVF, bool setVXtoVY, bool increamentI);
	Chip8Quirks(Chip8Quirks& other);
	bool resetVF;
	bool setVXtoVY;
	bool increamentI;
};

// * 2048-byte RAM
// * On-card RAM expansion up to 4096 bytes
// * 512-byte ROM operating system
// * There are 31, easy-to-use CHIP-8 instructions.
// * Each CHIP-8 instruction is a two-byte (4-hex-digit) code.
// * A timer (or real-time clock) can be set to any value between 00 and FF by a FX15 instruction. This timer is automatically decremented by one, 60 times per second until it reaches 00.
// * To use the CHIP-8 language, you must first store the 512-byte CHIP-8 language program at memory locations 0000 to 01FF.
// * When using CHIP-8 instructions your program must always begin at location 0200.
class Chip8 {
// IO & Storage
public:
	Chip8();
	Chip8(const Chip8&) = delete;
	Chip8(const Chip8&&) = delete;
	Chip8& operator= (const Chip8&) = delete;
	void reset();
	bool load_rom(const wstring& path);
	bool is_ROM_opened() const { return _isROMOpened; }
private:
	static constexpr int MEMORY_SIZE = 4096;
	uint8_t _memory[MEMORY_SIZE];
	random_device _randomDevice;
	mt19937 _mt19937;
	uniform_int_distribution<> _numDistribution;
	bool _isROMOpened;
	
// Instructions
public:
	uint16_t fetch_code() const;
	bool is_draw_code(uint16_t code) const { return (code & 0xF000) == 0xD000; }
	bool is_sprites_overlapped() const { return _variables[0xF] == 1; }
	void execute_code(uint16_t code);
private:
	void code_00E0();
	void code_00EE();
	void code_1MMM(uint16_t code);
	void code_2MMM(uint16_t code);
	void code_3XKK(uint16_t code);
	void code_4XKK(uint16_t code);
	void code_5XY0(uint16_t code);
	void code_6XKK(uint16_t code);
	void code_7XKK(uint16_t code);
	void code_8XY0(uint16_t code);
	void code_8XY1(uint16_t code);
	void code_8XY2(uint16_t code);
	// undocumented
	void code_8XY3(uint16_t code);
	void code_8XY4(uint16_t code);
	void code_8XY5(uint16_t code);
	// undocumented
	void code_8XY6(uint16_t code);
	// undocumented
	void code_8XY7(uint16_t code);
	// undocumented
	void code_8XYE(uint16_t code);
	void code_9XY0(uint16_t code);
	void code_AMMM(uint16_t code);
	void code_BMMM(uint16_t code);
	void code_CXKK(uint16_t code);
	void code_DXYN(uint16_t code);
	void code_EX9E(uint16_t code);
	void code_EXA1(uint16_t code);
	void code_FX07(uint16_t code);
	void code_FX0A(uint16_t code);
	void code_FX15(uint16_t code);
	void code_FX18(uint16_t code);
	void code_FX1E(uint16_t code);
	void code_FX29(uint16_t code);
	void code_FX33(uint16_t code);
	void code_FX55(uint16_t code);
	void code_FX65(uint16_t code);
private:
	uint16_t _opcode;
	static constexpr int VARIABLE_SIZE = 16;
	uint32_t _ticks;
	uint8_t _variables[VARIABLE_SIZE];
	uint16_t _I;
	uint16_t _programCounter;
	stack<uint16_t> _callStack;


// Keyboard
public:
	void on_key_down(int key);
	void on_key_up(int key);

	static constexpr int KEYPAD_COUNT = 16;
private:
	bool _hexKeyboard[KEYPAD_COUNT];
	int _wasKeyHeldDown;


// Timer
public:
	void countdown();
	uint8_t get_sound_timer() const { return _soundTimer; }
private:
	// 1 unit = 1/60 second
	uint8_t _timer, _soundTimer;


// Display Buffer
public:
	static constexpr int DISPLAY_ROWS = 32;
	static constexpr int DISPLAY_COLS = 64;
	const uint8_t* get_display_buffer() const { return &_displayBuffer[0][0]; }
private:
	uint8_t _fonts[80];
	uint8_t _displayBuffer[DISPLAY_ROWS][DISPLAY_COLS];
	

// Emulation Quirks
public:
	void set_quirks(Chip8Quirks& quirks);
	bool get_reset_VF() const { return _quirks.resetVF; }
	void set_reset_VF(bool value);
	bool is_VX_set_to_VY() const { return _quirks.setVXtoVY; }
	void set_VX_to_VY(bool value);
	bool is_increment_I() const { return _quirks.increamentI; }
	void set_increment_I(bool value);
private:
	Chip8Quirks _quirks;


// Video Configs
public:
	bool skip_on_sprites_overlap() const { return _skipOnSpriteCollision; }
	void set_skip_on_sprite_collision(bool skip);
private:
	bool _skipOnSpriteCollision;
};