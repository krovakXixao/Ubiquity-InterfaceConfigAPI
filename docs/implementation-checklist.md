# Assignment checklist

This is a scaffold; the solution remains to be implemented within the 3–4 hour budget.

## Model and mutations

- [ ] Ethernet: speed, duplex, MTU.
- [ ] VLAN: Ethernet parent, VLAN ID 1–4094.
- [ ] Bridge: members, STP on/off.
- [ ] All types: IPv4/IPv6 addresses with prefixes, admin up/down.
- [ ] Add, update, remove, and read interfaces without exposing mutable state.
- [ ] Reject VLAN parents that are missing or not Ethernet.
- [ ] Reject bridge members with IP addresses.
- [ ] Reject adding addresses to an existing bridge member.
- [ ] Reject removing referenced interfaces, including bridge members.
- [ ] Enforce constraints on updates as well as insertion.
- [ ] Failed mutations leave the configuration unchanged.
- [ ] Decide identity, ownership, errors, and address/prefix validation.
- [ ] Document policies for duplicate identity, self-membership, bridge cycles,
      and membership in multiple bridges; distinguish extra policies from requirements.

## Deliverables

- [ ] Public C++20 headers and configuration container implementation.
- [ ] Realistic example: creation, updates, and dependency-ordered teardown.
- [ ] README: design decisions, alternatives, and adding Bond/LAG.
- [ ] Debug and Release builds.
- [ ] Optional focused tests for invalid mutations and unchanged state.
- [ ] Pull request on a public repository.

No daemon, netlink/ioctl, JSON, thread safety, custom allocation, or hot-path
optimization is needed. Unit tests are optional.
