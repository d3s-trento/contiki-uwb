#!/bin/sh
mkdir -p "$PROJECT_DIR/.cache/analysis/$1"

get_log.sh $1 | \
	tee \
		>(grep "'epoch [0-9]\+ event" | awk 'BEGIN{print "node_id,epoch"} {print substr($4,0,length($4)-8)", "$7}' | convert_csv_to_feather.py > "$PROJECT_DIR/.cache/analysis/$1/event$1.fea") \
		> /dev/null

#		>(grep "'\[n\]TX_FS E [0-9]\+" | awk 'BEGIN{print "node_id,epoch,repetition_i"} {print gensub(/\.evb1000/,"","g",$4)","int(gensub(/'"'"'/,"","g",$8)/100)","(gensub(/'"'"'/,"","g",$8)%100)}' | convert_csv_to_feather.py > "$PROJECT_DIR/.cache/analysis/$1/tx_fs$1.fea") \
#		>(grep "'\[n\]epoch " | awk 'BEGIN{print "node_id,epoch,hop"} {print substr($4,0,length($4)-8)", "$7", "substr($9,0,length($9)-1)}' | convert_csv_to_feather.py > "$PROJECT_DIR/.cache/analysis/$1/e$1.fea") \
#		>(grep 'eSFD' | awk 'BEGIN{print "node_id,epoch,repetition_i,expected,actual"} {print substr($4, 0, length($4)-8)", "int($7/100)", "($7 % 100)", "$9", "gensub(/'"'"'/,"", "g", $11)}' | convert_csv_to_feather.py > "$PROJECT_DIR/.cache/analysis/$1/$1.fea") \

# get_log.sh $1 | \
# 	tee \
# 		>(grep 'eSFD' | awk 'BEGIN{print "node_id,epoch,repetition_i,expected,actual"} {print substr($4, 0, length($4)-8)", "int($7/100)", "($7 % 100)", "$9", "gensub(/'"'"'/,"", "g", $11)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/$1.csv") \
# 		>(grep "'\[n\]epoch " | awk 'BEGIN{print "node_id,epoch,hop"} {print substr($4,0,length($4)-8)", "$7", "substr($9,0,length($9)-1)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/e$1.csv") \
# 		>(grep "'epoch [0-9]\+ event" | awk 'BEGIN{print "node_id,epoch"} {print substr($4,0,length($4)-8)", "$7}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/event$1.csv") \
# 		>(grep "'\[tsm [0-9]*\]" | awk 'BEGIN {print "node_id,epoch,slots"} {print substr($4,0,length($4)-8)","substr($7,0,length($7)-7)","substr($8,0,length($8)-1)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/tsm$1.csv") \
# 		>(grep "'\[n\]TX_FS E [0-9]\+" | awk 'BEGIN{print "node_id,epoch,repetition_i"} {print gensub(/\.evb1000/,"","g",$4)","int(gensub(/'"'"'/,"","g",$8)/100)","(gensub(/'"'"'/,"","g",$8)%100)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/tx_fs$1.csv") \
# 		> /dev/null

# get_log.sh $1 | \
# 	tee \
# 		>(grep 'eSFD' | awk 'BEGIN{print "node_id,epoch,expected,actual"} {print substr($4, 0, length($4)-8)", "$7", "$9", "gensub(/'"'"'/,"", "g", $11)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/$1.csv") \
# 		>(grep "'\[n\]epoch " | awk 'BEGIN{print "node_id,epoch,hop"} {print substr($4,0,length($4)-8)", "$7", "substr($9,0,length($9)-1)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/e$1.csv") \
# 		>(grep "'epoch [0-9]\+ event" | awk 'BEGIN{print "node_id,epoch"} {print substr($4,0,length($4)-8)", "$7}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/event$1.csv") \
# 		>(grep "'\[tsm [0-9]*\]" | awk 'BEGIN {print "node_id,epoch,slots"} {print substr($4,0,length($4)-8)","substr($7,0,length($7)-7)","substr($8,0,length($8)-1)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/tsm$1.csv") \
# 		>(grep "'\[n\]TX_FS E [0-9]\+" | awk '{print gensub(/\.evb1000/,"","g",$4)","gensub(/'"'"'/,"","g",$8)} BEGIN{print "node_id,epoch"}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/tx_fs$1.csv") \
# 		> /dev/null

#get_log.sh $1 | grep 'eSFD' | awk 'BEGIN{print "node_id,epoch,expected,actual"} {print substr($4, 0, length($4)-8)", "$7", "$9", "gensub(/'"'"'/,"", "g", $11)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/$1.csv";
#get_log.sh $1 | grep "'\[n\]epoch " | awk 'BEGIN{print "node_id,epoch,hop"} {print substr($4,0,length($4)-8)", "$7", "substr($9,0,length($9)-1)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/e$1.csv";
#get_log.sh $1 | grep "'epoch [0-9]\+ event" | awk 'BEGIN{print "node_id,epoch"} {print substr($4,0,length($4)-8)", "$7}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/event$1.csv";
#get_log.sh $1 | grep "'\[tsm [0-9]*\]" | awk 'BEGIN {print "node_id,epoch,slots"} {print substr($4,0,length($4)-8)","substr($7,0,length($7)-7)","substr($8,0,length($8)-1)}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/tsm$1.csv";
#get_log.sh $1 | grep "'\[n\]TX_FS E [0-9]\+" | awk '{print gensub(/\.evb1000/,"","g",$4)","gensub(/'"'"'/,"","g",$8)} BEGIN{print "node_id,epoch"}' | zstd -z > "$PROJECT_DIR/.cache/analysis/$1/tx_fs$1.csv";

