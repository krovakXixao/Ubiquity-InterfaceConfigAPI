#include "interface_config/configuration.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

using namespace interface_config;

void check(bool condition)
{
    if (!condition)
        throw std::runtime_error("test expectation failed");
}

template<class Operation>
void rejected(Configuration& config, Operation operation)
{
    const auto before = config.interfaces();
    bool caught = false;
    try {
        operation();
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    check(caught && before == config.interfaces());
}

int main()
{
    try {
        Configuration c;
        c.add("eth0", {Ethernet{}, {}, AdminState::up});
        c.add("eth1", {Ethernet{}, {}});
        rejected(c, [&] { c.add("eth0", {Ethernet{}, {}}); });
        rejected(c, [&] { c.add("", {Ethernet{}, {}}); });
        rejected(c, [&] { c.update("missing", {Ethernet{}, {}}); });
        rejected(c, [&] { c.remove("missing"); });
        rejected(c, [&] { c.add("v", {Vlan{"missing", 1}, {}}); });
        rejected(c, [&] { c.add("v", {Vlan{"eth0", 0}, {}}); });
        rejected(c, [&] { c.add("v", {Vlan{"eth0", 4095}, {}}); });
        c.add("v", {Vlan{"eth0", 1}, {{"192.0.2.1", 32}, {"2001:db8::1", 128}}});
        rejected(c, [&] { c.add("v2", {Vlan{"v", 2}, {}}); });
        rejected(c, [&] { c.remove("eth0"); });
        rejected(c, [&] { c.update("eth0", {Bridge{}, {}}); });
        rejected(c, [&] { c.update("v", {Vlan{"eth1", 4095}, {}}); });
        rejected(c, [&] { c.add("br", {Bridge{{"v"}, STP::on}, {}}); });
        c.add("br", {Bridge{{"eth1"}, STP::on}, {{"192.0.2.254", 24}}});
        rejected(c, [&] { c.add("v2", {Vlan{"br", 2}, {}}); });
        rejected(c, [&] { c.remove("eth1"); });
        rejected(c, [&] { c.update("eth1", {Ethernet{}, {{"192.0.2.1", 24}}}); });
        rejected(c, [&] { c.add("br2", {Bridge{{"eth1"}, STP::off}, {}}); });
        rejected(c, [&] { c.add("br2", {Bridge{{"eth0", "eth0"}, STP::off}, {}}); });
        rejected(c, [&] { c.add("br2", {Bridge{{"br2"}, STP::off}, {}}); });
        rejected(c, [&] { c.add("br2", {Bridge{{"missing"}, STP::off}, {}}); });
        for (const Address address : {Address{"bad", 24}, Address{"192.0.2.1", 33},
                                      Address{"::1", 129}, Address{"::1%eth0", 64},
                                      Address{std::string("::1\0junk", 10), 64}}) {
            rejected(c, [&] { c.add("bad", {Ethernet{}, {address}}); });
        }
        rejected(c, [&] { c.add("bad", {Ethernet{0, Duplex::full, 1500}, {}}); });
        rejected(c, [&] { c.add("bad", {Ethernet{100, Duplex::full, 0}, {}}); });
        auto detached = c.get("eth0");
        detached.addresses.push_back({"0.0.0.0", 0});
        check(c.get("eth0").addresses.empty());
        c.update("eth0", detached);
        c.update("v", {Vlan{"eth1", 4094}, {{"::", 0}}});
        c.remove("eth0"); // Parent update releases the old dependency.
        c.remove("v");
        c.update("br", {Bridge{}, {}}); // Detach member before assigning addresses.
        c.update("eth1", {Ethernet{}, {{"192.0.2.1", 24}}});
        c.remove("eth1");
        c.add("outer", {Bridge{{"br"}, STP::off}, {}});
        rejected(c, [&] { c.update("br", {Bridge{{"outer"}, STP::off}, {}}); });
        rejected(c, [&] { c.remove("br"); });
        c.remove("outer");
        c.remove("br");
        check(c.interfaces().empty());
        bool missing = false;
        try { (void)c.get("missing"); }
        catch (const std::out_of_range&) { missing = true; }
        check(missing);
        std::cout << "Configuration tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
