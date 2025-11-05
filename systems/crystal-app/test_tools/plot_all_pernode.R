#!/usr/bin/env Rscript
library(grid)
library(plyr)
library(ggplot2)
library(scales)
library(ggrepel)

metrics_point= c("pdr", "s_pdr", "a_pdr", "T_SR", "duty")
metrics_point= c(metrics_point, "ton_s", "ton_a", "ton_t") #, "ton_total")
metrics_point= c(metrics_point, "tf_s", "tf_a", "tf_t", "hops")


f_stats = "pernode.txt"
f_energy_stats = "pernode_energy.txt"
f_t_stats = "pernode_T.txt"

data = read.table(f_stats, header=T)
if (file.exists(f_energy_stats)) {
        data = merge(data, read.table(f_energy_stats, header=T), all=T)
}
if (file.exists(f_t_stats)) {
	data = merge(data, read.table(f_t_stats, header=T), all=T)
}



#data = within(data, duty <- ontime/1000)

title = ""
update_geom_defaults("point", list(size = 1))

pdf("pernode.pdf", 12, 10)
for (m in metrics_point) {
	if (m %in% names(data)) {
		print (m)
		prefix = m

		plot = ggplot(data=data, aes_string(x="node", y=m)) + 
			theme_bw() +
			theme(text = element_text(size=16)) + 
			theme(legend.key.size =  unit(1.5, "lines"), legend.key = element_rect(fill='white')) +
					
			#guides(fill = guide_legend(nrows=2)) + 
			theme(panel.border = element_rect(color = 'black', fill=NA))+
			theme(legend.direction = "horizontal", legend.position="bottom", legend.text=element_text(size = rel(1.0))) +
			theme(axis.text=element_text(colour="black")) +
			#labs(color=st_color) 

			geom_point() +
			geom_text_repel(aes(label=node), size = 4.5, box.padding = unit(0.2, "lines"), point.padding = unit(0.08, "lines")) +
			ggtitle(title)

		print(plot)
	} else {
		print(paste(m, "NOT PRESENT"))
		
	}
}

#print(all)


#metrics_violin= c("noise_mean", "hops", "skew")
allnoise = read.table("recv_summary.log", h=T)[c("dst", "noise", "channel", "cca_busy")]
skew_hops = read.table("send_summary.log", h=T)[c("src", "sync_missed", "skew", "hops")]
skew_hops = skew_hops[skew_hops$sync_missed==0,]
skew_hops = within(skew_hops, hops<-hops+1)

allnoise$node <- as.factor(allnoise$dst)
skew_hops$node <- as.factor(skew_hops$src)

print("hops+")
ggplot(data=skew_hops, aes_string(x="node", y="hops")) + 
	theme_bw() +
	theme(text = element_text(size=16)) + 
	theme(legend.key.size =  unit(1.5, "lines"), legend.key = element_rect(fill='white')) +
			
	theme(panel.border = element_rect(color = 'black', fill=NA))+
	theme(legend.direction = "horizontal", legend.position="bottom", legend.text=element_text(size = rel(1.0))) +
	theme(axis.text=element_text(colour="black")) +
	theme(axis.text.x=element_text(angle=90)) + 
	#geom_violin(kernel="gaussian") +
	geom_boxplot(outlier.colour="gray30", outlier.size=1) +
	ggtitle(title)


print("noise")
noise_min = min(allnoise$noise)
noise_max = max(allnoise$noise)
plot_noise<-function(data, title) {
	noise_mean = setNames(aggregate(noise ~ data$node, data, FUN=mean), c("node", "noise"))
	noise_sd = setNames(aggregate(noise ~ data$node, data, FUN=sd), c("node", "sd"))
	noise_mean = merge(noise_mean, noise_sd, by="node", all=T)
	
	overall_mean = mean(data$noise)
	overall_median = median(data$noise)

	return (ggplot(data=data, aes_string(x="node", y="noise")) + 
	theme_bw() +
	theme(text = element_text(size=16)) + 
	theme(legend.key.size =  unit(1.5, "lines"), legend.key = element_rect(fill='white')) +
			
	theme(panel.border = element_rect(color = 'black', fill=NA))+
	theme(legend.direction = "horizontal", legend.position="bottom", legend.text=element_text(size = rel(1.0))) +
	theme(axis.text=element_text(colour="black")) +
	theme(axis.text.x=element_text(angle=90)) + 
	#geom_violin(kernel="gaussian") +
	geom_boxplot(outlier.colour="gray30", outlier.size=1) +
	geom_point(data=noise_mean, aes(x=node, y=noise), shape=18, color="blue", size=3.5) +
	geom_point(data=noise_mean, aes(x=node, y=noise-sd), shape=95, color="blue", size=5) +
	geom_point(data=noise_mean, aes(x=node, y=noise+sd), shape=95, color="blue", size=5) +
	geom_hline(yintercept=overall_mean, color="red") +
	geom_hline(yintercept=overall_median, color="black") +
	ylim(noise_min, noise_max) +
	ggtitle(title))
}

plot_noise(allnoise, "Noise: aggregated over all channels")
channels = sort(unique(allnoise$channel))
message("All channels:")
channels

for (c in channels) {
	message(c)
	print(plot_noise(allnoise[allnoise$channel==c,], paste("Noise, ch.", c)))
}


print("cca_busy")
busy_mean = setNames(aggregate(cca_busy ~ allnoise$node, allnoise, FUN=mean), c("node", "cca_busy"))
ggplot(data=allnoise, aes_string(x="node", y="cca_busy")) + 
	theme_bw() +
	theme(text = element_text(size=16)) + 
	theme(legend.key.size =  unit(1.5, "lines"), legend.key = element_rect(fill='white')) +
			
	theme(panel.border = element_rect(color = 'black', fill=NA))+
	theme(legend.direction = "horizontal", legend.position="bottom", legend.text=element_text(size = rel(1.0))) +
	theme(axis.text=element_text(colour="black")) +
	theme(axis.text.x=element_text(angle=90)) + 
	#geom_violin(kernel="gaussian") +
	geom_boxplot(outlier.colour="gray30", outlier.size=1) +
	geom_point(data=busy_mean, aes_string(x="node", y="cca_busy"), shape=18, color="blue", size=3.5) +
  ggtitle(title)

print("skew")
ggplot(data=skew_hops, aes_string(x="node", y="skew")) + 
	theme_bw() +
	theme(text = element_text(size=16)) + 
	theme(legend.key.size =  unit(1.5, "lines"), legend.key = element_rect(fill='white')) +
			
	theme(panel.border = element_rect(color = 'black', fill=NA))+
	theme(legend.direction = "horizontal", legend.position="bottom", legend.text=element_text(size = rel(1.0))) +
	theme(axis.text=element_text(colour="black")) +
	theme(axis.text.x=element_text(angle=90)) + 
	#geom_violin(kernel="gaussian") +
	geom_boxplot(outlier.colour="gray30", outlier.size=1) +
	ggtitle(title)

