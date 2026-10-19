//! Small, allocation-reusing WSOLA stretcher for the mobile PCM path.
//!
//! Zakuro produces one audio slice per emulated frame (about 546 stereo
//! frames at 60 Hz). Mobile frame work can take longer than 1/60 s, so passing
//! each slice straight to AVAudioPlayerNode/AudioTrack leaves gaps. This keeps
//! pitch while extending slices to match their wall-clock interval.

use std::time::Instant;

const SAMPLE_RATE: f64 = 32_728.0;
const GRAIN: usize = 512;
const HOP: usize = GRAIN / 2;
const SEARCH: usize = 128;
const MAX_STRETCH: f64 = 2.75;

pub struct AudioStretch {
    input: Vec<[i16; 2]>,
    position: f64,
    last: Option<usize>,
    tail: Vec<[f32; 2]>,
    factor: f64,
    window: Vec<f32>,
    last_frame_at: Instant,
}

impl AudioStretch {
    pub fn new() -> Self {
        let window = (0..GRAIN)
            .map(|i| 0.5 - 0.5 * (std::f32::consts::TAU * i as f32 / GRAIN as f32).cos())
            .collect();
        Self {
            input: Vec::with_capacity(GRAIN * 4),
            position: 0.0,
            last: None,
            tail: vec![[0.0; 2]; HOP],
            factor: 1.0,
            window,
            last_frame_at: Instant::now(),
        }
    }

    /// Add the slice from the latest emulated frame, stretching only as much
    /// as needed to provide roughly one console sample per wall-clock sample.
    pub fn push(&mut self, samples: &[[i16; 2]], out: &mut Vec<[i16; 2]>) {
        let now = Instant::now();
        let elapsed = now.duration_since(self.last_frame_at).as_secs_f64();
        self.last_frame_at = now;
        if samples.is_empty() {
            return;
        }

        // A brief scheduling hiccup must not slow the whole soundtrack. Keep
        // this bounded to avoid extreme, audible repetition on very slow CPUs.
        let wanted = desired_stretch(elapsed, samples.len());
        self.process(samples, wanted, out);
    }

    fn process(&mut self, samples: &[[i16; 2]], wanted: f64, out: &mut Vec<[i16; 2]>) {
        self.input.extend_from_slice(samples);
        self.factor += (wanted - self.factor) * 0.2;
        if wanted == 1.0 && self.factor < 1.001 {
            self.factor = 1.0;
        }

        loop {
            if let Some(last) = self.last.filter(|_| self.factor == 1.0) {
                self.position = (last + HOP) as f64;
            }
            let due = self.position as usize;
            let natural = self.last.map_or(due, |last| last + HOP);
            if due + SEARCH + GRAIN > self.input.len() || natural + GRAIN > self.input.len() {
                break;
            }

            let start = if natural == due {
                due
            } else {
                let template = &self.input[natural..natural + HOP];
                let score = |candidate: usize| {
                    let (mut dot, mut energy) = (0.0f32, 1e-9f32);
                    for (a, b) in self.input[candidate..candidate + HOP].iter().zip(template) {
                        let left = a[0] as f32 + a[1] as f32;
                        let right = b[0] as f32 + b[1] as f32;
                        dot += left * right;
                        energy += left * left;
                    }
                    dot / energy.sqrt()
                };
                (due.saturating_sub(SEARCH)..=due + SEARCH)
                    .map(|candidate| (score(candidate), candidate))
                    .max_by(|(a, x), (b, y)| {
                        a.total_cmp(b)
                            .then(natural.abs_diff(*y).cmp(&natural.abs_diff(*x)))
                    })
                    .map_or(due, |(_, candidate)| candidate)
            };

            let (first, second) = self.input[start..start + GRAIN].split_at(HOP);
            let (rising, falling) = self.window.split_at(HOP);
            for ((tail, sample), weight) in self.tail.iter_mut().zip(first).zip(rising) {
                out.push([
                    to_i16(tail[0] + sample[0] as f32 * weight),
                    to_i16(tail[1] + sample[1] as f32 * weight),
                ]);
            }
            for ((tail, sample), weight) in self.tail.iter_mut().zip(second).zip(falling) {
                *tail = [sample[0] as f32 * weight, sample[1] as f32 * weight];
            }
            self.last = Some(start);
            self.position += HOP as f64 / self.factor;
        }

        // Retain enough context for the next correlation search, but avoid
        // growing the scratch buffer over a long session.
        let used = (self.position as usize)
            .min(self.last.unwrap_or(0))
            .saturating_sub(SEARCH);
        if used > 0 {
            self.input.drain(..used);
            self.position -= used as f64;
            self.last = self.last.map(|last| last - used);
        }
    }
}

impl Default for AudioStretch {
    fn default() -> Self {
        Self::new()
    }
}

fn to_i16(sample: f32) -> i16 {
    sample.round().clamp(i16::MIN as f32, i16::MAX as f32) as i16
}

fn desired_stretch(elapsed_seconds: f64, input_frames: usize) -> f64 {
    (elapsed_seconds * SAMPLE_RATE / input_frames.max(1) as f64).clamp(1.0, MAX_STRETCH)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tone(count: usize) -> Vec<[i16; 2]> {
        (0..count)
            .map(|i| {
                let sample = (std::f32::consts::TAU * 440.0 * i as f32 / SAMPLE_RATE as f32).sin()
                    * 12_000.0;
                [sample as i16, sample as i16]
            })
            .collect()
    }

    #[test]
    fn matches_audio_rate_to_frame_time_with_a_safe_limit() {
        let frame_samples = 32_728.0 / 60.0;
        assert!((desired_stretch(1.0 / 60.0, frame_samples as usize) - 1.0).abs() < 0.01);
        assert!((desired_stretch(1.0 / 30.0, frame_samples as usize) - 2.0).abs() < 0.01);
        assert_eq!(desired_stretch(1.0 / 3.0, frame_samples as usize), MAX_STRETCH);
    }

    #[test]
    fn stretches_a_tone_without_lowering_its_pitch() {
        let input = tone(32_728);
        let mut stretcher = AudioStretch::new();
        let mut output = Vec::with_capacity(input.len() * 2);
        for chunk in input.chunks(546) {
            stretcher.process(chunk, 2.0, &mut output);
        }

        let ratio = output.len() as f64 / input.len() as f64;
        assert!(ratio > 1.8 && ratio < 2.1, "stretched by {ratio}");
        let crossings = output
            .windows(2)
            .skip(HOP)
            .filter(|pair| pair[0][0] < 0 && pair[1][0] >= 0)
            .count();
        let frequency = crossings as f64 / (output.len() - HOP) as f64 * SAMPLE_RATE;
        assert!((frequency - 440.0).abs() < 20.0, "pitch {frequency}");
    }

    #[test]
    fn ordinary_rate_audio_is_not_stretched() {
        let input = tone(20_000);
        let mut stretcher = AudioStretch::new();
        let mut output = Vec::with_capacity(input.len());
        for chunk in input.chunks(546) {
            stretcher.process(chunk, 1.0, &mut output);
        }
        assert!(output.len() > 18_000);
        for (actual, expected) in output[HOP..].iter().zip(&input[HOP..]) {
            assert!((actual[0] as i32 - expected[0] as i32).abs() < 8);
        }
    }
}
