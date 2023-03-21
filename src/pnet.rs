use std::net::Ipv4Addr;

use enet::{Enet, ChannelLimit, Address};

pub fn setup_network() {
    let enet = Enet::new().unwrap();
    let addy = Address::new(Ipv4Addr::new(127, 0, 0, 1), 8080);
    //enet.create_host::<u32>(Some(&addy), 32, ChannelLimit::Maximum, enet::BandwidthLimit::Unlimited, enet::BandwidthLimit::Unlimited);
}
