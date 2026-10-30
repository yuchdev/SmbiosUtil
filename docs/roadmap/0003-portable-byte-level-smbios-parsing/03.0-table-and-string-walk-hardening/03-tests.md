# 03 - Tests

**Parent story:** [README.md](README.md)
**Status:** ⬜ Not started

## Objective

Run Tasks 01-02's new fixtures together under the sanitizer CI job
(milestone 0002), add the synthetic big-endian-encoded-table fixture R9
calls for, and close out milestone 0003.

## Files

- `tests/unit/byte_reader_endianness_test.cpp` (new, satisfies R9): a byte
  buffer whose values were chosen so that a naive host-endian
  `reinterpret_cast` read would differ from the correct little-endian
  decode (e.g. `{0x01, 0x00}` as a `uint16_t` must decode to `1`, not
  `256`, regardless of host - this is actually indistinguishable from a
  correct big-endian host reading it raw, so the test's real assertion is
  that `read_u16` matches the DMTF-mandated little-endian interpretation
  for a value chosen to make byte-order mistakes visible, e.g.
  `{0x34, 0x12}` → must equal `0x1234`, never `0x3412`).

## Success criteria

- [ ] All of Story 01.0/02.0/03.0's synthetic-buffer tests pass together.
- [ ] Sanitizer CI job is green with every new fixture included.
- [ ] Full existing functional suite passes unmodified on real hardware.
- [ ] Milestone 0003's `status.md` is updated to ✅ for all three stories,
      with a closeout note confirming R1-R9 are satisfied.
