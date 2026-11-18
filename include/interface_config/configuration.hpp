#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace interface_config {
enum class AdminState { down, up };
enum class Duplex { half, full };
enum class STP { off, on };

struct Address {
    std::string value;
    unsigned prefix_length;
    bool operator==(const Address&) const = default;
};
struct Ethernet {
    unsigned speed_mbps = 1000;
    Duplex duplex = Duplex::full;
    unsigned mtu = 1500;
    bool operator==(const Ethernet&) const = default;
};
struct Vlan {
    std::string parent;
    unsigned id;
    bool operator==(const Vlan&) const = default;
};
struct Bridge {
    std::vector<std::string> members;
    STP stp = STP::off;
    bool operator==(const Bridge&) const = default;
};
using Properties = std::variant<Ethernet, Vlan, Bridge>;

// Detached values are editable; only Configuration owns committed state.
struct Interface {
    Properties properties;
    std::vector<Address> addresses;
    AdminState admin_state = AdminState::down;
    bool operator==(const Interface&) const = default;
};

class Configuration {
public:
    using Interfaces = std::map<std::string, Interface>;
    // Names are stable identities. update replaces the entire value, including type.
    // Invalid mutations throw std::invalid_argument and leave state unchanged.
    void add(std::string name, Interface interface);
    void update(const std::string& name, Interface interface);
    void remove(const std::string& name);
    // Detached snapshot. Missing names throw std::out_of_range.
    [[nodiscard]] Interface get(const std::string& name) const;
    // View is invalidated by successful mutations; copy it for a persistent snapshot.
    [[nodiscard]] const Interfaces& interfaces() const noexcept { return interfaces_; }
private:
    static void validate(const Interfaces& interfaces);
    Interfaces interfaces_;
};
} // namespace interface_config
