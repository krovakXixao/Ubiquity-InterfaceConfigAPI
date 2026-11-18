#include "interface_config/configuration.hpp"

#include <iostream>
#include <stdexcept>
#include <source_location>
#include <string>

using namespace interface_config;

namespace {
void check(bool condition,
           const std::source_location location = std::source_location::current())
{
    if (!condition)
        throw std::runtime_error(std::string(location.file_name()) + ":" +
                                 std::to_string(location.line()) + ": test expectation failed");
}

template<class Operation>
void rejected(Configuration& config, Operation operation,
              const std::source_location location = std::source_location::current())
{
    const auto before = config.interfaces();
    bool caught = false;
    try {
        operation();
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    check(caught, location);
    check(before == config.interfaces(), location);
}

void test_address_uniqueness_across_interfaces()
{
    Configuration addrs_conf;
    addrs_conf.add("a", {Ethernet{}, {{"192.0.2.1", 24}, {"2001:db8::1", 64}}});
    rejected(addrs_conf, [&] {
        addrs_conf.add("b", {Ethernet{}, {{"192.0.2.1", 32}}});
    });
    rejected(addrs_conf, [&] {
        addrs_conf.add("b", {Ethernet{}, {{"2001:0DB8:0:0:0:0:0:1", 128}}});
    });
    addrs_conf.add("b", {Ethernet{}, {{"192.0.2.2", 24}, {"2001:db8::2", 64}}});
    rejected(addrs_conf, [&] {
        addrs_conf.update("b", {Ethernet{}, {{"192.0.2.1", 16}}});
    });
    rejected(addrs_conf, [&] {
        addrs_conf.update("b", {Ethernet{}, {{"2001:db8:0::1", 48}}});
    });
}

void test_repeated_addresses_within_interface()
{
    Configuration addrs_conf;
    addrs_conf.add("a", {Ethernet{}, {{"192.0.2.1", 24}, {"2001:db8::1", 64}}});
    // Revalidating unchanged addresses is permitted; repeated addresses are not.
    auto own = addrs_conf.get("a");
    addrs_conf.update("a", own);
    own.addresses.push_back({"2001:0db8::1", 128});
    rejected(addrs_conf, [&] { addrs_conf.update("a", own); });
    auto repeated_ipv4 = addrs_conf.get("a");
    repeated_ipv4.addresses.push_back({"192.0.2.1", 32});
    rejected(addrs_conf, [&] { addrs_conf.update("a", repeated_ipv4); });
    rejected(addrs_conf, [&] {
        addrs_conf.add("duplicate-v4", {Ethernet{}, {{"198.51.100.1", 24},
                                                    {"198.51.100.1", 24}}});
    });
    rejected(addrs_conf, [&] {
        addrs_conf.add("duplicate-v6", {Ethernet{}, {{"2001:db8::3", 64},
                                                    {"2001:0db8:0::3", 128}}});
    });
}

void test_address_reuse_and_families()
{
    Configuration addrs_conf;
    addrs_conf.add("a", {Ethernet{}, {{"192.0.2.1", 24}, {"2001:db8::1", 64}}});
    addrs_conf.add("b", {Ethernet{}, {}});
    addrs_conf.update("a", {Ethernet{}, {}});
    addrs_conf.update("b", {Ethernet{}, {{"192.0.2.1", 24}}});
    addrs_conf.remove("b");
    addrs_conf.add("c", {Ethernet{}, {{"192.0.2.1", 32}}});
    // IPv4 and IPv4-mapped IPv6 remain distinct address families.
    addrs_conf.add("d", {Ethernet{}, {{"::ffff:192.0.2.1", 128}}});
}

void test_vlan_uniqueness_and_reuse()
{
    Configuration vlans;
    vlans.add("eth0", {Ethernet{}, {}});
    vlans.add("eth1", {Ethernet{}, {}});
    vlans.add("v0", {Vlan{"eth0", 10}, {}});
    rejected(vlans, [&] { vlans.add("duplicate", {Vlan{"eth0", 10}, {}}); });
    vlans.add("v1", {Vlan{"eth1", 10}, {}}); // Same ID, different parent.
    vlans.add("v2", {Vlan{"eth0", 20}, {}}); // Same parent, different ID.
    rejected(vlans, [&] { vlans.update("v1", {Vlan{"eth0", 10}, {}}); });
    rejected(vlans, [&] { vlans.update("v2", {Vlan{"eth0", 10}, {}}); });
    vlans.update("v0", vlans.get("v0")); // Its own key remains valid.
    vlans.update("v0", {Vlan{"eth0", 30}, {}});
    vlans.add("reused", {Vlan{"eth0", 10}, {}}); // Update released the old key.
    vlans.remove("reused");
    vlans.add("reused-again", {Vlan{"eth0", 10}, {}});
}

void test_interface_identity()
{
    Configuration c;
    c.add("eth0", {Ethernet{}, {}, AdminState::up});
    rejected(c, [&] { c.add("eth0", {Ethernet{}, {}}); });
    rejected(c, [&] { c.add("", {Ethernet{}, {}}); });
    rejected(c, [&] { c.update("missing", {Ethernet{}, {}}); });
    rejected(c, [&] { c.remove("missing"); });
    bool missing = false;
    try { (void)c.get("missing"); }
    catch (const std::out_of_range&) { missing = true; }
    check(missing);
}

void test_vlan_constraints_and_dependencies()
{
    Configuration c;
    c.add("eth0", {Ethernet{}, {}});
    c.add("eth1", {Ethernet{}, {}});
    c.add("br", {Bridge{}, {}});
    rejected(c, [&] { c.add("v", {Vlan{"missing", 1}, {}}); });
    rejected(c, [&] { c.add("v", {Vlan{"", 1}, {}}); });
    rejected(c, [&] { c.add("v", {Vlan{"eth0", 0}, {}}); });
    rejected(c, [&] { c.add("v", {Vlan{"eth0", 4095}, {}}); });
    c.add("v", {Vlan{"eth0", 1}, {}});
    rejected(c, [&] { c.add("v2", {Vlan{"v", 2}, {}}); });
    rejected(c, [&] { c.add("v2", {Vlan{"br", 2}, {}}); });
    rejected(c, [&] { c.remove("eth0"); });
    rejected(c, [&] { c.update("eth0", {Bridge{}, {}}); });
    rejected(c, [&] { c.update("v", {Vlan{"eth1", 4095}, {}}); });
    c.update("v", {Vlan{"eth1", 4094}, {}});
    c.remove("eth0");
    c.remove("v");
    c.remove("eth1");
    c.remove("br");
    check(c.interfaces().empty());
}

void test_bridge_membership_and_addresses()
{
    Configuration c;
    c.add("eth0", {Ethernet{}, {}});
    c.add("eth1", {Ethernet{}, {}});
    c.add("v", {Vlan{"eth0", 1}, {{"192.0.2.1", 32}}});
    rejected(c, [&] { c.add("br", {Bridge{{"v"}, STP::on}, {}}); });
    c.add("br", {Bridge{{"eth1"}, STP::on}, {{"192.0.2.254", 24}}});
    rejected(c, [&] { c.remove("eth1"); });
    rejected(c, [&] { c.update("eth1", {Ethernet{}, {{"192.0.2.2", 24}}}); });
    rejected(c, [&] { c.add("br2", {Bridge{{"eth1"}, STP::off}, {}}); });
    rejected(c, [&] { c.add("br2", {Bridge{{"eth0", "eth0"}, STP::off}, {}}); });
    rejected(c, [&] { c.add("br2", {Bridge{{"br2"}, STP::off}, {}}); });
    rejected(c, [&] { c.add("br2", {Bridge{{"missing"}, STP::off}, {}}); });
    c.update("br", {Bridge{}, {}});
    c.update("eth1", {Ethernet{}, {{"192.0.2.2", 24}}});
    c.remove("eth1");
}

void test_bridge_cycles_and_teardown()
{
    Configuration c;
    c.add("br", {Bridge{}, {}});
    c.add("outer", {Bridge{{"br"}, STP::off}, {}});
    rejected(c, [&] { c.update("br", {Bridge{{"outer"}, STP::off}, {}}); });
    rejected(c, [&] { c.remove("br"); });
    c.remove("outer");
    c.remove("br");
    check(c.interfaces().empty());
}

void test_address_validation()
{
    Configuration c;
    for (const Address address : {Address{"bad", 24}, Address{"192.0.2.1", 33},
                                  Address{"::1", 129}, Address{"::1%eth0", 64},
                                  Address{std::string("::1\0junk", 10), 64}}) {
        rejected(c, [&] { c.add("bad", {Ethernet{}, {address}}); });
    }
    c.add("valid", {Ethernet{}, {{"192.0.2.1", 32}, {"2001:db8::1", 128},
                                {"0.0.0.0", 0}, {"::", 0}}});
}

void test_ethernet_validation()
{
    Configuration c;
    rejected(c, [&] { c.add("bad", {Ethernet{0, Duplex::full, 1500}, {}}); });
    rejected(c, [&] { c.add("bad", {Ethernet{100, Duplex::full, 0}, {}}); });
}

void test_detached_snapshots()
{
    Configuration c;
    c.add("eth0", {Ethernet{}, {}});
    auto detached = c.get("eth0");
    detached.addresses.push_back({"0.0.0.0", 0});
    check(c.get("eth0").addresses.empty());
    c.update("eth0", detached);
    check(c.get("eth0") == detached);
}
} // namespace

int main()
{
    struct TestCase {
        const char* name;
        void (*run)();
    };
    const TestCase tests[] = {
        {"address uniqueness across interfaces", test_address_uniqueness_across_interfaces},
        {"repeated addresses within interface", test_repeated_addresses_within_interface},
        {"address reuse and families", test_address_reuse_and_families},
        {"vlan uniqueness and reuse", test_vlan_uniqueness_and_reuse},
        {"interface identity", test_interface_identity},
        {"vlan constraints and dependencies", test_vlan_constraints_and_dependencies},
        {"bridge membership and addresses", test_bridge_membership_and_addresses},
        {"bridge cycles and teardown", test_bridge_cycles_and_teardown},
        {"address validation", test_address_validation},
        {"ethernet validation", test_ethernet_validation},
        {"detached snapshots", test_detached_snapshots},
    };
    unsigned failures = 0;
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "PASS: " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}
