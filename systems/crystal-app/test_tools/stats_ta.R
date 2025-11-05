#!/usr/bin/env Rscript
options("width"=160)
sink(stdout(), type = "message")
library(plyr)
library(ggplot2)

CRYSTAL_TYPE_DATA = 0x02

CRYSTAL_BAD_DATA = 1
CRYSTAL_BAD_CRC = 2
CRYSTAL_HIGH_NOISE = 3
CRYSTAL_SILENCE = 4
CRYSTAL_TX = 5

ENERGY_LOGS = 0
STATETIME_LOGS = 0


ssum = read.table("send_summary.log", header=T, colClasses=c("numeric"))
rsum = read.table("recv_summary.log", header=T, colClasses=c("numeric"))
ta_log = read.table("ta.log", header=T, colClasses=c("numeric", "numeric", "numeric", "numeric", "numeric", "numeric", "numeric", "numeric", "numeric", "numeric", "numeric", "character", "numeric"))
asend = read.table("app_send.log", header=T, colClasses=c("numeric"))

if (file.exists("energy.log") && file.exists("energy_tf.log")) {
  ENERGY_LOGS = 1
  message("ENERGY LOGS = ",ENERGY_LOGS)
  energy = read.table("energy.log", header=T, colClasses=c("numeric"))
  energy_tf = read.table("energy_tf.log", header=T, colClasses=c("numeric"))
  energy = merge(energy, energy_tf, by=c("epoch", "node"))
}
if (file.exists("energy.log")) {
  energy = read.table("energy.log", header=T, colClasses=c("numeric", "numeric", "character", "numeric", "numeric", "numeric", "numeric", "numeric", "numeric"))
  if (dim(energy)[1] > 0) {
    STATETIME_LOGS = 1
  }
}

params = read.table("params_tbl.txt", header=T)
SINK = params$sink[1]
conc_senders = params$senders
message("Sink node: ", SINK, " concurrent senders: ", conc_senders)

payload_length = 0
if ("payload" %in% names(params)) {
    payload_length = params$payload
}
message("Payload length: ", payload_length)

message("Starting epoch per node")
start_epoch = setNames(aggregate(epoch ~ dst, rsum, FUN=min), c("node", "epoch"))
start_epoch

message("Distribution of starting epoch")
setNames(aggregate(node ~ epoch, start_epoch, FUN=length), c("epoch", "n_nodes"))

message("Max starting epoch: ", max(start_epoch$epoch))

#SKIP_END_EPOCHS = 30
SKIP_END_EPOCHS = 10
SKIP_START_EPOCHS = params$full_epochs[1]

message("SKIPPING last ", SKIP_END_EPOCHS, " epochs!")

min_epoch = max(c(SKIP_START_EPOCHS, params$start_epoch, min(rsum$epoch))+1)
max_epoch = max(rsum$epoch)-SKIP_END_EPOCHS
#max_epoch = min(params$start_epoch+params$active_epochs, max(rsum$epoch)-1)


n_epochs = max_epoch - min_epoch + 1
message(n_epochs, " epochs: [", min_epoch, ", ", max_epoch, "]")


e = ta_log$epoch
ta_log = ta_log[e>=min_epoch & e<=max_epoch,]
e = asend$epoch
asend = asend[e>=min_epoch & e<=max_epoch,]
e = ssum$epoch
ssum = ssum[e>=min_epoch & e<=max_epoch,]
e = rsum$epoch
rsum = rsum[e>=min_epoch & e<=max_epoch,]
if (ENERGY_LOGS) {
  e = energy$epoch
  energy = energy[e>=min_epoch & e<=max_epoch,]
}
if (STATETIME_LOGS) {
  e = energy$epoch
  energy = energy[e>=min_epoch & e<=max_epoch,]
}
rm(e)

alive = unique(read.table("node.log", header=T, colClasses=c("numeric", "numeric"))$n_id)
message("Alive nodes: ", length(alive))
print(sort(alive))

heard_from = unique(rsum$dst)
message("Participating nodes: ", length(heard_from))
print(sort(heard_from))

send = ta_log[ta_log$status==CRYSTAL_TX,]
recv = ta_log[ta_log$status!=CRYSTAL_TX,]

sent_anything = unique(send$src)
message("Nodes that logged sending something: ", length(sent_anything))
print(sort(sent_anything))

not_packets = (recv$type != CRYSTAL_TYPE_DATA) | (recv$status != 0)
wrong_packets = not_packets & (recv$status %in% c(CRYSTAL_BAD_DATA))

recv_from = unique(recv[!not_packets, c("src")])
message("Messages received from the following ", length(recv_from), " nodes")
print(sort(recv_from))

message("Alive nodes that didn't work")
lazy_nodes = setdiff(alive, c(heard_from, recv_from, sent_anything, SINK))
lazy_nodes

message("Packets of wrong types or length received:")
recv[wrong_packets,]

message("Num packets with bad CRC: ", sum(recv$status == CRYSTAL_BAD_CRC))

no_packet_ts = recv[recv$dst==SINK & not_packets & (recv$status %in% c(CRYSTAL_HIGH_NOISE, CRYSTAL_SILENCE)), c("epoch", "dst", "n_ta", "length", "status", "time")]
write.table(no_packet_ts, "empty_t.log", row.names=F)

recv=recv[!not_packets,]

sink_recv = recv[recv$dst==SINK,]
duplicates = duplicated(sink_recv[,c("src", "seqn")])
message("Duplicates: ", sum(duplicates))

sink_recv_nodup = sink_recv[!duplicates,]
sink_rsum = rsum[rsum$dst==SINK,]

# looking for log inconsistencies about TA records
t = aggregate(n_ta ~ epoch + dst, ta_log, FUN=length)
colnames(t) <- c("epoch", "dst", "n_rec_ta_present")
tmin = aggregate(n_ta ~ epoch + dst, ta_log, FUN=min)
colnames(tmin) <- c("epoch", "dst", "ta_min")
tmax = aggregate(n_ta ~ epoch + dst, ta_log, FUN=max)
colnames(tmax) <- c("epoch", "dst", "ta_max")

t = merge(t, tmin, all=T)
t = merge(t, tmax, all=T)
rm(tmin)
rm(tmax)
t = merge(rsum[c("epoch", "dst", "n_rec_ta")], t, all=T)

message("TA log inconsistencies")
t[is.na(t$n_rec_ta_present) | is.na(t$n_rec_ta) | t$n_rec_ta_present != t$n_rec_ta,]
rm(t)

usend = unique(send[c("src", "seqn", "epoch")])
# merging send records from both app and Crystal logs
all_send = merge(asend[,c("src", "seqn", "epoch", "acked")], usend, by=c("src", "seqn", "epoch"), all=T)
t = setNames(tryCatch(aggregate(epoch ~ src + seqn, all_send, FUN=length), error=function(e) data.frame(matrix(vector(), 0, 3))), c("src", "seqn", "n_epochs"))
colnames(t) <- c("src", "seqn", "n_epochs")
if (max(t$n_epochs) > 1) {
  message("Inconsistent epoch numbers in app and Crystal logs")
  print(merge(all_send, t[t$n_epochs>1,]))
  quit()
}
# at this point (src, seqn) are unique in all_send

sendrecv = merge(all_send[,c("src", "seqn", "epoch", "acked")], sink_recv_nodup[,c("src", "seqn", "epoch")], by=c("src", "seqn"), all.x=T, suffixes=c(".tx", ".rx"))
total_packets = dim(all_send)[1]
total_sent = dim(unique(usend[c("src", "seqn")]))[1]
nodups_recv = dim(sendrecv[!is.na(sendrecv$epoch.rx) | sendrecv$acked,])[1] # in sendrecv we don't have packets "received but not sent"
PDR = nodups_recv/total_sent
real_PDR = nodups_recv/total_packets
message("Total packets generated: ", total_packets, " sent: ", total_sent, " received: ", nodups_recv , " OLD_PDR: ", PDR, " real PDR: ", real_PDR)

lost = sendrecv[is.na(sendrecv$epoch.rx),c("src", "seqn", "epoch.tx", "acked")]
message("Not delivered packets: ", dim(lost)[1])
lost

notsent = merge(asend[,c("src", "seqn", "epoch", "acked")], send[,c("src", "seqn", "epoch")], by=c("src", "seqn"), all.x=T, suffixes=c(".app", ".cr"))
notsent = notsent[is.na(notsent$epoch.cr),]
message("Not sent packets: ", dim(notsent)[1])
print(notsent)

message("Epochs with loss:")
sort(unique(lost$epoch.tx))

message("Packets received not in the epoch they were sent")
print(sendrecv[!is.na(sendrecv$epoch.rx) & sendrecv$epoch.tx != sendrecv$epoch.rx,])

message("TX and RX records")
s = setNames(aggregate(epoch ~ ssum$src, ssum, FUN=length), c("node", "count_tx"))
r = setNames(aggregate(epoch ~ rsum$dst, rsum, FUN=length), c("node", "count_rx"))
print(merge(s,r,all=T)); rm(s); rm(r);

message("Distribution of number of consecutive misses of the sync packets from the sink")
print(setNames(aggregate(sync_missed ~ ssum$sync_missed, ssum, length), c("num_misses", "num_records")))

message("Nodes that lost a sync message more than twice in a row")
sort(unique(ssum[ssum$sync_missed>2,c("src")]))

# NA means dynamic
if (is.na(conc_senders) | conc_senders > 0) {

    message("Total and unique messages received by sink")

    rx_uniq = setNames(aggregate(
          paste(src,seqn) ~ epoch,
          sink_recv, 
            FUN=function(x) length(unique(x))),
          c("epoch", "uniq_rx"))                
    rx_dups = setNames(aggregate(
          sink_recv$src, 
          list(epoch=sink_recv$epoch), 
            FUN=function(x) (length(x) - length(unique(x)))),
          c("epoch", "dups"))
    acked = setNames(aggregate(
          send$acked, 
          list(epoch=send$epoch), 
            FUN=sum),
          c("epoch", "acks"))

    maxta = setNames(aggregate(
          rsum$n_ta, 
          list(epoch=rsum$epoch), 
            FUN=max),
          c("epoch", "n_ta_max"))

    minta = setNames(aggregate(
          rsum$n_ta, 
          list(epoch=rsum$epoch), 
            FUN=min),
          c("epoch", "n_ta_min"))

    senders = setNames(aggregate(
          paste(src,seqn) ~ epoch,
          send,
            FUN=function(x) length(unique(x))),
          c("epoch", "n_pkt"))
    
    avg_rxta = setNames(aggregate(
          sink_recv_nodup$n_ta, 
          list(epoch=sink_recv_nodup$epoch), 
            FUN=mean),
          c("epoch", "avg_rxta"))

    avg_rxta$avg_rxta <- avg_rxta$avg_rxta + 1

    txrx = merge(senders, rx_uniq, all=T)
    txrx[is.na(txrx$uniq_rx), c("uniq_rx")] <- c(0) # no receives means zero receives

    deliv = merge(
                merge(
                    merge(
                        merge(
                            merge(
                                merge(txrx, rx_dups, all=T),
                                minta, all=T),
                            maxta, all=T),
                        acked, all=T),
                    setNames(sink_rsum[,c("epoch", "n_ta")], c("epoch", "n_ta_sink")), all=T),
                avg_rxta, all=T)

    rownames(deliv) <- deliv$epoch

    deliv[is.na(deliv$n_pkt), "n_pkt"] <- 0
    deliv[is.na(deliv$uniq_rx), "uniq_rx" ] <- 0
    deliv[is.na(deliv$dups),"dups"] <- 0
    print(deliv[,c("epoch", "n_pkt", "uniq_rx", "acks", "dups", "n_ta_sink", "n_ta_min", "n_ta_max", "avg_rxta")])

    incompl = deliv[deliv$uniq_rx<deliv$n_pkt, ]
    incompl_epochs = sort(unique(incompl$epoch))
    message("Epochs with incomplete receive: ", length(incompl_epochs))
    print(incompl_epochs)

    message("Distribution of the number of packets per epoch")
    nsndr_hist = setNames(aggregate(epoch ~ deliv$n_pkt, deliv, FUN=length), c("n_pkt", "cases"))
    print(nsndr_hist)
    message("Distribution of the number of unique RX per epoch")
    uniq_rx_hist = setNames(aggregate(epoch ~ deliv$uniq_rx, deliv, FUN=length), c("uniq_rx", "cases"))
    print(uniq_rx_hist)

    if (!is.na(conc_senders)) {
        full_epochs = sum(deliv$n_pkt==conc_senders, na.rm=T)
        message("Distribution of TAs required (at sink), based only on ", full_epochs, " epochs with all ", conc_senders, " senders")
        if (full_epochs != 0) {
            ta_hist = setNames(aggregate(epoch ~ deliv$n_ta_sink, deliv, subset=deliv$n_pkt==conc_senders, FUN=length), c("n_ta_sink", "cases"))
            print(ta_hist)
            write.table(ta_hist, "ta_hist.txt", row.names=F)
        } else {
            message("Not a single epoch with ", conc_senders, " senders")
        }
    }
} else {
    incompl_epochs = c() # all epochs are complete if there are no senders
}

if (ENERGY_LOGS) {
  energy_pernode = setNames(aggregate(ontime ~ energy$node, energy, mean), c("node", "ontime"))

  message("Energy summary")
  esum = summary(energy_pernode$ontime)
  esum

  min_ontime = as.numeric(esum[1])
  mean_ontime = as.numeric(esum[4])
  max_ontime = as.numeric(esum[6])

  message("Avg. radio ON time (ms/s): ", mean_ontime)

  energy = within(energy, dst<-node)
  en = merge(rsum, energy, by=c("epoch","dst"))
  en = rename(en, c("dst"="src"))
  en = merge(en, ssum[c("epoch", "src", "sync_missed", "n_acks")], by=c("epoch", "src"), all.x=T) 
  en = within(en, ton_total<-(ton_s+ton_t+ton_a)/32768*1000)

  en = within(en, ton_s <- (ton_s/32768)*1000)
  ton_s_pernode = setNames(aggregate(ton_s ~ en$node, en, mean), c("node", "ton_s"))
  en_tas = en[en$n_ta>0,] # filtering out zero cases
  en_tas = within(en_tas, ton_t <- (ton_t/32768)*1000/n_ta)
  en_tas = within(en_tas, ton_a <- (ton_a/32768)*1000/n_ta)
  ton_t_pernode = setNames(aggregate(ton_t ~ en_tas$node, en_tas, mean), c("node", "ton_t"))
  ton_a_pernode = setNames(aggregate(ton_a ~ en_tas$node, en_tas, mean), c("node", "ton_a"))

  energy_pernode = merge(energy_pernode, ton_s_pernode, all.x=T)
  energy_pernode = merge(energy_pernode, ton_a_pernode, all.x=T)
  energy_pernode = merge(energy_pernode, ton_t_pernode, all.x=T)

  ton_total_pernode = setNames(aggregate(ton_total ~ en$node, en, mean), c("node", "ton_total"))
  ton_total_mean = mean(en$ton_total)
  energy_pernode = merge(energy_pernode, ton_total_pernode, all.x=T)

  en = within(en, tf_s <- (tf_s/32768)*1000)
  en = within(en, tf_t <- (tf_t/32768)*1000)
  en = within(en, tf_a <- (tf_a/32768)*1000)

  en_short_s = en[en$n_short_s != 0,]
  en_short_t = en[en$n_short_t != 0,]
  en_short_a = en[en$n_short_a != 0,]

  en_short_t = within(en_short_t, tf_t <- tf_t/n_short_t)
  en_short_t = within(en_short_t, ratio_short_t <- n_short_t/n_ta)
  en_short_a = within(en_short_a, tf_a <- tf_a/n_short_a)
  en_short_a = within(en_short_a, ratio_short_a <- n_short_a/n_ta)

  tf_s_pernode = setNames(aggregate(tf_s ~ en_short_s$node, en_short_s, mean), c("node", "tf_s"))
  tf_t_pernode = setNames(tryCatch(aggregate(tf_t ~ en_short_t$node, en_short_t, mean), error=function(e) data.frame(matrix(vector(), 0, 2))), c("node", "tf_t"))
  tf_a_pernode = setNames(tryCatch(aggregate(tf_a ~ en_short_a$node, en_short_a, mean), error=function(e) data.frame(matrix(vector(), 0, 2))), c("node", "tf_a"))


  pdf("tf.pdf", width=12, height=8)

  short_s_pernode = setNames(aggregate(en_short_s$n_short_s ~ en_short_s$node, en_short_s, sum), c("node", "n_short_s"))
  n_recv_pernode =  setNames(aggregate(s_rx_cnt>0 ~ rsum$dst, rsum, sum), c("node", "n_recv_s"))
  short_s_pernode = merge(short_s_pernode, n_recv_pernode)
  short_s_pernode = within(short_s_pernode, ratio_short_s <- n_short_s/n_recv_s)
  short_s_pernode[!is.finite(short_s_pernode$ratio_short_s), c("ratio_short_s")] <- NA 

  ggplot(data=en_short_s, aes(x=as.factor(node), y=tf_s)) + 
      geom_boxplot(outlier.colour="gray30", outlier.size=1) +
      #xlim(0, max_cca_busy) +    
      ggtitle("tf_s") +
      theme_bw()
  ggplot(data=rsum, aes(x=as.factor(dst), y=s_rx_cnt)) + 
      geom_boxplot(outlier.colour="gray30", outlier.size=1) +
      #xlim(0, max_cca_busy) +    
      ggtitle("S rx count") +
      theme_bw()
  ggplot(data=short_s_pernode, aes(x=as.factor(node), y=ratio_short_s)) + 
      geom_point() +
      ggtitle("Ratio of short S slots") +
      theme_bw()

  if (conc_senders>0) {
      p = ggplot(data=en_short_t, aes(x=as.factor(node), y=tf_t)) + 
      geom_boxplot(outlier.colour="gray30", outlier.size=1) +
      ggtitle("tf_t (epoch averages)") +
      theme_bw()
      print(p)
  ggplot(data=recv, aes(x=as.factor(dst), y=t_rx_cnt)) + 
      geom_boxplot(outlier.colour="gray30", outlier.size=1) +
      #xlim(0, max_cca_busy) +    
      ggtitle("T rx count") +
      theme_bw()
  #ggplot(data=en_short_a, aes(x=as.factor(node), y=ratio_short_t)) + 
  #    geom_boxplot(outlier.colour="gray30", outlier.size=1) +
  #    ggtitle("Ratio of short T slots (epoch averages)") +
  #    theme_bw()
  }

  ggplot(data=en_short_a, aes(x=as.factor(node), y=tf_a)) + 
      geom_boxplot(outlier.colour="gray30", outlier.size=1) +
      ggtitle("tf_a (epoch averages)") +
      theme_bw()
  ggplot(data=ta_log, aes(x=as.factor(dst), y=a_rx_cnt)) + 
      geom_boxplot(outlier.colour="gray30", outlier.size=1) +
      #xlim(0, max_cca_busy) +    
      ggtitle("A rx count") +
      theme_bw()
  ggplot(data=en_short_a, aes(x=as.factor(node), y=ratio_short_a)) + 
      geom_boxplot(outlier.colour="gray30", outlier.size=1) +
      ggtitle("Ratio of short A slots (epoch averages)") +
      theme_bw()
  dev.off()

  energy_pernode = merge(energy_pernode, tf_s_pernode, all.x=T)
  energy_pernode = merge(energy_pernode, tf_a_pernode, all.x=T)
  energy_pernode = merge(energy_pernode, tf_t_pernode, all.x=T)

  message("Radio ON per node")
  energy_pernode

  write.table(energy_pernode, "pernode_energy.txt", row.names=F)
}


if (STATETIME_LOGS) {
  
  # In case of underflows in statetime, some of the values collected (typically idle and rx_hunt)
  # reaches a huge value ~4e6 which is underflow of the uint32_t/1e3 (done by statetime).
  # Here we want to filer out the epoch where such condition occur.
  # Note: in real experiments we DO NOT keep our radio turned on for 4s!
  MAX_T = 4e6
  mask = (energy$idle > MAX_T | energy$rx_hunt > MAX_T |
          energy$tx_preamble > MAX_T | energy$tx_data > MAX_T |
          energy$rx_preamble > MAX_T | energy$rx_data > MAX_T)
  eepoch_remove = energy[mask,]$epoch
  energy = subset(energy, !(energy$epoch %in% eepoch_remove)) # remove all datas (wrt all nodes) related to that epoch        


  # Configuration Considered: Ch 2, plen 128, 64Mhz PRF, 6.8Mbps Datarate
  # Setting N of page 30 DW1000 Datasheet ver 2.17.
  # Average consumption coefficients considered.
  ref_voltage = 3.3 #V
  e_coeff = list(idle=18.0, tx_preamble=83.0, tx_data = 52.0, rx_hunt = 113.0, rx_preamble = 113.0, rx_data = 118.0) # milliamp
  e_coeff = lapply(e_coeff, function(curr_mA) curr_mA / 1e3 * ref_voltage) # watt

  # a statistic on its own
  # 1. Sum all energy related to different phases of a given epoch
  per_epoch = aggregate(. ~ node + epoch, subset(energy, select=-c(phase)), sum)
  per_epoch = within(per_epoch, {
                     idle        <- idle        * e_coeff$idle
                     tx_preamble <- tx_preamble * e_coeff$tx_preamble
                     tx_data     <- tx_data     * e_coeff$tx_data
                     rx_hunt     <- rx_hunt     * e_coeff$rx_hunt
                     rx_preamble <- rx_preamble * e_coeff$rx_preamble
                     rx_data     <- rx_data     * e_coeff$rx_data})

  per_epoch = within(per_epoch, e_total<-(idle + tx_preamble + tx_data + rx_hunt + rx_preamble + rx_data))
  per_node = do.call(data.frame, aggregate(e_total ~ node, per_epoch, FUN = function(x) c(mean = mean(x), sd = sd(x))))
  names(per_node) = c("node", "e_total_mean", "e_total_sd")

  write.table(per_epoch, "perepoch_energy.txt", row.names=F)
  write.table(per_node,  "pernode_energy.txt", row.names=F)

  ## sum statetime per node-phase-epoch
  #per_phase_epoch = aggregate(. ~ node + epoch + phase, energy, sum)
  ## compute per-epoch average wrt energy for all phases
  #per_phase_epoch$epoch = NULL
  #per_phase_avg = aggregate(. ~ node + phase, per_phase_epoch, mean)

  ## sum the mean of energy spent in the three phases
  #per_phase_avg$phase = NULL
  #per_node = aggregate(. ~ node, per_phase_avg, sum)

  message("RX-TX Energy per node")
  per_node
}



if (is.nan(PDR)) PDR = NA

# -- Crystal slot time analysis -------------------------------------------------------
# contributor: Diego Lobba

message("Computing slots time analysis")

# epoch	node	phase	slot_d	round_d	ntx	nrx
slot_log = read.table("glossy_slot.log", header=T, colClasses=c("numeric", "numeric", "character", "numeric", "numeric", "numeric", "numeric"))

# validate statetime with glossy slots info
# sum statetime and glossy slot entries related to the same node-epoch
uniq_phases = unique(energy$phase)
if ("F" %in% uniq_phases) {
  if (length(uniq_phases) > 1)
  # you can make statetime either report full epoch energy (F records), or
  # individual phases estimate (S, T, A without F records).
  stop("Full epochs and single phases energy stats have been found. Stop.")
} else {

  # this makes sense only when having S, T, A records
  e_perepoch = aggregate(. ~ node + epoch, subset(energy, select=-c(phase)), sum)
  e_perepoch = within(e_perepoch, t_total<-(idle + tx_preamble + tx_data + rx_hunt + rx_preamble + rx_data))
  e_perepoch = subset(e_perepoch, select=c(node, epoch, t_total))
  g_perepoch = aggregate(. ~ node + epoch, subset(slot_log, select=-c(phase)), sum)
  g_perepoch = within(g_perepoch, round_d<- round_d * 4.0064 / 1e3) # from 4ns to us
  g_perepoch = subset(g_perepoch, select=c(node, epoch, round_d)) # from 4ns to us
  e_validate = merge(e_perepoch, g_perepoch, by=c("node", "epoch"))
  write.table(e_validate, "statetime_validation.txt", row.names=F)
  e_validate$st_diff = e_validate$t_total - e_validate$round_d
  message("Comparison Epoch time: statetime - time_diff")
  summary(e_validate$st_diff)
}


slot_log["nround"] = slot_log$round_d / slot_log$slot_d
slot_log = ddply(.data=slot_log, .variables=.(node, epoch, phase),
      .fun=function(df) {
        data.frame("nround"=sum(df$nround), "ntx"=sum(df$ntx), "nrx"=sum(df$nrx))})
colnames(slot_log) = c("node", "epoch", "phase", "nround", "ntx", "nrx")

# filter epochs
slot_log = slot_log[slot_log$epoch >= min_epoch & slot_log$epoch <= max_epoch, ]

# average per epoch
perphase_slot_log = ddply(.data=slot_log, .variables=.(node, phase),
      .fun=function(df) {
        data.frame("nround_avg"=mean(df$nround), "nround_std"=sd(df$nround), "ntx"=mean(df$ntx), "nrx"=mean(df$nrx))})

# sum the average number of per-phase slots for each node
pernode_slot_log = ddply(.data=perphase_slot_log, .variables=.(node),
      .fun=function(df) {
        data.frame("nround_avg"=sum(df$nround_avg), "ntx"=sum(df$ntx), "nrx"=sum(df$nrx))})

write.table(perphase_slot_log, "slot_perphase.txt", row.names=F)
write.table(pernode_slot_log,  "slot_pernode.txt", row.names=F)

message("N slots at the sink: ")
print(pernode_slot_log[pernode_slot_log$node == SINK,])
message("Per phase slots at the sink: ")
print(perphase_slot_log[perphase_slot_log$node == SINK,])

rm(slot_log)
rm(perphase_slot_log)
rm(pernode_slot_log)

# -- Per-node and advanced stats ------------------------------------------------------

message("Computing the advanced stats")
message("Considering the following senders")
#senders = unique(c(recv_from, sent_anything ))
senders = heard_from[heard_from != SINK]
print(sort(senders))

pernode = data.frame(node=sort(unique(c(recv_from, sent_anything, heard_from))))

# per-node data PDR

pernode_sent = setNames(tryCatch(aggregate(seqn ~ sendrecv$src, sendrecv, FUN=length), error = function(e) data.frame(matrix(vector(), 0, 2))), c("node", "n_sent"))
pernode_recv = setNames(tryCatch(aggregate(seqn ~ sendrecv$src, sendrecv, subset=!is.na(sendrecv$epoch.rx), FUN=length), error = function(e) data.frame(matrix(vector(), 0, 2))), c("node", "n_recv"))
pernode_sent = pernode_sent[pernode_sent$n_sent>0,]

pernode_pdr = within(
                     merge(pernode_sent, pernode_recv),
                     pdr<-n_recv/n_sent)

pernode = merge(pernode, pernode_pdr, all.x=T)

# counting numbers of lost and received beacons

missed = setNames(tryCatch(aggregate(epoch ~ ssum$src, ssum, subset=ssum$sync_missed>0, FUN=length), error = function(e) data.frame(matrix(vector(), 0, 2))),
                  c("node", "s_missed"))

received = setNames(aggregate(epoch ~ rsum$dst, rsum, subset=rsum$s_rx_cnt>0, FUN=length),
                  c("node", "s_received"))

pernode1 = merge(received, missed, all=T)
pernode1[is.na(pernode1$s_received),c("s_received")] <- 0
pernode1[is.na(pernode1$s_missed),c("s_missed")] <- 0

pernode1 = within(pernode1, s_pdr<-s_received/(s_missed+s_received))
pernode1 = within(pernode1, s_missed_total<-n_epochs-s_received)
pernode1 = within(pernode1, s_pdr_total<-s_received/n_epochs)

total_s_received = sum(pernode1$s_received)

mean_s_pdr = total_s_received/(sum(pernode1$s_missed)+total_s_received)
min_s_pdr = min(pernode1$s_pdr)
max_s_pdr = max(pernode1$s_pdr)
mean_s_pdr_total = total_s_received/((length(senders)+1)*n_epochs) # +1 because the sink is also counted
min_s_pdr_total = min(pernode1$s_pdr_total)
max_s_pdr_total = max(pernode1$s_pdr_total)

pernode = merge(pernode, pernode1, all.x=T)

message("Synch phase reliability stats")
message("S_pdr: ", min_s_pdr, " ", mean_s_pdr, " ", max_s_pdr)
message("s_pdr_total: ", min_s_pdr_total, " ", mean_s_pdr_total, " ", max_s_pdr_total)

pernode_rx_cnt = setNames(aggregate(s_rx_cnt ~ rsum$dst, rsum, subset=rsum$s_rx_cnt>0, FUN=mean), c("node", "s_rx_cnt"))
pernode = merge(pernode, pernode_rx_cnt, all.x=T)

s_compl_recv = sum(rsum$s_rx_cnt>=params$n_tx_s)/(total_s_received)
message("S complete receives: ", s_compl_recv)

t = rsum[rsum$dst!=SINK, c("epoch", "dst", "n_ta")]
t = rename(t, c("dst"="src"))
t = merge(t, ssum[c("epoch", "src", "n_acks")])

tas_pernode = setNames(aggregate(n_ta ~ t$src, t, FUN=sum), c("node", "tas"))
acks_pernode = setNames(aggregate(n_acks ~ t$src, t, FUN=sum), c("node", "acks"))

apdr_pernode = merge(tas_pernode, acks_pernode)
apdr_pernode = within(apdr_pernode, a_pdr<-acks/tas)
pernode = merge(pernode, apdr_pernode[c("node", "a_pdr")], all.x=T)

min_a_pdr = min(apdr_pernode$a_pdr)
max_a_pdr = max(apdr_pernode$a_pdr)
mean_a_pdr = sum(apdr_pernode$acks)/sum(apdr_pernode$tas)
message("A_pdr: ", min_a_pdr, " ", mean_a_pdr, " ", max_a_pdr)

rm(t)
rm(tas_pernode)
rm(acks_pernode)
rm(apdr_pernode)




more_tas = 0
fewer_tas = 0
# counting number of extra or missing TAs (network-wise, as seen by the sink)
if (!is.na(conc_senders)) {
    if (conc_senders>0) {
        tas_diff = deliv$n_ta_sink - (deliv$n_sndr + params$n_empty)
    } else {
        tas_diff = sink_rsum$n_ta - params$n_empty
    }
    more_tas = sum(tas_diff[which(tas_diff>0)])/n_epochs
    fewer_tas = -sum(tas_diff[which(tas_diff<0)])/n_epochs
}

message("Mean # extra TAs: ", more_tas)
message("Mean # missing TAs: ", fewer_tas)

rxta_mean = NA
rxta_max = NA
if (conc_senders>0) {
    rxta_mean = mean(sink_recv_nodup$n_ta) + 1
    rxta_max = max(sink_recv_nodup$n_ta) + 1
}
message("Mean/max delay (in number of TAs): ", rxta_mean, " ", rxta_max)

# nodes that go to sleep earlier or later than the sink

pernode1 = data.frame(node=senders, n_early_sleep=c(0), n_late_sleep=c(0))
n_early = c()
n_late  = c()
for (e in min_epoch:max_epoch) {
    cur = rsum[rsum$epoch==e,]
    tas_diff = cur$n_ta - cur[cur$dst==SINK,]$n_ta
    if (length(tas_diff) == 0) {
      next
    }
    diff_tbl = data.frame(node=cur$dst, tas_diff=tas_diff)
    
    early_nodes = diff_tbl[diff_tbl$tas_diff<0,]$node
    late_nodes  = diff_tbl[diff_tbl$tas_diff>0,]$node
    n_early = c(n_early, length(early_nodes))
    n_late  = c(n_late, length(late_nodes))
    subs_early = pernode1$node %in% early_nodes
    subs_late = pernode1$node %in% late_nodes
    pernode1[subs_early,]$n_early_sleep = pernode1[subs_early,]$n_early_sleep + 1
    pernode1[subs_late, ]$n_late_sleep  = pernode1[subs_late, ]$n_late_sleep  + 1
}
min_early_sleepers  = min(n_early)
mean_early_sleepers = mean(n_early)
max_early_sleepers  = max(n_early)


min_late_sleepers  = min(n_late)
mean_late_sleepers = mean(n_late)
max_late_sleepers  = max(n_late)

message("Min, mean, max # early sleepers: ", min_early_sleepers, " ", mean_early_sleepers, " ", max_early_sleepers)
message("Min, mean, max # late sleepers:  ", min_late_sleepers, " ", mean_late_sleepers, " ", max_late_sleepers)


pernode1 = within(pernode1, early_sl<-n_early_sleep/n_epochs)
pernode1 = within(pernode1, late_sl<-n_late_sleep/n_epochs)
pernode = merge(pernode, pernode1[c("node", "early_sl", "late_sl")], all.x=T)

mean_early_sleep_epochs = mean(pernode$early_sl, na.rm=T)
max_early_sleep_epochs = max(pernode$early_sl, na.rm=T)
mean_late_sleep_epochs = mean(pernode$late_sl, na.rm=T)
max_late_sleep_epochs = max(pernode$late_sl, na.rm=T)
message("Mean and max of early sleep epochs: ", mean_early_sleep_epochs, " ", max_early_sleep_epochs)
message("Mean and max of late sleep epochs: ", mean_late_sleep_epochs, " ", max_late_sleep_epochs)

send_sync = ssum[ssum$sync_missed==0,]
pernode_hops = setNames(aggregate(hops ~ send_sync$src, send_sync, FUN=mean), c("node", "hops"))
pernode_hops$hops <- pernode_hops$hops + 1 # the logged value is actually the "relay count", i.e. hops-1

pernode = merge(pernode, pernode_hops, all.x=T)

hops_mean = mean(send_sync$hops) + 1
hops_max = max(pernode_hops$hops)
message("Mean hops: ", hops_mean)
message("Max hops: ", hops_max)


skew_mean = mean(send_sync$skew)


pernode_skew_min = setNames(aggregate(skew ~ send_sync$src, send_sync, FUN=min), c("node", "skew_min"))
pernode = merge(pernode, pernode_skew_min, all.x=T)
pernode_skew_mean = setNames(aggregate(skew ~ send_sync$src, send_sync, FUN=mean), c("node", "skew_mean"))
pernode = merge(pernode, pernode_skew_mean, all.x=T)
pernode_skew_max = setNames(aggregate(skew ~ send_sync$src, send_sync, FUN=max), c("node", "skew_max"))
pernode = merge(pernode, pernode_skew_max, all.x=T)

skew_diff_all = within(
                       merge(
                             setNames(send_sync[c("src", "skew")], c("node", "skew")), 
                             pernode_skew_mean, all.x=T), 
                       skew_diff<-skew_mean-skew)

pernode_skew_var = setNames(aggregate(abs(skew_diff) ~ skew_diff_all$node, skew_diff_all, FUN=mean), c("node", "skew_var"))
pernode = merge(pernode, pernode_skew_var, all.x=T)

skew_var_mean = mean(pernode_skew_var$skew_var)

pernode_noise_min = setNames(aggregate(noise ~ rsum$dst, rsum, FUN=min), c("node", "noise_min"))
pernode_noise_mean = setNames(aggregate(noise ~ rsum$dst, rsum, FUN=mean), c("node", "noise_mean"))
pernode_noise_max = setNames(aggregate(noise ~ rsum$dst, rsum, FUN=max), c("node", "noise_max"))
pernode = merge(pernode, pernode_noise_min, all.x=T)
pernode = merge(pernode, pernode_noise_mean, all.x=T)
pernode = merge(pernode, pernode_noise_max, all.x=T)
noise_mean = mean(rsum$noise)
noise_max = max(pernode_noise_mean$noise_mean)
message("Mean noise: ", noise_mean)
message("Max noise: ", noise_max)

write.table(pernode, "pernode.txt", row.names=F)

if (ENERGY_LOGS) {
  stats = data.frame(pdr=real_PDR, oldpdr=PDR, 
                     n_pkt=total_packets, n_delivered=nodups_recv,
                     ontime_min=min_ontime, ontime_mean=mean_ontime, ontime_max=max_ontime, 
                     ta_min=min(rsum$n_ta), ta_mean=mean(rsum$n_ta), ta_max=max(rsum$n_ta),
                     s_pdr_min=min_s_pdr,s_pdr_mean=mean_s_pdr,s_pdr_max=max_s_pdr,
                     s_pdr_total_min=min_s_pdr_total,s_pdr_total_mean=mean_s_pdr_total,s_pdr_total_max=max_s_pdr_total,
                     a_pdr_min=min_a_pdr,a_pdr_mean=mean_a_pdr,a_pdr_max=max_a_pdr,
                     hops_mean=hops_mean,
                     hops_max=hops_max,
                     skew_mean=skew_mean,
                     skew_var_mean=skew_var_mean,
                     extra_tas_mean=more_tas, missing_tas_mean=fewer_tas,
                     n_early_sleepers_mean=mean_early_sleepers, n_early_sleepers_max=max_early_sleepers, 
                     n_late_sleepers_mean=mean_late_sleepers, n_late_sleepers_max=max_late_sleepers, 
                     n_early_sleep_epochs_mean=mean_early_sleep_epochs, n_early_sleep_epochs_max=max_early_sleep_epochs,
                     n_late_sleep_epochs_mean=mean_late_sleep_epochs, n_late_sleep_epochs_max=max_late_sleep_epochs,
                     tons_min = min(energy_pernode$ton_s, na.rm=T), tons_mean = mean(energy_pernode$ton_s, na.rm=T), tons_max = max(energy_pernode$ton_s, na.rm=T),
                     tont_min = min(energy_pernode$ton_t, na.rm=T), tont_mean = mean(energy_pernode$ton_t, na.rm=T), tont_max = max(energy_pernode$ton_t, na.rm=T),
                     tona_min = min(energy_pernode$ton_a, na.rm=T), tona_mean = mean(energy_pernode$ton_a, na.rm=T), tona_max = max(energy_pernode$ton_a, na.rm=T),
                     ton_total_min = min(energy_pernode$ton_total, na.rm=T), ton_total_mean = ton_total_mean, ton_total_max = max(energy_pernode$ton_total, na.rm=T),
                     tfs_min = min(energy_pernode$tf_s, na.rm=T), tfs_mean = mean(energy_pernode$tf_s, na.rm=T), tfs_max = max(energy_pernode$tf_s, na.rm=T),
                     tft_min = min(energy_pernode$tf_t, na.rm=T), tft_mean = mean(energy_pernode$tf_t, na.rm=T), tft_max = max(energy_pernode$tf_t, na.rm=T),
                     tfa_min = min(energy_pernode$tf_a, na.rm=T), tfa_mean = mean(energy_pernode$tf_a, na.rm=T), tfa_max = max(energy_pernode$tf_a, na.rm=T),
                     noise_mean=noise_mean, noise_max=noise_max,
                     s_compl_recv = s_compl_recv,
                     n_lazy_nodes = length(lazy_nodes),
                     rxta_mean = rxta_mean, rxta_max = rxta_max
                     )
} else {
  stats = data.frame(pdr=real_PDR, oldpdr=PDR, 
                     n_pkt=total_packets, n_delivered=nodups_recv,
                     ta_min=min(rsum$n_ta), ta_mean=mean(rsum$n_ta), ta_max=max(rsum$n_ta),
                     s_pdr_min=min_s_pdr,s_pdr_mean=mean_s_pdr,s_pdr_max=max_s_pdr,
                     s_pdr_total_min=min_s_pdr_total,s_pdr_total_mean=mean_s_pdr_total,s_pdr_total_max=max_s_pdr_total,
                     a_pdr_min=min_a_pdr,a_pdr_mean=mean_a_pdr,a_pdr_max=max_a_pdr,
                     hops_mean=hops_mean,
                     hops_max=hops_max,
                     skew_mean=skew_mean,
                     skew_var_mean=skew_var_mean,
                     extra_tas_mean=more_tas, missing_tas_mean=fewer_tas,
                     n_early_sleepers_mean=mean_early_sleepers, n_early_sleepers_max=max_early_sleepers, 
                     n_late_sleepers_mean=mean_late_sleepers, n_late_sleepers_max=max_late_sleepers, 
                     n_epochs=n_epochs,
                     n_early_sleep_epochs_mean=mean_early_sleep_epochs, n_early_sleep_epochs_max=max_early_sleep_epochs,
                     n_late_sleep_epochs_mean=mean_late_sleep_epochs, n_late_sleep_epochs_max=max_late_sleep_epochs,
                     noise_mean=noise_mean, noise_max=noise_max,
                     s_compl_recv = s_compl_recv,
                     n_lazy_nodes = length(lazy_nodes),
                     rxta_mean = rxta_mean, rxta_max = rxta_max
                     )
}
write.table(stats, "summary.txt", row.names=F)
max_cca_busy = max(rsum$cca_busy)


pdf("cca_busy_ecdf.pdf", width=12, height=8)
ggplot(data=data.frame(cca_busy=rsum[rsum$dst==SINK & !(rsum$epoch %in% incompl_epochs), c("cca_busy")])) + 
    stat_ecdf(aes(x=cca_busy)) +
    xlim(0, max_cca_busy) +    
    ggtitle("ECDF of cca_busy counter for complete epochs") +
    theme_bw()

if (length(incompl_epochs) != 0) {
    #print(rsum[rsum$dst==SINK & rsum$epoch %in% incompl_epochs,])
    p=ggplot(data=data.frame(cca_busy=rsum[rsum$dst==SINK & rsum$epoch %in% incompl_epochs, c("cca_busy")])) + 
        stat_ecdf(aes(x=cca_busy)) + 
        xlim(0, max_cca_busy) +    
        ggtitle("ECDF of cca_busy counter for incomplete epochs") +
        theme_bw()
    print(p)
}
dev.off()


# general T phase success rate

nodes = c(senders, SINK)
pernode1 = data.frame(node=nodes, n_tx=c(0), n_rx=c(0), T_SR=NA)
if (length(senders)>0 && conc_senders>0) {
    n_tas_with_tx = 0
    for (e in min_epoch:max_epoch) {
        cur_t = ssum[ssum$epoch==e & ssum$n_tx>0,] # nodes who transmitted something during current epoch
        cur_r = recv[recv$epoch==e & recv$n_ta==0,] # nodes who received something during the first T
        nodes_who_received = cur_r$dst
        #print(sort(nodes_who_received))
        nodes_who_were_heard = unique(cur_r$src)
        nodes_who_sent = cur_t$src  # nodes who transmitted during current epoch and TA
        #print(sort(nodes_who_sent))

        # updating the set of sending nodes with those who were heard but didn't log the TX
        # (sometimes it happens in FBK)
        nodes_who_sent = unique(c(nodes_who_sent, nodes_who_were_heard))
        
        s = pernode1$node %in% nodes_who_received
        pernode1[s,]$n_rx = pernode1[s,]$n_rx + 1
        s = pernode1$node %in% nodes_who_sent
        pernode1[s,]$n_tx = pernode1[s,]$n_tx + 1
        
        if (length(nodes_who_sent)>0)
            n_tas_with_tx = n_tas_with_tx + 1
    }

    n_tx_total = sum(pernode1$n_tx)
    n_rx_total = sum(pernode1$n_rx)

    #print(n_tas_with_tx)
    pernode1 = within(pernode1, T_SR<-n_rx/(n_tas_with_tx-n_tx))
    pernode1[is.infinite(pernode1$T_SR), c("T_SR")] <- NA
    print(pernode1)

    T_success_rate = mean(pernode1$T_SR, na.rm=T)
    T_sink_success_rate = pernode1[pernode1$node==SINK,]$T_SR
    T_num_tx = n_tas_with_tx
    T_sink_num_rx = pernode1[pernode1$node==SINK,]$n_rx
} else {
    T_success_rate = NA
    T_sink_success_rate = NA
    T_num_tx = 0
    T_sink_num_rx = 0
}
#print(n_tas_with_tx)

message("Success rate of T phase: ", T_success_rate)
message("Success rate of T phase for the sink: ", T_sink_success_rate)
stats_T = data.frame(T_success_rate=T_success_rate, T_sink_success_rate=T_sink_success_rate, T_num_tx=T_num_tx, T_sink_num_rx=T_sink_num_rx)
write.table(stats_T, "summary_T.txt", row.names=F)
write.table(pernode1[c("node", "T_SR")], "pernode_T.txt", row.names=F)

