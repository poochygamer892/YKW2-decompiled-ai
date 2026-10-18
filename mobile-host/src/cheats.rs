//! Built-in YW2 performance patches. These are applied by Zakuro directly to
//! the emulated title memory; they are not Citra cheat files or UI settings.

use std::sync::OnceLock;
use zakuro_core::memory::{Memory, MemoryState};
use zakuro_core::System;

const YW2_TITLE_ID: u64 = 0x0004_0000_001B_2900;
const R_BUTTON: u32 = 1 << 8;
const CHEAT_SOURCE: &str = include_str!("../../cheats/00040000001B2900.txt");

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Width {
    Byte,
    Word,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Condition {
    Always,
    Buttons(u32),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Write {
    address: u32,
    value: u32,
    width: Width,
    condition: Condition,
}

fn parse_cheats(source: &str) -> Result<Vec<Write>, &'static str> {
    let mut writes = Vec::new();
    let mut base = 0u32;
    let mut condition = Condition::Always;

    for line in source.lines().map(str::trim) {
        if line.is_empty() || line.starts_with('#') || line.starts_with('[') {
            continue;
        }
        let mut words = line.split_whitespace();
        let command = u32::from_str_radix(words.next().ok_or("missing command")?, 16)
            .map_err(|_| "invalid command")?;
        let value = u32::from_str_radix(words.next().ok_or("missing value")?, 16)
            .map_err(|_| "invalid value")?;
        if words.next().is_some() {
            return Err("unexpected fields in cheat command");
        }

        match command & 0xFF00_0000 {
            0xD300_0000 => base = value,
            0xDD00_0000 => condition = Condition::Buttons(value),
            0xD200_0000 => {
                base = 0;
                condition = Condition::Always;
            }
            _ => {
                let address = base.wrapping_add(command & 0x0FFF_FFFF);
                let width = match command >> 28 {
                    0 => Width::Word,
                    2 => Width::Byte,
                    _ => return Err("unsupported cheat command"),
                };
                let write = Write {
                    address,
                    value,
                    width,
                    condition,
                };
                // FPS++ repeats the unconditional 60-FPS byte write from the
                // R=30 code. Deduplicating it lets the conditional R override
                // remain effective when both original entries are enabled.
                if !writes.contains(&write) {
                    writes.push(write);
                }
            }
        }
    }
    Ok(writes)
}

fn code_range_is_mapped(memory: &Memory, address: u32, length: u32) -> bool {
    (0..length).all(|offset| {
        memory
            .mapping_at(address.wrapping_add(offset))
            .is_some_and(|mapping| mapping.state == MemoryState::Code)
    })
}

fn apply_write(memory: &mut Memory, write: Write) {
    let length = match write.width {
        Width::Byte => 4,
        Width::Word => 4,
    };
    if !code_range_is_mapped(memory, write.address, length) {
        return;
    }

    match write.width {
        Width::Byte => {
            // Zakuro exposes privileged word writes; preserve the neighboring
            // three bytes so this 8-bit cheat cannot corrupt adjacent code.
            let mut bytes = [0; 4];
            memory.read_bytes(write.address, &mut bytes);
            bytes[0] = write.value as u8;
            memory.write32_privileged(write.address, u32::from_le_bytes(bytes));
        }
        Width::Word => {
            memory.write32_privileged(write.address, write.value);
        }
    }
}

pub(super) fn apply_all(system: &mut System, buttons: u32) {
    if !system
        .title
        .as_ref()
        .is_some_and(|title| title.program_id() == YW2_TITLE_ID)
    {
        return;
    }

    static WRITES: OnceLock<Vec<Write>> = OnceLock::new();
    let writes = WRITES.get_or_init(|| parse_cheats(CHEAT_SOURCE).unwrap_or_default());
    for &write in writes {
        if matches!(write.condition, Condition::Always)
            || matches!(write.condition, Condition::Buttons(mask) if buttons & mask == mask)
        {
            apply_write(&mut system.memory, write);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bundled_cheats_parse_and_merge_the_duplicate_fps_write() {
        let writes = parse_cheats(CHEAT_SOURCE).unwrap();
        assert_eq!(writes.len(), 3);
        assert!(writes.contains(&Write {
            address: 0x0861_28D,
            value: 1,
            width: Width::Byte,
            condition: Condition::Always,
        }));
        assert!(writes.contains(&Write {
            address: 0x0861_28D,
            value: 2,
            width: Width::Byte,
            condition: Condition::Buttons(R_BUTTON),
        }));
        assert!(writes.contains(&Write {
            address: 0x0086_1E5A,
            value: 0x3F80_0000,
            width: Width::Word,
            condition: Condition::Always,
        }));
    }

    #[test]
    fn r_button_only_selects_the_thirty_fps_override() {
        let writes = parse_cheats(CHEAT_SOURCE).unwrap();
        let active = |buttons| {
            writes
                .iter()
                .filter(|write| {
                    matches!(write.condition, Condition::Always)
                        || matches!(write.condition, Condition::Buttons(mask) if buttons & mask == mask)
                })
                .copied()
                .collect::<Vec<_>>()
        };

        assert_eq!(active(0).len(), 2);
        assert_eq!(active(R_BUTTON).len(), 3);
    }
}
