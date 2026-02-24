#pragma once

#include <array>

class InputManager {
private:
	InputManager() = default;
	InputManager(const InputManager&) = delete;
	InputManager(InputManager&&) = delete;
	InputManager& operator=(const InputManager&) = delete;
	InputManager& operator=(InputManager&&) = delete;

	// Index is mapped to virtual keycode, and value consists of 3 bits.
	// The LSB means if a keycode is currently pressed, 1 if it's pressed.
	// The second LSB means if a keycode is pressed just in this frame.
	// The third LSB means if a keycode is released just in this frame.
	// The second and third LSBs are reset to 0 in the end of each game loop.
	std::array<uint8_t, 256> buttonStates = { 0, };

	std::pair<XMINT2, XMINT2> mousePositions = { {INT32_MIN, INT32_MIN}, {INT32_MIN, INT32_MIN} };

public:
	static InputManager& GetInstance() {
		static InputManager inst;
		return inst;
	}

	inline void HandlePressed(uint8_t keycode) {
		auto value = buttonStates[keycode];
		// Set just pressed flag only if it was released right before.
		// xx1 -> xx1
		// xx0 -> x11
		auto invCurrentFlag = (value ^ (value | 1));
		buttonStates[keycode] |= (invCurrentFlag << 1) | 1;
	}
	inline void HandleReleased(uint8_t keycode) {
		auto value = buttonStates[keycode];
		buttonStates[keycode] = ((value & 6) | 4);
	}
	inline void ResetPreviousStates() {
		for (auto& state : buttonStates) {
			state &= 1;
		}
		mousePositions.first = mousePositions.second;
	}
	inline void HandleMouseMovement(int x, int y) {
		mousePositions.second = XMINT2{ x, y };
		if (mousePositions.first.x == INT32_MIN && mousePositions.first.y == INT32_MIN) {
			mousePositions.first = mousePositions.second;
		}
	}

	inline bool IsPressed(uint8_t keycode) const {
		return buttonStates[keycode] & 1;
	}
	inline bool IsReleased(uint8_t keycode) const {
		return !IsPressed(keycode);
	}
	// Note that this could return true even the key is released currently if key press and release happen in one frame.
	inline bool IsJustPressed(uint8_t keycode) const {
		return buttonStates[keycode] & 2;
	}
	// Note that this could return true even the key is pressed currently if key press and release happen in one frame.
	inline bool IsJustReleased(uint8_t keycode) const {
		return buttonStates[keycode] & 4;
	}
	inline XMINT2 GetMousePos() const {
		return mousePositions.second;
	}
	inline XMINT2 GetMouseMotionVec() const {
		return {
			mousePositions.second.x - mousePositions.first.x,
			mousePositions.second.y - mousePositions.first.y
		};
	}
};