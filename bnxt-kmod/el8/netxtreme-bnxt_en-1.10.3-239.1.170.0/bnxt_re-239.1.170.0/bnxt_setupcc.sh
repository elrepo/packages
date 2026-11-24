#!/bin/bash
# SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
# Copyright (c) 2018 - 2026 Broadcom Inc.

# The bnxt_setupcc.sh is responsible for configuring the qos settings on the NIC device

MAX_INTERFACE_COUNT=2
VERSION="239.1.170.0"

function usage {
    echo "bnxt_setupcc.sh version: " $VERSION
    cat <<EOM
Usage: $(basename "$0") [OPTION]...

  -d          RoCE Device Name (e.g. bnxt_re0, bnxt_re_bond0)
  -i          Ethernet Interface Name (e.g. p1p1 or for bond, specify slave interfaces like -i p6p1 -i p6p2)
  -m [1-3]    1 - PFC only
          2 - CC only
          3 - PFC + CC mode
  -v	      1 - Enable priority vlan
  -r [0-7]    RoCE Packet Priority
  -s VALUE    RoCE Packet DSCP Value
  -c [0-7]    RoCE CNP Packet Priority
  -p VALUE    RoCE CNP Packet DSCP Value
  -b VALUE    RoCE Bandwidth percentage for ETS configuration - Default is 50%
  -t [2]      Default mode (Only RoCE v2 is supported - Input Ignored)
  -C VALUE    Set CNP Service Type
  -u [1-4]    Utility to configure QoS settings
          1 - Use bnxtqos utility. Will disable lldptool if enabled.
          2 - Use lldptool
          3 - Use Broadcom niccli utility. Will disable lldptool if enabled.
          4 - Use dcb command (default)
  -V          Display version
  -h          Display help
EOM
    exit 2
}

function version {
    echo "bnxt_setupcc.sh version: " $VERSION
    exit 2
}

while getopts ":r:s:c:p:i:d:v:C:m:b:t:u:hV" o; do
    case "${o}" in
        r)
            r=${OPTARG}
            ;;
        s)
            s=${OPTARG}
            ;;
        c)
            c=${OPTARG}
            ;;
        p)
            p=${OPTARG}
            ;;
        i)
            i+=${OPTARG}" "
            ;;
        d)
            d=${OPTARG}
            ;;
        v)
            v=${OPTARG}
            ;;
	C)
            CNP_SERVICE_TYPE=${OPTARG}
            ;;
        m)
            m=${OPTARG}
            ;;
        b)
            b=${OPTARG}
            ;;
	#ignoring whatever the user passes for RoCE mode
        t)
            t=2
            ;;
	u)
	    u=${OPTARG}
	    ;;
        h)
            usage
            ;;
        V)
            version
            ;;

        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [ -z "${i}" ] || [ -z "${r}" ]; then
    usage
fi

if [ -z "${d}" ]
then
	sysfspath="/sys/class/infiniband/"
	for rdev in `ls $sysfspath`
	do
		pr=`echo $rdev|awk '{if($0 ~ /bnxt_/) print "yes"; else print "no";}'`
		if [ "$pr" == "no" ]
		then
			continue
		fi
		dpath="$sysfspath/$rdev/ports/1/gid_attrs/ndevs/0"
		ipiface=`cat $dpath`
		if [ "$ipiface " == "${i}" ]
		then
			d=$rdev
			break
		fi
	done
	if [ "${d}" == "" ]
	then
		echo "Could not locate RDMA device for ${i}interface."
		echo "Try using -d option explicitly."
		exit -1
	fi
	echo "Using RDMA device ${d} for interface ${ipiface}"
fi

if [ -z "${u}" ] || [ "$u" == 4 ]
then
	QOS_TOOL=4
elif [ "$u" == 2 ]
then
	QOS_TOOL=2
elif [ "$u" == 3 ]
then
	QOS_TOOL=3
elif [ "$u" == 1 ]
then
	QOS_TOOL=1
else
	echo "Invalid utility selected (-u option)"
	usage
	exit 1
fi

if [ $QOS_TOOL -eq 1 ]
then
	type bnxtqos >/dev/null 2>&1 ||
	       { echo >&2 "bnxtqos utility is not installed.  Aborting."; exit 1; }
elif [ $QOS_TOOL -eq 2 ]
then
	type lldptool >/dev/null 2>&1 ||
	       { echo >&2 "lldptool is not installed.  Aborting."; exit 1; }
elif [ $QOS_TOOL -eq 3 ]
then
	type niccli >/dev/null 2>&1 ||
	       { echo >&2 "niccli is not installed.  Aborting."; exit 1; }
else
	type dcb >>/dev/null 2>&1 ||
		{ echo >&2 "dcb is not installed.  Aborting."; exit 1; }
fi

EN_ROCE_DSCP=0
#s p r c needs to be checked for format conversion
scale='^[0-9]+$'
hexscale='^(0[xX])[0-9a-fA-F]+$'

if [ ! -z "${s}" ]
then
	if [[ $s =~ $scale ]]
	then
		tmp=`printf "0x%x" $s`
		s=$tmp
	else
		if ! [[ $s =~ $hexscale ]]
		then
			echo Invalid RoCE DSCP value
			exit -1
		fi
	fi
EN_ROCE_DSCP=1
fi

EN_CNP_DSCP=0
if [ ! -z "${p}" ]
then
	if [[ $p =~ $scale ]]
	then
		tmp=`printf "0x%x" $p`
		p=$tmp
	else
		if ! [[ $p =~ $hexscale ]]
		then
			echo Invalid RoCE CNP packet DSCP value
			exit -1
		fi
	fi
EN_CNP_DSCP=1
fi


EN_ROCE_PRI=0
if [ ! -z "${r}" ]
then

	if [[ $r =~ $scale ]]
	then
		tmp=`printf "0x%x" $r`
		r=$tmp
	else
		if ! [[ $r =~ $hexscale ]]
		then
			echo Invalid RoCE packet priority
			exit -1
		fi
	fi
EN_ROCE_PRI=1
fi


EN_CNP_PRI=0
if [ ! -z "${c}" ]
then
	if [[ $c =~ $scale ]]
	then
		tmp=`printf "0x%x" $c`
		c=$tmp
	else
		if ! [[ $c =~ $hexscale ]]
		then
			echo Invalid RoCE CNP packet priority
			exit -1
		fi
	fi
EN_CNP_PRI=1
fi


if_count=0

for interface in $i
do
if_count=`expr $if_count + 1`
done

if [ $if_count -gt $MAX_INTERFACE_COUNT ];
then
	echo "Number of interfaces more than supported"
	exit 1;
fi

INF_NAME1=`echo $i |cut -d ' ' -f1`
INF_NAME2=`echo $i |cut -d ' ' -f2`

if [[ "$INF_NAME1" == "$INF_NAME2" ]];
then
	INF_NAME2=
fi

if [ ! -f /sys/class/net/${INF_NAME1}/address ];
then
  echo Interface ${INF_NAME1} not found
  INF_NAME=$(cat /sys/class/infiniband/$d/ports/1/gid_attrs/ndevs/0)
  if [ -z "$INF_NAME" ]; then
	  exit 1
  else
	  echo "Interface $INF_NAME1 moved to $INF_NAME"
	  INF_NAME1=$INF_NAME
  fi
fi

PCI_BUS_ADDR1=$(ethtool -i ${INF_NAME1} | grep 'bus-info' | awk '{print $2}')

if [ "$INF_NAME2" != "" ];
then
  if [ ! -f /sys/class/net/${INF_NAME2}/address ];
  then
    echo Interface ${INF_NAME2} not found
    exit 1
  fi

  PCI_BUS_ADDR2=$(ethtool -i ${INF_NAME2} | grep 'bus-info' | awk '{print $2}')
else
	if [ -n "${RUN_SETUP_SCRIPT}" ]; then
		PCI_BUS_ADDR1=$(readlink -f /sys/class/infiniband/$d/device | \
				awk -F'/' '{print $(NF-0)}')
	fi
fi

if [ -z $m ]
then
	ENABLE_PFC=1
	ENABLE_CC=1
elif [ "$m" == "1" ]
then
	ENABLE_PFC=1
	ENABLE_CC=0
elif [ "$m" == "2" ]
then
	ENABLE_CC=1
	ENABLE_PFC=0
elif [ "$m" == "3" ]
then
	ENABLE_PFC=1
	ENABLE_CC=1
else
	echo "Invalid value for mode (-m option)"
	exit 1;
fi

ENABLE_DSCP=0
echo ENABLE_PFC = $ENABLE_PFC ENABLE_CC = $ENABLE_CC

if [ $EN_ROCE_DSCP -eq 1 ] || [ $EN_CNP_DSCP -eq 1 ]
then
	ENABLE_DSCP=1
fi

ENABLE_PRI=0
if [ $EN_ROCE_PRI -eq 1 ] || [ $EN_CNP_PRI -eq 1 ]
then
	ENABLE_PRI=1
fi

ENABLE_DSCP_BASED_PFC=1
if [ "$v" == "1" ]
then
	ENABLE_DSCP_BASED_PFC=0
fi

echo ENABLE_DSCP = $ENABLE_DSCP ENABLE_DSCP_BASED_PFC = $ENABLE_DSCP_BASED_PFC

DEV_NAME=$d
ROCE_DSCP=$s
ROCE_PRI=$r
ROCE_CNP_DSCP=$p
ROCE_CNP_PRI=$c
ROCE_BW=$b

if [ -z $ROCE_BW ]
then
	ROCE_BW=50
fi

if ! [[ "$ROCE_BW" =~ ^-?[0-9]+$ ]] || [ "$ROCE_BW" -eq 0 ] || [ "$ROCE_BW" -ge 100 ]; then
	echo "Invalid ROCE_BW value: $ROCE_BW. Enter an integer between 0 & 100 for option '-b'."
	exit -1
fi

L2_BW=`expr 100 - $ROCE_BW`

# Only RoCE v2 is supported
ROCE_MODE=2

echo L2 $L2_BW RoCE $ROCE_BW

echo "Using Ethernet interface $INF_NAME1  $INF_NAME2 and RoCE interface $DEV_NAME"

if [ "$CNP_SERVICE_TYPE" == "1" ]; then
        echo "CNP_SERVICE_TYPE is set to 1"
else
        CNP_SERVICE_TYPE=0
        if test -f "/sys/kernel/debug/bnxt_re/$DEV_NAME/info"; then
                CNP_SERVICE_TYPE=`cat /sys/kernel/debug/bnxt_re/$DEV_NAME/info|grep fw_service_prof_type_sup|awk '{print $3}'`
        fi
fi

# Define priority 2 tc mapping
pri2tc=""

for i in `seq 0 7`;
do
    if [ $EN_ROCE_PRI -eq 1 ] && [ $i -eq `printf "%d" $ROCE_PRI` ]; then
        pri2tc+=",$i:1"
    elif [ $EN_CNP_PRI -eq 1 ] && [ $i -eq `printf "%d" $ROCE_CNP_PRI` ] && [ $CNP_SERVICE_TYPE -eq 1 ] && [ $ENABLE_CC -eq 1 ]; then
       pri2tc+=",$i:2"
    else
        pri2tc+=",$i:0"
    fi
done

pri2tc=${pri2tc:1}

ethtool -A $INF_NAME1 rx off tx off
if [ "$INF_NAME2" != "" ];
then
    ethtool -A $INF_NAME2 rx off tx off
fi

bnxt_qos_rem_app_tlvs() {
	INF_NAME=$1
	j=0
	for i in `bnxtqos -dev=$INF_NAME get_qos|grep -e "Priority:" -e "Sel:" -e DSCP -e UDP -e "Ethertype:"|awk -F":" '{ print $2}'`
	do
		if [ $i == 0x8915 ]
		then
			i=35093
	fi

	if [ $j -eq 0 ]
	then
		APP_0=$i
	else
		APP_0=$APP_0,$i
	fi

	j=`expr $j + 1`

	if [ $j -eq 3 ]
	then
		bnxtqos -dev=$INF_NAME set_apptlv -d app=$APP_0
		j=0
	fi
	done
}

lldptool_rem_app_tlvs() {
	INF_NAME=$1
	for i in `lldptool -t -i $INF_NAME -V APP -c app|awk -F"(" '{ print $2}'|awk -F")" '{ print $1}'| sed "1 d"`
	do
		lldptool -T -i $INF_NAME -V APP -d app=$i
	done
}

niccli_rem_app_tlv() {
	PCI_BUSADDR=$1

	niccli_output=$(niccli --dev $PCI_BUSADDR qos --ets --show)
	if [ $? -ne 0 ]; then
		echo "Error: Failed Setting QOS Parameters"
		exit -1
	fi

	local del_commands=()
	j=0
	for i in `echo "$niccli_output"|grep -e \
                  "Priority:" -e "Sel:" -e DSCP -e UDP -e "Ethertype:"|awk -F":" '{ print $2}'`
	do
		if [ $i == 0x8915 ]
		then
			i=35093
	fi

	if [ $j -eq 0 ]
	then
		APP_0=$i
	else
		APP_0=$APP_0,$i
	fi

	j=`expr $j + 1`

	if [ $j -eq 3 ]
	then
		del_commands+=("qos --apptlv --del --app $APP_0")
		j=0
	fi
	done
	printf "%s\n" "${del_commands[@]}"
}

dcb_rem_app_tlv_pfc() {
    local dev="$1"
    local output
    output=$(dcb app show dev "$dev")

    # Parse the output and create del commands
    while IFS= read -r line; do
        if [[ "$line" == *":"* ]]; then
            # Extract the selector type and value
            local selector
            selector=$(echo "$line" | awk '{print $1}')
            local entries
            entries=$(echo "$line" | awk '{$1=""; print $0}' | sed 's/^ //')
            IFS=' ' read -r -a entry_array <<< "$entries"

            # Loop through each entry
            for entry in "${entry_array[@]}"; do
                #echo "dcb app del dev $dev $selector $entry"
                dcb app del dev "$dev" "$selector" "$entry"
            done
        fi
    done <<< "$output"
    dcb pfc set dev "$dev"  prio-pfc all:off

}

bnxt_qos_pgm_pfc_ets() {
	INF_NAME=$1
	echo "Setting pfc/ets on $INF_NAME"
	bnxt_qos_rem_app_tlvs $INF_NAME
	# bnxtqos requires nvm cfg 155,255,269 and 270 to be disabled
	if [ $CNP_SERVICE_TYPE -eq 1 ]
	then
		if ! bnxtqos -dev=$INF_NAME set_ets tsa=0:ets,1:ets,2:strict,3:strict,4:strict,5:strict,6:strict,7:strict priority2tc=$pri2tc tcbw=$L2_BW,$ROCE_BW; then
			echo "Does not support 8 TCs. Configuring 3 TCs "
			if ! bnxtqos -dev=$INF_NAME set_ets tsa=0:ets,1:ets,2:strict priority2tc=$pri2tc tcbw=$L2_BW,$ROCE_BW; then
				echo "Does not support 3 TCs. Configuring 4 TCs "
				if ! bnxtqos -dev=$INF_NAME set_ets tsa=0:ets,1:ets,2:strict,3:ets priority2tc=$pri2tc tcbw=$L2_BW,$ROCE_BW; then
					echo "bnxt_qos_pgm_pfc_ets: does not support 4 TCs"
					echo "Cannot program ets and priority map!"
					# continue execution
				fi
			fi
		fi

	else
		bnxtqos -dev=$INF_NAME set_ets tsa=0:ets,1:ets priority2tc=$pri2tc tcbw=$L2_BW,$ROCE_BW
	fi

	if [ ! -z "$ROCE_PRI" ] && [ $ENABLE_PFC -eq 1 ]
	then
		bnxtqos -dev=$INF_NAME set_pfc enabled=`printf "%d" $ROCE_PRI`
		sleep 1
		bnxtqos -dev=$INF_NAME set_apptlv app=`printf "%d" $ROCE_PRI`,3,4791
		sleep 1
	else
		bnxtqos -dev=$INF_NAME set_pfc enabled=none
	fi

	if [ $ENABLE_DSCP_BASED_PFC -eq 1 ] || [ $EN_ROCE_DSCP -eq 1 ]
	then
		bnxtqos -dev=$INF_NAME set_apptlv app=`printf "%d" $ROCE_PRI`,5,`printf "%d" $ROCE_DSCP`
		sleep 1
	fi
	if [ $ENABLE_CC -eq 1 ] &&  [ $CNP_SERVICE_TYPE -eq 1 ]
	then
		bnxtqos -dev=$INF_NAME set_apptlv app=`printf "%d" $ROCE_CNP_PRI`,5,`printf "%d" $ROCE_CNP_DSCP`
		sleep 1
	fi
	sleep 1
	bnxtqos -dev=$INF_NAME get_qos
}

lldptool_pgm_pfc_ets() {
	INF_NAME=$1
	lldptool_rem_app_tlvs $INF_NAME
	lldptool -T -i $INF_NAME -V ETS-CFG \
		tsa="0:ets,1:ets,2:strict,3:strict,4:strict,5:strict,6:strict,7:strict" \
		up2tc=$pri2tc  tcbw=$L2_BW,$ROCE_BW,0,0,0,0,0,0
	if [ ! -z "$ROCE_PRI" ] && [ $ENABLE_PFC -eq 1 ]
	then
		lldptool -L -i $INF_NAME adminStatus=rxtx
		lldptool -T -i $INF_NAME -V PFC enabled=`printf "%d" $ROCE_PRI`
		lldptool -T -i $INF_NAME -V APP app="`printf "%d" $ROCE_PRI`,3,4791"
		if [ $ENABLE_DSCP_BASED_PFC -eq 1 ] || [ $EN_ROCE_DSCP -eq 1 ]
		then
			lldptool -T -i $INF_NAME -V APP app="`printf "%d" \
				$ROCE_PRI`,5,`printf "%d" $ROCE_DSCP`"
		fi
	else
		lldptool -T -i $INF_NAME -V PFC enabled=none
	fi
	if [ $ENABLE_CC -eq 1 ] &&  [ $CNP_SERVICE_TYPE -eq 1 ]
	then
		lldptool -T -i $INF_NAME -V APP app="`printf "%d" \
			$ROCE_CNP_PRI`,5,`printf "%d" $ROCE_CNP_DSCP`"
		sleep 1
	fi
}
dcb_pgm_pfc_ets() {
	INF_NAME=$1
	echo "Setting pfc/ets $INF_NAME"
	if [[ -z "${RUN_SETUP_SCRIPT}" ]]; then
		dcb_rem_app_tlv_pfc $INF_NAME
	fi
	# replace ',' with ' ' in pri2tc as
	# dcb accepts pri2tc in this format.
	# Eg : 0:0 1:0 2:0 3:1 4:0 5:0 6:0 7:2
	dcb_pri2tc="${pri2tc//,/ }"
	if [ $CNP_SERVICE_TYPE -eq 1 ]
	then
		if ! dcb ets set dev \
		    $INF_NAME \
		    tc-tsa 0:ets 1:ets 2:strict \
		    tc-bw 0:$L2_BW 1:$ROCE_BW \
		    prio-tc $dcb_pri2tc > /dev/null 2>&1; then
			echo "Setting 3 TCs failed. Trying 8 TCs now"
			if ! dcb ets set dev $INF_NAME \
			    tc-tsa 0:ets 1:ets 2:strict 3:strict 4:strict 5:strict 6:strict 7:strict \
			    tc-bw 0:$L2_BW 1:$ROCE_BW \
			    prio-tc $dcb_pri2tc; then
				echo "Setting 8 TCs failed. Trying 4 TCs now"
				if ! dcb ets set dev $INF_NAME \
				    tc-tsa 0:ets 1:ets 2:strict 3:ets \
				    tc-bw 0:$L2_BW 1:$ROCE_BW \
				    prio-tc $dcb_pri2tc; then
					echo "dcb_pgm_pfc_ets: setting 4 TCs failed."
					# continue execution
				fi
			fi
		fi
	else
		dcb ets set dev $INF_NAME tc-tsa 0:ets 1:ets \
			tc-bw 0:$L2_BW 1:$ROCE_BW \
			prio-tc $dcb_pri2tc
	fi
	if [ ! -z "$ROCE_PRI" ] && [ $ENABLE_PFC -eq 1 ]
	then
		dcb pfc set dev $INF_NAME prio-pfc `printf "%d" $ROCE_PRI`:on
		dcb app add dev $INF_NAME dgram-port-prio 4791:$ROCE_PRI
	else
		dcb pfc set dev $INF_NAME prio-pfc all:off
	fi
	if [ $ENABLE_DSCP_BASED_PFC -eq 1 ] || [ $EN_ROCE_DSCP -eq 1 ]
	then
		dcb app add dev $INF_NAME dscp-prio $ROCE_DSCP:$ROCE_PRI
	fi
	if [ $ENABLE_CC -eq 1 ] &&  [ $CNP_SERVICE_TYPE -eq 1 ]
	then
		dcb app add dev $INF_NAME dscp-prio $ROCE_CNP_DSCP:$ROCE_CNP_PRI
	fi
}

niccli_pgm_pfc_ets() {
	PCI_BUSADDR=$1
	echo "Setting pfc/ets $PCI_BUSADDR"
	commands=()
	if [[ -z "${RUN_SETUP_SCRIPT}" ]]; then
		while IFS= read -r line; do
			commands+=("$line")
		done < <(niccli_rem_app_tlv "$PCI_BUSADDR")
	fi

	if echo "$(printf "%s\n" "${commands[@]}")" | grep -q "Error:"; then
		echo "$(printf "%s\n" "${commands[@]}")"
		echo "$(niccli --version)"
		exit -1
	fi

	if [ $CNP_SERVICE_TYPE -eq 1 ]; then

		commands+=("qos --ets --tsa 0:ets,1:ets,2:strict,3:strict,4:strict,5:strict,6:strict,7:strict --up2tc $pri2tc --tcbw $L2_BW,$ROCE_BW")
		commands+=("qos --ets --tsa 0:ets,1:ets,2:strict --up2tc $pri2tc --tcbw $L2_BW,$ROCE_BW")
		commands+=("qos --ets --tsa 0:ets,1:ets,2:strict,3:ets --up2tc $pri2tc --tcbw $L2_BW,$ROCE_BW")
	else
		commands+=("qos --ets --tsa 0:ets,1:ets --up2tc $pri2tc --tcbw $L2_BW,$ROCE_BW")
	fi

	if [ ! -z "$ROCE_PRI" ] && [ $ENABLE_PFC -eq 1 ]; then
		commands+=("qos --pfc --enable $(printf "%d" $ROCE_PRI)")
		commands+=("qos --apptlv --add --app $(printf "%d" $ROCE_PRI),3,4791")
	else
		commands+=("qos --pfc --enable 0xFF")
	fi

	if [ $ENABLE_DSCP_BASED_PFC -eq 1 ] || [ $EN_ROCE_DSCP -eq 1 ]; then
		commands+=("qos --apptlv --add --app $(printf "%d" $ROCE_PRI),5,$(printf "%d" $ROCE_DSCP)")
	fi

	if [ $ENABLE_CC -eq 1 ] && [ $CNP_SERVICE_TYPE -eq 1 ]; then
		commands+=("qos --apptlv --add --app $(printf "%d" $ROCE_CNP_PRI),5,$(printf "%d" $ROCE_CNP_DSCP)")
	fi

	niccli --dev $PCI_BUSADDR <<EOF 2>&1
$(printf "%s\n" "${commands[@]}")
EOF
}

check_qos_exists() {
	local conf="$1"
	local inf="$2"
	awk -v inf="$inf" '
	BEGIN { in_lldp=0; in_inf=0; has_qos=0; braces=0; inf_brace_level=-1 }
	/^lldp[ \t]*:[ \t]*$/ { in_lldp=1 }
	/{/ { braces++ }
	/}/ { braces--; if (in_inf && braces == inf_brace_level) in_inf=0; \
		if (in_lldp && braces == 0) in_lldp=0 }
	in_lldp && $0 ~ "^[ \t]*" inf "[ \t]*:[ \t]*$" { in_inf=1; inf_brace_level=braces }
	in_inf && /tlvid0080c209/ { has_qos=1 }
	END { if (braces != 0) exit 1; print has_qos }
	' "$conf"
}

check_inf_exists() {
	local conf="$1"
	local inf="$2"
	awk -v inf="$inf" '
	BEGIN { in_lldp=0; exists=0; braces=0; inf_brace_level=-1 }
	/^lldp[ \t]*:[ \t]*$/ { in_lldp=1 }
	/{/ { braces++ }
	/}/ { braces--; if (in_lldp && braces == 0) in_lldp=0 }
	in_lldp && $0 ~ "^[ \t]*" inf "[ \t]*:[ \t]*$" { exists=1 }
	END { if (braces != 0) exit 1; print exists }
	' "$conf"
}

inject_qos_new_inf() {
	local conf="$1"
	local tmp_conf="$2"
	local inf="$3"
	local up2tc="$4"
	local tcbw="$5"
	local pfc_en="$6"
	local app_list="$7"

	awk -v inf="$inf" -v up2tc="$up2tc" -v tcbw="$tcbw" -v pfc_en="$pfc_en" \
		-v app_list="$app_list" '
	BEGIN { in_lldp=0; braces=0; injected=0 }
	/^lldp[ \t]*:[ \t]*$/ { in_lldp=1 }
	/{/ { braces++; print; next }
	/}/ {
		braces--;
		if (in_lldp && braces == 0 && !injected) {
			print "  " inf " : "
			print "  {"
			print "    adminStatus = 3;"
			print "    tlvid0080c209 : "
			print "    {"
			print "      willing = 1;"
			print "      tsa = \"0:ets,1:ets,2:strict,3:strict," \
			      "4:strict,5:strict,6:strict,7:strict\";"
			print "      up2tc = \"" up2tc "\";"
			print "      tcbw = \"" tcbw "\";"
			print "    };"
			print "    tlvid0080c20b : "
			print "    {"
			print "      willing = 1;"
			print "      enabled = " pfc_en ";"
			print "    };"
			print "    tlvid0080c20c : "
			print "    {"
			if (app_list != "") {
				n = split(app_list, apps, ";")
				for (i = 1; i <= n; i++) {
					print "      APP" (i-1) " = \"" apps[i] "\";"
				}
			}
			print "    };"
			print "  };"
			injected=1
		}
		print; next
	}
	{ print }
	' "$conf" > "$tmp_conf"
}

inject_qos_existing_inf() {
	local conf="$1"
	local tmp_conf="$2"
	local inf="$3"
	local up2tc="$4"
	local tcbw="$5"
	local pfc_en="$6"
	local app_list="$7"

	awk -v inf="$inf" -v up2tc="$up2tc" -v tcbw="$tcbw" -v pfc_en="$pfc_en" \
		-v app_list="$app_list" '
	BEGIN { in_lldp=0; in_inf=0; braces=0; injected=0; inf_brace_level=-1 }
	/^lldp[ \t]*:[ \t]*$/ { in_lldp=1 }
	in_lldp && $0 ~ "^[ \t]*" inf "[ \t]*:[ \t]*$" { in_inf=1; inf_brace_level=braces }
	/{/ {
		braces++;
		print;
		if (in_inf && braces == (inf_brace_level + 1) && !injected) {
			print "    tlvid0080c209 : "
			print "    {"
			print "      willing = 1;"
			print "      tsa = \"0:ets,1:ets,2:strict,3:strict," \
			      "4:strict,5:strict,6:strict,7:strict\";"
			print "      up2tc = \"" up2tc "\";"
			print "      tcbw = \"" tcbw "\";"
			print "    };"
			print "    tlvid0080c20b : "
			print "    {"
			print "      willing = 1;"
			print "      enabled = " pfc_en ";"
			print "    };"
			print "    tlvid0080c20c : "
			print "    {"
			if (app_list != "") {
				n = split(app_list, apps, ";")
				for (i = 1; i <= n; i++) {
					print "      APP" (i-1) " = \"" apps[i] "\";"
				}
			}
			print "    };"
			injected=1
		}
		next
	}
	/}/ {
		braces--;
		if (in_inf && braces == inf_brace_level) in_inf=0;
		if (in_lldp && braces == 0) in_lldp=0;
		print; next
	}
	{ print }
	' "$conf" > "$tmp_conf"
}

calculate_qos_vars() {
	PFC_ENABLE=0
	if [ ! -z "$ROCE_PRI" ] && [ "$ENABLE_PFC" -eq 1 ]; then
		local pri_dec
		if ! pri_dec=$(printf "%d" "$ROCE_PRI" 2>/dev/null) || [ -z "$pri_dec" ]; then
			echo "Error: Invalid ROCE_PRI format ($ROCE_PRI)" >&2
		elif [ "$pri_dec" -ge 0 ] && [ "$pri_dec" -le 7 ]; then
			PFC_ENABLE=$((1 << pri_dec))
		else
			echo "Warning: ROCE_PRI ($ROCE_PRI) out of bounds (0-7)" >&2
		fi
	fi

	APP_LIST=""
	local sep=""
	if [ ! -z "$ROCE_PRI" ] && [ "$ENABLE_PFC" -eq 1 ]; then
		APP_LIST="${sep}$(printf "%d" $ROCE_PRI),3,4791"
		sep=";"
	fi
	if [ "$ENABLE_DSCP_BASED_PFC" -eq 1 ] || [ "$EN_ROCE_DSCP" -eq 1 ]; then
		APP_LIST="${APP_LIST}${sep}$(printf "%d" $ROCE_PRI),5,$(printf "%d" $ROCE_DSCP)"
		sep=";"
	fi
	if [ "$ENABLE_CC" -eq 1 ] && [ "$CNP_SERVICE_TYPE" -eq 1 ]; then
		local cnp_pri_dec=$(printf "%d" $ROCE_CNP_PRI)
		local cnp_dscp_dec=$(printf "%d" $ROCE_CNP_DSCP)
		APP_LIST="${APP_LIST}${sep}${cnp_pri_dec},5,${cnp_dscp_dec}"
		sep=";"
	fi

	TCBW="$L2_BW,$ROCE_BW,0,0,0,0,0,0"
}

update_lldpad_conf_for_inf() {
	local conf="$1"
	local inf="$2"

	local has_qos
	has_qos=$(check_qos_exists "$conf" "$inf")
	if [ $? -ne 0 ]; then
		echo "Error: Malformed lldpad.conf (unmatched braces). Skipping QoS injection." >&2
		return 1
	fi

	if [ "$has_qos" -eq 1 ]; then
		echo "QoS settings already exist for $inf in lldpad.conf. Skipping."
		return 0
	fi

	local inf_exists
	inf_exists=$(check_inf_exists "$conf" "$inf")
	if [ $? -ne 0 ]; then
		echo "Error: Malformed lldpad.conf (unmatched braces). Skipping QoS injection." >&2
		return 1
	fi
	local tmp_conf="${conf}.${inf}.tmp"

	if [ "$inf_exists" -eq 0 ]; then
		echo "Injecting $inf block with QoS into lldpad.conf..."
		inject_qos_new_inf "$conf" "$tmp_conf" "$inf" "$pri2tc" \
			"$TCBW" "$PFC_ENABLE" "$APP_LIST"
	else
		echo "Injecting QoS TLVs into existing $inf block in lldpad.conf..."
		inject_qos_existing_inf "$conf" "$tmp_conf" "$inf" "$pri2tc" \
			"$TCBW" "$PFC_ENABLE" "$APP_LIST"
	fi

	if [ -f "$tmp_conf" ] && [ -s "$tmp_conf" ]; then
		mv "$tmp_conf" "$conf" || {
			echo "Error: Failed to update $conf with $tmp_conf"
			rm -f "$tmp_conf"
			return 1
		}
	else
		echo "Error: Failed to generate temporary configuration for $inf"
		rm -f "$tmp_conf"
		return 1
	fi
	return 0
}

generate_lldpad_conf() {
	local LLDPAD_CONF="/var/lib/lldpad/lldpad.conf"
	local LOCK_FILE="/var/lib/lldpad/.lldpad.conf.lock"

	if ! mkdir -p /var/lib/lldpad; then
		echo "Error: Failed to create /var/lib/lldpad directory" >&2
		return 1
	fi

	calculate_qos_vars

	(
		if ! exec 9> "$LOCK_FILE"; then
			echo "Error: Failed to open lock file $LOCK_FILE" >&2
			exit 1
		fi
		if ! flock -x 9; then
			echo "Error: Failed to acquire lock on $LOCK_FILE" >&2
			exit 1
		fi
		trap 'exec 9>&-' EXIT

		# 1. Create base file if it doesn't exist
		if [ ! -f "$LLDPAD_CONF" ]; then
			echo "lldpad.conf does not exist. Creating base configuration..."
			cat <<EOF > "$LLDPAD_CONF"
lldp :
{
};
EOF
		elif ! grep -q "^[[:space:]]*lldp[ \t]*:" "$LLDPAD_CONF"; then
			cat <<EOF >> "$LLDPAD_CONF"
lldp :
{
};
EOF
		fi

		# 2. Process each interface
		for INF in $INF_NAME1 $INF_NAME2; do
			if [ -z "$INF" ]; then continue; fi
			update_lldpad_conf_for_inf "$LLDPAD_CONF" "$INF" || exit 1
		done
	) || return 1
}

# Generate lldpad.conf if it doesn't exist, mimicking udev settings
generate_lldpad_conf

if [ $QOS_TOOL -eq 1 ]
then
    SYSTEMCTL_STATUS=`command -v systemctl`
    if [ "$SYSTEMCTL_STATUS" == "" ];
    then
	echo "systemctl not found, install and re-run the script. exiting..."
	exit -1
    fi

    STATUS="$(systemctl is-active lldpad)"
    if [ "${STATUS}" = "active" ]; then
        #Stop lldpad
        echo "WARNING: Disabling lldpad service, and using bnxtqos tool for configuration"
        echo "  bnxtqos and lldptool cannot both run. Use bnxtqos to configure all ports."
        systemctl stop lldpad.service
    fi

    bnxt_qos_pgm_pfc_ets $INF_NAME1
    if [ "$INF_NAME2" != "" ];
    then
	bnxt_qos_pgm_pfc_ets $INF_NAME2
    fi

elif [ $QOS_TOOL -eq 3 ]
then
    SYSTEMCTL_STATUS=`command -v systemctl`
    if [ "$SYSTEMCTL_STATUS" == "" ];
    then
        echo "systemctl not found, install and re-run the script. exiting..."
        exit -1
    fi

    STATUS="$(systemctl is-active lldpad)"
    if [ "${STATUS}" = "active" ]; then
        #Stop lldpad
        echo "WARNING: Disabling lldpad service, and using niccli tool for configuration"
        echo "  niccli and lldptool cannot both run. Use niccli to configure all ports."
        systemctl stop lldpad.service
    fi

	niccli_pgm_pfc_ets $PCI_BUS_ADDR1
	if [ "$INF_NAME2" != "" ];
	then
		niccli_pgm_pfc_ets $PCI_BUS_ADDR2
	fi

elif [ $QOS_TOOL -eq 2 ]
then
    SYSTEMCTL_STATUS=`command -v systemctl`
    if [ "$SYSTEMCTL_STATUS" == "" ];
    then
        echo "systemctl not found, install and re-run the script. exiting..."
        exit -1
    fi

    STATUS="$(systemctl is-active lldpad)"
    if [ "${STATUS}" != "active" ]; then
        #Start lldpad
        systemctl start lldpad.service
    fi

    echo "Setting up LLDP"
    lldptool_pgm_pfc_ets $INF_NAME1
    if [ "$INF_NAME2" != "" ];
    then
	lldptool_pgm_pfc_ets $INF_NAME2
    fi

    systemctl restart lldpad.service

else
    # When QOS_TOOL is 4 - Configuring ets using dcb command.
    SYSTEMCTL_STATUS=`command -v systemctl`
    if [ "$SYSTEMCTL_STATUS" == "" ];
    then
        echo "systemctl not found, install and re-run the script. exiting..."
        exit -1
    fi

    STATUS="$(systemctl is-active lldpad)"
    if [ "${STATUS}" = "active" ]; then
        #Stop lldpad
        echo "Disabling lldpad service, and using dcb command for configuration"
        systemctl stop lldpad.service
    fi

    dcb_pgm_pfc_ets $INF_NAME1
    if [ "$INF_NAME2" != "" ];
    then
	dcb_pgm_pfc_ets $INF_NAME2
    fi
fi

PREVDIR=`pwd`
if mkdir /sys/kernel/config/bnxt_re/"$DEV_NAME" 2>/dev/null; then
	CONFIGFS_PRESENT=1
else
	if [ -d /sys/kernel/debug/bnxt_re/ ]; then
		DEBUGFS_PRESENT=1
	else
		DEBUGFS_PRESENT=0
	fi
fi

if  [ "$CONFIGFS_PRESENT" ] && [ "$CONFIGFS_PRESENT" -eq 1 ]; then
	cd /sys/kernel/config/bnxt_re/$DEV_NAME/ports/1/cc/
	#Disabling prio vlan insertion if dscp based pfc is enabled
	if [ -w "disable_prio_vlan_tx" ]; then
		if [ $ENABLE_DSCP_BASED_PFC -eq 1 ]; then
			echo -n 0x1 > disable_prio_vlan_tx
		else
			echo -n 0x0 > disable_prio_vlan_tx
		fi
	fi

	if [ $ENABLE_CC -eq 1 ]
	then
		echo "Setting up CC Settings"
		echo -n 0x1 > ecn_marking
		echo -n 0x1 > ecn_enable
		echo -n 1 > cc_mode
	else
		echo -n 0x0 > ecn_marking
		echo -n 0x0 > ecn_enable
	fi
	if [ "$CNP_SERVICE_TYPE" == "0" ]; then
		if [ ! -z "$ROCE_CNP_PRI" ]
		then
			echo -n $ROCE_CNP_PRI > cnp_prio
		fi
		if [ ! -z "$ROCE_PRI" ]
		then
			echo -n $ROCE_PRI > roce_prio
		fi

		if [ $ENABLE_DSCP -eq 1 ]
		then
			echo "Setting up DSCP/PRI"
			if [ ! -z "$ROCE_DSCP" ]
			then
				echo -n $ROCE_DSCP > roce_dscp
			fi
			if [ ! -z "$ROCE_CNP_DSCP" ]
			then
				echo -n $ROCE_CNP_DSCP > cnp_dscp
			fi
		fi
	fi
	echo -n 0x1 > apply

	cd $PREVDIR
	rmdir /sys/kernel/config/bnxt_re/$DEV_NAME
	echo "Completed Configuration"
fi

if [ "$DEBUGFS_PRESENT" ] && [ "$DEBUGFS_PRESENT" -eq 1 ]; then
	cd /sys/kernel/debug/bnxt_re/$PCI_BUS_ADDR1/cc_config/
	if [ $ENABLE_CC -eq 1 ]; then
		echo "Setting up CC Settings"
		echo -n 0x1 > enable_cc
		echo -n 0x1 > tos_ecn
		echo -n 1 > cc_mode
	else
		echo -n 0x0 > enable_cc
		echo -n 0x0 > tos_ecn
	fi
	cd $PREVDIR
	echo "Completed Configuration"
fi

if [ -n "${RUN_SETUP_SCRIPT}" ]; then
    echo "Complete"
    exit 0
fi

echo "Settings Default to use RoCE-v$ROCE_MODE"
mkdir -p /sys/kernel/config/rdma_cm/$DEV_NAME
echo "RoCE v2" > /sys/kernel/config/rdma_cm/$DEV_NAME/ports/1/default_roce_mode
if [ ! -z "$ROCE_DSCP" ]
then
	echo -n $((ROCE_DSCP << 2)) > /sys/kernel/config/rdma_cm/$DEV_NAME/ports/1/default_roce_tos
else
	echo -n 0 > /sys/kernel/config/rdma_cm/$DEV_NAME/ports/1/default_roce_tos
fi
rmdir /sys/kernel/config/rdma_cm/$DEV_NAME
