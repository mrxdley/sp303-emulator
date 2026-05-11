# Docs Index

Use this file as the human entry point for the repo.

## Start Here

- [README.md](../README.md)
  - high-level project summary
  - build/run instructions
  - repo layout

- [ONBOARDING.md](../ONBOARDING.md)
  - best starting point for new contributors or AI agents
  - explains the architecture split and workflow expectations

## Behavior / Testing

- [MANUAL_TEST_SUITE.md](../MANUAL_TEST_SUITE.md)
  - main manual-cross-check document
  - step-by-step workflows to verify against the SP-303 manual

- [VELOCITY_MANUAL_TESTS.md](../VELOCITY_MANUAL_TESTS.md)
  - focused test plan for velocity plumbing and pattern-event persistence

## Architecture / Reference

- [PROJECT_DOCUMENTATION.md](../PROJECT_DOCUMENTATION.md)
  - older architecture/reference writeup
  - useful as background, but some sections may lag behind the latest refactors

- [PATTERN_SEQUENCER_IMPLEMENTATION_PLAN.md](../PATTERN_SEQUENCER_IMPLEMENTATION_PLAN.md)
  - retained planning notes for pattern work

- [PATTERN_SEQUENCER_RESEARCH.md](../PATTERN_SEQUENCER_RESEARCH.md)
  - supporting notes and observations

- [HOW-TO-SAVE-LOAD-IMPLEMENT.md](../HOW-TO-SAVE-LOAD-IMPLEMENT.md)
  - save/load design notes

## Active Work / Gaps

- [TODO.md](../TODO.md)
- [AUDIO_LOOPBACK_HOWTO.md](AUDIO_LOOPBACK_HOWTO.md)
- [KNOWN_INACCURACIES.md](KNOWN_INACCURACIES.md)
  - current open tasks
  - convenience features
  - known implementation caveats

## Practical Guidance

If you are changing behavior:
1. Read [ONBOARDING.md](../ONBOARDING.md).
2. Find the workflow in [MANUAL_TEST_SUITE.md](../MANUAL_TEST_SUITE.md).
3. Confirm whether the behavior is meant to be authentic, approximate, or convenience-only.
4. Only then patch code.
