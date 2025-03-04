// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SDL3/SDL.h>
#include "common/config.h"
#include "common/logging/log.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/pad/pad.h"
#include "input/controller.h"

namespace Input {

GameController::GameController() {
    m_states_num = 0;
    m_last_state = State();
}

void GameController::ReadState(State* state, bool* isConnected, int* connectedCount) {
    std::scoped_lock lock{m_mutex};

    *isConnected = m_connected;
    *connectedCount = m_connected_count;
    *state = GetLastState();
}

int GameController::ReadStates(State* states, int states_num, bool* isConnected,
                               int* connectedCount) {
    std::scoped_lock lock{m_mutex};

    *isConnected = m_connected;
    *connectedCount = m_connected_count;

    int ret_num = 0;

    if (m_connected) {
        if (m_states_num == 0) {
            ret_num = 1;
            states[0] = m_last_state;
        } else {
            for (uint32_t i = 0; i < m_states_num; i++) {
                if (ret_num >= states_num) {
                    break;
                }
                auto index = (m_first_state + i) % MAX_STATES;
                if (!m_private[index].obtained) {
                    m_private[index].obtained = true;

                    states[ret_num++] = m_states[index];
                }
            }
        }
    }

    return ret_num;
}

State GameController::GetLastState() const {
    if (m_states_num == 0) {
        return m_last_state;
    }
    const u32 last = (m_first_state + m_states_num - 1) % MAX_STATES;
    return m_states[last];
}

void GameController::AddState(const State& state) {
    if (m_states_num >= MAX_STATES) {
        m_states_num = MAX_STATES - 1;
        m_first_state = (m_first_state + 1) % MAX_STATES;
    }

    const u32 index = (m_first_state + m_states_num) % MAX_STATES;
    m_states[index] = state;
    m_last_state = state;
    m_private[index].obtained = false;
    m_states_num++;
}

void GameController::CheckButton(int id, Libraries::Pad::OrbisPadButtonDataOffset button,
                                 bool is_pressed) {
    std::scoped_lock lock{m_mutex};
    auto state = GetLastState();
    state.time = Libraries::Kernel::sceKernelGetProcessTime();
    if (is_pressed) {
        state.buttonsState |= button;
    } else {
        state.buttonsState &= ~button;
    }

    AddState(state);
}

void GameController::Axis(int id, Input::Axis axis, int value) {
    using Libraries::Pad::OrbisPadButtonDataOffset;

    std::scoped_lock lock{m_mutex};
    auto state = GetLastState();

    state.time = Libraries::Kernel::sceKernelGetProcessTime();
    int axis_id = static_cast<int>(axis);
    state.axes[axis_id] = value;

    if (axis == Input::Axis::TriggerLeft) {
        if (value > 0) {
            state.buttonsState |= OrbisPadButtonDataOffset::L2;
        } else {
            state.buttonsState &= ~OrbisPadButtonDataOffset::L2;
        }
    }

    if (axis == Input::Axis::TriggerRight) {
        if (value > 0) {
            state.buttonsState |= OrbisPadButtonDataOffset::R2;
        } else {
            state.buttonsState &= ~OrbisPadButtonDataOffset::R2;
        }
    }

    AddState(state);
}

void GameController::Gyro(int id, const float gyro[3]) {
    std::scoped_lock lock{m_mutex};
    auto state = GetLastState();
    state.time = Libraries::Kernel::sceKernelGetProcessTime();

    // Update the angular velocity (gyro data)
    state.angularVelocity.x = gyro[0]; // X-axis
    state.angularVelocity.y = gyro[1]; // Y-axis
    state.angularVelocity.z = gyro[2]; // Z-axis

    AddState(state);
}
void GameController::Acceleration(int id, const float acceleration[3]) {
    std::scoped_lock lock{m_mutex};
    auto state = GetLastState();
    state.time = Libraries::Kernel::sceKernelGetProcessTime();

    // Update the acceleration values
    state.acceleration.x = acceleration[0]; // X-axis
    state.acceleration.y = acceleration[1]; // Y-axis
    state.acceleration.z = acceleration[2]; // Z-axis

    AddState(state);
}

// Stolen from
// https://github.com/xioTechnologies/Open-Source-AHRS-With-x-IMU/blob/master/x-IMU%20IMU%20and%20AHRS%20Algorithms/x-IMU%20IMU%20and%20AHRS%20Algorithms/AHRS/MahonyAHRS.cs
float eInt[3] = {0.0f, 0.0f, 0.0f}; // Integral error terms
const float Kp = 50.0f;             // Proportional gain
const float Ki = 1.0f;              // Integral gain
Libraries::Pad::OrbisFQuaternion o = {1, 0, 0, 0};
void GameController::CalculateOrientation(Libraries::Pad::OrbisFVector3& acceleration,
                                          Libraries::Pad::OrbisFVector3& angularVelocity,
                                          float deltaTime,
                                          Libraries::Pad::OrbisFQuaternion& orientation) {
    float ax = acceleration.x, ay = acceleration.y, az = acceleration.z;
    float gx = angularVelocity.x, gy = angularVelocity.y, gz = angularVelocity.z;

    float q1 = o.w, q2 = o.x, q3 = o.y, q4 = o.z;

    // Normalize accelerometer measurement
    float norm = std::sqrt(ax * ax + ay * ay + az * az);
    if (norm == 0.0f)
        return; // Handle NaN
    norm = 1.0f / norm;
    ax *= norm;
    ay *= norm;
    az *= norm;

    // Estimated direction of gravity
    float vx = 2.0f * (q2 * q4 - q1 * q3);
    float vy = 2.0f * (q1 * q2 + q3 * q4);
    float vz = q1 * q1 - q2 * q2 - q3 * q3 + q4 * q4;

    // Error is cross product between estimated direction and measured direction of gravity
    float ex = (ay * vz - az * vy);
    float ey = (az * vx - ax * vz);
    float ez = (ax * vy - ay * vx);
    if (Ki > 0.0f) {
        eInt[0] += ex * deltaTime; // Accumulate integral error
        eInt[1] += ey * deltaTime;
        eInt[2] += ez * deltaTime;
    } else {
        eInt[0] = eInt[1] = eInt[2] = 0.0f; // Prevent integral wind-up
    }

    // Apply feedback terms
    gx += Kp * ex + Ki * eInt[0];
    gy += Kp * ey + Ki * eInt[1];
    gz += Kp * ez + Ki * eInt[2];

    //// Integrate rate of change of quaternion
    q1 += (-q2 * gx - q3 * gy - q4 * gz) * (0.5f * deltaTime);
    q2 += (q1 * gx + q3 * gz - q4 * gy) * (0.5f * deltaTime);
    q3 += (q1 * gy - q2 * gz + q4 * gx) * (0.5f * deltaTime);
    q4 += (q1 * gz + q2 * gy - q3 * gx) * (0.5f * deltaTime);

    // Normalize quaternion
    norm = std::sqrt(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);
    norm = 1.0f / norm;
    orientation.w = q1 * norm;
    orientation.x = q2 * norm;
    orientation.y = q3 * norm;
    orientation.z = q4 * norm;
    o.w = q1 * norm;
    o.x = q2 * norm;
    o.y = q3 * norm;
    o.z = q4 * norm;
    LOG_DEBUG(Lib_Pad, "Calculated orientation: {:.2f} {:.2f} {:.2f} {:.2f}", orientation.x,
              orientation.y, orientation.z, orientation.w);
}

void GameController::SetLightBarRGB(u8 r, u8 g, u8 b) {
    if (m_sdl_gamepad != nullptr) {
        SDL_SetGamepadLED(m_sdl_gamepad, r, g, b);
    }
}

bool GameController::SetVibration(u8 smallMotor, u8 largeMotor) {
    if (m_sdl_gamepad != nullptr) {
        return SDL_RumbleGamepad(m_sdl_gamepad, (smallMotor / 255.0f) * 0xFFFF,
                                 (largeMotor / 255.0f) * 0xFFFF, -1);
    }
    return true;
}

void GameController::SetTouchpadState(int touchIndex, bool touchDown, float x, float y) {
    if (touchIndex < 2) {
        std::scoped_lock lock{m_mutex};
        auto state = GetLastState();
        state.time = Libraries::Kernel::sceKernelGetProcessTime();

        state.touchpad[touchIndex].state = touchDown;
        state.touchpad[touchIndex].x = static_cast<u16>(x * 1920);
        state.touchpad[touchIndex].y = static_cast<u16>(y * 941);

        AddState(state);
    }
}

void GameController::TryOpenSDLController() {
    if (m_sdl_gamepad == nullptr || !SDL_GamepadConnected(m_sdl_gamepad)) {
        int gamepad_count;
        SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
        m_sdl_gamepad = gamepad_count > 0 ? SDL_OpenGamepad(gamepads[0]) : nullptr;
        if (Config::getIsMotionControlsEnabled()) {
            if (SDL_SetGamepadSensorEnabled(m_sdl_gamepad, SDL_SENSOR_GYRO, true)) {
                gyro_poll_rate = SDL_GetGamepadSensorDataRate(m_sdl_gamepad, SDL_SENSOR_GYRO);
                LOG_INFO(Input, "Gyro initialized, poll rate: {}", gyro_poll_rate);
            } else {
                LOG_ERROR(Input, "Failed to initialize gyro controls for gamepad");
            }
            if (SDL_SetGamepadSensorEnabled(m_sdl_gamepad, SDL_SENSOR_ACCEL, true)) {
                accel_poll_rate = SDL_GetGamepadSensorDataRate(m_sdl_gamepad, SDL_SENSOR_ACCEL);
                LOG_INFO(Input, "Accel initialized, poll rate: {}", accel_poll_rate);
            } else {
                LOG_ERROR(Input, "Failed to initialize accel controls for gamepad");
            }
        }

        SDL_free(gamepads);

        SetLightBarRGB(0, 0, 255);
    }
}

u32 GameController::Poll() {
    std::scoped_lock lock{m_mutex};
    if (m_connected) {
        auto time = Libraries::Kernel::sceKernelGetProcessTime();
        if (m_states_num == 0) {
            auto diff = (time - m_last_state.time) / 1000;
            if (diff >= 100) {
                AddState(GetLastState());
            }
        } else {
            auto index = (m_first_state - 1 + m_states_num) % MAX_STATES;
            auto diff = (time - m_states[index].time) / 1000;
            if (m_private[index].obtained && diff >= 100) {
                AddState(GetLastState());
            }
        }
    }
    return 100;
}

} // namespace Input
