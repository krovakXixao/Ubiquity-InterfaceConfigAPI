#include "interface_config/configuration.hpp"

#include <arpa/inet.h>
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
} // namespace

void Configuration::validate(const Interfaces& interfaces)
{
    std::set<std::string> bridge_members;
    for (const auto& [name, interface] : interfaces) {
        require(!name.empty() && name.find('\0') == std::string::npos,
                "Interface name must be nonempty and contain no NUL");
        require(interface.admin_state == AdminState::up || interface.admin_state == AdminState::down,
                name + ": invalid admin state");
        for (const auto& address : interface.addresses) {
            in6_addr storage{};
            const bool no_nul = address.value.find('\0') == std::string::npos;
            const bool ipv4 = inet_pton(AF_INET, address.value.c_str(), &storage) == 1;
            const bool ipv6 = inet_pton(AF_INET6, address.value.c_str(), &storage) == 1;
            require(no_nul && ((ipv4 && address.prefix_length <= 32) ||
                              (ipv6 && address.prefix_length <= 128)),
                    name + ": invalid IP address or prefix");
        }
        if (const auto* ethernet = std::get_if<Ethernet>(&interface.properties)) {
            require(ethernet->speed_mbps > 0 && ethernet->mtu >= 68 && ethernet->mtu <= 65535,
                    name + ": invalid Ethernet speed or MTU");
            require(ethernet->duplex == Duplex::half || ethernet->duplex == Duplex::full,
                    name + ": invalid duplex");
        } else if (const auto* vlan = std::get_if<Vlan>(&interface.properties)) {
            require(vlan->id >= 1 && vlan->id <= 4094, name + ": VLAN ID must be 1..4094");
            const auto parent = interfaces.find(vlan->parent);
            require(parent != interfaces.end() &&
                        std::holds_alternative<Ethernet>(parent->second.properties),
                    name + ": VLAN parent must be an existing Ethernet interface");
        } else if (const auto* bridge = std::get_if<Bridge>(&interface.properties)) {
            require(bridge->stp == STP::off || bridge->stp == STP::on,
                    name + ": invalid STP state");
            for (const auto& member : bridge->members) {
                const auto found = interfaces.find(member);
                require(member != name && found != interfaces.end(),
                        name + ": bridge member must exist and cannot be itself");
                require(found->second.addresses.empty(), member + ": bridge member has IP addresses");
                require(bridge_members.insert(member).second,
                        member + ": duplicate member or membership in multiple bridges");
            }
        }
    }
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
