use crossbeam::channel::{select, unbounded};
use std::collections::HashMap;
use std::{fs, panic, thread, time};
use crate::compile;
use crate::ast;
use crate::pbin;
use crate::parser;

use crate::{vm_core, monad, vm, node};

pub fn hotplate(
    hp_rx_vat: crossbeam::channel::Receiver<vm_core::Vat>,
    ml_tx_vat: crossbeam::channel::Sender<vm_core::Vat>,

    mut tx_msg: crossbeam::channel::Sender<vm_core::Msg>,
) {
    loop {
        // Unload vat onto hotplate
        let mut vat = hp_rx_vat.recv().unwrap();

        while vat.inbox.len() > 0 {
            let msg = vat.inbox.pop().unwrap();
            //let return_msg = vm::run_msg(&mut vat, &msg, &mut tx_msg);
            let return_msg = vm::run_msg(&fs::read("kernel.plmb").unwrap(), &mut vat, &msg, &mut tx_msg);
            if let Some(return_msg_contents) = return_msg {
                tx_msg.send(return_msg_contents).unwrap();
            }
        }

        // Back on to the sushi belt
        let rs = ml_tx_vat.send(vat).unwrap();
    }
}

fn triage_msg(
    vat_inboxes: &mut HashMap<u32, Vec<vm_core::Msg>>,
    sleeping_vats: &mut Vec<vm_core::Vat>,
    tx_vat: &crossbeam::channel::Sender<vm_core::Vat>,
    msg: vm_core::Msg,
) {
    let wake_vat_pos = sleeping_vats
        .into_iter()
        .position(|x| x.vat_id == msg.dst_address.vat_id);
    if let Some(pos) = wake_vat_pos {
        let vtr = sleeping_vats.remove(pos);
        tx_vat.send(vtr);
    }

    vat_inboxes
        .get_mut(&msg.dst_address.vat_id)
        .unwrap()
        .push(msg);
}

fn deliver_mail(
    vat_inboxes: &mut HashMap<u32, Vec<vm_core::Msg>>,
    sleeping_vats: &mut Vec<vm_core::Vat>,
    mut cv: vm_core::Vat,
    vat_tx: &crossbeam::channel::Sender<vm_core::Vat>,
) {
    let mut vi = vat_inboxes.get_mut(&cv.vat_id).unwrap();

    // If inbox is empty, put the vat to sleep, otherwise deliver the mail
    if (vi.is_empty()) {
        sleeping_vats.push(cv);
    } else {
        let mut temp_msgs = vi.clone();
        vi.clear();
        vi.append(&mut cv.outbox);
        cv.inbox.append(&mut temp_msgs);
        vat_tx.send(cv).unwrap();
    }
}

fn main_loop(
    vat_tx: &crossbeam::channel::Sender<vm_core::Vat>,
    vat_rx: &crossbeam::channel::Receiver<vm_core::Vat>,
    tx_msg: &crossbeam::channel::Sender<vm_core::Msg>,
    rx_msg: &crossbeam::channel::Receiver<vm_core::Msg>,
    sleeping_vats: &mut Vec<vm_core::Vat>,
    vat_inboxes: &mut HashMap<u32, Vec<vm_core::Msg>>,
) {

    // TODO: Make sure that we check that the vat references don't get moved when a vector grows
    //panic!();

    // Main loop: one vat queue in, one box for sleeping vats
    // Pick vats off of the in-queue, grab all outbox messages and put them in inbox, delete vat if flagged (need to decide order), put box in sleep
    // Go through inbox and check if any of the messages are for a sleeping vat - this is slow, need good way to handle this.  One way is to create an inbox per vat at the mainloop level.  Then you just iterate over which ones are not empty.  If so, take vat out of sleep box, put messages into the inbox, and put it into the hotplate queue
    // Hotplate queue just runs the vats

    // If messages is empty and vat queue is empty, block on rx_ml_box/vat_rx, look at select! macro
    // If vats_out is zero, just block on messages, otherwise block on both somehow, or just vats

    loop {
        let mut waited_vat = None;
        let mut waited_msg = None;

        select! {
            recv(vat_rx) -> waited_vat_new => {
                waited_vat = Some(waited_vat_new);
            },
            recv(rx_msg) -> waited_msg_new => {
                waited_msg = Some(waited_msg_new);
            }
        }

        // Queue messages
        if let Some(wm) = waited_msg {
            let mut wmt = wm.unwrap();
            triage_msg(vat_inboxes, sleeping_vats, vat_tx, wmt);
        }

        loop {
            if let Ok(msg) = rx_msg.try_recv() {
                triage_msg(vat_inboxes, sleeping_vats, vat_tx, msg);
            } else {
                break;
            }
        }

        // Take one vat out of queue, stuff inbox, empty outbox, send to hotplate queue
        if let Some(wv) = waited_vat {
            deliver_mail(vat_inboxes, sleeping_vats, wv.unwrap(), &vat_tx);
        }

        loop {
            if let Ok(mut cv) = vat_rx.try_recv() {
                deliver_mail(vat_inboxes, sleeping_vats, cv, &vat_tx);
            } else {
                break;
            }
        }
    }
}

pub fn boot() {
    let (tx_ml_box, rx_ml_box) = unbounded::<vm_core::Msg>();

    // Main loop vat tx <-> hotplate vat rx, hotplate vat tx <-> main loop vat rx
    let (ml_vat_tx, ml_vat_rx) = unbounded::<vm_core::Vat>();
    let (hp_vat_tx, hp_vat_rx) = unbounded::<vm_core::Vat>();

    // Max concurrency
    let n_hotplates = 1;

    for n in 0..n_hotplates {
        let (ptx, prx) = (ml_vat_tx.clone(), hp_vat_rx.clone());
        let hp_msg_tx = tx_ml_box.clone();

        thread::spawn(move || hotplate(prx, ptx, hp_msg_tx));
    }

    // Insert seminal message
    let msg = vm_core::Msg {
        src_address: vm_core::EntityAddress::new(0, 0, 0),
        dst_address: vm_core::EntityAddress::new(0, 0, 0),
        contents: vm_core::MsgContents::BigBang{ function_id: 1, args: Vec::new() }
    };
    tx_ml_box.send(msg).unwrap();

    let mut sleeping_vats: Vec<vm_core::Vat> = Vec::new();
    let mut vat_inboxes: HashMap<u32, Vec<vm_core::Msg>> = HashMap::new();

    vat_inboxes.insert(0, Vec::new());
    let mut vat = vm_core::Vat::new();
    let mut node = node::Node::new();

    compile::compile_from_files(vec![String::from("./blah/basic_entity.plm")], "kernel.plmb");

    monad::load_monad("kernel.plmb", &mut vat);

    ml_vat_tx.send(vat);

    main_loop(
        &hp_vat_tx,
        &ml_vat_rx,
        &tx_ml_box,
        &rx_ml_box,
        &mut sleeping_vats,
        &mut vat_inboxes,
    );
}
