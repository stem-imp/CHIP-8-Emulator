#include "chip8.h"
#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>

using std::ifstream;
using std::cout;
using std::cerr;
using std::wcout;
using std::wcerr;
using std::endl;

Chip8Quirks::Chip8Quirks(bool resetVF, bool setVXtoVY, bool increamentI)
{
	this->resetVF = resetVF;
	this->setVXtoVY = setVXtoVY;
	this->increamentI = increamentI;
}

Chip8Quirks::Chip8Quirks(Chip8Quirks& other) : Chip8Quirks(other.resetVF, other.setVXtoVY, other.increamentI)
{}

Chip8::Chip8() : _memory(), _variables(),
	_I(0), _timer(0), _soundTimer(0), _programCounter(0x200),
	_hexKeyboard(), _wasKeyHeldDown(-1),
	_mt19937(_randomDevice()), _numDistribution(0x0, 0xFF),
	_displayBuffer(), _quirks(), _skipOnSpriteCollision(false),
	_isROMOpened(false),
	_fonts {
		0xF0, 0x90, 0x90, 0x90, 0xF0,
		0x20, 0x60, 0x20, 0x20, 0x70,
		0xF0, 0x10, 0xF0, 0x80, 0xF0,
		0xF0, 0x10, 0xF0, 0x10, 0xF0,
		0x90, 0x90, 0xF0, 0x10, 0x10,
		0xF0, 0x80, 0xF0, 0x10, 0xF0,
		0xF0, 0x80, 0xF0, 0x90, 0xF0,
		0xF0, 0x10, 0x20, 0x40, 0x40,
		0xF0, 0x90, 0xF0, 0x90, 0xF0,
		0xF0, 0x90, 0xF0, 0x10, 0xF0,
		0xF0, 0x90, 0xF0, 0x90, 0x90,
		0xE0, 0x90, 0xE0, 0x90, 0xE0,
		0xF0, 0x80, 0x80, 0x80, 0xF0,
		0xE0, 0x90, 0x90, 0x90, 0xE0,
		0xF0, 0x80, 0xF0, 0x80, 0xF0,
		0xF0, 0x80, 0xF0, 0x80, 0x80
	}
{
	reset();
}

void Chip8::reset()
{
	_ticks = 0;
	_I = _timer = _soundTimer = 0;
	_programCounter = 0x200;
	if (!_callStack.empty()) {
		_callStack.pop();
	}
	std::fill(_variables, _variables + VARIABLE_SIZE, 0);
	std::fill(_memory, _memory + MEMORY_SIZE, 0);
	_wasKeyHeldDown = -1;
	for (int i = 0; i < DISPLAY_ROWS; ++i) {
		std::fill(_displayBuffer[i], _displayBuffer[i] + DISPLAY_COLS, 255);
	}
	_isROMOpened = false;
}

bool Chip8::load_rom(const wstring& path)
{
	reset();

	ifstream ifs;
	ifs.open(path, ifstream::binary);
	if (!ifs.is_open() || !ifs.good()) {
		wcerr << "Open: " << path << " error" << endl;
		return false;
	}
	std::copy(_fonts, _fonts+16*5, _memory);

	ifs.seekg(0, ifs.end);
	size_t length = static_cast<size_t>(ifs.tellg());
	ifs.seekg(0, ifs.beg);

	ifs.read(reinterpret_cast<char*>(_memory + 0x200), length);
	ifs.close();

	_isROMOpened = true;

	return _isROMOpened;
}

uint16_t Chip8::fetch_code() const
{
	return ((_memory[_programCounter] << 8) | _memory[_programCounter + 1]);
}

void Chip8::execute_code(uint16_t code)
{
	switch (code & 0xF000) {
	case 0x0000:
		switch (code & 0x00FF) {
		case 0x00E0:
			code_00E0();
			break;
		case 0x00EE:
			code_00EE();
			break;
		default:
			cerr << "Unknown " << code << endl;
			break;
		}
		break;
	case 0x1000:
		code_1MMM(code);
		break;
	case 0x2000:
		code_2MMM(code);
		break;
	case 0x3000:
		code_3XKK(code);
		break;
	case 0x4000:
		code_4XKK(code);
		break;
	case 0x5000:
		code_5XY0(code);
		break;
	case 0x6000:
		code_6XKK(code);
		break;
	case 0x7000:
		code_7XKK(code);
		break;
	case 0x8000:
		switch (code & 0xF) {
		case 0x0:
			code_8XY0(code);
			break;
		case 0x1:
			code_8XY1(code);
			break;
		case 0x2:
			code_8XY2(code);
			break;
		case 0x3:
			code_8XY3(code);
			break;
		case 0x4:
			code_8XY4(code);
			break;
		case 0x5:
			code_8XY5(code);
			break;
		case 0x6:
			code_8XY6(code);
			break;
		case 0x7:
			code_8XY7(code);
			break;
		case 0xE:
			code_8XYE(code);
			break;
		default:
			cerr << "Unknown " << code << endl;
			break;
		}
		break;
	case 0x9000:
		code_9XY0(code);
		break;
	case 0xA000:
		code_AMMM(code);
		break;
	case 0xB000:
		code_BMMM(code);
		break;
	case 0xC000:
		code_CXKK(code);
		break;
	case 0xD000:
		code_DXYN(code);
		break;
	case 0xE000:
		switch (code & 0xF) {
		case 0xE:
			code_EX9E(code);
			break;
		case 0x1:
			code_EXA1(code);
			break;
		default:
			cerr << "Unknown " << code << endl;
			break;
		}
		break;
	case 0xF000:
		switch (code & 0x00FF) {
		case 0x0007:
			code_FX07(code);
			break;
		case 0x000A:
			code_FX0A(code);
			break;
		case 0x0015:
			code_FX15(code);
			break;
		case 0x0018:
			code_FX18(code);
			break;
		case 0x001E:
			code_FX1E(code);
			break;
		case 0x0029:
			code_FX29(code);
			break;
		case 0x0030:
			break;
		case 0x0033:
			code_FX33(code);
			break;
		case 0x0055:
			code_FX55(code);
			break;
		case 0x0065:
			code_FX65(code);
			break;
		default:
			cerr << "Unknown " << code << endl;
			break;
		}
		break;
	default:
		cerr << "Unknown " << code << endl;
		break;
	}
	_ticks++;
}

void Chip8::code_00E0()
{
	for (int row = 0; row < DISPLAY_ROWS; ++row) {
		std::fill(_displayBuffer[row], _displayBuffer[row] + DISPLAY_COLS, 255);
	}
	_programCounter += 2;
}

void Chip8::code_00EE()
{
	_programCounter = _callStack.top();
	_callStack.pop();
}

void Chip8::code_1MMM(uint16_t code)
{
	_programCounter = code & 0x0FFF;
}

void Chip8::code_2MMM(uint16_t code)
{
	_callStack.push(_programCounter + 2);
	_programCounter = code & 0x0FFF;
}

void Chip8::code_3XKK(uint16_t code)
{
	_programCounter += 2;
	if (_variables[(code & 0x0F00) >> 8] == (code & 0x00FF)) {
		_programCounter += 2;
	}
}

void Chip8::code_4XKK(uint16_t code)
{
	_programCounter += 2;
	if (_variables[(code & 0x0F00) >> 8] != (code & 0x00FF)) {
		_programCounter += 2;
	}
}

void Chip8::code_5XY0(uint16_t code)
{
	_programCounter += 2;
	if (_variables[(code & 0x0F00) >> 8] == _variables[(code & 0x00F0) >> 4]) {
		_programCounter += 2;
	}
}

void Chip8::code_6XKK(uint16_t code)
{
	_variables[(code & 0x0F00) >> 8] = (code & 0xFF);
	_programCounter += 2;
}

void Chip8::code_7XKK(uint16_t code)
{
	_variables[(code & 0x0F00) >> 8] += (code & 0xFF);
	_programCounter += 2;
}

void Chip8::code_8XY0(uint16_t code)
{
	_variables[(code & 0x0F00) >> 8] = _variables[(code & 0x00F0) >> 4];
	_programCounter += 2;
}

void Chip8::code_8XY1(uint16_t code)
{
	_variables[(code & 0x0F00) >> 8] |= _variables[(code & 0x00F0) >> 4];

	if (_quirks.resetVF) {
		_variables[0xF] = 0;
	}

	_programCounter += 2;
}

void Chip8::code_8XY2(uint16_t code)
{
	_variables[(code & 0x0F00) >> 8] &= _variables[(code & 0x00F0) >> 4];

	if (_quirks.resetVF) {
		_variables[0xF] = 0;
	}

	_programCounter += 2;
}

void Chip8::code_8XY3(uint16_t code)
{
	_variables[(code & 0x0F00) >> 8] ^= _variables[(code & 0x00F0) >> 4];

	if (_quirks.resetVF) {
		_variables[0xF] = 0;
	}

	_programCounter += 2;
}

void Chip8::code_8XY4(uint16_t code)
{
	int X = (code & 0x0F00) >> 8;
	int Y = (code & 0x00F0) >> 4;
	uint8_t carry = _variables[X] > 0xFF - _variables[Y];
	_variables[X] += _variables[Y];
	_variables[0xF] = carry;
	_programCounter += 2;
}

void Chip8::code_8XY5(uint16_t code)
{
	int X = (code & 0x0F00) >> 8;
	int Y = (code & 0x00F0) >> 4;
	uint8_t carry = _variables[X] >= _variables[Y];
	_variables[X] -= _variables[Y];
	_variables[0xF] = carry;
	_programCounter += 2;
}

void Chip8::code_8XY6(uint16_t code)
{
	int X = (code & 0x0F00) >> 8;
	if (_quirks.setVXtoVY) {
		_variables[X] = _variables[(code & 0x00F0) >> 4];
	}
	uint8_t carry = _variables[X] & 0x1;
	_variables[X] >>= 1;
	_variables[0xF] = carry;

	_programCounter += 2;
}

void Chip8::code_8XY7(uint16_t code)
{
	int X = (code & 0x0F00) >> 8;
	int Y = (code & 0x00F0) >> 4;
	uint8_t carry = (_variables[Y] >= _variables[X]);
	_variables[X] = _variables[Y] - _variables[X];
	_variables[0xF] = carry;
	_programCounter += 2;
}

void Chip8::code_8XYE(uint16_t code)
{
	int X = (code & 0x0F00) >> 8;
	if (_quirks.setVXtoVY) {
		_variables[X] = _variables[(code & 0x00F0) >> 4];
	}
	uint8_t carry = _variables[X] >> 7;
	_variables[X] <<= 1;
	_variables[0xF] = carry;

	_programCounter += 2;
}

void Chip8::code_9XY0(uint16_t code)
{
	_programCounter += 2;
	if (_variables[(code & 0x0F00) >> 8] != _variables[(code & 0x00F0) >> 4]) {
		_programCounter += 2;
	}
}

void Chip8::code_AMMM(uint16_t code)
{
	_I = (code & 0x0FFF);
	_programCounter += 2;
}

void Chip8::code_BMMM(uint16_t code)
{
	// config
	//_programCounter = (code & 0x0FFF) + _variables[(code & 0x0F00) >> 8];

	_programCounter = (code & 0x0FFF) + _variables[0];
}

void Chip8::code_CXKK(uint16_t code)
{
	int num = _numDistribution(_mt19937);
	_variables[(code & 0x0F00) >> 8] = num & (code & 0x00FF);
	_programCounter += 2;
}

void Chip8::code_DXYN(uint16_t code)
{
	int X = _variables[(code & 0x0F00) >> 8];
	int Y = _variables[(code & 0x00F0) >> 4];
	uint8_t N = code & 0x000F;
	_variables[0xF] = 0;
	Y %= DISPLAY_ROWS;
	X %= DISPLAY_COLS;
	for (uint8_t row = 0; Y + row < DISPLAY_ROWS && row < N; ++row) {
		uint8_t byte = _memory[_I + row];
		int r = Y + row;
		for (int col = 0; X + col < DISPLAY_COLS && col < 8; ++col) {
			int c = X + col;
			uint8_t bit = byte & (0x80 >> col);
			uint8_t* p = &_displayBuffer[r][c];
			//uint8_t bit = (byte >> col) & 0x1;
			//uint8_t* p = &_displayBuffer[(Y + row) % DISPLAY_ROWS][(X + (7 - col)) % DISPLAY_COLS];
			if (bit && *p == 0) _variables[0xF] = 1;
			*p = ((bit && *p == 0) || (!bit && *p == 255)) ? 255 : 0;
			/*if (bit) {
				if (*p == 0) {
					_variables[0xF] = 1;
					*p = 255;
				}
				else {
					*p = 0;
				}
			}*/
		}
	}
	_programCounter += 2;
}

void Chip8::code_EX9E(uint16_t code)
{
	_programCounter += 2;
	if (_hexKeyboard[_variables[(code & 0x0F00) >> 8]] == 1) {
		_programCounter += 2;
	}
}

void Chip8::code_EXA1(uint16_t code)
{
	_programCounter += 2;
	if (_hexKeyboard[_variables[(code & 0x0F00) >> 8]] == 0) {
		_programCounter += 2;
	}
}

void Chip8::code_FX07(uint16_t code)
{
	_variables[(code & 0x0F00) >> 8] = _timer;
	_programCounter += 2;
}

void Chip8::code_FX0A(uint16_t code)
{
	bool anyKeyPressed = false;
	int i = 0;
	for (; i < KEYPAD_COUNT; ++i) {
		if (_hexKeyboard[i]) {
			anyKeyPressed = _hexKeyboard[i];
			_wasKeyHeldDown = i;
			break;
		}
	}
	if (_wasKeyHeldDown != -1 && !_hexKeyboard[_wasKeyHeldDown]) {
		_variables[(code & 0x0F00) >> 8] = _wasKeyHeldDown;
		_wasKeyHeldDown = -1;
		_programCounter += 2;
	}
}

void Chip8::code_FX15(uint16_t code)
{
	_timer = _variables[(code & 0x0F00) >> 8];
	_programCounter += 2;
}

void Chip8::code_FX18(uint16_t code)
{
	_soundTimer = _variables[(code & 0x0F00) >> 8];
	if (_soundTimer > 0 && _soundTimer < 4) {
		_soundTimer = 4;
	}
	_programCounter += 2;
}

void Chip8::code_FX1E(uint16_t code)
{
	_I += _variables[(code & 0x0F00) >> 8];
	_programCounter += 2;
}

void Chip8::code_FX29(uint16_t code)
{
	_I = 5 * _variables[(code & 0x0F00) >> 8];
	_programCounter += 2;
}

void Chip8::code_FX33(uint16_t code)
{
	int value = _variables[(code & 0x0F00) >> 8];
	_memory[_I] = value / 100;
	_memory[_I + 1] = (value / 10) % 10;
	_memory[_I + 2] = value % 10;
	_programCounter += 2;
}

// In the original CHIP-8 implementation, and also in CHIP-48, I is left incremented after this instruction had been executed. In SCHIP, I is left unmodified.
void Chip8::code_FX55(uint16_t code)
{
	int X = (code & 0x0F00) >> 8;
	for (int i = 0; i <= X; i++) {
		_memory[_I + i] = _variables[i];
	}

	if (_quirks.increamentI) {
		_I += X + 1;
	}

	_programCounter += 2;
}

// In the original CHIP-8 implementation, and also in CHIP-48, I is left incremented after this instruction had been executed. In SCHIP, I is left unmodified.
void Chip8::code_FX65(uint16_t code)
{
	int X = (code & 0x0F00) >> 8;
	for (int i = 0; i <= X; i++) {
		_variables[i] = _memory[_I + i];
	}

	if (_quirks.increamentI) {
		_I += X + 1;
	}

	_programCounter += 2;
}

void Chip8::on_key_down(int key)
{
	_hexKeyboard[key] = 1;
}

void Chip8::on_key_up(int key)
{
	_hexKeyboard[key] = 0;
}

void Chip8::countdown()
{
	if (_timer > 0) {
		--_timer;
	}
	if (_soundTimer > 0) {
		--_soundTimer;
	}
}

void Chip8::set_quirks(Chip8Quirks& quirks)
{
	_quirks.resetVF = quirks.resetVF;
	_quirks.setVXtoVY = quirks.setVXtoVY;
	_quirks.increamentI = quirks.increamentI;
}

void Chip8::set_reset_VF(bool value)
{
	_quirks.resetVF = value;
}

void Chip8::set_VX_to_VY(bool value)
{
	_quirks.setVXtoVY = value;
}

void Chip8::set_increment_I(bool value)
{
	_quirks.increamentI = value;
}

void Chip8::set_skip_on_sprite_collision(bool skip)
{
	_skipOnSpriteCollision = skip;
}
