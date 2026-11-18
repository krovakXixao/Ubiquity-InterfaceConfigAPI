#include "interface_config/configuration.hpp"

#include <arpa/inet.h>
#include <array>
#include <functional>
#include <set>
#include <stdexcept>
#include <utility>

namespace interface_config {
namespace {
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::invalid_argument(message);
}
std::vector<std::string> references(const Interface& interface)
{
    if (const auto* vlan = std::get_if<Vlan>(&interface.properties))
        return {vlan->parent};
    if (const auto* bridge = std::get_if<Bridge>(&interface.properties))
        return bridge->members;
    return {};
}
using Interfaces = Configuration::Interfaces;
using AddressKey = std::pair<int, std::array<unsigned char, 16>>;
using AddressOwners = std::map<AddressKey, std::string>;
using VlanKeys = std::set<std::pair<std::string, unsigned>>;

void validate_name(const std::string& name)
{
    require(!name.empty(), "Interface name must be nonempty");
    require(name.find('\0') == std::string::npos, "Interface name must contain no NUL");
}

void validate_admin_state(const std::string& name, AdminState state)
{
    require(state == AdminState::up || state == AdminState::down,
            name + ": invalid admin state");
}

AddressKey validate_address(const std::string& name, const Address& address)
{
    require(address.value.find('\0') == std::string::npos,
            name + ": IP address must contain no NUL");
    std::array<unsigned char, 16> storage{};
    const bool ipv4 = inet_pton(AF_INET, address.value.c_str(), storage.data()) == 1;
    const bool ipv6 = !ipv4 && inet_pton(AF_INET6, address.value.c_str(), storage.data()) == 1;
    require(ipv4 || ipv6, name + ": invalid IP address: " + address.value);
    require(address.prefix_length <= (ipv4 ? 32u : 128u),
            name + ": invalid IP prefix length: " + std::to_string(address.prefix_length));
    return {ipv4 ? AF_INET : AF_INET6, storage};
}

void validate_addresses(const std::string& name, const std::vector<Address>& addresses,
                        AddressOwners& owners)
{
    for (const auto& address : addresses) {
        const auto key = validate_address(name, address);
        const auto [owner, inserted] = owners.emplace(key, name);
        require(inserted,
                name + ": IP address " + address.value + " already assigned to " + owner->second);
    }
}

void validate_ethernet(const std::string& name, const Ethernet& ethernet)
{
    require(ethernet.speed_mbps > 0, name + ": Ethernet speed must be positive");
    require(ethernet.mtu >= 68 && ethernet.mtu <= 65535,
            name + ": Ethernet MTU must be 68..65535");
    require(ethernet.duplex == Duplex::half || ethernet.duplex == Duplex::full,
            name + ": invalid duplex");
}

void validate_vlan(const std::string& name, const Vlan& vlan,
                   const Interfaces& interfaces, VlanKeys& keys)
{
    require(vlan.id >= 1 && vlan.id <= 4094, name + ": VLAN ID must be 1..4094");
    const auto parent = interfaces.find(vlan.parent);
    require(parent != interfaces.end(), name + ": VLAN parent does not exist: " + vlan.parent);
    require(std::holds_alternative<Ethernet>(parent->second.properties),
            name + ": VLAN parent must be Ethernet: " + vlan.parent);
    require(keys.emplace(vlan.parent, vlan.id).second,
            name + ": duplicate VLAN parent/ID combination: " +
                vlan.parent + "/" + std::to_string(vlan.id));
}

void validate_bridge(const std::string& name, const Bridge& bridge,
                     const Interfaces& interfaces, std::set<std::string>& members)
{
    require(bridge.stp == STP::off || bridge.stp == STP::on, name + ": invalid STP state");
    for (const auto& member : bridge.members) {
        require(member != name, name + ": bridge cannot contain itself");
        const auto found = interfaces.find(member);
        require(found != interfaces.end(), name + ": bridge member does not exist: " + member);
        require(found->second.addresses.empty(), member + ": bridge member has IP addresses");
        require(members.insert(member).second,
                member + ": duplicate member or membership in multiple bridges");
    }
}

void validate_dependency_cycles(const Interfaces& interfaces)
{
    // Nested bridges are allowed, but the reference graph must remain acyclic.
    std::set<std::string> visiting, visited;
    const std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (visited.contains(name))
            return;
        require(visiting.insert(name).second, name + ": dependency cycle");
        for (const auto& reference : references(interfaces.at(name)))
            visit(reference);
        visiting.erase(name);
        visited.insert(name);
    };
    for (const auto& entry : interfaces)
        visit(entry.first);
}
} // namespace

void Configuration::validate(const Interfaces& interfaces)
{
    AddressOwners address_owners;
    VlanKeys vlan_keys;
    std::set<std::string> bridge_members;
    for (const auto& [name, interface] : interfaces) {
        validate_name(name);
        validate_admin_state(name, interface.admin_state);
        validate_addresses(name, interface.addresses, address_owners);
        if (const auto* ethernet = std::get_if<Ethernet>(&interface.properties))
            validate_ethernet(name, *ethernet);
        else if (const auto* vlan = std::get_if<Vlan>(&interface.properties))
            validate_vlan(name, *vlan, interfaces, vlan_keys);
        else if (const auto* bridge = std::get_if<Bridge>(&interface.properties))
            validate_bridge(name, *bridge, interfaces, bridge_members);
    }
    validate_dependency_cycles(interfaces);
}

void Configuration::add(std::string name, Interface interface)
{
    require(!interfaces_.contains(name), name + ": interface already exists");
    auto candidate = interfaces_;
    candidate.emplace(std::move(name), std::move(interface));
    validate(candidate);
    interfaces_.swap(candidate);
}
void Configuration::update(const std::string& name, Interface interface)
{
    require(interfaces_.contains(name), name + ": interface does not exist");
    auto candidate = interfaces_;
    candidate.at(name) = std::move(interface);
    validate(candidate);
    interfaces_.swap(candidate);
}
void Configuration::remove(const std::string& name)
{
    require(interfaces_.contains(name), name + ": interface does not exist");
    for (const auto& [other, interface] : interfaces_)
        for (const auto& reference : references(interface))
            require(reference != name, name + ": still referenced by " + other);
    interfaces_.erase(name);
}
Interface Configuration::get(const std::string& name) const
{
    return interfaces_.at(name);
}
} // namespace interface_config
