# Choose a look, then set its strength

Make Grain Now 0.2 keeps its everyday controls small: Look, Grain type, Overall strength, New look and New variation. Advanced starts collapsed.

| Look | Intended starting point |
| --- | --- |
| Fine & gentle | Fine, restrained texture and glow |
| Soft color | Moderate color grain with soft glow; default |
| Coarse monochrome | Larger, monochrome texture |
| Warm lights | Stronger warm halation |
| Dream bloom | Broad, prominent diffusion |
| Gritty color | Dense color texture |
| Clean detail | Very subtle grain and restrained highlights |
| Soft portrait | Gentle grain and diffusion |
| Night lights | Stronger shadow texture, bright-light halation |
| Subtle warmth | Restrained texture with warm highlights |
| Dreamy monochrome | Monochrome texture with broad bloom |
| Heavy texture | Large, dense texture and restrained glow |

These are creative starting points, not emulations of named film stocks. They set grain amount/size/color/response, both glow amounts/radii, and shared highlight extraction. They preserve Grain type, Input encoding, Overall strength and Grain seed. The original six choices retain their order and original seven style values; explicitly choosing a look now also sets its tonal response and extraction instead of inheriting them.

**Overall strength** blends the completed effect with the incoming RGB image. 100 uses the full result; 0 is exact bypass, including native alpha/hidden pixels. It does not change the grain pattern, size or thresholds. Alpha stays unchanged.

**New look** chooses a different complete look and refreshes the grain seed when grain settings are unlocked. **New variation** changes the current look within bounded ranges, preserving Grain seed and keeping zero-strength modules/monochrome color off. Variation range is under Advanced, default 25. These are one-time saved actions. Adobe Undo restores the previous state.

Advanced contains independent module presets (including Off for both glows), individual controls, settings locks, input encoding, seeds and New grain pattern. A manual style change or module preset sets Look to Custom. Module selectors also show Custom after a full look or manual changes.

**Locks** preserve their module's settings during New look / New variation. They do not prohibit deliberate manual edits, explicit look/module selection or New grain pattern. Threshold and softness are shared by halation and bloom; locking either preserves both shared values. An unlocked glow can still change final luminance, which influences downstream grain response. Locks preserve settings, rather than isolating a module's final pixels from other processing.

**New grain pattern** changes Grain seed only. Explore seed stores the repeatable state used for look/variation exploration; it is separate from the rendered grain pattern. Input encoding remains an explicit choice for the pixels arriving from upstream effects.

**Gaussian (fast)** and **Simulated (slow)** keep their existing algorithms and size mappings. Simulated is the default for newly added instances. Older projects keep their saved model. Slower does not establish greater physical accuracy; Simulated is an experimental equal-radius particle approximation and costs more GPU time, particularly at 4K50.
