# TODO - LockScreen smooth power-loss animation

- [ ] Update `src/UI/lockscreen.h` sudden-loss restore loop to be frame-timed and non-blocking (smooth selection box motion).
- [ ] Improve `startWarningAnimation()` to overwrite/clear animation region each frame to reduce choppiness.
- [ ] Ensure animation timing and button debouncing are stable (no long blocking delays).
- [ ] Build/flash and verify smoothness.

