#include "presentation.h"
#include "ykw2_mobile_host.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr int kTopWidth = 400;
constexpr int kBottomWidth = 320;
constexpr int kHeight = 240;
constexpr int kSampleRate = 32728;
constexpr uint32_t kA = 1u << 0, kB = 1u << 1, kSelect = 1u << 2;
constexpr uint32_t kStart = 1u << 3, kRight = 1u << 4, kLeft = 1u << 5;
constexpr uint32_t kUp = 1u << 6, kDown = 1u << 7, kR = 1u << 8;
constexpr uint32_t kL = 1u << 9, kX = 1u << 10, kY = 1u << 11;

void set_key(uint32_t& buttons, SDL_Scancode key, uint32_t bit) {
    if (SDL_GetKeyboardState(nullptr)[key]) buttons |= bit;
}

void draw_texture(SDL_Renderer* renderer, SDL_Texture* texture,
                  const ykw2::desktop::Rect& rect) {
    const SDL_Rect target{rect.x, rect.y, rect.width, rect.height};
    SDL_RenderCopy(renderer, texture, nullptr, &target);
}

void draw_frame(SDL_Renderer* renderer, SDL_Texture* top, SDL_Texture* bottom,
                int width, int height, bool show_bottom,
                float stylus_x, float stylus_y) {
    SDL_RenderSetLogicalSize(renderer, width, height);
    const auto layout = ykw2::desktop::make_presentation(width, height, show_bottom);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    draw_texture(renderer, top, layout.top);
    if (layout.bottom_visible) {
        SDL_Rect shadow{layout.bottom.x - 4, layout.bottom.y - 4,
                        layout.bottom.width + 8, layout.bottom.height + 8};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 210);
        SDL_RenderFillRect(renderer, &shadow);
        draw_texture(renderer, bottom, layout.bottom);
        const int cursor_x = layout.bottom.x + static_cast<int>(
            std::clamp(stylus_x, 0.0f, 319.0f) * layout.bottom.width / 320.0f);
        const int cursor_y = layout.bottom.y + static_cast<int>(
            std::clamp(stylus_y, 0.0f, 239.0f) * layout.bottom.height / 240.0f);
        SDL_SetRenderDrawColor(renderer, 30, 235, 255, 255);
        SDL_Rect cursor{cursor_x - 7, cursor_y - 7, 14, 14};
        SDL_RenderDrawRect(renderer, &cursor);
        SDL_RenderDrawLine(renderer, cursor_x - 11, cursor_y, cursor_x + 11, cursor_y);
        SDL_RenderDrawLine(renderer, cursor_x, cursor_y - 11, cursor_x, cursor_y + 11);
    }
    SDL_RenderPresent(renderer);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Uso: ykw2_desktop <ROM-3DS-descifrada> [libreria-recompilada]\n"
                  << "F1 muestra/oculta la pantalla táctil; F11 alterna pantalla completa; clic sobre ella para tocar.\n"
                  << "Teclado: WASD, flechas, Z/X/F/G, Q/E, Enter/Backspace.\n"
                  << "Stylus: I/J/K/L mueve; Espacio mantiene toque/arrastre.\n"
                  << "Mando: botones y stick izquierdo; stick derecho mueve stylus, R3 mantiene toque.\n";
        return 2;
    }
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Yo-kai Watch 2 · Desktop",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : nullptr;
    if (!window || !renderer) {
        std::cerr << "No se pudo crear ventana/renderizador SDL: " << SDL_GetError() << '\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    auto* top_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, kTopWidth, kHeight);
    auto* bottom_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, kBottomWidth, kHeight);
    if (!top_texture || !bottom_texture) {
        std::cerr << "No se pudieron crear las texturas SDL: " << SDL_GetError() << '\n';
        SDL_DestroyTexture(top_texture);
        SDL_DestroyTexture(bottom_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    char* preference_path = SDL_GetPrefPath("YKW2", "Desktop");
    if (!preference_path) {
        std::cerr << "SDL no pudo obtener el directorio de datos: " << SDL_GetError() << '\n';
        SDL_DestroyTexture(top_texture); SDL_DestroyTexture(bottom_texture);
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        return 1;
    }
    const auto data_dir = std::filesystem::path(preference_path);
    SDL_free(preference_path);
    std::filesystem::create_directories(data_dir);
    std::array<char, 1024> error{};
    const auto rom = std::filesystem::absolute(argv[1]).string();
    const auto data = data_dir.string();
    const auto recomp = argc == 3 ? std::filesystem::absolute(argv[2]).string() : std::string{};
    MobileRuntime* runtime = ykw2_mobile_open(rom.c_str(),
        recomp.empty() ? nullptr : recomp.c_str(), data.c_str(), error.data(), error.size());
    if (!runtime) {
        std::cerr << "Zakuro no pudo abrir la ROM: " << error.data() << '\n';
        SDL_DestroyTexture(top_texture); SDL_DestroyTexture(bottom_texture);
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        return 1;
    }

    SDL_AudioSpec audio_spec{};
    audio_spec.freq = kSampleRate;
    audio_spec.format = AUDIO_S16SYS;
    audio_spec.channels = 2;
    audio_spec.samples = 2048;
    const SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &audio_spec, nullptr, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);

    std::vector<uint8_t> top(400 * 240 * 4), bottom(320 * 240 * 4);
    std::vector<int16_t> samples(4096 * 2);
    bool running = true, show_bottom = true, touch_down = false;
    uint32_t buttons = 0;
    float circle_x = 0, circle_y = 0;
    float stylus_x = 160.0f, stylus_y = 120.0f;
    int touch_x = -1, touch_y = -1;
    SDL_GameController* controller = nullptr;
    auto previous_tick = std::chrono::steady_clock::now();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_F1)
                show_bottom = !show_bottom;
            if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_F11) {
                const Uint32 flags = SDL_GetWindowFlags(window);
                SDL_SetWindowFullscreen(window,
                    (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED && !controller && SDL_IsGameController(event.cdevice.which))
                controller = SDL_GameControllerOpen(event.cdevice.which);
            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
                SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == event.cdevice.which) {
                SDL_GameControllerClose(controller); controller = nullptr;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h);
                const auto layout = ykw2::desktop::make_presentation(w, h, show_bottom);
                int pointer_x = 0, pointer_y = 0;
                touch_down = ykw2::desktop::map_bottom_touch(layout, event.button.x,
                    event.button.y, pointer_x, pointer_y);
                if (touch_down) {
                    stylus_x = static_cast<float>(pointer_x);
                    stylus_y = static_cast<float>(pointer_y);
                }
            }
            if ((event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
                touch_down = false;
            }
            if (event.type == SDL_MOUSEMOTION && touch_down) {
                int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h);
                const auto layout = ykw2::desktop::make_presentation(w, h, show_bottom);
                int pointer_x = 0, pointer_y = 0;
                if (ykw2::desktop::map_bottom_touch(layout, event.motion.x, event.motion.y,
                                                    pointer_x, pointer_y)) {
                    stylus_x = static_cast<float>(pointer_x);
                    stylus_y = static_cast<float>(pointer_y);
                }
            }
        }

        buttons = 0;
        set_key(buttons, SDL_SCANCODE_Z, kA); set_key(buttons, SDL_SCANCODE_X, kB);
        set_key(buttons, SDL_SCANCODE_F, kY); set_key(buttons, SDL_SCANCODE_G, kX);
        set_key(buttons, SDL_SCANCODE_Q, kL); set_key(buttons, SDL_SCANCODE_E, kR);
        set_key(buttons, SDL_SCANCODE_RETURN, kStart); set_key(buttons, SDL_SCANCODE_BACKSPACE, kSelect);
        set_key(buttons, SDL_SCANCODE_UP, kUp); set_key(buttons, SDL_SCANCODE_DOWN, kDown);
        set_key(buttons, SDL_SCANCODE_LEFT, kLeft); set_key(buttons, SDL_SCANCODE_RIGHT, kRight);
        float stylus_dx = 0.0f, stylus_dy = 0.0f;
        bool controller_touch = false;
        const auto* key_state = SDL_GetKeyboardState(nullptr);
        circle_x = (key_state[SDL_SCANCODE_D] ? 1.0f : 0.0f) -
                   (key_state[SDL_SCANCODE_A] ? 1.0f : 0.0f);
        circle_y = (key_state[SDL_SCANCODE_W] ? 1.0f : 0.0f) -
                   (key_state[SDL_SCANCODE_S] ? 1.0f : 0.0f);
        if (controller) {
            const auto button = [controller](SDL_GameControllerButton b) {
                return SDL_GameControllerGetButton(controller, b) != 0;
            };
            if (button(SDL_CONTROLLER_BUTTON_A)) buttons |= kA;
            if (button(SDL_CONTROLLER_BUTTON_B)) buttons |= kB;
            if (button(SDL_CONTROLLER_BUTTON_X)) buttons |= kX;
            if (button(SDL_CONTROLLER_BUTTON_Y)) buttons |= kY;
            if (button(SDL_CONTROLLER_BUTTON_BACK)) buttons |= kSelect;
            if (button(SDL_CONTROLLER_BUTTON_START)) buttons |= kStart;
            if (button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) buttons |= kL;
            if (button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) buttons |= kR;
            if (button(SDL_CONTROLLER_BUTTON_DPAD_UP)) buttons |= kUp;
            if (button(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) buttons |= kDown;
            if (button(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) buttons |= kLeft;
            if (button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= kRight;
            const Sint16 ax = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
            const Sint16 ay = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
            if (std::abs(ax) >= 5000) circle_x = ax / 32767.0f;
            // Zakuro uses positive Y for forward; SDL's controller Y is positive down.
            if (std::abs(ay) >= 5000) circle_y = -ay / 32767.0f;
            const Sint16 sx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
            const Sint16 sy = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);
            stylus_dx = std::abs(sx) < 5000 ? 0.0f : sx / 32767.0f;
            stylus_dy = std::abs(sy) < 5000 ? 0.0f : sy / 32767.0f;
            controller_touch = button(SDL_CONTROLLER_BUTTON_RIGHTSTICK);
        }

        stylus_dx += (key_state[SDL_SCANCODE_L] ? 1.0f : 0.0f) -
                     (key_state[SDL_SCANCODE_J] ? 1.0f : 0.0f);
        stylus_dy += (key_state[SDL_SCANCODE_K] ? 1.0f : 0.0f) -
                     (key_state[SDL_SCANCODE_I] ? 1.0f : 0.0f);
        const float magnitude = std::sqrt(stylus_dx * stylus_dx + stylus_dy * stylus_dy);
        if (magnitude > 1.0f) { stylus_dx /= magnitude; stylus_dy /= magnitude; }
        const auto now = std::chrono::steady_clock::now();
        const float delta_seconds = std::clamp(
            std::chrono::duration<float>(now - previous_tick).count(), 0.0f, 0.05f);
        stylus_x = std::clamp(stylus_x + stylus_dx * 420.0f * delta_seconds, 0.0f, 319.0f);
        stylus_y = std::clamp(stylus_y + stylus_dy * 420.0f * delta_seconds, 0.0f, 239.0f);
        const bool keyboard_touch = key_state[SDL_SCANCODE_SPACE] != 0;
        const bool touch_active = touch_down || keyboard_touch || controller_touch;
        if (touch_active || std::abs(stylus_dx) > 0.01f || std::abs(stylus_dy) > 0.01f)
            show_bottom = true;
        touch_x = touch_active ? static_cast<int>(std::lround(stylus_x)) : -1;
        touch_y = touch_active ? static_cast<int>(std::lround(stylus_y)) : -1;

        if (ykw2_mobile_set_input(runtime, buttons, circle_x, circle_y,
                                  touch_x, touch_y) != 0 ||
            ykw2_mobile_run_frames(runtime, 1) != 1 ||
            ykw2_mobile_copy_screens(runtime, top.data(), top.size(), bottom.data(), bottom.size()) != 0) {
            std::cerr << "El runtime Zakuro terminó o falló.\n";
            break;
        }
        SDL_UpdateTexture(top_texture, nullptr, top.data(), kTopWidth * 4);
        SDL_UpdateTexture(bottom_texture, nullptr, bottom.data(), kBottomWidth * 4);
        if (audio) {
            const int count = ykw2_mobile_take_audio(runtime, samples.data(), samples.size() / 2);
            if (count > 0 && SDL_GetQueuedAudioSize(audio) < 65536)
                SDL_QueueAudio(audio, samples.data(), static_cast<Uint32>(count * 2 * sizeof(int16_t)));
        }
        int width = 0, height = 0; SDL_GetWindowSize(window, &width, &height);
        draw_frame(renderer, top_texture, bottom_texture, width, height, show_bottom,
                   stylus_x, stylus_y);
        const auto frame_end = std::chrono::steady_clock::now();
        const auto elapsed = frame_end - previous_tick;
        const auto frame = std::chrono::microseconds(16667);
        if (elapsed < frame) std::this_thread::sleep_for(frame - elapsed);
        previous_tick = std::chrono::steady_clock::now();
    }

    if (audio) SDL_CloseAudioDevice(audio);
    if (controller) SDL_GameControllerClose(controller);
    ykw2_mobile_close(runtime);
    SDL_DestroyTexture(top_texture); SDL_DestroyTexture(bottom_texture);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
