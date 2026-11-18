#include "interface_config/configuration.hpp"

#include <iostream>
#include <stdexcept>

using namespace interface_config;

int main()
{
    Configuration gateway;
    // Ports must have no IP addresses before joining a bridge.
    gateway.add("eth1", {Ethernet{1000, Duplex::full, 1500}, {}, AdminState::up});
    gateway.add("eth2", {Ethernet{1000, Duplex::half, 1200}, {}, AdminState::up});
    gateway.add("br0", {Bridge{{"eth1", "eth2"}, STP::on},
                        {{"192.168.10.1", 24}, {"fd00:10::1", 64}}, AdminState::up});
    gateway.add("eth3", {Ethernet{1000, Duplex::full, 1500}, {}, AdminState::up});
    gateway.add("eth3.10", {Vlan{"eth3", 10},
                            {{"192.168.20.1", 24}, {"fd00:20::1", 64}}, AdminState::up});
    auto uplink = gateway.get("eth3");
    std::get<Ethernet>(uplink.properties).mtu = 9000;
    gateway.update("eth3", uplink);
    try {
        gateway.remove("eth3");
    } catch (const std::invalid_argument& error) {
        std::cout << "Expected rejection: " << error.what() << '\n';
    }
    std::cout << "Configured " << gateway.interfaces().size() << " interfaces\n";
    gateway.remove("eth3.10");
    gateway.remove("eth3");
    gateway.remove("br0");
    gateway.remove("eth2");
    gateway.remove("eth1");
    std::cout << "Teardown complete: " << gateway.interfaces().size() << " interfaces\n";
}
