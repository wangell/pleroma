pub fn build_packet() {}

pub const ETHERNET_HEADER_LENGTH: usize = 14;
pub const MIN_PAYLOAD_LENGTH: usize = 46;
pub const MIN_FRAME_LENGTH: usize = ETHERNET_HEADER_LENGTH + MIN_PAYLOAD_LENGTH;

pub fn empty_ethernet_frame(src_mac: [u8; 6], dst_mac: [u8; 6]) -> [u8; MIN_FRAME_LENGTH] {
    let mut frame = [0u8; MIN_FRAME_LENGTH];

    // Ethernet header
    frame[0..6].copy_from_slice(&dst_mac);
    frame[6..12].copy_from_slice(&src_mac);
    frame[12..14].copy_from_slice(&[0x05, 0xDC]);


    // Payload = zeros

    frame
}

pub fn create_eth_ipv4_udp_packet() -> [u8; 62] {
    let mut packet: [u8; 62] = [0; 62];

    // Ethernet header
    let src_mac = [0x00, 0x11, 0x22, 0x33, 0x44, 0x55];
    //let src_mac = [0xf7, 0xbe, 0x9e, 0x27, 0xdd, 0x5a];
    //let dst_mac = [0xff, 0xff, 0xff, 0xff, 0xff, 0xff];
    //let dst_mac = [0xff, 0xff, 0xff, 0xff, 0xff, 0xff];
    //let dst_mac = [0x52, 0x54, 0x00, 0x12, 0x34, 0x56];
    let dst_mac = [0x18, 0xc0, 0x4d, 0x32, 0xc3, 0x34];
    //let dst_mac = [0x7a, 0x46, 0x34, 0xc1, 0xfb, 0x74];
    //let dst_mac = [0xf8, 0xac, 0x65, 0x26, 0x1c, 0x3b];
    let ethertype_ipv4 = [0x08, 0x00];

    packet[..6].copy_from_slice(&dst_mac);
    packet[6..12].copy_from_slice(&src_mac);
    packet[12..14].copy_from_slice(&ethertype_ipv4);

    // IPv4 header
    let version_and_ihl = 0x45;
    let dscp_and_ecn = 0x00;
    let total_length: u16 = 0x002E;
    let identification: u16 = 0x0000;
    let flags_and_fragment_offset: u16 = 0x0000;
    let ttl = 0x40; // 64
    let protocol_udp = 0x11;
    let header_checksum: u16 = 0x0000;

    packet[14] = version_and_ihl;
    packet[15] = dscp_and_ecn;
    packet[16..18].copy_from_slice(&total_length.to_be_bytes());
    packet[18..20].copy_from_slice(&identification.to_be_bytes());
    packet[20..22].copy_from_slice(&flags_and_fragment_offset.to_be_bytes());
    packet[22] = ttl;
    packet[23] = protocol_udp;
    packet[24..26].copy_from_slice(&header_checksum.to_be_bytes());

    let src_ip = [192, 168, 100, 2];
    let dst_ip = [192, 168, 100, 3];

    packet[26..30].copy_from_slice(&src_ip);
    packet[30..34].copy_from_slice(&dst_ip);

    // UDP header
    let src_port: u16 = 5555;
    let dst_port: u16 = 5555;
    let udp_length: u16 = 0x000D;
    let udp_checksum: u16 = 0x0000;

    packet[34..36].copy_from_slice(&src_port.to_be_bytes());
    packet[36..38].copy_from_slice(&dst_port.to_be_bytes());
    packet[38..40].copy_from_slice(&udp_length.to_be_bytes());
    packet[40..42].copy_from_slice(&udp_checksum.to_be_bytes());

    let payload: &[u8] = b"Hello";
    packet[42..47].copy_from_slice(payload);

    packet
}

use core::mem;

#[repr(packed)]
struct EthernetHeader {
    dst_mac: [u8; 6],
    src_mac: [u8; 6],
    ethertype: u16,
}

#[repr(packed)]
struct ArpHeader {
    hw_type: u16,
    proto_type: u16,
    hw_addr_len: u8,
    proto_addr_len: u8,
    opcode: u16,
    sender_hw_addr: [u8; 6],
    sender_proto_addr: [u8; 4],
    target_hw_addr: [u8; 6],
    target_proto_addr: [u8; 4],
}

pub fn create_arp_broadcast_packet(src_mac: &[u8; 6], src_ip: &[u8; 4]) -> [u8; 42] {
    let eth_header = EthernetHeader {
        dst_mac: [0xff, 0xff, 0xff, 0xff, 0xff, 0xff],
        //dst_mac: [0x18, 0xc0, 0x4d, 0x32, 0xc3, 0x34],
        src_mac: *src_mac,
        ethertype: u16::to_be(0x0806), // ARP
    };

    let arp_header = ArpHeader {
        hw_type: u16::to_be(1),         // Ethernet
        proto_type: u16::to_be(0x0800), // IPv4
        hw_addr_len: 6,
        proto_addr_len: 4,
        opcode: u16::to_be(1), // ARP request
        sender_hw_addr: *src_mac,
        sender_proto_addr: *src_ip,
        target_hw_addr: [0, 0, 0, 0, 0, 0],
        target_proto_addr: [0, 0, 0, 0], // Set the target IP address here
    };

    let mut packet = [0u8; 42];
    let eth_header_ptr = &eth_header as *const EthernetHeader as *const u8;
    let arp_header_ptr = &arp_header as *const ArpHeader as *const u8;

    unsafe {
        core::ptr::copy_nonoverlapping(
            eth_header_ptr,
            packet.as_mut_ptr(),
            mem::size_of::<EthernetHeader>(),
        );
        core::ptr::copy_nonoverlapping(
            arp_header_ptr,
            packet.as_mut_ptr().add(mem::size_of::<EthernetHeader>()),
            mem::size_of::<ArpHeader>(),
        );
    }

    packet
}
