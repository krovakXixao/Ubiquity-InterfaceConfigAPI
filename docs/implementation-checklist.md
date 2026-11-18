# Assignment checklist

Local implementation and Debug/Release verification are complete. Public PR delivery remains pending.

## Model and mutations

- [x] Ethernet: speed, duplex, MTU.
- [x] VLAN: Ethernet parent, VLAN ID 1–4094.
- [x] Bridge: members, STP on/off.
- [x] All types: IPv4/IPv6 addresses with prefixes, admin up/down.
- [x] Add, update, remove, and read interfaces without exposing mutable state.
- [x] Reject VLAN parents that are missing or not Ethernet.
- [x] Reject bridge members with IP addresses.
- [x] Reject adding addresses to an existing bridge member.
- [x] Reject removing referenced interfaces, including bridge members.
- [x] Enforce constraints on updates as well as insertion.
- [x] Failed mutations leave the configuration unchanged.
- [x] Decide identity, ownership, errors, and address/prefix validation.
- [x] Document policies for duplicate identity, self-membership, bridge cycles,
      and membership in multiple bridges; distinguish extra policies from requirements.

## Deliverables

- [x] Public C++20 headers and configuration container implementation.
- [x] Realistic example: creation, updates, and dependency-ordered teardown.
- [x] README: design decisions, alternatives, and adding Bond/LAG.
- [x] Debug and Release builds.
- [x] Optional focused tests for invalid mutations and unchanged state.
- [ ] Pull request on a public repository.

No daemon, netlink/ioctl, JSON, thread safety, custom allocation, or hot-path
optimization is needed. Unit tests are optional.
