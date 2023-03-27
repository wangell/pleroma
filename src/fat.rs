extern crate alloc;
use crate::bin_helpers::{read_le_u16, read_le_u32, read_u8, read_utf8_str};
pub use crate::common::vec;
use crate::common::Vec;
pub use alloc::str;
pub use alloc::str::FromStr;
pub use alloc::string::String;

pub struct FatFs<'a> {
    data: &'a [u8],
    fat_type: FatType,

    bios_parameter_block: BiosParameterBlock,
    extended_boot_record: ExtendedBootRecord,

    fat_table: Vec<u16>,
}

pub enum FatType {
    Fat16,
}

impl<'a> FatFs<'a> {
    pub fn new(data: &'a [u8], fat_type: FatType) -> Self {
        let bios_parameter_block = read_bios_parameter_block(data);
        let extended_boot_record = read_extended_boot_record(data);

        let start_fat_addr = bios_parameter_block.bytes_per_sector * bios_parameter_block.reserved_sector_count;
        let start_fat = &data[(start_fat_addr as usize)..];
        let fat_size = bios_parameter_block.sectors_per_fat;
        let mut fat_table: Vec<u16> = Vec::new();

        for i in (0..fat_size * bios_parameter_block.bytes_per_sector).step_by(2) {
            let start_byte = (start_fat_addr + i) as usize;
            let val = read_le_u16(&data[start_byte..]);
            fat_table.push(val);
        }

        FatFs {
            data,
            fat_type,

            bios_parameter_block,
            extended_boot_record,
            fat_table,
        }
    }

    fn dump_fat_table(&self) {
        for (index, value) in self.fat_table.iter().enumerate() {
            println!("{}: {}", index, value);
        }
    }

    // TODO: remove byte by byte coping
    pub fn read_clusters(
        &self,
        file_size: u32,
        clusters: &Vec<u16>,
    ) -> Vec<u8> {
        let mut data: Vec<u8> = vec![0; file_size as usize];
        let cluster_size_in_bytes =
            self.bios_parameter_block.sectors_per_cluster as u32 * self.bios_parameter_block.bytes_per_sector as u32;
        let mut data_i = 0;

        for i in clusters {
            let cluster_loc = (((i - 2) as u32 * self.bios_parameter_block.sectors_per_cluster as u32)
                + self.first_data_sector() as u32)
                * self.bios_parameter_block.bytes_per_sector as u32;

            for b in 0..cluster_size_in_bytes {
                let d = self.data[(cluster_loc as u32 + b as u32) as usize];
                data[data_i as usize] = d;
                data_i += 1;

                if data_i == file_size as usize {
                    return data;
                }
            }
        }

        return data;
    }

    fn fat_size(&self) -> u16 {
        self.bios_parameter_block.sectors_per_fat
    }

    fn first_data_sector(&self) -> u16 {
        self.bios_parameter_block.reserved_sector_count + (self.bios_parameter_block.fat_table_count as u16 * self.fat_size()) + self.root_dir_sectors()
    }

    fn root_dir_sectors(&self) -> u16 {
        ((self.bios_parameter_block.root_dir_entry_count * 32) + (self.bios_parameter_block.bytes_per_sector - 1)) / self.bios_parameter_block.bytes_per_sector
    }

    pub fn data_clusters(&self, cluster: u16) -> Vec<u16> {
        let mut our_clusters = vec![cluster];

        let fat_offset = cluster;
        let first_fat_sector = self.bios_parameter_block.reserved_sector_count;
        let fat_sector =
            first_fat_sector as u32 + (fat_offset as u32 / self.bios_parameter_block.sectors_per_fat as u32);
        let ent_offset = fat_offset % self.bios_parameter_block.sectors_per_fat as u16;

        let table_val: u16 = self.fat_table[ent_offset as usize];

        if table_val == 0xFFF7 {
            panic!("Bad sector");
        }

        if table_val < 0xFFF7 {
            let mut next_clusters = self.data_clusters(table_val);
            our_clusters.append(&mut next_clusters);
        }

        return our_clusters;
    }

    pub fn read_file(&self, entry: &DirectoryEntry) -> Vec<u8> {
        let clusters = self.data_clusters(entry.standard_format.cluster_id_lsb);

        self.read_clusters(entry.standard_format.file_size, &clusters)
    }

    pub fn list_root_dir(&self) -> Vec<DirectoryEntry> {

        let bpb = &self.bios_parameter_block;

        let root_dir_bytes = (bpb.root_dir_entry_count * 32);
        let root_dir_start = (self.first_data_sector() - self.root_dir_sectors()) * bpb.bytes_per_sector;

        let mut entries : Vec<DirectoryEntry> = Vec::new();

        let mut i = 0;
        loop {
            let entry_base = (root_dir_start + i * 32) as usize;

            // Entry that starts with zero = end of directory
            if self.data[entry_base] == 0 {
                break;
            }

            if self.data[(root_dir_start + i * 32) as usize] != 0xE5 {
                let start_thing = root_dir_start + i * 32;
                let end_thing = start_thing + 8;

                // 11th byte  == 0x0f == LFN file
                if self.data[((root_dir_start + i * 32) + 11) as usize] == 0x0F {
                    let entry = LFN {
                        entry_order_raw: read_u8(&self.data[entry_base + 0..entry_base + 1]),
                        raw_str1: read_lfn_name(&self.data[entry_base + 1..entry_base + 1 + 10]),

                        // Always 0x0F for LFN
                        attribute: read_u8(&self.data[entry_base + 11..entry_base + 11 + 1]),

                        entry_type: read_u8(&self.data[entry_base + 12..entry_base + 12 + 1]),
                        checksum: read_u8(&self.data[entry_base + 13..entry_base + 13 + 1]),
                        raw_str2: read_lfn_name(&self.data[entry_base + 14..entry_base + 14 + 12]),

                        // This should always be zero
                        _unused: read_le_u16(&self.data[entry_base + 26..entry_base + 26 + 2]),
                        raw_str3: read_lfn_name(&self.data[entry_base + 28..entry_base + 28 + 4]),
                    };
                } else {
                    let entry = StandardFormat {
                        filename: read_utf8_str(&self.data[entry_base..entry_base + 8]),
                        extension: read_utf8_str(&self.data[entry_base + 8..entry_base + 11]),
                        raw_attributes: 0,

                        // Skip these fields, they are not used
                        // reserved: u8,
                        creation_time: 0,
                        creation_time_hms: 0,
                        creation_time_ymd: 0,
                        last_access_date: 0,

                        // Not used on FAT16
                        cluster_id_msb: 0,

                        last_modified_time: 0,
                        last_modified_date: 0,

                        cluster_id_lsb: read_le_u16(&self.data[entry_base + 26..entry_base + 26 + 2]),

                        file_size: read_le_u32(&self.data[entry_base + 28..entry_base + 28 + 4]),
                    };

                    entries.push(DirectoryEntry {
                        standard_format: entry,
                        lfn: Vec::new()
                    });
                }
            }

            i += 1;
        }

        entries
    }
}

#[derive(Debug)]
pub struct BiosParameterBlock {
    bytes_per_sector: u16,
    sectors_per_cluster: u8,
    reserved_sector_count: u16,
    fat_table_count: u8,
    root_dir_entry_count: u16,
    total_sectors_in_lv: u16,
    media_descriptor_type: u8,

    // FAT 12/16 only
    sectors_per_fat: u16,

    sectors_per_track: u16,
    head_count: u16,
    hidden_sector_count: u32,
    large_sector_count: u32,
}

// Only for FAT16 for now
#[derive(Debug)]
pub struct ExtendedBootRecord {
    drive_number: u8,
    flags: u8,
    signature: u8,
    serial_number: u32,
    volume_label: String,
    system_id: String,
    //boot_code: [u8; 448],
    boot_partition_signature: u16,
}

#[derive(Debug)]
pub struct DirectoryEntry {
    standard_format: StandardFormat,
    lfn: Vec<LFN>,
}

impl DirectoryEntry {
    pub fn filename(&self) -> String {
        let mut s = self.standard_format.filename.clone();
        s.push_str(".");
        s.push_str(&self.standard_format.extension);

        s
    }
}

#[derive(Debug)]
pub struct StandardFormat {
    filename: String,
    extension: String,
    raw_attributes: u8,

    // Skip these fields, they are not used
    // reserved: u8,
    creation_time: u8,
    creation_time_hms: u16,
    creation_time_ymd: u16,
    last_access_date: u16,

    // Not used on FAT16
    cluster_id_msb: u16,

    last_modified_time: u16,
    last_modified_date: u16,

    cluster_id_lsb: u16,

    file_size: u32,
}

#[derive(Debug)]
pub struct LFN {
    entry_order_raw: u8,
    raw_str1: String,

    // Always set to 0x0F
    attribute: u8,
    entry_type: u8,
    checksum: u8,
    raw_str2: String,
    _unused: u16,
    raw_str3: String,
}

pub fn read_lfn_name(vblock: &[u8]) -> String {
    let mut real_block: [u8; 32] = [0; 32];

    let mut n_len = 0;
    for i in (0..vblock.len()).step_by(2) {
        if !vblock[i].is_ascii() || vblock[i] == 0 {
            break;
        }
        real_block[i / 2] = vblock[i];
        n_len += 1;
    }

    let s = match str::from_utf8(&real_block[..n_len]) {
        Ok(v) => v,
        Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
    };

    return String::from(s);
}

pub fn read_extended_boot_record(fs: &[u8]) -> ExtendedBootRecord {
    let drive_number = read_u8(&fs[36..]);
    let flags = read_u8(&fs[37..]);
    let signature = read_u8(&fs[38..]);
    let serial_number = read_le_u32(&fs[39..]);

    let volume_label = read_utf8_str(&fs[43..54]);
    let system_id = read_utf8_str(&fs[54..62]);

    let boot_partition_signature = read_le_u16(&fs[510..]);

    ExtendedBootRecord {
        drive_number,
        flags,
        signature,
        serial_number,
        volume_label,
        system_id,
        boot_partition_signature,
    }
}

pub fn read_bios_parameter_block(fs: &[u8]) -> BiosParameterBlock {
    // Ignore first 11 bytes

    BiosParameterBlock {
        bytes_per_sector: read_le_u16(&fs[11..]),
        sectors_per_cluster: read_u8(&fs[13..]),
        reserved_sector_count: read_le_u16(&fs[14..]),
        fat_table_count: read_u8(&fs[16..]),
        root_dir_entry_count: read_le_u16(&fs[17..]),
        total_sectors_in_lv: read_le_u16(&fs[19..]),
        media_descriptor_type: read_u8(&fs[21..]),

        sectors_per_fat: read_le_u16(&fs[22..]),

        sectors_per_track: read_le_u16(&fs[24..]),
        head_count: read_le_u16(&fs[26..]),
        hidden_sector_count: read_le_u32(&fs[28..]),
        large_sector_count: read_le_u32(&fs[32..]),
    }
}
