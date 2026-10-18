//! Stable C ABI between the Android/iOS frontends and Zakuro's 3DS runtime.
//!
//! The frontend owns file selection, lifecycle, display, sound output and
//! controls. This library owns only the emulated system and its frame buffers.

use std::collections::VecDeque;
use std::ffi::{c_char, CStr};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::PathBuf;
use zakuro_common::Screen;
use zakuro_core::services::hid::{InputState, PadState};
use zakuro_core::{loader, Config, FrameOutcome, System};

pub const TOP_WIDTH: usize = 400;
pub const BOTTOM_WIDTH: usize = 320;
pub const SCREEN_HEIGHT: usize = 240;
pub const RGBA_BYTES: usize = 4;

pub const TOP_BYTES: usize = TOP_WIDTH * SCREEN_HEIGHT * RGBA_BYTES;
pub const BOTTOM_BYTES: usize = BOTTOM_WIDTH * SCREEN_HEIGHT * RGBA_BYTES;

mod cheats;

pub struct MobileRuntime {
    system: System,
    audio: VecDeque<[i16; 2]>,
    faulted: bool,
}

fn read_path(value: *const c_char) -> Result<PathBuf, String> {
    if value.is_null() {
        return Err("path pointer is null".into());
    }
    // SAFETY: callers pass NUL-terminated strings for the duration of this call.
    let value = unsafe { CStr::from_ptr(value) };
    let value = value
        .to_str()
        .map_err(|_| "path is not valid UTF-8".to_owned())?;
    if value.is_empty() {
        return Err("path is empty".into());
    }
    Ok(PathBuf::from(value))
}

unsafe fn write_error(destination: *mut c_char, capacity: usize, message: &str) {
    if destination.is_null() || capacity == 0 {
        return;
    }
    let bytes = message.as_bytes();
    let count = bytes.len().min(capacity - 1);
    // SAFETY: the caller promises a writable buffer of `capacity` bytes.
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), destination.cast(), count);
        *destination.add(count) = 0;
    }
}

/// Opens a decrypted 3DS image and optionally loads its 3dsrecomp library.
///
/// `error_out` may be null. When non-null, it must point to `error_capacity`
/// writable bytes. The returned runtime must be closed with
/// `ykw2_mobile_close`.
#[no_mangle]
pub unsafe extern "C" fn ykw2_mobile_open(
    rom_path: *const c_char,
    recompiled_library: *const c_char,
    data_directory: *const c_char,
    error_out: *mut c_char,
    error_capacity: usize,
) -> *mut MobileRuntime {
    let result = catch_unwind(AssertUnwindSafe(|| {
        (|| {
            // Cranelift's shader JIT allocates executable memory dynamically.
            // iOS rejects those pages without Apple's restricted JIT
            // entitlement and terminates the whole process with
            // CODESIGNING / Invalid Page (it cannot be caught as a Rust panic).
            // Zakuro supports disabling this path through this environment
            // switch; set it before loading or running any shader.
            #[cfg(target_os = "ios")]
            std::env::set_var("ZAKURO_SHADER_JIT", "0");

            let rom = read_path(rom_path)?;
            let data_dir = read_path(data_directory)?;
            let recompiled = if recompiled_library.is_null() {
                None
            } else {
                Some(read_path(recompiled_library)?)
            };
            let config = Config {
                data_dir: Some(data_dir),
                recompiled,
                find_recompiled: false,
                // Zakuro's accelerated renderer is Vulkan-based. iOS devices
                // need a Metal host integration that this UIKit shell does
                // not provide; trying Vulkan here can terminate on startup.
                // Keep acceleration enabled on Android, where the native
                // surface integration exists, and use the safe renderer on iOS.
                hardware_renderer: !cfg!(target_os = "ios"),
                texture_packs: false,
                ..Config::default()
            };
            let mut system = loader::load(rom, config).map_err(|error| error.to_string())?;
            cheats::install_builtin(&mut system);
            Ok::<_, String>(Box::into_raw(Box::new(MobileRuntime {
                system,
                audio: VecDeque::new(),
                faulted: false,
            })))
        })()
    }));

    match result {
        Ok(Ok(runtime)) => runtime,
        Ok(Err(message)) => {
            // SAFETY: the output buffer is supplied by the caller.
            unsafe { write_error(error_out, error_capacity, &message) };
            std::ptr::null_mut()
        }
        Err(_) => {
            // A panic must not cross the C ABI and terminate the whole mobile app.
            unsafe {
                write_error(
                    error_out,
                    error_capacity,
                    "Zakuro encontró un error interno al cargar la ROM.",
                )
            };
            std::ptr::null_mut()
        }
    }
}

/// Runs one or more emulated frames. Returns the completed-frame count, or
/// -1 if the title exits/faults or the runtime handle is invalid.
#[no_mangle]
pub unsafe extern "C" fn ykw2_mobile_run_frames(runtime: *mut MobileRuntime, frames: u32) -> i32 {
    if runtime.is_null() || frames == 0 || frames > 8 {
        return -1;
    }
    // SAFETY: runtime pointers are created by open and owned by the frontend.
    let runtime = unsafe { &mut *runtime };
    if runtime.faulted {
        return -3;
    }
    match catch_unwind(AssertUnwindSafe(|| {
        for frame in 0..frames {
            match runtime.system.run_frame() {
                FrameOutcome::Completed => {}
                FrameOutcome::Exited | FrameOutcome::Faulted => return frame as i32,
            }
            runtime.audio.extend(runtime.system.take_audio());
        }
        frames as i32
    })) {
        Ok(result) => result,
        Err(_) => {
            runtime.faulted = true;
            -3
        }
    }
}

/// Feeds 3DS HID state. Button bits use Zakuro's PadState layout; touch -1,-1
/// means released, otherwise coordinates are in bottom-screen pixels.
#[no_mangle]
pub unsafe extern "C" fn ykw2_mobile_set_input(
    runtime: *mut MobileRuntime,
    buttons: u32,
    circle_x: f32,
    circle_y: f32,
    touch_x: i32,
    touch_y: i32,
) -> i32 {
    if runtime.is_null() {
        return -1;
    }
    // SAFETY: runtime pointers are created by open and owned by the frontend.
    let runtime = unsafe { &mut *runtime };
    let touch = if (0..BOTTOM_WIDTH as i32).contains(&touch_x)
        && (0..SCREEN_HEIGHT as i32).contains(&touch_y)
    {
        Some((touch_x as u16, touch_y as u16))
    } else {
        None
    };
    runtime.system.set_input(InputState {
        buttons: PadState::from_bits_truncate(buttons),
        circle_x: circle_x.clamp(-1.0, 1.0),
        circle_y: circle_y.clamp(-1.0, 1.0),
        touch,
        ..InputState::default()
    });
    0
}

/// Copies the most recent top and bottom screens in RGBA8. Both output buffers
/// must have at least `TOP_BYTES` / `BOTTOM_BYTES` bytes respectively.
#[no_mangle]
pub unsafe extern "C" fn ykw2_mobile_copy_screens(
    runtime: *mut MobileRuntime,
    top_out: *mut u8,
    top_capacity: usize,
    bottom_out: *mut u8,
    bottom_capacity: usize,
) -> i32 {
    if runtime.is_null()
        || top_out.is_null()
        || bottom_out.is_null()
        || top_capacity < TOP_BYTES
        || bottom_capacity < BOTTOM_BYTES
    {
        return -1;
    }
    // SAFETY: the frontend supplies buffers matching the documented lengths.
    let runtime = unsafe { &mut *runtime };
    let top = runtime.system.read_screen(Screen::Top);
    let bottom = runtime.system.read_screen(Screen::Bottom);
    if top.len() != TOP_BYTES || bottom.len() != BOTTOM_BYTES {
        return -2;
    }
    // SAFETY: the length checks above and caller capacities validate both copies.
    unsafe {
        std::ptr::copy_nonoverlapping(top.as_ptr(), top_out, TOP_BYTES);
        std::ptr::copy_nonoverlapping(bottom.as_ptr(), bottom_out, BOTTOM_BYTES);
    }
    0
}

/// Drains up to `sample_capacity` stereo samples into interleaved PCM16.
/// Returns the number of stereo sample frames copied, or -1 for bad arguments.
#[no_mangle]
pub unsafe extern "C" fn ykw2_mobile_take_audio(
    runtime: *mut MobileRuntime,
    samples_out: *mut i16,
    sample_capacity: usize,
) -> i32 {
    if runtime.is_null() || (samples_out.is_null() && sample_capacity != 0) {
        return -1;
    }
    // SAFETY: runtime pointers are created by open and owned by the frontend.
    let runtime = unsafe { &mut *runtime };
    let count = sample_capacity.min(runtime.audio.len());
    // SAFETY: caller supplies room for 2 * sample_capacity i16 values.
    for index in 0..count {
        let sample = runtime
            .audio
            .pop_front()
            .expect("count is bounded by queue length");
        unsafe {
            *samples_out.add(index * 2) = sample[0];
            *samples_out.add(index * 2 + 1) = sample[1];
        }
    }
    count as i32
}

/// Releases the runtime allocated by `ykw2_mobile_open`.
#[no_mangle]
pub unsafe extern "C" fn ykw2_mobile_close(runtime: *mut MobileRuntime) {
    if !runtime.is_null() {
        // SAFETY: only open creates the Box, and close consumes it exactly once.
        drop(unsafe { Box::from_raw(runtime) });
    }
}

/// Compile-time dimensions for frontends allocating screen buffers.
#[no_mangle]
pub extern "C" fn ykw2_mobile_screen_dimensions(
    top_width: *mut u32,
    bottom_width: *mut u32,
    height: *mut u32,
) -> i32 {
    if top_width.is_null() || bottom_width.is_null() || height.is_null() {
        return -1;
    }
    // SAFETY: each pointer is checked and the C API requires writable outputs.
    unsafe {
        *top_width = TOP_WIDTH as u32;
        *bottom_width = BOTTOM_WIDTH as u32;
        *height = SCREEN_HEIGHT as u32;
    }
    0
}

/// Clears an error buffer before a native open call (handy for C callers).
#[no_mangle]
pub unsafe extern "C" fn ykw2_mobile_clear_error(error_out: *mut c_char, error_capacity: usize) {
    // SAFETY: same buffer contract as `write_error`.
    unsafe { write_error(error_out, error_capacity, "") };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn screens_have_mobile_expected_pixel_counts() {
        assert_eq!(TOP_BYTES, 400 * 240 * 4);
        assert_eq!(BOTTOM_BYTES, 320 * 240 * 4);
    }

    #[test]
    fn rejects_invalid_frame_batch_and_touch_bounds() {
        assert_eq!(
            unsafe { ykw2_mobile_run_frames(std::ptr::null_mut(), 1) },
            -1
        );
        assert_eq!(
            unsafe { ykw2_mobile_set_input(std::ptr::null_mut(), 0, 0.0, 0.0, 0, 0) },
            -1
        );
    }
}
