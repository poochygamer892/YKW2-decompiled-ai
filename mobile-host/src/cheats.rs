//! Built-in YW2 performance patches, registered with Zakuro's cheat engine.
//!
//! Using the core's engine is important when a recompiled library is loaded:
//! it tracks code-page writes and invalidates stale translated functions.

use zakuro_core::System;

const YW2_TITLE_ID: u64 = 0x0004_0000_001B_2900;
const CHEAT_SOURCE: &str = include_str!("../../cheats/00040000001B2900.txt");

pub(super) fn install_builtin(system: &mut System) {
    if !system
        .title
        .as_ref()
        .is_some_and(|title| title.program_id() == YW2_TITLE_ID)
    {
        return;
    }

    for mut bundled in zakuro_core::cheats::parse(CHEAT_SOURCE) {
        bundled.enabled = true;
        bundled.builtin = true;

        if let Some(existing) = system
            .cheats
            .iter_mut()
            .find(|cheat| cheat.name == bundled.name && cheat.lines() == bundled.lines())
        {
            existing.enabled = true;
        } else {
            system.cheats.push(bundled);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bundled_cheats_parse_with_native_zakuro_engine() {
        let cheats = zakuro_core::cheats::parse(CHEAT_SOURCE);
        assert_eq!(cheats.len(), 3);
        assert_eq!(
            cheats.iter().map(|cheat| cheat.name.as_str()).collect::<Vec<_>>(),
            ["60FPS (FPS++)", "60FPS (Hold R button for 30FPS)", "Game Speed Normal"]
        );
        assert!(cheats.iter().all(|cheat| !cheat.lines().is_empty()));
        assert!(cheats.iter().any(|cheat| {
            cheat.name == "60FPS (FPS++)"
                && cheat.lines().iter().any(|line| line == "2086128D 00000001")
        }));
    }
}
